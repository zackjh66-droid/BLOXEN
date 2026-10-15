using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using Windows.Storage;
namespace Hollowwake.Services {
 public static class GameImport {
  public const long ArchiveBytes=12528679454L;
  const string ArchiveHash="bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5";
  const string Source="https://public-build-archives-bucket.erafn.dev/6.21.zip";
  static readonly HashSet<string> Quarantine=new HashSet<string>(StringComparer.OrdinalIgnoreCase){"6.21/FortniteGame/Binaries/Win64/FortniteLauncher.exe","6.21/FortniteGame/Binaries/Win64/Launcher.bat","6.21/FortniteGame/Binaries/Win64/imgui.ini"};
  public static string Root {get{return Path.Combine(ApplicationData.Current.LocalFolder.Path,"GameClean");}}
  public static bool Ready {get{return File.Exists(Path.Combine(ApplicationData.Current.LocalFolder.Path,"import-complete.txt"));}}
  static string Hex(byte[] b){return BitConverter.ToString(b).Replace("-","").ToLowerInvariant();}
  static async Task Free(long required){var p=await ApplicationData.Current.LocalFolder.Properties.RetrievePropertiesAsync(new[]{"System.FreeSpace"});if(!p.ContainsKey("System.FreeSpace"))throw new IOException("Cannot verify app storage space.");var available=Convert.ToUInt64(p["System.FreeSpace"]);if(available<(ulong)required)throw new IOException("Insufficient app storage: need "+(required/1000000000.0).ToString("F1")+" GB free. Increase Xbox development storage or free space, then retry.");}
  static bool Safe(string n){return n.StartsWith("6.21/",StringComparison.Ordinal)&&!n.Contains("\\")&&!n.Contains(":")&&!n.Contains("\0")&&!n.Split('/').Any(p=>p==".."||p=="."||p.EndsWith(" ")||p.EndsWith("."));}
  public static async Task Run(IProgress<string> progress,CancellationToken token){
   if(Ready){progress.Report("Pinned source has already been imported; native code will recheck executable hashes.");return;}
   var local=ApplicationData.Current.LocalFolder.Path;long existing=Directory.Exists(Root)?Directory.EnumerateFiles(Root,"*",SearchOption.AllDirectories).Sum(f=>new FileInfo(f).Length):0;string zip=Path.Combine(local,"Original-6.21.zip");long offset=File.Exists(zip)?new FileInfo(zip).Length:0;if(offset>ArchiveBytes)throw new IOException("Existing archive is larger than the pinned source. Preserve it and clear only this experimental app's import data before retrying.");
   await Free(Math.Max(0,ArchiveBytes-offset+26771777271L-existing)+1073741824L);
   using(var handler=new HttpClientHandler{AllowAutoRedirect=false,UseCookies=false})using(var http=new HttpClient(handler){Timeout=TimeSpan.FromMinutes(10)}){
    while(offset<ArchiveBytes){token.ThrowIfCancellationRequested();long end=Math.Min(ArchiveBytes-1,offset+32*1024*1024-1);using(var request=new HttpRequestMessage(HttpMethod.Get,Source)){
     request.Headers.Range=new System.Net.Http.Headers.RangeHeaderValue(offset,end);
     using(var response=await http.SendAsync(request,HttpCompletionOption.ResponseHeadersRead,token)){
      var range=response.Content.Headers.ContentRange;if(response.StatusCode!=HttpStatusCode.PartialContent||range==null||range.From!=offset||range.To!=end||range.Length!=ArchiveBytes)throw new IOException("Archive server refused the exact byte range; HTTP "+(int)response.StatusCode+". No redirected or guessed source was used.");
      using(var input=await response.Content.ReadAsStreamAsync())using(var output=new FileStream(zip,FileMode.OpenOrCreate,FileAccess.Write,FileShare.None,262144,true)){
       output.Position=offset;byte[] buffer=new byte[262144];long remaining=end-offset+1;
       while(remaining>0){int n=await input.ReadAsync(buffer,0,(int)Math.Min(buffer.Length,remaining),token);if(n==0)throw new IOException("Archive download ended early; retry resumes saved bytes.");await output.WriteAsync(buffer,0,n,token);remaining-=n;offset+=n;progress.Report("Downloading immutable archive: "+(100.0*offset/ArchiveBytes).ToString("F1")+"%");}
       if(await input.ReadAsync(buffer,0,1,token)!=0)throw new IOException("Range response exceeded its declared size.");await output.FlushAsync(token);
      }
     }
    }}
   }
   progress.Report("Verifying all 12.53 GB against the inspected archive SHA-256…");
   using(var stream=File.OpenRead(zip))using(var sha=SHA256.Create()){
    byte[] buffer=new byte[1048576];int n;while((n=await stream.ReadAsync(buffer,0,buffer.Length,token))>0)sha.TransformBlock(buffer,0,n,buffer,0);sha.TransformFinalBlock(new byte[0],0,0);if(Hex(sha.Hash)!=ArchiveHash)throw new IOException("Archive SHA-256 mismatch. No files were extracted. Preserve the rejected ZIP for inspection.");
   }
   using(var stream=File.OpenRead(zip))using(var archive=new ZipArchive(stream,ZipArchiveMode.Read,false)){
    if(archive.Entries.Count>50000)throw new IOException("Excessive ZIP entry count.");var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);long expanded=0;
    foreach(var e in archive.Entries){var n=e.FullName;if(!Safe(n)||!seen.Add(n))throw new IOException("Unsafe or duplicate ZIP path.");int kind=(e.ExternalAttributes>>16)&0xf000;if(kind!=0&&kind!=0x8000&&kind!=0x4000)throw new IOException("ZIP links/special files are not permitted.");if(e.Length<0||e.Length>12L*1024*1024*1024||e.Length/Math.Max(1,e.CompressedLength)>1000)throw new IOException("Excessive ZIP expansion.");expanded=checked(expanded+e.Length);if(expanded>32L*1024*1024*1024)throw new IOException("Expanded ZIP exceeds limit.");var ext=Path.GetExtension(n).ToLowerInvariant();if(new[]{".zip",".rar",".7z",".cab",".iso",".gz",".tar"}.Contains(ext))throw new IOException("Nested archive requires separate inspection.");}
    await Free(Math.Max(0,expanded-existing)+1073741824L);Directory.CreateDirectory(Root);var record=new List<string>{"Source archive unchanged; SHA256="+ArchiveHash,"No pristine-distribution claim; no launcher/scripts executed."};int done=0;
    foreach(var e in archive.Entries){token.ThrowIfCancellationRequested();bool quarantined=Quarantine.Contains(e.FullName);var targetBase=quarantined?Path.Combine(local,"GameQuarantine"):Root;var target=Path.GetFullPath(Path.Combine(targetBase,e.FullName.Replace('/',Path.DirectorySeparatorChar)));if(!target.StartsWith(Path.GetFullPath(targetBase)+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))throw new IOException("Destination escape rejected.");
     if(e.FullName.EndsWith("/")){Directory.CreateDirectory(target);continue;}Directory.CreateDirectory(Path.GetDirectoryName(target));long bytes=0;string hash;
     using(var input=e.Open())using(var output=new FileStream(target,FileMode.Create,FileAccess.Write,FileShare.None,1048576,true))using(var sha=SHA256.Create()){
      byte[] buffer=new byte[1048576];int n;while((n=await input.ReadAsync(buffer,0,buffer.Length,token))>0){bytes+=n;if(bytes>e.Length)throw new IOException("Expanded member exceeds declared size.");sha.TransformBlock(buffer,0,n,buffer,0);await output.WriteAsync(buffer,0,n,token);}sha.TransformFinalBlock(new byte[0],0,0);hash=Hex(sha.Hash);
     }if(bytes!=e.Length)throw new IOException("Expanded member ended early.");record.Add((quarantined?"QUARANTINE ":"COPY ")+e.FullName+" bytes="+bytes+" sha256="+hash);if(++done%5==0)progress.Report("Extracted "+done+" files; original ZIP preserved.");
    }
    await FileIO.WriteLinesAsync(await ApplicationData.Current.LocalFolder.CreateFileAsync("import-inventory.txt",CreationCollisionOption.ReplaceExisting),record);
   }
   await FileIO.WriteTextAsync(await ApplicationData.Current.LocalFolder.CreateFileAsync("import-complete.txt",CreationCollisionOption.ReplaceExisting),ArchiveHash);progress.Report("Import complete. Main image is verified again by the native loader before mapping.");
  }
 }
}
