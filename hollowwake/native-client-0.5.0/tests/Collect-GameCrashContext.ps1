param([Parameter(Mandatory=$true)][string]$CrashRoot,[Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][datetime]$Since)
# Only portable code locations from the original engine's own recent crash XML.
# Never export the XML, command line, account fields, modules' filesystem paths,
# memory dumps, arbitrary files or raw register/stack contents.
$records=@()
if(Test-Path -LiteralPath $CrashRoot -PathType Container){
 foreach($directory in (Get-ChildItem -LiteralPath $CrashRoot -Directory|Where-Object{!($_.Attributes-band [IO.FileAttributes]::ReparsePoint)}|Sort-Object LastWriteTimeUtc -Descending|Select-Object -First 2)){
  $path=Join-Path $directory.FullName 'CrashContext.runtime-xml';if(!(Test-Path -LiteralPath $path -PathType Leaf)){continue};$file=Get-Item -LiteralPath $path
  if($file.Length-gt 1048576-or $file.LastWriteTimeUtc-lt $Since.ToUniversalTime().AddSeconds(-2)-or ($file.Attributes-band [IO.FileAttributes]::ReparsePoint)){continue}
  $settings=[Xml.XmlReaderSettings]::new();$settings.DtdProcessing=[Xml.DtdProcessing]::Prohibit;$settings.XmlResolver=$null;$settings.MaxCharactersInDocument=1048576
  $reader=[Xml.XmlReader]::Create($path,$settings)
  try{
   $document=[Xml.XmlDocument]::new();$document.XmlResolver=$null;$document.Load($reader);$errorNode=$document.SelectSingleNode('//RuntimeProperties/ErrorMessage');$classification='unclassified'
   if($errorNode-and $errorNode.InnerText-match 'Attempt to process None/Default__Field before it has been added'){$classification='core-uobject-registration-order'}
   $frames=@();$addresses=@()
   foreach($field in @('PCallStack','CallStack')){
    $node=$document.SelectSingleNode('//RuntimeProperties/'+$field);if(!$node){continue};$text=$node.InnerText
    foreach($match in [regex]::Matches($text,'(?m)^\s*([A-Za-z0-9_.-]{1,96})\s+(0x[0-9A-Fa-f]{1,16})\s+\+\s+(0x[0-9A-Fa-f]{1,16})(?:\s|$)')|Select-Object -First 32){$frames+=@{module=$match.Groups[1].Value;imageBase=$match.Groups[2].Value;RVA=$match.Groups[3].Value}}
    foreach($match in [regex]::Matches($text,'\b0x[0-9A-Fa-f]{1,16}\b')|Select-Object -First 64){$addresses+=$match.Value}
   }
   $records+=@{errorClass=$classification;portableFrames=$frames;codeAddresses=$addresses;writtenUtc=$file.LastWriteTimeUtc.ToString('o')}
  }finally{$reader.Dispose()}
 }
}
@{scope='Recent original engine crash code locations only; no raw XML/dumps/secrets exported';records=$records}|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 (Join-Path $OutputDirectory 'engine-crash-code-locations.json')
