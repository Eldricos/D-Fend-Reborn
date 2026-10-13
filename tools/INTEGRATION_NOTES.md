# Open integration items (follow-up work)

Done: obsolete per-unit workarounds removed, shared help (`ApplicationHelpCommand`),
"HelpContext" property unified, `VclLayout::onLabelClick`/`makeLinkLabel`,
named toolbar separators, VCL auto-size column widths, Linux special
folders (`GetSpecialFolder`), Linux DOSBox/helper program detection,
`TValueListEditorWidget` read-only rows and edit masks, profile editor
tree icons.

Still open:
- Shared `StartProcessNative` helper in Process.h (copies in DosBoxUnit, ScummVMUnit, WindowsProfileUnit).
- Shared `VK_*` key constants in compat (several units define their own).
- `PrgConsts` array constants are 0-based now (Delphi: 1-based) – review users.
