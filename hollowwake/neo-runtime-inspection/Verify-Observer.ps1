param([Parameter(Mandatory=$true)][string]$ObserverScript)
$ErrorActionPreference='Stop'
$t=$null;$parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($ObserverScript,[ref]$t,[ref]$parseErrors)
if($parseErrors){throw 'Observer syntax check failed'}
foreach($name in @('Snapshot','ReferenceStorageSummary')){
 $node=$ast.Find({param($n) $n-is [Management.Automation.Language.FunctionDefinitionAst]-and $n.Name-eq $name},$true)
 if(!$node){throw ('Observer fixture missing function '+$name)}
 Invoke-Expression $node.Extent.Text
}
$data=@(@{root='a';bytes=2147483648L},@{root='a';bytes=7L},@{root='b';bytes=11L})
$stats=ReferenceStorageSummary $data
if($stats.observedDataFiles-ne 3-or $stats.observedDataBytes-ne 2147483666L-or $stats.byRoot.Count-ne 2){throw 'Hashtable storage totals failed'}
$a=@($stats.byRoot|Where-Object{$_.root-eq 'a'})[0]
if($a.files-ne 2-or $a.bytes-ne 2147483655L){throw 'Per-root storage totals failed'}
$empty=ReferenceStorageSummary @()
if($empty.observedDataFiles-ne 0-or $empty.observedDataBytes-ne 0-or $empty.byRoot.Count-ne 0){throw 'Empty inventory failed'}
$dir=Join-Path $env:RUNNER_TEMP 'NeoObserverOwnFixture'
New-Item -ItemType Directory -Force (Join-Path $dir 'Tokens')|Out-Null
try{
 [IO.File]::WriteAllBytes((Join-Path $dir 'a.pak'),[byte[]](1))
 [IO.File]::WriteAllBytes((Join-Path $dir 'a.sig'),[byte[]]@())
 [IO.File]::WriteAllBytes((Join-Path $dir 'b.chunk'),[byte[]](1,2,3))
 [IO.File]::WriteAllBytes((Join-Path $dir 'Tokens/fixture.bin'),[byte[]](1,2))
 $items=Snapshot @(@{name='own-fixture';path=$dir})
 $summary=ReferenceStorageSummary $items
 if($summary.observedDataFiles-ne 3-or $summary.observedDataBytes-ne 4){throw 'Real filesystem snapshot or excluded private directory fixture failed'}
}finally{Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue}
Write-Output ('PASS: observer hashtable totals, 64-bit totals, empty inventory, chunk files and excluded private directory; PowerShell '+$PSVersionTable.PSVersion)
