# D-Fend Reborn – porting guide

D-Fend Reborn is a direct C++17 / Qt 6 port of the Delphi source code of
**D-Fend Reloaded** (repository `D-Fend-Reloaded-Base`).  Goal of the first
phase: *get everything working with the original look and feel*.  Optimising
and modernising comes later, so **translate the Delphi code as literally as
possible** – keep unit structure, identifiers, control flow and comments.

## Repository layout

| Path | Contents |
| --- | --- |
| `CMakeLists.txt` | Qt Creator / CMake project (open this file in Qt Creator) |
| `src/main.cpp` | replaces `DFend.dpr` (unit initialization, main form, event loop) |
| `src/compat/` | Delphi RTL/VCL compatibility layer (hand written, shared by all units) |
| `src/core/` | non visual units: `<Unit>.h` + `<Unit>.cpp` for every `<Unit>.pas` |
| `src/forms/` | forms and frames: `<Unit>.ui` (generated from the `.dfm`), `<Unit>.h`, `<Unit>.cpp` |
| `resources/` | images extracted from the DFMs (`glyphs/`, `imagelists/`, `pictures/`, `icons/`, `vcl/`) and `dfend.qrc` |
| `tools/dfm2ui.py` | DFM → Qt Designer `.ui` converter (re-run after changing the converter) |
| `tools/pas2h.py` | generated the initial C++ headers from the Pascal `interface` sections |
| `tools/langstrings.py` | language strings: `check` / `sync` / `add` for `src/core/LanguageStrings.inc` (the table of all strings) and `data/Lang/English.ini` / `German.ini` (originally: generated the default strings from the Pascal source) |
| `tools/check_headers.sh` | compiles every header on its own (`-fsyntax-only`) |
| `tools/formpreview/` | renders a `.ui` file to PNG (visual check of the conversion) |

Building: `cmake -S . -B build && cmake --build build` or open
`CMakeLists.txt` in Qt Creator.  Requires Qt ≥ 6.4 (Widgets, Network, Xml,
Multimedia).

## Units that are *not* ported

| Delphi unit | Replacement |
| --- | --- |
| FastMM4*, ResHookUnit, VistaToolsUnit | not needed |
| GIFImage, Resample, JPEG, PNGImage | `QImage` / `QImageReader` (GIF/JPEG/PNG/BMP built in) |
| Hash*_U, HashCalc, SDUGeneral | `QCryptographicHash::Md5` (HashCalc keeps its interface, see header) |
| MediaInterface (external DLL player) | Qt Multimedia (`QMediaPlayer`, `QVideoWidget`) |
| FileNameConvertor.old* | dead code |
| PacakgeManagerRepositoriesEditFormUnit | unused duplicate of PackageManagerRepositoriesEditFormUnit |
| Indy (IdHTTP …) | `QNetworkAccessManager` (synchronous helper with `QEventLoop` is fine) |
| XMLDoc / MSXML (IXMLNode …) | `QDomDocument` / `QDomElement` |
| SevenZipVCL / ZipMaster | the external `7z` executable (`7z.exe` on Windows) via `QProcess` |
| Registry (TRegistry) | `QSettings(…, QSettings::NativeFormat)` on Windows |

## General translation rules

* **One Delphi unit = one C++ header + one source file**, same base name.
  The header contains the translated `interface` section, the `.cpp` the
  `implementation` section.  Private helper routines of the implementation
  section become `static` functions in the `.cpp`.
* **Identifiers keep their Delphi spelling** (types, routines, constants,
  fields, enum values, parameters).  Delphi is case insensitive – always use
  the spelling of the *declaration*.  C++ keywords get a trailing `_`
  (`default_`, `new_`).
* **Every Delphi class instance is a pointer**: `TGame*`, `TStringList*`,
  `TPrgSetup*` – including `TStringList`.  `X := TFoo.Create(...)` →
  `X = new TFoo(...)`, `X.Free` → `delete X`.  Short lived local lists may be
  stack objects (`TStringList St;`) when they never escape the function.
  Functions that return a new `TStringList` keep doing so (`TStringList*`, the
  caller deletes it) – exactly like the Delphi code.
* **Properties** become accessor pairs: `property Name` → `Name()` /
  `SetName(Value)`.  `Game.Name := 'x'` → `Game->SetName("x")`.
  Indexed and array properties take the index as first argument:
  `GameDB[I]` → `GameDB->Game(I)`, `PrgSetup.LinuxRemap['C']` →
  `PrgSetup->LinuxRemap('C')`.
* `var` / `out` parameters are references (`int &X`, `TGame *&G`).
  `const` parameters of class type are plain pointers.
* Records → `struct` (value semantics).  Enums keep their enumerator names.
  `set of TFoo` → `QSet<int>` (`X in S` → `S.contains(X)`, `Include(S, X)` →
  `S.insert(X)`).  Dynamic arrays → `QList<T>`.  Static arrays → C arrays.
  Delphi interfaces (`IModernProfileEditorFrame`, `ISetupFrame`) → abstract
  classes; frames inherit from them in addition to `QWidget`.
* Procedural types / `of object` events → `std::function<...>`
  (`TNotifyEvent` = `std::function<void(QObject*)>`).  Assign with a lambda:
  `InitData.OnCheckValue = [this](QObject *S, bool &OK) { CheckValue(S, OK); };`
* `with X do` is expanded.  `try/finally` → explicit cleanup (or RAII),
  `try/except` → `try { } catch (...) { }`.  `Exit` → `return`, `Result` →
  a local `result` variable that is returned.
* Strings: use the 1-based helpers from `SysUtils.h` (`Copy`, `Pos`,
  `Delete`, `Insert`, `Length`, `Trim`, `UpperCase`, `IntToStr`,
  `StrToIntDef`, `Format`, …) – they have Delphi semantics, which avoids off by
  one errors.  `S[I]` (1-based) → `S[I - 1]`.  `#13#10` → `LineBreak` or
  `"\r\n"` where the exact bytes matter.
* **Paths**: wherever the Delphi code concatenates `'\'` into a file system
  path use `PathDelim`.  `IncludeTrailingPathDelimiter`, `ExtractFilePath`, …
  work with both separators.  DOS paths *inside* DOSBox (`C:\GAME`) keep `\`.
* `Integer` → `int`, `Cardinal`/`DWord` → `quint32`, `Int64` → `qint64`,
  `Boolean` → `bool`, `Char` → `QChar`, `String` → `QString`,
  `TDateTime` → `TDateTime` (double, Delphi epoch), `THandle` → `THandle`
  (= process id, see `Process.h`), `TColor` → `TColor` (int, `$00BBGGRR`).
* Windows only code (registry, shell links, CD drives, screensaver, …) goes
  into `#ifdef Q_OS_WIN` blocks with a sensible fallback for other systems.
  Include `<windows.h>` only through `"WinApi.h"` and call the explicit wide
  API (`CreateFileW`, `ShellExecuteW`, …).  Everything must also compile on
  Linux.
* Unit `initialization` / `finalization` sections become
  `void <Unit>_Initialization();` / `void <Unit>_Finalization();` declared at
  the end of the header (`src/main.cpp` calls them).  Units without such
  sections do not declare them.
* Keep the original comments (translate German comments only if trivial).
  Mark anything you could not translate faithfully with `// TODO(port): …`.

## Compatibility layer (`src/compat`)

| Header | Provides |
| --- | --- |
| `SysUtils.h` | strings, files (`FileExists`, `ForceDirectories`, `FindFirst`/`FindNext`/`TSearchRec`, `ReadTextFile`/`WriteTextFile`), dates (`TDateTime`, `Now`, `FormatDateTime`), `Format`, `Exception`, `ParamStr`, `Sleep` |
| `Classes.h` | `TStringList` (Objects, Values, Names, Sorted, CommaText, LoadFromFile …), `TList` (=`QList<void*>`), `THandle`, `TNotifyEvent`, `TShiftState`, `TCloseAction` |
| `IniFiles.h` | `TMemIniFile`, `TIniFile` (case insensitive, order preserving) |
| `Dialogs.h` | `MessageDlg`, `ShowMessage`, `InputQuery`, `SelectDirectory`, `TOpenDialog`, `TSaveDialog`, `TModalResult` values (`mrOk` …), `mbYes`/`mbNo` … |
| `Process.h` | `StartProcess`, `RunProcessAndWait`, `RunProcessAndGetOutput`, `IsProcessRunning`, `ShellOpen`, `OpenFolder` |
| `Graphics.h` | `TColor` + `clXxx` constants, `TColorToQColor`, `StringToColor`, `TImageList` |
| `ComCtrls.h` | `TListView` widget with the VCL object model (`Items()->Add()`, `TListItem::Caption()/SubItems()/Data()/ImageIndex()`, `Columns()`, `ViewStyle`, `Selected()`, `SmallImages/LargeImages`, signals `selectItem`, `dblClick`, `columnClick`, `change`, …) |
| `VclForm.h` | `TVclForm` (base of all forms): `ShowModal()`, `ModalResult()`/`setModalResult()`, `closeQuery()` |
| `VclLayout.h` | `VclLayout::setupForm()` (Align/Anchors engine and all runtime fix-ups), status bar helpers, `imageList()`, `setTabVisible()`, `installKeyPreview()`, `setAlignedExtent()`, `onLabelClick()` (TLabel.OnClick), `makeLinkLabel()` (blue underlined hand-cursor label) |
| `VclWidgets.h` | `TRadioGroupWidget`, `TSplitterWidget`, `TColorBoxWidget`, `TValueListEditorWidget`, `TImageWidget` |
| `WinApi.h` | `<windows.h>` without the macros that clash with Delphi names |

Extend the compat layer when the same missing RTL/VCL feature is needed by
several units – keep it generic and Delphi-like.

## Forms and frames

`tools/dfm2ui.py` converted every `.dfm` into `src/forms/<Unit>.ui` with the
same object names and pixel positions.  The generated header declares the form
class (base `TVclForm`, frames `QWidget`, main form `QMainWindow`) with a
`Ui::<FormName> ui;` member – DFM components are accessed as `ui.Name`
(they are **not** class members any more).

### Control mapping

| VCL | Qt (`ui.X`) | Typical translation |
| --- | --- | --- |
| TLabel | `QLabel` | `Caption` → `text()/setText()` |
| TEdit | `QLineEdit` | `Text` → `text()/setText()` |
| TLabeledEdit | `QLineEdit` + `QLabel` named `X_EditLabel` | `X.EditLabel.Caption` → `ui.X_EditLabel->setText()`; hide/show both |
| TMemo / TRichEdit | `QPlainTextEdit` / `QTextEdit` | `Lines.Text` → `toPlainText()/setPlainText()`, `Lines.Add(S)` → `appendPlainText(S)` |
| TButton / TBitBtn | `QPushButton` | `Caption` → `setText()`; `ModalResult` is wired automatically |
| TSpeedButton | `QToolButton` | `Down` → `isChecked()` |
| TCheckBox / TRadioButton | `QCheckBox` / `QRadioButton` | `Checked` → `isChecked()/setChecked()`, `State=cbGrayed` → `Qt::PartiallyChecked` |
| TComboBox | `QComboBox` | `Items.Add` → `addItem`, `Items.Clear` → `clear()`, `ItemIndex` → `currentIndex()/setCurrentIndex()`, `Text` → `currentText()/setCurrentText()` (editable) |
| TListBox / TCheckListBox | `QListWidget` | `Items[i]` → `item(i)->text()`, `Checked[i]` → `item(i)->checkState()` (create items with `Qt::ItemIsUserCheckable` for check list boxes) |
| TListView | `TListView` (compat) | same object model as VCL, see `ComCtrls.h` |
| TTreeView | `QTreeWidget` (one column, header hidden) | `TTreeNode` → `QTreeWidgetItem*`, `Node.Text` → `text(0)`, `Node.Data` → `data(0, Qt::UserRole)` (store pointers with `QVariant::fromValue<void*>(p)`), `Items.AddChild(P, S)` → `new QTreeWidgetItem(P, {S})`, `Selected` → `currentItem()` |
| TPageControl / TTabSheet | `QTabWidget` / page `QWidget` | `ActivePage := X` → `setCurrentWidget(ui.X)`, `X.Caption` → `setTabText(indexOf(ui.X), …)`, `TabVisible` → `VclLayout::setTabVisible` |
| TNotebook | `QStackedWidget` | `PageIndex` → `currentIndex()` |
| TGroupBox / TPanel / TBevel | `QGroupBox` / `QFrame` / `QFrame` | `Caption` → `setTitle()` (group box) |
| TRadioGroup | `TRadioGroupWidget` | `Items` → `setItems()/addItem()`, `ItemIndex` → `itemIndex()/setItemIndex()`, `OnClick` → signal `clicked()` |
| TSpinEdit | `QSpinBox` | `Value` → `value()/setValue()` |
| TTrackBar | `QSlider` | `Position` → `value()` |
| TStringGrid | `QTableWidget` | fixed rows/columns are the Qt headers: `Cells[C,R]` with `R >= FixedRows` → `item(R - FixedRows, C - FixedCols)` (see dynamic properties `vclFixedRows`/`vclFixedCols`) |
| TValueListEditor | `TValueListEditorWidget` | `Values[K]`, `Keys[R]`, `Strings` → see header |
| TImage | `TImageWidget` | `Picture.LoadFromFile` → `setPicture(QPixmap(f))` |
| TStatusBar | `QStatusBar` | `Panels[i].Text` → `VclLayout::setStatusText(ui.Bar, i, S)`, `SimpleText` → `VclLayout::setSimpleText` |
| TToolBar / TToolButton | `QToolBar` / `QAction` | `Button.Enabled` → `ui.Button->setEnabled()`, `OnClick` → `triggered` |
| TMainMenu / TMenuItem | `QMenuBar` / `QAction` (`QMenu` for items with sub items) | `Caption` → `setText()` (`QMenu`: `setTitle()`), `Visible` of a sub menu → `ui.M->menuAction()->setVisible()`, `Checked` → `setChecked()` |
| TPopupMenu | `QMenu` | shown automatically for controls with `PopupMenu`; `OnPopup` → `aboutToShow`, `Popup(X,Y)` → `popup(QPoint)` |
| TSplitter | `TSplitterWidget` | works automatically |
| TImageList (design time images) | — | `TImageList::FromResource("<Unit>", "<ListName>")` or `VclLayout::imageList()` |
| TOpenDialog / TSaveDialog / TTimer / TTrayIcon | — (not in the `.ui`) | create in code: `TOpenDialog`, `TSaveDialog` (compat), `QTimer`, `QSystemTrayIcon`; take the settings (Filter, DefaultExt, Interval …) from the `.dfm` |

`Tag` of any control: `ui.X->property("tag").toInt()`.  `Hint` → `toolTip`.
`Visible := False` → `setVisible(false)` (the align engine reacts to it).
Setting `Left/Top/Width/Height` of an anchored or aligned control works with
`setGeometry()`/`resize()` – the layout engine adopts the new values.

### Form skeleton

```cpp
TFooForm::TFooForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DFM event wiring (see the comment block in the header)
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.ListBox, &QListWidget::itemDoubleClicked, this, [this]() { ListBoxDblClick(ui.ListBox); });
    FormCreate(this);   // OnCreate
}

TFooForm::~TFooForm() { /* OnDestroy */ }

void TFooForm::showEvent(QShowEvent *E) { TVclForm::showEvent(E); FormShow(this); }   // OnShow
```

* Event handler methods keep their Delphi names and the `Sender` parameter
  (`QObject*`), so shared handlers can test `Sender == ui.Button1`.
* `ModalResult` of buttons is wired by `VclLayout::setupForm` *before* your
  `OnClick` connection, exactly like VCL (OnClick may call
  `setModalResult(mrNone)` to keep the dialog open).  Code that assigns
  `ModalResult := mrOk` calls `setModalResult(mrOk)`.
* `OnCloseQuery` → override `bool closeQuery()`, `OnClose` → `closeEvent`,
  `OnResize` → `resizeEvent`, `OnKeyDown` with `KeyPreview` →
  `VclLayout::installKeyPreview(this, …)`.  Delphi virtual key codes become
  `Qt::Key_*` values.
* The `Show…Dialog(const AOwner : TComponent; …)` functions in the interface
  section become free functions taking `QWidget *AOwner`; they create the
  form with `new TFooForm(AOwner)`, call `ShowModal()`, read the results and
  `delete` the form.
* Frames (TFrame) derive from `QWidget`; their constructor does
  `ui.setupUi(this); VclLayout::setupForm(this);`.  Code that creates a frame
  at runtime (`TFrameX.Create(Owner)` + `Parent := Panel` + `Align := alClient`)
  becomes `F = new TFrameX(Panel); F->setProperty("vclAlign", "alClient");`
  followed by `VclLayout::setupForm(Panel)` or a layout – see
  `ModernProfileEditorFormUnit.cpp` for the reference implementation.
* `LanguageSetup.Xyz` → `LanguageSetup->Xyz()`; `PrgSetup.Xyz` →
  `PrgSetup->Xyz()`.

## Workflow for porting a unit

1. Read `<Unit>.pas` (and `.dfm`) completely.
2. Review the generated header; fix anything the generator got wrong but keep
   public signatures stable (other units are ported in parallel against
   them).  Add `_Initialization`/`_Finalization` declarations if needed.
3. Write `<Unit>.cpp` translating the implementation section routine by
   routine.
4. Check it compiles: `tools/check_headers.sh src/<dir>/<Unit>.h` for the
   header and
   `g++ -std=c++17 -fsyntax-only $(pkg-config --cflags Qt6Widgets Qt6Xml Qt6Network) -Isrc/compat -Isrc/core -Isrc/forms -I/tmp/dfend-headercheck src/<dir>/<Unit>.cpp`
   for the source (run `tools/check_headers.sh` once to generate the `ui_*.h`
   files into `/tmp/dfend-headercheck`).
