import json
from pathlib import Path
import subprocess
import sys
import tempfile


class Client:
    def __init__(self, server):
        self.process = subprocess.Popen(
            server if isinstance(server, list) else [server],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
        )
        self.request_id = 0
        self.problems = {}

    def send(self, method, params, request=False):
        message = {"jsonrpc": "2.0", "method": method, "params": params}
        if request:
            self.request_id += 1
            message["id"] = self.request_id
        body = json.dumps(message, ensure_ascii=False).encode()
        self.process.stdin.write(f"Content-Length: {len(body)}\r\n\r\n".encode() + body)
        self.process.stdin.flush()
        if not request:
            return
        while True:
            headers = {}
            while True:
                line = self.process.stdout.readline()
                if not line:
                    raise AssertionError(f"server exited: {self.process.poll()}")
                if line == b"\r\n":
                    break
                key, value = line.split(b":", 1)
                headers[key.lower()] = value.strip()
            response = json.loads(
                self.process.stdout.read(int(headers[b"content-length"]))
            )
            if response.get("method") == "textDocument/publishDiagnostics":
                event = response["params"]
                self.problems[event["uri"]] = event["diagnostics"]
            elif response.get("id") == self.request_id:
                assert "result" in response, response
                return response["result"]

    def open(self, path, text):
        self.send(
            "textDocument/didOpen",
            {
                "textDocument": {
                    "uri": path.as_uri(),
                    "languageId": "lispb",
                    "text": text,
                    "version": 1,
                }
            },
        )

    def edit(self, path, text, version):
        self.send(
            "textDocument/didChange",
            {
                "textDocument": {"uri": path.as_uri(), "version": version},
                "contentChanges": [{"text": text}],
            },
        )

    def query(self, method, path, position=None):
        params = {"textDocument": {"uri": path.as_uri()}}
        if position is not None:
            params["position"] = position
        return self.send("textDocument/" + method, params, True)


def position(text, token, skip=0):
    before = text[: text.index(token) + skip]
    return {
        "line": before.count("\n"),
        "character": len(before.split("\n")[-1].encode("utf-16-le")) // 2,
    }


def create_project(root):
    fixture = Path(__file__).resolve().parents[2]
    kernel_text = (fixture / "kernel/tests/constants.lispb").read_text()
    schema = '(module sample :header "data.h"\n  (record Item (member count @scalar)))'
    registry = '(include "scalars.lispb")'
    scalar = '(type scalar :spelling "unsigned int" :header "stdint.h")'
    packed = """(module wire :header "wire.h" :namespace wire
 (enum Tag std::uint8_t :bit-width 2 (value Left) (value Right) (projections (cpp)))
 (integer-scalar Small :bit-width 3)
 (union Payload (case Left @Small) (case Right @Small))
 (record Packet :layout packed :bits 8
   (member kind @Tag)
   (member body @Payload :tag kind)
   (projections (cpp :storage std::uint8_t))))"""
    material = """(material M_Test (asset "/Game/Generated/Materials/M_Test")
 (domain surface) (blend opaque) (shading default-lit)
 (parameter scalar Intensity 1.0)
 (parameter color Tint 1.0 0.25 0.5 1.0)
 (parameter texture Mask "/Engine/EngineResources/WhiteSquareTexture" :sampler linear-grayscale)
 (emissive (float3 Intensity Intensity Intensity)))"""
    slate = """(include "macros.lispb")
(widget-class STest (function Build (params (value title)) (caption title)))"""
    macros = "(defmacro caption (text) (STextBlock :Text $text))"
    project = """(lispb-project :language-version 1 :project-root "."
 (cpp-schema data :types "types.lispb" :sources ("data.lispb" "packed.lispb") :output-root (project-path "out"))
 (kernel ops :sources ("ops.lispb") :output-root (project-path "out"))
 (slate ui :sources ("ui.lispb") :output-root (project-path "out"))
 (material mat :source "mat.lispb" :artifact (build-path "test.bin"))
 (group combined :targets (ops ui)))"""
    files = {
        "project": project,
        "types": registry,
        "scalars": scalar,
        "packed": packed,
        "data": schema,
        "ops": kernel_text,
        "ui": slate,
        "mat": material,
        "macros": macros,
    }
    paths = {key: root / (key + ".lispb") for key in files}
    for key, text in files.items():
        paths[key].write_text(text, encoding="utf-8")
    return files, paths


def semantic_tokens(client, path, legend, bounds=None):
    params = {"textDocument": {"uri": path.as_uri()}}
    method = "textDocument/semanticTokens/full"
    if bounds is not None:
        params["range"] = bounds
        method = "textDocument/semanticTokens/range"
    data = client.send(method, params, True)["data"]
    line = column = 0
    tokens = []
    for delta, start, length, kind, modifiers in zip(*[iter(data)] * 5, strict=True):
        line += delta
        column = start if delta else column + start
        assert length and (
            not tokens
            or line > tokens[-1][0]
            or column >= tokens[-1][1] + tokens[-1][2]
        )
        tokens.append((line, column, length, legend[kind], modifiers))
    return tokens


def exercise(client, root, hover_formats=()):
    files, paths = create_project(root)
    schema = files["data"]
    scalar = files["scalars"]
    capabilities = {
        "documentSymbol": {"hierarchicalDocumentSymbolSupport": True},
        "semanticTokens": {
            "requests": {"full": True, "range": True},
            "formats": ["relative"],
            "tokenTypes": ["namespace", "struct", "type", "parameter", "keyword"],
            "tokenModifiers": ["declaration"],
        },
    }
    if hover_formats:
        capabilities["hover"] = {"contentFormat": hover_formats}
    result = client.send(
        "initialize",
        {
            "processId": None,
            "rootUri": root.as_uri(),
            "capabilities": {"textDocument": capabilities},
        },
        True,
    )
    provider = result["capabilities"]["semanticTokensProvider"]
    assert provider["full"] and provider["range"]
    assert provider["legend"]["tokenModifiers"] == ["declaration"]
    legend = provider["legend"]["tokenTypes"]
    assert result["capabilities"]["definitionProvider"]
    client.send("initialized", {})
    for key in ("project", "data", "ops", "ui", "mat"):
        client.open(paths[key], files[key])

    markup = hover_formats[0] if hover_formats else "plaintext"

    def hover(path, text, token, *fields, skip=0):
        start = position(text, token, skip)
        response = client.query("hover", path, start)
        assert response and response["contents"]["kind"] == markup, response
        assert response["range"]["start"] == start, response
        value = response["contents"]["value"]
        for label, expected in fields:
            field = (
                f"**{label}:** ` {expected} `"
                if markup == "markdown"
                else f"{label}: {expected}"
            )
            assert field in value, (field, value)
        return value

    cpp = ("C++ type", "unsigned int")
    header = ("Header", "stdint.h")
    hover(paths["data"], schema, "@scalar", cpp, header)
    client.open(paths["scalars"], scalar)
    hover(paths["scalars"], scalar, "scalar", cpp, header)
    client.send(
        "textDocument/didClose", {"textDocument": {"uri": paths["scalars"].as_uri()}}
    )
    mat = files["mat"]
    fields = (("Parameter type", "scalar"), ("Default", "1"))
    declaration = hover(paths["mat"], mat, "Intensity", *fields)
    assert declaration == hover(paths["mat"], mat, "float3 Intensity", *fields, skip=7)
    hover(
        paths["mat"],
        mat,
        "Tint",
        ("Parameter type", "color"),
        ("Default", "1, 0.25, 0.5, 1"),
    )
    hover(
        paths["mat"],
        mat,
        "Mask",
        ("Parameter type", "texture"),
        (
            "Default texture",
            "/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture",
        ),
        ("Sampler", "linear-grayscale"),
    )

    packed = files["packed"]
    client.open(paths["packed"], packed)
    layout_fields = (
        ("Bit offset", "2"),
        ("Bit width", "3"),
        ("Owner storage", "8 bits"),
        ("Discriminator", "kind"),
        ("Active case", "body.Left when kind = 0"),
        ("Active case", "body.Right when kind = 1"),
    )
    hover(paths["packed"], packed, "body", *layout_fields)
    hover(
        paths["packed"], packed, "@Tag", ("C++ type", "wire::Tag"), ("Header", "wire.h")
    )
    loose = root / "loose.lispb"
    client.open(loose, packed)
    hover(loose, packed, "body", *layout_fields)
    client.send("textDocument/didClose", {"textDocument": {"uri": loose.as_uri()}})
    symbols = client.query("documentSymbol", paths["data"])
    assert symbols[0]["children"][0]["name"] == "Item", symbols
    assert client.query("foldingRange", paths["data"])[0]["endLine"] == 1
    for key, use, definition, target in (
        ("data", "@scalar", "scalar", "scalars"),
        ("ops", "(types floating)", "floating", "ops"),
        ("mat", "float3 Intensity", "Intensity", "mat"),
        ("ui", "caption title", "title", "ui"),
        ("project", "(ops ui)", "ops", "project"),
    ):
        skip = {"data": 1, "ops": 7, "ui": 8, "mat": 7, "project": 1}[key]
        pos = position(files[key], use, skip)
        location = client.query("definition", paths[key], pos)
        assert location and Path(location["uri"]).name == paths[target].name, (
            key,
            location,
            client.problems,
        )
        expected = position(files[target], definition)
        assert location["range"]["start"] == expected, (key, location)
        assert definition in client.query("hover", paths[key], pos)["contents"]["value"]
    for key in ("project", "data", "ops", "ui", "mat"):
        assert not client.problems.get(paths[key].as_uri()), (key, client.problems)

    tokens = semantic_tokens(client, paths["data"], legend)
    for name, kind, flags in (
        ("Item", "struct", 1),
        ("count", "property", 1),
        ("@scalar", "type", 0),
    ):
        pos = position(schema, name)
        assert (pos["line"], pos["character"], len(name), kind, flags) in tokens, tokens
    bounds = {"start": {"line": 1, "character": 0}, "end": {"line": 2, "character": 0}}
    assert semantic_tokens(client, paths["data"], legend, bounds) == [
        t for t in tokens if t[0] == 1
    ]
    mat_tokens = semantic_tokens(client, paths["mat"], legend)
    assert any(kind == "parameter" and not flags for _, _, _, kind, flags in mat_tokens)
    pos = position(files["project"], "(ops ui)", 1)
    assert (pos["line"], pos["character"], 3, "namespace", 0) in semantic_tokens(
        client, paths["project"], legend
    )
    bad_material = files["mat"].replace("float3 Intensity", "float3 Missing")
    client.edit(paths["mat"], bad_material, 2)
    client.query("documentSymbol", paths["mat"])
    assert client.problems[paths["mat"].as_uri()]
    client.edit(paths["mat"], files["mat"], 3)
    client.query("documentSymbol", paths["mat"])
    assert not client.problems[paths["mat"].as_uri()]
    edit = mat.replace("Intensity 1.0", "Intensity 2.5")
    client.edit(paths["mat"], edit, 4)
    hover(paths["mat"], edit, "float3 Intensity", ("Default", "2.5"), skip=7)

    macro = client.query("definition", paths["ui"], position(files["ui"], "caption"))
    assert macro["uri"] == paths["macros"].as_uri()
    included = client.query(
        "definition", paths["ui"], position(files["ui"], "macros.lispb")
    )
    assert included["uri"] == paths["macros"].as_uri()
    client.open(paths["macros"], files["macros"])
    parameter = client.query(
        "definition", paths["macros"], position(files["macros"], "$text", 1)
    )
    assert parameter["range"]["start"] == position(files["macros"], "text")

    client.open(
        paths["scalars"], "; edited 😀\n" + scalar.replace("stdint.h", "edited.h")
    )
    use = position(schema, "@scalar", 2)
    hover(paths["data"], schema, "@scalar", ("Header", "edited.h"))
    assert client.query("definition", paths["data"], use)["range"]["start"]["line"] == 1
    client.send(
        "textDocument/didClose", {"textDocument": {"uri": paths["scalars"].as_uri()}}
    )
    assert client.query("definition", paths["data"], use)["range"]["start"]["line"] == 0
    hover(paths["data"], schema, "@scalar", header)

    broken = schema.replace("@scalar", "@missing")
    client.edit(paths["data"], broken, 2)
    assert client.query("hover", paths["data"], position(broken, "@missing")) is None
    client.query("documentSymbol", paths["data"])
    issues = client.problems[paths["data"].as_uri()]
    assert issues and issues[0]["range"]["start"] == position(broken, "@missing"), (
        issues
    )
    fixed = schema.replace('"data.h"', '"😀.h"').replace("\n", " ")
    client.edit(paths["data"], fixed, 3)
    assert client.query("definition", paths["data"], position(fixed, "@scalar", 1))
    assert not client.problems[paths["data"].as_uri()]
    partial = schema[: schema.index("@scalar")] + "@sc"
    client.edit(paths["data"], partial, 4)
    items = client.query("completion", paths["data"], position(partial, "@sc", 3))
    assert "@scalar" in [item["label"] for item in items], items
    for version in range(5, 12):
        client.edit(paths["data"], schema + "\n" * (version % 3), version)
        assert client.query("definition", paths["data"], use)
        assert client.query("definition", paths["data"], use)
    scratch = root / "scratch \u5024 #%.lispb"
    client.open(scratch, '(example "😀" :flag #cpp{a\r\nb}cpp# ; note\r\n  1)')
    tokens = semantic_tokens(client, scratch, legend)
    assert (0, 14, 5, "keyword", 0) in tokens, tokens
    assert (1, 0, 6, "string", 0) in tokens, tokens
    assert (1, 7, 6, "comment", 0) in tokens, tokens
    client.edit(scratch, '(example "bad\\q" :after', 2)
    assert (0, 17, 6, "keyword", 0) in semantic_tokens(client, scratch, legend)
    client.edit(scratch, broken, 3)
    client.edit(scratch, schema, 2)
    client.query("documentSymbol", scratch)
    issues = client.problems[scratch.as_uri()]
    assert issues and issues[0]["range"]["start"] == position(broken, "@missing"), (
        issues
    )
    client.send("textDocument/didClose", {"textDocument": {"uri": scratch.as_uri()}})
    assert client.query("documentSymbol", scratch) is None
    assert not client.problems[scratch.as_uri()]
    assert not (root / "out").exists()
    client.send("shutdown", {}, True)
    client.send("exit", {})
    assert client.process.wait(timeout=5) == 0


if __name__ == "__main__":
    client = Client(sys.argv[1])
    try:
        with tempfile.TemporaryDirectory(prefix="Lispb editor ") as temp:
            exercise(client, Path(temp))
    finally:
        client.process.kill()
        client.process.wait()
