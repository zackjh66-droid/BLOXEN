using System;using System.IO;
namespace Hollowwake.Services {
 // Durable operation markers contain no account tokens, URLs or game data.
 public sealed class LauncherRecovery {
  readonly string root;
  public bool PreviousBootWasInterrupted{get;private set;}
  string PathFor(string name){return Path.Combine(root,name);}
  void Write(string name,string text){Directory.CreateDirectory(root);File.WriteAllText(PathFor(name),text);}
  void Delete(string name){try{var path=PathFor(name);if(File.Exists(path))File.Delete(path);}catch(IOException){}catch(UnauthorizedAccessException){}}
  public LauncherRecovery(string directory){root=directory;}
  public bool BeginBoot(){try{return BeginBootCore();}catch(IOException){return true;}catch(UnauthorizedAccessException){return true;}}
  bool BeginBootCore(){
   PreviousBootWasInterrupted=File.Exists(PathFor("launcher-boot.pending"));
   bool migrated=File.Exists(PathFor("launcher-recovery-v1.initialized"));
   if(!migrated){if(File.Exists(PathFor("content-data-audit.txt")))Write("launcher-recovery.pending","Existing installation: verification is paused for this first recovery-capable start.");Write("launcher-recovery-v1.initialized","Recovery markers initialized.");}
   bool recover=File.Exists(PathFor("launcher-boot.pending"))||File.Exists(PathFor("content-verification.pending"))||File.Exists(PathFor("native-startup.pending"))||File.Exists(PathFor("client-entry.pending"))||File.Exists(PathFor("launcher-recovery.pending"));
   if(recover)Write("launcher-recovery.pending","Previous operation did not complete; automatic verification is paused. Existing files are retained.");
   Write("launcher-boot.pending","Launcher initialization in progress.");return recover;
  }
  public void CompleteBoot(){Delete("launcher-boot.pending");}
  public void BeginVerification(){Write("content-verification.pending","Content verification/download in progress.");}
  public void EndVerification(bool completed){Delete("content-verification.pending");if(completed)Delete("launcher-recovery.pending");}
  public void BeginNativeStartup(){Write("native-startup.pending","Native binding explicitly requested from Launch.");}
  public void EndNativeStartup(){Delete("native-startup.pending");}
 }
}
