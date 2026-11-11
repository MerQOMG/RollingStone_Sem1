param([string]$Destination = 'build/3d/package/RollingStone3D')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$target = [IO.Path]::GetFullPath((Join-Path $root $Destination))
if (Test-Path -LiteralPath $target) { throw 'Choose a new package directory; existing packages are never overwritten.' }
$binary = Join-Path $root 'build/3d/bin/RollingStone3D.exe'
if (!(Test-Path -LiteralPath $binary)) { throw 'Run build_3d.cmd Release first.' }
New-Item -ItemType Directory -Path $target | Out-Null
Copy-Item -LiteralPath $binary -Destination $target
Get-ChildItem -LiteralPath (Join-Path $root 'build/3d/bin') -Filter 'rs3d_*.exe' | Copy-Item -Destination $target
Copy-Item -LiteralPath (Join-Path $root 'configs3d') -Destination $target -Recurse
$docs = Join-Path $target 'docs'
New-Item -ItemType Directory -Path $docs | Out-Null
foreach ($name in @('README.md','DEPENDENCIES.md','VALIDATION.md')) {
    Copy-Item -LiteralPath (Join-Path $root ('docs/3d/'+$name)) -Destination $docs
}
$notices = Join-Path $target 'licenses'
New-Item -ItemType Directory -Path $notices | Out-Null
foreach ($vendor in @('JoltPhysics','raylib','raygui')) {
    Copy-Item -LiteralPath (Join-Path $root ('.deps3d/'+$vendor+'/LICENSE')) -Destination (Join-Path $notices ($vendor+'.txt'))
}
# Several bundled raylib components carry notices inside their source headers.
# Preserve the complete upstream files instead of guessing which paragraphs apply.
Copy-Item -LiteralPath (Join-Path $root '.deps3d/raylib/src/external') -Destination (Join-Path $notices 'raylib_external') -Recurse
$launcher = @'
@echo off
setlocal
pushd "%~dp0"
if not exist RollingStone3D.exe (
  echo ERROR: RollingStone3D.exe is missing from this package.
  popd
  exit /b 1
)
RollingStone3D.exe %*
set "RS3D_EXIT=%errorlevel%"
popd
exit /b %RS3D_EXIT%
'@
[IO.File]::WriteAllText((Join-Path $target 'run_3d.cmd'),$launcher,[Text.Encoding]::ASCII)
$rows = Get-ChildItem -LiteralPath $target -Recurse -File | ForEach-Object {
    [pscustomobject]@{path=$_.FullName.Substring($target.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
}
$rows | ConvertTo-Json -Depth 3 | Set-Content -Encoding utf8 (Join-Path $target 'package-sha256.json')
Write-Output "Packaged $target (system CJK font is not redistributed)"
