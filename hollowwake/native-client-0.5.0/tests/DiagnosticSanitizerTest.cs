using System;using System.Linq;using Hollowwake.Services;
class DiagnosticSanitizerTest {
 sealed class MissingMetadataFailure:Exception {public override string StackTrace {get{throw new NotSupportedException();}}}
 sealed class PrivateFailure:Exception {
  public PrivateFailure(string message,Exception inner):base(message,inner){}
  public override string StackTrace {get{return "   at Hollowwake.MainPage.ContentRetry_Click(password=do-not-send-password-or-access-token) in C:\\private\\secret.cs:line 12\n   at https://secret.invalid/?access_token=do-not-send-password-or-access-token\n";}}
 }
 static void Check(bool ok,string reason){if(!ok)throw new Exception(reason);}
 static void Main(){
  Check(DiagnosticSanitizer.EntryInvocationReported("NATIVE_ENTRY thread=123 calling original entry; lobby not confirmed")&&!DiagnosticSanitizer.EntryInvocationReported("NATIVE_ENTRY_RETURN value=621")&&!DiagnosticSanitizer.EntryInvocationReported("No NATIVE_ENTRY calling original entry"),"Entry invocation marker recognition differs from actual production log");
  const string secret="do-not-send-password-or-access-token";int omitted;
  var events=DiagnosticSanitizer.Events("NATIVE_PROXY_BLOCKED KERNEL32.dll!__chkstk\nNATIVE_CRASH code=3221225477 address=123456 access=read target=18446744073709551615; exception continues to OS\nNATIVE_LOAD_EXCEPTION HRESULT=-2147467259 password="+secret+"\nNATIVE_STARTUP commandLine=https://example.invalid/?access_token="+secret+"\nCALLed USER personal file\n",out omitted);
  Check(events.Count==4&&omitted==0,"Expected structured native events");
  Check((string)events[0]["name"]=="KERNEL32.dll!__chkstk","Exact import name missing");
  var fault=(System.Collections.Generic.Dictionary<string,object>)events[1]["values"];Check((string)fault["accessType"]=="0"&&(string)fault["target"]=="18446744073709551615","Crash access/target must remain exact strings, without 64-bit precision loss");
  var flattened=String.Join(" ",events.SelectMany(e=>e.Values).Select(v=>v is System.Collections.Generic.Dictionary<string,object>?String.Join(" ",((System.Collections.Generic.Dictionary<string,object>)v).Values):v.ToString()));Check(!flattened.Contains(secret)&&!flattened.Contains("https://"),"Arbitrary log content escaped sanitization");
  Check(DiagnosticSanitizer.Binding("MSVCRT.dll!??2@YAPEAX_K@Z")&&!DiagnosticSanitizer.Binding("https://secret")&&!DiagnosticSanitizer.Binding("DLL!symbol\npassword=x"),"Binding validation failed");
  events=DiagnosticSanitizer.Events("NATIVE_CRASH_STACK_CANDIDATE stack_index=0 caller=Fortnite621.imports.private.dll RVA=621 raw=secret",out omitted);Check(events.Count==1&&(string)events[0]["caller"]=="Fortnite621.imports.private.dll"&&(string)((System.Collections.Generic.Dictionary<string,object>)events[0]["values"])["stack_index"]=="0", "Bounded crash candidate fields were lost");
  events=DiagnosticSanitizer.Events(String.Join("\n",Enumerable.Repeat("NATIVE_CRASH code=621",300)),out omitted);Check(events.Count==256&&omitted==44,"Diagnostic event bound not accounted for");
  var detail=ManagedFailure.Describe(new PrivateFailure("Layout cycle detected: "+secret,new System.IO.IOException("https://secret.invalid/?token="+secret)),"framework "+secret);
  Check(detail.Contains("category=layout-cycle")&&detail.Contains("Hollowwake.MainPage.ContentRetry_Click")&&detail.Contains("System.IO.IOException")&&!detail.Contains(secret)&&!detail.Contains("https:")&&!detail.Contains("C:")&&!detail.Contains("password="),"Managed failure details exported message, arguments, URL or source path");
  Check(ManagedFailure.Describe(new Exception("unrecognized "+secret),"The application called an interface that was marshalled for a different thread").Contains("category=wrong-thread"),"Framework-only failure category was lost");
  Check(ManagedFailure.Describe(new MissingMetadataFailure())=="category=metadata-unavailable","Unavailable AOT stack metadata broke basic failure recording");
  Console.WriteLine("PASS actual diagnostic field extraction: exact mangled/ordinal imports, native error values, secret/URL/raw-text exclusion, bounded event count. No account secrets or game/Xbox execution.");
 }
}
