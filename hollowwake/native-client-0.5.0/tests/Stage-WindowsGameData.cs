// Private CI input preparation. Uses the unchanged production stream/hash reader.
// It does not create content-ready markers, consume account tickets or launch code.
using System;using System.IO;using System.Linq;using System.Collections;using System.Collections.Generic;using System.Net;using System.Net.Http;using System.Security.Cryptography;using System.Threading;using System.Threading.Tasks;using System.Web.Script.Serialization;using Hollowwake.Services;
class StageWindowsGameData {
 static readonly JavaScriptSerializer Json=new JavaScriptSerializer{MaxJsonLength=2000000};
 static Dictionary<string,object> Obj(object v){return (Dictionary<string,object>)v;}
 static long Num(object v){return Convert.ToInt64(v);}
 static string Hex(byte[] v){return BitConverter.ToString(v).Replace("-","").ToLowerInvariant();}
 static int Main(string[] args){try{Run(args).GetAwaiter().GetResult();return 0;}catch(Exception e){Console.Error.WriteLine(e.GetType().Name+": "+e.Message);return 1;}}
 static async Task Run(string[] args){
  if(args.Length!=4)throw new ArgumentException("plan index GameClean output-json");
  byte[] raw=File.ReadAllBytes(args[0]);var p=Obj(Json.DeserializeObject(System.Text.Encoding.UTF8.GetString(raw)));var ix=Obj(Json.DeserializeObject(File.ReadAllText(args[1])));var ar=Obj(p["archive"]);
  var plan=new ContentPlan{Build=(string)p["build"],ArchiveUrl=(string)ar["url"],ArchiveBytes=Num(ar["bytes"]),ArchiveSha256=(string)ar["sha256"]};using(var sha=SHA256.Create())plan.Id=Hex(sha.ComputeHash(raw));
  if((string)ix["build"]!=plan.Build||(string)ix["archiveSha256"]!=plan.ArchiveSha256||Num(ix["archiveBytes"])!=plan.ArchiveBytes)throw new IOException("Index archive/build mismatch");
  var entries=new Dictionary<string,Dictionary<string,object>>(StringComparer.Ordinal);foreach(object o in (object[])ix["entries"]){var e=Obj(o);entries.Add((string)e["path"],e);}
  foreach(object o in (object[])p["files"]){var f=Obj(o);string path=(string)f["path"];var e=entries[path];if(Num(f["bytes"])!=Num(e["bytes"])||(string)f["sha256"]!=(string)e["sha256"])throw new IOException("Index file mismatch");plan.Files.Add(new ContentFile{Path=path,Bytes=Num(f["bytes"]),Sha256=(string)f["sha256"],Offset=Num(e["offset"]),Compressed=Num(e["compressed"]),Method=Convert.ToInt32(e["method"])});}
  long total=plan.Validate();if(plan.Files.Count!=139||entries.Count!=139||total!=26449827840L)throw new IOException("Unexpected complete data inventory");
  string root=Path.GetFullPath(args[2]);Directory.CreateDirectory(root);long free=new DriveInfo(Path.GetPathRoot(root)).AvailableFreeSpace;if(free<total+1073741824L)throw new IOException("Insufficient free bytes for complete private game data: available="+free+" required="+(total+1073741824L));
  ServicePointManager.SecurityProtocol=SecurityProtocolType.Tls12;long network=0,committed=0;int count=0;DateTime last=DateTime.MinValue;Func<Task> pins=()=>{if(plan.Validate()!=total)throw new IOException("Input pins changed");return Task.CompletedTask;};
  using(var cancellation=new CancellationTokenSource(TimeSpan.FromMinutes(30)))using(var handler=new HttpClientHandler{AllowAutoRedirect=false,UseCookies=false})using(var http=new HttpClient(handler){Timeout=TimeSpan.FromMinutes(30)}){
   await ContentTransfer.DownloadSequentialArchive(http,plan.ArchiveUrl,plan.ArchiveBytes,plan.ArchiveSha256,root,plan.Files,new HashSet<string>(),pins,cancellation.Token,
    n=>{network+=n;if((DateTime.UtcNow-last).TotalSeconds>=15){Console.WriteLine("INPUT networkBytes="+network+" committedBytes="+committed+" files="+count);last=DateTime.UtcNow;}},f=>Console.WriteLine("INPUT starting "+f.Path),null,f=>{committed+=f.Bytes;count++;});
  }
  if(network!=plan.ArchiveBytes||committed!=total||count!=139||ContentTransfer.MeasureSelected(root,plan.Files)!=total)throw new IOException("Final staging inventory mismatch");
  File.WriteAllText(args[3],Json.Serialize(new{scope="Private Windows test preparation; production app independently verifies data before launch",build=plan.Build,files=count,bytes=committed,networkBytes=network,originalArchiveSha256=plan.ArchiveSha256,wholeArchiveHashPassed=true,allFileHashesPassed=true,source=plan.ArchiveUrl,ownerAuthenticatedDownloaderTested=false,fortniteExecuted=false}));Console.WriteLine("INPUT complete: original archive and all 139 file hashes passed");
 }
}
