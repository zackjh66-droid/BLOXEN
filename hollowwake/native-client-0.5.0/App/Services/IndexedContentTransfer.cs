using System;using System.IO;using System.IO.Compression;using System.Net;using System.Net.Http;using System.Net.Http.Headers;using System.Security.Cryptography;using System.Threading;using System.Threading.Tasks;
namespace Hollowwake.Services {
 public static partial class ContentTransfer {
  public static async Task DownloadIndexedFile(HttpClient http,string url,long archiveBytes,string target,ContentFile file,Func<Task> authorize,IProgress<string> progress,CancellationToken token){
   if(!ContentPlan.Safe(file.Path)||!ContentPlan.Hash(file.Sha256)||file.Bytes<0||file.Offset<=0||file.Compressed<=0||file.Offset>archiveBytes-file.Compressed||(file.Method!=0&&file.Method!=8))throw new IOException("Invalid signed ZIP data extent.");
   await authorize();var temporary=target+".download.part";Directory.CreateDirectory(Path.GetDirectoryName(target));
   try{
    using(var input=new RemoteExtent(http,url,archiveBytes,file.Offset,file.Compressed))using(var decoded=file.Method==8?(Stream)new DeflateStream(input,CompressionMode.Decompress,true):input)using(var output=new FileStream(temporary,FileMode.Create,FileAccess.Write,FileShare.None,1048576,true))using(var sha=SHA256.Create()){
     var buffer=new byte[1048576];long written=0;var check=DateTime.UtcNow;var display=DateTime.MinValue;int read;
     while((read=await decoded.ReadAsync(buffer,0,buffer.Length,token))>0){
      written+=read;if(written>file.Bytes)throw new IOException("Expanded content exceeded its pinned size.");await output.WriteAsync(buffer,0,read,token);sha.TransformBlock(buffer,0,read,buffer,0);
      if((DateTime.UtcNow-display).TotalSeconds>=1){progress.Report("Downloading "+Path.GetFileName(file.Path)+" "+(100.0*written/Math.Max(1,file.Bytes)).ToString("F1")+"%");display=DateTime.UtcNow;}
      if((DateTime.UtcNow-check).TotalSeconds>=30){await authorize();check=DateTime.UtcNow;}
     }
     sha.TransformFinalBlock(new byte[0],0,0);if(written!=file.Bytes||Hex(sha.Hash)!=file.Sha256)throw new IOException("Downloaded content failed its pinned size/SHA-256 check: "+Path.GetFileName(file.Path));
     while(await input.ReadAsync(buffer,0,buffer.Length,token)>0){}if(input.Remaining!=0)throw new IOException("Compressed content ended early.");
    }
    await authorize();token.ThrowIfCancellationRequested();if(File.Exists(target))File.Move(target,target+".replaced-"+Guid.NewGuid().ToString("N"));File.Move(temporary,target);
   }finally{if(File.Exists(temporary))File.Delete(temporary);}
  }
  public static async Task EnsureIndexed(ContentPlan plan,string root,Func<Task> authorize,Func<long,Task> requireSpace,IProgress<string> progress,CancellationToken token){
   plan.Validate();await authorize();var content=Path.Combine(root,"GameClean");long missing=0;var ready=new System.Collections.Generic.HashSet<string>();int checkedFiles=0;
   foreach(var f in plan.Files){token.ThrowIfCancellationRequested();progress.Report("Checking game content "+(++checkedFiles)+" / "+plan.Files.Count);if(await Matches(Destination(content,f.Path),f.Bytes,f.Sha256,token))ready.Add(f.Path);else missing=checked(missing+f.Bytes);}
   if(missing>0)await requireSpace(checked(missing+1073741824L));
   using(var handler=new HttpClientHandler{AllowAutoRedirect=false,UseCookies=false})using(var http=new HttpClient(handler){Timeout=TimeSpan.FromHours(2)}){
    foreach(var f in plan.Files){if(ready.Contains(f.Path))continue;await authorize();await DownloadIndexedFile(http,plan.ArchiveUrl,plan.ArchiveBytes,Destination(content,f.Path),f,authorize,progress,token);ready.Add(f.Path);progress.Report("Verified game content "+ready.Count+" / "+plan.Files.Count);}
   }
   await authorize();Directory.CreateDirectory(root);File.WriteAllText(Path.Combine(root,"content-ready.txt"),plan.Id);progress.Report("Game content verified. No full ZIP was stored; client compatibility is still checked before launch.");
  }
  sealed class RemoteExtent:Stream {
   readonly HttpClient http;readonly string url;readonly long archiveBytes,start,total;long consumed,chunkRemaining;int interruptions;HttpResponseMessage response;Stream input;string etag;
   public RemoteExtent(HttpClient client,string source,long archiveSize,long offset,long bytes){http=client;url=source;archiveBytes=archiveSize;start=offset;total=bytes;}
   public long Remaining{get{return total-consumed;}}
   void CloseChunk(){if(input!=null){input.Dispose();input=null;}if(response!=null){response.Dispose();response=null;}chunkRemaining=0;}
   async Task OpenChunk(CancellationToken token){
    CloseChunk();long offset=start+consumed;long length=Math.Min(16L*1024*1024,Remaining);
    using(var request=new HttpRequestMessage(HttpMethod.Get,url)){
     request.Headers.Range=new RangeHeaderValue(offset,offset+length-1);
     // A separate unauthenticated HTTP client is used: no project bearer/cookie.
     response=await http.SendAsync(request,HttpCompletionOption.ResponseHeadersRead,token);var range=response.Content.Headers.ContentRange;
     if(response.StatusCode!=HttpStatusCode.PartialContent||range==null||range.Unit!="bytes"||range.From!=offset||range.To!=offset+length-1||range.Length!=archiveBytes||response.Content.Headers.ContentLength.HasValue&&response.Content.Headers.ContentLength.Value!=length){CloseChunk();throw new InvalidDataException("Archive host refused the exact content range; no full-archive fallback was started.");}
     string current=response.Headers.ETag==null?null:response.Headers.ETag.ToString();if(etag!=null&&etag!=current){CloseChunk();throw new InvalidDataException("Archive identity changed during download.");}if(etag==null)etag=current;
     input=await response.Content.ReadAsStreamAsync();chunkRemaining=length;
    }
   }
   public override async Task<int> ReadAsync(byte[] b,int o,int n,CancellationToken token){
    if(n==0||Remaining==0)return 0;
    while(true){token.ThrowIfCancellationRequested();try{
     if(input==null||chunkRemaining==0)await OpenChunk(token);
     int read=await input.ReadAsync(b,o,(int)Math.Min(n,chunkRemaining),token);if(read==0)throw new IOException("Content range ended early.");consumed+=read;chunkRemaining-=read;return read;
    }catch(InvalidDataException){throw;}catch(Exception ex)when(ex is IOException||ex is HttpRequestException){CloseChunk();if(++interruptions>8)throw new IOException("Content connection failed repeatedly; verified files are retained. Retry to restart the current file.");await Task.Delay(1000,token);}}
   }
   public override int Read(byte[] b,int o,int n){return ReadAsync(b,o,n,CancellationToken.None).GetAwaiter().GetResult();}
   public override bool CanRead{get{return true;}}public override bool CanSeek{get{return false;}}public override bool CanWrite{get{return false;}}public override long Length{get{return total;}}public override long Position{get{return consumed;}set{throw new NotSupportedException();}}
   protected override void Dispose(bool disposing){if(disposing)CloseChunk();base.Dispose(disposing);}public override void Flush(){}public override long Seek(long x,SeekOrigin y){throw new NotSupportedException();}public override void SetLength(long x){throw new NotSupportedException();}public override void Write(byte[] b,int o,int n){throw new NotSupportedException();}
  }
 }
}
