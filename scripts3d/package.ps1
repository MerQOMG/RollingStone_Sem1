param([string]$Destination = 'build/3d/package/RollingStone3D', [string]$ExampleDirectory = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$target = [IO.Path]::GetFullPath((Join-Path $root $Destination))
if (Test-Path -LiteralPath $target) { throw 'Choose a new package directory; existing packages are never overwritten.' }
$binary = Join-Path $root 'build/3d/bin/RollingStone3D.exe'
if (!(Test-Path -LiteralPath $binary)) { throw 'Run build_3d.cmd Release first.' }
New-Item -ItemType Directory -Path $target | Out-Null
Copy-Item -LiteralPath $binary -Destination $target
Copy-Item -LiteralPath (Join-Path $root 'run_elastic_3d.cmd') -Destination $target
Copy-Item -LiteralPath (Join-Path $root 'run_obstacles_3d.cmd') -Destination $target
Get-ChildItem -LiteralPath (Join-Path $root 'build/3d/bin') -Filter 'rs3d_*.exe' | Copy-Item -Destination $target
Copy-Item -LiteralPath (Join-Path $root 'configs3d') -Destination $target -Recurse
$docs = Join-Path $target 'docs'
New-Item -ItemType Directory -Path $docs | Out-Null
foreach ($name in @('README.md','DEPENDENCIES.md','VALIDATION.md','SHAPE_SAMPLER.md','ELASTIC_BARRIER.md','ANALYSIS_AND_TIMELINE.md','OBSTACLES_HEATMAP_TIMELINE_VALIDATION.md','OBSTACLE_DISTRIBUTION_FIX.md')) {
    Copy-Item -LiteralPath (Join-Path $root ('docs/3d/'+$name)) -Destination $docs
}
$readme = @'
# RollingStone3D

双击 run_obstacles_3d.cmd 打开障碍、热图与时间轴演示，点击“批量”开始。
双击 run_elastic_3d.cmd 打开弹性挡板场景。

完整说明见 [操作文档](docs/README.md)、[统计与回放格式](docs/ANALYSIS_AND_TIMELINE.md) 和 [验收记录](docs/OBSTACLES_HEATMAP_TIMELINE_VALIDATION.md)。

读取保存结果：`run_3d.cmd --view-results "结果目录"`，或在窗口“打开结果”选择 manifest.ini。
执行随包测试：`run_3d.cmd --self-test`。
程序沿用本机中文字体，不需要安装编译器；package-sha256.json 可核对随包文件。
'@
[IO.File]::WriteAllText((Join-Path $target 'README.md'),$readme,[Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath (Join-Path $root 'docs/3d/evidence') -Destination (Join-Path $docs 'evidence') -Recurse
if ($ExampleDirectory) {
    $example = [IO.Path]::GetFullPath((Join-Path $root $ExampleDirectory))
    if (!(Test-Path -LiteralPath (Join-Path $example 'FILES_COMPLETE.txt'))) { throw 'Example must be a committed result archive.' }
    New-Item -ItemType Directory -Path (Join-Path $target 'examples') | Out-Null
    Copy-Item -LiteralPath $example -Destination (Join-Path $target 'examples/three_trials') -Recurse
}
Copy-Item -LiteralPath (Join-Path $root 'docs/3d/images') -Destination (Join-Path $docs 'images') -Recurse
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
