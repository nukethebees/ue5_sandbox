from pathlib import Path
import hashlib
import json
import shutil
import sys
import tempfile
from xml.etree import ElementTree
from zipfile import ZipFile

from session import Client, exercise


def test_package(packages, editor, root, expected, vsix_engine):
    vsix = packages / f"lispb-{editor}.vsix"
    with ZipFile(vsix) as archive:
        archive.extractall(root)
    manifest = ElementTree.parse(root / "extension.vsixmanifest")
    target = manifest.find(".//{*}InstallationTarget")
    if editor == "vscode":
        assert target.get("Id") == "Microsoft.VisualStudio.Code"
        extension = root / "extension"
        package = json.loads((extension / "package.json").read_bytes())
        assert any(language["id"] == "lispb" and ".lispb" in language["extensions"]
                   for language in package["contributes"]["languages"])
        assert "onLanguage:lispb" in package["activationEvents"]
        assert "grammars" not in package["contributes"]
        server = extension / "bin/lispb-lsp.exe"
        command = ["node", str(Path(__file__).with_name("vscode-adapter.js")), str(extension)]
    else:
        assert target.get("Version") == "[18.0,19.0)"
        assert target.find("{*}ProductArchitecture").text == "amd64"
        asset = manifest.find(".//{*}Asset[@Type='Microsoft.VisualStudio.MefComponent']")
        component = root / asset.get("Path")
        assert component.is_file()
        assert not list(root.rglob("*.tmLanguage.json"))
        # The IDE normally supplies the SDK assemblies. Use the package build's resolved SDK.
        for dependency in (packages / "visualstudio/bin/Release/net472").glob("*.dll"):
            if dependency.name != component.name:
                shutil.copyfile(dependency, root / dependency.name)
        server = root / "lispb-lsp.exe"
        command = ["powershell.exe", "-NoProfile", "-NonInteractive", "-File",
                   str(Path(__file__).with_name("visualstudio-adapter.ps1")), str(component), str(vsix), vsix_engine]
    assert hashlib.sha256(server.read_bytes()).digest() == expected
    client = Client(command)
    try:
        fixture = root / "fixture"
        fixture.mkdir()
        exercise(client, fixture)
    finally:
        client.process.kill()
        client.process.wait()
    print(f"{editor}: packaged adapter, server and LSP features including semantic tokens passed", flush=True)


if __name__ == "__main__":
    packages = Path(sys.argv[1]).resolve()
    expected = hashlib.sha256(Path(sys.argv[2]).read_bytes()).digest()
    for editor in ("vscode", "visualstudio"):
        with tempfile.TemporaryDirectory(prefix=f"Lispb {editor} ") as temp:
            test_package(packages, editor, Path(temp), expected, sys.argv[3])
