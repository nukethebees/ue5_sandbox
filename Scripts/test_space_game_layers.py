import unittest

from Scripts.check_space_game_layers import FORBIDDEN, dependencies


class LayerChecks(unittest.TestCase):
    def test_rejects_includes_and_non_include_bindings(self) -> None:
        for text in (
            '#include <SpaceGamePresentation/presentation/LevelPresentation.h>',
            '#include "SpaceGame/simulation/TestBatchOrchestrator.h"',
            'friend struct FLevelPresentation;',
            'TOptional<FLevelPresentation> output;',
            'class UNiagaraComponent;',
            '#include <SandboxGameShared/utilities/actor_utils.h>',
        ):
            with self.subTest(text=text):
                self.assertIsNotNone(FORBIDDEN.search(text))

    def test_accepts_neutral_outputs(self) -> None:
        self.assertIsNone(FORBIDDEN.search('TConstArrayView<FEntityFrameChange> changes;'))

    def test_dependency_lists_and_single_additions(self) -> None:
        self.assertEqual(
            dependencies('''
                PublicDependencyModuleNames.AddRange(new string[] { "Core", "SGCollision" });
                PrivateDependencyModuleNames.Add("SpaceGamePresentation");
                // PrivateDependencyModuleNames.Add("Ignored");
            '''),
            {"Core", "SGCollision", "SpaceGamePresentation"},
        )


if __name__ == "__main__":
    unittest.main()
