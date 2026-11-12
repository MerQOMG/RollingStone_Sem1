param([string]$BuildDir = 'build/3d')
$ErrorActionPreference = 'Stop'
$root=Split-Path $PSScriptRoot -Parent
$build=Join-Path $root $BuildDir
$rows=@()
foreach($cfg in @('Release','Debug')) {
  foreach($item in @(@('Jolt','jolt/Jolt.dir'),@('raylib','raylib/raylib/raylib.dir'),@('glfw','raylib/raylib/external/glfw/src/glfw.dir'),@('RollingStone3D','RollingStone3D.dir'),@('rs3d_abi','rs3d_abi.dir'),@('rs3d_core','rs3d_core.dir'),@('rs3d_geometry_tests','rs3d_geometry_tests.dir'),@('rs3d_morphology_tests','rs3d_morphology_tests.dir'),@('rs3d_terrain_tests','rs3d_terrain_tests.dir'),@('rs3d_input_tests','rs3d_input_tests.dir'),@('rs3d_physics_tests','rs3d_physics_tests.dir'),@('rs3d_elastic_tests','rs3d_elastic_tests.dir'),@('rs3d_statistics_tests','rs3d_statistics_tests.dir'),@('rs3d_config_tests','rs3d_config_tests.dir'),@('rs3d_batch_tests','rs3d_batch_tests.dir'),@('rs3d_settling_tests','rs3d_settling_tests.dir'),@('rs3d_session_tests','rs3d_session_tests.dir'))) {
    # MSBuild hashes long target names in the tlog directory.
    $logs=@(Get-ChildItem -LiteralPath (Join-Path $build ($item[1]+'/'+$cfg)) -Filter CL.command.1.tlog -Recurse)
    if ($logs.Count -ne 1) { throw "Expected one actual compiler log for $($item[0])/$cfg" }
    $p=$logs[0].FullName
    $content=Get-Content -Raw -LiteralPath $p
    $commands=($content -split "`r?`n") | Where-Object {$_ -and !$_.StartsWith('^')}
    foreach($command in $commands) {
      if($item[0] -eq 'raylib') {
        $meshDefinitions=[regex]::Matches($command,'MAX_MESH_VERTEX_BUFFERS=(\d+)')
        if($meshDefinitions.Count -ne 1 -or $meshDefinitions[0].Groups[1].Value -ne '9') {throw "Unsafe/duplicate raylib mesh buffer definition: $p"}
      }
      $crt=if($cfg -eq 'Debug'){'/MTd'}else{'/MT'}
      if($command -notmatch ('(?i)'+$crt+'\b') -or $command -match '(?i)/MDd?\b|/fp:fast|/GL\b|_HAS_EXCEPTIONS=0') {throw "Invalid CRT/FP/IPO/exception flags: $p"}
      if($command -notmatch '(?i)/fp:precise') {throw "Missing precise FP flag: $p"}
      if($item[0] -notin @('raylib','glfw') -and $command -notmatch '(?i)/EHsc') {throw "Missing exceptions: $p"}
      if($item[0] -notin @('raylib','glfw') -and $command -notmatch '(?i)/std:c\+\+17\b') {throw "Missing C++17: $p"}
      if($item[0] -in @('rs3d_abi','rs3d_core','rs3d_geometry_tests','rs3d_morphology_tests','rs3d_terrain_tests','rs3d_physics_tests','rs3d_elastic_tests','rs3d_statistics_tests','rs3d_config_tests','rs3d_batch_tests','rs3d_settling_tests','rs3d_session_tests','RollingStone3D') -and ($command -notmatch 'JPH_DOUBLE_PRECISION' -or $command -notmatch 'JPH_CROSS_PLATFORM_DETERMINISTIC')) {throw "Missing public ABI macros: $p"}
    }
    $rows += [pscustomobject]@{target=$item[0];configuration=$cfg;command_count=$commands.Count;verified=$true;sample_command=$commands[0]}
  }
}
$rows | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $build 'evidence/compiler-audit.json')
$rows | Select-Object target,configuration,command_count,verified | Format-Table -AutoSize
Write-Output 'PASS actual compiler command audit'
