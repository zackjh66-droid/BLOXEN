using System;
using System.IO;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Windows.Data.Json;
using Windows.Storage;
namespace Hollowwake.Services {
 public static class ContentDownload {
  static ContentPlan Parse(JsonObject document){
   if(document.GetNamedNumber("schema")!=1)throw new IOException("Unknown content manifest schema.");var a=document.GetNamedObject("archive");var p=new ContentPlan{Build=document.GetNamedString("build"),Id=document.GetNamedString("manifestId"),ArchiveUrl=a.GetNamedString("url"),ArchiveSha256=a.GetNamedString("sha256"),ArchiveBytes=(long)a.GetNamedNumber("bytes"),ExpiresAt=(long)document.GetNamedNumber("expiresAt")};
   foreach(var x in document.GetNamedArray("files")){var f=x.GetObject();double count=f.GetNamedNumber("bytes");if(count<0||count!=Math.Floor(count)||count>12L*1024*1024*1024)throw new IOException("Invalid content size.");p.Files.Add(new ContentFile{Path=f.GetNamedString("path"),Bytes=(long)count,Sha256=f.GetNamedString("sha256")});}if(p.Files.Count!=139||p.Validate()!=26449827840L)throw new IOException("Required Season 6 data inventory differs.");return p;
  }
  static async Task<ContentPlan> Packaged(){
   // Only the immutable signed installed package can supply this fallback.
   // Nothing is read from an imported/unsigned LocalState manifest.
   var file=await Windows.ApplicationModel.Package.Current.InstalledLocation.GetFileAsync("ContentPlan621.json");var text=await FileIO.ReadTextAsync(file);var document=JsonObject.Parse(text);string id;
   using(var sha=SHA256.Create())id=BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(text))).Replace("-","").ToLowerInvariant();
   document["manifestId"]=JsonValue.CreateStringValue(id);document["expiresAt"]=JsonValue.CreateNumberValue(DateTimeOffset.UtcNow.ToUnixTimeMilliseconds()+15*60*1000);return Parse(document);
  }
  static async Task AttachIndex(ContentPlan plan){
   var file=await Windows.ApplicationModel.Package.Current.InstalledLocation.GetFileAsync("ContentIndex621.json");var index=JsonObject.Parse(await FileIO.ReadTextAsync(file));
   if(index.GetNamedNumber("schema")!=1||index.GetNamedString("build")!=plan.Build||index.GetNamedString("archiveSha256")!=plan.ArchiveSha256||index.GetNamedNumber("archiveBytes")!=plan.ArchiveBytes)throw new IOException("Signed content index differs from the archive pin.");
   var entries=new System.Collections.Generic.Dictionary<string,JsonObject>(StringComparer.Ordinal);foreach(var v in index.GetNamedArray("entries")){var e=v.GetObject();entries.Add(e.GetNamedString("path"),e);}
   if(entries.Count!=plan.Files.Count)throw new IOException("Signed content index count differs.");
   foreach(var f in plan.Files){JsonObject e;if(!entries.TryGetValue(f.Path,out e)||e.GetNamedNumber("bytes")!=f.Bytes||e.GetNamedString("sha256")!=f.Sha256)throw new IOException("Signed content record differs.");f.Offset=(long)e.GetNamedNumber("offset");f.Compressed=(long)e.GetNamedNumber("compressed");f.Method=(int)e.GetNamedNumber("method");if(f.Offset<=0||(f.Compressed<0||(f.Compressed==0&&(f.Bytes!=0||f.Method!=0)))||f.Offset>plan.ArchiveBytes-f.Compressed||(f.Method!=0&&f.Method!=8))throw new IOException("Invalid signed content extent.");}
  }
  static bool CatalogUnavailable(ApiException error){return(error.Status==HttpStatusCode.NotFound&&(error.Code=="not_found"||error.Code=="route_not_found"||error.Code=="invalid_response"))||(error.Status==HttpStatusCode.ServiceUnavailable&&error.Code=="content_not_configured")||(error.Status==HttpStatusCode.OK&&error.Code=="invalid_response");}
  public static async Task<string> AuditPackagedPlan(){
   var plan=await Packaged();await AttachIndex(plan);return "{\"scope\":\"Signed installed metadata parse only; no download, authentication, game or Xbox test\",\"files\":"+plan.Files.Count+",\"expandedBytes\":"+plan.Validate()+",\"archiveBytes\":"+plan.ArchiveBytes+",\"manifestId\":\""+plan.Id+"\"}";
  }
  static async Task<long> FreeBytes(){
   var info=await ApplicationData.Current.LocalFolder.Properties.RetrievePropertiesAsync(new[]{"System.FreeSpace"});
   if(!info.ContainsKey("System.FreeSpace"))throw new IOException("Xbox app-storage free space is unavailable.");return checked((long)Convert.ToUInt64(info["System.FreeSpace"]));
  }
  static async Task<long> AuditCode(string installed,IProgress<string> progress,CancellationToken token){
   string root=ApplicationData.Current.LocalFolder.Path;File.WriteAllText(Path.Combine(root,"content-code-audit.txt"),"CONTENT_CODE bytes=0 total=38 registered=0 missing=38\n");
   var doc=JsonObject.Parse(await FileIO.ReadTextAsync(await Windows.ApplicationModel.Package.Current.InstalledLocation.GetFileAsync("package-code-index.json")));
   if(doc.GetNamedNumber("schema")!=1||doc.GetNamedString("build")!="6.21-CL-4526925")throw new IOException("Unknown installed code inventory.");
   var records=new System.Collections.Generic.List<ContentFile>();
   foreach(var value in doc.GetNamedArray("files")){var f=value.GetObject();records.Add(new ContentFile{Path=f.GetNamedString("path"),Bytes=(long)f.GetNamedNumber("bytes"),Sha256=f.GetNamedString("sha256")});}
   if(records.Count!=38)throw new IOException("Installed native code inventory is incomplete.");
   long bytes=await Task.Run(()=>ContentTransfer.VerifyCode(installed,records,progress,token));
   File.WriteAllText(Path.Combine(root,"content-code-audit.txt"),"CONTENT_CODE bytes="+bytes+" total=38 registered=38 missing=0\n");progress.Report("Client installed: "+(bytes/1000000.0).ToString("F1")+" MB · 38/38 code files verified");return bytes;
  }
  sealed class Capture:IProgress<ContentProgress>{
   readonly IProgress<ContentProgress> target;public ContentProgress Last;
   public Capture(IProgress<ContentProgress> output){target=output;}
   public void Report(ContentProgress state){Last=state;target?.Report(state);}
  }
  public static async Task<ContentProgress> Run(ApiClient api,string projectToken,IProgress<string> progress,CancellationToken token,IProgress<ContentProgress> details=null,IProgress<string> codeProgress=null){
   await api.SendAsync("/api/device/profile",null,projectToken,token);
   string root=ApplicationData.Current.LocalFolder.Path,installed=Windows.ApplicationModel.Package.Current.InstalledLocation.Path;long before=await FreeBytes();
   File.WriteAllText(Path.Combine(root,"content-space-audit.txt"),"CONTENT_SPACE_BEFORE bytes="+before+"\n");
   try{
    await AuditCode(installed,codeProgress??progress,token);
    var plan=await Packaged();await AttachIndex(plan);var capture=new Capture(details);
    long lastCheck=0;
    Func<Task> authorize=async()=>{
     long now=DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();if(now-lastCheck<30000&&now<plan.ExpiresAt)return;
     await api.SendAsync("/api/device/profile",null,projectToken,token);
     if(now>=plan.ExpiresAt)plan.ExpiresAt=now+15*60*1000;lastCheck=now;
    };
    if(File.Exists(Path.Combine(installed,"bundled-content.json"))){
     var marker=JsonObject.Parse(File.ReadAllText(Path.Combine(installed,"bundled-content.json")));
     if(marker.GetNamedNumber("schema")!=1||marker.GetNamedString("build")!=plan.Build||marker.GetNamedString("mode")!="installed-package-only"||marker.GetNamedString("contentPlanSha256")!=plan.Id||marker.GetNamedNumber("files")!=plan.Files.Count||marker.GetNamedNumber("expandedBytes")!=plan.Validate())throw new IOException("Bundled content marker differs.");
     await Task.Run(()=>ContentTransfer.VerifyInstalled(installed,plan.Files,authorize,progress,token));await api.SendAsync("/api/device/profile",null,projectToken,token);
     long bytes=ContentTransfer.MeasureSelected(installed,plan.Files);
     var state=new ContentProgress{Phase="Verified",Complete=true,TotalFiles=plan.Files.Count,VerifiedFiles=plan.Files.Count,RequiredBytes=plan.Validate(),VerifiedBytes=bytes,OnDiskBytes=bytes};
     File.WriteAllText(Path.Combine(root,"content-data-audit.txt"),"CONTENT_INVENTORY bytes="+bytes+" total="+plan.Files.Count+" registered="+plan.Files.Count+" missing=0\nCONTENT_TRANSFER bytes=0 total=0\nCONTENT_DATA_READY bytes="+bytes+" total="+plan.Validate()+"\n");capture.Report(state);return state;
    }
    progress.Report("Preparing the automatic game-data download.");
    Func<long,Task> space=async required=>{if(await FreeBytes()<required)throw new IOException("Insufficient Xbox app storage. Required free bytes: "+required);};
    await Task.Run(()=>ContentTransfer.EnsureIndexed(plan,root,authorize,space,progress,token,capture));
    await api.SendAsync("/api/device/profile",null,projectToken,token);
    if(capture.Last==null||!capture.Last.Complete||capture.Last.VerifiedFiles!=139||capture.Last.VerifiedBytes!=26449827840L||capture.Last.OnDiskBytes!=26449827840L)throw new IOException("Final data audit is incomplete.");
    return capture.Last;
   }finally{
    try{long after=await FreeBytes();File.AppendAllText(Path.Combine(root,"content-space-audit.txt"),"CONTENT_SPACE_AFTER bytes="+after+"\n");}catch{}
   }
  }
 }
}
