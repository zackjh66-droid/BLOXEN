using System;using System.IO;using System.Text;using System.Collections.Generic;using System.Linq;using System.Security.Cryptography;using System.Threading;using System.Threading.Tasks;using Windows.Data.Json;using Windows.Storage;
namespace Hollowwake.Services {
 public static class ClientDiagnostics {
  static readonly SemaphoreSlim sending=new SemaphoreSlim(1,1);
  static string Root{get{return ApplicationData.Current.LocalFolder.Path;}}
  static string Pending{get{return Path.Combine(Root,"PendingDiagnostics");}}
  public static string Version(){var v=Windows.ApplicationModel.Package.Current.Id.Version;return v.Major+"."+v.Minor+"."+v.Build+"."+v.Revision;}
  public static void BeginAttempt(){try{ApplicationData.Current.LocalSettings.Values["DiagnosticAttempt"]=Guid.NewGuid().ToString("N");var native=Path.Combine(Root,"native-launch.log");ApplicationData.Current.LocalSettings.Values["DiagnosticOffset"]=File.Exists(native)?new FileInfo(native).Length:0L;var managed=Path.Combine(Root,"managed-launch.log");ApplicationData.Current.LocalSettings.Values["DiagnosticManagedOffset"]=File.Exists(managed)?new FileInfo(managed).Length:0L;}catch{}}
  public static void MarkEntryPending(){try{File.WriteAllText(Path.Combine(Root,"client-entry.pending"),"A native launch was requested; completion has not been recorded.");}catch{}}
  public static void CompleteAttempt(){try{var path=Path.Combine(Root,"client-entry.pending");if(File.Exists(path))File.Delete(path);}catch{}}
  static string Read(string name,long offset=0){try{var path=Path.Combine(Root,name);if(!File.Exists(path))return "";using(var file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite)){if(offset<0||offset>file.Length)offset=0;file.Position=Math.Max(offset,file.Length-1048576);using(var reader=new StreamReader(file,Encoding.UTF8))return reader.ReadToEnd();}}catch{return "";}}
  static IJsonValue Value(object value){
   if(value is string)return JsonValue.CreateStringValue((string)value);if(value is bool)return JsonValue.CreateBooleanValue((bool)value);if(value is int)return JsonValue.CreateNumberValue((int)value);
   var dictionary=value as Dictionary<string,object>;if(dictionary!=null){var d=new JsonObject();foreach(var e in dictionary)d[e.Key]=Value(e.Value);return d;}
   var list=value as IEnumerable<Dictionary<string,object>>;if(list!=null){var a=new JsonArray();foreach(var e in list)a.Add(Value(e));return a;}throw new InvalidOperationException("Unsupported diagnostic field");
  }
  public static JsonObject Snapshot(string trigger){
   var id=ApplicationData.Current.LocalSettings.Values["DiagnosticAttempt"] as string;if(String.IsNullOrEmpty(id)||id.Length!=32){id=Guid.NewGuid().ToString("N");ApplicationData.Current.LocalSettings.Values["DiagnosticAttempt"]=id;}
   long offset=0;try{offset=Convert.ToInt64(ApplicationData.Current.LocalSettings.Values["DiagnosticOffset"]??0L);}catch{}
   long managedOffset=0;try{managedOffset=Convert.ToInt64(ApplicationData.Current.LocalSettings.Values["DiagnosticManagedOffset"]??0L);}catch{}
   var native=Read("native-launch.log",offset);int omitted;var events=DiagnosticSanitizer.Events(native+"\n"+Read("managed-launch.log",managedOffset)+"\nNATIVE_STARTUP "+Read("runtime-policy.txt")+ "\n" + Read("content-data-audit.txt") + "\n" + Read("content-code-audit.txt") + "\n" + Read("content-space-audit.txt"),out omitted);
   var missing=new List<Dictionary<string,object>>();var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);int redirected=0,platformMissing=0,platformTotal=0;
   foreach(var row in Read("native-imports-unresolved.txt").Split(new[]{'\n'},StringSplitOptions.RemoveEmptyEntries)){var n=row.Trim();if(DiagnosticSanitizer.Binding(n)&&names.Add("redirected:"+n)){missing.Add(new Dictionary<string,object>{{"name",n},{"source","redirected"},{"error",127},{"normal",true},{"main",false}});redirected++;}}
   JsonObject platform;if(JsonObject.TryParse(Read("native-platform-imports.json"),out platform))try{
    var pairs=platform.GetNamedArray("pairs");platformTotal=pairs.Count;
    foreach(var value in pairs){var pair=value.GetObject();if(pair.GetNamedBoolean("nativeCapabilityUnavailable",false)){var capability=pair.GetNamedString("name","");if(DiagnosticSanitizer.Binding(capability))events.Add(new Dictionary<string,object>{{"kind","NATIVE_CAPABILITY_UNAVAILABLE"},{"name",capability},{"values",new Dictionary<string,object>{{"Win32",((int)pair.GetNamedNumber("capabilityError",127)).ToString()}}}});}if(pair.GetNamedBoolean("resolved"))continue;var n=pair.GetNamedString("name");if(!DiagnosticSanitizer.Binding(n)||!names.Add("platform:"+n))continue;
     int code=(int)pair.GetNamedNumber("error",127);if(code<0||code>1000000)code=127;missing.Add(new Dictionary<string,object>{{"name",n},{"source","platform"},{"error",code},{"normal",pair.GetNamedBoolean("normalImport",false)},{"main",pair.GetNamedBoolean("mainImage",false)}});platformMissing++;
    }
   }catch{}
   JsonObject provider;if(JsonObject.TryParse(Read("native-provider-imports.json"),out provider))try{foreach(var value in provider.GetNamedArray("pairs")){var pair=value.GetObject();if(pair.GetNamedBoolean("resolved"))continue;var n=pair.GetNamedString("name");if(!DiagnosticSanitizer.Binding(n)||!names.Add("licensed-provider:"+n))continue;int code=(int)pair.GetNamedNumber("error",127);if(code<0||code>1000000)code=127;missing.Add(new Dictionary<string,object>{{"name",n},{"source","licensed-provider"},{"error",code},{"normal",pair.GetNamedBoolean("normalImport",false)},{"main",false}});}}catch{}
   if(events.Count>256){int extra=events.Count-256;events.RemoveRange(0,extra);omitted+=extra;}
   if(missing.Count>2048)throw new IOException("Diagnostic binding inventory exceeds fixed limit");
   var report=new JsonObject{{"schema",JsonValue.CreateNumberValue(1)},{"build",JsonValue.CreateStringValue("6.21-CL-4526925")},{"packageVersion",JsonValue.CreateStringValue(Version())},{"attemptId",JsonValue.CreateStringValue(id)},{"trigger",JsonValue.CreateStringValue(trigger)},{"runtime",JsonValue.CreateStringValue("native-packaged")},{"entryReported",JsonValue.CreateBooleanValue(native.Contains("NATIVE_ENTRY calling"))},{"previousAttemptIncomplete",JsonValue.CreateBooleanValue(File.Exists(Path.Combine(Root,"client-entry.pending"))&&trigger=="next_start")},{"events",Value(events)},{"missing",Value(missing)},{"counts",Value(new Dictionary<string,object>{{"platformTotal",platformTotal},{"platformMissing",platformMissing},{"redirectedMissing",redirected},{"eventsOmitted",omitted}})}};
   var lines=new List<string>{"Unresolved bindings only: "+missing.Count};foreach(var e in missing)lines.Add((string)e["name"]+" ["+e["source"]+"; error="+e["error"]+"]");if(missing.Count==0)lines.Add("No unresolved binding records in the saved audit. This does not prove Fortnite ran.");File.WriteAllLines(Path.Combine(Root,"client-missing-imports.txt"),lines);return report;
  }
  public static async Task<string> SubmitAsync(ApiClient api,string token,string trigger){
   if(api==null||String.IsNullOrEmpty(token)||!api.Origin.StartsWith("https://",StringComparison.OrdinalIgnoreCase))return null;
   try{
    var report=Snapshot(trigger);var text=report.Stringify();if(Encoding.UTF8.GetByteCount(text)>262144)return null;string hash;using(var sha=SHA256.Create())hash=BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(text))).Replace("-","").ToLowerInvariant();
    if((ApplicationData.Current.LocalSettings.Values["DiagnosticLastHash"] as string)==api.Origin+"|"+hash)return null;
    Directory.CreateDirectory(Pending);var destination=Path.Combine(Pending,hash+".json");if(!File.Exists(destination)){var temporary=destination+"."+Guid.NewGuid().ToString("N")+".tmp";File.WriteAllText(temporary,text);try{File.Move(temporary,destination);}catch(IOException){if(File.Exists(temporary))File.Delete(temporary);}}
    if(trigger=="next_start")CompleteAttempt();return await FlushAsync(api,token);
   }catch{return null;}
  }
  public static async Task<string> FlushAsync(ApiClient api,string token){
   if(api==null||String.IsNullOrEmpty(token)||!Directory.Exists(Pending)||!await sending.WaitAsync(0))return null;
   try{
    string latest=null;using(var timeout=new CancellationTokenSource(TimeSpan.FromSeconds(4))){
     foreach(var path in Directory.GetFiles(Pending,"*.json").OrderBy(File.GetLastWriteTimeUtc).Take(5)){
      if(new FileInfo(path).Length>262144)continue;JsonObject payload;if(!JsonObject.TryParse(File.ReadAllText(path),out payload))continue;
      var response=await api.SendAsync("/api/device/diagnostics",payload,token,timeout.Token);Guid reportId;if(!response.GetNamedBoolean("ok",false)||!Guid.TryParse(response.GetNamedString("reportId",""),out reportId))break;
      latest=reportId.ToString();File.WriteAllText(Path.Combine(Root,"diagnostic-upload-status.txt"),"Uploaded failure report "+latest+". Reports require sign-in; this ID is not an access credential. Native execution is unverified.");ApplicationData.Current.LocalSettings.Values["DiagnosticLastHash"]=api.Origin+"|"+Path.GetFileNameWithoutExtension(path);File.Delete(path);
     }
    }return latest;
   }catch{return null;}finally{sending.Release();}
  }
  public static string MissingText(){try{Snapshot("manual");return Read("client-missing-imports.txt");}catch{return "Could not parse saved unresolved-import records. Raw logs remain in LocalState.";}}
 }
}
