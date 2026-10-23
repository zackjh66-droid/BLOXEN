using Windows.ApplicationModel;
using Windows.ApplicationModel.Activation;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
namespace Hollowwake
{
    sealed partial class App : Application
    {
        internal static bool PreviousLauncherBootInterrupted;
        internal static string ExecutionStage="launcher";
        internal static bool UnsupportedSelfTest;
        internal static bool NativePreflight;
        internal static bool AudioPreflight;
        internal static bool WglPreflight;
        internal static void StartupStage(string stage){
            ExecutionStage=stage;
            try{System.IO.File.AppendAllText(System.IO.Path.Combine(Windows.Storage.ApplicationData.Current.LocalFolder.Path,"launcher-startup.log"),System.DateTimeOffset.UtcNow.ToString("o")+" version="+Services.ClientDiagnostics.Version()+" stage="+stage+"\n");}catch{}
        }
        public App() { RequiresPointerMode=ApplicationRequiresPointerMode.WhenRequested; UnhandledException += OnUnhandledException; InitializeComponent(); Suspending += OnSuspending; Resuming += OnResuming; }
        private void OnUnhandledException(object sender,UnhandledExceptionEventArgs e){
            // Preserve the failure; do not mark the exception handled or fake recovery.
            try{var root=Windows.Storage.ApplicationData.Current.LocalFolder.Path;System.IO.File.AppendAllText(System.IO.Path.Combine(root,"managed-launch.log"),System.DateTimeOffset.UtcNow.ToString("o")+" CONTENT_FAILED HRESULT="+e.Exception.HResult.ToString("X8")+" code="+(ExecutionStage=="content-verification"?2:ExecutionStage=="windows-storage-verification"?3:ExecutionStage=="native-window-binding"?4:ExecutionStage=="native-client-entry"?5:1)+" stage="+ExecutionStage+" type="+e.Exception.GetType().FullName+" "+Services.ManagedFailure.Describe(e.Exception,e.Message)+"\n");}catch{}
        }
        protected override void OnLaunched(LaunchActivatedEventArgs e)
        {
            StartupStage("app-activation");
            NativePreflight=e.Arguments=="--native-preflight";
            AudioPreflight=e.Arguments=="--audio-preflight";
            WglPreflight=e.Arguments=="--wgl-preflight";
            UnsupportedSelfTest=e.Arguments=="hollowwake-selftest-unsupported";
            var frame = Window.Current.Content as Frame;
            if (frame == null) { frame = new Frame(); Window.Current.Content = frame; }
            if (frame.Content == null) frame.Navigate(typeof(MainPage));
            Window.Current.Activate();
            StartupStage("window-activated");
        }
        private void OnSuspending(object sender, SuspendingEventArgs e) { ((Window.Current.Content as Frame)?.Content as MainPage)?.Pause(); }
        private void OnResuming(object sender, object e) { ((Window.Current.Content as Frame)?.Content as MainPage)?.Resume(); }
    }
}
