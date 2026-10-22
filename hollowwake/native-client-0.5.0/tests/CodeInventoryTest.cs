using System;using System.Collections.Generic;using System.IO;using Hollowwake.Services;
public class CodeInventoryTest {
 static void Reject(Action call){try{call();}catch(IOException){return;}throw new Exception("Invalid code inventory accepted");}
 public static void Main(){
  var images=new List<string>();for(int i=0;i<28;++i)images.Add("6.21/image"+i+".dll");
  var proxies=new List<string>();for(int i=0;i<18;++i)proxies.Add("HWproxy"+i+".dll");
  var actual=new List<string>(images);actual.AddRange(proxies);actual.AddRange(new[]{"HollowwakeClientRuntime.dll","HWAUD.dll","dsoal-aldrv.dll","HWMGL.dll","libgallium_wgl.dll","libglapi.dll","DXIL.dll"});
  if(CodeInventory.Validate(images,proxies,actual,"audio-mesa")!=53)throw new Exception("Expanded dependency set rejected");
  var missing=new List<string>(actual);missing.RemoveAt(0);Reject(()=>CodeInventory.Validate(images,proxies,missing,"audio-mesa"));
  var substitute=new List<string>(actual);substitute[0]="other.dll";Reject(()=>CodeInventory.Validate(images,proxies,substitute,"audio-mesa"));
  var duplicate=new List<string>(actual);duplicate.Add(actual[0].ToUpperInvariant());Reject(()=>CodeInventory.Validate(images,proxies,duplicate,"audio-mesa"));
  var unsafeImages=new List<string>(images);unsafeImages[0]="../image.dll";Reject(()=>CodeInventory.Validate(unsafeImages,proxies,actual,"audio-mesa"));
  Reject(()=>CodeInventory.Validate(images,proxies,actual,"unknown"));Reject(()=>CodeInventory.Validate(images,proxies,actual,"audio"));
  actual.RemoveRange(actual.Count-4,4);if(CodeInventory.Validate(images,proxies,actual,"audio")!=49)throw new Exception("Audio-only exact set rejected");
  Console.WriteLine("PASS exact signed dependency/provider path set, expanded 53 files, missing/substitution/case-duplicate/traversal/variant rejection. File hashes verified separately by production verifier.");
 }
}
