param([switch]$CoreOnly)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$deps = @(
  @('JoltPhysics','jrouwe/JoltPhysics','v5.3.0','0373ec0dd762e4bc2f6acdb08371ee84fa23c6db'),
  @('raylib','raysan5/raylib','5.5','c1ab645ca298a2801097931d1079b10ff7eb9df8'),
  @('raygui','raysan5/raygui','4.0','25c8c65a6e5f0f4d4b564a0343861898c6f2778b'))
New-Item -ItemType Directory -Force (Join-Path $root '.deps3d') | Out-Null
foreach($d in $deps) {
  if($CoreOnly -and $d[0] -ne 'JoltPhysics') { continue }
  $dest = Join-Path $root ('.deps3d/' + $d[0])
  if(!(Test-Path -LiteralPath $dest)) {
    git clone --depth 1 --branch $d[2] ('https://github.com/' + $d[1] + '.git') $dest
    if($LASTEXITCODE -ne 0) { throw "Download failed: $($d[0])" }
  }
  $actual = git -C $dest rev-parse HEAD
  if($actual -ne $d[3]) { throw "SHA mismatch for $($d[0]): $actual" }
  Write-Output "$($d[0]) $actual verified"
}
