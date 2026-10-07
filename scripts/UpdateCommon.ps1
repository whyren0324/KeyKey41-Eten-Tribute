# Shared read-only planning and validation. Compatible with Windows PowerShell 5.1.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:TipKey = 'SOFTWARE\Classes\CLSID\{8C9D652A-9B99-4B77-BA9A-3B0F76923B7B}\InProcServer32'

function Get-CanonicalPath([string]$Path) {
    [IO.Path]::GetFullPath($Path).TrimEnd('\')
}
function Assert-ChildPath([string]$Root, [string]$Path) {
    $rootPath = Get-CanonicalPath $Root
    $childPath = Get-CanonicalPath $Path
    if (-not $childPath.StartsWith($rootPath + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escapes intended directory: $Path"
    }
    $childPath
}
function Assert-NoLinkedAncestor([string]$Path) {
    $ancestor = Get-Item -LiteralPath $Path
    while ($null -ne $ancestor) {
        if ($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked directory is not allowed: $Path" }
        $ancestor = $ancestor.Parent
    }
}
function Get-PeMachine([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $reader = New-Object IO.BinaryReader($stream)
    try {
        if ($reader.ReadUInt16() -ne 0x5a4d) { throw "Not a PE file: $Path" }
        $stream.Position = 0x3c
        $offset = $reader.ReadInt32()
        if ($offset -lt 64 -or $offset -gt ($stream.Length - 6)) { throw 'Invalid PE header' }
        $stream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x4550) { throw 'Invalid PE signature' }
        $reader.ReadUInt16()
    } finally { $reader.Dispose(); $stream.Dispose() }
}
function Get-UpdatePlan([string]$PackagePath, [string]$Mode) {
    $root = Get-CanonicalPath (Resolve-Path -LiteralPath $PackagePath).Path
    if ((Get-Item -LiteralPath $root).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Package root must not be a link' }
    $manifest = Get-Content -LiteralPath (Join-Path $root 'update-manifest.json') -Raw | ConvertFrom-Json
    if ($manifest.schema -ne 1 -or $manifest.ipc -ne 1 -or $manifest.architecture -ne 'x64') { throw 'Unsupported update package' }
    if ($manifest.id -notmatch '^[a-zA-Z0-9][a-zA-Z0-9._-]{0,79}$') { throw 'Invalid package id' }
    $required = @('app/McBopomofoServer_x64.exe', 'app/McBopomofoConfig_x64.exe',
        'app/data/data.txt', 'app/data/data-plain-bpmf.txt', 'app/data/associated-phrases-v2.txt',
        'app/data/dictionary_service.json', 'app/data/bpmfvs-variants.txt', 'app/data/bpmfvs-pua.txt',
        'app/data/opencc/tw2s.json', 'app/data/opencc/TSCharacters.ocd2', 'app/data/opencc/TSPhrases.ocd2',
        'app/data/opencc/TWVariantsRev.ocd2', 'app/data/opencc/TWVariantsRevPhrases.ocd2')
    if ($Mode -eq 'Full') { $required += @('app/McBopomofoTIP_x64.dll', 'app/McBopomofoTIP_x86.dll') }
    $seen = @{}
    $files = @()
    foreach ($entry in $manifest.files) {
        $relative = [string]$entry.path
        if ($relative -notmatch '^app/[a-zA-Z0-9_./-]+$' -or $relative.Split('/') -contains '..' -or $seen.ContainsKey($relative)) {
            throw "Invalid or duplicate package path: $relative"
        }
        $seen[$relative] = $true
        $path = Assert-ChildPath $root (Join-Path $root $relative)
        $item = Get-Item -LiteralPath $path
        if ($item.PSIsContainer) { throw 'Expected regular file' }
        $parent = $item
        while ($parent.FullName -ne $root) {
            if ($parent.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Links are not allowed in package' }
            $parent = Get-Item -LiteralPath (Split-Path -Parent $parent.FullName)
        }
        if ($entry.sha256 -notmatch '^[0-9a-fA-F]{64}$' -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.sha256) {
            throw "Checksum mismatch: $relative"
        }
        if ($relative -match '\.(exe|dll)$') {
            $machine = Get-PeMachine $path
            $expected = if ($relative -match '_x86\.dll$') { 0x14c } else { 0x8664 }
            if ($machine -ne $expected) { throw "Architecture mismatch: $relative" }
        }
        $files += [pscustomobject]@{ Relative = $relative; Source = $path; Hash = $entry.sha256 }
    }
    foreach ($name in $required) { if (-not $seen.ContainsKey($name)) { throw "Package missing $name" } }
    [pscustomobject]@{ Id = $manifest.id; Root = $root; Version = $manifest.version; Files = $files; Mode = $Mode }
}
function Get-RegValue([string]$View, [string]$Key, [string]$Name) {
    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,
        [Microsoft.Win32.RegistryView]::$View)
    try {
        $sub = $base.OpenSubKey($Key)
        if ($null -eq $sub) { return $null }
        try { $sub.GetValue($Name, $null) } finally { $sub.Dispose() }
    } finally { $base.Dispose() }
}
function Set-RegValue([string]$View, [string]$Key, [string]$Name, $Value) {
    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,
        [Microsoft.Win32.RegistryView]::$View)
    try {
        $sub = $base.CreateSubKey($Key)
        try {
            if ($null -eq $Value) { $sub.DeleteValue($Name, $false) }
            else { $sub.SetValue($Name, $Value) }
        } finally { $sub.Dispose() }
    } finally { $base.Dispose() }
}
