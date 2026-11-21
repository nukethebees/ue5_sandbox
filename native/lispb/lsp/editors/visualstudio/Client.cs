using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.LanguageServer.Client;
using Microsoft.VisualStudio.Threading;
using Microsoft.VisualStudio.Utilities;

namespace Lispb
{
    internal static class ContentTypes
    {
        [Export, Name("Lispb"), BaseDefinition(CodeRemoteContentDefinition.CodeRemoteBaseTypeName)]
        public static readonly ContentTypeDefinition Lispb = null;

        [Export, FileExtension(".lispb"), ContentType("Lispb")]
        public static readonly FileExtensionToContentTypeDefinition Extension = null;
    }

    [Export(typeof(ILanguageClient)), ContentType("Lispb")]
    public sealed class Client : ILanguageClient
    {
        public string Name => "Lispb";
        public IEnumerable<string> ConfigurationSections => null;
        public object InitializationOptions => null;
        public IEnumerable<string> FilesToWatch => new[] { "**/*.lispb" };
        public bool ShowNotificationOnInitializeFailed => true;
        public event AsyncEventHandler<EventArgs> StartAsync;
        public event AsyncEventHandler<EventArgs> StopAsync { add { } remove { } }
        public Task OnLoadedAsync() => StartAsync.InvokeAsync(this, EventArgs.Empty);
        public Task OnServerInitializedAsync() => Task.CompletedTask;
        public Task<InitializationFailureContext> OnServerInitializeFailedAsync(ILanguageClientInitializationInfo info)
            => Task.FromResult(new InitializationFailureContext { FailureMessage = info.StatusMessage });

        public Task<Connection> ActivateAsync(CancellationToken token)
        {
            token.ThrowIfCancellationRequested();
            var directory = Path.GetDirectoryName(typeof(Client).Assembly.Location);
            var info = new ProcessStartInfo(Path.Combine(directory, "lispb-lsp.exe"))
            {
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardInput = true,
                RedirectStandardOutput = true
            };
            var server = Process.Start(info);
            return Task.FromResult(new Connection(server.StandardOutput.BaseStream, server.StandardInput.BaseStream));
        }
    }
}
