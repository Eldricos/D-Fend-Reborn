# Open integration items (follow-up work)

Done: obsolete per-unit workarounds removed, shared help (`ApplicationHelpCommand`),
"HelpContext" property unified, `VclLayout::onLabelClick`/`makeLinkLabel`,
named toolbar separators, VCL auto-size column widths.

Still open:
- Shared `StartProcessNative` helper in Process.h (copies in DosBoxUnit, ScummVMUnit, WindowsProfileUnit).
- `GetSpecialFolder` (CommonTools): map CSIDL values to QStandardPaths on non-Windows; check CSIDL constants vs. <shlobj.h> on Windows.
- Shared `VK_*` key constants in compat (several units define their own).
- `TValueListEditorWidget`: read-only rows / EditMask support (ProfileEditorFormUnit emulates locally).
- `PrgConsts` array constants are 0-based now (Delphi: 1-based) – review users.
- Windows build has not been compiled yet (all `#ifdef Q_OS_WIN` code paths).
- Default DOSBox path detection on Linux (`dosbox` in PATH) for the first run wizard.
- Tree node icons in the modern profile editor.
