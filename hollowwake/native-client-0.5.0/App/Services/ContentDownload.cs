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
   foreach(var x in document.GetNamedArray("files")){var f=x.GetObject();double count=f.GetNamedNumber("bytes");if(count<0||count!=Math.Floor(count)||count>12L*1024*1024*1024)throw new IOException("Invalid content size.");p.Files.Add(new ContentFile{Path=f.GetNamedString("path"),Bytes=(long)count,Sha256=f.GetNamedString("sha256")});}p.Validate();return p;
  }
  static async Task<ContentPlan> Packaged(){
   // Only the immutable signed installed package can supply this fallback.
   // Nothing is read from an imported/unsigned LocalState manifest.
   var file=await Windows.ApplicationModel.Package.Current.InstalledLocation.GetFileAsync("ContentPlan621.json");var text=await FileIO.ReadTextAsync(file);var document=JsonObject.Parse(text);string id;
   using(var sha=SHA256.Create())id=BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(text))).Replace("-","").ToLowerInvariant();
   document["manifestId"]=JsonValue.CreateStringValue(id);document["expiresAt"]=JsonValue.CreateNumberValue(DateTimeOffset.UtcNow.ToUnixTimeMilliseconds()+15*60*1000);return Parse(document);
  }
  static bool CatalogUnavailable(ApiException error){return(error.Status==HttpStatusCode.NotFound&&(error.Code=="not_found"||error.Code=="route_not_found"||error.Code=="invalid_response"))||(error.Status==HttpStatusCode.ServiceUnavailable&&error.Code=="content_not_configured")||(error.Status==HttpStatusCode.OK&&error.Code=="invalid_response");}
  public static async Task<string> AuditPackagedPlan(){
   var plan=await Packaged();return "{\"scope\":\"Signed installed metadata parse only; no download, authentication, game or Xbox test\",\"files\":"+plan.Files.Count+",\"expandedBytes\":"+plan.Validate()+",\"archiveBytes\":"+plan.ArchiveBytes+",\"manifestId\":\""+plan.Id+"\"}";
  }
  public static async Task Run(ApiClient api,string projectToken,IProgress<string> progress,CancellationToken token){
   // A missing deployment endpoint must not turn into an account bypass.
   await api.SendAsync("/api/device/profile",null,projectToken,token);ContentPlan plan;bool fromPackage=false;
   try{plan=Parse(await api.SendAsync("/api/device/content/6.21",null,projectToken,token));}
   catch(ApiException error)when(CatalogUnavailable(error)){plan=await Packaged();fromPackage=true;progress.Report("Using content hashes from the signed package; linked-account authorization remains required.");}
   long lastCheck=0;Func<Task> authorize=async()=>{long now=DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();if(now-lastCheck<30000&&now<plan.ExpiresAt)return;await api.SendAsync("/api/device/profile",null,projectToken,token);if(DateTimeOffset.UtcNow.ToUnixTimeMilliseconds()>=plan.ExpiresAt){if(fromPackage){plan.ExpiresAt=now+15*60*1000;}else{var renewed=Parse(await api.SendAsync("/api/device/content/6.21",null,projectToken,token));if(renewed.Id!=plan.Id)throw new IOException("Content manifest changed during transfer; restart the download.");plan.ExpiresAt=renewed.ExpiresAt;}}lastCheck=DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();};
   Func<long,Task> space=async required=>{var info=await ApplicationData.Current.LocalFolder.Properties.RetrievePropertiesAsync(new[]{"System.FreeSpace"});if(!info.ContainsKey("System.FreeSpace")||Convert.ToUInt64(info["System.FreeSpace"])<(ulong)required)throw new IOException("Insufficient Xbox app storage for verified content. Required free bytes: "+required);};
   string localRoot=ApplicationData.Current.LocalFolder.Path;await Task.Run(()=>ContentTransfer.Ensure(plan,localRoot,authorize,space,progress,token));
  }
 }
}
