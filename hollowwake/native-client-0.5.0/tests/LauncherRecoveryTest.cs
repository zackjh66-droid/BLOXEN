using System;using System.IO;using Hollowwake.Services;
public static class LauncherRecoveryTest {
 static void Check(bool value,string message){if(!value)throw new Exception(message);}
 public static void Main(){var root=Path.Combine(Path.GetTempPath(),"HollowwakeRecovery-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);try{
  var r=new LauncherRecovery(root);Check(!r.BeginBoot()&&!r.PreviousBootWasInterrupted,"Clean install/current boot was misreported as an interrupted prior boot");r.CompleteBoot();
  var normal=new LauncherRecovery(root);Check(!normal.BeginBoot()&&!normal.PreviousBootWasInterrupted,"Completed boot was treated as a crash");
  var interrupted=new LauncherRecovery(root);Check(interrupted.BeginBoot()&&interrupted.PreviousBootWasInterrupted,"Actual prior interrupted boot was not distinguished from the current start");interrupted.CompleteBoot();interrupted.EndVerification(true);r.BeginVerification();
  Check(new LauncherRecovery(root).BeginBoot(),"Interrupted verification did not enter recovery");r.CompleteBoot();Check(File.Exists(Path.Combine(root,"content-verification.pending")),"Recovery silently cleared incomplete verification");
  r.EndVerification(true);Check(!new LauncherRecovery(root).BeginBoot(),"Explicit successful verification did not release recovery");r.CompleteBoot();
  r.BeginNativeStartup();Check(new LauncherRecovery(root).BeginBoot(),"Interrupted binding did not enter recovery");r.EndNativeStartup();r.CompleteBoot();r.EndVerification(true);
  File.WriteAllText(Path.Combine(root,"client-entry.pending"),"Original requested entry did not return");Check(new LauncherRecovery(root).BeginBoot(),"Prior guest crash did not enter recovery");Check(File.Exists(Path.Combine(root,"client-entry.pending")),"Recovery destroyed existing guest crash evidence");r.CompleteBoot();
  File.WriteAllText(Path.Combine(root,"owned-game-data.bin"),"Preserved data");File.Delete(Path.Combine(root,"launcher-recovery-v1.initialized"));File.Delete(Path.Combine(root,"client-entry.pending"));File.WriteAllText(Path.Combine(root,"content-data-audit.txt"),"Previous installation");Check(new LauncherRecovery(root).BeginBoot(),"Existing installation migration did not pause automatic verification");Check(File.ReadAllText(Path.Combine(root,"owned-game-data.bin"))=="Preserved data","Recovery modified existing files");
  Console.WriteLine("PASS durable interrupted-boot/verification/native-binding/guest-entry recovery, upgrade migration, explicit successful retry, existing data/evidence preserved. Owned files only; no game or Xbox execution.");
 }finally{Directory.Delete(root,true);}}
}
