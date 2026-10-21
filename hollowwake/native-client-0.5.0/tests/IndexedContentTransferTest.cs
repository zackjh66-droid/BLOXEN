using System;using System.IO;using System.IO.Compression;using System.Net;using System.Net.Http;using System.Net.Sockets;using System.Security.Cryptography;using System.Text;using System.Threading;using System.Threading.Tasks;using System.Collections.Generic;using Hollowwake.Services;
public class IndexedContentTransferTest {
 static string Hash(byte[] data){using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(data)).Replace("-","").ToLowerInvariant();}
 static void Check(bool ok,string message){if(!ok)throw new Exception(message);}
 sealed class Server:IDisposable {
  readonly HttpListener listener=new HttpListener();readonly byte[] bytes;readonly string mode;readonly Task serving;public string Url;public int Requests;public bool SawCredentials;public List<long> Starts=new List<long>();
  public Server(byte[] data,string behavior){bytes=data;mode=behavior;var socket=new TcpListener(IPAddress.Loopback,0);socket.Start();int port=((IPEndPoint)socket.LocalEndpoint).Port;socket.Stop();Url="http://127.0.0.1:"+port+"/fixture.zip";listener.Prefixes.Add("http://127.0.0.1:"+port+"/");listener.Start();serving=Task.Run(async()=>{while(listener.IsListening){HttpListenerContext context;try{context=await listener.GetContextAsync();}catch{break;}try{
   Requests++;SawCredentials|=context.Request.Headers["Authorization"]!=null||context.Request.Headers["Cookie"]!=null;var range=context.Request.Headers["Range"];bool streaming=mode.StartsWith("stream",StringComparison.Ordinal);Check(streaming||range!=null&&range.StartsWith("bytes="),"No bounded range");var bounds=streaming?new[]{"0",(bytes.Length-1).ToString()}:range.Substring(6).Split('-');long start=Int64.Parse(bounds[0]),end=Int64.Parse(bounds[1]);Starts.Add(start);Check(start>=0&&end<bytes.Length&&end>=start,"Range outside actual fixture");context.Response.StatusCode=mode=="ignored"||streaming?200:206;context.Response.Headers["Content-Range"]="bytes "+start+"-"+end+"/"+bytes.Length;context.Response.Headers["ETag"]="\"owned-content-fixture\"";context.Response.ContentLength64=end-start+1;
   if((mode=="cut"||mode=="stream-cut")&&Requests==1){await context.Response.OutputStream.WriteAsync(bytes,(int)start,12345);context.Response.Abort();continue;}
   var block=new byte[65536];for(long position=start;position<=end;){int n=(int)Math.Min(block.Length,end-position+1);Buffer.BlockCopy(bytes,(int)position,block,0,n);if((mode=="corrupt"||mode=="stream-corrupt")&&position==start)block[0]^=1;await context.Response.OutputStream.WriteAsync(block,0,n);position+=n;}context.Response.Close();
  }catch{try{context.Response.Abort();}catch{}}}});}
  public void Dispose(){listener.Stop();listener.Close();serving.GetAwaiter().GetResult();}
 }
 sealed class Capture:IProgress<ContentProgress>{public ContentProgress Last;public void Report(ContentProgress value){Last=value;}}
 public static void Main(){Run().GetAwaiter().GetResult();}
 public static async Task Run(){var folder=Path.Combine(Path.GetTempPath(),"HwOwnedIndexed-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(folder);try{
  var payload=new byte[20*1024*1024+321];new Random(621).NextBytes(payload);var name="6.21/FortniteGame/Content/Hollowwake-owned-fixture.txt";var archive=Path.Combine(folder,"owned.zip");
  using(var zip=ZipFile.Open(archive,ZipArchiveMode.Create))using(var entry=zip.CreateEntry(name,CompressionLevel.Optimal).Open())await entry.WriteAsync(payload,0,payload.Length);
  var bytes=File.ReadAllBytes(archive);long compressed;using(var zip=ZipFile.OpenRead(archive))compressed=zip.GetEntry(name).CompressedLength;long offset=30+BitConverter.ToUInt16(bytes,26)+BitConverter.ToUInt16(bytes,28);
  var file=new ContentFile{Path=name,Bytes=payload.Length,Sha256=Hash(payload),Offset=offset,Compressed=compressed,Method=8};var progress=new Progress<string>();int authorized=0;Func<Task> allow=()=>{authorized++;return Task.CompletedTask;};
  foreach(var mode in new[]{"normal","cut"})using(var server=new Server(bytes,mode))using(var http=new HttpClient(new HttpClientHandler{UseCookies=false,AllowAutoRedirect=false})){
   var target=Path.Combine(folder,mode,"fixture.txt");await ContentTransfer.DownloadIndexedFile(http,server.Url,bytes.Length,target,file,allow,progress,CancellationToken.None);Check(await ContentTransfer.Matches(target,file.Bytes,file.Sha256,CancellationToken.None),"Actual resumed bytes differ");Check(server.Requests>=(mode=="cut"?3:2),"Actual ranged chunking/resume did not occur");Check(!server.SawCredentials,"Credentials leaked to data host");Check(!File.Exists(target+".download.part"),"Temporary file left after verification");
  }
  using(var server=new Server(bytes,"normal"))using(var http=new HttpClient(new HttpClientHandler{UseCookies=false,AllowAutoRedirect=false})){
   int leases=0;var target=Path.Combine(folder,"lease-renewal","fixture.txt");
   await ContentTransfer.DownloadIndexedFile(http,"https://unused.invalid/archive",bytes.Length,target,file,allow,progress,CancellationToken.None,null,null,()=>{leases++;return Task.FromResult(server.Url+"?lease="+leases);});
   Check(leases==server.Requests&&leases>=2&&await ContentTransfer.Matches(target,file.Bytes,file.Sha256,CancellationToken.None),"Range requests did not reacquire the current lease or verify actual bytes");
   Check(!server.SawCredentials,"Account credentials leaked during lease refresh");
  }
  // Exercise the actual fallback transport and decompressor with an HTTP
  // server which ignores Range and returns the complete original fixture ZIP.
  using(var server=new Server(bytes,"stream"))using(var http=new HttpClient(new HttpClientHandler{UseCookies=false,AllowAutoRedirect=false})){
   var content=Path.Combine(folder,"streamed");long received=0;int commits=0;var ready=new HashSet<string>();bool ignored=false;
   try{await ContentTransfer.DownloadIndexedFile(http,server.Url,bytes.Length,Path.Combine(folder,"range-refused","file"),file,allow,progress,CancellationToken.None);}catch(ArchiveRangeIgnoredException){ignored=true;}
   Check(ignored&&server.Requests==1,"Ignored ranges did not select the explicit streaming fallback");
   await ContentTransfer.DownloadSequentialArchive(http,server.Url,bytes.Length,Hash(bytes),content,new[]{file},ready,allow,CancellationToken.None,n=>received+=n,null,null,f=>commits++);
   var target=Path.Combine(content,name.Replace('/',Path.DirectorySeparatorChar));
   Check(received==bytes.Length&&commits==1&&ready.Contains(name)&&await ContentTransfer.Matches(target,file.Bytes,file.Sha256,CancellationToken.None),"Sequential installation did not write/hash the actual selected file");
   Check(!server.SawCredentials&&Directory.GetFiles(content,"*.zip",SearchOption.AllDirectories).Length==0,"Sequential fallback leaked credentials or stored a full archive");
  }
  foreach(var mode in new[]{"stream-corrupt","stream-cut"})using(var server=new Server(bytes,mode))using(var http=new HttpClient()){
   var content=Path.Combine(folder,mode);bool rejected=false;
   try{await ContentTransfer.DownloadSequentialArchive(http,server.Url,bytes.Length,Hash(bytes),content,new[]{file},new HashSet<string>(),allow,CancellationToken.None,null,null,null,null);}catch(IOException){rejected=true;}catch(HttpRequestException){rejected=true;}
   Check(rejected&&(!Directory.Exists(content)||Directory.GetFiles(content,"*.download.part",SearchOption.AllDirectories).Length==0),"Changed/truncated full stream was accepted or left unverified temporary data");
  }
  using(var server=new Server(bytes,"stream"))using(var http=new HttpClient()){
   var content=Path.Combine(folder,"stream-cancel");var cancel=new CancellationTokenSource();bool rejected=false;
   try{await ContentTransfer.DownloadSequentialArchive(http,server.Url,bytes.Length,Hash(bytes),content,new[]{file},new HashSet<string>(),allow,cancel.Token,n=>cancel.Cancel(),null,null,null);}catch(OperationCanceledException){rejected=true;}
   Check(rejected&&(!Directory.Exists(content)||Directory.GetFiles(content,"*.download.part",SearchOption.AllDirectories).Length==0),"Sequential cancellation did not stop real reads/clean temporary data");
  }
  using(var server=new Server(bytes,"stream"))using(var http=new HttpClient()){
   var content=Path.Combine(folder,"stream-revoked");int checks=0;bool rejected=false;
   try{await ContentTransfer.DownloadSequentialArchive(http,server.Url,bytes.Length,Hash(bytes),content,new[]{file},new HashSet<string>(),()=>{if(++checks>1)throw new UnauthorizedAccessException();return Task.CompletedTask;},CancellationToken.None,null,null,null,null);}catch(UnauthorizedAccessException){rejected=true;}
   Check(rejected&&!File.Exists(Path.Combine(content,name.Replace('/',Path.DirectorySeparatorChar))),"Revoked sequential transfer committed game data");
  }
  Console.WriteLine("PASS real HTTP 200 sequential fallback: actual selected-file decompression/writes/SHA-256, full original archive SHA-256, corruption/truncation rejection, cancellation/revocation cleanup, no ZIP stored and no account credentials sent.");
  var reuse=Path.Combine(folder,"reuse");var existing=Path.Combine(reuse,"GameClean",name.Replace('/',Path.DirectorySeparatorChar));Directory.CreateDirectory(Path.GetDirectoryName(existing));File.Copy(Path.Combine(folder,"normal","fixture.txt"),existing);
  var plan=new ContentPlan{Build="6.21-CL-4526925",Id=Hash(new byte[]{6,2,1}),ArchiveUrl="https://public-build-archives-bucket.erafn.dev/6.21.zip",ArchiveBytes=12528679454L,ArchiveSha256="bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5"};plan.Files.Add(file);
  bool independentMissing=false;
  try{await ContentTransfer.EnsureIndexed(plan,reuse,allow,n=>{independentMissing=true;throw new IOException("Owned missing Windows-storage inventory fixture");},progress,CancellationToken.None,null,null,new HashSet<string>());}catch(IOException){}
  Check(independentMissing&&File.Exists(existing)&&!File.Exists(Path.Combine(reuse,"content-ready.txt")),"Missing independent Windows inventory was replaced by System.IO claims or existing data was removed");
  var captured=new Capture();File.WriteAllText(Path.Combine(reuse,"content-ready.txt"),"old writable marker");
  await ContentTransfer.EnsureIndexed(plan,reuse,allow,n=>{throw new Exception("Valid files must not require new storage");},progress,CancellationToken.None,captured);
  Check(captured.Last.Complete&&captured.Last.NetworkBytes==0&&captured.Last.VerifiedFiles==1&&captured.Last.OnDiskBytes==payload.Length,"Existing data was not independently measured and verified");
  var auditLines=File.ReadAllLines(Path.Combine(reuse,"content-data-audit.txt"));Check(auditLines.Length==3&&auditLines[0].StartsWith("CONTENT_INVENTORY ")&&auditLines[1].StartsWith("CONTENT_TRANSFER ")&&auditLines[2].StartsWith("CONTENT_DATA_READY "),"Structured content receipt lost numeric event boundaries");
  int auditChecks=0;bool tamperRejected=false;
  try{await ContentTransfer.EnsureIndexed(plan,reuse,()=>{if(++auditChecks==2)File.WriteAllBytes(existing,new byte[]{1,2,3});return Task.CompletedTask;},n=>Task.CompletedTask,progress,CancellationToken.None,captured);}catch(IOException){tamperRejected=true;}
  Check(tamperRejected&&!captured.Last.Complete&&captured.Last.OnDiskBytes==3&&!File.Exists(Path.Combine(reuse,"content-ready.txt")),"Changed final data retained a false ready marker");
  var diskFull=Path.Combine(folder,"disk-full");Directory.CreateDirectory(diskFull);File.WriteAllText(Path.Combine(diskFull,"content-ready.txt"),"stale");bool noSpace=false;
  try{await ContentTransfer.EnsureIndexed(plan,diskFull,allow,n=>{Check(n==payload.Length+1073741824L,"Wrong required storage size");throw new IOException("Owned insufficient-storage fixture");},progress,CancellationToken.None,captured);}catch(IOException){noSpace=true;}
  Check(noSpace&&!captured.Last.Complete&&captured.Last.NetworkBytes==0&&captured.Last.OnDiskBytes==0&&!File.Exists(Path.Combine(diskFull,"content-ready.txt")),"Low space became fake installed content");
  var codeRoot=Path.Combine(folder,"owned-code");Directory.CreateDirectory(codeRoot);var code=Path.Combine(codeRoot,"OwnFixture.dll");File.WriteAllBytes(code,new byte[]{6,2,1});
  var codeFiles=new List<ContentFile>{new ContentFile{Path="OwnFixture.dll",Bytes=3,Sha256=Hash(new byte[]{6,2,1})}};
  Check(await ContentTransfer.VerifyCode(codeRoot,codeFiles,progress,CancellationToken.None)==3,"Installed code bytes not measured");
  File.WriteAllBytes(code,new byte[]{6,2,0});bool changedCode=false;try{await ContentTransfer.VerifyCode(codeRoot,codeFiles,progress,CancellationToken.None);}catch(IOException){changedCode=true;}Check(changedCode,"Changed code was accepted");
  foreach(var mode in new[]{"ignored","corrupt"})using(var server=new Server(bytes,mode))using(var http=new HttpClient()){
   var target=Path.Combine(folder,mode,"fixture.txt");bool rejected=false;try{await ContentTransfer.DownloadIndexedFile(http,server.Url,bytes.Length,target,file,allow,progress,CancellationToken.None);}catch(Exception ex)when(ex is IOException||ex is InvalidDataException){rejected=true;}Check(rejected&&!File.Exists(target)&&!File.Exists(target+".download.part"),"Invalid response became ready content");if(mode=="ignored")Check(server.Requests==1,"Ignored range caused an uncontrolled full-archive fallback");
  }
  using(var server=new Server(bytes,"normal"))using(var http=new HttpClient()){
   var target=Path.Combine(folder,"revoked","fixture.txt");bool denied=false;try{await ContentTransfer.DownloadIndexedFile(http,server.Url,bytes.Length,target,file,()=>{throw new UnauthorizedAccessException();},progress,CancellationToken.None);}catch(UnauthorizedAccessException){denied=true;}Check(denied&&server.Requests==0,"Revoked account started network transfer");
   int checks=0;denied=false;try{await ContentTransfer.DownloadIndexedFile(http,server.Url,bytes.Length,target,file,()=>{if(++checks>1)throw new UnauthorizedAccessException();return Task.CompletedTask;},progress,CancellationToken.None);}catch(UnauthorizedAccessException){denied=true;}Check(denied&&!File.Exists(target),"Revocation before commit was ignored");
   var cancelled=new CancellationTokenSource();cancelled.Cancel();bool stopped=false;try{await ContentTransfer.DownloadIndexedFile(http,server.Url,bytes.Length,target,file,allow,progress,cancelled.Token);}catch(OperationCanceledException){stopped=true;}Check(stopped&&!File.Exists(target+".download.part"),"Cancellation left unverified content");
  }
  var emptyBytes=new byte[128];emptyBytes[64]=3;emptyBytes[65]=0;using(var server=new Server(emptyBytes,"normal"))using(var http=new HttpClient()){
   var empty=new ContentFile{Path="6.21/FortniteGame/Content/Owned-empty.txt",Bytes=0,Sha256=Hash(new byte[0]),Offset=64,Compressed=2,Method=8};var target=Path.Combine(folder,"empty","fixture.txt");await ContentTransfer.DownloadIndexedFile(http,server.Url,emptyBytes.Length,target,empty,allow,progress,CancellationToken.None);Check(new FileInfo(target).Length==0,"Empty indexed file failed");
  }
  using(var server=new Server(emptyBytes,"normal"))using(var http=new HttpClient()){
   var empty=new ContentFile{Path="6.21/FortniteGame/Content/Owned-empty-stored.txt",Bytes=0,Sha256=Hash(new byte[0]),Offset=64,Compressed=0,Method=0};var target=Path.Combine(folder,"empty-stored","fixture.txt");await ContentTransfer.DownloadIndexedFile(http,server.Url,emptyBytes.Length,target,empty,allow,progress,CancellationToken.None);Check(new FileInfo(target).Length==0&&server.Requests==0,"Stored empty entry must verify empty SHA without an invalid byte range");
   empty.Bytes=1;bool rejected=false;try{await ContentTransfer.DownloadIndexedFile(http,server.Url,emptyBytes.Length,target,empty,allow,progress,CancellationToken.None);}catch(IOException){rejected=true;}Check(rejected,"Nonempty file with zero compressed bytes accepted");
  }
  Check(authorized>=4,"Authorization was not rechecked before commit");Console.WriteLine("PASS real HTTP range download, 16 MiB chunk boundaries, interrupted compressed-stream resume, actual 20 MiB decompression and SHA-256 verification, ignored-range/corruption rejection, no account credentials on archive requests, authorization before transfer and commit, cancellation and temporary-file cleanup; final-path independent rehash, measured reuse, post-download tamper rejection, stale-marker removal, insufficient-space rejection and installed-code hash verification. Original fixtures only; no game/Xbox execution.");
 }finally{Directory.Delete(folder,true);}}
}
