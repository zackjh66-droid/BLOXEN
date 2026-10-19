using System;using System.IO;using System.IO.Compression;using System.Net;using System.Net.Http;using System.Net.Sockets;using System.Security.Cryptography;using System.Text;using System.Threading;using System.Threading.Tasks;using System.Collections.Generic;using Hollowwake.Services;
public class IndexedContentTransferTest {
 static string Hash(byte[] data){using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(data)).Replace("-","").ToLowerInvariant();}
 static void Check(bool ok,string message){if(!ok)throw new Exception(message);}
 sealed class Server:IDisposable {
  readonly HttpListener listener=new HttpListener();readonly byte[] bytes;readonly string mode;readonly Task serving;public string Url;public int Requests;public bool SawCredentials;public List<long> Starts=new List<long>();
  public Server(byte[] data,string behavior){bytes=data;mode=behavior;var socket=new TcpListener(IPAddress.Loopback,0);socket.Start();int port=((IPEndPoint)socket.LocalEndpoint).Port;socket.Stop();Url="http://127.0.0.1:"+port+"/fixture.zip";listener.Prefixes.Add("http://127.0.0.1:"+port+"/");listener.Start();serving=Task.Run(async()=>{while(listener.IsListening){HttpListenerContext context;try{context=await listener.GetContextAsync();}catch{break;}try{
   Requests++;SawCredentials|=context.Request.Headers["Authorization"]!=null||context.Request.Headers["Cookie"]!=null;var range=context.Request.Headers["Range"];Check(range!=null&&range.StartsWith("bytes="),"No bounded range");var bounds=range.Substring(6).Split('-');long start=Int64.Parse(bounds[0]),end=Int64.Parse(bounds[1]);Starts.Add(start);Check(start>=0&&end<bytes.Length&&end>=start,"Range outside actual fixture");context.Response.StatusCode=mode=="ignored"?200:206;context.Response.Headers["Content-Range"]="bytes "+start+"-"+end+"/"+bytes.Length;context.Response.Headers["ETag"]="\"owned-content-fixture\"";context.Response.ContentLength64=end-start+1;
   if(mode=="cut"&&Requests==1){await context.Response.OutputStream.WriteAsync(bytes,(int)start,12345);context.Response.Abort();continue;}
   var block=new byte[65536];for(long position=start;position<=end;){int n=(int)Math.Min(block.Length,end-position+1);Buffer.BlockCopy(bytes,(int)position,block,0,n);if(mode=="corrupt"&&position==start)block[0]^=1;await context.Response.OutputStream.WriteAsync(block,0,n);position+=n;}context.Response.Close();
  }catch{try{context.Response.Abort();}catch{}}}});}
  public void Dispose(){listener.Stop();listener.Close();serving.GetAwaiter().GetResult();}
 }
 public static void Main(){Run().GetAwaiter().GetResult();}
 public static async Task Run(){var folder=Path.Combine(Path.GetTempPath(),"HwOwnedIndexed-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(folder);try{
  var payload=new byte[20*1024*1024+321];new Random(621).NextBytes(payload);var name="6.21/FortniteGame/Content/Hollowwake-owned-fixture.txt";var archive=Path.Combine(folder,"owned.zip");
  using(var zip=ZipFile.Open(archive,ZipArchiveMode.Create))using(var entry=zip.CreateEntry(name,CompressionLevel.Optimal).Open())await entry.WriteAsync(payload,0,payload.Length);
  var bytes=File.ReadAllBytes(archive);long compressed;using(var zip=ZipFile.OpenRead(archive))compressed=zip.GetEntry(name).CompressedLength;long offset=30+BitConverter.ToUInt16(bytes,26)+BitConverter.ToUInt16(bytes,28);
  var file=new ContentFile{Path=name,Bytes=payload.Length,Sha256=Hash(payload),Offset=offset,Compressed=compressed,Method=8};var progress=new Progress<string>();int authorized=0;Func<Task> allow=()=>{authorized++;return Task.CompletedTask;};
  foreach(var mode in new[]{"normal","cut"})using(var server=new Server(bytes,mode))using(var http=new HttpClient(new HttpClientHandler{UseCookies=false,AllowAutoRedirect=false})){
   var target=Path.Combine(folder,mode,"fixture.txt");await ContentTransfer.DownloadIndexedFile(http,server.Url,bytes.Length,target,file,allow,progress,CancellationToken.None);Check(await ContentTransfer.Matches(target,file.Bytes,file.Sha256,CancellationToken.None),"Actual resumed bytes differ");Check(server.Requests>=(mode=="cut"?3:2),"Actual ranged chunking/resume did not occur");Check(!server.SawCredentials,"Credentials leaked to data host");Check(!File.Exists(target+".download.part"),"Temporary file left after verification");
  }
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
  Check(authorized>=4,"Authorization was not rechecked before commit");Console.WriteLine("PASS real HTTP range download, 16 MiB chunk boundaries, interrupted compressed-stream resume, actual 20 MiB decompression and SHA-256 verification, ignored-range/corruption rejection, no account credentials on archive requests, authorization before transfer and commit, cancellation and temporary-file cleanup. Original fixtures only; no game/Xbox execution.");
 }finally{Directory.Delete(folder,true);}}
}
