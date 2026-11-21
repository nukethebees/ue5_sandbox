const assert = require("node:assert/strict");
const { spawn } = require("node:child_process");
const path = require("node:path");
const fs = require("node:fs");
const { createRequire } = require("node:module");
const vm = require("node:vm");

const root = process.argv[2];
const entry = path.resolve(root, require(path.join(root, "package.json")).main);
const extensionRequire = createRequire(entry);
extensionRequire.resolve("vscode-languageclient/node");

const watcher = {};
const vscode = { workspace: { createFileSystemWatcher(pattern) {
    assert.equal(pattern, "**/*.lispb");
    return watcher;
} } };
const TransportKind = { stdio: Symbol("stdio") };
class LanguageClient {
    constructor(id, displayName, server, options) {
        assert.equal(id, "lispb");
        assert.deepEqual([...options.documentSelector].map(item => item.language), ["lispb"]);
        assert.equal(server.transport, TransportKind.stdio);
        assert.equal(options.synchronize.fileEvents, watcher);
        this.command = server.command;
    }
    start() {
        const child = spawn(this.command, { stdio: "inherit", windowsHide: true });
        child.on("exit", code => { process.exitCode = code ?? 1; });
    }
}

const extension = { exports: {} };
vm.runInNewContext(fs.readFileSync(entry, "utf8"), {
    module: extension,
    require(name) {
        if (name === "vscode") { return vscode; }
        if (name === "vscode-languageclient/node") { return { LanguageClient, TransportKind }; }
        return extensionRequire(name);
    }
}, { filename: entry });
extension.exports.activate({ subscriptions: [], asAbsolutePath: file => path.resolve(root, file) });
