param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [string]$BuildDir = "build_dx12",
    [switch]$Reconfigure
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

function Assert-LastExit {
    param([string]$Message)
    if ($LASTEXITCODE -ne 0) {
        throw "$Message (exit $LASTEXITCODE)"
    }
}

function Assert-CacheValue {
    param(
        [string]$CachePath,
        [string]$Regex,
        [string]$FailureMessage
    )

    if (-not (Select-String -Path $CachePath -Pattern $Regex -Quiet)) {
        throw $FailureMessage
    }
}

if ($Reconfigure -or -not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
    Write-Host "[ConfigureIntelliSenseBuild]"
    cmake -S . -B $BuildDir -G "Visual Studio 17 2022" -A x64 `
        -DWB_BUILD_DX12_DEMO=ON `
        -DWB_BUILD_STANDALONE=ON `
        -DBUILD_TESTING=ON `
        -DCMAKE_CXX_STANDARD=17 | Out-Host
    Assert-LastExit "CMake configure failed"
}

$cachePath = Join-Path $BuildDir "CMakeCache.txt"
if (-not (Test-Path $cachePath)) {
    throw "Missing CMake cache: $cachePath"
}

Write-Host "[ValidateIntelliSenseContext]"
Assert-CacheValue -CachePath $cachePath -Regex '^WB_BUILD_DX12_DEMO:BOOL=ON$' `
    -FailureMessage "WB_BUILD_DX12_DEMO is OFF in $cachePath"
Assert-CacheValue -CachePath $cachePath -Regex '^CMAKE_CXX_STANDARD:.*=17$' `
    -FailureMessage "CMAKE_CXX_STANDARD is not 17 in $cachePath"

Write-Host "[BuildDockFrameworkDemo]"
cmake --build $BuildDir --target dock_framework_demo --config $Config | Out-Host
Assert-LastExit "dock_framework_demo build failed"

Write-Host "[BuildDX12Demo]"
cmake --build $BuildDir --target dx12_demo --config $Config | Out-Host
Assert-LastExit "dx12_demo build failed"

Write-Host "[IntelliSenseSanity] PASS"
Write-Host "If squiggles remain in VS Code:"
Write-Host "  1. Run: CMake: Configure"
Write-Host "  2. Run: C/C++: Reset IntelliSense Database"
Write-Host "  3. Run: Developer: Reload Window"
