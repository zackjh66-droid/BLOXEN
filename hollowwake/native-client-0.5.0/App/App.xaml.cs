using Windows.ApplicationModel;
using Windows.ApplicationModel.Activation;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
namespace Hollowwake
{
    sealed partial class App : Application
    {
        internal static bool UnsupportedSelfTest;
        internal static bool NativePreflight;
        internal static bool AudioPreflight;
        public App() { InitializeComponent(); Suspending += OnSuspending; Resuming += OnResuming; }
        protected override void OnLaunched(LaunchActivatedEventArgs e)
        {
            NativePreflight=e.Arguments=="--native-preflight";
            AudioPreflight=e.Arguments=="--audio-preflight";
            UnsupportedSelfTest=e.Arguments=="hollowwake-selftest-unsupported";
            var frame = Window.Current.Content as Frame;
            if (frame == null) { frame = new Frame(); Window.Current.Content = frame; }
            if (frame.Content == null) frame.Navigate(typeof(MainPage));
            Window.Current.Activate();
        }
        private void OnSuspending(object sender, SuspendingEventArgs e) { ((Window.Current.Content as Frame)?.Content as MainPage)?.Pause(); }
        private void OnResuming(object sender, object e) { ((Window.Current.Content as Frame)?.Content as MainPage)?.Resume(); }
    }
}
