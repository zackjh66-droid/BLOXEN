// Original fallback for a source which ignores HTTP Range. One forward-only
// bounded ZIP stream; no full archive on disk and no executable extraction.
using System;using System.IO;using System.IO.Compression;using System.Linq;using System.Collections.Generic;using System.Net;using System.Net.Http;using System.Security.Cryptography;using System.Threading;using System.Threading.Tasks;
namespace Hollowwake.Services {
 public sealed class ArchiveRangeIgnoredException:IOException {public ArchiveRangeIgnoredException():base("Archive host ignored HTTP byte ranges; a verified sequential transfer is required.") {}}
 public static partial class ContentTransfer {
  public static async Task DownloadSequentialArchive(HttpClient http,string url,long archiveBytes,string expectedHash,string content,IList<ContentFile> files,HashSet<string> ready,Func<Task> authorize,CancellationToken token,Action<long> received,Action<ContentFile> starting,Action<long> writtenProgress,Action<ContentFile> committed){
   if(archiveBytes<1||archiveBytes>16L*1024*1024*1024||!ContentPlan.Hash(expectedHash)||files.Count==0||files.Count>10000)throw new IOException("Invalid sequential archive bounds.");
   var ordered=files.OrderBy(f=>f.Offset).ToList();long previous=0;var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
   foreach(var f in ordered){if(!ContentPlan.Safe(f.Path)||!ContentPlan.Hash(f.Sha256)||!names.Add(f.Path)||f.Bytes<0||f.Bytes>12L*1024*1024*1024||f.Offset<previous||f.Offset<=0||f.Compressed<0||f.Compressed==0&&(f.Bytes!=0||f.Method!=0)||f.Offset>archiveBytes-f.Compressed||(f.Method!=0&&f.Method!=8))throw new IOException("Invalid sequential data extent.");previous=checked(f.Offset+f.Compressed);}
   await authorize();token.ThrowIfCancellationRequested();
   using(var request=new HttpRequestMessage(HttpMethod.Get,url)){
    request.Headers.UserAgent.ParseAdd("Hollowwake/0.5.8");
    using(var response=await http.SendAsync(request,HttpCompletionOption.ResponseHeadersRead,token)){
     if(response.StatusCode!=HttpStatusCode.OK||response.Content.Headers.ContentLength.HasValue&&response.Content.Headers.ContentLength.Value!=archiveBytes)throw new IOException("Archive host refused the pinned full-stream length/status.");
     using(var raw=await response.Content.ReadAsStreamAsync())using(var counted=new ArchiveCounter(raw,archiveBytes,authorize,received)){
      var buffer=new byte[1048576];
      foreach(var file in ordered){
       token.ThrowIfCancellationRequested();
       await Discard(counted,file.Offset-counted.Consumed,buffer,token);
       if(ready.Contains(file.Path)){await Discard(counted,file.Compressed,buffer,token);continue;}
       starting?.Invoke(file);var target=Destination(content,file.Path);var temporary=target+".download.part";Directory.CreateDirectory(Path.GetDirectoryName(target));
       try{
        using(var extent=new ArchiveExtent(counted,file.Compressed))using(var decoded=file.Method==8?(Stream)new DeflateStream(extent,CompressionMode.Decompress,true):extent)using(var output=new FileStream(temporary,FileMode.Create,FileAccess.Write,FileShare.None,1048576,true))using(var sha=SHA256.Create()){
         long written=0;int n;
         while((n=await decoded.ReadAsync(buffer,0,buffer.Length,token))>0){written=checked(written+n);if(written>file.Bytes)throw new IOException("Sequential expansion exceeds its pinned file size.");await output.WriteAsync(buffer,0,n,token);sha.TransformBlock(buffer,0,n,buffer,0);writtenProgress?.Invoke(written);}
         sha.TransformFinalBlock(new byte[0],0,0);
         if(written!=file.Bytes||Hex(sha.Hash)!=file.Sha256)throw new IOException("Sequential content failed its pinned size/SHA-256: "+Path.GetFileName(file.Path));
         await Discard(extent,extent.Remaining,buffer,token);await output.FlushAsync(token);
        }
        await authorize();token.ThrowIfCancellationRequested();
        if(File.Exists(target))File.Move(target,target+".replaced-"+Guid.NewGuid().ToString("N"));File.Move(temporary,target);ready.Add(file.Path);committed?.Invoke(file);
       }finally{if(File.Exists(temporary))File.Delete(temporary);}
      }
      // Include unselected extents and the central directory in the original
      // archive SHA-256. Per-file hashes are still mandatory before each commit.
      await Discard(counted,archiveBytes-counted.Consumed,buffer,token);
      if(await counted.ReadAsync(buffer,0,1,token)!=0)throw new IOException("Sequential archive exceeds its pinned length.");
      if(counted.FinishHash()!=expectedHash)throw new IOException("Sequential original archive SHA-256 mismatch. Individually verified files were preserved; installation is not marked ready.");
      await authorize();token.ThrowIfCancellationRequested();
     }
    }
   }
  }
  static async Task Discard(Stream input,long count,byte[] buffer,CancellationToken token){
   if(count<0)throw new IOException("Sequential archive position moved past a pinned extent.");
   while(count>0){int n=await input.ReadAsync(buffer,0,(int)Math.Min(buffer.Length,count),token);if(n==0)throw new EndOfStreamException("Sequential archive ended before the pinned extent.");count-=n;}
  }
  sealed class ArchiveCounter:Stream {
   readonly Stream source;readonly long maximum;readonly Func<Task> authorize;readonly Action<long> received;readonly SHA256 sha=SHA256.Create();DateTime check=DateTime.UtcNow;public long Consumed;
   public ArchiveCounter(Stream input,long bytes,Func<Task> auth,Action<long> progress){source=input;maximum=bytes;authorize=auth;received=progress;}
   public override async Task<int> ReadAsync(byte[] buffer,int offset,int count,CancellationToken token){
    token.ThrowIfCancellationRequested();if((DateTime.UtcNow-check).TotalSeconds>=30){await authorize();check=DateTime.UtcNow;}
    int n=await source.ReadAsync(buffer,offset,count,token);Consumed=checked(Consumed+n);if(Consumed>maximum)throw new IOException("Sequential response exceeds pinned archive size.");if(n>0){sha.TransformBlock(buffer,offset,n,buffer,offset);received?.Invoke(n);}return n;
   }
   public string FinishHash(){if(Consumed!=maximum)throw new EndOfStreamException("Sequential archive length differs.");sha.TransformFinalBlock(new byte[0],0,0);return Hex(sha.Hash);}
   protected override void Dispose(bool disposing){if(disposing)sha.Dispose();base.Dispose(disposing);}
   public override int Read(byte[] b,int o,int n){return ReadAsync(b,o,n,CancellationToken.None).GetAwaiter().GetResult();}public override bool CanRead=>true;public override bool CanSeek=>false;public override bool CanWrite=>false;public override long Length=>maximum;public override long Position{get=>Consumed;set=>throw new NotSupportedException();}public override void Flush(){}public override long Seek(long o,SeekOrigin s){throw new NotSupportedException();}public override void SetLength(long n){throw new NotSupportedException();}public override void Write(byte[] b,int o,int n){throw new NotSupportedException();}
  }
  sealed class ArchiveExtent:Stream {
   readonly Stream source;public long Remaining;
   public ArchiveExtent(Stream input,long bytes){source=input;Remaining=bytes;}
   public override async Task<int> ReadAsync(byte[] buffer,int offset,int count,CancellationToken token){if(Remaining==0||count==0)return 0;int n=await source.ReadAsync(buffer,offset,(int)Math.Min(count,Remaining),token);if(n==0)throw new EndOfStreamException("Pinned compressed file extent ended early.");Remaining-=n;return n;}
   public override int Read(byte[] b,int o,int n){return ReadAsync(b,o,n,CancellationToken.None).GetAwaiter().GetResult();}public override bool CanRead=>true;public override bool CanSeek=>false;public override bool CanWrite=>false;public override long Length=>throw new NotSupportedException();public override long Position{get=>throw new NotSupportedException();set=>throw new NotSupportedException();}public override void Flush(){}public override long Seek(long o,SeekOrigin s){throw new NotSupportedException();}public override void SetLength(long n){throw new NotSupportedException();}public override void Write(byte[] b,int o,int n){throw new NotSupportedException();}
  }
 }
}
