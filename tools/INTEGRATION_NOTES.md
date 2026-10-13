# Open integration items (follow-up work)

Done: obsolete per-unit workarounds removed, shared help (`ApplicationHelpCommand`),
"HelpContext" property unified, `VclLayout::onLabelClick`/`makeLinkLabel`,
named toolbar separators, VCL auto-size column widths, Linux special
folders (`GetSpecialFolder`), Linux DOSBox/helper program detection,
`TValueListEditorWidget` read-only rows and edit masks, profile editor
tree icons, shared `StartProcessNative` (Process.h), shared `VK_*`
constants and `QtKeyToVK` (VirtualKeys.h), review of the 0-based `PrgConsts`
lists (all users loop over the whole list, as the Delphi code did),
Linux: wallpaper (GNOME, KDE, Xfce, Cinnamon, MATE, LXQt), screensaver
inhibition (D-Bus), DOSBox/ScummVM window centering (SDL variables), DOSBox
conf files with UTF-8 host paths and DOS code page texts (DosCodePage.h),
OEM conversion of the installation support texts.

Still open:
- Platform gaps marked with `TODO(port)` in the sources (physical floppy/CD
  drive access and auto mounting without drive letters, help topics in CHM
  viewers, resample filters, Windows installer packages in the package
  manager, recursive folder watching in the quick start window, search
  animation, color box names).
  Notes that only explain a deliberate difference are marked `Port note:`.
