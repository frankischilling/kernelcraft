#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BaseDirectory,
    [Parameter(Mandatory = $true)][string]$CandidateDirectory,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [ValidateRange(1, 20)][int]$Pairs = 5,
    [switch]$Pipelined
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$roots = @{ base = (Resolve-Path -LiteralPath $BaseDirectory).Path; candidate = (Resolve-Path -LiteralPath $CandidateDirectory).Path }
$harnessHashes = [ordered]@{}
foreach ($path in @('tests/render_profile.h', 'tests/render_benchmark.c', 'tests/cloud_render_checks.h')) {
    $baseHash = Get-FileHash -LiteralPath (Join-Path $roots.base $path)
    $candidateHash = Get-FileHash -LiteralPath (Join-Path $roots.candidate $path)
    if ($baseHash.Hash -ne $candidateHash.Hash) { throw "Both builds must use the same $path." }
    $harnessHashes[$path] = $baseHash.Hash
}
foreach ($root in $roots.Values) {
    if (-not (Test-Path -LiteralPath (Join-Path $root 'bin/windows/Release/benchmark.exe'))) {
        throw "Build the Release benchmark before measuring: $root"
    }
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Choose a new output directory to preserve earlier measurements.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$outputRoot = (Resolve-Path -LiteralPath $OutputDirectory).Path
@{
    utc = [DateTime]::UtcNow.ToString('o')
    cpu = @(Get-CimInstance Win32_Processor | Select-Object Name)
    graphics = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion)
    os = @(Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version)
    harnessSha256 = $harnessHashes['tests/render_profile.h']
    benchmarkSourcesSha256 = $harnessHashes
    directories = $roots
    pairs = $Pairs
    mode = if ($Pipelined) { 'pipelined' } else { 'serialized' }
    debug = [bool]$env:KERNELCRAFT_PROFILE_DEBUG
    skipText = [bool]$env:KERNELCRAFT_PROFILE_SKIP_TEXT
    scene = $env:KERNELCRAFT_PROFILE_SCENE
    atmosphere = [bool]$env:KERNELCRAFT_PROFILE_ATMOSPHERE
    phase = $env:KERNELCRAFT_PROFILE_PHASE
    requestedWidth = $env:KERNELCRAFT_PROFILE_WIDTH
    requestedHeight = $env:KERNELCRAFT_PROFILE_HEIGHT
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot 'environment.json') -Encoding UTF8
$columns = @('scenario', 'fps', 'frame_mean_ms', 'frame_median_ms', 'frame_p95_ms', 'frame_p99_ms', 'one_percent_low_fps',
    'cpu_submit_mean_ms', 'gpu_mean_ms', 'terrain_draws', 'triangles', 'surface_blocks', 'queries', 'rebuild_mean_ms',
    'chunks_rebuilt', 'upload_calls', 'upload_bytes', 'gpu_p99_ms')
$results = @()
$expectedScenes = if ($env:KERNELCRAFT_PROFILE_SCENE) { 1 } else { 10 }
$previousProfile = $env:KERNELCRAFT_RENDER_PROFILE
$previousCSV = $env:KERNELCRAFT_PROFILE_CSV
$previousPipeline = $env:KERNELCRAFT_PROFILE_PIPELINED
try {
    $env:KERNELCRAFT_RENDER_PROFILE = '1'
    $env:KERNELCRAFT_PROFILE_PIPELINED = if ($Pipelined) { '1' } else { $null }
    for ($pair = 1; $pair -le $Pairs; $pair++) {
        $order = if ($pair % 2) { @('base', 'candidate') } else { @('candidate', 'base') }
        foreach ($variant in $order) {
            $name = "pair-$pair-$variant"
            $log = Join-Path $outputRoot "$name.log"
            $env:KERNELCRAFT_PROFILE_CSV = Join-Path $outputRoot "$name-frames.csv"
            Write-Host "Starting $name at $([DateTime]::Now.ToString('HH:mm:ss'))"
            Push-Location (Join-Path $roots[$variant] 'bin/windows/Release')
            try {
                & '.\benchmark.exe' > $log 2>&1
                if ($LASTEXITCODE -ne 0) { throw "Profile failed; see $log" }
            } finally { Pop-Location }
            $lines = @(Get-Content -LiteralPath $log | Where-Object { $_.StartsWith('PROFILE_RESULT ') })
            $cloudLines = @(Get-Content -LiteralPath $log | Where-Object { $_.StartsWith('PROFILE_CLOUD ') })
            if ($lines.Count -ne $expectedScenes) { throw "Expected $expectedScenes complete scenes in $log" }
            if ($cloudLines.Count -ne $expectedScenes) { throw "Expected $expectedScenes cloud measurements in $log" }
            foreach ($line in $lines) {
                $values = $line.Substring('PROFILE_RESULT '.Length).Split(',')
                if ($values.Count -ne $columns.Count) { throw "Unexpected result schema in $log" }
                $row = [ordered]@{ pair = $pair; variant = $variant }
                for ($column = 0; $column -lt $columns.Count; $column++) { $row[$columns[$column]] = $values[$column] }
                $cloud = @($cloudLines | Where-Object { $_.StartsWith("PROFILE_CLOUD $($values[0]),") })
                if ($cloud.Count -ne 1) { throw "Missing cloud measurement for $($values[0])" }
                $cloudValues = $cloud[0].Substring('PROFILE_CLOUD '.Length).Split(',')
                if ($cloudValues.Count -ne 6) { throw 'Unexpected cloud result schema' }
                $cloudColumns = @('cloud_gpu_mean_ms', 'cloud_gpu_p95_ms', 'cloud_gpu_p99_ms', 'cloud_cpu_mean_ms', 'cloud_draws')
                for ($column = 0; $column -lt $cloudColumns.Count; $column++) { $row[$cloudColumns[$column]] = $cloudValues[$column + 1] }
                $results += [pscustomobject]$row
            }
            $results | Export-Csv -LiteralPath (Join-Path $outputRoot 'runs.csv') -NoTypeInformation -Encoding UTF8
            Write-Host "Finished $name; all profile invariants passed."
        }
    }
} finally {
    $env:KERNELCRAFT_RENDER_PROFILE = $previousProfile
    $env:KERNELCRAFT_PROFILE_CSV = $previousCSV
    $env:KERNELCRAFT_PROFILE_PIPELINED = $previousPipeline
}
Write-Host "Completed $Pairs alternating pairs: $outputRoot"
