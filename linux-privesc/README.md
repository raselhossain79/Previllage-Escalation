# Linux Privilege Escalation — Reference Library

A structured, mechanism-first reference for escalating privileges on Linux systems during penetration tests, CTFs, and OSCP-style engagements. Each file covers one distinct technique: what it is, how to detect it, how to exploit it (flag-by-flag command breakdowns), and how to defend against it.

---

## How to Use This Repo

1. **Always start with `00-enumeration.md`.** Privilege escalation is fundamentally an enumeration problem — most of your time on a box should go into gathering information, not running exploits blind.
2. Run through the enumeration checklist, then match whatever you find against the relevant file below.
3. Each file ends with a **Quick Checklist** — use these as a fast reference during live engagements once you already understand the underlying mechanism.
4. Files are numbered roughly in the order you're likely to check them (easiest/most common first), not in order of difficulty.

---

## File Index — What Each File Covers and When to Use It

| File | Technique | Use When You Find... |
|------|-----------|----------------------|
| `00-enumeration.md` | Full system enumeration baseline | Starting point for every engagement — always run first |
| `01-suid-abuse.md` | SUID binary exploitation via GTFOBins | `find / -perm -4000` shows a non-standard binary |
| `02-sudo-misconfiguration.md` | NOPASSWD / ALL sudo rules | `sudo -l` shows exploitable entries |
| `03-cron-job-abuse.md` | Writable cron scripts, PATH abuse in cron | `/etc/crontab` or `cron.d/` shows a root job pointing to a writable script |
| `04-kernel-exploit.md` | Kernel version exploits (Dirty COW, udev, etc.) | Old/unpatched kernel version from `uname -a`, no other vector works |
| `05-weak-file-permissions.md` | Writable `/etc/passwd`, readable `/etc/shadow`, exposed SSH keys | Basic permission checks show world-writable sensitive files |
| `06-path-hijacking.md` | PATH variable abuse against SUID binaries/scripts | `strings`/`ltrace` shows a command called without a full path |
| `07-capabilities.md` | Linux capabilities abuse (`cap_setuid`, `cap_dac_override`, etc.) | `getcap -r /` shows a dangerous capability on a usable binary |
| `08-environment-variables.md` | LD_PRELOAD, LD_LIBRARY_PATH, PYTHONPATH, BASH_ENV abuse | `sudo -l` shows `env_keep` retaining dangerous variables |
| `09-credential-hunting.md` | Finding plaintext credentials across the filesystem | No direct privesc vector found yet — hunt for creds to pivot/login as another user |
| `10-docker-lxc-breakout.md` | Docker/LXD group membership abuse | `id`/`groups` shows `docker` or `lxd` group membership |
| `11-nfs-misconfiguration.md` | `no_root_squash` NFS share abuse | `showmount -e` or `/etc/exports` reveals a share with `no_root_squash` |
| `12-passwd-shadow-abuse.md` | Deep-dive: every `/etc/passwd`/`/etc/shadow` entry point, backups included | Need the full playbook beyond the basics covered in `05` |
| `13-ssh-key-abuse.md` | Private key discovery, authorized_keys injection, agent hijacking | SSH keys found on disk, or `.ssh/` directories are writable |
| `14-wildcard-injection.md` | tar/rsync/chown wildcard flag injection (GTFOBins category) | A root cron/script uses an unquoted `*` inside a directory you can write to |
| `15-systemd-timer-service-abuse.md` | Modern cron replacement — timer/service unit abuse | `systemctl list-timers` shows a writable unit or script, cron-style boxes with no crontab |
| `16-polkit-pkexec-exploitation.md` | CVE-2021-4034 (PwnKit) and related polkit CVEs | `pkexec` is SUID-present and target looks unpatched/older (pre-2022 patch level) |
| `17-shared-library-injection.md` | Exploiting a SUID binary's missing shared library via RPATH | `ldd` on a SUID binary shows `=> not found` |
| `18-sudo-token-tty-hijacking.md` | pwfeedback buffer overflow (CVE-2019-18634), sudo token/TTY hijacking | `sudo -l` shows `pwfeedback` enabled, or you have ptrace access to another user's authenticated session |

---

## Suggested Enumeration Order (Field Workflow)

```
1. 00-enumeration.md          → full baseline sweep (always first)
2. 01 + 02                    → SUID + sudo (fastest wins, check first)
3. 07                         → capabilities (equally fast, often overlooked)
4. 03 + 15                    → cron AND systemd timers (check both — modern boxes may use either)
5. 05 + 12 + 13                → file permissions / passwd-shadow / SSH keys
6. 06 + 08 + 17                → PATH / env var / library hijacking (needs more specific conditions)
7. 10 + 11                     → container group abuse / NFS (environment-dependent, check if applicable)
8. 14                          → wildcard injection (check alongside cron/timer findings)
9. 16 + 18                     → known CVEs (polkit, sudo pwfeedback) — quick, low-cost checks
10. 09                         → credential hunting (ongoing, run in parallel with everything above)
11. 04                         → kernel exploits (last resort — higher risk of instability/crash)
```

---

## Conventions Used Across This Library

- **Mechanism-first explanations** — every technique explains *why* it works before showing *how* to exploit it
- **Flag-by-flag command breakdowns** — every non-trivial command includes a table or breakdown of what each argument does
- **GTFOBins cross-references** — noted wherever a technique maps to a documented GTFOBins entry
- **Quick Checklist** — every file ends with a condensed, copy-pasteable checklist for live use
- **Blue Team notes** — each file includes detection/hardening guidance, not just offense
- Full English only, consistent terminal-command style throughout

---

## Disclaimer

This library is intended strictly for authorized penetration testing, CTF practice, and personal lab environments (e.g., Metasploitable 2, TryHackMe, HackTheBox). Do not use any technique here against systems you do not have explicit written authorization to test.
