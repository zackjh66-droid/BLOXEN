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
   string root=ApplicationData.Current.LocalFolder.Path;File.WriteAllText(Path.Combine(root,"content-code-audit.txt"),"CONTENT_CODE inventory verification pending\n");
   var doc=JsonObject.Parse(await FileIO.ReadTextAsync(await Windows.ApplicationModel.Package.Current.InstalledLocation.GetFileAsync("package-code-index.json")));
   if(doc.GetNamedNumber("schema")!=1||doc.GetNamedString("build")!="6.21-CL-4526925")throw new IOException("Unknown installed code inventory.");
   var records=new System.Collections.Generic.List<ContentFile>();
   foreach(var value in doc.GetNamedArray("files")){var f=value.GetObject();records.Add(new ContentFile{Path=f.GetNamedString("path"),Bytes=(long)f.GetNamedNumber("bytes"),Sha256=f.GetNamedString("sha256")});}
   var manifest=JsonObject.Parse(await FileIO.ReadTextAsync(await Windows.ApplicationModel.Package.Current.InstalledLocation.GetFileAsync("native-package-manifest.json")));
   if(manifest.GetNamedString("build")!=doc.GetNamedString("build"))throw new IOException("Signed prepared code build differs.");
   var images=new System.Collections.Generic.List<string>();foreach(var value in manifest.GetNamedArray("files"))images.Add(value.GetObject().GetNamedString("packageRelativePath"));
   var proxies=new System.Collections.Generic.List<string>();foreach(var value in manifest.GetNamedArray("proxyDlls"))proxies.Add(value.GetObject().GetNamedString("proxyDll"));
   var indexed=new System.Collections.Generic.List<string>();foreach(var record in records)indexed.Add(record.Path);
   int expected=CodeInventory.Validate(images,proxies,indexed,doc.GetNamedString("providerVariant","audio"));
   long bytes=await Task.Run(()=>ContentTransfer.VerifyCode(installed,records,progress,token));
   File.WriteAllText(Path.Combine(root,"content-code-audit.txt"),"CONTENT_CODE bytes="+bytes+" total="+expected+" registered="+expected+" missing=0\n");progress.Report("Client installed: "+(bytes/1000000.0).ToString("F1")+" MB · "+expected+"/"+expected+" code files verified");return bytes;
  }
  // A separate WinRT pass must agree with the streamed SHA-256 verifier.
  // Do not infer installation from a ready marker or signed manifest sizes.
  public static async Task<long> VerifyWindowsFiles(string content,System.Collections.Generic.IList<ContentFile> files,CancellationToken token){
   long total=0;
   foreach(var record in files){
    token.ThrowIfCancellationRequested();
    var expected=Path.GetFullPath(Path.Combine(content,record.Path.Replace('/',Path.DirectorySeparatorChar)));
    var file=await StorageFile.GetFileFromPathAsync(expected);
    if(!String.Equals(file.Path,expected,StringComparison.OrdinalIgnoreCase))throw new IOException("Windows storage resolved an unexpected game-data path.");
    var properties=await file.GetBasicPropertiesAsync();
    if(properties.Size!=(ulong)record.Bytes)throw new IOException("Windows storage reports missing or different game-data bytes: "+Path.GetFileName(expected));
    using(var input=await file.OpenReadAsync()){
     if(input.Size!=properties.Size)throw new IOException("Game data changed during Windows storage verification.");
     if(input.Size>0){
      using(var first=new Windows.Storage.Streams.DataReader(input.GetInputStreamAt(0)))if(await first.LoadAsync(1)!=1)throw new IOException("Windows storage cannot read the game-data file.");
      using(var last=new Windows.Storage.Streams.DataReader(input.GetInputStreamAt(input.Size-1)))if(await last.LoadAsync(1)!=1)throw new IOException("Windows storage cannot read the end of the game-data file.");
     }
    }
    total=checked(total+record.Bytes);
   }
   return total;
  }
  public static async Task StorageContract(){
   var local=ApplicationData.Current.LocalFolder;string name="owned-storage-check-"+Guid.NewGuid().ToString("N")+".bin";
   var records=new System.Collections.Generic.List<ContentFile>{new ContentFile{Path=name,Bytes=3}};
   var file=await local.CreateFileAsync(name,CreationCollisionOption.FailIfExists);
   bool sizeRejected=false,missingRejected=false;
   try{
    await FileIO.WriteBytesAsync(file,new byte[]{6,2,1});
    if(await VerifyWindowsFiles(local.Path,records,CancellationToken.None)!=3)throw new IOException("Owned WinRT storage count differs.");
    var ownedPlan=new ContentPlan();ownedPlan.Files.AddRange(records);if((await ExistingWindowsFiles(local.Path,ownedPlan,CancellationToken.None)).Count!=1)throw new IOException("Existing real WinRT file was missed");
    records[0].Bytes=4;if((await ExistingWindowsFiles(local.Path,ownedPlan,CancellationToken.None)).Count!=0)throw new IOException("Wrong-size existing file became a reuse candidate");try{await VerifyWindowsFiles(local.Path,records,CancellationToken.None);}catch(IOException){sizeRejected=true;}
    await file.DeleteAsync(StorageDeleteOption.PermanentDelete);
    if((await ExistingWindowsFiles(local.Path,ownedPlan,CancellationToken.None)).Count!=0)throw new IOException("Missing real WinRT file became a reuse candidate");
    try{await VerifyWindowsFiles(local.Path,records,CancellationToken.None);}catch(FileNotFoundException){missingRejected=true;}
    if(!sizeRejected||!missingRejected)throw new IOException("Owned WinRT storage rejection contract failed.");
    await FileIO.WriteTextAsync(await local.CreateFileAsync("native-storage-check.json",CreationCollisionOption.ReplaceExisting),"{\"scope\":\"Owned UWP file create/open/measure/read/delete; no game or Xbox execution\",\"passed\":true,\"missingRejected\":true,\"sizeRejected\":true}");
   }finally{try{await file.DeleteAsync(StorageDeleteOption.PermanentDelete);}catch{}}
  }
  static async Task ConfirmWindowsStorage(string content,ContentPlan plan,string root,CancellationToken token){
   Hollowwake.App.ExecutionStage="windows-storage-verification";
   File.WriteAllText(Path.Combine(root,"content-location.txt"),"Package: "+Windows.ApplicationModel.Package.Current.Id.FullName+"\nLocalState: "+root+"\nGame data: "+content+"\n");
   try{
    var bytes=await VerifyWindowsFiles(content,plan.Files,token);
    if(bytes!=plan.Validate())throw new IOException("Windows storage inventory differs from the hash-verified game data.");
    File.AppendAllText(Path.Combine(root,"content-location.txt"),"WinRT independently opened "+plan.Files.Count+" files; total file bytes="+bytes+". SHA-256 was checked separately by the transfer verifier.\n");
   }catch{
    var marker=Path.Combine(root,"content-ready.txt");if(File.Exists(marker))File.Delete(marker);
    File.AppendAllText(Path.Combine(root,"content-data-audit.txt"),"CONTENT_DATA_PENDING total="+plan.Files.Count+"\n");
    throw;
   }
  }
  static async Task<System.Collections.Generic.HashSet<string>> ExistingWindowsFiles(string root,ContentPlan plan,CancellationToken token){
   var present=new System.Collections.Generic.HashSet<string>(StringComparer.Ordinal);long bytes=0;
   foreach(var record in plan.Files){
    token.ThrowIfCancellationRequested();
    try{var file=await StorageFile.GetFileFromPathAsync(Path.GetFullPath(Path.Combine(root,record.Path.Replace('/',Path.DirectorySeparatorChar))));var info=await file.GetBasicPropertiesAsync();if(info.Size==(ulong)record.Bytes){present.Add(record.Path);bytes=checked(bytes+record.Bytes);}}
    catch(FileNotFoundException){}catch(DirectoryNotFoundException){}
   }
   File.AppendAllText(Path.Combine(ApplicationData.Current.LocalFolder.Path,"content-location.txt"),"Before download: WinRT found "+present.Count+" / "+plan.Files.Count+" correctly sized existing files; bytes="+bytes+". This is metadata only; reuse also requires SHA-256.\n");return present;
  }
  sealed class Capture:IProgress<ContentProgress>{
   readonly IProgress<ContentProgress> target;public ContentProgress Last;
   public Capture(IProgress<ContentProgress> output){target=output;}
   public void Report(ContentProgress state){Last=state;var display=state.Copy();if(display.Complete){display.Complete=false;display.Phase="Checking Windows storage";}target?.Report(display);}
  }
  public static async Task<ContentProgress> Run(ApiClient api,string projectToken,IProgress<string> progress,CancellationToken token,IProgress<ContentProgress> details=null,IProgress<string> codeProgress=null){
   await api.SendAsync("/api/device/profile",null,projectToken,token);
   string root=ApplicationData.Current.LocalFolder.Path,installed=Windows.ApplicationModel.Package.Current.InstalledLocation.Path;long before=await FreeBytes();
   File.WriteAllText(Path.Combine(root,"content-location.txt"),"Package: "+Windows.ApplicationModel.Package.Current.Id.FullName+"\nLocalState: "+root+"\nGame data: "+Path.Combine(root,"GameClean")+"\n");
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
     File.WriteAllText(Path.Combine(root,"content-data-audit.txt"),"CONTENT_INVENTORY bytes="+bytes+" total="+plan.Files.Count+" registered="+plan.Files.Count+" missing=0\nCONTENT_TRANSFER bytes=0 total=0\nCONTENT_DATA_READY bytes="+bytes+" total="+plan.Validate()+"\n");await ConfirmWindowsStorage(installed,plan,root,token);details?.Report(state);return state;
    }
    string leaseUrl=null;long leaseExpires=0;
    Func<Task<string>> contentSource=async()=>{
     long now=DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
     if(leaseUrl!=null&&now<leaseExpires-120000)return leaseUrl;
     var lease=await api.SendAsync("/api/device/content-lease",new JsonObject(),projectToken,token);
     var candidate=lease.GetNamedString("archiveUrl");long expires=(long)lease.GetNamedNumber("expiresAt");Uri uri;
     if(lease.GetNamedString("build")!=plan.Build||lease.GetNamedString("archiveSha256")!=plan.ArchiveSha256||lease.GetNamedNumber("archiveBytes")!=plan.ArchiveBytes||!lease.GetNamedBoolean("private")||expires<now+60000||expires>now+16*60000||!Uri.TryCreate(candidate,UriKind.Absolute,out uri)||uri.Scheme!="https"||uri.Host!="ricswvi50gagbcuh.private.blob.vercel-storage.com"||!uri.IsDefaultPort||uri.UserInfo.Length!=0||uri.Fragment.Length!=0||uri.AbsolutePath!="/client621/"+plan.ArchiveSha256+"/6.21.zip"||uri.Query.Length>8192||!uri.Query.Contains("vercel-blob-signature=")||!uri.Query.Contains("vercel-blob-delegation="))throw new IOException("Private content lease differs from the pinned Hollowwake release.");
     leaseUrl=candidate;leaseExpires=expires;return leaseUrl;
    };
    progress.Report("Checking Windows storage for missing game files.");
    var existing=await ExistingWindowsFiles(Path.Combine(root,"GameClean"),plan,token);
    progress.Report("Downloading missing game files from Hollowwake; final verification runs after download.");
    Func<long,Task> space=async required=>{if(await FreeBytes()<required)throw new IOException("Insufficient Xbox app storage. Required free bytes: "+required);};
    await Task.Run(()=>ContentTransfer.EnsureIndexed(plan,root,authorize,space,progress,token,capture,contentSource,existing));
    await api.SendAsync("/api/device/profile",null,projectToken,token);
    if(capture.Last==null||!capture.Last.Complete||capture.Last.VerifiedFiles!=139||capture.Last.VerifiedBytes!=26449827840L||capture.Last.OnDiskBytes!=26449827840L)throw new IOException("Final data audit is incomplete.");
    await ConfirmWindowsStorage(Path.Combine(root,"GameClean"),plan,root,token);
    details?.Report(capture.Last);return capture.Last;
   }finally{
    try{long after=await FreeBytes();File.AppendAllText(Path.Combine(root,"content-space-audit.txt"),"CONTENT_SPACE_AFTER bytes="+after+"\n");}catch{}
   }
  }
 }
}
