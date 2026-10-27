param(
    [Parameter(Mandatory=$true)][string]$Source,
    [Parameter(Mandatory=$true)][string]$Receipt,
    [Parameter(Mandatory=$true)][string]$PreviousReceipt,
    [Parameter(Mandatory=$true)][string]$Target,
    [Parameter(Mandatory=$true)][string]$Run,
    [switch]$Apply
)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
function Under([string]$Path,[string]$Root) {
    $full=[IO.Path]::GetFullPath($Path)
    $prefix=[IO.Path]::GetFullPath($Root).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) {throw "Path outside intended root: $full"}
    return $full
}
function Closed {
    $processes=@(Get-CimInstance Win32_Process -Filter "Name='amtrucks.exe'")
    if ($processes.Count) {throw "ATS is running: $($processes | Select-Object ProcessId,ExecutablePath | ConvertTo-Json -Compress). No files written."}
}
function Hash([string]$Path) {
    if (Test-Path -LiteralPath $Path -PathType Leaf) {return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
    return $null
}
$Source=[IO.Path]::GetFullPath($Source)
$Target=Under $Target 'C:/Users/Crym2/Documents/American Truck Simulator/mod'
$Run=Under $Run (Join-Path $repo 'build')
New-Item -ItemType Directory -Force -Path $Run | Out-Null
$new=Get-Content -LiteralPath $Receipt -Raw | ConvertFrom-Json -AsHashtable
$old=Get-Content -LiteralPath $PreviousReceipt -Raw | ConvertFrom-Json -AsHashtable
$changes=@();$preserved=@()
foreach ($relative in $new.files.Keys | Sort-Object) {
    $sourceFile=Under (Join-Path $Source $relative) $Source
    $targetFile=Under (Join-Path $Target $relative) $Target
    $desired=$new.files[$relative]
    if ((Hash $sourceFile) -ne $desired) {throw "Source receipt mismatch: $relative"}
    $current=Hash $targetFile
    if ($current -eq $desired) {continue}
    if ($old.files.ContainsKey($relative)) {
        if ($desired -eq $old.files[$relative] -and $current) {$preserved+= $relative;continue}
        if ($current -ne $old.files[$relative]) {throw "Customized or missing changed file requires inspection: $relative"}
    } elseif ($current) {throw "Existing unowned file requires inspection: $relative"}
    $changes+=@{path=$relative;before=$current;after=$desired}
}
$plan=@{version=$new.version;target=$Target;source=$Source;files=$changes;preservedCustomFiles=$preserved;packageType='unpacked directory'}
$plan | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $Run 'install-plan.json')
if (-not $changes.Count) {Write-Output 'NO_OP: all requested files already installed.';exit 0}
Closed
if (-not $Apply) {Write-Output "PLAN: $($changes.Count) changed/new files; preserve $($preserved.Count) customized unchanged assets; $Target";exit 0}
$backupRoot=Join-Path $repo 'backups'
$backup=Under (Join-Path $backupRoot ('camera-mod-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))) $backupRoot
New-Item -ItemType Directory -Force -Path $backup | Out-Null
foreach ($entry in $changes) {
    if ($entry.before) {
        $copy=Under (Join-Path $backup $entry.path) $backup
        New-Item -ItemType Directory -Force -Path (Split-Path $copy) | Out-Null
        Copy-Item -LiteralPath (Join-Path $Target $entry.path) -Destination $copy
        if ((Hash $copy) -ne $entry.before) {throw 'Backup mismatch'}
    }
}
$config=[IO.Path]::GetFullPath((Join-Path $Target '../../config.cfg'))
if (Test-Path -LiteralPath $config) {Copy-Item -LiteralPath $config -Destination (Join-Path $backup 'config.cfg')}
Closed
foreach ($entry in $changes) {
    if ((Hash (Join-Path $Source $entry.path)) -ne $entry.after -or (Hash (Join-Path $Target $entry.path)) -ne $entry.before) {throw 'Files changed after planning'}
}
$done=@()
try {
    Closed
    foreach ($entry in $changes) {
        $destination=Under (Join-Path $Target $entry.path) $Target
        New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
        $temporary=$destination+'.bettergps-tmp'
        if (Test-Path -LiteralPath $temporary) {throw "Temporary file collision: $temporary"}
        Copy-Item -LiteralPath (Join-Path $Source $entry.path) -Destination $temporary
        if ((Hash $temporary) -ne $entry.after) {throw 'Staged file hash mismatch'}
        [IO.File]::Move($temporary,$destination,$true);$done+=$entry
        if ((Hash $destination) -ne $entry.after) {throw 'Installed file hash mismatch'}
    }
} catch {
    foreach ($entry in $done) {
        $destination=Under (Join-Path $Target $entry.path) $Target
        if ($entry.before) {Copy-Item -LiteralPath (Join-Path $backup $entry.path) -Destination $destination}
        else {Remove-Item -LiteralPath $destination}
    }
    throw
}
$plan.backup=$backup;$plan.installedUtc=[DateTime]::UtcNow.ToString('o')
$plan | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $Run 'install-receipt.json')
Write-Output "INSTALLED: $($new.version), $($changes.Count) files; backup $backup"
