import json
from pathlib import Path

phases = ["generate-code"]
Path("phases.txt").write_text("presets\n", encoding="utf-8", newline="\n")
Path("CMakePresets.json").write_text(json.dumps({
    "version": 6,
    "configurePresets": [
        {
            "name": phase,
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/out/" + phase,
            "cacheVariables": {"PHASE": phase},
        }
        for phase in phases
    ],
    "workflowPresets": [
        {"name": phase, "steps": [{"type": "configure", "name": phase}]}
        for phase in phases
    ],
}), encoding="utf-8")
