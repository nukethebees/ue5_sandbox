#include "SbxMeshGenLab/SbxMeshGenLabSettings.h"

#include "Editor.h"
#include "Editor/SbxMeshGenLabEditorMode.h"
#include "EditorModeManager.h"
#include <CQTest.h>

TEST_CLASS(MeshGenLabUi, "SandboxMesh.UnitTests")
{
    TEST_METHOD(RoundTripsInactiveShapeSettings)
    {
        auto* const settings{NewObject<USbxMeshGenLabSettings>()};
        settings->shape = ESbxMeshShape::Box;
        settings->cylinder_radial_segments = 2;
        auto const request{settings->to_request()};
        if (!TestRunner->TestTrue(TEXT("Ignore inactive shape constraints"), request.has_value())) {
            return;
        }

        auto* const restored{NewObject<USbxMeshGenLabSettings>()};
        restored->load_request(*request);
        TestRunner->TestEqual(
            TEXT("Retain inactive parameters"), restored->cylinder_radial_segments, 2);
        restored->shape = ESbxMeshShape::Cylinder;
        TestRunner->TestFalse(TEXT("Validate the selected shape"),
                              restored->to_request().has_value());
    }

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
