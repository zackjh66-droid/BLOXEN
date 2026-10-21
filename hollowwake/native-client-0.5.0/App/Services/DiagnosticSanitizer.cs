// Source-owned structured diagnostics: no arbitrary log text, URLs, credentials,
// command lines, file contents or environment values enter a report.
using System;using System.Collections.Generic;using System.Text.RegularExpressions;
namespace Hollowwake.Services {
 public static class DiagnosticSanitizer {
  public static readonly HashSet<string> Kinds=new HashSet<string>(("ATTEMPT_BEGIN NATIVE_PREFLIGHT NATIVE_PLATFORM_PREFLIGHT NATIVE_PROVIDER_PREFLIGHT NATIVE_CAPABILITY_UNAVAILABLE NATIVE_PLATFORM_LOAD_TIME_REQUIRED NATIVE_PROXY_BLOCKED NATIVE_LOAD_BLOCKED NATIVE_LOAD_FAILED NATIVE_LOAD_EXCEPTION NATIVE_STARTUP_FAILED NATIVE_STARTUP_BOUND NATIVE_THREAD_FAILED NATIVE_ENTRY NATIVE_CRASH NATIVE_LOAD NATIVE_STARTUP NATIVE_ROUTE DEPENDENCY IMPORT UNWIND PROTECT HASH MAP RELOC STOP CALLED_UNSUPPORTED CRASH EXECUTE CLIENT_NOT_READY USER_REQUESTED CONTENT_READY CONTENT_FAILED CONTENT_INVENTORY CONTENT_TRANSFER CONTENT_DATA_READY CONTENT_DATA_PENDING CONTENT_CODE CONTENT_SPACE_BEFORE CONTENT_SPACE_AFTER OTHER_NATIVE_FAILURE").Split(' '));
  static readonly Regex binding=new Regex(@"^[A-Za-z0-9_.-]{1,96}![A-Za-z0-9_?@$#.:~+<>\-]{1,220}$");
  static readonly Regex searchBinding=new Regex(@"[A-Za-z0-9_.-]{1,96}![A-Za-z0-9_?@$#.:~+<>\-]{1,220}");
  static readonly Regex numbers=new Regex(@"\b(code|error|HRESULT|Win32|RVA|address|target|bytes|unresolved|total|registered|missing|windowBind|controllerCount)=(-?(?:0x)?[0-9a-fA-F]{1,20})(?=\s|;|,|$)");
  // This is a call-attempt marker, not proof that guest instructions executed.
  public static bool EntryInvocationReported(string text){return Regex.IsMatch(text??"",@"(?m)^NATIVE_ENTRY (?:thread=[0-9]+ )?calling original entry(?:;|$)");}
  public static bool Binding(string text){return text!=null&&binding.IsMatch(text);}
  public static List<Dictionary<string,object>> Events(string text,out int omitted){
   var result=new List<Dictionary<string,object>>();
   foreach(var line in (text??"").Split(new[]{'\n'},StringSplitOptions.RemoveEmptyEntries)){
    string kind=null;foreach(Match word in Regex.Matches(line,@"\b[A-Z][A-Z0-9_]{1,60}\b"))if(Kinds.Contains(word.Value)){kind=word.Value;break;}if(kind==null)continue;
    var values=new Dictionary<string,object>();foreach(Match m in numbers.Matches(line))values[m.Groups[1].Value]=m.Groups[2].Value;
    if(kind=="NATIVE_CRASH"){
     var access=Regex.Match(line,@"\baccess=(read|write|execute)(?=\s|;|,|$)");if(access.Success)values["accessType"]=access.Groups[1].Value=="read"?"0":access.Groups[1].Value=="write"?"1":"8";
    }else values.Remove("target");
    var entry=new Dictionary<string,object>{{"kind",kind},{"values",values}};
    var name=searchBinding.Match(line);if(name.Success&&Binding(name.Value))entry["name"]=name.Value;
    var caller=Regex.Match(line,@"\bcaller=([A-Za-z0-9_.-]{1,96})(?=\s|;|,|$)");if(caller.Success)entry["caller"]=caller.Groups[1].Value;
    result.Add(entry);
   }
   omitted=Math.Max(0,result.Count-256);if(omitted>0)result.RemoveRange(0,omitted);return result;
  }
 }
}
