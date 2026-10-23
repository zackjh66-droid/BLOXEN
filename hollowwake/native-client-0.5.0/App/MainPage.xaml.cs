using System;
using System.Net;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using Hollowwake.Services;
using Windows.Data.Json;
using Windows.Storage;
using Windows.Storage.Streams;
using Windows.System;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media.Imaging;

namespace Hollowwake
{
    public sealed partial class MainPage : Page
    {
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)] private static extern int HWBindWindow(string path);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)] private static extern int HWMemoryPolicy(string path);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)] private static extern int HWAttemptPackagedClient();
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)] private static extern int HWConfigureClientLogin(string exchangeCode,uint expiresIn);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int HWPreflightClient();
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int HWStockCursorContract();
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)] private static extern int HWNativeWindowProbe(string reportPath);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)] private static extern int HWMultimediaTimerProbe(string reportPath);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int HWNetworkAvailabilityProbe(out uint flags,out uint error);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)] private static extern int HWAudioProviderProbe(string reportPath);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int HWFactoryBridgePolicy();
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)] private static extern bool HWConfigureNetwork(string hostname,ushort port);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int HWClockContract();
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int HWSystemInfoContract();
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int HWAllocatorContract();
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern void HWKey(uint key,int down);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern void HWChar(uint key);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern void HWViewport(int width,int height);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern void HWHostActivation(int active);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int HWWindowContract();
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int HWControllerCount();
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)] private static extern int HWWglProviderProbe(string report);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern void HWSetControllerFocus(int active);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int HWGraphicsContract();
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern void HWUnsupportedContract();
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int HWTlsContract();
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern void HWPointer(int x,int y,uint buttons,int wheel);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern void HWMouseDelta(int dx,int dy);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern void HWKeyDetails(uint key,uint scan,int down,int repeat,int extended,int menu,int wasDown);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern void HWCharacterDetails(uint code,uint scan);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern void HWTouch(uint id,int x100,int y100,uint flags,uint milliseconds);
        private bool importBusy,loadBusy,policyReady,contentReady,runtimeAudited;
        private CancellationTokenSource contentOperation;
        private string LaunchLog {get{return Path.Combine(ApplicationData.Current.LocalFolder.Path,"native-launch.log");}}
        private readonly SessionStore store = new SessionStore();
        private ApiClient api;
        private CancellationTokenSource operation = new CancellationTokenSource();
        private string sessionToken, pendingDeviceCode;
        private int generation;
        private bool allowLocalHttp, ready, startupBusy;
        private readonly DispatcherTimer heartbeat = new DispatcherTimer { Interval = TimeSpan.FromMinutes(1) };
        private bool heartbeatBusy;
        private readonly LauncherRecovery recovery;
        private bool recoveryMode;
        public MainPage()
        {
            App.StartupStage("page-construction");
            InitializeComponent();
            App.StartupStage("xaml-created");
            recovery=new LauncherRecovery(ApplicationData.Current.LocalFolder.Path);recoveryMode=recovery.BeginBoot();App.PreviousLauncherBootInterrupted=recovery.PreviousBootWasInterrupted;
            var core=CoreWindow.GetForCurrentThread();core.PointerCursor=null;core.SizeChanged+=(s,e)=>{if(policyReady)HWViewport((int)e.Size.Width,(int)e.Size.Height);};core.Activated+=(s,e)=>{if(!policyReady)return;HWHostActivation(e.WindowActivationState!=CoreWindowActivationState.Deactivated?1:0);HWSetControllerFocus(core.ActivationMode==CoreWindowActivationMode.ActivatedInForeground?1:0);};core.KeyDown+=(s,e)=>{if(policyReady)HWKeyDetails((uint)e.VirtualKey,e.KeyStatus.ScanCode,1,Math.Max(1,(int)e.KeyStatus.RepeatCount),e.KeyStatus.IsExtendedKey?1:0,e.KeyStatus.IsMenuKeyDown?1:0,e.KeyStatus.WasKeyDown?1:0);};core.KeyUp+=(s,e)=>{if(policyReady)HWKeyDetails((uint)e.VirtualKey,e.KeyStatus.ScanCode,0,Math.Max(1,(int)e.KeyStatus.RepeatCount),e.KeyStatus.IsExtendedKey?1:0,e.KeyStatus.IsMenuKeyDown?1:0,e.KeyStatus.WasKeyDown?1:0);};core.CharacterReceived+=(s,e)=>{if(policyReady)HWCharacterDetails(e.KeyCode,e.KeyStatus.ScanCode);};
            core.PointerMoved += ForwardPointer; core.PointerPressed += (s,e)=>ForwardPointer(s,e,2); core.PointerReleased += (s,e)=>ForwardPointer(s,e,4); core.PointerWheelChanged += ForwardPointer;
            // Relative motion comes from the real host mouse device, never inferred from a warped cursor.
            var mouse = Windows.Devices.Input.MouseDevice.GetForCurrentView();
            if(mouse != null) mouse.MouseMoved += (s,e) => { if(policyReady) HWMouseDelta(e.MouseDelta.X,e.MouseDelta.Y); };
            Button previous=null;GotFocus+=(s,e)=>{var selected=FocusManager.GetFocusedElement() as Button;if(ReferenceEquals(previous,selected))return;if(previous!=null){previous.ClearValue(Button.BackgroundProperty);previous.ClearValue(Button.ForegroundProperty);}previous=selected;if(selected!=null){selected.Background=new Windows.UI.Xaml.Media.SolidColorBrush(Windows.UI.Color.FromArgb(255,255,155,0));selected.Foreground=new Windows.UI.Xaml.Media.SolidColorBrush(Windows.UI.Colors.Black);}};
            Loaded += async (s, e) => { if (!ready) await StartLauncherAsync(); };
            heartbeat.Tick += async (s, e) => { if (!heartbeatBusy && sessionToken != null) { heartbeatBusy = true; try { await ValidateSessionAsync(false); } finally { heartbeatBusy = false; } } };
            SystemNavigationManager.GetForCurrentView().BackRequested += async (s, e) => { e.Handled = true; if(!loadBusy)await CancelLinkAsync(); };
        }
        private void ForwardPointer(CoreWindow sender, PointerEventArgs args){ForwardPointer(sender,args,1);}
        private void ForwardPointer(CoreWindow sender, PointerEventArgs args,uint touchPhase)
        {
            if(!policyReady) return;
            if(args.CurrentPoint.PointerDevice.PointerDeviceType == Windows.Devices.Input.PointerDeviceType.Touch){
                var point=args.CurrentPoint;uint flags=touchPhase;
                HWTouch(point.PointerId,(int)Math.Round(point.Position.X*100),(int)Math.Round(point.Position.Y*100),flags,unchecked((uint)(point.Timestamp/1000)));return;
            }
            if(args.CurrentPoint.PointerDevice.PointerDeviceType != Windows.Devices.Input.PointerDeviceType.Mouse) return;
            var p=args.CurrentPoint; var b=p.Properties;
            uint buttons=(b.IsLeftButtonPressed?1u:0u)|(b.IsRightButtonPressed?2u:0u)|(b.IsMiddleButtonPressed?4u:0u)|(b.IsXButton1Pressed?8u:0u)|(b.IsXButton2Pressed?16u:0u);
            HWPointer((int)p.Position.X,(int)p.Position.Y,buttons,b.IsHorizontalMouseWheel?0:b.MouseWheelDelta);
        }
        private void Show(StackPanel panel)
        {
            foreach (var p in new[] { SplashPanel, SetupPanel, LinkPanel, LinkedPanel, CancelledPanel, StartupFailurePanel }) p.Visibility = p == panel ? Visibility.Visible : Visibility.Collapsed;
        }
        private void ResetOperation() { contentReady=false;runtimeAudited=false;generation++; operation.Cancel(); operation.Dispose(); operation = new CancellationTokenSource(); }
        private async Task StartLauncherAsync()
        {
            if(startupBusy)return;startupBusy=true;LaunchButton.IsEnabled=false;ImportButton.IsEnabled=false;
            try{ready=true;App.StartupStage("page-loaded");await InitializeAsync();recovery.CompleteBoot();App.StartupStage("launcher-ready");}
            catch(Exception error){
                // Handle only this managed initialization operation. Game/native
                // exceptions still follow their real OS/guest failure path.
                var stage=App.ExecutionStage;ready=false;contentReady=false;
                try{await ManagedLog("LAUNCHER_STARTUP_FAILED stage="+stage+" HRESULT="+error.HResult.ToString("X8")+" type="+error.GetType().FullName);}catch{}
                App.StartupStage("launcher-startup-failed");
                StartupFailureText.Text="Stage: "+stage+"\nError: 0x"+error.HResult.ToString("X8")+"\n"+error.GetType().Name;
                Show(StartupFailurePanel);StatusText.Text="Launcher initialization stopped. Game startup has not been attempted. Existing files are retained.";
                StartupRetryButton.Focus(FocusState.Programmatic);
            }finally{startupBusy=false;LaunchButton.IsEnabled=!importBusy&&!loadBusy;ImportButton.IsEnabled=!importBusy&&!loadBusy;if(ready&&!App.NativePreflight&&LinkedPanel.Visibility==Visibility.Visible&&!importBusy&&!loadBusy)LaunchButton.Focus(FocusState.Programmatic);}
        }
        private async void StartupRetry_Click(object sender,RoutedEventArgs e){if(startupBusy)return;ResetOperation();await StartLauncherAsync();}
        private async Task InitializeAsync()
        {
            Show(SplashPanel); StatusText.Text = "Experimental native client loader. No working Fortnite lobby is claimed.";
            policyReady=false;
            if(recoveryMode)await ManagedLog("NATIVE_STARTUP recovery mode; automatic verification and native initialization paused; files retained");
            App.StartupStage("startup-policy-write");
            await FileIO.WriteTextAsync(await ApplicationData.Current.LocalFolder.CreateFileAsync("runtime-policy.txt",CreationCollisionOption.ReplaceExisting),"route=native-packaged nativeDeferredUntilLaunch=true recovery="+recoveryMode.ToString().ToLowerInvariant()+" xboxUnverified=true");
            if(App.NativePreflight||App.AudioPreflight||App.WglPreflight||App.UnsupportedSelfTest)await EnsureNativeReadyAsync();
            if(App.WglPreflight){try{var result=await Task.Run(()=>HWWglProviderProbe(Path.Combine(ApplicationData.Current.LocalFolder.Path,"native-wgl-probe.json")));StatusText.Text="Owned WGL provider test HRESULT="+result+"; no game code ran.";}catch(Exception ex){File.WriteAllText(Path.Combine(ApplicationData.Current.LocalFolder.Path,"wgl-managed-failure.log"),ex.ToString());StatusText.Text="Owned WGL check failed; no game code ran.";}return;}
            if(App.AudioPreflight){var audioResult=await Task.Run(()=>HWAudioProviderProbe(Path.Combine(ApplicationData.Current.LocalFolder.Path,"native-audio-probe.json")));StatusText.Text="Owned audio provider test HRESULT="+audioResult+". No game code ran.";return;}
            if(App.NativePreflight){try{await ContentDownload.StorageContract();await FileIO.WriteTextAsync(await ApplicationData.Current.LocalFolder.CreateFileAsync("packaged-content-plan.json",CreationCollisionOption.ReplaceExisting),await ContentDownload.AuditPackagedPlan());var nativeWindowResult=await Task.Run(()=>HWNativeWindowProbe(Path.Combine(ApplicationData.Current.LocalFolder.Path,"native-window-probe.json")));await ManagedLog("OWNED_NATIVE_WINDOW_ROUTE result="+nativeWindowResult+"; not Fortnite execution");if(await Task.Run(()=>HWMultimediaTimerProbe(Path.Combine(ApplicationData.Current.LocalFolder.Path,"native-multimedia-timers.json")))<0)throw new InvalidOperationException("Actual UWP multimedia timer scheduling/cancellation failed");uint networkFlags,networkError;var networkResult=HWNetworkAvailabilityProbe(out networkFlags,out networkError);
                    bool knownLan=false,unknownNetwork=false;int activeProfiles=0;
                    foreach(var profile in Windows.Networking.Connectivity.NetworkInformation.GetConnectionProfiles()){
                        if(profile.GetNetworkConnectivityLevel()==Windows.Networking.Connectivity.NetworkConnectivityLevel.None)continue;
                        activeProfiles++;var adapter=profile.NetworkAdapter;if(adapter==null){unknownNetwork=true;continue;}
                        var kind=adapter.IanaInterfaceType;if(kind==6||kind==71)knownLan=true;else if(kind!=24)unknownNetwork=true;
                    }
                    if(networkError==0&&(unknownNetwork||networkFlags!=(knownLan?1u:0u)||networkResult!=(knownLan?1:0)))throw new InvalidOperationException("Native network availability disagrees with live UWP connection profiles");
                    if(networkError!=0&&networkError!=50)throw new InvalidOperationException("Native UWP connectivity query failed: "+networkError);
                    if(networkError==50&&!unknownNetwork)throw new InvalidOperationException("Network availability rejected a supported live interface");
                    await FileIO.WriteTextAsync(await ApplicationData.Current.LocalFolder.CreateFileAsync("native-network-availability.json",CreationCollisionOption.ReplaceExisting),"{\"scope\":\"Real hosted UWP network profile query; not game login or Xbox\",\"activeProfiles\":"+activeProfiles+",\"knownLan\":"+knownLan.ToString().ToLowerInvariant()+",\"unsupportedInterface\":"+unknownNetwork.ToString().ToLowerInvariant()+",\"error\":"+networkError+",\"flags\":"+networkFlags+",\"gameExecuted\":false}");
                    if(HWStockCursorContract()!=621)throw new InvalidOperationException("Actual CoreWindow cursor API contract failed");var graphicsPolicy=await Task.Run(()=>HWFactoryBridgePolicy());await FileIO.WriteTextAsync(await ApplicationData.Current.LocalFolder.CreateFileAsync("native-graphics-policy.json",CreationCollisionOption.ReplaceExisting),"{\"scope\":\"Actual hosted AppContainer factory-dispatch memory operation, no game/Xbox execution\",\"hresult\":"+graphicsPolicy+",\"gameExecuted\":false}");await Task.Run(()=>HWPreflightClient());Show(LinkedPanel);UsernameText.Text="OWNED PROGRESS LAYOUT FIXTURE";
                ApplyContentProgress(new ContentProgress{Phase="Downloading",FileName="Owned display fixture, not an actual download",TotalFiles=139,VerifiedFiles=60,RequiredBytes=26449827840L,VerifiedBytes=10000000000L,DownloadBytes=12000000000L,NetworkBytes=6000000000L,BytesPerSecond=10000000});
                CodeStatusText.Text="Owned UI fixture; no client code was downloaded or executed.";
                StatusText.Text="Native import preflight complete. No game execution attempted.";Log_Click(this,new RoutedEventArgs());return;}catch(Exception ex){await FileIO.WriteTextAsync(await ApplicationData.Current.LocalFolder.CreateFileAsync("preflight-failure.txt",CreationCollisionOption.ReplaceExisting),ex.ToString());StatusText.Text="Owned hosted preflight failed. Inspect preflight-failure.txt; no game code ran.";return;}}
            var selfTestMarker=Path.Combine(ApplicationData.Current.LocalFolder.Path,"selftest-unsupported.once");if(App.UnsupportedSelfTest||File.Exists(selfTestMarker)){if(File.Exists(selfTestMarker))File.Delete(selfTestMarker);HWUnsupportedContract();return;}
            App.StartupStage("startup-delay");
            await Task.Delay(1500);
            try
            {
                App.StartupStage("backend-config-read");
                var file = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///backend-config.json"));
                var config = JsonObject.Parse(await FileIO.ReadTextAsync(file));
                allowLocalHttp = config.GetNamedBoolean("AllowLocalHttp", false);
                var origin = config.GetNamedString("BackendOrigin", "");
                var saved = ApplicationData.Current.LocalSettings.Values["BackendOrigin"] as string;
                if (String.IsNullOrWhiteSpace(origin)) origin = saved;
                if (String.IsNullOrWhiteSpace(origin)) { Show(SetupPanel); ConnectButton.Focus(FocusState.Programmatic); return; }
                await ConnectAsync(origin);
            }
            catch { Show(SetupPanel); StatusText.Text = "Configuration could not be loaded. Enter your deployed HTTPS backend origin."; ConnectButton.Focus(FocusState.Programmatic); }
        }
        private async Task<bool> EnsureNativeReadyAsync(){
            if(policyReady)return true;
            App.ExecutionStage="native-window-binding";
            try{recovery.BeginNativeStartup();
                var bound=HWBindWindow(LaunchLog);policyReady=bound>=0;
                await FileIO.WriteTextAsync(await ApplicationData.Current.LocalFolder.CreateFileAsync("runtime-policy.txt",CreationCollisionOption.ReplaceExisting),"route=native-packaged windowBind="+bound+" xboxUnverified=true");
                if(!policyReady)await ManagedLog("NATIVE_STARTUP_FAILED window binding HRESULT="+bound.ToString("X8"));
                return policyReady;
            }catch(Exception ex){await ManagedLog("NATIVE_STARTUP_FAILED native binding HRESULT="+ex.HResult.ToString("X8"));return false;}
            finally{recovery.EndNativeStartup();App.ExecutionStage="launcher";}
        }
        private async Task ConnectAsync(string origin)
        {
            ResetOperation(); heartbeat.Stop();
            var candidate = new ApiClient(origin.Trim(), allowLocalHttp);
            try
            {
                App.StartupStage("account-service-status");
                var status = await candidate.SendAsync("/api/status", null, null, operation.Token);
                if (status.GetNamedString("project", "") != "Hollowwake") throw new ArgumentException("This is not a Hollowwake account service.");
                api?.Dispose(); api = candidate;
                ApplicationData.Current.LocalSettings.Values["BackendOrigin"] = api.Origin;
                App.StartupStage("protected-session-read");
                sessionToken = await store.LoadAsync(api.Origin);
                App.StartupStage("protected-session-complete");
                if (sessionToken != null) await ValidateSessionAsync(true); else await StartLinkAsync();
            }
            catch { if (api != candidate) candidate.Dispose(); throw; }
        }
        private async void Connect_Click(object sender, RoutedEventArgs e)
        {
            ConnectButton.IsEnabled = false;
            try { await ConnectAsync(OriginInput.Text); }
            catch (OperationCanceledException) { }
            catch (ArgumentException) { StatusText.Text = "Use a valid HTTPS origin for a deployed Hollowwake backend, without a path or query."; }
            catch { StatusText.Text = "Cannot reach the account service. Check its HTTPS address and certificate, then try again."; }
            finally { ConnectButton.IsEnabled = true; }
        }
        private async Task CancelPendingAsync()
        {
            var old = pendingDeviceCode; pendingDeviceCode = null;
            if (old == null || api == null) return;
            try { using (var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(5))) await api.SendAsync("/api/device/cancel", ApiClient.Body("device_code", old), null, timeout.Token); }
            catch { /* It will expire server-side; never log the secret. */ }
        }
        private async Task StartLinkAsync()
        {
            App.StartupStage("account-link-begin");
            if (api == null) { Show(SetupPanel); return; }
            ResetOperation(); heartbeat.Stop(); var ticket = generation; var cancellation = operation.Token;
            await CancelPendingAsync();
            Show(LinkPanel); CodeText.Text = "Requesting…"; WebsiteText.Text = api.Origin + "/link"; QrImage.Source = null;
            PollingRing.IsActive = true; StatusText.Text = "Connecting securely to your Hollowwake account service…";
            RetryButton.Focus(FocusState.Programmatic);
            try
            {
                App.StartupStage("device-code-request");
                var issued = await api.SendAsync("/api/device/code", ApiClient.Body("client_id", "hollowwake-uwp"), null, cancellation);
                App.StartupStage("device-code-response");
                if (ticket != generation) return;
                pendingDeviceCode = issued.GetNamedString("device_code");
                var code = issued.GetNamedString("user_code");
                CodeText.Text = code.Length == 8 ? code.Substring(0, 4) + " " + code.Substring(4) : code;
                var verification = new Uri(issued.GetNamedString("verification_uri"));
                if (verification.GetLeftPart(UriPartial.Authority) != api.Origin) throw new ArgumentException("Unexpected verification origin.");
                WebsiteText.Text = verification.ToString();
                var expiry = DateTimeOffset.UtcNow.AddSeconds(issued.GetNamedNumber("expires_in"));
                ExpiryText.Text = "Expires at " + expiry.ToLocalTime().ToString("t") + ". Only approve a code on your own device.";
                await SetQrAsync(issued.GetNamedString("qr_png_base64"));
                if (ticket != generation) return;
                StatusText.Text = "Waiting for your explicit website approval. Retry requests a new code. B cancels.";
                int interval = Math.Max(5, (int)issued.GetNamedNumber("interval", 5));
                while (!cancellation.IsCancellationRequested && ticket == generation && DateTimeOffset.UtcNow < expiry)
                {
                    await Task.Delay(TimeSpan.FromSeconds(interval), cancellation);
                    try
                    {
                        var result = await api.SendAsync("/api/device/token", ApiClient.Body("grant_type", "urn:ietf:params:oauth:grant-type:device_code", "device_code", pendingDeviceCode, "client_id", "hollowwake-uwp"), null, cancellation);
                        var token = result.GetNamedString("access_token");
                        if (ticket != generation) { await RevokeTokenAsync(token); return; }
                        pendingDeviceCode = null; sessionToken = token;
                        await store.SaveAsync(api.Origin, token);
                        if (ticket != generation) { await RevokeTokenAsync(token); await store.ClearAsync(); sessionToken = null; return; }
                        await ValidateSessionAsync(true); return;
                    }
                    catch (ApiException ex) when (ex.Code == "authorization_pending") { }
                    catch (ApiException ex) when (ex.Code == "slow_down") { interval += 5; }
                    catch (ApiException ex) when (ex.Code == "rate_limited") { interval = Math.Max(interval, 30); StatusText.Text = "Requests are rate limited. Waiting before polling again."; }
                    catch (ApiException ex) when (ex.Code == "expired_token" || ex.Code == "invalid_grant" || ex.Code == "access_denied")
                    { StatusText.Text = "Code expired, cancelled, used or denied. Choose Retry for a fresh code."; PollingRing.IsActive = false; return; }
                    catch (OperationCanceledException) when (!cancellation.IsCancellationRequested)
                    { interval = Math.Min(interval + 5, 30); StatusText.Text = "Connection timed out. Waiting to retry; your code is unchanged."; }
                    catch (System.Net.Http.HttpRequestException)
                    { interval = Math.Min(interval + 5, 30); StatusText.Text = "Connection lost. Retrying until this code expires."; }
                }
                if (ticket == generation) { StatusText.Text = "This code expired. Choose Retry."; PollingRing.IsActive = false; }
            }
            catch (OperationCanceledException) { }
            catch { if (ticket == generation) { StatusText.Text = "Linking could not continue. Check your connection and choose Retry."; PollingRing.IsActive = false; } }
        }
        private async Task SetQrAsync(string encoded)
        {
            if (encoded.Length > 100000) throw new ArgumentException("QR response too large.");
            var bytes = Convert.FromBase64String(encoded);
            using (var stream = new InMemoryRandomAccessStream())
            {
                using (var writer = new DataWriter(stream)) { writer.WriteBytes(bytes); await writer.StoreAsync(); writer.DetachStream(); }
                stream.Seek(0); var bitmap = new BitmapImage(); await bitmap.SetSourceAsync(stream); QrImage.Source = bitmap;
            }
        }
        private async Task ValidateSessionAsync(bool focus)
        {
            if (api == null || sessionToken == null) return;
            int ticket = generation; var token = sessionToken;
            Show(LinkedPanel); if(!importBusy&&!loadBusy)UsernameText.Text = "Checking saved session…";
            try
            {
                var profile = await api.SendAsync("/api/device/profile", null, token, operation.Token);
                if (ticket != generation || sessionToken != token) return;
                UsernameText.Text = profile.GetNamedString("publicUsername");
                await api.SendAsync("/api/device/renew", new JsonObject(), token, operation.Token);
                if(!importBusy&&!loadBusy)StatusText.Text = recoveryMode?"Recovery: previous verification/startup was interrupted. Files are retained. Open Show launch log; Download / Verify resumes only when selected.":"Account verified. Native initialization starts only when Launch is selected.";
                heartbeat.Start();
                if(focus){
                    var receipt=await ClientDiagnostics.SubmitAsync(api,token,"next_start");
                    if(receipt!=null)StatusText.Text+=" Failure report uploaded: "+receipt;
                    if(!recoveryMode)await PrepareContentAsync();if(!startupBusy&&!importBusy)LaunchButton.Focus(FocusState.Programmatic);
                }else{var ignored=ClientDiagnostics.FlushAsync(api,token);}

            }
            catch (ApiException ex) when (ex.Status == HttpStatusCode.Unauthorized)
            {
                if (ticket != generation) return;
                heartbeat.Stop(); sessionToken = null; await store.ClearAsync();
                await StartLinkAsync();
            }
            catch (OperationCanceledException) { if (ticket == generation) StatusText.Text = "Connection timed out. Choose Check connection; your protected session is retained."; }
            catch { if (ticket == generation) { UsernameText.Text = "Connection unavailable"; StatusText.Text = "Cannot verify this session offline. Choose Check connection to retry. No game session has started."; } }
        }
        private void ApplyContentProgress(ContentProgress state){ContentStatusText.Text=state.Phase+" · "+state.FileName;
                        ContentProgressBar.Value=state.Phase=="Downloading"?(state.DownloadBytes==0?0:100.0*state.NetworkBytes/state.DownloadBytes):100.0*state.VerifiedBytes/Math.Max(1,state.RequiredBytes);
                        ContentBytesText.Text="Downloaded this session: "+(state.NetworkBytes/1000000000.0).ToString("F2")+" GB / "+(state.DownloadBytes/1000000000.0).ToString("F2")+" GB\nVerified: "+state.VerifiedFiles+"/"+state.TotalFiles+" files · "+(state.VerifiedBytes/1000000000.0).ToString("F2")+"/"+(state.RequiredBytes/1000000000.0).ToString("F2")+" GB"+(state.Phase=="Downloading"?" · "+(state.BytesPerSecond/1000000.0).ToString("F1")+" MB/s":"");
        }
        private async Task<bool> PrepareContentAsync(){
            if(contentReady)return true;if(importBusy||loadBusy||api==null||sessionToken==null)return false;
            if(!File.Exists(Path.Combine(Windows.ApplicationModel.Package.Current.InstalledLocation.Path,"native-package-manifest.json"))){StatusText.Text="The actual client is not included in this package. Install the private client build before downloading game content.";return false;}
            try{recovery.BeginVerification();}catch(Exception ex)when(ex is IOException||ex is UnauthorizedAccessException){StatusText.Text="Cannot write the download checkpoint. Check Xbox app storage; no download was marked complete.";return false;}App.ExecutionStage="content-verification";
            int ticket=generation;var token=sessionToken;importBusy=true;contentReady=false;LaunchButton.IsEnabled=false;ImportButton.IsEnabled=false;PauseContentButton.IsEnabled=true;ContentRing.IsActive=true;
            using(var cancel=CancellationTokenSource.CreateLinkedTokenSource(operation.Token)){contentOperation=cancel;
                try{var progress=new Progress<string>(message=>{if(ticket==generation)ContentStatusText.Text=message;});
                    var codeProgress=new Progress<string>(message=>{if(ticket==generation)CodeStatusText.Text=message;});
                    var detail=new Progress<ContentProgress>(state=>{if(ticket==generation)ApplyContentProgress(state);});
                    var result=await ContentDownload.Run(api,token,progress,cancel.Token,detail,codeProgress);
                    if(!result.Complete||result.OnDiskBytes!=26449827840L)throw new IOException("Installed data audit is incomplete.");cancel.Token.ThrowIfCancellationRequested();if(ticket!=generation||token!=sessionToken)return false;contentReady=true;recoveryMode=false;StatusText.Text="Selected data on disk: "+(result.OnDiskBytes/1000000000.0).ToString("F2")+" GB; this session downloaded "+(result.NetworkBytes/1000000000.0).ToString("F2")+" GB. Native client login/lobby remain unfinished.";await ManagedLog("CONTENT_READY bytes="+result.OnDiskBytes+" total="+result.VerifiedFiles+" missing=0");await ClientDiagnostics.SubmitAsync(api,token,"manual");return true;}
                catch(OperationCanceledException){if(ticket==generation)StatusText.Text="Download paused. Verified files are retained; Retry restarts only unfinished files.";return false;}
                catch(Exception ex){if(ticket==generation)StatusText.Text=ex is ApiException?"Game download stopped: "+((ApiException)ex).Code+" (HTTP "+(int)((ApiException)ex).Status+"). Check the launch log.":(ex is IOException||ex is InvalidDataException)?ex.Message:"Content download stopped (HRESULT "+ex.HResult.ToString("X8")+"). Check the launch log.";await ManagedLog("CONTENT_FAILED HRESULT="+ex.HResult.ToString("X8"));var receipt=await ClientDiagnostics.SubmitAsync(api,token,"preflight");return false;}
                finally{recovery.EndVerification(contentReady);App.ExecutionStage="launcher";contentOperation=null;importBusy=false;ContentRing.IsActive=false;PauseContentButton.IsEnabled=false;ImportButton.IsEnabled=!startupBusy;LaunchButton.IsEnabled=!startupBusy;}
            }
        }
        private async void ContentRetry_Click(object sender,RoutedEventArgs e){if(startupBusy||importBusy||loadBusy)return;contentReady=false;await PrepareContentAsync();}
        private void ContentPause_Click(object sender,RoutedEventArgs e){if(contentOperation!=null)contentOperation.Cancel();}
        private async Task ManagedLog(string text){await FileIO.AppendTextAsync(await ApplicationData.Current.LocalFolder.CreateFileAsync("managed-launch.log",CreationCollisionOption.OpenIfExists),DateTimeOffset.UtcNow.ToString("o")+" "+text+"\n");}
        private async void Import_Click(object sender,RoutedEventArgs e){await ImportFiles(false);}
        private async void BootImport_Click(object sender,RoutedEventArgs e){await ImportFiles(true);}
        private async Task ImportFiles(bool minimum){
            if(importBusy||loadBusy)return;if(!policyReady){StatusText.Text="Executable-memory policy failed. Show the log; import is paused to avoid an unnecessary 39 GB download.";return;}
            if(sessionToken==null){StatusText.Text="Link your project account first.";return;}importBusy=true;
            try{await api.SendAsync("/api/device/profile",null,sessionToken,operation.Token);var progress=new Progress<string>(text=>{var ignored=Dispatcher.RunAsync(CoreDispatcherPriority.Normal,()=>StatusText.Text=text);});if(File.Exists(Path.Combine(Windows.ApplicationModel.Package.Current.InstalledLocation.Path,"native-package-manifest.json"))){if(minimum){StatusText.Text="Native code is included in this installed package. Use full content verification/download for the game assets.";await ManagedLog("BOOT_IMPORT_NOT_NEEDED native code is installed; no LocalState DLL is used by the native loader");return;}await ContentDownload.Run(api,sessionToken,progress,operation.Token);await ManagedLog("NATIVE_CONTENT_READY data-only files verified; installed code was not replaced");StatusText.Text="Content verified. Choose Attempt client loading.";return;}await Task.Run(()=>minimum?BootImport.Run(progress,operation.Token):GameImport.Run(progress,operation.Token));StatusText.Text="Import complete. Choose Attempt client loading.";await ManagedLog(minimum?"Minimum boot input hashes verified; full ZIP was not downloaded and launchers/scripts were not selected.":"Immutable archive verified; clean copy and quarantine inventory complete.");
                // Keep the verified Season 6 landscape visible in the host. Only guest rendering may replace it with the actual game loading/lobby.

            }catch(OperationCanceledException){StatusText.Text="Import paused. Download bytes are retained; retry resumes. Incomplete extraction is never marked ready.";}
            catch(Exception ex){await ManagedLog("Import failed HRESULT="+ex.HResult.ToString("X8")+(ex is IOException?" "+ex.Message:""));StatusText.Text=ex is IOException?ex.Message:"Import failed. Show managed-launch.log; no launcher was executed.";}finally{importBusy=false;}
        }
        private async void Launch_Click(object sender,RoutedEventArgs e){
            if(startupBusy||loadBusy||importBusy)return;if(sessionToken!=null&&!contentReady&&!await PrepareContentAsync())return;if(!await EnsureNativeReadyAsync()){StatusText.Text="Native graphics window binding failed. Open launch logs.";await ManagedLog("NATIVE_STARTUP_FAILED window binding unavailable");var receipt=await ClientDiagnostics.SubmitAsync(api,sessionToken,"preflight");if(receipt!=null)StatusText.Text+=" Report uploaded: "+receipt;return;}
            if(sessionToken==null){StatusText.Text="Link your project account first.";return;}loadBusy=true;
            try{ClientDiagnostics.BeginAttempt();await api.SendAsync("/api/device/profile",null,sessionToken,operation.Token);StatusText.Text="Attempting real client initialization. The app may close at the first unsupported API. Reopen it to retrieve launch logs.";await ManagedLog("USER_REQUESTED native packaged-client path; no long-lived project session is passed into guest code.");var origin=new Uri(api.Origin);if(!HWConfigureNetwork(origin.DnsSafeHost,(ushort)origin.Port))throw new InvalidOperationException("Native backend host configuration rejected");var compatibility=await Task.Run(()=>HWPreflightClient());if(compatibility<0){StatusText.Text="Native imports remain incomplete. Downloaded content is retained; the failure report contains exact names.";await ManagedLog("CLIENT_NOT_READY import preflight blocked content transfer and native entry");var receipt=await ClientDiagnostics.SubmitAsync(api,sessionToken,"preflight");if(receipt!=null)StatusText.Text+=" Report uploaded: "+receipt;return;}if(!contentReady)throw new IOException("Game content verification is not complete.");await api.SendAsync("/api/device/profile",null,sessionToken,operation.Token);await ManagedLog("CONTENT_READY authenticated manifest and all selected content hashes verified");await ClientDiagnostics.SubmitAsync(api,sessionToken,"before_entry");
                var gameGrant=await api.SendAsync("/api/client-adapter/exchange",new JsonObject(),sessionToken,operation.Token);
                var gameCode=gameGrant.GetNamedString("exchange_code","");var gameAccount=gameGrant.GetNamedString("account_id","");var lifetime=gameGrant.GetNamedNumber("expires_in",0);
                if(gameGrant.GetNamedString("build","")!="6.21-CL-4526925"||gameGrant.GetNamedString("origin","")!=api.Origin||gameGrant.GetNamedString("authentication_type","")!="exchangecode"||!System.Text.RegularExpressions.Regex.IsMatch(gameAccount,@"\A[a-f0-9]{32}\z")||!System.Text.RegularExpressions.Regex.IsMatch(gameCode,@"\A[A-Za-z0-9_-]{43}\z")||lifetime<1||lifetime>300||lifetime!=Math.Floor(lifetime))throw new InvalidOperationException("Game account grant validation failed");
                if(HWConfigureClientLogin(gameCode,(uint)lifetime)<0)throw new InvalidOperationException("Native game account ticket rejected");
                gameCode=null;gameGrant=null;await ManagedLog("NATIVE_STARTUP linked account game ticket configured; project session remains in launcher only");
                ClientDiagnostics.MarkEntryPending();App.ExecutionStage="native-client-entry";var result=await Task.Run(()=>HWAttemptPackagedClient());ClientDiagnostics.CompleteAttempt();await ManagedLog("Native loading returned "+result+". Original lobby success has not been established.");StatusText.Text="Native loading stopped with result "+result+". Choose Show launch log; restart the app before another native load attempt.";var uploaded=await ClientDiagnostics.SubmitAsync(api,sessionToken,"launch_return");if(uploaded!=null)StatusText.Text+=" Report uploaded: "+uploaded;}
            catch(Exception ex){if(ex is IOException){await ManagedLog("CONTENT_FAILED "+ex.Message);StatusText.Text=ex.Message;}else{await ManagedLog("NATIVE_LOAD_EXCEPTION HRESULT="+ex.HResult.ToString("X8"));StatusText.Text="Native attempt failed. Retrieve the launch logs.";}ClientDiagnostics.CompleteAttempt();var receipt=await ClientDiagnostics.SubmitAsync(api,sessionToken,"launch_return");if(receipt!=null)StatusText.Text+=" Report uploaded: "+receipt;}finally{loadBusy=false;}
        }
        private static async Task<string> StorageLocationReport(){
            var local=ApplicationData.Current.LocalFolder;
            string report="STORAGE LOCATION (queried from this running app)\nPackage: "+Windows.ApplicationModel.Package.Current.Id.FullName+"\nLocalState: "+local.Path+"\nInstalled package: "+Windows.ApplicationModel.Package.Current.InstalledLocation.Path+"\nGame data: "+Path.Combine(local.Path,"GameClean","6.21")+"\n";
            try{
                var game=await local.GetFolderAsync("GameClean");
                var build=await game.GetFolderAsync("6.21");
                var fortnite=await build.GetFolderAsync("FortniteGame");
                var content=await fortnite.GetFolderAsync("Content");
                var paks=await content.GetFolderAsync("Paks");
                var files=await paks.GetFilesAsync();ulong bytes=0;
                foreach(var file in files){var properties=await file.GetBasicPropertiesAsync();bytes=checked(bytes+properties.Size);}
                report+="WinRT Paks directory: "+paks.Path+"\nWinRT direct file count: "+files.Count+"; bytes: "+bytes+"\nThis independent listing measures files; it does not verify their hashes.\n";
            }catch(Exception error){report+="WinRT Paks listing unavailable: HRESULT=0x"+error.HResult.ToString("X8")+"\nNo file count or installed size is inferred from the manifest.\n";}
            return report;
        }
        private async void Log_Click(object sender,RoutedEventArgs e){
            string storage=await StorageLocationReport();
            string native="",policy="",managed="";
            foreach(var name in new[]{"native-launch.log","runtime-policy.txt","managed-launch.log","native-imports-unresolved.txt","native-platform-imports.json","content-data-audit.txt","content-code-audit.txt","content-space-audit.txt","content-location.txt"}){
                try{var content=await FileIO.ReadTextAsync(await ApplicationData.Current.LocalFolder.GetFileAsync(name));if(name=="native-launch.log")native=content;else if(name=="runtime-policy.txt")policy=content;else managed+=name+"\n"+content+"\n";}catch{}
            }
            int start=native.LastIndexOf("ATTEMPT_BEGIN",StringComparison.Ordinal);string latest=start>=0?native.Substring(start):native;
            var important=new System.Collections.Generic.List<string>();var phases=new System.Collections.Generic.List<string>();
            foreach(var raw in latest.Split(new[]{'\n'},StringSplitOptions.RemoveEmptyEntries)){
                var line=raw.Trim();
                if(IsFailureLine(line))important.Add(line);
                if(line.StartsWith("NATIVE_")||line.StartsWith("ATTEMPT_BEGIN")||line.StartsWith("HASH ")||line.StartsWith("IMPORT ")||line.StartsWith("PROTECT ")||line.StartsWith("UNWIND ")||line.StartsWith("DEPENDENCY ")||line.StartsWith("EXECUTE "))phases.Add(line);
            }
            string summary="CLIENT FAILURE SUMMARY — package "+ClientDiagnostics.Version()+"\n";
            if(important.Count>0)summary+=important[important.Count-1];
            else{
                summary+=loadBusy?"The latest client attempt is still in progress.":"The latest attempt has not recorded a named failure.";
                var previous=new System.Collections.Generic.List<string>();
                if(start>0)foreach(var raw in native.Substring(0,start).Split(new[]{'\n'},StringSplitOptions.RemoveEmptyEntries)){var line=raw.Trim();if(IsFailureLine(line))previous.Add(line);}
                if(previous.Count>0)summary+="\n\nPREVIOUS RECORDED FAILURE (earlier attempt)\n"+previous[previous.Count-1];
            }
            if(phases.Count>0)summary+="\n\nLAST STAGE\n"+phases[phases.Count-1];
            try{var upload=Path.Combine(ApplicationData.Current.LocalFolder.Path,"diagnostic-upload-status.txt");if(File.Exists(upload))summary+="\n\n"+File.ReadAllText(upload);}catch{}
            summary+="\n\nNative route: game DLLs must be installed in the private client package. A LocalState boot import cannot provide them. The original lobby has not been reached.";

            string full=storage+"\nruntime-policy.txt\n"+policy+"\nnative-launch.log\n"+native+"\nmanaged-launch.log\n"+managed;
            var block=new TextBlock{Text=summary,FontSize=18,TextWrapping=TextWrapping.Wrap,IsTextSelectionEnabled=false};
            Windows.UI.Xaml.Automation.AutomationProperties.SetAutomationId(block,"FailureSummary");
            var dialog=new ContentDialog{Title="Native client diagnostics — "+ClientDiagnostics.Version(),CloseButtonText="Close",PrimaryButtonText="Missing imports",SecondaryButtonText="Full log",Content=block};
            Windows.UI.Xaml.Automation.AutomationProperties.SetAutomationId(dialog,"ClientDiagnostics");
            Button previousDiagnostic=null;dialog.GotFocus+=(diagnosticSender,focusArgs)=>{var selected=FocusManager.GetFocusedElement() as Button;if(previousDiagnostic!=null){previousDiagnostic.ClearValue(Button.BackgroundProperty);previousDiagnostic.ClearValue(Button.ForegroundProperty);}previousDiagnostic=selected;if(selected!=null){selected.Background=new Windows.UI.Xaml.Media.SolidColorBrush(Windows.UI.Color.FromArgb(255,255,155,0));selected.Foreground=new Windows.UI.Xaml.Media.SolidColorBrush(Windows.UI.Colors.Black);}};
            string[] lines=null;int page=0;const int pageSize=10;
            Action render=()=>{int count=Math.Max(1,(lines.Length+pageSize-1)/pageSize);block.Text="PAGE "+(page+1)+" / "+count+"\n\n"+String.Join("\n",lines,page*pageSize,Math.Min(pageSize,lines.Length-page*pageSize));dialog.PrimaryButtonText="Next page";dialog.SecondaryButtonText="Previous page";};
            dialog.PrimaryButtonClick+=(d,args)=>{args.Cancel=true;if(lines==null){var report=ClientDiagnostics.MissingText();lines=DiagnosticRows(report);page=0;}else page=(page+1)%Math.Max(1,(lines.Length+pageSize-1)/pageSize);render();};
            dialog.SecondaryButtonClick+=(d,args)=>{args.Cancel=true;if(lines==null){lines=DiagnosticRows(full);page=0;}else page=(page+Math.Max(1,(lines.Length+pageSize-1)/pageSize)-1)%Math.Max(1,(lines.Length+pageSize-1)/pageSize);render();};
            await dialog.ShowAsync();
        }
        private static string[] DiagnosticRows(string text){var rows=new System.Collections.Generic.List<string>();foreach(var raw in text.Split(new[]{'\n'},StringSplitOptions.RemoveEmptyEntries)){var line=raw.TrimEnd('\r');for(int offset=0;offset<line.Length;offset+=40)rows.Add(line.Substring(offset,Math.Min(40,line.Length-offset)));}return rows.Count>0?rows.ToArray():new[]{"No records."};}
        private static bool IsFailureLine(string line){return line.StartsWith("NATIVE_PLATFORM_AUDIT_FAILED ")||line.StartsWith("NATIVE_STARTUP_FAILED ")||line.StartsWith("CONTENT_FAILED ")||line.StartsWith("NATIVE_PROXY_BLOCKED ")||line.StartsWith("NATIVE_LOAD_BLOCKED ")||line.StartsWith("NATIVE_LOAD_FAILED ")||line.StartsWith("NATIVE_LOAD_EXCEPTION ")||line.StartsWith("NATIVE_CRASH ")||line.StartsWith("DEP blocked ")||line.StartsWith("DEP ambiguous ")||line.StartsWith("DEP UNRESOLVED ")||line.StartsWith("STOP ")||line.StartsWith("CRASH ")||line.StartsWith("CALLED_UNSUPPORTED ");}
        private async Task RevokeTokenAsync(string token)
        {
            try { using (var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(5))) await api.SendAsync("/api/device/logout", new JsonObject(), token, timeout.Token); } catch { }
        }
        private async void Retry_Click(object sender, RoutedEventArgs e) { if (sessionToken != null) await ValidateSessionAsync(true); else await StartLinkAsync(); }
        private async void Check_Click(object sender, RoutedEventArgs e) { await ValidateSessionAsync(true); }
        private async void Cancel_Click(object sender, RoutedEventArgs e) { await CancelLinkAsync(); }
        private async Task CancelLinkAsync()
        {
            if(sessionToken!=null){if(contentOperation!=null)contentOperation.Cancel();return;}
            ResetOperation(); await CancelPendingAsync(); PollingRing.IsActive = false; CodeText.Text = ""; QrImage.Source = null;
            Show(CancelledPanel); StatusText.Text = "Link request cancelled. No game or lobby was launched."; StartButton.Focus(FocusState.Programmatic);
        }
        private async void SignOut_Click(object sender, RoutedEventArgs e)
        {
            ResetOperation(); heartbeat.Stop(); var old = sessionToken; sessionToken = null;
            if (old != null) await RevokeTokenAsync(old);
            await store.ClearAsync(); Show(CancelledPanel); StatusText.Text = "Signed out locally. If offline, also revoke the device from account settings."; StartButton.Focus(FocusState.Programmatic);
        }
        private async void Page_KeyDown(object sender, KeyRoutedEventArgs e)
        {
            if(loadBusy)return; // The running guest owns B/Escape, not launcher cancellation.
            FocusNavigationDirection? direction=null;
            if(e.Key==VirtualKey.GamepadDPadUp||e.Key==VirtualKey.GamepadLeftThumbstickUp)direction=FocusNavigationDirection.Up;
            if(e.Key==VirtualKey.GamepadDPadDown||e.Key==VirtualKey.GamepadLeftThumbstickDown)direction=FocusNavigationDirection.Down;
            if(e.Key==VirtualKey.GamepadDPadLeft||e.Key==VirtualKey.GamepadLeftThumbstickLeft)direction=FocusNavigationDirection.Left;
            if(e.Key==VirtualKey.GamepadDPadRight||e.Key==VirtualKey.GamepadLeftThumbstickRight)direction=FocusNavigationDirection.Right;
            if(direction.HasValue){e.Handled=FocusManager.TryMoveFocus(direction.Value);return;}

            if (e.Key == VirtualKey.GamepadB || e.Key == VirtualKey.Escape) { e.Handled = true; await CancelLinkAsync(); }
        }
        public void Pause() { if(policyReady)HWSetControllerFocus(0); ResetOperation(); heartbeat.Stop(); }
        public async void Resume()
        {
            var core=CoreWindow.GetForCurrentThread();
            if(policyReady)HWSetControllerFocus(core!=null&&core.ActivationMode==CoreWindowActivationMode.ActivatedInForeground?1:0);
            if(loadBusy)return; // Resume guest input without restarting launcher linking.
            if (!ready || api == null) return;
            if (sessionToken != null) await ValidateSessionAsync(true); else await StartLinkAsync();
        }
    }
}
