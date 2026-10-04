param(
    # 此机器已有 MATLAB 随附的 LCC64；可传入其他 LCC64 安装目录。
    [string]$LccRoot = 'D:/Program Files/Polyspace/R2020b/sys/lcc64/lcc64'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$compiler = Join-Path $LccRoot 'bin/lcc64.exe'
$linker = Join-Path $LccRoot 'bin/lcclnk64.exe'
if (!(Test-Path -LiteralPath $compiler) -or !(Test-Path -LiteralPath $linker)) {
    throw 'LCC64 compiler/linker not found. Pass -LccRoot with its installation directory.'
}

# GPIO/按键测试编译真实 Core/Src/gpio.c，避免只验证替身中的引脚映射。
$outputDir = Join-Path $projectRoot 'build/host-tests/gpio-open-loop'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$flags = @('-noregistrylookup', "-I$LccRoot/include64", "-I$PSScriptRoot/stubs",
    "-I$projectRoot/Core/Inc", "-I$projectRoot/BSP/Inc", "-I$projectRoot/Application",
    "-I$projectRoot/App/Components/Input", "-I$projectRoot/App/Components/Output")
$sources = @('Tests/test_gpio_open_loop.c', 'Core/Src/gpio.c', 'BSP/Src/bsp_gpio.c',
    'App/Components/Input/button.c', 'App/Components/Output/led.c', 'Application/motor_open_loop.c')
$objects = @()
foreach ($source in $sources) {
    $objectName = [IO.Path]::GetFileNameWithoutExtension($source) + '.obj'
    $objects += $objectName
    if ($source -eq 'App/Components/Input/button.c') {
        # 旧版 LCC 不支持这个指针表的指定初始化。测试副本按字段声明顺序
        # 去掉初始化标记，保留正式驱动的配置值和状态机代码。
        $buttonSource = [IO.File]::ReadAllText((Join-Path $projectRoot $source))
        $fieldPattern = '\.(gpio_port|gpio_pin|active_high|debounce_ms|long_press_ms|repeat_ms)\s*=\s*'
        if ([regex]::Matches($buttonSource, $fieldPattern).Count -ne 24) {
            throw 'Button configuration changed; review LCC initializer compatibility conversion.'
        }
        $buttonCompatPath = Join-Path $outputDir 'button_lcc.c'
        [IO.File]::WriteAllText($buttonCompatPath, [regex]::Replace($buttonSource, $fieldPattern, ''))
        & $compiler @flags "-Fo$outputDir/$objectName" $buttonCompatPath
    } else {
        & $compiler @flags "-Fo$outputDir/$objectName" (Join-Path $projectRoot $source)
    }
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath "$outputDir/$objectName")) {
        throw "Compilation failed: $source"
    }
}
Push-Location $outputDir
try {
    & $linker "-L$LccRoot/lib64" -o test_gpio_open_loop.exe @objects
    if ($LASTEXITCODE -ne 0) { throw 'GPIO/open-loop test link failed.' }
    & .\test_gpio_open_loop.exe
    if ($LASTEXITCODE -ne 0) { throw 'GPIO/open-loop tests failed.' }
} finally {
    Pop-Location
}

foreach ($ppr in @(1, 2)) {
    $outputDir = Join-Path $projectRoot "build/host-tests/ppr-$ppr"
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
    $flags = @('-noregistrylookup', "-I$LccRoot/include64",
        "-I$PSScriptRoot/stubs", "-I$projectRoot/BSP/Inc",
        "-I$projectRoot/App/Components/Actuators", "-DBSP_SPEED_PULSES_PER_REV=$ppr")
    $sources = @('Tests/test_speed_motor.c', 'BSP/Src/bsp_timer.c',
        'BSP/Src/bsp_pwm.c', 'App/Components/Actuators/motor.c')
    $objects = @()
    foreach ($source in $sources) {
        $objectName = [IO.Path]::GetFileNameWithoutExtension($source) + '.obj'
        $objects += $objectName
        & $compiler @flags "-Fo$outputDir/$objectName" (Join-Path $projectRoot $source)
        if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath "$outputDir/$objectName")) {
            throw "Compilation failed: $source"
        }
    }
    # 此版本链接器从当前目录查找目标文件，故在产物目录中执行。
    Push-Location $outputDir
    try {
        & $linker "-L$LccRoot/lib64" -o test_speed_motor.exe @objects
        if ($LASTEXITCODE -ne 0) { throw 'Host test link failed.' }
        & .\test_speed_motor.exe
        if ($LASTEXITCODE -ne 0) { throw "Host tests failed (ppr=$ppr)." }
    } finally {
        Pop-Location
    }
}

$outputDir = Join-Path $projectRoot 'build/host-tests/control'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$flags = @('-noregistrylookup', "-I$LccRoot/include64", "-I$projectRoot/Application", "-I$projectRoot/Algorithm")
$objects = @()
foreach ($source in @('Tests/test_control.c', 'Application/motor_control.c', 'Algorithm/pid.c',
    'Application/state_machine.c', 'Application/system_config.c', 'Application/sensor_filter.c', 'Algorithm/filter.c')) {
    $objectName = [IO.Path]::GetFileNameWithoutExtension($source) + '.obj'
    $objects += $objectName
    & $compiler @flags "-Fo$outputDir/$objectName" (Join-Path $projectRoot $source)
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath "$outputDir/$objectName")) { throw "Compilation failed: $source" }
}
Push-Location $outputDir
try {
    & $linker "-L$LccRoot/lib64" -o test_control.exe @objects
    if ($LASTEXITCODE -ne 0) { throw 'Control test link failed.' }
    & .\test_control.exe
    if ($LASTEXITCODE -ne 0) { throw 'Control tests failed.' }
} finally { Pop-Location }

# 完整模式/UI 测试使用正式非阻塞蜂鸣器驱动，HAL 替身记录 GPIO 和 Delay 调用。
$outputDir = Join-Path $projectRoot 'build/host-tests/modes-ui'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$flags = @('-noregistrylookup', "-I$LccRoot/include64", "-I$PSScriptRoot/stubs",
    "-I$projectRoot/Core/Inc", "-I$projectRoot/Application", "-I$projectRoot/Algorithm",
    "-I$projectRoot/App/Components/Output")
$objects = @()
foreach ($source in @('Tests/test_modes_ui.c', 'Application/ui_model.c', 'Application/state_machine.c',
    'Application/system_config.c', 'Application/motor_control.c', 'Algorithm/pid.c',
    'App/Components/Output/buzzer.c')) {
    $objectName = [IO.Path]::GetFileNameWithoutExtension($source) + '.obj'
    $objects += $objectName
    & $compiler @flags "-Fo$outputDir/$objectName" (Join-Path $projectRoot $source)
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath "$outputDir/$objectName")) { throw "Compilation failed: $source" }
}
Push-Location $outputDir
try {
    & $linker "-L$LccRoot/lib64" -o test_modes_ui.exe @objects
    if ($LASTEXITCODE -ne 0) { throw 'Modes/UI test link failed.' }
    & .\test_modes_ui.exe
    if ($LASTEXITCODE -ne 0) { throw 'Modes/UI tests failed.' }
} finally { Pop-Location }

$outputDir = Join-Path $projectRoot 'build/host-tests/system'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$flags = @('-noregistrylookup', "-I$LccRoot/include64", "-I$projectRoot/Application", "-I$projectRoot/Algorithm", "-I$projectRoot/Utils")
$objects = @()
foreach ($source in @('Tests/test_system.c', 'Application/heartbeat.c', 'Application/system_config.c',
    'Application/persistent_store.c', 'Application/system_persist.c', 'Utils/crc.c')) {
    $objectName = [IO.Path]::GetFileNameWithoutExtension($source) + '.obj'
    $objects += $objectName
    & $compiler @flags "-Fo$outputDir/$objectName" (Join-Path $projectRoot $source)
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath "$outputDir/$objectName")) { throw "Compilation failed: $source" }
}
Push-Location $outputDir
try {
    & $linker "-L$LccRoot/lib64" -o test_system.exe @objects
    if ($LASTEXITCODE -ne 0) { throw 'System test link failed.' }
    & .\test_system.exe
    if ($LASTEXITCODE -ne 0) { throw 'System tests failed.' }
} finally { Pop-Location }

$outputDir = Join-Path $projectRoot 'build/host-tests/sensors'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$flags = @('-noregistrylookup', "-I$LccRoot/include64", "-I$PSScriptRoot/stubs",
    "-I$projectRoot/Core/Inc", "-I$projectRoot/BSP/Inc", "-I$projectRoot/App/Components/Sensors")
$objects = @()
foreach ($source in @('Tests/test_sensors.c', 'App/Components/Sensors/mq135.c',
    'App/Components/Sensors/anemometer.c', 'App/Components/Sensors/sht30.c', 'App/Components/Sensors/bmp280.c')) {
    $objectName = [IO.Path]::GetFileNameWithoutExtension($source) + '.obj'
    $objects += $objectName
    & $compiler @flags "-Fo$outputDir/$objectName" (Join-Path $projectRoot $source)
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath "$outputDir/$objectName")) { throw "Compilation failed: $source" }
}
Push-Location $outputDir
try {
    & $linker "-L$LccRoot/lib64" -o test_sensors.exe @objects
    if ($LASTEXITCODE -ne 0) { throw 'Sensor test link failed.' }
    & .\test_sensors.exe
    if ($LASTEXITCODE -ne 0) { throw 'Sensor tests failed.' }
} finally { Pop-Location }
