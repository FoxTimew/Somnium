# Interactive listener entry point, copied beside run.cmd during registration.
# Pinned Git LFS must be visible before checkout, which precedes the CI scripts.
[CmdletBinding()]
param([string]$Root = 'C:\SomniumCI', [string]$RunnerDirectory = 'C:\ActionsRunner')
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'The runner must not run elevated'
}
if ([Diagnostics.Process]::GetCurrentProcess().SessionId -eq 0) {
    throw 'The runner requires an interactive session'
}
$listenerPath = [IO.Path]::GetFullPath((Join-Path $RunnerDirectory 'bin/Runner.Listener.exe'))
$existingListener = Get-Process -Name 'Runner.Listener' -ErrorAction SilentlyContinue | Where-Object {
    $_.Path -eq $listenerPath
}
if ($existingListener) {
    Write-Host 'The Windows runner is already running'
    exit 0
}
if (-not (Test-Path "$Root/tools/bin/git-lfs.exe")) { throw 'Run provisioning first' }
$env:PATH = "$Root/tools/bin;$env:PATH"
$env:SOMNIUM_CI_ROOT = $Root
Set-Location -LiteralPath $RunnerDirectory
& ./run.cmd
exit $LASTEXITCODE
