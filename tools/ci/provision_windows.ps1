# Provisions exact tools independently of GitHub; administrative installs are explicit.
# The runner itself must execute as an ordinary user in an interactive session.
[CmdletBinding()]
param([string]$Root = 'C:\SomniumCI', [switch]$Machine)
$ErrorActionPreference = 'Stop'
$pins = Get-Content "$PSScriptRoot/toolchain.json" -Raw | ConvertFrom-Json
$Root = [IO.Path]::GetFullPath($Root)
$downloads = Join-Path $Root 'tools/downloads'
New-Item -ItemType Directory -Force $downloads | Out-Null
function Get-PinnedFile($Pin) {
    $path = Join-Path $downloads ($Pin.sha256 + '-' + ($Pin.url.Split('/')[-1]))
    if (-not (Test-Path -LiteralPath $path)) {
        Invoke-WebRequest -Uri $Pin.url -OutFile ($path + '.partial') -UseBasicParsing
        Move-Item -LiteralPath ($path + '.partial') -Destination $path
    }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Pin.sha256) {
        throw "Checksum mismatch: $path. Remove the invalid download explicitly."
    }
    return $path
}
function Invoke-Installer($File, $Arguments) {
    $process = Start-Process -FilePath $File -ArgumentList $Arguments -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -notin 0,3010) { throw "Installer failed: $File ($($process.ExitCode))" }
    if ($process.ExitCode -eq 3010) { Write-Warning 'A reboot is required before running CI.' }
}
$python = Join-Path $Root 'tools/python/python.exe'
if (-not (Test-Path -LiteralPath $python)) {
    $archive = Get-PinnedFile $pins.Windows.python
    & tar.exe -xf $archive -C (Join-Path $Root 'tools')
    if ($LASTEXITCODE -ne 0) { throw 'Python extraction failed' }
}
& $python "$PSScriptRoot/provision.py" --root $Root
if ($LASTEXITCODE -ne 0) { throw 'Portable tool provisioning failed' }
if ($Machine) {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Run -Machine once from an administrator PowerShell. CI must never use an administrator account.'
    }
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    $found = @()
    if (Test-Path $vswhere) {
        $found = @((& $vswhere -all -products '*' -format json | ConvertFrom-Json) | Where-Object {
            Test-Path (Join-Path $_.installationPath "VC/Tools/MSVC/$($pins.msvc)/bin/Hostx64/x64/cl.exe")
        })
    }
    if ($found.Count -eq 0) {
        Invoke-Installer (Get-PinnedFile $pins.Windows.msvc) @('--quiet','--wait','--norestart','--nocache',
            '--installPath', ('"' + (Join-Path $Root 'tools/BuildTools') + '"'),
            '--addProductLang','en-US',
            '--add','Microsoft.VisualStudio.Workload.VCTools','--includeRecommended',
            '--add','Microsoft.VisualStudio.Component.Windows11SDK.26100')
    }
    # sccache 0.18 corrupts non-UTF8 localized /showIncludes output when piped.
    # Install English resources rather than losing Ninja's header dependencies.
    $instances = & $vswhere -all -products '*' -format json | ConvertFrom-Json
    $instance = $instances | Where-Object {
        Test-Path (Join-Path $_.installationPath "VC/Tools/MSVC/$($pins.msvc)/bin/Hostx64/x64/cl.exe")
    } | Select-Object -First 1
    if (-not $instance) { throw 'Pinned MSVC installation missing after provisioning' }
    $english = Join-Path $instance.installationPath "VC/Tools/MSVC/$($pins.msvc)/bin/Hostx64/x64/1033/clui.dll"
    if (-not (Test-Path $english)) {
        Invoke-Installer "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/setup.exe" @('modify',
            '--installPath', ('"' + $instance.installationPath + '"'), '--addProductLang','en-US','--quiet','--norestart')
        if (-not (Test-Path $english)) { throw 'English MSVC language pack missing after provisioning' }
    }
    $llvm = Join-Path $Root "tools/llvm-$($pins.clang_tidy_windows)"
    if (-not (Test-Path "$llvm/bin/clang-tidy.exe")) {
        Invoke-Installer (Get-PinnedFile $pins.Windows.llvm) @('/S',"/D=$llvm")
    }
    $vulkan = Join-Path $Root "tools/vulkan/$($pins.vulkan_sdk)"
    if (-not (Test-Path "$vulkan/Bin/VkLayer_khronos_validation.json")) {
        Invoke-Installer (Get-PinnedFile $pins.Windows.vulkan) @('--root',('"' + $vulkan + '"'),
            '--accept-licenses','--default-answer','--confirm-command','install')
    }
    $graphics = Get-WindowsCapability -Online -Name 'Tools.Graphics.DirectX*'
    if ($graphics.State -ne 'Installed') { $graphics | Add-WindowsCapability -Online | Out-Null }
}
Write-Host "Provisioning complete. Validate from the dedicated account: tools/ci/windows.ps1 doctor --root $Root"
