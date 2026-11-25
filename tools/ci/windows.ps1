# Loads the pinned native tool paths and MSVC environment for local or CI use.
# Pass exactly the same CLI arguments to run.py on both paths.
$ErrorActionPreference = 'Stop'
$taskArguments = @($args)
$root = if ($env:SOMNIUM_CI_ROOT) { $env:SOMNIUM_CI_ROOT } else { 'C:\SomniumCI' }
for ($i = 0; $i -lt $taskArguments.Count - 1; ++$i) {
    if ($taskArguments[$i] -eq '--root') { $root = $taskArguments[$i + 1] }
}
$python = Join-Path $root 'tools/python/python.exe'
if (-not (Test-Path $python)) { throw "Run tools/ci/provision_windows.ps1 -Root $root first" }
$pins = Get-Content "$PSScriptRoot/toolchain.json" -Raw | ConvertFrom-Json
$env:PATH = @("$root/tools/bin", "$root/tools/python/Lib/site-packages/cmake/data/bin",
    "$root/tools/python/Lib/site-packages/clang_format/data/bin", "$root/tools/python/Scripts",
    "$root/tools/llvm-$($pins.clang_tidy_windows)/bin", $env:PATH) -join ';'
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$instances = & $vswhere -all -products '*' -format json | ConvertFrom-Json
$vs = $instances | Where-Object { Test-Path (Join-Path $_.installationPath "VC/Tools/MSVC/$($pins.msvc)/bin/Hostx64/x64/cl.exe") } | Select-Object -First 1
if (-not $vs) { throw "MSVC $($pins.msvc) missing; run the -Machine provisioning pass" }
if ($vs.installationVersion -ne $pins.visual_studio) { throw "Expected Visual Studio build $($pins.visual_studio), got $($vs.installationVersion)" }
$module = Join-Path $vs.installationPath 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll'
Import-Module $module
Enter-VsDevShell -VsInstallPath $vs.installationPath -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64 -vcvars_ver=$($pins.msvc) -winsdk=$($pins.windows_sdk)" | Out-Null
$env:SOMNIUM_CI_ROOT = $root
$env:PYTHONUTF8 = '1'
& $python "$PSScriptRoot/run.py" @taskArguments
exit $LASTEXITCODE
