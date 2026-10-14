param([ValidateSet('Debug', 'Release', 'Upgrade')][string]$Preset = 'Upgrade')
$ErrorActionPreference = 'Stop'
# 工具与固件分目录存放，从脚本位置定位工程，不依赖调用者当前目录。
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$projectRoot = Join-Path $repositoryRoot 'firmware'
# 使用已安装的 STM32Cube 工具；正常 PATH 已有工具时也可直接使用。
$bundleRoot = Join-Path $env:LOCALAPPDATA 'stm32cube/bundles'
function Find-BundleTool([string]$family, [string]$name) {
    $root = Join-Path $bundleRoot $family
    $candidates = Get-ChildItem -Path $root -Filter $name -Recurse -File -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending
    if ($candidates) { return $candidates[0].FullName }
    $command = Get-Command $name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    throw "Missing tool: $name"
}
$cmake = Find-BundleTool 'cmake' 'cmake.exe'
$gcc = Find-BundleTool 'gnu-tools-for-stm32' 'arm-none-eabi-gcc.exe'
$ninja = Find-BundleTool 'ninja' 'ninja.exe'
$originalPath = $env:Path
Push-Location $projectRoot
try {
    $env:Path = (Split-Path $gcc) + ';' + (Split-Path $ninja) + ';' + $originalPath
    & $cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & $cmake --build --preset $Preset --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Firmware build failed.' }
} finally {
    $env:Path = $originalPath
    Pop-Location
}
