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
OEM conversion of the installation support texts, physical drives
(PhysicalDrivesUnit: CD/DVD, floppy and removable drives from /sys/block and
the mounted file systems; CD mounting incl. ASK/NUMBER/LABEL/FILE/FOLDER and
auto mounting, installation support from floppy/CD, quick start drive nodes,
reading/writing CD/floppy images via the device files; without permission
for a device file it is opened via udisks2 (Block.OpenDevice, older udisks:
OpenForBackup/OpenForRestore), inserted but unmounted discs/floppies are
mounted via udisks2 (Filesystem.Mount)), HTML help (data/Help) on all
systems with context id -> page mapping (HelpTools.cpp), GitHub Actions build
checks (Linux + Windows), resample filters for image scaling (ImageStretch.cpp), data packages of the
package manager as .7z archives installed on all systems (DataPackageUnit),
single game packages split from the original packages and published by the
"Game packages" workflow (DFRPackageTool, tools/PackageTool/README.md).

Still open:
- Platform gaps marked with `TODO(port)` in the sources (recursive folder watching in the quick start window,
  search animation, color box names).
  Notes that only explain a deliberate difference are marked `Port note:`.
