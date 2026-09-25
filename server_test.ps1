# Runs ChatGuard inside a real open.mp server with testmode\chatguard_test.pwn and checks its verdict.
$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$dist = Join-Path $root 'deps\server-dist\Server'
$server = Join-Path $root 'build\server'
$dll = Join-Path $root 'build\ChatGuard.dll'

if (-not (Test-Path $dll)) { throw "Run build.bat first: $dll not found" }
if (-not (Test-Path $server)) { Copy-Item $dist $server -Recurse }
Copy-Item $dll (Join-Path $server 'components') -Force

# Local-only server: no masterlist announce, no artwork web server, loopback bind.
$config = Get-Content (Join-Path $dist 'config.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$config.announce = $false
$config.artwork.enable = $false
$config.network.bind = '127.0.0.1'
$config.network.port = 7797
$config.max_bots = 5
$config.pawn.main_scripts = @('chatguard_test 1')
$chatguard = Get-Content (Join-Path $root 'testmode\chatguard.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$config | Add-Member -Force -NotePropertyName chatguard -NotePropertyValue $chatguard
[IO.File]::WriteAllText((Join-Path $server 'config.json'), ($config | ConvertTo-Json -Depth 10), [Text.UTF8Encoding]::new($false))

# Real Russian modes are saved in cp1251, so the test mode is too.
$source = [IO.File]::ReadAllText((Join-Path $root 'testmode\chatguard_test.pwn'), [Text.Encoding]::UTF8)
$pwn = Join-Path $server 'gamemodes\chatguard_test.pwn'
[IO.File]::WriteAllText($pwn, $source, [Text.Encoding]::GetEncoding(1251))

$qawno = Join-Path $server 'qawno'
& (Join-Path $qawno 'pawncc.exe') $pwn "-i$qawno\include" "-i$root\pawn" "-o$server\gamemodes\chatguard_test.amx"
if ($LASTEXITCODE -ne 0) { throw 'Pawn compilation failed' }

$output = Join-Path $server 'test_output.txt'
$process = Start-Process -FilePath (Join-Path $server 'omp-server.exe') -WorkingDirectory $server `
    -RedirectStandardOutput $output -RedirectStandardError "$output.err" -PassThru -NoNewWindow
if (-not $process.WaitForExit(30000)) {
    $process.Kill()
    throw 'Server did not exit within 30 s'
}

$log = Get-Content $output -Raw
$log -split "`r?`n" | Where-Object { $_ -match 'ChatGuard|chatguard-test|error|warning' }
if ($log -notmatch '\[chatguard-test\] done: \d+ checks, 0 failed') {
    Write-Host "`nSERVER TEST FAILED, full output: $output"
    exit 1
}
Write-Host "`nSERVER TEST PASSED"
