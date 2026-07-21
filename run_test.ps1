$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$venvPython = Join-Path $repoRoot '.venv\Scripts\python.exe'
$testScript = Join-Path $repoRoot 'tools\test_device.py'

if (-not (Test-Path $venvPython)) {
    throw "Python virtual environment not found at $venvPython"
}

if (-not (Test-Path $testScript)) {
    throw "Test script not found at $testScript"
}

& $venvPython $testScript @args
exit $LASTEXITCODE
