using System;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Windows.Data.Json;
namespace Hollowwake.Services
{
    public sealed class ApiException : Exception
    {
        public string Code { get; private set; }
        public HttpStatusCode Status { get; private set; }
        public ApiException(string code, HttpStatusCode status) : base(code) { Code = code; Status = status; }
    }
    public sealed class ApiClient : IDisposable
    {
        private readonly HttpClient client;
        public string Origin { get; private set; }
        public ApiClient(string origin, bool allowLocalHttp)
        {
            var uri = new Uri(origin, UriKind.Absolute);
            bool loopback = uri.IsLoopback;
            if ((uri.Scheme != "https" && !(allowLocalHttp && loopback && uri.Scheme == "http")) || uri.AbsolutePath != "/" || uri.Query.Length > 0 || uri.Fragment.Length > 0 || uri.UserInfo.Length > 0)
                throw new ArgumentException("Enter an HTTPS origin with no path, credentials or query.");
            Origin = uri.GetLeftPart(UriPartial.Authority);
            var handler = new HttpClientHandler { AllowAutoRedirect = false, UseCookies = false };
            client = new HttpClient(handler) { Timeout = TimeSpan.FromSeconds(20), BaseAddress = new Uri(Origin) };
        }
        public async Task<JsonObject> SendAsync(string path, JsonObject body, string token, CancellationToken cancellationToken)
        {
            using (var request = new HttpRequestMessage(body == null ? HttpMethod.Get : HttpMethod.Post, path))
            {
                if (token != null) request.Headers.Authorization = new System.Net.Http.Headers.AuthenticationHeaderValue("Bearer", token);
                if (body != null) request.Content = new StringContent(body.Stringify(), Encoding.UTF8, "application/json");
                using (var response = await client.SendAsync(request, cancellationToken))
                {
                    if (response.Content.Headers.ContentLength > 1048576) throw new ApiException("invalid_response", response.StatusCode);
                    var text = await response.Content.ReadAsStringAsync();
                    if (text.Length > 1048576 || !JsonObject.TryParse(text, out var data)) throw new ApiException("invalid_response", response.StatusCode);
                    if (!response.IsSuccessStatusCode) throw new ApiException(data.GetNamedString("error", "request_failed"), response.StatusCode);
                    return data;
                }
            }
        }
        public static JsonObject Body(params string[] pairs)
        {
            var result = new JsonObject();
            for (int i = 0; i < pairs.Length; i += 2) result[pairs[i]] = JsonValue.CreateStringValue(pairs[i + 1]);
            return result;
        }
        public void Dispose() { client.Dispose(); }
    }
}
