#requires -Version 5.1
[CmdletBinding()]
param([string]$ToolchainRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectDirectory = Split-Path $PSScriptRoot -Parent
$temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
$fixture = Join-Path $temporaryRoot ('kernelcraft build test ' + [Guid]::NewGuid().ToString('N'))
$powershell = Join-Path ([Environment]::SystemDirectory) 'WindowsPowerShell/v1.0/powershell.exe'
$completed = $false
$buildNumber = 0

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Invoke-Build([string[]]$BuildArguments = @(), [switch]$ExpectFailure) {
    $script:buildNumber++
    $log = Join-Path $fixture "build-$script:buildNumber.log"
    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $fixture 'build.ps1')) + $BuildArguments
    if ($ToolchainRoot) { $arguments += @('-ToolchainRoot', $ToolchainRoot) }
    $timer = [Diagnostics.Stopwatch]::StartNew()
    # Expected compiler errors arrive on stderr through the child PowerShell.
    $savedPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $powershell @arguments > $log 2>&1
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $savedPreference }
    $timer.Stop()
    Write-Host ("Build {0}: {1:N2}s (exit {2})" -f $script:buildNumber, $timer.Elapsed.TotalSeconds, $code)
    if (($code -ne 0) -ne [bool]$ExpectFailure) {
        Get-Content -LiteralPath $log | Write-Host
        throw "Unexpected build result; see $log"
    }
}

function Get-Stamp([string]$Path) {
    Assert-True ([IO.File]::Exists($Path)) "Expected compiled object: $Path"
    return [IO.File]::GetLastWriteTimeUtc($Path).Ticks
}

try {
    New-Item -ItemType Directory -Path $fixture | Out-Null
    foreach ($name in @('build.ps1', 'src', 'libs', 'tests', 'LICENSE')) {
        Copy-Item -LiteralPath (Join-Path $projectDirectory $name) -Destination $fixture -Recurse
    }
    $releaseObjects = Join-Path $fixture 'obj/windows/Release'
    $worldObject = Join-Path $releaseObjects 'src/world/world.o'
    $optionsObject = Join-Path $releaseObjects 'src/utils/options.o'
    $worldDependency = Join-Path $releaseObjects 'src/world/world.d'
    $worldSettings = Join-Path $releaseObjects 'src/world/world.settings'
    $header = Join-Path $fixture 'src/world/chunk.h'
    $source = Join-Path $fixture 'src/utils/options.c'

    Invoke-Build
    $worldStamp = Get-Stamp $worldObject
    $optionsStamp = Get-Stamp $optionsObject
    Invoke-Build
    Assert-True ((Get-Stamp $worldObject) -eq $worldStamp) 'Unchanged builds must reuse world objects'
    Assert-True ((Get-Stamp $optionsObject) -eq $optionsStamp) 'Unchanged builds must reuse options objects'

    # Relative CPATH selects different headers when the caller changes directory.
    $savedIncludePath = $env:CPATH
    $originalSourceBytes = [IO.File]::ReadAllBytes($source)
    try {
        foreach ($choice in @('a', 'b')) {
            $include = Join-Path $fixture "caller-$choice/inc"
            New-Item -ItemType Directory -Path $include -Force | Out-Null
            $choiceHeader = Join-Path $include 'choice.h'
            $value = if ($choice -eq 'a') { 11 } else { 22 }
            [IO.File]::WriteAllText($choiceHeader, "#define BUILD_CHOICE $value`n")
            [IO.File]::SetLastWriteTimeUtc($choiceHeader, [DateTime]::UtcNow.AddDays(-1))
        }
        [IO.File]::AppendAllText($source, "`n#include <choice.h>`nint build_fixture_choice(void) { return BUILD_CHOICE; }`n")
        $env:CPATH = 'inc'
        Push-Location (Join-Path $fixture 'caller-a')
        try { Invoke-Build } finally { Pop-Location }
        $firstChoiceHash = (Get-FileHash -LiteralPath $optionsObject).Hash
        Push-Location (Join-Path $fixture 'caller-b')
        try {
            Invoke-Build
            Assert-True ((Get-FileHash -LiteralPath $optionsObject).Hash -ne $firstChoiceHash) 'A new caller directory must compile its selected relative header'
            $optionsStamp = Get-Stamp $optionsObject
            # An in-process script uses PowerShell's location even when .NET cwd differs.
            $directArguments = @{}
            if ($ToolchainRoot) { $directArguments.ToolchainRoot = $ToolchainRoot }
            & (Join-Path $fixture 'build.ps1') @directArguments *> (Join-Path $fixture 'in-process.log')
            Assert-True ((Get-Stamp $optionsObject) -eq $optionsStamp) 'In-process builds must resolve relative dependencies against the compiler directory'
        } finally { Pop-Location }
    } finally {
        $env:CPATH = $savedIncludePath
        [IO.File]::WriteAllBytes($source, $originalSourceBytes)
    }
    Invoke-Build
    $worldStamp = Get-Stamp $worldObject
    $optionsStamp = Get-Stamp $optionsObject

    [IO.File]::SetLastWriteTimeUtc($header, [DateTime]::UtcNow)
    Invoke-Build
    Assert-True ((Get-Stamp $worldObject) -gt $worldStamp) 'Header changes must rebuild callers'
    Assert-True ((Get-Stamp $optionsObject) -eq $optionsStamp) 'Header changes must preserve unrelated objects'
    $worldStamp = Get-Stamp $worldObject

    [IO.File]::SetLastWriteTimeUtc($source, [DateTime]::UtcNow)
    Invoke-Build
    Assert-True ((Get-Stamp $optionsObject) -gt $optionsStamp) 'Source changes must rebuild their object'
    Assert-True ((Get-Stamp $worldObject) -eq $worldStamp) 'Source changes must preserve unrelated objects'

    Remove-Item -LiteralPath $optionsObject
    Invoke-Build
    $optionsStamp = Get-Stamp $optionsObject
    Assert-True ((Get-Stamp $worldObject) -eq $worldStamp) 'A missing object must not invalidate unrelated objects'

    [IO.File]::WriteAllText($worldDependency, 'invalid dependency data')
    Invoke-Build
    Assert-True ((Get-Stamp $worldObject) -gt $worldStamp) 'Damaged dependencies must trigger recompilation'
    $worldStamp = Get-Stamp $worldObject
    Remove-Item -LiteralPath $worldDependency
    Invoke-Build
    Assert-True ((Get-Stamp $worldObject) -gt $worldStamp) 'Missing dependencies must trigger recompilation'
    $worldStamp = Get-Stamp $worldObject
    [IO.File]::WriteAllText($worldSettings, 'invalid compiler settings')
    Invoke-Build
    Assert-True ((Get-Stamp $worldObject) -gt $worldStamp) 'Damaged compiler settings must trigger recompilation'
    $worldStamp = Get-Stamp $worldObject

    $headerBytes = [IO.File]::ReadAllBytes($header)
    Remove-Item -LiteralPath $header
    Invoke-Build -ExpectFailure
    Assert-True ((Get-Stamp $worldObject) -eq $worldStamp) 'A missing header must not replace a valid object'
    [IO.File]::WriteAllBytes($header, $headerBytes)
    Invoke-Build
    Assert-True ((Get-Stamp $worldObject) -gt $worldStamp) 'Restoring a header must recover without cleaning'
    $worldStamp = Get-Stamp $worldObject

    $sourceBytes = [IO.File]::ReadAllBytes($source)
    $optionsStamp = Get-Stamp $optionsObject
    [IO.File]::AppendAllText($source, "`n#error deliberate compilation failure`n")
    Invoke-Build -ExpectFailure
    Assert-True ((Get-Stamp $optionsObject) -eq $optionsStamp) 'Failed compilation must preserve the previous object'
    [IO.File]::WriteAllBytes($source, $sourceBytes)
    Invoke-Build
    Assert-True ((Get-Stamp $optionsObject) -gt $optionsStamp) 'Restoring a source must recover without cleaning'

    $buildScript = Join-Path $fixture 'build.ps1'
    $originalScript = [IO.File]::ReadAllText($buildScript)
    $changedScript = $originalScript.Replace("'-O2'", "'-O1'")
    Assert-True ($changedScript -ne $originalScript) 'The fixture must change the Release compiler option'
    [IO.File]::WriteAllText($buildScript, $changedScript)
    Invoke-Build
    Assert-True ((Get-Stamp $worldObject) -gt $worldStamp) 'Changed compiler options must invalidate cached objects'
    [IO.File]::WriteAllText($buildScript, $originalScript)
    Invoke-Build
    $worldStamp = Get-Stamp $worldObject

    # Change a compiler search option without editing the build script.
    $savedIncludePath = $env:CPATH
    try {
        $includeDirectory = Join-Path $fixture 'extra includes'
        New-Item -ItemType Directory -Path $includeDirectory | Out-Null
        $env:CPATH = $includeDirectory
        if ($savedIncludePath) { $env:CPATH += ';' + $savedIncludePath }
        Invoke-Build
        Assert-True ((Get-Stamp $worldObject) -gt $worldStamp) 'Compiler search environment changes must invalidate cached objects'
    } finally { $env:CPATH = $savedIncludePath }
    Invoke-Build
    $worldStamp = Get-Stamp $worldObject

    Invoke-Build @('-Configuration', 'Debug')
    $debugObject = Join-Path $fixture 'obj/windows/Debug/src/world/world.o'
    $debugStamp = Get-Stamp $debugObject
    Assert-True ((Get-Stamp $worldObject) -eq $worldStamp) 'Debug builds must preserve Release objects'
    Invoke-Build @('-Clean')
    Assert-True (-not (Test-Path -LiteralPath $releaseObjects)) 'Clean must remove the selected object directory'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $fixture 'bin/windows/Release'))) 'Clean must remove the selected output directory'
    Assert-True ((Get-Stamp $debugObject) -eq $debugStamp) 'Release clean must preserve Debug objects'
    Assert-True (Test-Path -LiteralPath (Join-Path $fixture 'bin/windows/Debug/minecraft_clone.exe')) 'Release clean must preserve Debug executables'
    Invoke-Build
    $null = Get-Stamp $worldObject
    Write-Host 'Windows build regression tests passed'
    $completed = $true
} finally {
    if ($completed) {
        $resolvedFixture = [IO.Path]::GetFullPath($fixture)
        if (-not $resolvedFixture.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase) -or
            (Split-Path $resolvedFixture -Leaf) -notlike 'kernelcraft build test *' -or
            ((Get-Item -LiteralPath $resolvedFixture).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing to remove an unexpected fixture path: $resolvedFixture"
        }
        Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
    } else {
        Write-Host "Build regression fixture retained: $fixture"
    }
}
