@echo off
setlocal
set "KEYKEY_PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if defined PROCESSOR_ARCHITEW6432 set "KEYKEY_PS=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
"%KEYKEY_PS%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Update-KeyKey.ps1" %*
pause
