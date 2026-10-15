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
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)] private static extern int HWAttempt(string root);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern void HWKey(uint key,int down);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern void HWChar(uint key);
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int HWWindowContract();
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int HWControllerCount();
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int HWGraphicsContract();
        [DllImport("HollowwakeClientRuntime.dll",CallingConvention=CallingConvention.Cdecl)] private static extern void HWUnsupportedContract();
        private bool importBusy,loadBusy,policyReady;
        private string LaunchLog {get{return Path.Combine(ApplicationData.Current.LocalFolder.Path,"native-launch.log");}}
        private readonly SessionStore store = new SessionStore();
        private ApiClient api;
        private CancellationTokenSource operation = new CancellationTokenSource();
        private string sessionToken, pendingDeviceCode;
        private int generation;
        private bool allowLocalHttp, ready;
        private readonly DispatcherTimer heartbeat = new DispatcherTimer { Interval = TimeSpan.FromMinutes(1) };
        private bool heartbeatBusy;
        public MainPage()
        {
            InitializeComponent();
            var core=CoreWindow.GetForCurrentThread();core.KeyDown+=(s,e)=>HWKey((uint)e.VirtualKey,1);core.KeyUp+=(s,e)=>HWKey((uint)e.VirtualKey,0);core.CharacterReceived+=(s,e)=>HWChar(e.KeyCode);
            Button previous=null;GotFocus+=(s,e)=>{var selected=FocusManager.GetFocusedElement() as Button;if(previous!=null)previous.ClearValue(Button.BackgroundProperty);previous=selected;if(selected!=null)selected.Background=new Windows.UI.Xaml.Media.SolidColorBrush(Windows.UI.Color.FromArgb(255,255,138,36));};
            Loaded += async (s, e) => { if (!ready) { ready = true; await InitializeAsync(); } };
            heartbeat.Tick += async (s, e) => { if (!heartbeatBusy && sessionToken != null) { heartbeatBusy = true; try { await ValidateSessionAsync(false); } finally { heartbeatBusy = false; } } };
            SystemNavigationManager.GetForCurrentView().BackRequested += async (s, e) => { e.Handled = true; await CancelLinkAsync(); };
        }
        private void Show(StackPanel panel)
        {
            foreach (var p in new[] { SplashPanel, SetupPanel, LinkPanel, LinkedPanel, CancelledPanel }) p.Visibility = p == panel ? Visibility.Visible : Visibility.Collapsed;
        }
        private void ResetOperation() { generation++; operation.Cancel(); operation.Dispose(); operation = new CancellationTokenSource(); }
        private async Task InitializeAsync()
        {
            Show(SplashPanel); StatusText.Text = "Experimental native client loader. No working Fortnite lobby is claimed.";
            try {var bound=HWBindWindow(LaunchLog);var policy=HWMemoryPolicy(LaunchLog);var contract=HWWindowContract();var graphics=HWGraphicsContract();policyReady=bound>=0&&policy==621&&contract==621&&graphics==621;await FileIO.WriteTextAsync(await ApplicationData.Current.LocalFolder.CreateFileAsync("runtime-policy.txt",CreationCollisionOption.ReplaceExisting),"windowBind="+bound+" memoryExecution="+policy+" windowMessageContract="+contract+" graphicsContract="+graphics+" controllerCount="+HWControllerCount()+" xboxUnverified=true");}
            catch(Exception ex){await ManagedLog("Native initialization HRESULT="+ex.HResult.ToString("X8"));}
            var selfTestMarker=Path.Combine(ApplicationData.Current.LocalFolder.Path,"selftest-unsupported.once");if(App.UnsupportedSelfTest||File.Exists(selfTestMarker)){if(File.Exists(selfTestMarker))File.Delete(selfTestMarker);HWUnsupportedContract();return;}
            await Task.Delay(1500);
            try
            {
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
        private async Task ConnectAsync(string origin)
        {
            ResetOperation(); heartbeat.Stop();
            var candidate = new ApiClient(origin.Trim(), allowLocalHttp);
            try
            {
                var status = await candidate.SendAsync("/api/status", null, null, operation.Token);
                if (status.GetNamedString("project", "") != "Hollowwake") throw new ArgumentException("This is not a Hollowwake account service.");
                api?.Dispose(); api = candidate;
                ApplicationData.Current.LocalSettings.Values["BackendOrigin"] = api.Origin;
                sessionToken = await store.LoadAsync(api.Origin);
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
            if (api == null) { Show(SetupPanel); return; }
            ResetOperation(); heartbeat.Stop(); var ticket = generation; var cancellation = operation.Token;
            await CancelPendingAsync();
            Show(LinkPanel); CodeText.Text = "Requesting…"; WebsiteText.Text = api.Origin + "/link"; QrImage.Source = null;
            PollingRing.IsActive = true; StatusText.Text = "Connecting securely to your Hollowwake account service…";
            RetryButton.Focus(FocusState.Programmatic);
            try
            {
                var issued = await api.SendAsync("/api/device/code", ApiClient.Body("client_id", "hollowwake-uwp"), null, cancellation);
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
            Show(LinkedPanel); UsernameText.Text = "Checking saved session…";
            try
            {
                var profile = await api.SendAsync("/api/device/profile", null, token, operation.Token);
                if (ticket != generation || sessionToken != token) return;
                UsernameText.Text = profile.GetNamedString("publicUsername");
                await api.SendAsync("/api/device/renew", new JsonObject(), token, operation.Token);
                StatusText.Text = policyReady?"Account verified. Import the inspected archive, then attempt native client loading. This is not a proven port.":"Native executable-memory policy or initialization failed. Show the launch log before downloading game files.";
                heartbeat.Start(); if (focus) ImportButton.Focus(FocusState.Programmatic);
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
        private async Task ManagedLog(string text){await FileIO.AppendTextAsync(await ApplicationData.Current.LocalFolder.CreateFileAsync("managed-launch.log",CreationCollisionOption.OpenIfExists),DateTimeOffset.UtcNow.ToString("o")+" "+text+"\n");}
        private async void Import_Click(object sender,RoutedEventArgs e){
            if(importBusy||loadBusy)return;if(!policyReady){StatusText.Text="Executable-memory policy failed. Show the log; import is paused to avoid an unnecessary 39 GB download.";return;}
            if(sessionToken==null){StatusText.Text="Link your project account first.";return;}importBusy=true;
            try{await api.SendAsync("/api/device/profile",null,sessionToken,operation.Token);await Task.Run(()=>GameImport.Run(new Progress<string>(text=>{var ignored=Dispatcher.RunAsync(CoreDispatcherPriority.Normal,()=>StatusText.Text=text);}),operation.Token));StatusText.Text="Import complete. Choose Attempt client loading.";await ManagedLog("Immutable archive verified; clean copy and quarantine inventory complete.");
                var splash=Path.Combine(GameImport.Root,"6.21","FortniteGame","Content","Splash","Splash.bmp");if(File.Exists(splash)){var f=await StorageFile.GetFileFromPathAsync(splash);using(var input=await f.OpenReadAsync()){var image=new BitmapImage();await image.SetSourceAsync(input);BackgroundImage.Source=image;}}
            }catch(OperationCanceledException){StatusText.Text="Import paused. Download bytes are retained; retry resumes. Incomplete extraction is never marked ready.";}
            catch(Exception ex){await ManagedLog("Import failed HRESULT="+ex.HResult.ToString("X8")+(ex is IOException?" "+ex.Message:""));StatusText.Text=ex is IOException?ex.Message:"Import failed. Show managed-launch.log; no launcher was executed.";}finally{importBusy=false;}
        }
        private async void Launch_Click(object sender,RoutedEventArgs e){
            if(loadBusy||importBusy)return;if(!policyReady||!GameImport.Ready){StatusText.Text="A successful executable-memory check and completed pinned archive import are required.";return;}
            if(sessionToken==null){StatusText.Text="Link your project account first.";return;}loadBusy=true;
            try{await api.SendAsync("/api/device/profile",null,sessionToken,operation.Token);StatusText.Text="Attempting real client initialization. The app may close at the first unsupported API. Reopen it to retrieve launch logs.";await ManagedLog("USER_REQUESTED real client boot; no project credential is passed into guest code.");var result=await Task.Run(()=>HWAttempt(GameImport.Root));await ManagedLog("Native loading returned "+result+". No lobby success is inferred.");StatusText.Text="Native loading stopped with result "+result+". Choose Show launch log; restart the app before another mapping attempt.";}
            catch(Exception ex){await ManagedLog("Native call HRESULT="+ex.HResult.ToString("X8"));StatusText.Text="Native attempt failed. Retrieve the launch logs.";}finally{loadBusy=false;}
        }
        private async void Log_Click(object sender,RoutedEventArgs e){
            string text="Logs are in this package's LocalState: runtime-policy.txt, native-launch.log, managed-launch.log and import-inventory.txt. They contain no project tokens.\n\n";
            foreach(var n in new[]{"runtime-policy.txt","native-launch.log","managed-launch.log"}){try{text+="\n"+n+"\n"+await FileIO.ReadTextAsync(await ApplicationData.Current.LocalFolder.GetFileAsync(n));}catch{}}
            var dialog=new ContentDialog{Title="Client loader diagnostics",CloseButtonText="Close",Content=new ScrollViewer{Content=new TextBlock{Text=text,TextWrapping=TextWrapping.Wrap,IsTextSelectionEnabled=true},MaxHeight=500}};await dialog.ShowAsync();
        }
        private async Task RevokeTokenAsync(string token)
        {
            try { using (var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(5))) await api.SendAsync("/api/device/logout", new JsonObject(), token, timeout.Token); } catch { }
        }
        private async void Retry_Click(object sender, RoutedEventArgs e) { if (sessionToken != null) await ValidateSessionAsync(true); else await StartLinkAsync(); }
        private async void Check_Click(object sender, RoutedEventArgs e) { await ValidateSessionAsync(true); }
        private async void Cancel_Click(object sender, RoutedEventArgs e) { await CancelLinkAsync(); }
        private async Task CancelLinkAsync()
        {
            if (sessionToken != null) return;
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
            if (e.Key == VirtualKey.GamepadB || e.Key == VirtualKey.Escape) { e.Handled = true; await CancelLinkAsync(); }
        }
        public void Pause() { ResetOperation(); heartbeat.Stop(); }
        public async void Resume()
        {
            if (!ready || api == null) return;
            if (sessionToken != null) await ValidateSessionAsync(true); else await StartLinkAsync();
        }
    }
}
