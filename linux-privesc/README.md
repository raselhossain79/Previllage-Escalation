# Linux Privilege Escalation — Reference Library

A structured, mechanism-first reference for escalating privileges on Linux systems during penetration tests, CTFs, and OSCP-style engagements. Each file covers one distinct technique: what it is, how to detect it, how to exploit it (flag-by-flag command breakdowns), and how to defend against it.

---

## How to Use This Repo

1. **Always start with `00-enumeration.md`.** Privilege escalation is fundamentally an enumeration problem — most of your time on a box should go into gathering information, not running exploits blind.
2. Not every engagement needs every file. Use the **scenario-based usage** section below to pick the relevant subset for the box in front of you, then fall back to the full trigger table if nothing in your scenario's subset pans out.
3. Each file ends with a **Quick Checklist** — use these as a fast reference during live engagements once you already understand the underlying mechanism.

---

## Scenario-Based Usage — Which Files Actually Apply

Real engagements don't need all 19 files. Use whichever bucket below matches the box you're on.

### A. Web App Pentest — Landed a Low-Priv Shell (e.g. www-data)
Priority order:
```
00 → 07 → 01 → 02 → 09 → 12
```
- `00` baseline, `07` capabilities and `01` SUID are fast wins worth checking before anything else
- `02` sudo misconfig — web users occasionally get a lazy sudo rule
- `09` credential hunting — web configs (`wp-config.php`, `.env`) are usually the richest source on this type of box
- `12` passwd/shadow — check permissions since web server users sometimes run with loose umask-created files

### B. Internal Network Pentest / Infrastructure Box
Priority order:
```
00 → 11 → 10 → 13 → 09 → 03 → 15
```
- `11` NFS misconfiguration and `10` Docker/LXD group abuse are far more common on internal infra than on a public-facing web box
- `13` SSH key abuse matters a lot here — internal boxes trust each other via keys
- `03`/`15` cron and systemd timers — infra boxes are full of scheduled maintenance scripts

### C. CTF / OSCP-Style Exam Box
Use the full systematic sweep — these boxes are deliberately built to test breadth:
```
00 → 01 → 02 → 07 → 03 → 15 → 05 → 12 → 13 → 06 → 08 → 17 → 14 → 16 → 18 → 10 → 11 → 09 → 04
```
This is the complete suggested order further down in this README — run it top to bottom when you have no other context to narrow the search.

### D. Modern / Recently-Patched Infrastructure (containers, systemd-based, current patch level)
Older techniques are less likely to land here; focus on what's still realistically unpatched:
```
00 → 10 → 15 → 16 → 17 → 14
```
- `16` (PwnKit) and `18` (sudo pwfeedback) are worth a quick check regardless, since patching compliance lags even on modern stacks
- `04` kernel exploits are usually a dead end here — skip unless nothing else works

### E. Legacy / Old System (e.g. Metasploitable-style, very outdated patch level)
```
00 → 04 → 01 → 02 → 03 → 05 → 09
```
- Old boxes are where kernel exploits (`04`) actually pay off — don't skip it here the way you would on a modern target
- Basic SUID/sudo/cron/permissions vectors are usually sufficient and often multiple are open simultaneously

### F. "Nothing Obvious Works" — Deeper/Edge-Case Pass
When the common vectors (SUID, sudo, cron, capabilities) are all clean:
```
06 → 08 → 17 → 14 → 11 → 10 → 18
```
These require more specific preconditions (writable PATH entries, retained env vars, missing libraries, wildcard-vulnerable scripts, NFS/container group membership, sudo edge-case bugs) — check them when the obvious stuff has already been ruled out.

---

## Full File Index — Every Technique and Its Trigger Condition

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

## Full Suggested Enumeration Order (No Scenario Context / CTF Default)

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
