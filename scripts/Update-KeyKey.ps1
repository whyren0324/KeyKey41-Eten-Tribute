param(
    [ValidateSet('Menu','Status','Plan','Backend','Full','Rollback','Baseline')][string]$Action = 'Menu',
    [string]$PackagePath = (Split-Path -Parent $PSScriptRoot),
    [ValidateSet('Backend','Full')][string]$PlanMode = 'Full'
)
. (Join-Path $PSScriptRoot 'UpdateCommon.ps1')

if (-not [Environment]::Is64BitProcess -or $env:PROCESSOR_ARCHITECTURE -ne 'AMD64') {
    throw 'Use native 64-bit PowerShell on Windows x64. ARM64 is not supported by this updater.'
}
if ($Action -eq 'Menu') {
    Write-Host 'KeyKey 41 update: 1 Backend / 2 Full / 3 Status / 4 Rollback / 5 Validate only / 6 Restore MSI baseline'
    $choice = Read-Host 'Select'
    $Action = switch ($choice) { '1' {'Backend'} '2' {'Full'} '3' {'Status'} '4' {'Rollback'} '5' {'Plan'} '6' {'Baseline'} default { return } }
}
if ($Action -eq 'Plan') { Get-UpdatePlan $PackagePath $PlanMode | Select-Object Id,Version,Mode,Root; return }

$installRoot = Get-RegValue Registry64 'SOFTWARE\KeyKey41' 'UpdateInstallRoot'
if (-not $installRoot) { $installRoot = Get-RegValue Registry64 'SOFTWARE\KeyKey41' 'InstallDir' }
$tip64 = Get-RegValue Registry64 $script:TipKey ''
$tip32 = Get-RegValue Registry32 $script:TipKey ''
if (-not $installRoot -and $tip64) { $installRoot = Split-Path -Parent $tip64 }
if (-not $installRoot) { throw 'An existing registered KeyKey 41 installation is required. Install once with MSI first.' }
$installRoot = Get-CanonicalPath $installRoot
Assert-NoLinkedAncestor $installRoot
$updatesRoot = Assert-ChildPath $installRoot (Join-Path $installRoot 'updates')
if (Test-Path -LiteralPath $updatesRoot) { Assert-NoLinkedAncestor $updatesRoot }
$statePath = Join-Path $updatesRoot 'previous.json'
$baselinePath = Join-Path $updatesRoot 'baseline.json'

function Get-RuntimeProcesses {
    @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ProcessName -match '^McBopomofo(Server|Config)(_(x64|x86))?$' -and
        $_.SessionId -eq [Diagnostics.Process]::GetCurrentProcess().SessionId
    })
}
function Get-BackendPath {
    $active = Get-RegValue Registry64 'SOFTWARE\KeyKey41' 'ActiveServerPath'
    if ($active) { return [string]$active }
    foreach ($name in @('McBopomofoServer_x64.exe','McBopomofoServer.exe')) {
        $path = Join-Path (Split-Path -Parent $tip64) $name
        if (Test-Path -LiteralPath $path) { return $path }
    }
    throw 'Installed backend not found'
}
function Show-Status {
    Write-Host "Registered x64 frontend: $tip64"
    Write-Host "Registered x86 frontend: $tip32"
    Write-Host "Selected backend: $(Get-BackendPath)"
    Get-RuntimeProcesses | ForEach-Object {
        try {
            [pscustomobject]@{ Process=$_.ProcessName; PID=$_.Id; Path=$_.Path; Version=$_.MainModule.FileVersionInfo.FileVersion }
        } catch { Write-Warning 'A runtime process exited or could not be inspected' }
    } | Format-Table -AutoSize
    $incomplete = 0
    $loaded = foreach ($process in Get-Process) {
        try {
            foreach ($module in $process.Modules) {
                if ($module.ModuleName -like 'McBopomofoTIP*.dll') {
                    [pscustomobject]@{Process=$process.ProcessName; PID=$process.Id; Frontend=$module.FileName;
                        Version=$module.FileVersionInfo.FileVersion;
                        NeedsRestart=($module.FileName -ne $tip64 -and $module.FileName -ne $tip32)}
                }
            }
        } catch { $incomplete++ }
    }
    $loaded | Format-Table -AutoSize
    Write-Host "Process inspection incomplete for $incomplete processes. Reopen apps to load the new frontend; sign out/in if needed."
}
if ($Action -eq 'Status') { Show-Status; return }
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run Update.cmd as administrator using the same Windows account.'
}
if (-not $tip64 -or -not $tip32) { throw 'Both x64 and x86 frontend registrations must exist' }

if (-not ('KeyKeyUpdateNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class KeyKeyUpdateNative {
  public delegate bool EnumProc(IntPtr h, IntPtr data);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p, IntPtr data);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
}
'@
}
function Stop-OwnedRuntime {
    foreach ($process in Get-RuntimeProcesses) {
        $path = $process.Path
        if (-not $path) { throw "Cannot inspect process $($process.Id)" }
        $null = Assert-ChildPath $installRoot $path
        $targetId = $process.Id
        $callback = [KeyKeyUpdateNative+EnumProc]{ param($h,$data)
            [uint32]$windowPid = 0
            $null = [KeyKeyUpdateNative]::GetWindowThreadProcessId($h,[ref]$windowPid)
            if ($windowPid -eq $targetId) {
                if ($process.ProcessName -match 'Server') {
                    $null = [KeyKeyUpdateNative]::PostMessage($h,0x111,[IntPtr]1011,[IntPtr]::Zero)
                } else { $null = [KeyKeyUpdateNative]::PostMessage($h,0x10,[IntPtr]::Zero,[IntPtr]::Zero) }
            }
            return $true
        }
        $null = [KeyKeyUpdateNative]::EnumWindows($callback,[IntPtr]::Zero)
        if (-not $process.WaitForExit(5000)) { throw "Process $targetId did not close. Close the IME backend/settings and retry." }
    }
}
function Start-BackendProcess([string]$Path) {
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $Path
    $info.WorkingDirectory = Split-Path -Parent $Path
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    return [Diagnostics.Process]::Start($info)
}
function Start-CheckedBackend([string]$Path) {
    $proc = Start-BackendProcess $Path
    $script:StartedBackend = $proc
    $deadline = [DateTime]::UtcNow.AddSeconds(12)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($proc.HasExited) { throw 'New backend exited before startup completed' }
        $pipe = New-Object IO.Pipes.NamedPipeClientStream('.', 'WinMcBopomofo_IPC_Pipe', [IO.Pipes.PipeDirection]::InOut, [IO.Pipes.PipeOptions]::Asynchronous)
        try {
            $pipe.Connect(300)
            $bytes = [Text.Encoding]::UTF8.GetBytes("8`n")
            $pipe.Write($bytes,0,$bytes.Length)
            $buffer = New-Object byte[] 1024
            $read = $pipe.ReadAsync($buffer,0,$buffer.Length)
            if (-not $read.Wait(1500)) { throw 'Backend health response timed out' }
            $response = [Text.Encoding]::UTF8.GetString($buffer,0,$read.Result).Trim() -split "`n"
            if ($response.Count -eq 3 -and $response[0] -eq '1' -and $response[1] -eq [string]$proc.Id) {
                Write-Host "Backend ready: PID $($proc.Id), version $($response[2])"
                return
            }
        } catch { } finally { $pipe.Dispose() }
        Start-Sleep -Milliseconds 200
    }
    throw 'New backend did not pass the IPC health check'
}
function Restore-State($State) {
    Set-RegValue Registry64 $script:TipKey '' $State.tip64
    Set-RegValue Registry32 $script:TipKey '' $State.tip32
    Set-RegValue Registry64 'SOFTWARE\KeyKey41' 'ActiveServerPath' $State.activeServer
    $runKey = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey('Software\Microsoft\Windows\CurrentVersion\Run')
    try {
        if ($null -eq $State.autorun) { $runKey.DeleteValue('Win-McBopomofo-Server',$false) }
        else { $runKey.SetValue('Win-McBopomofo-Server',$State.autorun) }
    } finally { $runKey.Dispose() }
}

$lock = New-Object Threading.Mutex($false,'Global\KeyKey41Update')
$locked = $false
$changed = $false
$completed = $false
$oldState = $null
$script:StartedBackend = $null
try {
    $locked = $lock.WaitOne(0)
    if (-not $locked) { throw 'Another update is running' }
    $runKey = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\Microsoft\Windows\CurrentVersion\Run')
    $runValue = if ($runKey) { try { $runKey.GetValue('Win-McBopomofo-Server',$null) } finally { $runKey.Dispose() } } else { $null }
    $oldState = [pscustomobject]@{tip64=$tip64; tip32=$tip32;
        activeServer=(Get-RegValue Registry64 'SOFTWARE\KeyKey41' 'ActiveServerPath');
        backend=(Get-BackendPath); autorun=$runValue;
        sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value}
    if ($Action -in @('Rollback','Baseline')) {
        $restoreFile = if ($Action -eq 'Baseline') { $baselinePath } else { $statePath }
        $next = Get-Content -LiteralPath $restoreFile -Raw | ConvertFrom-Json
        if ($next.sid -ne $oldState.sid) { throw 'Rollback must use the account that performed the update' }
        foreach ($path in @($next.tip64,$next.tip32,$next.backend)) {
            $null = Assert-ChildPath $installRoot $path
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Rollback file missing: $path" }
        }
    } else {
        $plan = Get-UpdatePlan $PackagePath $Action
        $id = $plan.Id + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
        $releaseDir = Assert-ChildPath $updatesRoot (Join-Path $updatesRoot $id)
        $null = New-Item -ItemType Directory -Path $releaseDir
        foreach ($entry in $plan.Files) {
            $destination = Assert-ChildPath $releaseDir (Join-Path $releaseDir $entry.Relative.Substring(4))
            $null = New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force
            Copy-Item -LiteralPath $entry.Source -Destination $destination
            if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.Hash) { throw 'Staged file checksum mismatch' }
        }
        & "$env:SystemRoot\System32\icacls.exe" $releaseDir /grant '*S-1-15-2-1:(OI)(CI)(RX)' /T /Q | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Unable to grant AppContainer read access' }
        $next = [pscustomobject]@{tip64=$tip64;tip32=$tip32;backend=(Join-Path $releaseDir 'McBopomofoServer_x64.exe')}
        if ($Action -eq 'Full') {
            $next.tip64 = Join-Path $releaseDir 'McBopomofoTIP_x64.dll'
            $next.tip32 = Join-Path $releaseDir 'McBopomofoTIP_x86.dll'
        }
    }
    # Durable recovery state is written before stopping any process or switching registration.
    $null = New-Item -ItemType Directory -Path $updatesRoot -Force
    if (-not (Test-Path -LiteralPath $baselinePath)) {
        $oldState | ConvertTo-Json | Set-Content -LiteralPath $baselinePath -Encoding UTF8
    }
    $oldState | ConvertTo-Json | Set-Content -LiteralPath ($statePath + '.tmp') -Encoding UTF8
    Move-Item -LiteralPath ($statePath + '.tmp') -Destination $statePath -Force
    $changed = $true
    Set-RegValue Registry64 'SOFTWARE\KeyKey41' 'UpdateInstallRoot' $installRoot
    Set-RegValue Registry64 'SOFTWARE\KeyKey41' 'UpdateInProgress' 1
    Stop-OwnedRuntime
    if ($Action -in @('Rollback','Baseline')) { Restore-State $next }
    else {
        Set-RegValue Registry64 'SOFTWARE\KeyKey41' 'ActiveServerPath' $next.backend
        Set-RegValue Registry64 $script:TipKey '' $next.tip64
        Set-RegValue Registry32 $script:TipKey '' $next.tip32
        $runKey = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey('Software\Microsoft\Windows\CurrentVersion\Run')
        try { $runKey.SetValue('Win-McBopomofo-Server','"' + $next.backend + '"') } finally { $runKey.Dispose() }
    }
    if ($Action -in @('Rollback','Baseline')) {
        # Old installations may predate the health command; verify process survival.
        $p = Start-BackendProcess $next.backend
        $script:StartedBackend = $p
        Start-Sleep -Seconds 2
        if ($p.HasExited) { throw 'Restored backend exited' }
    } else { Start-CheckedBackend $next.backend }
    Set-RegValue Registry64 'SOFTWARE\KeyKey41' 'UpdateInProgress' 0
    $completed = $true
    $tip64 = $next.tip64; $tip32 = $next.tip32
    Write-Host 'Update complete. No uninstall or forced application closure was performed.'
    Show-Status
} catch {
    $failure = $_
    if ($changed -and $oldState -and -not $completed) {
        try {
            # Only terminate a failed new process launched by this invocation.
            if ($script:StartedBackend -and -not $script:StartedBackend.HasExited) {
                Stop-Process -Id $script:StartedBackend.Id -Force
                $null = $script:StartedBackend.WaitForExit(5000)
            }
            Restore-State $oldState
            if (@(Get-RuntimeProcesses | Where-Object { $_.ProcessName -match 'Server' }).Count -eq 0) {
                Start-BackendProcess $oldState.backend | Out-Null
            }
            Write-Warning 'Previous registration and backend restored.'
        } catch { Write-Warning "Automatic recovery failed. Recovery snapshot: $statePath. $($_.Exception.Message)" }
    }
    throw $failure
} finally {
    if ($changed) { Set-RegValue Registry64 'SOFTWARE\KeyKey41' 'UpdateInProgress' 0 }
    if ($locked) { $lock.ReleaseMutex() }
    $lock.Dispose()
}
