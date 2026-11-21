const vscode = require("vscode");
const { LanguageClient, TransportKind } = require("vscode-languageclient/node");

let client;

async function activate(context) {
    const watcher = vscode.workspace.createFileSystemWatcher("**/*.lispb");
    context.subscriptions.push(watcher);
    client = new LanguageClient("lispb", "Lispb", {
        command: context.asAbsolutePath("bin/lispb-lsp.exe"),
        transport: TransportKind.stdio
    }, {
        documentSelector: [{ scheme: "file", language: "lispb" }],
        synchronize: { fileEvents: watcher }
    });
    await client.start();
}

async function deactivate() {
    if (client) {
        await client.stop();
    }
}

module.exports = { activate, deactivate };
