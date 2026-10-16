using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using Windows.Data.Json;
using Windows.Storage;
namespace Hollowwake.Services {
 public static class BootImport {
  public static bool Ready {get{return File.Exists(Path.Combine(ApplicationData.Current.LocalFolder.Path,"boot-import-complete.txt"));}}
  public static async Task Run(IProgress<string> progress,CancellationToken token){
   if(Ready){progress.Report("Minimum boot files already imported; native loading rechecks executable hashes.");return;}
   var file=await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///boot-inputs.json"));var manifest=JsonObject.Parse(await FileIO.ReadTextAsync(file));var source=manifest.GetNamedString("source");if(source!="https://public-build-archives-bucket.erafn.dev/6.21.zip")throw new IOException("Unexpected boot source.");
   var files=manifest.GetNamedArray("files");if(files.Count>200)throw new IOException("Boot manifest count limit.");var free=await ApplicationData.Current.LocalFolder.Properties.RetrievePropertiesAsync(new[]{"System.FreeSpace"});if(!free.ContainsKey("System.FreeSpace")||Convert.ToUInt64(free["System.FreeSpace"])<2UL*1024*1024*1024)throw new IOException("Minimum boot import needs2GB reported free app storage.");
   var local=ApplicationData.Current.LocalFolder.Path;var cache=Path.Combine(local,"BootDownload");Directory.CreateDirectory(cache);var inventory=new System.Collections.Generic.List<string>{"Minimum file import; whole ZIP not downloaded or authenticated here.","Individual SHA256 values match inspected source. Not a pristine Epic-distribution claim. Launchers/scripts are not selected."};int index=0;
   using(var handler=new HttpClientHandler{AllowAutoRedirect=false,UseCookies=false})using(var client=new HttpClient(handler){Timeout=TimeSpan.FromMinutes(10)}){
    client.DefaultRequestHeaders.UserAgent.ParseAdd("Hollowwake/0.4");
    foreach(var value in files){token.ThrowIfCancellationRequested();var item=value.GetObject();var name=item.GetNamedString("path");if(!name.StartsWith("6.21/",StringComparison.Ordinal)||name.Contains("\\")||name.Contains(":")||name.Split('/').Any(p=>p==".."||p=="."))throw new IOException("Unsafe boot manifest path.");long offset=(long)item.GetNamedNumber("offset"),compressed=(long)item.GetNamedNumber("compressed"),size=(long)item.GetNamedNumber("bytes");int method=(int)item.GetNamedNumber("method");if(offset<0||compressed<0||size<0||size>512L*1024*1024||offset>GameImport.ArchiveBytes-compressed||method!=0&&method!=8)throw new IOException("Boot manifest bounds.");
     var partial=Path.Combine(cache,index+".compressed.part");long received=File.Exists(partial)?new FileInfo(partial).Length:0;if(received>compressed)throw new IOException("Boot partial exceeds pinned compressed size.");progress.Report("Boot import "+(++index)+"/"+files.Count+": "+Path.GetFileName(name));
     while(received<compressed){long end=Math.Min(compressed-1,received+32*1024*1024-1);using(var request=new HttpRequestMessage(HttpMethod.Get,source)){
      request.Headers.Range=new System.Net.Http.Headers.RangeHeaderValue(offset+received,offset+end);using(var response=await client.SendAsync(request,HttpCompletionOption.ResponseHeadersRead,token)){
       var range=response.Content.Headers.ContentRange;if(response.StatusCode!=HttpStatusCode.PartialContent||range==null||range.From!=offset+received||range.To!=offset+end||range.Length!=GameImport.ArchiveBytes)throw new IOException("Boot source refused pinned range: HTTP "+(int)response.StatusCode);
       using(var input=await response.Content.ReadAsStreamAsync())using(var output=new FileStream(partial,FileMode.OpenOrCreate,FileAccess.Write,FileShare.None,262144,true)){
        output.Position=received;byte[] buffer=new byte[262144];long remaining=end-received+1;while(remaining>0){int n=await input.ReadAsync(buffer,0,(int)Math.Min(buffer.Length,remaining),token);if(n==0)throw new IOException("Boot range ended early; retry resumes.");await output.WriteAsync(buffer,0,n,token);remaining-=n;received+=n;}if(await input.ReadAsync(buffer,0,1,token)!=0)throw new IOException("Boot range overflow.");await output.FlushAsync(token);
       }
      }
     }}
     var target=Path.GetFullPath(Path.Combine(GameImport.Root,name.Replace('/',Path.DirectorySeparatorChar)));if(!target.StartsWith(Path.GetFullPath(GameImport.Root)+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))throw new IOException("Boot destination escape.");Directory.CreateDirectory(Path.GetDirectoryName(target));long copied=0;string digest;
     using(var raw=File.OpenRead(partial))using(var input=method==8?(Stream)new DeflateStream(raw,CompressionMode.Decompress,false):raw)using(var output=new FileStream(target,FileMode.Create,FileAccess.Write,FileShare.None,1048576,true))using(var sha=SHA256.Create()){
      byte[] buffer=new byte[1048576];int n;while((n=await input.ReadAsync(buffer,0,buffer.Length,token))>0){copied+=n;if(copied>size)throw new IOException("Boot expansion overflow.");sha.TransformBlock(buffer,0,n,buffer,0);await output.WriteAsync(buffer,0,n,token);}sha.TransformFinalBlock(new byte[0],0,0);digest=BitConverter.ToString(sha.Hash).Replace("-","").ToLowerInvariant();
     }if(copied!=size||digest!=item.GetNamedString("sha256"))throw new IOException("Boot file hash mismatch; entry is not accepted.");inventory.Add("COPY "+name+" bytes="+copied+" sha256="+digest);
    }
   }
   await FileIO.WriteLinesAsync(await ApplicationData.Current.LocalFolder.CreateFileAsync("boot-import-inventory.txt",CreationCollisionOption.ReplaceExisting),inventory);await FileIO.WriteTextAsync(await ApplicationData.Current.LocalFolder.CreateFileAsync("boot-import-complete.txt",CreationCollisionOption.ReplaceExisting),"Individual inspected input hashes verified; full PAK/media files not present.");progress.Report("Minimum boot import complete. Full game assets are still absent; this is an early initialization test.");
  }
 }
}
