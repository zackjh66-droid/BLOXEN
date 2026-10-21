using System;using System.IO;using System.IO.Compression;using System.Net;using System.Net.Http;using System.Net.Http.Headers;using System.Security.Cryptography;using System.Threading;using System.Threading.Tasks;
namespace Hollowwake.Services {
 public static partial class ContentTransfer {
  public static async Task DownloadIndexedFile(HttpClient http,string url,long archiveBytes,string target,ContentFile file,Func<Task> authorize,IProgress<string> progress,CancellationToken token,Action<long> received=null,Action<long> writtenProgress=null){
   if(!ContentPlan.Safe(file.Path)||!ContentPlan.Hash(file.Sha256)||file.Bytes<0||file.Offset<=0||(file.Compressed<0||(file.Compressed==0&&(file.Bytes!=0||file.Method!=0)))||file.Offset>archiveBytes-file.Compressed||(file.Method!=0&&file.Method!=8))throw new IOException("Invalid signed ZIP data extent.");
   await authorize();var temporary=target+".download.part";Directory.CreateDirectory(Path.GetDirectoryName(target));
   try{
    using(var input=new RemoteExtent(http,url,archiveBytes,file.Offset,file.Compressed,received))using(var decoded=file.Method==8?(Stream)new DeflateStream(input,CompressionMode.Decompress,true):input)using(var output=new FileStream(temporary,FileMode.Create,FileAccess.Write,FileShare.None,1048576,true))using(var sha=SHA256.Create()){
     var buffer=new byte[1048576];long written=0;var check=DateTime.UtcNow;var display=DateTime.MinValue;int read;
     while((read=await decoded.ReadAsync(buffer,0,buffer.Length,token))>0){
      written+=read;if(written>file.Bytes)throw new IOException("Expanded content exceeded its pinned size.");await output.WriteAsync(buffer,0,read,token);writtenProgress?.Invoke(written);sha.TransformBlock(buffer,0,read,buffer,0);
      if((DateTime.UtcNow-display).TotalSeconds>=1){progress.Report("Downloading "+Path.GetFileName(file.Path)+" "+(100.0*written/Math.Max(1,file.Bytes)).ToString("F1")+"%");display=DateTime.UtcNow;}
      if((DateTime.UtcNow-check).TotalSeconds>=30){await authorize();check=DateTime.UtcNow;}
     }
     sha.TransformFinalBlock(new byte[0],0,0);if(written!=file.Bytes||Hex(sha.Hash)!=file.Sha256)throw new IOException("Downloaded content failed its pinned size/SHA-256 check: "+Path.GetFileName(file.Path));
     while(await input.ReadAsync(buffer,0,buffer.Length,token)>0){}if(input.Remaining!=0)throw new IOException("Compressed content ended early.");
    }
    await authorize();token.ThrowIfCancellationRequested();if(File.Exists(target))File.Move(target,target+".replaced-"+Guid.NewGuid().ToString("N"));File.Move(temporary,target);
   }finally{if(File.Exists(temporary))File.Delete(temporary);}
  }
  public static long MeasureSelected(string content,System.Collections.Generic.IList<ContentFile> files){
   long total=0;foreach(var file in files){var path=Destination(content,file.Path);if(File.Exists(path))total=checked(total+new FileInfo(path).Length);}return total;
  }
  static void SaveDataAudit(string root,ContentProgress state){
   Directory.CreateDirectory(root);
   string text="CONTENT_INVENTORY bytes="+state.OnDiskBytes+" total="+state.TotalFiles+" registered="+state.VerifiedFiles+" missing="+Math.Max(0,state.TotalFiles-state.VerifiedFiles)+"\nCONTENT_TRANSFER bytes="+state.NetworkBytes+" total="+state.DownloadBytes+"\n"+(state.Complete?"CONTENT_DATA_READY":"CONTENT_DATA_PENDING")+" bytes="+state.VerifiedBytes+" total="+state.RequiredBytes+"\n";
   var target=Path.Combine(root,"content-data-audit.txt");File.WriteAllText(target,text);
  }
  public static async Task EnsureIndexed(ContentPlan plan,string root,Func<Task> authorize,Func<long,Task> requireSpace,IProgress<string> progress,CancellationToken token,IProgress<ContentProgress> details=null){
   long expected=plan.Validate();string content=Path.Combine(root,"GameClean"),marker=Path.Combine(root,"content-ready.txt");
   if(File.Exists(marker))File.Delete(marker);
   var state=new ContentProgress{RequiredBytes=expected,TotalFiles=plan.Files.Count};var ready=new System.Collections.Generic.HashSet<string>();long missing=0,existing=0;int inspected=0;DateTime last=DateTime.MinValue;var clock=new System.Diagnostics.Stopwatch();
   Action<bool> publish=force=>{if(!force&&(DateTime.UtcNow-last).TotalMilliseconds<500)return;last=DateTime.UtcNow;details?.Report(state.Copy());};
   try{
    await authorize();state.OnDiskBytes=MeasureSelected(content,plan.Files);SaveDataAudit(root,state);publish(true);
    foreach(var file in plan.Files){
     token.ThrowIfCancellationRequested();state.FileName=Path.GetFileName(file.Path);progress.Report("Checking installed data "+(++inspected)+" / "+plan.Files.Count);
     if(await Matches(Destination(content,file.Path),file.Bytes,file.Sha256,token)){ready.Add(file.Path);existing+=file.Bytes;state.VerifiedBytes+=file.Bytes;state.VerifiedFiles++;}
     else{missing=checked(missing+file.Bytes);state.DownloadBytes=checked(state.DownloadBytes+file.Compressed);}
     publish(true);
    }
    if(missing>0)await requireSpace(checked(missing+1073741824L));
    clock.Start();state.Phase=missing>0?"Downloading":"Checking existing files";publish(true);
    using(var handler=new HttpClientHandler{AllowAutoRedirect=false,UseCookies=false})using(var http=new HttpClient(handler){Timeout=TimeSpan.FromHours(2)}){
     foreach(var file in plan.Files){
      if(ready.Contains(file.Path))continue;await authorize();state.FileName=Path.GetFileName(file.Path);state.CurrentBytes=0;state.CurrentFileBytes=file.Bytes;
      var message=new InlineStringProgress(text=>{progress.Report(text);publish(false);});
      try{await DownloadIndexedFile(http,plan.ArchiveUrl,plan.ArchiveBytes,Destination(content,file.Path),file,authorize,message,token,n=>{state.NetworkBytes+=n;state.BytesPerSecond=state.NetworkBytes/Math.Max(.001,clock.Elapsed.TotalSeconds);publish(false);},n=>{state.CurrentBytes=n;state.OnDiskBytes=existing+n;publish(false);});}
      catch(ArchiveRangeIgnoredException){
       state.Phase="Downloading sequential stream";state.DownloadBytes=checked(state.NetworkBytes+plan.ArchiveBytes);progress.Report("Archive host requires a sequential download. Streaming and verifying game data; no full ZIP will be stored.");publish(true);
       await DownloadSequentialArchive(http,plan.ArchiveUrl,plan.ArchiveBytes,plan.ArchiveSha256,content,plan.Files,ready,authorize,token,
        n=>{state.NetworkBytes+=n;state.BytesPerSecond=state.NetworkBytes/Math.Max(.001,clock.Elapsed.TotalSeconds);publish(false);},
        selected=>{state.FileName=Path.GetFileName(selected.Path);state.CurrentBytes=0;state.CurrentFileBytes=selected.Bytes;publish(true);},
        n=>{state.CurrentBytes=n;state.OnDiskBytes=existing+n;publish(false);},
        selected=>{existing+=selected.Bytes;state.VerifiedFiles++;state.VerifiedBytes+=selected.Bytes;state.CurrentBytes=0;state.OnDiskBytes=MeasureSelected(content,plan.Files);SaveDataAudit(root,state);publish(true);});
       break;
      }
      ready.Add(file.Path);existing+=file.Bytes;state.VerifiedFiles++;state.VerifiedBytes+=file.Bytes;state.OnDiskBytes=MeasureSelected(content,plan.Files);SaveDataAudit(root,state);publish(true);
     }
    }
    // Independent second pass reads the final paths. A ready set/marker alone is never proof.
    state.Phase="Final verification";state.VerifiedFiles=0;state.VerifiedBytes=0;state.CurrentBytes=0;state.CurrentFileBytes=0;publish(true);
    foreach(var file in plan.Files){
     token.ThrowIfCancellationRequested();await authorize();state.FileName=Path.GetFileName(file.Path);
     if(!await Matches(Destination(content,file.Path),file.Bytes,file.Sha256,token))throw new IOException("Installed data is missing or changed: "+Path.GetFileName(file.Path));
     state.VerifiedFiles++;state.VerifiedBytes+=file.Bytes;publish(true);
    }
    state.OnDiskBytes=MeasureSelected(content,plan.Files);
    if(state.VerifiedFiles!=plan.Files.Count||state.VerifiedBytes!=expected||state.OnDiskBytes!=expected)throw new IOException("Final installed data count/size differs.");
    await authorize();token.ThrowIfCancellationRequested();state.Complete=true;state.Phase="Verified";
    SaveDataAudit(root,state);File.WriteAllText(marker,plan.Id);publish(true);
    progress.Report("Game-data hashes checked; confirming Windows storage: "+state.VerifiedFiles+" files, "+(state.OnDiskBytes/1000000000.0).ToString("F2")+" GB on disk. This session downloaded "+(state.NetworkBytes/1000000000.0).ToString("F2")+" GB.");
   }catch{
    state.Complete=false;state.Phase=token.IsCancellationRequested?"Paused":"Failed";
    if(File.Exists(marker))File.Delete(marker);
    state.OnDiskBytes=MeasureSelected(content,plan.Files);SaveDataAudit(root,state);publish(true);throw;
   }
  }
  sealed class InlineStringProgress:IProgress<string>{readonly Action<string> send;public InlineStringProgress(Action<string> action){send=action;}public void Report(string text){send(text);}}
  sealed class RemoteExtent:Stream {
   readonly HttpClient http;readonly string url;readonly long archiveBytes,start,total;readonly Action<long> received;long consumed,chunkRemaining;int interruptions;HttpResponseMessage response;Stream input;string etag;
   public RemoteExtent(HttpClient client,string source,long archiveSize,long offset,long bytes,Action<long> progress){http=client;url=source;archiveBytes=archiveSize;start=offset;total=bytes;received=progress;}
   public long Remaining{get{return total-consumed;}}
   void CloseChunk(){if(input!=null){input.Dispose();input=null;}if(response!=null){response.Dispose();response=null;}chunkRemaining=0;}
   async Task OpenChunk(CancellationToken token){
    CloseChunk();long offset=start+consumed;long length=Math.Min(16L*1024*1024,Remaining);
    using(var request=new HttpRequestMessage(HttpMethod.Get,url)){
     request.Headers.Range=new RangeHeaderValue(offset,offset+length-1);request.Headers.UserAgent.ParseAdd("Hollowwake/0.5.8");
     // A separate unauthenticated HTTP client is used: no project bearer/cookie.
     response=await http.SendAsync(request,HttpCompletionOption.ResponseHeadersRead,token);var range=response.Content.Headers.ContentRange;
     if(response.StatusCode==HttpStatusCode.OK){CloseChunk();throw new ArchiveRangeIgnoredException();}
     if(response.StatusCode!=HttpStatusCode.PartialContent||range==null||range.Unit!="bytes"||range.From!=offset||range.To!=offset+length-1||range.Length!=archiveBytes||response.Content.Headers.ContentLength.HasValue&&response.Content.Headers.ContentLength.Value!=length){CloseChunk();throw new InvalidDataException("Archive host refused the pinned content range/status. No game data was marked ready.");}
     string current=response.Headers.ETag==null?null:response.Headers.ETag.ToString();if(etag!=null&&etag!=current){CloseChunk();throw new InvalidDataException("Archive identity changed during download.");}if(etag==null)etag=current;
     input=await response.Content.ReadAsStreamAsync();chunkRemaining=length;
    }
   }
   public override async Task<int> ReadAsync(byte[] b,int o,int n,CancellationToken token){
    if(n==0||Remaining==0)return 0;
    while(true){token.ThrowIfCancellationRequested();try{
     if(input==null||chunkRemaining==0)await OpenChunk(token);
     int read=await input.ReadAsync(b,o,(int)Math.Min(n,chunkRemaining),token);if(read==0)throw new IOException("Content range ended early.");consumed+=read;chunkRemaining-=read;received?.Invoke(read);return read;
    }catch(ArchiveRangeIgnoredException){throw;}catch(InvalidDataException){throw;}catch(Exception ex)when(ex is IOException||ex is HttpRequestException){CloseChunk();if(++interruptions>8)throw new IOException("Content connection failed repeatedly; verified files are retained. Retry to restart the current file.");await Task.Delay(1000,token);}}
   }
   public override int Read(byte[] b,int o,int n){return ReadAsync(b,o,n,CancellationToken.None).GetAwaiter().GetResult();}
   public override bool CanRead{get{return true;}}public override bool CanSeek{get{return false;}}public override bool CanWrite{get{return false;}}public override long Length{get{return total;}}public override long Position{get{return consumed;}set{throw new NotSupportedException();}}
   protected override void Dispose(bool disposing){if(disposing)CloseChunk();base.Dispose(disposing);}public override void Flush(){}public override long Seek(long x,SeekOrigin y){throw new NotSupportedException();}public override void SetLength(long x){throw new NotSupportedException();}public override void Write(byte[] b,int o,int n){throw new NotSupportedException();}
  }
 }
}
