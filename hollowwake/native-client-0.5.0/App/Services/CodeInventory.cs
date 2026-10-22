using System;
using System.Collections.Generic;
using System.IO;
namespace Hollowwake.Services {
 // Both inputs come exclusively from the immutable signed installed package.
 public static class CodeInventory {
  static void Add(HashSet<string> paths,string path){
   if(String.IsNullOrEmpty(path)||path.Contains("\\")||path.Contains(":")||!path.EndsWith(".dll",StringComparison.OrdinalIgnoreCase))throw new IOException("Unsafe signed code path.");
   foreach(var part in path.Split('/'))if(part.Length==0||part=="."||part==".."||part.EndsWith(".")||part.EndsWith(" "))throw new IOException("Unsafe signed code path.");
   if(!paths.Add(path))throw new IOException("Duplicate signed code path.");
  }
  public static int Validate(IEnumerable<string> images,IEnumerable<string> proxies,IEnumerable<string> actual,string variant){
   if(variant!="audio"&&variant!="audio-mesa")throw new IOException("Unknown signed code provider variant.");
   var expected=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
   int count=0;foreach(var path in images){Add(expected,path);++count;}if(count==0)throw new IOException("No prepared client images.");
   count=0;foreach(var path in proxies){Add(expected,path);++count;}if(count==0)throw new IOException("No prepared import providers.");
   foreach(var path in new[]{"HollowwakeClientRuntime.dll","HWAUD.dll","dsoal-aldrv.dll"})Add(expected,path);
   if(variant=="audio-mesa")foreach(var path in new[]{"HWMGL.dll","libgallium_wgl.dll","libglapi.dll","DXIL.dll"})Add(expected,path);
   var supplied=new HashSet<string>(StringComparer.OrdinalIgnoreCase);foreach(var path in actual)Add(supplied,path);
   if(!expected.SetEquals(supplied))throw new IOException("Installed code index differs from signed prepared dependencies/providers.");
   return expected.Count;
  }
 }
}
