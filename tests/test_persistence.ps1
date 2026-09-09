#requires -Version 5.1
param([Parameter(Mandatory)][string]$Binary, [Parameter(Mandatory)][string]$SmokeBinary)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Binary = (Resolve-Path -LiteralPath $Binary).Path
$SmokeBinary = (Resolve-Path -LiteralPath $SmokeBinary).Path
$temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$fixture = [IO.Path]::GetFullPath((Join-Path $temporaryRoot ('kernelcraft-restart-' + [Guid]::NewGuid().ToString('N'))))
New-Item -ItemType Directory -Path $fixture | Out-Null
$savedPhase = $env:KERNELCRAFT_TEST_RESTART
$savedWorld = $env:KERNELCRAFT_TEST_WORLD

function Invoke-Expected([int]$Expected, [string[]]$Arguments, [string]$Message = '', [string]$Program = $Binary) {
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& $Program @Arguments 2>&1)
        $status = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    $text = ($output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
    if ($status -ne $Expected -or ($Message -and $text -notmatch $Message)) {
        throw "Expected exit $Expected and '$Message', got $status : $text"
    }
    Write-Host $text.TrimEnd()
}

Push-Location $fixture
try {
    $sentinel = Join-Path $fixture 'kernelcraft.kcw'
    [IO.File]::WriteAllText($sentinel, 'existing default save sentinel')
    Invoke-Expected 0 @('--no-save') -Program $SmokeBinary
    if ([IO.File]::ReadAllText($sentinel) -ne 'existing default save sentinel') { throw 'Temporary session changed the default save' }
    $env:KERNELCRAFT_TEST_WORLD = Join-Path $fixture 'world with spaces.kcw'
    $env:KERNELCRAFT_TEST_RESTART = 'save'
    Invoke-Expected 0 @('--world', 'world with spaces.kcw', '--seed', '42')
    $originalHash = (Get-FileHash -LiteralPath $env:KERNELCRAFT_TEST_WORLD).Hash
    $env:KERNELCRAFT_TEST_RESTART = 'load'
    Invoke-Expected 0 @('--world', 'world with spaces.kcw')
    if ((Get-FileHash -LiteralPath $env:KERNELCRAFT_TEST_WORLD).Hash -ne $originalHash) { throw 'Restart changed the saved world' }
    $standardWorld = $env:KERNELCRAFT_TEST_WORLD
    $env:KERNELCRAFT_TEST_WORLD = Join-Path $fixture 'crouched.kcw'
    $env:KERNELCRAFT_TEST_RESTART = 'crouch-save'
    Invoke-Expected 0 @('--world', 'crouched.kcw', '--seed', '42')
    $crouchedHash = (Get-FileHash -LiteralPath $env:KERNELCRAFT_TEST_WORLD).Hash
    $env:KERNELCRAFT_TEST_RESTART = 'crouch-load'
    Invoke-Expected 0 @('--world', 'crouched.kcw')
    if ((Get-FileHash -LiteralPath $env:KERNELCRAFT_TEST_WORLD).Hash -ne $crouchedHash) { throw 'Restart changed the crouched save' }
    $env:KERNELCRAFT_TEST_WORLD = $standardWorld
    $env:KERNELCRAFT_TEST_RESTART = 'load'
    Invoke-Expected 1 @('--world', 'world with spaces.kcw', '--seed', '7') 'existing save'
    if ((Get-FileHash -LiteralPath $env:KERNELCRAFT_TEST_WORLD).Hash -ne $originalHash) { throw 'Rejected seed changed the save' }
    $env:KERNELCRAFT_TEST_RESTART = 'fail'
    $env:KERNELCRAFT_TEST_WORLD = Join-Path $fixture 'missing/world.kcw'
    Invoke-Expected 1 @('--world', 'missing/world.kcw', '--seed', '42') 'Application save failure status and cleanup checks passed'
    if (Test-Path -LiteralPath (Join-Path $fixture 'missing')) { throw 'Failed save created an unexpected directory' }
    [IO.File]::WriteAllText((Join-Path $fixture 'corrupt.kcw'), 'broken save')
    Invoke-Expected 1 @('--world', 'corrupt.kcw') 'Cannot load world'
    if ([IO.File]::ReadAllText((Join-Path $fixture 'corrupt.kcw')) -ne 'broken save') { throw 'Corrupt save was replaced' }
    Invoke-Expected 1 @('--seed', '-1') 'decimal integer'
    Invoke-Expected 0 @('--help') '--world'
    if ([IO.File]::ReadAllText($sentinel) -ne 'existing default save sentinel') { throw 'Changed the default save' }
    Write-Host 'Native process restart, launch-relative path, preserved saves, and CLI checks passed'
} finally {
    Pop-Location
    $env:KERNELCRAFT_TEST_RESTART = $savedPhase
    $env:KERNELCRAFT_TEST_WORLD = $savedWorld
    $boundedRoot = $temporaryRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $fixture.StartsWith($boundedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        ((Get-Item -LiteralPath $fixture).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing to remove an unexpected fixture directory: $fixture"
    }
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
