// Original shared transfer core; no Windows APIs, executable loading or account tokens.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
namespace Hollowwake.Services {
 public sealed class ContentFile {public string Path;public long Bytes;public string Sha256;public long Offset=-1,Compressed;public int Method=-1;}
 public sealed class ContentPlan {
  public string Build,Id,ArchiveUrl,ArchiveSha256;public long ArchiveBytes,ExpiresAt;public readonly List<ContentFile> Files=new List<ContentFile>();
  public long Validate(){
   if(Build!="6.21-CL-4526925"||!Hash(Id)||Files.Count==0||Files.Count>10000||ArchiveBytes!=12528679454L||ArchiveSha256!="bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5"||ArchiveUrl!="https://public-build-archives-bucket.erafn.dev/6.21.zip")throw new IOException("Invalid content build or archive pin.");
   long total=0;var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
   foreach(var f in Files){if(!Safe(f.Path)||!seen.Add(f.Path)||f.Bytes<0||f.Bytes>12L*1024*1024*1024||!Hash(f.Sha256))throw new IOException("Unsafe content manifest entry.");total=checked(total+f.Bytes);}
   if(total>64L*1024*1024*1024)throw new IOException("Content expansion exceeds limit.");return total;
  }
  public static bool Hash(string s){return s!=null&&Regex.IsMatch(s,"^[0-9a-f]{64}$");}
  public static bool Safe(string p){
   if(String.IsNullOrEmpty(p)||p.Length>=512||!(p.StartsWith("6.21/FortniteGame/Content/",StringComparison.Ordinal)||p.StartsWith("6.21/Engine/Binaries/ThirdParty/CEF3/Win64/",StringComparison.Ordinal))||!Regex.IsMatch(p,@"\.(pak|sig|mp4|srt|png|jpg|bmp|txt|bin|dat)$",RegexOptions.IgnoreCase)||Regex.IsMatch(p,@"[\\:\x00-\x1f%]"))return false;
   foreach(var part in p.Split('/'))if(part.Length==0||part=="."||part==".."||part.EndsWith(".")||part.EndsWith(" ")||Regex.IsMatch(part,@"^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)",RegexOptions.IgnoreCase))return false;return true;
  }
 }
 public static partial class ContentTransfer {
  static string Hex(byte[] b){return BitConverter.ToString(b).Replace("-","").ToLowerInvariant();}
  public static async Task<bool> Matches(string path,long bytes,string expected,CancellationToken token){
   if(!File.Exists(path)||new FileInfo(path).Length!=bytes)return false;
   using(var stream=File.OpenRead(path))using(var sha=SHA256.Create()){byte[] b=new byte[1048576];int n;while((n=await stream.ReadAsync(b,0,b.Length,token))>0)sha.TransformBlock(b,0,n,b,0);sha.TransformFinalBlock(new byte[0],0,0);return Hex(sha.Hash)==expected;}
  }
  static string Destination(string root,string relative){if(!ContentPlan.Safe(relative))throw new IOException("Unsafe content path.");var prefix=System.IO.Path.GetFullPath(root).TrimEnd(System.IO.Path.DirectorySeparatorChar)+System.IO.Path.DirectorySeparatorChar;var result=System.IO.Path.GetFullPath(System.IO.Path.Combine(prefix,relative.Replace('/',System.IO.Path.DirectorySeparatorChar)));if(!result.StartsWith(prefix,StringComparison.OrdinalIgnoreCase))throw new IOException("Content path escapes destination.");return result;}
  public static async Task VerifyInstalled(string root,IList<ContentFile> files,Func<Task> authorize,IProgress<string> progress,CancellationToken token){
   var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);int done=0;
   foreach(var file in files){token.ThrowIfCancellationRequested();if(!ContentPlan.Safe(file.Path)||!ContentPlan.Hash(file.Sha256)||file.Bytes<0||!seen.Add(file.Path))throw new IOException("Invalid bundled content record.");await authorize();if(!await Matches(Destination(root,file.Path),file.Bytes,file.Sha256,token))throw new IOException("Installed content missing or changed: "+file.Path+". Reinstall the complete private package; no download fallback was used.");progress.Report("Verifying installed content "+(++done)+" / "+files.Count);}
   if(done==0)throw new IOException("Empty bundled content inventory.");await authorize();
  }
  // Used by both the real app and owned-fixture tests. No file is ready before its actual hash passes.
  public static async Task ExtractVerified(string archive,string root,IList<ContentFile> files,Func<Task> authorize,IProgress<string> progress,CancellationToken token){
   var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);var audit=new List<string>();
   using(var input=File.OpenRead(archive))using(var zip=new ZipArchive(input,ZipArchiveMode.Read,false)){
    if(zip.Entries.Count>100000)throw new IOException("Too many archive entries.");long expanded=0;
    foreach(var e in zip.Entries){var n=e.FullName;int kind=(e.ExternalAttributes>>16)&0xf000;if(!seen.Add(n)||n.StartsWith("/")||n.Contains("\\")||n.Contains(":")||n.Contains("\0")||Array.Exists(n.Split('/'),x=>x==".."||x==".")||(kind!=0&&kind!=0x8000&&kind!=0x4000))throw new IOException("Unsafe archive inventory.");expanded=checked(expanded+e.Length);if(expanded>64L*1024*1024*1024)throw new IOException("Archive expansion exceeds limit.");}
    seen.Clear();int done=0;
    foreach(var f in files){token.ThrowIfCancellationRequested();if(!ContentPlan.Safe(f.Path)||!ContentPlan.Hash(f.Sha256)||f.Bytes<0||!seen.Add(f.Path))throw new IOException("Invalid content record.");await authorize();var target=Destination(root,f.Path);
     if(await Matches(target,f.Bytes,f.Sha256,token)){audit.Add("VERIFIED "+f.Path+" "+f.Sha256);continue;}
     var member=zip.GetEntry(f.Path);if(member==null||member.Length!=f.Bytes)throw new IOException("Pinned content member missing or resized.");Directory.CreateDirectory(System.IO.Path.GetDirectoryName(target));var temporary=target+".hw-part";
     try{
      using(var source=member.Open())using(var output=new FileStream(temporary,FileMode.Create,FileAccess.Write,FileShare.None,1048576,true)){byte[] b=new byte[1048576];long total=0;int n;while((n=await source.ReadAsync(b,0,b.Length,token))>0){total+=n;if(total>f.Bytes)throw new IOException("Content expansion exceeds declared length.");await output.WriteAsync(b,0,n,token);}if(total!=f.Bytes)throw new IOException("Content ended early.");}
      if(!await Matches(temporary,f.Bytes,f.Sha256,token))throw new IOException("Content SHA-256 mismatch: "+f.Path);
      await authorize();if(File.Exists(target)){var rejected=target+".replaced-"+Guid.NewGuid().ToString("N");File.Move(target,rejected);audit.Add("REPLACED "+f.Path+" previous copy retained under "+System.IO.Path.GetFileName(rejected));}
      File.Move(temporary,target);audit.Add("VERIFIED "+f.Path+" "+f.Sha256);progress.Report("Verified content "+(++done)+" / "+files.Count);
     }finally{if(File.Exists(temporary))File.Delete(temporary);}
    }
   }
   Directory.CreateDirectory(root);File.WriteAllLines(System.IO.Path.Combine(root,"content-verification.txt"),audit);
  }
  public static async Task Ensure(ContentPlan plan,string localRoot,Func<Task> authorize,Func<long,Task> requireSpace,IProgress<string> progress,CancellationToken token){
   long expanded=plan.Validate();await authorize();var content=System.IO.Path.Combine(localRoot,"GameClean");var original=System.IO.Path.Combine(localRoot,"Original-6.21.zip");var marker=System.IO.Path.Combine(localRoot,"content-ready.txt");if(File.Exists(marker))File.Delete(marker);
   bool all=true;long missingBytes=0;foreach(var file in plan.Files)if(!await Matches(Destination(content,file.Path),file.Bytes,file.Sha256,token)){all=false;missingBytes=checked(missingBytes+file.Bytes);}
   if(!all){
    bool archiveReady=await Matches(original,plan.ArchiveBytes,plan.ArchiveSha256,token);
    await requireSpace(checked((archiveReady?0:plan.ArchiveBytes)+missingBytes+1073741824L));
    if(!archiveReady){
     if(File.Exists(original))throw new IOException("Existing original archive does not match the selected content build; it was preserved.");
     var temporary=original+".hw-part";
     using(var handler=new HttpClientHandler{AllowAutoRedirect=false,UseCookies=false})using(var http=new HttpClient(handler){Timeout=TimeSpan.FromHours(2)})using(var request=new HttpRequestMessage(HttpMethod.Get,plan.ArchiveUrl))using(var response=await http.SendAsync(request,HttpCompletionOption.ResponseHeadersRead,token)){
      if(response.StatusCode!=HttpStatusCode.OK||response.Content.Headers.ContentLength.HasValue&&response.Content.Headers.ContentLength.Value!=plan.ArchiveBytes)throw new IOException("Archive server returned an unexpected status or length.");
      using(var input=await response.Content.ReadAsStreamAsync())using(var output=new FileStream(temporary,FileMode.Create,FileAccess.Write,FileShare.None,1048576,true)){byte[] b=new byte[1048576];long total=0;var check=DateTime.UtcNow;int n;while((n=await input.ReadAsync(b,0,b.Length,token))>0){total+=n;if(total>plan.ArchiveBytes)throw new IOException("Archive transfer exceeds pinned size.");await output.WriteAsync(b,0,n,token);if((DateTime.UtcNow-check).TotalSeconds>=30){await authorize();check=DateTime.UtcNow;progress.Report("Downloading content archive "+(100.0*total/plan.ArchiveBytes).ToString("F1")+"%");} }if(total!=plan.ArchiveBytes)throw new IOException("Content archive ended early; no content is ready.");}
     }
     if(!await Matches(temporary,plan.ArchiveBytes,plan.ArchiveSha256,token))throw new IOException("Content archive SHA-256 mismatch; rejected temporary bytes retained for inspection.");File.Move(temporary,original);
    }
    await ExtractVerified(original,content,plan.Files,authorize,progress,token);
   }
   await authorize();File.WriteAllText(marker,plan.Id);progress.Report("Content verified; native client startup may proceed.");
  }
 }
}
