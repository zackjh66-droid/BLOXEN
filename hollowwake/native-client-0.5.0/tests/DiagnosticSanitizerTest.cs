using System;using System.Linq;using Hollowwake.Services;
class DiagnosticSanitizerTest {
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
  events=DiagnosticSanitizer.Events(String.Join("\n",Enumerable.Repeat("NATIVE_CRASH code=621",300)),out omitted);Check(events.Count==256&&omitted==44,"Diagnostic event bound not accounted for");
  Console.WriteLine("PASS actual diagnostic field extraction: exact mangled/ordinal imports, native error values, secret/URL/raw-text exclusion, bounded event count. No account secrets or game/Xbox execution.");
 }
}
