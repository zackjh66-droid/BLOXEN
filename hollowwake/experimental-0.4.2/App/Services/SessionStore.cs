using System;
using System.Threading.Tasks;
using Windows.Data.Json;
using Windows.Security.Cryptography;
using Windows.Security.Cryptography.DataProtection;
using Windows.Storage;
namespace Hollowwake.Services
{
    public sealed class SessionStore
    {
        private const string Filename = "device-session.protected";
        public async Task SaveAsync(string origin, string token)
        {
            var json = ApiClient.Body("origin", origin, "token", token).Stringify();
            var buffer = CryptographicBuffer.ConvertStringToBinary(json, BinaryStringEncoding.Utf8);
            var encrypted = await new DataProtectionProvider("LOCAL=user").ProtectAsync(buffer);
            var file = await ApplicationData.Current.LocalFolder.CreateFileAsync(Filename, CreationCollisionOption.ReplaceExisting);
            await FileIO.WriteBufferAsync(file, encrypted);
        }
        public async Task<string> LoadAsync(string origin)
        {
            try
            {
                var file = await ApplicationData.Current.LocalFolder.GetFileAsync(Filename);
                var data = await FileIO.ReadBufferAsync(file);
                var decrypted = await new DataProtectionProvider().UnprotectAsync(data);
                var json = JsonObject.Parse(CryptographicBuffer.ConvertBinaryToString(BinaryStringEncoding.Utf8, decrypted));
                if (!String.Equals(json.GetNamedString("origin"), origin, StringComparison.Ordinal)) return null;
                return json.GetNamedString("token");
            }
            catch { return null; } // Expired user protection context/corrupt store => re-link, never expose data.
        }
        public async Task ClearAsync()
        {
            var item = await ApplicationData.Current.LocalFolder.TryGetItemAsync(Filename);
            if (item != null) await item.DeleteAsync(StorageDeleteOption.PermanentDelete);
        }
    }
}
