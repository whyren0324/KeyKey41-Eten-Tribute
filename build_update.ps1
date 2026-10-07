param(
    [string]$Configuration = 'Release',
    [string]$X64BuildRoot = 'build',
    [string]$X86BuildRoot = 'build_x86',
    [string]$OpenCCBuildRoot = 'build_brian',
    [ValidateSet('Backend','Full')][string]$Mode = 'Full'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'scripts\UpdateCommon.ps1')
$repo = $PSScriptRoot
$x64 = Join-Path $repo "$X64BuildRoot\bin\$Configuration"
$x86 = Join-Path $repo "$X86BuildRoot\bin\$Configuration"
$server = Join-Path $x64 'McBopomofoServer.exe'
$version = [Diagnostics.FileVersionInfo]::GetVersionInfo($server).FileVersion.Trim()
if ($version -notmatch '^\d+\.\d+\.\d+\.\d+$') { throw 'Server has no valid file version; build first' }
$id = 'keykey-' + $version + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$package = Join-Path $repo "dist\$id"
$app = Join-Path $package 'app'
$null = New-Item -ItemType Directory -Path $app
$copies = @{
    'McBopomofoServer_x64.exe' = $server
    'McBopomofoConfig_x64.exe' = (Join-Path $x64 'McBopomofoConfig.exe')
}
if ($Mode -eq 'Full') {
    $copies['McBopomofoTIP_x64.dll'] = Join-Path $x64 'McBopomofoTIP_v2.dll'
    $copies['McBopomofoTIP_x86.dll'] = Join-Path $x86 'McBopomofoTIP_v2.dll'
}
foreach ($entry in $copies.GetEnumerator()) {
    $fileVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($entry.Value).FileVersion.Trim()
    if ($fileVersion -ne $version) { throw "Binary version mismatch: $($entry.Value)" }
    Copy-Item -LiteralPath $entry.Value -Destination (Join-Path $app $entry.Key)
}
$data = Join-Path $app 'data'
$null = New-Item -ItemType Directory -Path (Join-Path $data 'opencc')
foreach ($name in @('data.txt','data-plain-bpmf.txt','associated-phrases-v2.txt','dictionary_service.json','bpmfvs-variants.txt','bpmfvs-pua.txt')) {
    Copy-Item -LiteralPath (Join-Path $repo "data\$name") -Destination $data
}
Copy-Item -LiteralPath (Join-Path $repo 'third_party\OpenCC\data\config\tw2s.json') -Destination (Join-Path $data 'opencc')
foreach ($name in @('TSCharacters.ocd2','TSPhrases.ocd2','TWVariantsRev.ocd2','TWVariantsRevPhrases.ocd2')) {
    Copy-Item -LiteralPath (Join-Path $repo "$OpenCCBuildRoot\third_party\OpenCC\data\$name") -Destination (Join-Path $data 'opencc')
}
Copy-Item -LiteralPath (Join-Path $repo 'VERSION_HISTORY.md') -Destination (Join-Path $app 'VERSION_HISTORY.md')
$entries = @(Get-ChildItem -LiteralPath $app -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{path=$_.FullName.Substring($package.Length + 1).Replace('\','/');
        sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
})
[ordered]@{schema=1;ipc=1;architecture='x64';id=$id;version=$version;files=$entries} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $package 'update-manifest.json') -Encoding UTF8
$null = New-Item -ItemType Directory -Path (Join-Path $package 'scripts')
foreach ($name in @('UpdateCommon.ps1','Update-KeyKey.ps1')) {
    Copy-Item -LiteralPath (Join-Path $repo "scripts\$name") -Destination (Join-Path $package 'scripts')
}
Copy-Item -LiteralPath (Join-Path $repo 'Update.cmd') -Destination $package
Copy-Item -LiteralPath (Join-Path $repo 'docs\updating.md') -Destination (Join-Path $package 'README.md')
$null = Get-UpdatePlan $package $Mode
Write-Host "Validated $Mode update package: $package"
Write-Output $package
