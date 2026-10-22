param([Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][datetime]$Since,[Parameter(Mandatory=$true)][hashtable]$Roots)
# Only known game Saved/Logs locations, recently written text logs. No dumps,
# arbitrary directory recursion, memory, credentials or game assets are exported.
$records=@();$remaining=8
foreach($label in ($Roots.Keys|Sort-Object)){
 $root=$Roots[$label];if(!$root-or !(Test-Path -LiteralPath $root -PathType Container)){continue}
 foreach($file in (Get-ChildItem -LiteralPath $root -File -Filter '*.log'|Where-Object{$_.LastWriteTimeUtc-ge $Since.ToUniversalTime().AddSeconds(-2)}|Sort-Object LastWriteTimeUtc -Descending|Select-Object -First $remaining)){
  if($remaining-le 0){break};$remaining--
  $stream=[IO.File]::Open($file.FullName,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite-bor [IO.FileShare]::Delete)
  try{
   $tailBytes=[Math]::Min([long]1048576,$stream.Length);$null=$stream.Seek(-$tailBytes,[IO.SeekOrigin]::End);$reader=[IO.StreamReader]::new($stream,[Text.Encoding]::UTF8,$true,4096,$true)
   try{$text=$reader.ReadToEnd()}finally{$reader.Dispose()}
   if($tailBytes-lt $file.Length){$newline=$text.IndexOf("`n");if($newline-ge 0){$text=$text.Substring($newline+1)}}
   $text=$text-replace '(?i)(-AUTH_PASSWORD=)[^\s]+','$1[REDACTED]'-replace '(?i)(Bearer\s+)[^\s"<>]+','$1[REDACTED]'-replace '(?i)((?:exchange_code|access_token|refresh_token|session_token)["\s:=]+)[^\s",;&<>]+','$1[REDACTED]'-replace '(?<![A-Za-z0-9_-])[A-Za-z0-9_-]{43}(?![A-Za-z0-9_-])','[REDACTED]'
   $outputName=('game-log-'+$label+'-'+(8-$remaining)+'.log');$text|Set-Content -Encoding UTF8 (Join-Path $OutputDirectory $outputName)
   $records+=@{location=$label;sourceBytes=$file.Length;exportedTailBytes=$tailBytes;writtenUtc=$file.LastWriteTimeUtc.ToString('o');output=$outputName}
  }finally{$stream.Dispose()}
 }
}
@{scope='Bounded recent actual game text logs; no loading-screen/lobby inference';files=$records}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 (Join-Path $OutputDirectory 'game-logs-inventory.json')
