param([Parameter(Mandatory=$true)][string]$PackagePath,
      [Parameter(Mandatory=$true)][string]$ExpectedSid,
      [Parameter(Mandatory=$true)][string]$LogPath)
$ErrorActionPreference = 'Stop'
Start-Transcript -LiteralPath $LogPath -Force
try {
    if ([Security.Principal.WindowsIdentity]::GetCurrent().User.Value -ne $ExpectedSid) {
        throw 'Elevation must use the same Windows account, not another administrator account.'
    }
    & (Join-Path $PackagePath 'scripts\Update-KeyKey.ps1') -Action Full -PackagePath $PackagePath
    exit 0
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
} finally { Stop-Transcript }
