// VCL layout engine and runtime fix-ups for the generated .ui files.
//
// The .ui files created by tools/dfm2ui.py position every control at its
// original VCL pixel position.  VCL specific information is stored in dynamic
// properties (vclAlign, vclAnchors, vclModalResult ...).  setupForm() evaluates
// those properties after Ui::setupUi() has run:
//
//   * Align (alTop/alBottom/alLeft/alRight/alClient) and Anchors are emulated
//     exactly like TWinControl.AlignControls does it, so resizable forms behave
//     like the original ones.
//   * TSplitter, TToolBar embedded controls, tool button drop down menus,
//     popup menus (PopupMenu property), named menu separators, radio menu
//     items, BitBtn Kind glyphs, ModalResult buttons, status bar panels,
//     hidden tab sheets, label FocusControl and AutoSize, column widths and the
//     form's BorderStyle/Position are applied.
#pragma once

#include <QString>
#include <QList>
#include <QIcon>
#include <functional>

class QWidget;
class QStatusBar;
class QAction;
class QMenu;
class QTreeWidget;
class QTabWidget;

namespace VclLayout {

// Call directly after ui.setupUi(this) for forms, frames and the main window.
void setupForm(QWidget *Top);

// Re-run the align/anchor layout of a container (normally automatic).
void relayout(QWidget *Container);

// Set the width (alLeft/alRight) or height (alTop/alBottom) of an aligned
// control, like assigning Width/Height of an aligned VCL control.
void setAlignedExtent(QWidget *Control, int Extent);

// Change a control's bounds and update its anchor rules (VCL SetBounds).
void setBounds(QWidget *Control, int Left, int Top, int Width, int Height);

// Resize labels with AutoSize=True to their text (called automatically when
// the form is shown; call it after changing captions of a visible form).
void autoSizeLabels(QWidget *Root);

// ClientWidth/ClientHeight assignment that respects bsDialog fixed sizes.
void setClientSize(QWidget *Form, int Width, int Height);

// Delphi TForm.KeyPreview: Handler is called for every key press of any child
// control before the control gets it.  Return true to consume the key.
void installKeyPreview(QWidget *Form, const std::function<bool(int Key, Qt::KeyboardModifiers Mods)> &Handler);

// TStatusBar helpers (panels are created by setupForm from vclPanelWidths)
void setStatusText(QStatusBar *Bar, int Panel, const QString &Text);   // Panels[Panel].Text
QString statusText(QStatusBar *Bar, int Panel);
void setSimpleText(QStatusBar *Bar, const QString &Text);               // SimpleText
void setStatusPanelWidth(QStatusBar *Bar, int Panel, int Width);

// Image list of a form as exported by dfm2ui ("MainUnit", "ImageList").
QList<QIcon> imageList(const QString &Unit, const QString &Name);
QIcon imageListIcon(const QString &Unit, const QString &Name, int Index);

// TabSheet.TabVisible
void setTabVisible(QTabWidget *Tabs, QWidget *Page, bool Visible);
// TabSheet.PageIndex / ActivePage helpers
void setActivePage(QTabWidget *Tabs, QWidget *Page);

// Icon of a TBitBtn Kind (bkOK, bkCancel, bkHelp, ...)
QIcon bitBtnKindIcon(const QString &Kind);

// Returns the QMenu used as PopupMenu for widget W (or nullptr).
QMenu *popupMenuOf(QWidget *W);

}  // namespace VclLayout
