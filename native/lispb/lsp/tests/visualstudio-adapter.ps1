param([Parameter(Mandatory)][string] $Component,
      [Parameter(Mandatory)][string] $Package,
      [Parameter(Mandatory)][string] $VsixEngine)
$ErrorActionPreference = 'Stop'
$reader = [Reflection.Assembly]::LoadFrom($VsixEngine).GetType(
    'Microsoft.VisualStudio.ExtensionManager.InstallableExtensionImpl', $true)
$extension = [Activator]::CreateInstance($reader, @($Package, [Globalization.CultureInfo]::CurrentUICulture))
if ($extension.Header.Identifier -ne 'nukethebees.Lispb.VisualStudio') {
    throw "Unexpected VSIX identity in $Package"
}

Add-Type -TypeDefinition @'
using System.IO;
using System.Threading.Tasks;
public static class PipeRelay {
    public static Task Start(Stream input, Stream output) {
        return Task.Run(async () => {
            byte[] buffer = new byte[8192];
            int count;
            while ((count = await input.ReadAsync(buffer, 0, buffer.Length)) != 0) {
                await output.WriteAsync(buffer, 0, count);
                await output.FlushAsync();
            }
        });
    }
}
'@
$assembly = [Reflection.Assembly]::LoadFrom($Component)
$client = $assembly.CreateInstance('Lispb.Client')
$registration = $assembly.GetType('Lispb.ContentTypes')
$extension = $registration.GetField('Extension').CustomAttributes
if ('.lispb' -notin $extension.ConstructorArguments.Value) {
    throw 'Missing Lispb file extension registration'
}
$connection = $client.ActivateAsync([Threading.CancellationToken]::None).GetAwaiter().GetResult()
try {
    $send = [PipeRelay]::Start([Console]::OpenStandardInput(), $connection.Writer)
    [PipeRelay]::Start($connection.Reader, [Console]::OpenStandardOutput()).GetAwaiter().GetResult()
} finally {
    $connection.Dispose()
}
