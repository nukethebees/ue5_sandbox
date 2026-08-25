# SandboxUI showcase

`WBP_SandboxUIShowcase` is an editor-only visual catalogue of the reusable Slate widgets supplied
by SandboxUI. Its live example data and generated layout are implemented by
`USandboxUIShowcase` in the `SandboxUIExamples` editor module.

For an interactive view, right-click `EUW_SandboxUIShowcase` and choose **Run Editor Utility
Widget**. The utility embeds `WBP_SandboxUIShowcase`, so both entry points display the same gallery
and example data. Use the WBP for normal UMG inspection and the EUW for an interactive, scrollable
editor tab.

When a reusable SandboxUI widget is added, a representative live example should normally be added
to this gallery as well. Keep example-only data and presentation in `SandboxUIExamples`; production
widgets must not depend on the showcase.
