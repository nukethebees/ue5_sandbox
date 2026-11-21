import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from xml.etree import ElementTree as ET
from zipfile import ZIP_DEFLATED, ZipFile


def package_visual_studio(source, work, server, temp, msbuild):
    for name in ("Client.cs", "Lispb.csproj", "extension.vsixmanifest"):
        shutil.copyfile(source / name, work / name)
    subprocess.run(
        [
            msbuild,
            "Lispb.csproj",
            "-restore",
            "-nologo",
            "-verbosity:minimal",
            "-property:Configuration=Release",
            f"-property:LispbServer={server}",
        ],
        cwd=work,
        check=True,
    )
    shutil.copyfile(work / "bin/Release/net472/Lispb.Client.vsix", temp)


def vscode_manifest(metadata):
    return f"""<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011">
  <Metadata>
    <Identity Language="en-US" Id="{metadata["name"]}" Version="{metadata["version"]}"
              Publisher="{metadata["publisher"]}" TargetPlatform="win32-x64"/>
    <DisplayName>{metadata["displayName"]}</DisplayName>
    <Icon>extension/{metadata["icon"]}</Icon>
    <Description>{metadata["description"]}</Description>
    <Tags>lispb</Tags>
    <Categories>Programming Languages</Categories>
    <Properties>
      <Property Id="Microsoft.VisualStudio.Code.Engine" Value="{metadata["engines"]["vscode"]}"/>
    </Properties>
  </Metadata>
  <Installation><InstallationTarget Id="Microsoft.VisualStudio.Code"/></Installation>
  <Dependencies/>
  <Assets>
    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true"/>
    <Asset Type="Microsoft.VisualStudio.Services.Icons.Default" Path="extension/{metadata["icon"]}" Addressable="true"/>
  </Assets>
</PackageManifest>"""


def install_vscode_dependencies(source, work):
    for name in ("package.json", "package-lock.json"):
        shutil.copyfile(source / name, work / name)
    subprocess.run(
        [
            "npm.cmd",
            "ci",
            "--omit=dev",
            "--ignore-scripts",
            "--no-audit",
            "--no-fund",
            "--cache",
            str(work / ".npm-cache"),
        ],
        cwd=work,
        check=True,
    )


def vscode_files(source, work, server, metadata):
    assets = {Path("package.json"), Path(metadata["main"]), Path(metadata["icon"])}
    for language in metadata["contributes"]["languages"]:
        assets.update(Path(icon) for icon in language.get("icon", {}).values())
    for asset in sorted(assets):
        yield source / asset, "extension/" + asset.as_posix()

    for path in sorted((work / "node_modules").rglob("*")):
        if (
            path.is_file()
            and ".bin" not in path.parts
            and path.name != ".package-lock.json"
        ):
            yield path, "extension/" + path.relative_to(work).as_posix()
    yield server, "extension/bin/lispb-lsp.exe"


def write_vscode_archive(destination, files, manifest):
    with ZipFile(destination, "w", compression=ZIP_DEFLATED) as archive:
        types = ET.Element(
            "Types",
            xmlns="http://schemas.openxmlformats.org/package/2006/content-types",
        )
        ET.SubElement(
            types, "Default", Extension="vsixmanifest", ContentType="text/xml"
        )
        for path, name in files:
            archive.write(path, name)
            ET.SubElement(
                types,
                "Override",
                PartName="/" + name,
                ContentType="image/png"
                if path.suffix == ".png"
                else "application/octet-stream",
            )
        archive.writestr(
            "[Content_Types].xml",
            ET.tostring(types, encoding="utf-8", xml_declaration=True),
        )
        archive.writestr("extension.vsixmanifest", manifest)


def package_vscode(source, work, server, temp):
    install_vscode_dependencies(source, work)
    metadata = json.loads((source / "package.json").read_bytes())
    files = vscode_files(source, work, server, metadata)
    write_vscode_archive(temp, files, vscode_manifest(metadata))


def main():
    editor, executable, destination, *tools = sys.argv[1:]
    source = Path(__file__).resolve().parent / editor
    package = Path(destination).resolve()
    server = Path(executable).resolve()
    work = package.parent / editor
    work.mkdir(parents=True, exist_ok=True)
    temp = package.with_suffix(".tmp")

    if editor == "visualstudio":
        package_visual_studio(source, work, server, temp, *tools)
    else:
        package_vscode(source, work, server, temp)
    os.replace(temp, package)
    print(package)


if __name__ == "__main__":
    main()
