#include "Editor/SbxMeshGenLabEditorMode.h"

#include "Editor.h"
#include "EditorModeManager.h"

#include <CQTest.h>

TEST_CLASS(MeshGenLabUi, "SandboxMesh.UnitTests")
{
    TEST_METHOD(ActivatesAndDeactivatesTheLabEditorMode)
    {
        auto& mode_tools{GLevelEditorModeTools()};
        mode_tools.ActivateMode(USbxMeshGenLabEditorMode::mode_id);

        TestRunner->TestTrue(TEXT("Sandbox Mesh editor mode activates"),
                             mode_tools.IsModeActive(USbxMeshGenLabEditorMode::mode_id));

        mode_tools.DeactivateMode(USbxMeshGenLabEditorMode::mode_id);
        TestRunner->TestFalse(TEXT("Sandbox Mesh editor mode deactivates"),
                              mode_tools.IsModeActive(USbxMeshGenLabEditorMode::mode_id));
    }
};
