using System;using System.Text;using System.Text.RegularExpressions;
namespace Hollowwake.Services {
 // Export categories and static frame names, never exception messages,
 // arguments, URLs, account tokens or source paths. Do not handle the exception.
 public static class ManagedFailure {
  static string Category(string message){
   var bounded=message??"";if(bounded.Length>16384)bounded=bounded.Substring(0,16384);var text=bounded.ToLowerInvariant();
   if(text.Contains("layout")&&text.Contains("cycle"))return "layout-cycle";
   if(text.Contains("different thread")||text.Contains("wrong thread"))return "wrong-thread";
   if(text.Contains("access")&&text.Contains("denied"))return "access-denied";
   if(text.Contains("closed")||text.Contains("disposed"))return "closed-object";
   if(text.Contains("sharing violation"))return "sharing-violation";
   if(text.Contains("memory")&&(text.Contains("enough")||text.Contains("out of")))return "out-of-memory";
   if(text.Contains("unspecified"))return "unspecified";
   return "unclassified";
  }
  public static string Describe(Exception error,string frameworkMessage=null){
   try{return DescribeCore(error,frameworkMessage);}catch{return "category=metadata-unavailable";}
  }
  static string DescribeCore(Exception error,string frameworkMessage){
   if(error==null)return "category=unavailable";
   string category=Category(error.Message);if(category=="unclassified")category=Category(frameworkMessage);
   var output=new StringBuilder("category="+category);int depth=0;
   for(var current=error;current!=null&&depth<4;current=current.InnerException,depth++){
    var type=Regex.Replace(current.GetType().FullName??"Exception","[^A-Za-z0-9_.$+<>`]","");if(type.Length>160)type=type.Substring(0,160);
    output.Append(" exception").Append(depth).Append('=').Append(type).Append(':').Append(current.HResult.ToString("X8"));
    string stack=current.StackTrace??"";if(stack.Length>16384)stack=stack.Substring(0,16384);int count=0;
    foreach(var line in stack.Split('\n')){
     if(count>=8)break;
     var frame=Regex.Match(line,@"^\s*at\s+((?:Hollowwake|System|Windows|Microsoft)\.[A-Za-z0-9_.$+<>`]+)(?:\(|\s|$)");
     if(!frame.Success)continue;string method=frame.Groups[1].Value;if(method.Length>200)method=method.Substring(0,200);
     output.Append(" frame").Append(depth).Append('_').Append(count++).Append('=').Append(method);
    }
   }
   return output.ToString();
  }
 }
}
