#requires -Version 5.1
[CmdletBinding()]
param(
    [switch]$Run,
    [switch]$Test,
    [switch]$Benchmark,
    [switch]$Clean,
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [string]$ToolchainRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') {
    throw 'Use make on Linux. This script builds native Windows executables.'
}

$projectDirectory = $PSScriptRoot
$outputDirectory = [IO.Path]::GetFullPath((Join-Path $projectDirectory "bin/windows/$Configuration"))

if ($Clean) {
    # Only remove this configuration's generated directory, never a junction target.
    $outputRoot = [IO.Path]::GetFullPath((Join-Path $projectDirectory 'bin/windows')) + [IO.Path]::DirectorySeparatorChar
    if (-not $outputDirectory.StartsWith($outputRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Output directory is outside the build root: $outputDirectory"
    }
    for ($directory = $outputDirectory; $directory -and $directory -ne $projectDirectory; $directory = Split-Path $directory -Parent) {
        if ((Test-Path -LiteralPath $directory) -and
            ((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing to clean through a junction or symbolic link: $directory"
        }
    }
    if (Test-Path -LiteralPath $outputDirectory) {
        Remove-Item -LiteralPath $outputDirectory -Recurse -Force
    }
    Write-Host "Cleaned $outputDirectory"
    return
}

function Test-Toolchain([string]$Directory) {
    foreach ($relativePath in @('bin/gcc.exe', 'bin/objdump.exe', 'include/GL/glew.h', 'include/GL/freeglut.h', 'include/GLFW/glfw3.h')) {
        if (-not (Test-Path -LiteralPath (Join-Path $Directory $relativePath))) { return $false }
    }
    return $true
}

if (-not $ToolchainRoot) {
    $candidates = @()
    if ($env:KERNELCRAFT_TOOLCHAIN) { $candidates += $env:KERNELCRAFT_TOOLCHAIN }
    $gccOnPath = Get-Command gcc.exe -ErrorAction SilentlyContinue
    if ($gccOnPath) { $candidates += Split-Path (Split-Path $gccOnPath.Source -Parent) -Parent }
    $candidates += @("$env:SystemDrive/msys64/ucrt64", "$env:SystemDrive/msys64/mingw64")
    foreach ($candidate in $candidates) {
        if (Test-Toolchain $candidate) {
            $ToolchainRoot = $candidate
            break
        }
    }
}
if (-not $ToolchainRoot -or -not (Test-Toolchain $ToolchainRoot)) {
    throw 'A MinGW toolchain with GCC, GLFW, GLEW, and freeglut is required. See docs/windows.md. Use -ToolchainRoot for a custom installation.'
}

$toolchainDirectory = (Resolve-Path -LiteralPath $ToolchainRoot).Path
$compilerDirectory = Join-Path $toolchainDirectory 'bin'
$compiler = Join-Path $compilerDirectory 'gcc.exe'
$objdump = Join-Path $compilerDirectory 'objdump.exe'
$savedPath = $env:PATH

function Invoke-Native([string]$Program, [string[]]$Arguments = @()) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE"
    }
}

function Copy-RuntimeLibraries([string[]]$Executables) {
    $pending = [Collections.Generic.Queue[string]]::new()
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($executable in $Executables) { $pending.Enqueue($executable) }
    while ($pending.Count) {
        $binary = $pending.Dequeue()
        $imports = & $objdump -p $binary
        if ($LASTEXITCODE -ne 0) { throw "Cannot inspect DLL dependencies: $binary" }
        foreach ($line in $imports) {
            if ($line -notmatch '^\s*DLL Name:\s+(.+?)\s*$') { continue }
            $dll = $Matches[1]
            if (-not $seen.Add($dll)) { continue }
            # Use the system OpenGL driver even if a software renderer is installed in the toolchain.
            if ($dll -ieq 'OPENGL32.dll') { continue }
            $source = Join-Path $compilerDirectory $dll
            if (Test-Path -LiteralPath $source) {
                Copy-Item -LiteralPath $source -Destination $outputDirectory -Force
                $pending.Enqueue($source)
            } elseif ($dll -notmatch '^(api-ms-win-|ext-ms-)' -and
                      -not (Test-Path -LiteralPath (Join-Path ([Environment]::SystemDirectory) $dll))) {
                throw "Missing runtime dependency $dll required by $binary"
            }
        }
    }
}

try {
    $env:PATH = "$compilerDirectory;$savedPath"
    $machine = & $compiler -dumpmachine
    if ($LASTEXITCODE -ne 0 -or $machine -ne 'x86_64-w64-mingw32') {
        throw 'Use the 64-bit MinGW GCC from MSYS2 UCRT64 or MINGW64, not the MSYS compiler.'
    }
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    $flags = @('-std=c11', '-Wall', '-Wformat=2', '-Wstrict-prototypes', '-Werror', "-I$(Join-Path $projectDirectory 'src')")
    if ($Configuration -eq 'Release') { $flags += '-O2' } else { $flags += @('-O0', '-g3') }
    $libraries = @('-lopengl32', '-lglfw3', '-lglew32', '-lfreeglut', '-lm')
    $sources = @(Get-ChildItem -LiteralPath (Join-Path $projectDirectory 'src') -Filter '*.c' -Recurse -File | Sort-Object FullName | ForEach-Object FullName)
    $commonSources = @($sources | Where-Object { $_ -ne (Join-Path $projectDirectory 'src/main.c') })
    $game = Join-Path $outputDirectory 'minecraft_clone.exe'
    $executables = @($game)

    Write-Host "Building $Configuration with $compiler"
    Invoke-Native $compiler ($flags + $sources + @('-o', $game) + $libraries)

    if ($Test) {
        $worldTest = Join-Path $outputDirectory 'test-world.exe'
        $worldSources = @('tests/test_world.c', 'src/world/chunk.c', 'src/world/edit.c', 'src/world/cube.c', 'src/world/mesh.c', 'src/world/world.c', 'src/math/math.c', 'src/graphics/frustum.c', 'src/utils/raycast.c') |
            ForEach-Object { Join-Path $projectDirectory $_ }
        Invoke-Native $compiler ($flags + $worldSources + @('-o', $worldTest, '-lm'))

        $editTest = Join-Path $outputDirectory 'test-edits.exe'
        $editSources = @((Join-Path $projectDirectory 'tests/test_edits.c')) + @($worldSources | Select-Object -Skip 1)
        Invoke-Native $compiler ($flags + $editSources + @('-o', $editTest, '-lm'))

        $selectionTest = Join-Path $outputDirectory 'test-selection.exe'
        $selectionSources = @((Join-Path $projectDirectory 'tests/test_selection.c')) + @($worldSources | Select-Object -Skip 1)
        Invoke-Native $compiler ($flags + $selectionSources + @('-o', $selectionTest, '-lm'))

        $shaderTest = Join-Path $outputDirectory 'test-shader.exe'
        $shaderSources = @('tests/test_shader.c', 'src/graphics/shader.c', 'src/graphics/texture.c') | ForEach-Object { Join-Path $projectDirectory $_ }
        Invoke-Native $compiler ($flags + $shaderSources + @('-o', $shaderTest) + $libraries)

        $smokeTest = Join-Path $outputDirectory 'test-startup.exe'
        $smokeFlags = @('-Wl,--wrap=glfwCreateWindow', '-Wl,--wrap=glfwWindowShouldClose', '-Wl,--wrap=glfwSetInputMode', '-Wl,--wrap=glfwDestroyWindow', '-Wl,--wrap=glfwGetInputMode', '-Wl,--wrap=glfwGetWindowAttrib', '-Wl,--wrap=glfwGetKey', '-Wl,--wrap=glfwGetFramebufferSize', '-Wl,--wrap=glfwWaitEvents', '-Wl,--wrap=glfwSwapBuffers', '-Wl,--wrap=glfwGetTime')
        Invoke-Native $compiler ($flags + $sources + @((Join-Path $projectDirectory 'tests/app_smoke.c')) + $smokeFlags + @('-o', $smokeTest) + $libraries)
        $executables += @($worldTest, $editTest, $selectionTest, $shaderTest, $smokeTest)
    }
    if ($Test -or $Benchmark) {
        $renderTest = Join-Path $outputDirectory 'benchmark.exe'
        $benchmarkFlags = @('-Wl,--wrap=glDrawArrays', '-Wl,--wrap=glDrawElements')
        Invoke-Native $compiler ($flags + @((Join-Path $projectDirectory 'tests/render_benchmark.c')) + $commonSources + $benchmarkFlags + @('-o', $renderTest) + $libraries)
        $executables += $renderTest
    }

    Copy-RuntimeLibraries $executables
    Copy-Item -LiteralPath (Join-Path $projectDirectory 'src/assets') -Destination $outputDirectory -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $projectDirectory 'LICENSE') -Destination $outputDirectory -Force
    $licenseDirectory = Join-Path $outputDirectory 'licenses'
    New-Item -ItemType Directory -Path $licenseDirectory -Force | Out-Null
    foreach ($package in @('freeglut', 'glew', 'glfw', 'gcc-libs', 'libwinpthread', 'winpthreads')) {
        $licenseSource = Join-Path $toolchainDirectory "share/licenses/$package"
        if (Test-Path -LiteralPath $licenseSource) {
            Copy-Item -LiteralPath $licenseSource -Destination $licenseDirectory -Recurse -Force
        }
    }

    # Exercise the copied DLLs without finding development libraries through PATH.
    $env:PATH = [Environment]::SystemDirectory + ';' + $env:SystemRoot
    if ($Test) {
        Push-Location $outputDirectory
        try {
            Invoke-Native $worldTest
            Invoke-Native $editTest
            Invoke-Native $selectionTest
            Invoke-Native $shaderTest
        } finally { Pop-Location }
        Push-Location ([IO.Path]::GetTempPath())
        try { Invoke-Native $smokeTest } finally { Pop-Location }
        Write-Host 'Windows startup and shutdown test passed'
    }
    if ($Test -or $Benchmark) {
        Push-Location $outputDirectory
        try { Invoke-Native $renderTest } finally { Pop-Location }
    }
    Write-Host "Windows build ready: $game"
    if ($Run) {
        Push-Location $outputDirectory
        try { Invoke-Native $game } finally { Pop-Location }
    }
} finally {
    $env:PATH = $savedPath
}
