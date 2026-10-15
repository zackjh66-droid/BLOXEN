using Windows.ApplicationModel;
using Windows.ApplicationModel.Activation;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
namespace Hollowwake
{
    sealed partial class App : Application
    {
        public App() { InitializeComponent(); Suspending += OnSuspending; Resuming += OnResuming; }
        protected override void OnLaunched(LaunchActivatedEventArgs e)
        {
            var frame = Window.Current.Content as Frame;
            if (frame == null) { frame = new Frame(); Window.Current.Content = frame; }
            if (frame.Content == null) frame.Navigate(typeof(MainPage));
            Window.Current.Activate();
        }
        private void OnSuspending(object sender, SuspendingEventArgs e) { ((Window.Current.Content as Frame)?.Content as MainPage)?.Pause(); }
        private void OnResuming(object sender, object e) { ((Window.Current.Content as Frame)?.Content as MainPage)?.Resume(); }
    }
}
