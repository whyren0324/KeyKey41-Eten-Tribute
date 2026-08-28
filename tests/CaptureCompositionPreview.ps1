param(
    [ValidateSet("color", "keykey")]
    [string]$Mode,
    [string]$OutputPath
)

$ErrorActionPreference = "Stop"
$previewExe = Join-Path $PSScriptRoot "..\build\bin\Release\CompositionDisplayPreview.exe"
if (-not (Test-Path -LiteralPath $previewExe)) {
    throw "Preview executable not found: $previewExe"
}

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class KeyKeyPreviewNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
}
"@

$process = Start-Process -FilePath $previewExe -ArgumentList $Mode -PassThru
try {
    $handle = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 50; $attempt++) {
        Start-Sleep -Milliseconds 100
        $process.Refresh()
        $handle = $process.MainWindowHandle
        if ($handle -ne [IntPtr]::Zero) { break }
    }
    if ($handle -eq [IntPtr]::Zero) {
        throw "Preview window did not appear."
    }

    Start-Sleep -Milliseconds 500
    $rect = New-Object KeyKeyPreviewNative+RECT
    if (-not [KeyKeyPreviewNative]::GetWindowRect($handle, [ref]$rect)) {
        throw "Could not read preview window bounds."
    }

    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    $bitmap = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0,
            (New-Object System.Drawing.Size $width, $height))
        $fullOutputPath = [System.IO.Path]::GetFullPath($OutputPath)
        $directory = [System.IO.Path]::GetDirectoryName($fullOutputPath)
        [System.IO.Directory]::CreateDirectory($directory) | Out-Null
        $bitmap.Save($fullOutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
} finally {
    if (-not $process.HasExited) {
        $process.CloseMainWindow() | Out-Null
        if (-not $process.WaitForExit(2000)) {
            $process.Kill()
        }
    }
}
