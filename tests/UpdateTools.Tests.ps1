. (Join-Path (Split-Path -Parent $PSScriptRoot) 'scripts\UpdateCommon.ps1')

Describe 'Update package validation without modifying the installation' {
    function Assert-PackageRejected([string]$FixturePath) {
        $rejected = $false
        try { $null = Get-UpdatePlan $FixturePath Full } catch { $rejected = $true }
        $rejected | Should Be $true
    }
    function New-Fixture {
        $root = Join-Path $TestDrive ([Guid]::NewGuid().ToString('N'))
        $null = New-Item -ItemType Directory -Path (Join-Path $root 'app/data/opencc')
        $paths = @('app/McBopomofoServer_x64.exe','app/McBopomofoConfig_x64.exe',
            'app/McBopomofoTIP_x64.dll','app/McBopomofoTIP_x86.dll',
            'app/data/data.txt','app/data/data-plain-bpmf.txt','app/data/associated-phrases-v2.txt',
            'app/data/dictionary_service.json','app/data/bpmfvs-variants.txt','app/data/bpmfvs-pua.txt',
            'app/data/opencc/tw2s.json','app/data/opencc/TSCharacters.ocd2','app/data/opencc/TSPhrases.ocd2',
            'app/data/opencc/TWVariantsRev.ocd2','app/data/opencc/TWVariantsRevPhrases.ocd2')
        $files = foreach ($path in $paths) {
            $destination = Join-Path $root $path
            if ($path -match '\.(exe|dll)$') {
                $bytes = New-Object byte[] 128
                $bytes[0]=0x4d; $bytes[1]=0x5a; $bytes[0x3c]=64
                $bytes[64]=0x50; $bytes[65]=0x45
                $machine = if ($path -match 'x86') { [uint16]0x14c } else { [uint16]0x8664 }
                [BitConverter]::GetBytes($machine).CopyTo($bytes,68)
                [IO.File]::WriteAllBytes($destination,$bytes)
            } else { [IO.File]::WriteAllText($destination,'fixture') }
            @{path=$path;sha256=(Get-FileHash -LiteralPath $destination).Hash}
        }
        @{schema=1;ipc=1;architecture='x64';id='fixture';version='0.9.8.0';files=@($files)} |
            ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $root 'update-manifest.json')
        $root
    }
    It 'accepts a complete package for both modes' {
        $root = New-Fixture
        (Get-UpdatePlan $root Full).Mode | Should Be 'Full'
        (Get-UpdatePlan $root Backend).Files.Count | Should Be 15
    }
    It 'rejects tampered files' {
        $root = New-Fixture
        Add-Content -LiteralPath (Join-Path $root 'app/data/data.txt') -Value 'changed'
        Assert-PackageRejected $root
    }
    It 'rejects missing files' {
        $root = New-Fixture
        Remove-Item -LiteralPath (Join-Path $root 'app/McBopomofoTIP_x86.dll')
        Assert-PackageRejected $root
    }
    It 'rejects wrong PE architecture even with matching checksum' {
        $root = New-Fixture
        $dll = Join-Path $root 'app/McBopomofoTIP_x86.dll'
        Copy-Item -LiteralPath (Join-Path $root 'app/McBopomofoTIP_x64.dll') -Destination $dll -Force
        $manifest = Get-Content (Join-Path $root 'update-manifest.json') -Raw | ConvertFrom-Json
        ($manifest.files | Where-Object path -eq 'app/McBopomofoTIP_x86.dll').sha256 = (Get-FileHash $dll).Hash
        $manifest | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $root 'update-manifest.json')
        Assert-PackageRejected $root
    }
    It 'rejects traversal, duplicate paths, and unsupported protocol' {
        foreach ($case in @('traversal','duplicate','protocol')) {
            $root = New-Fixture
            $file = Join-Path $root 'update-manifest.json'
            $manifest = Get-Content $file -Raw | ConvertFrom-Json
            switch ($case) {
                traversal { $manifest.files[0].path='app/../outside.exe' }
                duplicate { $manifest.files += $manifest.files[0] }
                protocol { $manifest.ipc=99 }
            }
            $manifest | ConvertTo-Json -Depth 5 | Set-Content $file
            Assert-PackageRejected $root
        }
    }
    It 'requires full TIP files only for Full mode' {
        $root = New-Fixture
        $file = Join-Path $root 'update-manifest.json'
        $manifest = Get-Content $file -Raw | ConvertFrom-Json
        $manifest.files = @($manifest.files | Where-Object { $_.path -notmatch 'TIP' })
        $manifest | ConvertTo-Json -Depth 5 | Set-Content $file
        (Get-UpdatePlan $root Backend).Mode | Should Be 'Backend'
        Assert-PackageRejected $root
    }
    It 'does not mutate the package during planning' {
        $root = New-Fixture
        $before = Get-FileHash (Join-Path $root 'update-manifest.json')
        $null = Get-UpdatePlan $root Full
        (Get-FileHash $before.Path).Hash | Should Be $before.Hash
        (Get-ChildItem $root -File -Recurse).Count | Should Be 16
    }
}
