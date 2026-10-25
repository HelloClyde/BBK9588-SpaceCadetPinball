param([string]$Python = 'python', [string]$Prefix = '')
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $buildArgs = @('build_native.py')
    if ($Prefix) { $buildArgs += @('--prefix', $Prefix) }
    & $Python @buildArgs
    if ($LASTEXITCODE) { throw 'Space Cadet build failed' }
    & $Python package_release.py
    if ($LASTEXITCODE) { throw 'Space Cadet packaging failed' }
} finally { Pop-Location }
