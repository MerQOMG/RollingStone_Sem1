param([Parameter(Mandatory=$true)][string]$Package)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$workspaceTestRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/3d')) + [IO.Path]::DirectorySeparatorChar
$packagePath = (Resolve-Path -LiteralPath $Package).Path
$target = Join-Path (Split-Path $packagePath -Parent) ('拒写目录 with spaces ' + [guid]::NewGuid().ToString('N'))
$target = [IO.Path]::GetFullPath($target)
if (!$target.StartsWith($workspaceTestRoot,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'ACL tests are restricted to new directories within this workspace build/3d.'
}
if (Test-Path -LiteralPath $target) { throw 'Refusing to change an existing directory ACL.' }
New-Item -ItemType Directory -Path $target | Out-Null
$original = Get-Acl -LiteralPath $target
$originalSddl = $original.Sddl
$identity = [Security.Principal.WindowsIdentity]::GetCurrent().User
$rule = [Security.AccessControl.FileSystemAccessRule]::new(
    $identity,
    [Security.AccessControl.FileSystemRights]'CreateFiles, CreateDirectories',
    [Security.AccessControl.InheritanceFlags]::None,
    [Security.AccessControl.PropagationFlags]::None,
    [Security.AccessControl.AccessControlType]::Deny)
$evidence = Join-Path $root 'build/3d/evidence'
$log = Join-Path $evidence 'deployment-acl-final.txt'
try {
    $denied = Get-Acl -LiteralPath $target
    $denied.AddAccessRule($rule)
    Set-Acl -LiteralPath $target -AclObject $denied
    & (Join-Path $packagePath 'RollingStone3D.exe') --config (Join-Path $packagePath 'configs3d/batch_throughput.ini') --batch 2 --headless --output $target > $log 2>&1
    $nativeExit = $LASTEXITCODE
    $output = Get-Content -LiteralPath $log -Raw
    if ($nativeExit -ne 1 -or $output -notmatch 'status=io_error' -or $output -notmatch 'code=5') {
        throw "Unexpected denied-output result, exit=$nativeExit. See $log"
    }
    if (@(Get-ChildItem -LiteralPath $target -Force).Count -ne 0) { throw 'Denied root contains unexpected results.' }
} finally {
    Set-Acl -LiteralPath $target -AclObject $original
}
$restored = (Get-Acl -LiteralPath $target).Sddl -eq $originalSddl
if (!$restored) { throw 'Original ACL restoration verification failed.' }
[pscustomobject]@{passed=$true;directory=$target;exit_code=$nativeExit;win32_error=5;acl_restored=$restored;executable_sha256=(Get-FileHash -LiteralPath (Join-Path $packagePath 'RollingStone3D.exe')).Hash} |
    ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $evidence 'deployment-acl-final.json')
Write-Output 'PASS real NTFS Deny, io_error/code5/exit1, no result files, original ACL restored.'
