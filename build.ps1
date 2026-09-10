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
$objectDirectory = [IO.Path]::GetFullPath((Join-Path $projectDirectory "obj/windows/$Configuration"))

if ($Clean) {
    # Validate both configuration directories before removing either one.
    foreach ($root in @('bin/windows', 'obj/windows')) {
        $buildRoot = [IO.Path]::GetFullPath((Join-Path $projectDirectory $root)) + [IO.Path]::DirectorySeparatorChar
        $target = [IO.Path]::GetFullPath((Join-Path $buildRoot $Configuration))
        if (-not $target.StartsWith($buildRoot, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Output directory is outside the build root: $target"
        }
        for ($directory = $target; $directory -and $directory -ne $projectDirectory; $directory = Split-Path $directory -Parent) {
            if ((Test-Path -LiteralPath $directory) -and
                ((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
                throw "Refusing to clean through a junction or symbolic link: $directory"
            }
        }
    }
    foreach ($target in @($outputDirectory, $objectDirectory)) {
        if (Test-Path -LiteralPath $target) {
            Remove-Item -LiteralPath $target -Recurse -Force
        }
        Write-Host "Cleaned $target"
    }
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

function Read-Dependencies([string]$Path) {
    # GCC emits one fixed target, continued lines, escaped spaces/#, and doubled $.
    $contents = [IO.File]::ReadAllText($Path) -replace '\\\r?\n', ''
    if ($contents -notmatch '\Akernelcraft-object:\s+(.+)\s*\z') { return }
    foreach ($token in [regex]::Matches($Matches[1], '(?:\\[ \t#]|[^\s])+')) {
        $name = ($token.Value -replace '\\([ \t#])', '$1').Replace('$$', '$')
        if (-not [IO.Path]::IsPathRooted($name)) {
            $name = Join-Path $compilerWorkingDirectory $name
        }
        [IO.Path]::GetFullPath($name)
    }
}

function Test-ObjectCurrent([string]$Object, [string]$Dependency, [string]$Settings, [string]$Source) {
    if (-not [IO.File]::Exists($Object) -or -not [IO.File]::Exists($Dependency) -or -not [IO.File]::Exists($Settings)) {
        return $false
    }
    try {
        $dependencyHash = (Get-FileHash -LiteralPath $Dependency -Algorithm SHA256).Hash
        if ([IO.File]::ReadAllText($Settings) -cne "$compileSignature`n$dependencyHash") { return $false }
        $dependencies = @(Read-Dependencies $Dependency)
        if ($Source -notin $dependencies) { return $false }
        $objectTime = [IO.File]::GetLastWriteTimeUtc($Object)
        foreach ($path in $dependencies) {
            if (-not $dependencyTimes.ContainsKey($path)) {
                if (-not [IO.File]::Exists($path)) { return $false }
                $dependencyTimes[$path] = [IO.File]::GetLastWriteTimeUtc($path)
            }
            if ($dependencyTimes[$path] -gt $objectTime) { return $false }
        }
    } catch {
        # Missing, truncated, or unreadable metadata is a cache miss, never a hit.
        return $false
    }
    return $true
}

function Get-CompiledObject([string]$Source) {
    if ($compiledObjects.ContainsKey($Source)) { return $compiledObjects[$Source] }
    $projectRoot = $projectDirectory.TrimEnd('\') + '\'
    $sourcePath = [IO.Path]::GetFullPath($Source)
    if (-not $sourcePath.StartsWith($projectRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Source is outside the project: $Source"
    }
    $relative = $sourcePath.Substring($projectRoot.Length)
    $object = Join-Path $objectDirectory ([IO.Path]::ChangeExtension($relative, '.o'))
    $dependency = [IO.Path]::ChangeExtension($object, '.d')
    $settings = [IO.Path]::ChangeExtension($object, '.settings')
    if (-not (Test-ObjectCurrent $object $dependency $settings $sourcePath)) {
        New-Item -ItemType Directory -Path (Split-Path $object -Parent) -Force | Out-Null
        $temporaryObject = "$object.tmp"
        $temporaryDependency = "$dependency.tmp"
        try {
            Write-Host "Compiling $relative"
            Invoke-Native $compiler ($flags + @('-MD', '-MF', $temporaryDependency, '-MT', 'kernelcraft-object', '-c', $sourcePath, '-o', $temporaryObject)) | Out-Host
            if ($sourcePath -notin @(Read-Dependencies $temporaryDependency)) {
                throw "Compiler produced incomplete dependencies for $Source"
            }
            $dependencyHash = (Get-FileHash -LiteralPath $temporaryDependency -Algorithm SHA256).Hash
            # Publish only successful compiler output. Paths are beneath objectDirectory.
            Move-Item -LiteralPath $temporaryDependency -Destination $dependency -Force
            Move-Item -LiteralPath $temporaryObject -Destination $object -Force
            [IO.File]::WriteAllText($settings, "$compileSignature`n$dependencyHash")
        } finally {
            foreach ($temporary in @($temporaryObject, $temporaryDependency)) {
                if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
            }
        }
    }
    $compiledObjects[$Source] = $object
    return $object
}

function Build-Executable([string[]]$Sources, [string]$Destination, [string[]]$LinkArguments = @()) {
    $objects = @($Sources | ForEach-Object { Get-CompiledObject $_ })
    # Always relink: changed libraries and linker options cannot leave a stale executable.
    Write-Host "Linking $(Split-Path $Destination -Leaf)"
    Invoke-Native $compiler ($flags + $objects + @('-o', $Destination) + $LinkArguments)
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
    $compilerVersion = & $compiler -dumpfullversion -dumpversion
    if ($LASTEXITCODE -ne 0) { throw 'Cannot determine the compiler version.' }
    $compilerFrontend = & $compiler -print-prog-name=cc1
    if ($LASTEXITCODE -ne 0 -or -not [IO.File]::Exists($compilerFrontend)) { throw 'Cannot locate the C compiler frontend.' }
    # Native programs follow PowerShell's filesystem location, not .NET's cwd.
    $compilerWorkingDirectory = $ExecutionContext.SessionState.Path.CurrentFileSystemLocation.ProviderPath
    $signatureInputs = @($compiler, $compilerVersion, $compilerWorkingDirectory, (Get-FileHash -LiteralPath $compiler).Hash,
        (Get-FileHash -LiteralPath $compilerFrontend).Hash, (Get-FileHash -LiteralPath $PSCommandPath).Hash) + $flags
    foreach ($variable in @('PATH', 'CPATH', 'C_INCLUDE_PATH', 'COMPILER_PATH', 'GCC_EXEC_PREFIX')) {
        $signatureInputs += @($variable, [Environment]::GetEnvironmentVariable($variable))
    }
    $hasher = [Security.Cryptography.SHA256]::Create()
    try {
        $signatureBytes = [Text.Encoding]::UTF8.GetBytes(($signatureInputs | ConvertTo-Json -Compress))
        $compileSignature = [BitConverter]::ToString($hasher.ComputeHash($signatureBytes))
    } finally { $hasher.Dispose() }
    $dependencyTimes = [Collections.Generic.Dictionary[string, DateTime]]::new([StringComparer]::OrdinalIgnoreCase)
    $compiledObjects = [Collections.Generic.Dictionary[string, string]]::new([StringComparer]::OrdinalIgnoreCase)
    $libraries = @('-lopengl32', '-lglfw3', '-lglew32', '-lfreeglut', '-lm')
    $sources = @(Get-ChildItem -LiteralPath (Join-Path $projectDirectory 'src') -Filter '*.c' -Recurse -File | Sort-Object FullName | ForEach-Object FullName)
    $commonSources = @($sources | Where-Object { $_ -ne (Join-Path $projectDirectory 'src/main.c') })
    $game = Join-Path $outputDirectory 'minecraft_clone.exe'
    $executables = @($game)

    Write-Host "Building $Configuration with $compiler"
    Build-Executable $sources $game $libraries

    if ($Test) {
        $worldTest = Join-Path $outputDirectory 'test-world.exe'
        $worldSources = @('tests/test_world.c', 'src/world/chunk.c', 'src/world/edit.c', 'src/world/player.c', 'src/world/save.c', 'src/world/cube.c', 'src/world/mesh.c', 'src/world/mesh_visibility.c', 'src/world/occlusion.c', 'src/world/world.c', 'src/math/math.c', 'src/graphics/frustum.c', 'src/utils/raycast.c') |
            ForEach-Object { Join-Path $projectDirectory $_ }
        $worldSources += Join-Path $projectDirectory 'src/world/day_night.c'
        $worldSources += Join-Path $projectDirectory 'src/world/chat.c'
        Build-Executable $worldSources $worldTest @('-lm')

        $editTest = Join-Path $outputDirectory 'test-edits.exe'
        $editSources = @((Join-Path $projectDirectory 'tests/test_edits.c')) + @($worldSources | Select-Object -Skip 1)
        Build-Executable $editSources $editTest @('-lm')

        $selectionTest = Join-Path $outputDirectory 'test-selection.exe'
        $selectionSources = @((Join-Path $projectDirectory 'tests/test_selection.c')) + @($worldSources | Select-Object -Skip 1)
        Build-Executable $selectionSources $selectionTest @('-lm')

        $playerTest = Join-Path $outputDirectory 'test-player.exe'
        $playerSources = @((Join-Path $projectDirectory 'tests/test_player.c')) + @($worldSources | Select-Object -Skip 1)
        Build-Executable $playerSources $playerTest @('-lm')

        $seedTest = Join-Path $outputDirectory 'test-seed.exe'
        $seedSources = @((Join-Path $projectDirectory 'tests/test_seed.c')) + @($worldSources | Select-Object -Skip 1)
        Build-Executable $seedSources $seedTest @('-lm')

        $saveTest = Join-Path $outputDirectory 'test-save.exe'
        $saveSources = @((Join-Path $projectDirectory 'tests/test_save.c')) + @($worldSources | Select-Object -Skip 1)
        Build-Executable $saveSources $saveTest @('-Wl,--wrap=fwrite', '-Wl,--wrap=fflush', '-Wl,--wrap=fclose', '-Wl,--wrap=__imp__commit', '-Wl,--wrap=__imp_MoveFileExA', '-Wl,--wrap=calloc', '-lm')

        $optionsTest = Join-Path $outputDirectory 'test-options.exe'
        $optionsSources = @('tests/test_options.c', 'src/utils/options.c') | ForEach-Object { Join-Path $projectDirectory $_ }
        Build-Executable $optionsSources $optionsTest

        $shaderTest = Join-Path $outputDirectory 'test-shader.exe'
        $shaderSources = @('tests/test_shader.c', 'src/graphics/shader.c', 'src/graphics/texture.c') | ForEach-Object { Join-Path $projectDirectory $_ }
        Build-Executable $shaderSources $shaderTest $libraries

        $hudTest = Join-Path $outputDirectory 'test-hud.exe'
        Build-Executable (@((Join-Path $projectDirectory 'tests/test_hud.c')) + $commonSources) $hudTest (@('-Wl,--wrap=renderText', '-Wl,--wrap=loadTexture', '-Wl,--wrap=glutBitmapString', '-Wl,--wrap=__imp_glutBitmapString') + $libraries)

        $smokeTest = Join-Path $outputDirectory 'test-startup.exe'
        $smokeFlags = @('-Wl,--wrap=glfwCreateWindow', '-Wl,--wrap=glfwWindowShouldClose', '-Wl,--wrap=glfwSetInputMode', '-Wl,--wrap=glfwDestroyWindow', '-Wl,--wrap=glfwGetInputMode', '-Wl,--wrap=glfwGetWindowAttrib', '-Wl,--wrap=glfwGetKey', '-Wl,--wrap=glfwGetFramebufferSize', '-Wl,--wrap=glfwWaitEvents', '-Wl,--wrap=glfwSwapBuffers', '-Wl,--wrap=glfwGetTime', '-Wl,--wrap=HUDDraw', '-Wl,--wrap=renderSky')
        Build-Executable ($sources + @((Join-Path $projectDirectory 'tests/app_smoke.c'))) $smokeTest ($smokeFlags + $libraries)
        $persistenceTest = Join-Path $outputDirectory 'test-persistence.exe'
        $persistenceFlags = @($smokeFlags | Where-Object { $_ -notin @('-Wl,--wrap=glfwGetFramebufferSize', '-Wl,--wrap=glfwWaitEvents', '-Wl,--wrap=renderSky') })
        Build-Executable ($sources + @((Join-Path $projectDirectory 'tests/app_persistence.c'))) $persistenceTest ($persistenceFlags + $libraries)
        $executables += @($hudTest, $persistenceTest, $worldTest, $editTest, $selectionTest, $playerTest, $seedTest, $saveTest, $optionsTest, $shaderTest, $smokeTest)
    }
    if ($Test -or $Benchmark) {
        $renderTest = Join-Path $outputDirectory 'benchmark.exe'
        $benchmarkFlags = @('-Wl,--wrap=glDrawArrays', '-Wl,--wrap=glDrawElements', '-Wl,--wrap=occlusionBoundsHidden', '-Wl,--wrap=meshVisibilityIntersects', '-Wl,--wrap=renderText')
        Build-Executable (@((Join-Path $projectDirectory 'tests/render_benchmark.c')) + $commonSources) $renderTest ($benchmarkFlags + $libraries)
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
            Invoke-Native $playerTest
            Invoke-Native $seedTest
            Invoke-Native $saveTest
            Invoke-Native $optionsTest
            Invoke-Native $shaderTest
            Invoke-Native $hudTest
        } finally { Pop-Location }
        & (Join-Path $projectDirectory 'tests/test_persistence.ps1') -Binary $persistenceTest -SmokeBinary $smokeTest
    }
    if ($Test -or $Benchmark) {
        Push-Location $outputDirectory
        try { Invoke-Native $renderTest } finally { Pop-Location }
    }
    Write-Host "Windows build ready: $game"
    if ($Run) {
        # Assets are executable-relative; preserve the caller's save directory.
        Invoke-Native $game
    }
} finally {
    $env:PATH = $savedPath
}
