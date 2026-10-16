// C++ port of ProfileEditorFormUnit.pas (implementation section).
#include "ProfileEditorFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "IconManagerFormUnit.h"
#include "ProfileMountEditorFormUnit.h"
#include "SerialEditFormUnit.h"
#include "UserInfoFormUnit.h"
#include "IconLoaderUnit.h"
#include "GameDBToolsUnit.h"
#include "ModernProfileEditorFormUnit.h"
#include "DosBoxUnit.h"
#include "DOSBoxConfUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "TextEditPopupUnit.h"

TProfileEditorForm* ProfileEditorForm = nullptr;

static int LastPage = 0, LastTop = 0, LastLeft = 0;

// ---------------------------------------------------------------------------
// Local helpers (VCL features without a direct Qt equivalent)
// ---------------------------------------------------------------------------

namespace {

// Event filter calling a function (OnKeyDown / OnKeyUp of controls)
class TEventHook : public QObject
{
public:
    TEventHook(QObject *Target, std::function<bool(QEvent *)> AHandler) : QObject(Target), FHandler(std::move(AHandler)) { Target->installEventFilter(this); }
    bool eventFilter(QObject *, QEvent *E) override { return FHandler(E); }
private:
    std::function<bool(QEvent *)> FHandler;
};

// ItemProps[Row].ReadOnly
void SetItemReadOnly(TValueListEditorWidget *E, int Row, bool B)
{
    E->setReadOnly(Row, B);
}

bool ItemReadOnly(TValueListEditorWidget *E, int Row)
{
    return E->isReadOnly(Row);
}

// Strings.ValueFromIndex[I]
QString VFI(TValueListEditorWidget *E, int I)
{
    if (I < 0 || I >= E->rowCount()) return QString();
    return E->value(I);
}

// Strings.ValueFromIndex[I]:=S
void SetVFI(TValueListEditorWidget *E, int I, const QString &S)
{
    if (I < 0 || I >= E->rowCount()) return;
    E->setValue(I, S);
}

// Strings[I] ('Key=Value')
QString VLEString(TValueListEditorWidget *E, int I)
{
    return E->key(I) + "=" + E->value(I);
}

// Builder for "Strings.Add(...); ItemProps[Strings.Count-1]...." sequences
struct TVLE {
    TValueListEditorWidget *E;
    explicit TVLE(TValueListEditorWidget *AE) : E(AE) {}
    int Last() const { return E->rowCount() - 1; }
    // Strings.Add('Key=Value')
    void Add(const QString &S)
    {
        int P = S.indexOf('=');
        E->insertRow(P >= 0 ? S.left(P) : S, P >= 0 ? S.mid(P + 1) : QString());
    }
    // Strings.Delete(I)
    void Delete(int I) { if (I >= 0 && I < E->rowCount()) E->deleteRow(I); }
    // ItemProps[Strings.Count-1].ReadOnly:=True
    void ReadOnly() { SetItemReadOnly(E, Last(), true); }
    // ItemProps[Strings.Count-1].EditStyle:=esEllipsis
    void Ellipsis() { E->setEllipsis(Last(), true); }
    // ItemProps[Strings.Count-1].EditStyle:=esPickList (a pick list is shown as soon as it has items)
    void PickListStyle() {}
    // ItemProps[Strings.Count-1].EditMask:=...
    void EditMask(const QString &Mask) { E->setEditMask(Last(), Mask); }
    // ItemProps[Strings.Count-1].PickList.Add(S)
    void Pick(const QString &S)
    {
        QStringList PL = E->pickList(Last());
        PL << S;
        E->setPickList(Last(), PL);
    }
    // St:=...; try ItemProps[Strings.Count-1].PickList.Assign(St); finally St.Free; end;
    void PickAssign(TStringList *St)
    {
        E->setPickList(Last(), St->ToQStringList());
        delete St;
    }
};

// TStringGrid helpers (FixedRows = FixedCols = 0): Cells[0,R]
QString GridCell(QTableWidget *G, int R)
{
    QTableWidgetItem *I = G->item(R, 0);
    return I ? I->text() : QString();
}

void SetGridCell(QTableWidget *G, int R, const QString &S)
{
    if (!G->item(R, 0)) G->setItem(R, 0, new QTableWidgetItem());
    G->item(R, 0)->setText(S);
}

// TListBox.Items
TStringList ListBoxItems(QListWidget *L)
{
    TStringList St;
    for (int I = 0; I < L->count(); I++) St.Add(L->item(I)->text());
    return St;
}

// TRichEdit.Lines
TStringList MemoLines(QTextEdit *M)
{
    TStringList St;
    St.SetText(M->toPlainText());
    return St;
}

void SetMemoLines(QTextEdit *M, const TStringList &St)
{
    M->setPlainText(St.ToQStringList().join("\n"));
}

void SetMemoFontName(QTextEdit *M, const QString &Name)
{
    QFont F = M->font();
    F.setFamily(Name);
    F.setStyleHint(QFont::Monospace);
    M->setFont(F);
}

// TabSheet.Caption
void SetTabCaption(QTabWidget *Tabs, QWidget *Page, const QString &S)
{
    int I = Tabs->indexOf(Page);
    if (I >= 0) Tabs->setTabText(I, S);
}

}  // namespace

// ---------------------------------------------------------------------------
// TProfileEditorForm
// ---------------------------------------------------------------------------

TProfileEditorForm::TProfileEditorForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // Non visual DFM components
    OpenDialog = new TOpenDialog(this);
    SaveDialog = new TSaveDialog(this);
    DosBoxTxtOpenDialog = new TOpenDialog(this);
    DosBoxTxtOpenDialog->DefaultExt = "txt";
    ImageList = TImageList::FromResource("ProfileEditorFormUnit", "ImageList");
    ImageListM = TImageList::FromResource("ProfileEditorFormUnit", "ImageListM");
    if (ImageList == nullptr) ImageList = new TImageList();
    if (ImageListM == nullptr) ImageListM = new TImageList();

    // DFM event wiring
    const QList<QAbstractButton *> WorkButtons = {
        ui.ExtraDirsAddButton, ui.ExtraDirsEditButton, ui.ExtraDirsDelButton,
        ui.ExtraFilesAddButton, ui.ExtraFilesEditButton, ui.ExtraFilesDelButton,
        ui.IconSelectButton, ui.IconDeleteButton, ui.GenerateScreenshotFolderNameButton,
        ui.GenerateGameDataFolderNameButton, ui.GameInfoMetaDataButton,
        ui.MountingAddButton, ui.MountingEditButton, ui.MountingDelButton, ui.MountingDeleteAllButton, ui.MountingAutoCreateButton,
        ui.AutoexecBootFloppyImageButton, ui.AutoexecBootFloppyImageAddButton, ui.AutoexecBootFloppyImageDelButton,
        ui.AutoexecClearButton, ui.AutoexecLoadButton, ui.AutoexecSaveButton,
        ui.FinalizationClearButton, ui.FinalizationLoadButton, ui.FinalizationSaveButton,
        ui.CustomSetsClearButton, ui.CustomSetsLoadButton, ui.CustomSetsSaveButton,
        ui.CustomSetsEnvAdd, ui.CustomSetsEnvDel};
    for (QAbstractButton *B : WorkButtons)
        connect(B, &QAbstractButton::clicked, this, [this, B]() { ButtonWork(B); });
    for (QAction *A : {ui.PopupAdd, ui.PopupEdit, ui.PopupDelete})
        connect(A, &QAction::triggered, this, [this, A]() { ButtonWork(A); });

    connect(ui.ProfileSettingsValueListEditor, &TValueListEditorWidget::editButtonClicked, this, [this](int Row) {
        ui.ProfileSettingsValueListEditor->setCurrentCell(Row, 0);   // commit the in place editor
        ProfileSettingsValueListEditorEditButtonClick(ui.ProfileSettingsValueListEditor);
    });
    new TEventHook(ui.ProfileSettingsValueListEditor, [this](QEvent *E) {
        if (E->type() == QEvent::KeyRelease) {
            auto *K = static_cast<QKeyEvent *>(E);
            int Key = K->key();
            ProfileSettingsValueListEditorKeyUp(ui.ProfileSettingsValueListEditor, Key, K->modifiers());
        }
        return false;
    });
    connect(ui.ProfileSettingsValueListEditor, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *I) {
        ProfileSettingsValueListEditorSetEditText(ui.ProfileSettingsValueListEditor, I->column(), I->row() + 1, I->text());
    });
    connect(ui.ExtraDirsListBox, &QListWidget::currentRowChanged, this, [this]() { ExtraDirsListBoxClick(ui.ExtraDirsListBox); });
    connect(ui.ExtraDirsListBox, &QListWidget::itemDoubleClicked, this, [this]() { ExtraDirsListBoxDblClick(ui.ExtraDirsListBox); });
    new TEventHook(ui.ExtraDirsListBox, [this](QEvent *E) {
        if (E->type() == QEvent::KeyPress) {
            auto *K = static_cast<QKeyEvent *>(E);
            int Key = K->key();
            ExtraDirsListBoxKeyDown(ui.ExtraDirsListBox, Key, K->modifiers() & ~Qt::KeypadModifier);
        }
        return false;
    });
    connect(ui.ExtraFilesListBox, &QListWidget::currentRowChanged, this, [this]() { ExtraFilesListBoxClick(ui.ExtraFilesListBox); });
    connect(ui.ExtraFilesListBox, &QListWidget::itemDoubleClicked, this, [this]() { ExtraFilesListBoxDblClick(ui.ExtraFilesListBox); });
    new TEventHook(ui.ExtraFilesListBox, [this](QEvent *E) {
        if (E->type() == QEvent::KeyPress) {
            auto *K = static_cast<QKeyEvent *>(E);
            int Key = K->key();
            ExtraFilesListBoxKeyDown(ui.ExtraFilesListBox, Key, K->modifiers() & ~Qt::KeypadModifier);
        }
        return false;
    });
    connect(ui.GameInfoValueListEditor, &TValueListEditorWidget::editButtonClicked, this, [this](int Row) {
        ui.GameInfoValueListEditor->setCurrentCell(Row, 0);
        GameInfoValueListEditorEditButtonClick(ui.GameInfoValueListEditor);
    });
    connect(ui.SearchGameButton, &QAction::triggered, this, [this]() { SearchClick(ui.SearchGameButton); });
    connect(ui.GeneralValueListEditor, &TValueListEditorWidget::editButtonClicked, this, [this](int Row) {
        ui.GeneralValueListEditor->setCurrentCell(Row, 0);
        GeneralValueListEditorEditButtonClick(ui.GeneralValueListEditor);
    });
    // (DOSBox installation changed: grey out the settings it does not use)
    connect(ui.GeneralValueListEditor, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *I) {
        if ((I != nullptr) && (I->column() == 1) && (I->row() == 15 + (PrgSetup->AllowTextModeLineChange() ? 1 : 0))) MarkIgnoredDOSBoxSettings();
    });
    connect(ui.EnvironmentValueListEditor, &TValueListEditorWidget::editButtonClicked, this, [this](int Row) {
        ui.EnvironmentValueListEditor->setCurrentCell(Row, 0);
        EnvironmentValueListEditorEditButtonClick(ui.EnvironmentValueListEditor);
    });
    connect(ui.MountingListView, &TListView::dblClick, this, [this]() { MountingListViewDblClick(ui.MountingListView); });
    connect(ui.MountingListView, &TListView::keyDown, this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        MountingListViewKeyDown(ui.MountingListView, K, Mods & ~Qt::KeypadModifier);
    });
    connect(ui.AutoexecBootHDImageComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { AutoexecBootHDImageComboBoxChange(ui.AutoexecBootHDImageComboBox); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.PreviousButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.PreviousButton); });
    connect(ui.NextButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.NextButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods & ~Qt::KeypadModifier);
        return false;
    });

    // Shortcuts of the PopupMenu items are active while the list view has the focus (VCL IsMenuKey)
    for (QAction *A : {ui.PopupAdd, ui.PopupEdit, ui.PopupDelete}) {
        A->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        ui.MountingListView->addAction(A);
    }

    FormCreate(this);
}

TProfileEditorForm::~TProfileEditorForm()
{
    FormDestroy(this);
    delete OpenDialog;
    delete SaveDialog;
    delete DosBoxTxtOpenDialog;
    delete ImageList;
    delete ImageListM;
}

void TProfileEditorForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TProfileEditorForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    HideGameInfoPage = false;
}

// Assigns the images of ImageList / ImageListM to the controls using them
void TProfileEditorForm::ApplyImageLists()
{
    // PageControl.Images = ImageList (ImageIndex 0..7), PageControl2.Images = ImageList (ImageIndex 8..13)
    const QList<QWidget *> Pages1 = {ui.ProfileSettingsSheet, ui.GameInfoSheet, ui.GeneralSheet, ui.EnvironmentSheet, ui.MountingSheet, ui.SoundSheet, ui.AutoexecSheet, ui.CustomSetsSheet};
    for (int I = 0; I < Pages1.size(); I++) {
        int J = ui.PageControl->indexOf(Pages1[I]);
        if (J >= 0 && I < ImageList->Count()) ui.PageControl->setTabIcon(J, ImageList->Icon(I));
    }
    const QList<QWidget *> Pages2 = {ui.SoundSBSheet, ui.SoundGUSSheet, ui.SoundMIDISheet, ui.SoundJoystickSheet, ui.SoundMiscSheet, ui.SoundVolumeSheet};
    for (int I = 0; I < Pages2.size(); I++) {
        int J = ui.PageControl2->indexOf(Pages2[I]);
        if (J >= 0 && I + 8 < ImageList->Count()) ui.PageControl2->setTabIcon(J, ImageList->Icon(I + 8));
    }
    // ToolBar.Images = ImageList (SearchGameButton.ImageIndex = 14)
    if (14 < ImageList->Count()) ui.SearchGameButton->setIcon(ImageList->Icon(14));
    // PopupMenu.Images = ImageListM
    if (0 < ImageListM->Count()) ui.PopupAdd->setIcon(ImageListM->Icon(0));
    if (1 < ImageListM->Count()) ui.PopupEdit->setIcon(ImageListM->Icon(1));
    if (2 < ImageListM->Count()) ui.PopupDelete->setIcon(ImageListM->Icon(2));
}

void TProfileEditorForm::InitGUI()
{
    TStringList* St;
    int I;
    QString S;

    // DoubleBuffered, SetVistaFonts and Font.Charset are not needed with Qt.

    ui.AutoexecBootFloppyImageTab->setColumnWidth(0, ui.AutoexecBootFloppyImageTab->width() - 2 * ui.AutoexecBootFloppyImageTab->frameWidth() - 25);

    SetRichEditPopup(ui.NotesMemo);
    SetRichEditPopup(ui.CustomSetsMemo);
    SetRichEditPopup(ui.AutoexecMemo);
    SetRichEditPopup(ui.FinalizationMemo);

    MoveStatus = 0;

    setWindowTitle(LanguageSetup->ProfileEditor());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.PreviousButton->setText(RemoveUnderline(LanguageSetup->OK()) + " && " + LanguageSetup->Previous());
    ui.NextButton->setText(RemoveUnderline(LanguageSetup->OK()) + " && " + LanguageSetup->Next());

    // Profile Settings Sheet

    I = ui.ProfileSettingsValueListEditor->columnWidth(0) + ui.ProfileSettingsValueListEditor->columnWidth(1);
    ui.ProfileSettingsValueListEditor->setColumnWidth(0, 4 * I / 10);
    ui.ProfileSettingsValueListEditor->setColumnWidth(1, 6 * I / 10);

    SetTabCaption(ui.PageControl, ui.ProfileSettingsSheet, LanguageSetup->ProfileEditorProfileSettingsSheet());
    // IconPanel.ControlStyle:=IconPanel.ControlStyle-[csParentBackground]: not needed with Qt
    ui.IconSelectButton->setText(LanguageSetup->ProfileEditorIconSelect());
    ui.IconDeleteButton->setText(LanguageSetup->ProfileEditorIconDelete());
    ui.ProfileSettingsValueListEditor->setTitleCaptions({LanguageSetup->Key(), LanguageSetup->Value()});
    {
        TVLE V(ui.ProfileSettingsValueListEditor);
        V.Delete(0);
        V.Add(LanguageSetup->ProfileEditorProfileName() + "=");
        V.Add(LanguageSetup->ProfileEditorFilename() + "=");
        V.ReadOnly();
        V.Add(LanguageSetup->ProfileEditorGameEXE() + "=");
        V.Ellipsis();
        V.Add(LanguageSetup->ProfileEditorRelPath() + "=");
        V.PickListStyle();
        V.ReadOnly();
        V.Pick(RemoveUnderline(LanguageSetup->Yes()));
        V.Pick(RemoveUnderline(LanguageSetup->No()));
        V.Add(LanguageSetup->ProfileEditorGameParameters() + "=");
        V.Add(LanguageSetup->ProfileEditorSetupEXE() + "=");
        V.Ellipsis();
        V.Add(LanguageSetup->ProfileEditorRelPath() + "=");
        V.PickListStyle();
        V.ReadOnly();
        V.Pick(RemoveUnderline(LanguageSetup->Yes()));
        V.Pick(RemoveUnderline(LanguageSetup->No()));
        V.Add(LanguageSetup->ProfileEditorSetupParameters() + "=");
        V.Add(LanguageSetup->ProfileEditorLoadFix() + "=");
        V.PickListStyle();
        V.ReadOnly();
        V.Pick(RemoveUnderline(LanguageSetup->Yes()));
        V.Pick(RemoveUnderline(LanguageSetup->No()));
        V.Add(LanguageSetup->ProfileEditorLoadFixMemory() + "=");
        V.EditMask("900");
        V.Add(LanguageSetup->ProfileEditorCaptureFolder() + "=");
        V.Ellipsis();
    }
    ui.ExtraFilesLabel->setText(LanguageSetup->ProfileEditorExtraFiles());
    ui.ExtraFilesAddButton->setToolTip(RemoveUnderline(LanguageSetup->Add()));
    ui.ExtraFilesEditButton->setToolTip(RemoveUnderline(LanguageSetup->Edit()));
    ui.ExtraFilesDelButton->setToolTip(RemoveUnderline(LanguageSetup->Del()));
    ui.ExtraDirsLabel->setText(LanguageSetup->ProfileEditorExtraDirs());
    ui.ExtraDirsAddButton->setToolTip(RemoveUnderline(LanguageSetup->Add()));
    ui.ExtraDirsEditButton->setToolTip(RemoveUnderline(LanguageSetup->Edit()));
    ui.ExtraDirsDelButton->setToolTip(RemoveUnderline(LanguageSetup->Del()));
    ui.GenerateScreenshotFolderNameButton->setText(LanguageSetup->ProfileEditorGenerateScreenshotFolderName());

    // Game Info Sheet

    if (HideGameInfoPage) VclLayout::setTabVisible(ui.PageControl, ui.GameInfoSheet, false); else {
        SetTabCaption(ui.PageControl, ui.GameInfoSheet, LanguageSetup->ProfileEditorGameInfoSheet());
        ui.GameInfoValueListEditor->setTitleCaptions({LanguageSetup->Key(), LanguageSetup->Value()});
        TVLE V(ui.GameInfoValueListEditor);
        V.Delete(0);
        V.Add(LanguageSetup->GameGenre() + "=");
        V.PickListStyle();
        V.PickAssign(ExtGenreList(GetCustomGenreName(GameDB->GetGenreList())));
        V.Add(LanguageSetup->GameDeveloper() + "=");
        V.PickListStyle();
        V.PickAssign(GameDB->GetDeveloperList());
        V.Add(LanguageSetup->GamePublisher() + "=");
        V.PickListStyle();
        V.PickAssign(GameDB->GetPublisherList());
        V.Add(LanguageSetup->GameYear() + "=");
        V.PickListStyle();
        V.PickAssign(GameDB->GetYearList());
        V.Add(LanguageSetup->GameLanguage() + "=");
        V.PickListStyle();
        V.PickAssign(ExtLanguageList(GetCustomLanguageName(GameDB->GetLanguageList())));
        V.Add(LanguageSetup->GameWWW() + "=");
        V.Add(LanguageSetup->GameDataDir() + "=");
        V.Ellipsis();
        V.Add(LanguageSetup->GameFavorite() + "=");
        V.ReadOnly();
        V.Pick(RemoveUnderline(LanguageSetup->Yes()));
        V.Pick(RemoveUnderline(LanguageSetup->No()));
        ui.GenerateGameDataFolderNameButton->setText(LanguageSetup->ProfileEditorGenerateGameDataFolder());
        ui.GameInfoMetaDataButton->setText(LanguageSetup->ProfileEditorUserDefinedInfo());
        ui.NotesLabel->setText(LanguageSetup->GameNotes() + ":");
    }

    InitStagingSheet();

    // General Sheet

    SetTabCaption(ui.PageControl, ui.GeneralSheet, LanguageSetup->ProfileEditorGeneralSheet());
    ui.GeneralValueListEditor->setTitleCaptions({LanguageSetup->Key(), LanguageSetup->Value()});
    {
        TVLE V(ui.GeneralValueListEditor);
        auto YesNo = [&V]() { V.ReadOnly(); V.Pick(RemoveUnderline(LanguageSetup->Yes())); V.Pick(RemoveUnderline(LanguageSetup->No())); };
        V.Delete(0);
        V.Add(LanguageSetup->GameCloseDOSBoxAfterGameExit() + "=");
        YesNo();
        V.Add(LanguageSetup->GameStartFullscreen() + "=");
        YesNo();
        V.Add(LanguageSetup->GameAutoLockMouse() + "=");
        YesNo();
        V.Add(LanguageSetup->GameForce2ButtonMouseMode() + "=");
        YesNo();
        V.Add(LanguageSetup->GameSwapMouseButtons() + "=");
        YesNo();
        V.Add(LanguageSetup->GameUseDoublebuffering() + "=");
        YesNo();
        V.Add(LanguageSetup->GameAspectCorrection() + "=");
        YesNo();
        V.Add(LanguageSetup->GameUseScanCodes() + "=");
        YesNo();
        V.Add(LanguageSetup->GameMouseSensitivity() + "=");
        V.EditMask("9000");
        V.PickAssign(ValueToList(GameDB->ConfOpt()->MouseSensitivity(), ";,"));
        V.Add(LanguageSetup->GameRender() + "=");
        V.PickListStyle();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->Render(), ";,"));
        V.Add(LanguageSetup->GameWindowResolution() + "=");
        V.PickListStyle();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->ResolutionWindow(), ";,"));
        V.Add(LanguageSetup->GameFullscreenResolution() + "=");
        V.PickListStyle();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->ResolutionFullscreen(), ";,"));
        V.Add(LanguageSetup->GameScale() + "=");
        V.ReadOnly();
        V.PickListStyle();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->Scale(), ";,"));
        if (PrgSetup->AllowTextModeLineChange()) {
            V.Add(LanguageSetup->GameTextModeLines() + "=");
            V.ReadOnly();
            V.Pick("25");
            V.Pick("28");
            V.Pick("50");
        }
        V.Add(LanguageSetup->GamePriorityForeground() + "=");
        V.ReadOnly();
        V.Pick("lower");
        V.Pick("normal");
        V.Pick("higher");
        V.Pick("highest");
        V.Add(LanguageSetup->GamePriorityBackground() + "=");
        V.ReadOnly();
        V.Pick("lower");
        V.Pick("normal");
        V.Pick("higher");
        V.Pick("highest");
        V.Add(LanguageSetup->GameDOSBoxVersion() + "=");
        V.Ellipsis();
        V.Add(LanguageSetup->GameKeyMapper() + "=");
        V.Ellipsis();
    }

    // Environment Sheet

    SetTabCaption(ui.PageControl, ui.EnvironmentSheet, LanguageSetup->ProfileEditorEnvironmentSheet());
    ui.EnvironmentValueListEditor->setTitleCaptions({LanguageSetup->Key(), LanguageSetup->Value()});
    ui.KeyboardLayoutInfoLabel->setText(LanguageSetup->GameKeyboardLayoutInfo());
    {
        QFont F = ui.KeyboardLayoutInfoLabel->font();
        if (F.pointSize() > 1) F.setPointSize(F.pointSize() - 1);
        ui.KeyboardLayoutInfoLabel->setFont(F);
    }
    {
        TVLE V(ui.EnvironmentValueListEditor);
        auto YesNo = [&V]() { V.ReadOnly(); V.Pick(RemoveUnderline(LanguageSetup->Yes())); V.Pick(RemoveUnderline(LanguageSetup->No())); };
        auto Lock = [&V]() { V.ReadOnly(); V.PickListStyle(); V.Pick(LanguageSetup->DoNotChange()); V.Pick(LanguageSetup->Off()); V.Pick(LanguageSetup->On()); };
        V.Delete(0);
        V.Add(LanguageSetup->GameMemory() + "=");
        V.EditMask("90");
        V.PickListStyle();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->Memory(), ";,"));
        V.Add(LanguageSetup->GameXMS() + "=");
        YesNo();
        V.Add(LanguageSetup->GameEMS() + "=");
        YesNo();
        V.Add(LanguageSetup->GameUMB() + "=");
        YesNo();
        V.Add(LanguageSetup->GameDOS32A() + "=");
        YesNo();
        V.Add(LanguageSetup->GameCycles() + "=");
        V.PickListStyle();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->Cycles(), ";,"));
        V.Add(LanguageSetup->GameCyclesUp() + "=");
        V.EditMask("90000");
        V.PickAssign(ValueToList(GameDB->ConfOpt()->CyclesUp(), ";,"));
        V.Add(LanguageSetup->GameCyclesDown() + "=");
        V.EditMask("90000");
        V.PickAssign(ValueToList(GameDB->ConfOpt()->CyclesDown(), ";,"));
        V.Add(LanguageSetup->GameCore() + "=");
        V.PickListStyle();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->Core(), ";,"));
        V.Add(LanguageSetup->GameFrameskip() + "=");
        V.PickListStyle();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->Frameskip(), ";,"));
        V.Add(LanguageSetup->GameVideoCard() + "=");
        V.ReadOnly();
        V.PickListStyle();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->Video(), ";,"));
        V.Add(LanguageSetup->GameKeyboardLayout() + "=");
        V.ReadOnly();
        V.PickListStyle();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->KeyboardLayout(), ";,"));
        V.Add(LanguageSetup->GameKeyboardCodepage() + "=");
        V.ReadOnly();
        V.PickListStyle();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->Codepage(), ";,"));
        V.Add(LanguageSetup->GameKeyboardNumLock() + "=");
        Lock();
        V.Add(LanguageSetup->GameKeyboardCapsLock() + "=");
        Lock();
        V.Add(LanguageSetup->GameKeyboardScrollLock() + "=");
        Lock();
        V.Add(LanguageSetup->GameSerial() + " 1=");
        V.Ellipsis();
        V.Add(LanguageSetup->GameSerial() + " 2=");
        V.Ellipsis();
        V.Add(LanguageSetup->GameSerial() + " 3=");
        V.Ellipsis();
        V.Add(LanguageSetup->GameSerial() + " 4=");
        V.Ellipsis();
        V.Add(LanguageSetup->GameIPX() + "=");
        YesNo();
        V.Add(LanguageSetup->GameIPXEstablishConnection() + "=");
        V.ReadOnly();
        V.Pick(RemoveUnderline(LanguageSetup->GameIPXEstablishConnectionNone()));
        V.Pick(RemoveUnderline(LanguageSetup->GameIPXEstablishConnectionClient()));
        V.Pick(RemoveUnderline(LanguageSetup->GameIPXEstablishConnectionServer()));
        V.Add(LanguageSetup->GameIPXAddress() + "=");
        V.Add(LanguageSetup->GameIPXPort() + "=");
        V.Add(LanguageSetup->GameReportedDOSVersion() + "=");
        V.PickAssign(ValueToList(GameDB->ConfOpt()->ReportedDOSVersion(), ";,"));
    }

    // Mounting Sheet

    SetTabCaption(ui.PageControl, ui.MountingSheet, LanguageSetup->ProfileEditorMountingSheet());
    ui.MountingAddButton->setText(LanguageSetup->ProfileEditorMountingAdd());
    ui.MountingEditButton->setText(LanguageSetup->ProfileEditorMountingEdit());
    ui.MountingDelButton->setText(LanguageSetup->ProfileEditorMountingDel());
    ui.MountingDeleteAllButton->setText(LanguageSetup->ProfileEditorMountingDelAll());
    ui.MountingAutoCreateButton->setText(LanguageSetup->ProfileEditorMountingAutoCreate());
    InitMountingListView(ui.MountingListView);
    ui.AutoMountCheckBox->setText(LanguageSetup->ProfileEditorMountingAutoMountCDs());
    ui.PopupAdd->setText(LanguageSetup->ProfileEditorMountingAdd());
    ui.PopupEdit->setText(LanguageSetup->ProfileEditorMountingEdit());
    ui.PopupDelete->setText(LanguageSetup->ProfileEditorMountingDel());
    ui.PopupEdit->setShortcut(QKeySequence(Qt::Key_Return));
    UserIconLoader->DialogImage(DI_Add, ImageListM, 0);
    UserIconLoader->DialogImage(DI_Edit, ImageListM, 1);
    UserIconLoader->DialogImage(DI_Delete, ImageListM, 2);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    // Sound Sheet

    SetTabCaption(ui.PageControl, ui.SoundSheet, LanguageSetup->ProfileEditorSoundSheet());
    ui.SoundValueListEditor->setTitleCaptions({LanguageSetup->Key(), LanguageSetup->Value()});
    {
        TVLE V(ui.SoundValueListEditor);
        V.Delete(0);
        V.Add(LanguageSetup->ProfileEditorSoundEnableSound() + "=");
        V.ReadOnly();
        V.Pick(RemoveUnderline(LanguageSetup->Yes()));
        V.Pick(RemoveUnderline(LanguageSetup->No()));
        V.Add(LanguageSetup->ProfileEditorSoundSampleRate() + "=");
        V.ReadOnly();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->Rate(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundBlockSize() + "=");
        V.PickAssign(ValueToList(GameDB->ConfOpt()->Blocksize(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundPrebuffer() + "=");
        V.Pick("1");
        V.Pick("5");
        V.Pick("10");
        V.Pick("15");
        V.Pick("20");
        V.Pick("25");
        V.Pick("30");
    }
    SetTabCaption(ui.PageControl2, ui.SoundSBSheet, LanguageSetup->ProfileEditorSoundSoundBlaster());
    SetTabCaption(ui.PageControl2, ui.SoundGUSSheet, LanguageSetup->ProfileEditorSoundGUS());
    SetTabCaption(ui.PageControl2, ui.SoundMIDISheet, LanguageSetup->ProfileEditorSoundMIDI());
    SetTabCaption(ui.PageControl2, ui.SoundJoystickSheet, LanguageSetup->ProfileEditorSoundJoystick());
    SetTabCaption(ui.PageControl2, ui.SoundMiscSheet, LanguageSetup->ProfileEditorSoundMisc());

    ui.SoundSBValueListEditor->setTitleCaptions({LanguageSetup->Key(), LanguageSetup->Value()});
    {
        TVLE V(ui.SoundSBValueListEditor);
        V.Delete(0);
        V.Add(LanguageSetup->ProfileEditorSoundSBType() + "=");
        V.ReadOnly();
        V.PickListStyle();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->Sblaster(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundSBAddress() + "=");
        V.ReadOnly();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->SBBase(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundSBIRQ() + "=");
        V.ReadOnly();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->IRQ(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundSBDMA() + "=");
        V.ReadOnly();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->Dma(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundSBHDMA() + "=");
        V.ReadOnly();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->HDMA(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundSBOplMode() + "=");
        V.ReadOnly();
        V.PickListStyle();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->Oplmode(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundSBOplRate() + "=");
        V.ReadOnly();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->OPLRate(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundSBUseMixer() + "=");
        V.ReadOnly();
        V.Pick(RemoveUnderline(LanguageSetup->Yes()));
        V.Pick(RemoveUnderline(LanguageSetup->No()));
    }

    ui.SoundGUSValueListEditor->setTitleCaptions({LanguageSetup->Key(), LanguageSetup->Value()});
    {
        TVLE V(ui.SoundGUSValueListEditor);
        V.Delete(0);
        V.Add(LanguageSetup->ProfileEditorSoundGUSEnabled() + "=");
        V.ReadOnly();
        V.Pick(RemoveUnderline(LanguageSetup->Yes()));
        V.Pick(RemoveUnderline(LanguageSetup->No()));
        V.Add(LanguageSetup->ProfileEditorSoundGUSAddress() + "=");
        V.ReadOnly();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->GUSBase(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundGUSIRQ() + "=");
        V.ReadOnly();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->GUSIRQ(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundGUSDMA() + "=");
        V.ReadOnly();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->GUSDma(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundGUSRate() + "=");
        V.ReadOnly();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->GUSRate(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundGUSPath() + "=");
    }

    ui.SoundMIDIValueListEditor->setTitleCaptions({LanguageSetup->Key(), LanguageSetup->Value()});
    {
        TVLE V(ui.SoundMIDIValueListEditor);
        V.Delete(0);
        V.Add(LanguageSetup->ProfileEditorSoundMIDIType() + "=");
        V.ReadOnly();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->MPU401(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundMIDIDevice() + "=");
        V.ReadOnly();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->MIDIDevice(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundMIDIConfigInfo() + "=");
    }

    ui.SoundJoystickValueListEditor->setTitleCaptions({LanguageSetup->Key(), LanguageSetup->Value()});
    {
        TVLE V(ui.SoundJoystickValueListEditor);
        auto YesNo = [&V]() { V.ReadOnly(); V.Pick(RemoveUnderline(LanguageSetup->Yes())); V.Pick(RemoveUnderline(LanguageSetup->No())); };
        V.Delete(0);
        V.Add(LanguageSetup->ProfileEditorSoundJoystickType() + "=");
        V.ReadOnly();
        V.PickListStyle();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->Joysticks(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundJoystickTimed() + "=");
        YesNo();
        V.Add(LanguageSetup->ProfileEditorSoundJoystickAutoFire() + "=");
        YesNo();
        V.Add(LanguageSetup->ProfileEditorSoundJoystickSwap34() + "=");
        YesNo();
        V.Add(LanguageSetup->ProfileEditorSoundJoystickButtonwrap() + "=");
        YesNo();
    }

    ui.SoundMiscValueListEditor->setTitleCaptions({LanguageSetup->Key(), LanguageSetup->Value()});
    {
        TVLE V(ui.SoundMiscValueListEditor);
        V.Delete(0);
        V.Add(LanguageSetup->ProfileEditorSoundMiscEnablePCSpeaker() + "=");
        V.ReadOnly();
        V.Pick(RemoveUnderline(LanguageSetup->Yes()));
        V.Pick(RemoveUnderline(LanguageSetup->No()));
        V.Add(LanguageSetup->ProfileEditorSoundMiscPCSpeakerRate() + "=");
        V.ReadOnly();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->PCRate(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundMiscEnableTandy() + "=");
        V.ReadOnly();
        V.Pick("auto");
        V.Pick("off");
        V.Pick("on");
        V.Add(LanguageSetup->ProfileEditorSoundMiscTandyRate() + "=");
        V.ReadOnly();
        V.PickAssign(ValueToList(GameDB->ConfOpt()->TandyRate(), ";,"));
        V.Add(LanguageSetup->ProfileEditorSoundMiscEnableDisneySoundsSource() + "=");
        V.ReadOnly();
        V.Pick(RemoveUnderline(LanguageSetup->Yes()));
        V.Pick(RemoveUnderline(LanguageSetup->No()));
    }

    SetTabCaption(ui.PageControl2, ui.SoundVolumeSheet, LanguageSetup->ProfileEditorSoundVolumeSheet());
    ui.SoundVolumeLeftLabel->setText(LanguageSetup->Left());
    ui.SoundVolumeRightLabel->setText(LanguageSetup->Right());
    ui.SoundVolumeMasterLabel->setText(LanguageSetup->ProfileEditorSoundMasterVolume());
    ui.SoundVolumeDisneyLabel->setText(LanguageSetup->ProfileEditorSoundMiscDisneySoundsSource());
    ui.SoundVolumeSpeakerLabel->setText(LanguageSetup->ProfileEditorSoundMiscPCSpeaker());
    ui.SoundVolumeGUSLabel->setText(LanguageSetup->ProfileEditorSoundGUS());
    ui.SoundVolumeSBLabel->setText(LanguageSetup->ProfileEditorSoundSoundBlaster());
    ui.SoundVolumeFMLabel->setText(LanguageSetup->ProfileEditorSoundFM());

    // Autoexec Sheet

    SetTabCaption(ui.PageControl, ui.AutoexecSheet, LanguageSetup->ProfileEditorAutoexecSheet());
    ui.AutoexecOverrideGameStartCheckBox->setText(LanguageSetup->ProfileEditorAutoexecOverrideGameStart());
    ui.AutoexecOverrideMountingCheckBox->setText(LanguageSetup->ProfileEditorAutoexecOverrideMounting());
    ui.AutoexecUse4DOSCheckBox->setText(LanguageSetup->ProfileEditorAutoexecUse4DOS());
    S = LanguageSetup->ProfileEditorAutoexecBat();
    if ((S != "") && (S[Length(S) - 1] == ':')) S = Trim(Copy(S, 1, Length(S) - 1));
    SetTabCaption(ui.AutoexecPageControl, ui.AutoexecSheet1, S);
    SetMemoFontName(ui.AutoexecMemo, "Courier New");
    ui.AutoexecClearButton->setText(LanguageSetup->Del());
    ui.AutoexecLoadButton->setText(LanguageSetup->Load());
    ui.AutoexecSaveButton->setText(LanguageSetup->Save());
    SetTabCaption(ui.AutoexecPageControl, ui.AutoexecSheet2, LanguageSetup->ProfileEditorFinalization());
    SetMemoFontName(ui.FinalizationMemo, "Courier New");
    ui.FinalizationClearButton->setText(LanguageSetup->Del());
    ui.FinalizationLoadButton->setText(LanguageSetup->Load());
    ui.FinalizationSaveButton->setText(LanguageSetup->Save());
    ui.AutoexecBootNormal->setText(LanguageSetup->ProfileEditorAutoexecBootNormal());
    ui.AutoexecBootHDImage->setText(LanguageSetup->ProfileEditorAutoexecBootHDImage());
    ui.AutoexecBootFloppyImage->setText(LanguageSetup->ProfileEditorAutoexecBootFloppyImage());
    ui.AutoexecBootFloppyImageInfoLabel->setText(LanguageSetup->ProfileMountingSwitchImage());
    ui.AutoexecBootFloppyImageButton->setToolTip(LanguageSetup->ChooseFile());
    ui.AutoexecBootFloppyImageAddButton->setToolTip(LanguageSetup->ProfileMountingAddImage());
    ui.AutoexecBootFloppyImageDelButton->setToolTip(LanguageSetup->ProfileMountingDelImage());

    // CustomSets Sheet

    SetTabCaption(ui.PageControl, ui.CustomSetsSheet, LanguageSetup->ProfileEditorCustomSetsSheet());
    SetMemoFontName(ui.CustomSetsMemo, "Courier New");
    ui.CustomSetsClearButton->setText(LanguageSetup->Del());
    ui.CustomSetsLoadButton->setText(LanguageSetup->Load());
    ui.CustomSetsSaveButton->setText(LanguageSetup->Save());
    ui.CustomSetsEnvLabel->setText(LanguageSetup->ProfileEditorCustomSetsEnvironment());
    ui.CustomSetsValueListEditor->setTitleCaptions({LanguageSetup->Key(), LanguageSetup->Value()});
    ui.CustomSetsEnvAdd->setText(LanguageSetup->Add());
    ui.CustomSetsEnvDel->setText(LanguageSetup->Del());

    Q_UNUSED(St);
    LoadLinks();
}

void TProfileEditorForm::LoadLinks()
{
    LinkFile->AddLinksToMenu(ui.SearchPopupMenu, 0, 1, [this](QObject *S) { SearchClick(S); }, 15);
    ui.SearchGameButton->setText(LanguageSetup->ProfileEditorLookUpGame() + " " + LinkFile->Name(0));
    ui.SearchGameButton->setToolTip(LinkFile->Link(0));
}

void TProfileEditorForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    Mounting = new TStringList();

    InitGUI();

    UserIconLoader->DirectLoad(ImageList, "ClassicProfileEditor");
    UserIconLoader->DialogImage(DI_Screenshot, ui.GenerateScreenshotFolderNameButton);
    UserIconLoader->DialogImage(DI_Add, ui.ExtraFilesAddButton);
    UserIconLoader->DialogImage(DI_Edit, ui.ExtraFilesEditButton);
    UserIconLoader->DialogImage(DI_Delete, ui.ExtraFilesDelButton);
    UserIconLoader->DialogImage(DI_Add, ui.ExtraDirsAddButton);
    UserIconLoader->DialogImage(DI_Edit, ui.ExtraDirsEditButton);
    UserIconLoader->DialogImage(DI_Delete, ui.ExtraDirsDelButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.GenerateGameDataFolderNameButton);
    UserIconLoader->DialogImage(DI_Internet, ImageList, 14);
    UserIconLoader->DialogImage(DI_Edit, ImageList, 15);
    UserIconLoader->DialogImage(DI_Add, ui.MountingAddButton);
    UserIconLoader->DialogImage(DI_Edit, ui.MountingEditButton);
    UserIconLoader->DialogImage(DI_Delete, ui.MountingDelButton);
    UserIconLoader->DialogImage(DI_Wizard, ui.MountingAutoCreateButton);
    UserIconLoader->DialogImage(DI_Clear, ui.AutoexecClearButton);
    UserIconLoader->DialogImage(DI_Load, ui.AutoexecLoadButton);
    UserIconLoader->DialogImage(DI_Save, ui.AutoexecSaveButton);
    UserIconLoader->DialogImage(DI_Clear, ui.FinalizationClearButton);
    UserIconLoader->DialogImage(DI_Load, ui.FinalizationLoadButton);
    UserIconLoader->DialogImage(DI_Save, ui.FinalizationSaveButton);
    UserIconLoader->DialogImage(DI_SelectFile, ui.AutoexecBootFloppyImageButton);
    UserIconLoader->DialogImage(DI_Add, ui.AutoexecBootFloppyImageAddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.AutoexecBootFloppyImageDelButton);
    UserIconLoader->DialogImage(DI_Clear, ui.CustomSetsClearButton);
    UserIconLoader->DialogImage(DI_Load, ui.CustomSetsLoadButton);
    UserIconLoader->DialogImage(DI_Save, ui.CustomSetsSaveButton);
    UserIconLoader->DialogImage(DI_Add, ui.CustomSetsEnvAdd);
    UserIconLoader->DialogImage(DI_Delete, ui.CustomSetsEnvDel);
    UserIconLoader->DialogImage(DI_Previous, ui.PreviousButton);
    UserIconLoader->DialogImage(DI_Next, ui.NextButton);
    ApplyImageLists();

    if ((Game == nullptr) && (LoadTemplate != nullptr)) {
        Game = LoadTemplate;
        try { LoadData(); } catch (...) { Game = nullptr; throw; }
        Game = nullptr;
    } else {
        LoadData();
        ProfileEditorOpenCheck(Game);
    }

    if (RestoreLastPosition) {
        ui.PageControl->setCurrentIndex(LastPage);
        if (LastTop > 0) move(x(), LastTop);
        if (LastLeft > 0) move(LastLeft, y());
    }
}

void TProfileEditorForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    delete Mounting;
    Mounting = nullptr;
}

void TProfileEditorForm::LoadData()
{
    TStringList *St, *St2;
    QString S, T;
    int I, ShiftNr;
    TValueListEditorWidget *E;

    OldFileName = "";

    // Profile Settings Sheet

    E = ui.ProfileSettingsValueListEditor;
    if (Game == nullptr) {
        SetVFI(E, 1, LanguageSetup->ProfileEditorNoFilename());
        SetVFI(E, 3, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 6, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 8, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 9, "64");
        SetVFI(E, 10, IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()));
    } else {
        IconName = Game->Icon();
        LoadIcon();
        if (LoadTemplate == nullptr) {
            if (Game->Name() != "") SetVFI(E, 0, Game->Name());
        }
        if (LoadTemplate != nullptr) {
            SetVFI(E, 1, LanguageSetup->ProfileEditorNoFilename());
        } else {
            if (Game->SetupFile() != "") {
                OldFileName = Game->SetupFile();
                SetVFI(E, 1, ChangeFileExt(ExtractFileName(Game->SetupFile()), ""));
                SetItemReadOnly(E, 1, false);
            } else {
                SetItemReadOnly(E, 0, true);
                SetVFI(E, 0, LanguageSetup->ProfileEditorNoFilename());
                SetVFI(E, 1, LanguageSetup->ProfileEditorNoFilename());
            }
        }

        SetVFI(E, 3, RemoveUnderline(LanguageSetup->No()));
        if (Game->GameExe() != "") {
            S = Trim(ExtUpperCase(Game->GameExe()));
            if (Copy(S, 1, 7) == "DOSBOX:") {
                SetVFI(E, 3, RemoveUnderline(LanguageSetup->Yes()));
                SetVFI(E, 2, Copy(Trim(Game->GameExe()), 8, MaxInt));
            } else {
                SetVFI(E, 2, Game->GameExe());
            }
        }
        if (Game->GameParameters() != "") SetVFI(E, 4, Game->GameParameters());

        SetVFI(E, 6, RemoveUnderline(LanguageSetup->No()));
        if (Game->SetupExe() != "") {
            S = Trim(ExtUpperCase(Game->SetupExe()));
            if (Copy(S, 1, 7) == "DOSBOX:") {
                SetVFI(E, 6, RemoveUnderline(LanguageSetup->Yes()));
                SetVFI(E, 5, Copy(Trim(Game->SetupExe()), 8, MaxInt));
            } else {
                SetVFI(E, 5, Game->SetupExe());
            }
        }
        if (Game->SetupParameters() != "") SetVFI(E, 7, Game->SetupParameters());

        if (Game->LoadFix()) SetVFI(E, 8, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 8, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 9, IntToStr(Game->LoadFixMemory()));
        if (Game->CaptureFolder() != "") SetVFI(E, 10, Game->CaptureFolder()); else SetVFI(E, 10, IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()));

        St = ValueToList(Game->ExtraFiles());
        I = 0; while (I < St->Count()) if (Trim((*St)[I]) == "") St->Delete(I); else I++;
        ui.ExtraFilesListBox->addItems(St->ToQStringList());
        delete St;
        if (ui.ExtraFilesListBox->count() > 0) ui.ExtraFilesListBox->setCurrentRow(0);
        St = ValueToList(Game->ExtraDirs());
        I = 0; while (I < St->Count()) if (Trim((*St)[I]) == "") St->Delete(I); else I++;
        ui.ExtraDirsListBox->addItems(St->ToQStringList());
        delete St;
        if (ui.ExtraDirsListBox->count() > 0) ui.ExtraDirsListBox->setCurrentRow(0);
    }
    ExtraDirsListBoxClick(nullptr);
    ExtraFilesListBoxClick(nullptr);

    // Game Info Sheet

    if (!HideGameInfoPage) {
        E = ui.GameInfoValueListEditor;
        if (Game == nullptr) {
            SetVFI(E, 6, RemoveUnderline(LanguageSetup->No()));
        } else {
            if (Game->Genre() != "") SetVFI(E, 0, GetCustomGenreName(Game->Genre()));
            if (Game->Developer() != "") SetVFI(E, 1, Game->Developer());
            if (Game->Publisher() != "") SetVFI(E, 2, Game->Publisher());
            if (Game->Year() != "") SetVFI(E, 3, Game->Year());
            if (Game->Language() != "") SetVFI(E, 4, GetCustomLanguageName(Game->Language()));
            if (Game->WWW(1) != "") SetVFI(E, 5, Game->WWW(1));
            if (Game->DataDir() != "") SetVFI(E, 6, Game->DataDir());
            if (Game->Favorite()) SetVFI(E, 7, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 7, RemoveUnderline(LanguageSetup->No()));
            St = StringToStringList(Game->Notes());
            SetMemoLines(ui.NotesMemo, *St);
            delete St;
        }
    }

    // General Sheet

    E = ui.GeneralValueListEditor;
    if (Game == nullptr) {
        SetVFI(E, 0, RemoveUnderline(LanguageSetup->Yes()));
        SetVFI(E, 1, RemoveUnderline(LanguageSetup->Yes()));
        SetVFI(E, 2, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 3, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 4, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 5, RemoveUnderline(LanguageSetup->Yes()));
        SetVFI(E, 6, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 7, RemoveUnderline(LanguageSetup->Yes()));
        SetVFI(E, 8, "100");
        SetVFI(E, 9, "surface");
        SetVFI(E, 10, "original");
        SetVFI(E, 11, "original");
        SetVFI(E, 12, "normal2x");
        if (PrgSetup->AllowTextModeLineChange()) {
            SetVFI(E, 13, "25");
            ShiftNr = 1;
        } else {
            ShiftNr = 0;
        }
        SetVFI(E, 13 + ShiftNr, "higher");
        SetVFI(E, 14 + ShiftNr, "normal");
        SetVFI(E, 15 + ShiftNr, "default");
        SetVFI(E, 16 + ShiftNr, "default");
    } else {
        if (Game->CloseDosBoxAfterGameExit()) SetVFI(E, 0, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 0, RemoveUnderline(LanguageSetup->No()));
        if (Game->StartFullscreen()) SetVFI(E, 1, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 1, RemoveUnderline(LanguageSetup->No()));
        if (Game->AutoLockMouse()) SetVFI(E, 2, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 2, RemoveUnderline(LanguageSetup->No()));
        if (Game->Force2ButtonMouseMode()) SetVFI(E, 3, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 3, RemoveUnderline(LanguageSetup->No()));
        if (Game->SwapMouseButtons()) SetVFI(E, 4, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 4, RemoveUnderline(LanguageSetup->No()));
        if (Game->UseDoublebuffering()) SetVFI(E, 5, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 5, RemoveUnderline(LanguageSetup->No()));
        if (Game->AspectCorrection()) SetVFI(E, 6, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 6, RemoveUnderline(LanguageSetup->No()));
        if (Game->UseScanCodes()) SetVFI(E, 7, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 7, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 8, IntToStr(Game->MouseSensitivity()));
        if (Game->Render() != "") SetVFI(E, 9, Game->Render());
        if (Game->WindowResolution() != "") SetVFI(E, 10, Game->WindowResolution());
        if (Game->FullscreenResolution() != "") SetVFI(E, 11, Game->FullscreenResolution());

        St = ValueToList(GameDB->ConfOpt()->Scale(), ";,");
        if (Game->Scale() == "") {
            if (St->Count() > 0) SetVFI(E, 12, (*St)[0]);
        } else {
            S = Trim(ExtUpperCase(Game->Scale()));
            for (I = 0; I <= St->Count() - 1; I++) {
                T = Trim(ExtUpperCase((*St)[I]));
                if (Pos('(', T) == 0) continue;
                T = Copy(T, Pos('(', T) + 1, MaxInt);
                if (Pos(')', T) == 0) continue;
                T = Copy(T, 1, Pos(')', T) - 1);
                if (Trim(T) == S) SetVFI(E, 12, (*St)[I]);
            }
        }
        delete St;

        if (PrgSetup->AllowTextModeLineChange()) {
            if ((Game->TextModeLines() == 25) || (Game->TextModeLines() == 28) || (Game->TextModeLines() == 50))
                SetVFI(E, 13, IntToStr(Game->TextModeLines()));
            else
                SetVFI(E, 13, "25");
            ShiftNr = 1;
        } else {
            ShiftNr = 0;
        }

        St = ValueToList(Game->Priority(), ",");
        if ((St->Count() >= 1) && ((*St)[0] != "")) SetVFI(E, 13 + ShiftNr, (*St)[0]);
        if ((St->Count() >= 2) && ((*St)[1] != "")) SetVFI(E, 14 + ShiftNr, (*St)[1]);
        delete St;

        if (Game->CustomDOSBoxDir() != "") SetVFI(E, 15 + ShiftNr, Game->CustomDOSBoxDir()); else SetVFI(E, 15 + ShiftNr, "default");
        if (Game->CustomKeyMappingFile() != "") SetVFI(E, 16 + ShiftNr, Game->CustomKeyMappingFile()); else SetVFI(E, 16 + ShiftNr, "default");
    }

    // Environment Sheet

    E = ui.EnvironmentValueListEditor;
    if (Game == nullptr) {
        SetVFI(E, 0, "32");
        SetVFI(E, 1, RemoveUnderline(LanguageSetup->Yes()));
        SetVFI(E, 2, RemoveUnderline(LanguageSetup->Yes()));
        SetVFI(E, 3, RemoveUnderline(LanguageSetup->Yes()));
        SetVFI(E, 4, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 5, "auto");
        SetVFI(E, 6, "500");
        SetVFI(E, 7, "20");
        SetVFI(E, 8, "auto");
        SetVFI(E, 9, "0");
        SetVFI(E, 10, "vga");
        SetVFI(E, 11, "default");
        SetVFI(E, 12, "default");
        SetVFI(E, 13, LanguageSetup->DoNotChange());
        SetVFI(E, 14, LanguageSetup->DoNotChange());
        SetVFI(E, 15, LanguageSetup->DoNotChange());
        SetVFI(E, 16, "dummy");
        SetVFI(E, 17, "dummy");
        SetVFI(E, 18, "disabled");
        SetVFI(E, 19, "disabled");
        SetVFI(E, 20, RemoveUnderline(LanguageSetup->No()));
        // 21-23: ''
        SetVFI(E, 24, "default");
    } else {
        SetVFI(E, 0, IntToStr(Game->Memory()));
        if (Game->XMS()) SetVFI(E, 1, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 1, RemoveUnderline(LanguageSetup->No()));
        if (Game->EMS()) SetVFI(E, 2, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 2, RemoveUnderline(LanguageSetup->No()));
        if (Game->UMB()) SetVFI(E, 3, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 3, RemoveUnderline(LanguageSetup->No()));
        if (Game->UseDOS32A()) SetVFI(E, 4, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 4, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 5, Game->Cycles());
        SetVFI(E, 6, IntToStr(Game->CyclesUp()));
        SetVFI(E, 7, IntToStr(Game->CyclesDown()));
        if (Game->Core() != "") SetVFI(E, 8, Game->Core());
        SetVFI(E, 9, IntToStr(Game->FrameSkip()));
        if (Game->VideoCard() != "") SetVFI(E, 10, Game->VideoCard());
        if (Game->KeyboardLayout() != "") SetVFI(E, 11, Game->KeyboardLayout()); else SetVFI(E, 11, "default");
        if (Game->Codepage() != "") SetVFI(E, 12, Game->Codepage()); else SetVFI(E, 12, "default");

        S = Trim(ExtUpperCase(Game->NumLockStatus()));
        if ((S == "ON") || (S == "1") || (S == "TRUE")) SetVFI(E, 13, LanguageSetup->On()); else {
            if ((S == "OFF") || (S == "0") || (S == "FALSE")) SetVFI(E, 13, LanguageSetup->Off()); else SetVFI(E, 13, LanguageSetup->DoNotChange());
        }
        S = Trim(ExtUpperCase(Game->CapsLockStatus()));
        if ((S == "ON") || (S == "1") || (S == "TRUE")) SetVFI(E, 14, LanguageSetup->On()); else {
            if ((S == "OFF") || (S == "0") || (S == "FALSE")) SetVFI(E, 14, LanguageSetup->Off()); else SetVFI(E, 14, LanguageSetup->DoNotChange());
        }
        S = Trim(ExtUpperCase(Game->ScrollLockStatus()));
        if ((S == "ON") || (S == "1") || (S == "TRUE")) SetVFI(E, 15, LanguageSetup->On()); else {
            if ((S == "OFF") || (S == "0") || (S == "FALSE")) SetVFI(E, 15, LanguageSetup->Off()); else SetVFI(E, 15, LanguageSetup->DoNotChange());
        }
        SetVFI(E, 16, Game->Serial1());
        SetVFI(E, 17, Game->Serial2());
        SetVFI(E, 18, Game->Serial3());
        SetVFI(E, 19, Game->Serial4());
        if (Game->IPX()) SetVFI(E, 20, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 20, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 21, LanguageSetup->GameIPXEstablishConnectionNone());
        S = Trim(ExtUpperCase(Game->IPXType()));
        if (S == "CLIENT") SetVFI(E, 21, LanguageSetup->GameIPXEstablishConnectionClient());
        if (S == "SERVER") SetVFI(E, 21, LanguageSetup->GameIPXEstablishConnectionServer());
        if (Game->IPXAddress() != "") SetVFI(E, 22, Game->IPXAddress());
        if (Game->IPXPort() != "") SetVFI(E, 23, Game->IPXPort());
        if (Game->ReportedDOSVersion() != "") SetVFI(E, 24, Game->ReportedDOSVersion());
    }

    // Mounting Sheet

    if (Game != nullptr) {
        for (I = 0; I <= 9; I++)
            if (Game->NrOfMounts() >= I + 1) Mounting->Add(Game->Mount(I)); else break;
        LoadMountingList();
        ui.AutoMountCheckBox->setChecked(Game->AutoMountCDs());
    }

    // Sound Sheet

    if (Game == nullptr) {
        E = ui.SoundValueListEditor;
        SetVFI(E, 0, RemoveUnderline(LanguageSetup->Yes()));
        SetVFI(E, 1, "22050");
        SetVFI(E, 2, "2048");
        SetVFI(E, 3, "10");
        E = ui.SoundSBValueListEditor;
        SetVFI(E, 0, "sb16");
        SetVFI(E, 1, "220");
        SetVFI(E, 2, "7");
        SetVFI(E, 3, "1");
        SetVFI(E, 4, "5");
        SetVFI(E, 5, "auto");
        SetVFI(E, 6, "22050");
        SetVFI(E, 7, RemoveUnderline(LanguageSetup->Yes()));
        E = ui.SoundGUSValueListEditor;
        SetVFI(E, 0, RemoveUnderline(LanguageSetup->Yes()));
        SetVFI(E, 1, "240");
        SetVFI(E, 2, "5");
        SetVFI(E, 3, "1");
        SetVFI(E, 4, "22050");
        SetVFI(E, 5, "C:\\ULTRASND");
        E = ui.SoundMIDIValueListEditor;
        SetVFI(E, 0, "intelligent");
        SetVFI(E, 1, "default");
        SetVFI(E, 2, "");
        E = ui.SoundJoystickValueListEditor;
        SetVFI(E, 0, "none");
        SetVFI(E, 1, RemoveUnderline(LanguageSetup->Yes()));
        SetVFI(E, 2, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 3, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 4, RemoveUnderline(LanguageSetup->Yes()));
        E = ui.SoundMiscValueListEditor;
        SetVFI(E, 0, RemoveUnderline(LanguageSetup->Yes()));
        SetVFI(E, 1, "22050");
        SetVFI(E, 2, "auto");
        SetVFI(E, 3, "22050");
        SetVFI(E, 4, RemoveUnderline(LanguageSetup->Yes()));
    } else {
        E = ui.SoundValueListEditor;
        if (!Game->MixerNosound()) SetVFI(E, 0, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 0, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 1, IntToStr(Game->MixerRate()));
        SetVFI(E, 2, IntToStr(Game->MixerBlocksize()));
        SetVFI(E, 3, IntToStr(Game->MixerPrebuffer()));
        E = ui.SoundSBValueListEditor;
        if (Game->SBType() != "") SetVFI(E, 0, Game->SBType());
        SetVFI(E, 1, Game->SBBase());
        SetVFI(E, 2, IntToStr(Game->SBIRQ()));
        SetVFI(E, 3, IntToStr(Game->SBDMA()));
        SetVFI(E, 4, IntToStr(Game->SBHDMA()));
        if (Game->SBOplMode() != "") SetVFI(E, 5, Game->SBOplMode());
        SetVFI(E, 6, IntToStr(Game->SBOplRate()));
        if (Game->SBMixer()) SetVFI(E, 7, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 7, RemoveUnderline(LanguageSetup->No()));
        E = ui.SoundGUSValueListEditor;
        if (Game->GUS()) SetVFI(E, 0, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 0, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 1, Game->GUSBase());
        SetVFI(E, 2, IntToStr(Game->GUSIRQ()));
        SetVFI(E, 3, IntToStr(Game->GUSDMA()));
        SetVFI(E, 4, IntToStr(Game->GUSRate()));
        if (Game->GUSUltraDir() != "") SetVFI(E, 5, Game->GUSUltraDir());
        E = ui.SoundMIDIValueListEditor;
        if (Game->MIDIType() != "") SetVFI(E, 0, Game->MIDIType());
        if (Game->MIDIDevice() != "") SetVFI(E, 1, Game->MIDIDevice());
        if (Game->MIDIConfig() != "") SetVFI(E, 2, Game->MIDIConfig());
        E = ui.SoundJoystickValueListEditor;
        if (Game->JoystickType() != "") SetVFI(E, 0, Game->JoystickType());
        if (Game->JoystickTimed()) SetVFI(E, 1, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 1, RemoveUnderline(LanguageSetup->No()));
        if (Game->JoystickAutoFire()) SetVFI(E, 2, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 2, RemoveUnderline(LanguageSetup->No()));
        if (Game->JoystickSwap34()) SetVFI(E, 3, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 3, RemoveUnderline(LanguageSetup->No()));
        if (Game->JoystickButtonwrap()) SetVFI(E, 4, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 4, RemoveUnderline(LanguageSetup->No()));
        E = ui.SoundMiscValueListEditor;
        if (Game->SpeakerPC()) SetVFI(E, 0, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 0, RemoveUnderline(LanguageSetup->No()));
        SetVFI(E, 1, IntToStr(Game->SpeakerRate()));
        SetVFI(E, 2, Game->SpeakerTandy());
        SetVFI(E, 3, IntToStr(Game->SpeakerTandyRate()));
        if (Game->SpeakerDisney()) SetVFI(E, 4, RemoveUnderline(LanguageSetup->Yes())); else SetVFI(E, 4, RemoveUnderline(LanguageSetup->No()));
        ui.SoundVolumeMasterLeftEdit->setValue(Game->MixerVolumeMasterLeft());
        ui.SoundVolumeMasterRightEdit->setValue(Game->MixerVolumeMasterRight());
        ui.SoundVolumeDisneyLeftEdit->setValue(Game->MixerVolumeDisneyLeft());
        ui.SoundVolumeDisneyRightEdit->setValue(Game->MixerVolumeDisneyRight());
        ui.SoundVolumeSpeakerLeftEdit->setValue(Game->MixerVolumeSpeakerLeft());
        ui.SoundVolumeSpeakerRightEdit->setValue(Game->MixerVolumeSpeakerRight());
        ui.SoundVolumeGUSLeftEdit->setValue(Game->MixerVolumeGUSLeft());
        ui.SoundVolumeGUSRightEdit->setValue(Game->MixerVolumeGUSRight());
        ui.SoundVolumeSBLeftEdit->setValue(Game->MixerVolumeSBLeft());
        ui.SoundVolumeSBRightEdit->setValue(Game->MixerVolumeSBRight());
        ui.SoundVolumeFMLeftEdit->setValue(Game->MixerVolumeFMLeft());
        ui.SoundVolumeFMRightEdit->setValue(Game->MixerVolumeFMRight());
    }

    // Autoexec Sheet

    if (Game == nullptr) {
        ui.AutoexecOverrideGameStartCheckBox->setChecked(false);
        ui.AutoexecOverrideMountingCheckBox->setChecked(false);
        ui.AutoexecUse4DOSCheckBox->setChecked(false);
    } else {
        ui.AutoexecOverrideGameStartCheckBox->setChecked(Game->AutoexecOverridegamestart());
        ui.AutoexecOverrideMountingCheckBox->setChecked(Game->AutoexecOverrideMount());
        ui.AutoexecUse4DOSCheckBox->setChecked(Game->Use4DOS());
        St = StringToStringList(Game->Autoexec()); SetMemoLines(ui.AutoexecMemo, *St); delete St;
        St = StringToStringList(Game->AutoexecFinalization()); SetMemoLines(ui.FinalizationMemo, *St); delete St;
        S = Trim(Game->AutoexecBootImage());
        if ((S == "2") || (S == "3")) {
            ui.AutoexecBootHDImage->setChecked(true);
            if (S == "2") ui.AutoexecBootHDImageComboBox->setCurrentIndex(0); else ui.AutoexecBootHDImageComboBox->setCurrentIndex(1);
        } else if (S != "") {
            ui.AutoexecBootFloppyImage->setChecked(true);
            St2 = new TStringList();
            S = Trim(S);
            I = Pos('$', S);
            while (I > 0) { St2->Add(Trim(Copy(S, 1, I - 1))); S = Trim(Copy(S, I + 1, MaxInt)); I = Pos('$', S); }
            St2->Add(S);
            ui.AutoexecBootFloppyImageTab->setRowCount(St2->Count());
            for (I = 0; I <= St2->Count() - 1; I++) SetGridCell(ui.AutoexecBootFloppyImageTab, I, (*St2)[I]);
            delete St2;
        }
    }
    if (ui.AutoexecBootFloppyImageTab->currentRow() < 0) ui.AutoexecBootFloppyImageTab->setCurrentCell(0, 0);

    // CustomSets Sheet

    if (Game == nullptr) {
        TVLE(ui.CustomSetsValueListEditor).Add("PATH=Z:\\");
    } else {
        St = StringToStringList(Game->CustomSettings());
        SetMemoLines(ui.CustomSetsMemo, *St);
        delete St;
        St = StringToStringList(Game->Environment());
        for (I = 0; I <= St->Count() - 1; I++) if (Trim((*St)[I]) != "")
            TVLE(ui.CustomSetsValueListEditor).Add(Replace((*St)[I], "[" + IntToStr('=') + "]", "="));
        delete St;
    }

    // Sheet Nr

    if (PrgSetup->ReopenLastProfileEditorTab() && (Game != nullptr)) {
        switch (Game->LastOpenTab()) {
            case 0: case 1: case 2: case 3: case 4: case 5: ui.PageControl->setCurrentIndex(Game->LastOpenTab()); break;
            case 6: ui.PageControl->setCurrentIndex(7); break;
            case 7: ui.PageControl->setCurrentIndex(6); break;
            case 8: ui.PageControl->setCurrentIndex(2); break;
            case 9: ui.PageControl->setCurrentIndex(5); break;
        }
    } else {
        ui.PageControl->setCurrentIndex(0);
    }

    // DOSBox Staging Sheet

    for (I = 0; I < StagingSettingCount; I++) {
        S = (Game == nullptr) ? QString() : Trim(Game->StagingSetting(I));
        SetVFI(StagingValueListEditor, I, S.isEmpty() ? QStringLiteral("default") : S);
    }

    MarkIgnoredDOSBoxSettings();

    // Set profile name in caption

    ProfileSettingsValueListEditorSetEditText(ui.ProfileSettingsValueListEditor, ui.ProfileSettingsValueListEditor->currentColumn(), ui.ProfileSettingsValueListEditor->currentRow() + 1, "");
}

// Rows of settings the DOSBox version of the profile does not use (e.g. the
// scaler for DOSBox Staging) are greyed out and read only; their values stay
// in the profile for other DOSBox versions.
static void SetRowIgnored(TValueListEditorWidget* E, int Row, bool Ignored, const QString& Hint)
{
    if ((Row < 0) || (Row >= E->rowCount())) return;
    QTableWidgetItem* K = E->item(Row, 0);
    if (K == nullptr) return;
    constexpr int RoleIgnored = Qt::UserRole + 70, RoleWasReadOnly = Qt::UserRole + 71;
    if (K->data(RoleIgnored).toBool() == Ignored) return;
    const QSignalBlocker Blocker(E);
    if (Ignored) K->setData(RoleWasReadOnly, E->isReadOnly(Row));
    E->setReadOnly(Row, Ignored || K->data(RoleWasReadOnly).toBool());
    for (int C = 0; C < 2; C++) {
        QTableWidgetItem* I = E->item(Row, C);
        if (I == nullptr) continue;
        if (Ignored) {
            I->setForeground(E->palette().brush(QPalette::Disabled, QPalette::Text));
            I->setToolTip(Hint);
        } else {
            I->setData(Qt::ForegroundRole, QVariant());
            I->setToolTip(QString());
        }
    }
    K->setData(RoleIgnored, Ignored);
}

void TProfileEditorForm::MarkIgnoredDOSBoxSettings()
{
    const int ShiftNr = PrgSetup->AllowTextModeLineChange() ? 1 : 0;
    QString Installation = Trim(ui.GeneralValueListEditor->value(15 + ShiftNr));
    if (ExtUpperCase(Installation) == "DEFAULT") Installation.clear();
    const TDOSBoxKind Kind = DOSBoxKindOfProfileInstallation(Installation);
    const QString Hint = LanguageSetup->ProfileEditorDOSBoxSettingIgnored();
    struct TRow { TValueListEditorWidget* E; int Row; const char* Section; const char* Key; };
    const TRow Rows[] = {
        {ui.GeneralValueListEditor, 5, "sdl", "fulldouble"},
        {ui.GeneralValueListEditor, 7, "sdl", "usescancodes"},
        {ui.GeneralValueListEditor, 9, "sdl", "output"},
        {ui.GeneralValueListEditor, 11, "sdl", "fullresolution"},
        {ui.GeneralValueListEditor, 12, "render", "scaler"},
        {ui.GeneralValueListEditor, 13 + ShiftNr, "sdl", "priority"},
        {ui.GeneralValueListEditor, 14 + ShiftNr, "sdl", "priority"},
        {ui.EnvironmentValueListEditor, 9, "render", "frameskip"},
        {ui.SoundSBValueListEditor, 6, "sblaster", "oplrate"},
        {ui.SoundGUSValueListEditor, 4, "gus", "gusrate"},
        {ui.SoundMiscValueListEditor, 1, "speaker", "pcrate"},
        {ui.SoundMiscValueListEditor, 3, "speaker", "tandyrate"},
    };
    for (const TRow& R : Rows) SetRowIgnored(R.E, R.Row, DOSBoxSettingIgnored(Kind, QLatin1String(R.Section), QLatin1String(R.Key)), Hint);
    // ("DOSBox Staging" tab: only used by DOSBox Staging)
    for (int I = 0; I < StagingSettingCount; I++) SetRowIgnored(StagingValueListEditor, I, Kind != dkStaging, LanguageSetup->ProfileEditorStagingOnly());
}

void TProfileEditorForm::InitStagingSheet()
{
    auto* Page = new QWidget(ui.PageControl);
    auto* Layout = new QVBoxLayout(Page);
    auto* Info = new QLabel(LanguageSetup->ProfileEditorStagingInfo(), Page);
    Info->setWordWrap(true);
    Layout->addWidget(Info);
    StagingValueListEditor = new TValueListEditorWidget(Page);
    StagingValueListEditor->setTitleCaptions({LanguageSetup->Key(), LanguageSetup->Value()});
    Layout->addWidget(StagingValueListEditor, 1);
    TVLE V(StagingValueListEditor);
    while (StagingValueListEditor->rowCount() > 0) V.Delete(0);
    for (int I = 0; I < StagingSettingCount; I++) {
        const TStagingSettingInfo& S = StagingSettings[I];
        V.Add(LanguageSetup->ProfileEditorStagingSetting(I) + "=default");
        if (S.Kind == skList) V.ReadOnly();
        if ((S.Kind == skFile) || (S.Kind == skFolder)) V.Ellipsis();
        else for (const QString& Value : QString::fromLatin1(S.Values).split(';')) V.Pick(Value);
    }
    StagingValueListEditor->setColumnWidth(0, 300);   // (long names)
    connect(StagingValueListEditor, &TValueListEditorWidget::editButtonClicked, this, [this](int Row) { StagingEditButtonClick(Row); });
    // (added as the last tab: the other code uses the indexes of the tabs)
    ui.PageControl->addTab(Page, QStringLiteral("DOSBox Staging"));
}

void TProfileEditorForm::StagingEditButtonClick(int Row)
{
    if ((Row < 0) || (Row >= StagingSettingCount) || StagingValueListEditor->isReadOnly(Row)) return;
    QString S = Trim(VFI(StagingValueListEditor, Row));
    if (ExtUpperCase(S) == "DEFAULT") S.clear();
    const QString Start = S.isEmpty() ? PrgSetup->BaseDir() : MakeAbsPath(S, PrgSetup->BaseDir());
    if (StagingSettings[Row].Kind == skFolder)
        S = QFileDialog::getExistingDirectory(this, LanguageSetup->ProfileEditorStagingSetting(Row), Start);
    else
        S = QFileDialog::getOpenFileName(this, LanguageSetup->ProfileEditorStagingSetting(Row), Start, QStringLiteral("SoundFont (*.sf2 *.sf3);;* (*)"));
    if (!S.isEmpty()) SetVFI(StagingValueListEditor, Row, MakeRelPath(QDir::toNativeSeparators(S), PrgSetup->BaseDir()));
}

void TProfileEditorForm::LoadIcon()
{
    QString S;

    if (IconName == "") {
        ui.IconImage->clearPicture();
        return;
    }
    try {
        S = MakeAbsIconName(IconName);
        if (FileExists(S)) ui.IconImage->setPicture(LoadImageFromFile(S));
    } catch (...) {}
}

void TProfileEditorForm::LoadMountingList()
{
    LoadMountingListView(ui.MountingListView, Mounting);
}

bool TProfileEditorForm::CheckProfile()
{
    int I;
    QString S1, S2, S;
    bool B = false;
    TValueListEditorWidget *E;

    // Profile Settings Sheet

    E = ui.ProfileSettingsValueListEditor;
    S1 = VFI(E, 9);
    if (!TryStrToInt(Trim(S1), I)) {
        if (Game != nullptr) S2 = IntToStr(Game->LoadFixMemory()); else S2 = IntToStr(DefaultValueReaderGame->LoadFixMemory());
        if (MessageDlg(Format(LanguageSetup->MessageInvalidValue(), {S1, LanguageSetup->ProfileEditorLoadFixMemory(), S2}), mtWarning, mbYes | mbNo, 0) != mrYes) {
            ui.PageControl->setCurrentIndex(0); return false;
        }
    }

    S1 = VFI(E, 2);
    if ((!EditingTemplate) && (LoadTemplate != nullptr) && (Trim(S1) == "") && ui.AutoexecBootNormal->isChecked()) {
        if (MessageDlg(LanguageSetup->MessageNoGameFileNameWarning(), mtConfirmation, mbYes | mbNo, 0) != mrYes) {
            ui.PageControl->setCurrentIndex(0); return false;
        }
    }

    S1 = VFI(E, 2);
    S2 = VFI(E, 5);
    if ((!EditingTemplate) && ui.AutoexecBootNormal->isChecked()) {
        if ((VFI(E, 3) == RemoveUnderline(LanguageSetup->No())) && (Trim(S1) != "")) {
            S = MakeAbsPath(S1, PrgSetup->BaseDir());
            if (IsWindowsExe(S)) {
                if ((Game == nullptr) || (!Game->IgnoreWindowsFileWarnings())) {
                    if (MessageDlg(Format(LanguageSetup->MessageWindowsExeEditWarning(), {S}), mtConfirmation, mbYes | mbNo, 0) != mrYes) {
                        ui.PageControl->setCurrentIndex(0); return false;
                    }
                }
            }
        }
        if ((VFI(E, 6) == RemoveUnderline(LanguageSetup->No())) && (Trim(S2) != "")) {
            S = MakeAbsPath(S2, PrgSetup->BaseDir());
            if (IsWindowsExe(S)) {
                if ((Game == nullptr) || (!Game->IgnoreWindowsFileWarnings())) {
                    if (MessageDlg(Format(LanguageSetup->MessageWindowsExeEditWarning(), {S}), mtConfirmation, mbYes | mbNo, 0) != mrYes) {
                        ui.PageControl->setCurrentIndex(0); return false;
                    }
                }
            }
        }
    }

    // General Sheet

    E = ui.GeneralValueListEditor;
    S1 = VFI(E, 8);
    if (!TryStrToInt(Trim(S1), I)) {
        if (Game != nullptr) S2 = IntToStr(Game->MouseSensitivity()); else S2 = IntToStr(DefaultValueReaderGame->MouseSensitivity());
        if (MessageDlg(Format(LanguageSetup->MessageInvalidValue(), {S1, LanguageSetup->GameMouseSensitivity(), S2}), mtWarning, mbYes | mbNo, 0) != mrYes) {
            ui.PageControl->setCurrentIndex(2); return false;
        }
    }

    S1 = VFI(E, 12);
    if (Pos('(', S1) == 0) B = false; else {
        S1 = Copy(S1, Pos('(', S1) + 1, MaxInt);
        B = (Pos(')', S1) != 0);
    }
    if (!B) {
        S1 = VFI(E, 12);
        if (Game != nullptr) S2 = Game->Scale(); else S2 = DefaultValueReaderGame->Scale();
        if (MessageDlg(Format(LanguageSetup->MessageInvalidValue(), {S1, LanguageSetup->GameScale(), S2}), mtWarning, mbYes | mbNo, 0) != mrYes) {
            ui.PageControl->setCurrentIndex(2); return false;
        }
    }

    // Environment Sheet

    E = ui.EnvironmentValueListEditor;
    S1 = VFI(E, 0);
    if (!TryStrToInt(Trim(S1), I)) {
        if (Game != nullptr) S2 = IntToStr(Game->Memory()); else S2 = IntToStr(DefaultValueReaderGame->Memory());
        if (MessageDlg(Format(LanguageSetup->MessageInvalidValue(), {S1, LanguageSetup->GameMemory(), S2}), mtWarning, mbYes | mbNo, 0) != mrYes) {
            ui.PageControl->setCurrentIndex(3); return false;
        }
    }

    S1 = VFI(E, 6);
    if (!TryStrToInt(Trim(S1), I)) {
        if (Game != nullptr) S2 = IntToStr(Game->CyclesUp()); else S2 = IntToStr(DefaultValueReaderGame->CyclesUp());
        if (MessageDlg(Format(LanguageSetup->MessageInvalidValue(), {S1, LanguageSetup->GameCyclesUp(), S2}), mtWarning, mbYes | mbNo, 0) != mrYes) {
            ui.PageControl->setCurrentIndex(3); return false;
        }
    }

    S1 = VFI(E, 7);
    if (!TryStrToInt(Trim(S1), I)) {
        if (Game != nullptr) S2 = IntToStr(Game->CyclesDown()); else S2 = IntToStr(DefaultValueReaderGame->CyclesDown());
        if (MessageDlg(Format(LanguageSetup->MessageInvalidValue(), {S1, LanguageSetup->GameCyclesDown(), S2}), mtWarning, mbYes | mbNo, 0) != mrYes) {
            ui.PageControl->setCurrentIndex(3); return false;
        }
    }

    S1 = VFI(E, 9);
    if (!TryStrToInt(Trim(S1), I)) {
        if (Game != nullptr) S2 = IntToStr(Game->FrameSkip()); else S2 = IntToStr(DefaultValueReaderGame->FrameSkip());
        if (MessageDlg(Format(LanguageSetup->MessageInvalidValue(), {S1, LanguageSetup->GameFrameskip(), S2}), mtWarning, mbYes | mbNo, 0) != mrYes) {
            ui.PageControl->setCurrentIndex(3); return false;
        }
    }

    // Note: as in the original code B keeps its value from the scale check if no IPX value matches
    S1 = Trim(ExtUpperCase(VFI(E, 21)));
    if (S1 == Trim(ExtUpperCase(LanguageSetup->GameIPXEstablishConnectionNone()))) B = true;
    if (S1 == Trim(ExtUpperCase(LanguageSetup->GameIPXEstablishConnectionClient()))) B = true;
    if (S1 == Trim(ExtUpperCase(LanguageSetup->GameIPXEstablishConnectionServer()))) B = true;
    if (!B) {
        S1 = VFI(E, 21);
        if (Game != nullptr) S2 = Game->IPXType(); else S2 = DefaultValueReaderGame->IPXType();
        if (MessageDlg(Format(LanguageSetup->MessageInvalidValue(), {S1, LanguageSetup->GameIPX(), S2}), mtWarning, mbYes | mbNo, 0) != mrYes) {
            ui.PageControl->setCurrentIndex(3); return false;
        }
    }

    // Sound Sheet

    struct TIntCheck { TValueListEditorWidget *E; int Index; QString Name; std::function<QString(TGame *)> Current; };
    const QList<TIntCheck> Checks = {
        {ui.SoundValueListEditor, 1, LanguageSetup->ProfileEditorSoundSampleRate(), [](TGame *G) { return IntToStr(G->MixerRate()); }},
        {ui.SoundValueListEditor, 2, LanguageSetup->ProfileEditorSoundBlockSize(), [](TGame *G) { return IntToStr(G->MixerBlocksize()); }},
        {ui.SoundValueListEditor, 3, LanguageSetup->ProfileEditorSoundPrebuffer(), [](TGame *G) { return IntToStr(G->MixerPrebuffer()); }},
        {ui.SoundSBValueListEditor, 1, LanguageSetup->ProfileEditorSoundSBAddress(), [](TGame *G) { return G->SBBase(); }},
        {ui.SoundSBValueListEditor, 2, LanguageSetup->ProfileEditorSoundSBIRQ(), [](TGame *G) { return IntToStr(G->SBIRQ()); }},
        {ui.SoundSBValueListEditor, 3, LanguageSetup->ProfileEditorSoundSBDMA(), [](TGame *G) { return IntToStr(G->SBDMA()); }},
        {ui.SoundSBValueListEditor, 4, LanguageSetup->ProfileEditorSoundSBHDMA(), [](TGame *G) { return IntToStr(G->SBHDMA()); }},
        {ui.SoundSBValueListEditor, 6, LanguageSetup->ProfileEditorSoundSBOplRate(), [](TGame *G) { return IntToStr(G->SBOplRate()); }},
        {ui.SoundMiscValueListEditor, 1, LanguageSetup->ProfileEditorSoundMiscPCSpeakerRate(), [](TGame *G) { return IntToStr(G->SpeakerRate()); }},
        {ui.SoundMiscValueListEditor, 3, LanguageSetup->ProfileEditorSoundMiscTandyRate(), [](TGame *G) { return IntToStr(G->SpeakerTandyRate()); }}};
    for (const TIntCheck &C : Checks) {
        S1 = VFI(C.E, C.Index);
        if (!TryStrToInt(Trim(S1), I)) {
            if (Game != nullptr) S2 = C.Current(Game); else S2 = C.Current(DefaultValueReaderGame);
            if (MessageDlg(Format(LanguageSetup->MessageInvalidValue(), {S1, C.Name, S2}), mtWarning, mbYes | mbNo, 0) != mrYes) {
                ui.PageControl->setCurrentIndex(5); return false;
            }
        }
    }

    return true;
}

void TProfileEditorForm::OKButtonClick(QObject* Sender)
{
    int I, ShiftNr;
    TStringList* St;
    QString S, NewFileName;
    TValueListEditorWidget *E;

    if (!CheckProfile()) {
        setModalResult(mrNone);
        return;
    }

    switch (Sender->property("tag").toInt()) {
        case 0: MoveStatus = 0; break;
        case 1: MoveStatus = -1; break;
        case 2: MoveStatus = 1; break;
    }

    // An exception in an event handler is shown by Application.HandleException in Delphi.
    try {
        if (Game == nullptr) {
            I = GameDB->Add(VFI(ui.ProfileSettingsValueListEditor, 0));
            Game = GameDB->Game(I);
        }

        // Profile Settings Sheet

        Game->SetIcon(IconName);
        E = ui.ProfileSettingsValueListEditor;
        if (!ItemReadOnly(E, 0)) Game->SetName(VFI(E, 0));
        ProfileEditorCloseCheck(Game, VFI(E, 2), VFI(E, 4));
        if (VFI(E, 3) == RemoveUnderline(LanguageSetup->Yes())) S = "DOSBox:"; else S = "";
        Game->SetGameExe(S + VFI(E, 2));
        Game->SetGameParameters(VFI(E, 4));
        if (VFI(E, 6) == RemoveUnderline(LanguageSetup->Yes())) S = "DOSBox:"; else S = "";
        Game->SetSetupExe(S + VFI(E, 5));
        Game->SetSetupParameters(VFI(E, 7));
        Game->SetLoadFix(VFI(E, 8) == RemoveUnderline(LanguageSetup->Yes()));
        try { Game->SetLoadFixMemory(StrToInt(Trim(VFI(E, 9)))); } catch (...) {}
        Game->SetCaptureFolder(MakeRelPath(VFI(E, 10), PrgSetup->BaseDir()));
        {
            TStringList L = ListBoxItems(ui.ExtraFilesListBox);
            Game->SetExtraFiles(ListToValue(&L));
            L = ListBoxItems(ui.ExtraDirsListBox);
            Game->SetExtraDirs(ListToValue(&L));
        }

        // Game Info Sheet

        if (!HideGameInfoPage) {
            E = ui.GameInfoValueListEditor;
            Game->SetGenre(GetEnglishGenreName(VFI(E, 0)));
            Game->SetDeveloper(VFI(E, 1));
            Game->SetPublisher(VFI(E, 2));
            Game->SetYear(VFI(E, 3));
            Game->SetLanguage(GetEnglishLanguageName(VFI(E, 4)));
            Game->SetWWW(1, VFI(E, 5));
            Game->SetDataDir(MakeRelPath(VFI(E, 6), PrgSetup->BaseDir()));
            Game->SetFavorite(VFI(E, 7) == RemoveUnderline(LanguageSetup->Yes()));
            TStringList L = MemoLines(ui.NotesMemo);
            Game->SetNotes(StringListToString(&L));
        }

        // General Sheet

        E = ui.GeneralValueListEditor;
        Game->SetCloseDosBoxAfterGameExit(VFI(E, 0) == RemoveUnderline(LanguageSetup->Yes()));
        Game->SetStartFullscreen(VFI(E, 1) == RemoveUnderline(LanguageSetup->Yes()));
        Game->SetAutoLockMouse(VFI(E, 2) == RemoveUnderline(LanguageSetup->Yes()));
        Game->SetForce2ButtonMouseMode(VFI(E, 3) == RemoveUnderline(LanguageSetup->Yes()));
        Game->SetSwapMouseButtons(VFI(E, 4) == RemoveUnderline(LanguageSetup->Yes()));
        Game->SetUseDoublebuffering(VFI(E, 5) == RemoveUnderline(LanguageSetup->Yes()));
        Game->SetAspectCorrection(VFI(E, 6) == RemoveUnderline(LanguageSetup->Yes()));
        Game->SetUseScanCodes(VFI(E, 7) == RemoveUnderline(LanguageSetup->Yes()));
        try { Game->SetMouseSensitivity(StrToInt(Trim(VFI(E, 8)))); } catch (...) {}
        Game->SetRender(VFI(E, 9));
        Game->SetWindowResolution(VFI(E, 10));
        Game->SetFullscreenResolution(VFI(E, 11));
        S = VFI(E, 12);
        if (Pos('(', S) == 0) Game->SetScale(""); else {
            S = Copy(S, Pos('(', S) + 1, MaxInt);
            if (Pos(')', S) == 0) Game->SetScale(""); else Game->SetScale(Copy(S, 1, Pos(')', S) - 1));
        }

        if (PrgSetup->AllowTextModeLineChange()) {
            try { Game->SetTextModeLines(StrToInt(VFI(E, 13))); } catch (...) {}
            ShiftNr = 1;
        } else {
            ShiftNr = 0;
        }
        Game->SetPriority(VFI(E, 13 + ShiftNr) + "," + VFI(E, 14 + ShiftNr));
        Game->SetCustomDOSBoxDir(VFI(E, 15 + ShiftNr));
        Game->SetCustomKeyMappingFile(VFI(E, 16 + ShiftNr));

        // Environment Sheet

        E = ui.EnvironmentValueListEditor;
        try { Game->SetMemory(StrToInt(Trim(VFI(E, 0)))); } catch (...) {}
        Game->SetXMS(VFI(E, 1) == RemoveUnderline(LanguageSetup->Yes()));
        Game->SetEMS(VFI(E, 2) == RemoveUnderline(LanguageSetup->Yes()));
        Game->SetUMB(VFI(E, 3) == RemoveUnderline(LanguageSetup->Yes()));
        Game->SetUseDOS32A(VFI(E, 4) == RemoveUnderline(LanguageSetup->Yes()));
        Game->SetCycles(VFI(E, 5));
        try { Game->SetCyclesUp(StrToInt(Trim(VFI(E, 6)))); } catch (...) {}
        try { Game->SetCyclesDown(StrToInt(Trim(VFI(E, 7)))); } catch (...) {}
        Game->SetCore(VFI(E, 8));
        try { Game->SetFrameSkip(StrToInt(Trim(VFI(E, 9)))); } catch (...) {}
        Game->SetVideoCard(VFI(E, 10));
        Game->SetKeyboardLayout(VFI(E, 11));
        Game->SetCodepage(VFI(E, 12));
        if (VFI(E, 13) == LanguageSetup->DoNotChange()) Game->SetNumLockStatus("");
        if (VFI(E, 13) == LanguageSetup->Off()) Game->SetNumLockStatus("off");
        if (VFI(E, 13) == LanguageSetup->On()) Game->SetNumLockStatus("on");
        if (VFI(E, 14) == LanguageSetup->DoNotChange()) Game->SetCapsLockStatus("");
        if (VFI(E, 14) == LanguageSetup->Off()) Game->SetCapsLockStatus("off");
        if (VFI(E, 14) == LanguageSetup->On()) Game->SetCapsLockStatus("on");
        if (VFI(E, 15) == LanguageSetup->DoNotChange()) Game->SetScrollLockStatus("");
        if (VFI(E, 15) == LanguageSetup->Off()) Game->SetScrollLockStatus("off");
        if (VFI(E, 15) == LanguageSetup->On()) Game->SetScrollLockStatus("on");
        Game->SetSerial1(VFI(E, 16));
        Game->SetSerial2(VFI(E, 17));
        Game->SetSerial3(VFI(E, 18));
        Game->SetSerial4(VFI(E, 19));
        Game->SetIPX(VFI(E, 20) == RemoveUnderline(LanguageSetup->Yes()));
        if (Trim(ExtUpperCase(VFI(E, 21))) == Trim(ExtUpperCase(LanguageSetup->GameIPXEstablishConnectionNone()))) Game->SetIPXType("none");
        if (Trim(ExtUpperCase(VFI(E, 21))) == Trim(ExtUpperCase(LanguageSetup->GameIPXEstablishConnectionClient()))) Game->SetIPXType("client");
        if (Trim(ExtUpperCase(VFI(E, 21))) == Trim(ExtUpperCase(LanguageSetup->GameIPXEstablishConnectionServer()))) Game->SetIPXType("server");
        Game->SetIPXAddress(VFI(E, 22));
        Game->SetIPXPort(VFI(E, 23));
        Game->SetReportedDOSVersion(VFI(E, 24));

        // Mounting Sheet

        Game->SetNrOfMounts(Mounting->Count());
        for (I = 0; I <= 9; I++)
            if (Mounting->Count() > I) Game->SetMount(I, (*Mounting)[I]); else Game->SetMount(I, "");
        Game->SetAutoMountCDs(ui.AutoMountCheckBox->isChecked());

        // Sound Sheet

        E = ui.SoundValueListEditor;
        // Note: index 1 (instead of 0) as in the original code
        Game->SetMixerNosound(VFI(E, 1) == RemoveUnderline(LanguageSetup->No()));
        try { Game->SetMixerRate(StrToInt(Trim(VFI(E, 1)))); } catch (...) {}
        try { Game->SetMixerBlocksize(StrToInt(Trim(VFI(E, 2)))); } catch (...) {}
        try { Game->SetMixerPrebuffer(StrToInt(Trim(VFI(E, 3)))); } catch (...) {}
        E = ui.SoundSBValueListEditor;
        Game->SetSBType(VFI(E, 0));
        Game->SetSBBase(Trim(VFI(E, 1)));
        try { Game->SetSBIRQ(StrToInt(Trim(VFI(E, 2)))); } catch (...) {}
        try { Game->SetSBDMA(StrToInt(Trim(VFI(E, 3)))); } catch (...) {}
        try { Game->SetSBHDMA(StrToInt(Trim(VFI(E, 4)))); } catch (...) {}
        Game->SetSBOplMode(VFI(E, 5));
        try { Game->SetSBOplRate(StrToInt(Trim(VFI(E, 6)))); } catch (...) {}
        Game->SetSBMixer(VFI(E, 7) == RemoveUnderline(LanguageSetup->Yes()));
        E = ui.SoundGUSValueListEditor;
        Game->SetGUS(VFI(E, 0) == RemoveUnderline(LanguageSetup->Yes()));
        Game->SetGUSBase(Trim(VFI(E, 1)));
        Game->SetGUSIRQ(StrToInt(Trim(VFI(E, 2))));
        Game->SetGUSDMA(StrToInt(Trim(VFI(E, 3))));
        Game->SetGUSRate(StrToInt(Trim(VFI(E, 4))));
        Game->SetGUSUltraDir(VFI(E, 5));
        E = ui.SoundMIDIValueListEditor;
        Game->SetMIDIType(VFI(E, 0));
        Game->SetMIDIDevice(VFI(E, 1));
        Game->SetMIDIConfig(VFI(E, 2));
        E = ui.SoundJoystickValueListEditor;
        Game->SetJoystickType(VFI(E, 0));
        Game->SetJoystickTimed(VFI(E, 1) == RemoveUnderline(LanguageSetup->Yes()));
        Game->SetJoystickAutoFire(VFI(E, 2) == RemoveUnderline(LanguageSetup->Yes()));
        Game->SetJoystickSwap34(VFI(E, 3) == RemoveUnderline(LanguageSetup->Yes()));
        Game->SetJoystickButtonwrap(VFI(E, 4) == RemoveUnderline(LanguageSetup->Yes()));
        E = ui.SoundMiscValueListEditor;
        Game->SetSpeakerPC(VFI(E, 0) == RemoveUnderline(LanguageSetup->Yes()));
        try { Game->SetSpeakerRate(StrToInt(Trim(VFI(E, 1)))); } catch (...) {}
        // Note: the original code writes the value back to the editor here (SpeakerTandy is never stored)
        SetVFI(E, 2, Game->SpeakerTandy());
        try { Game->SetSpeakerTandyRate(StrToInt(Trim(VFI(E, 3)))); } catch (...) {}
        Game->SetSpeakerDisney(VFI(E, 4) == RemoveUnderline(LanguageSetup->Yes()));
        for (int J = 0; J < StagingSettingCount; J++) {
            const QString V = Trim(VFI(StagingValueListEditor, J));
            Game->SetStagingSetting(J, V.isEmpty() ? QStringLiteral("default") : V);
        }
        Game->SetMixerVolumeMasterLeft(ui.SoundVolumeMasterLeftEdit->value());
        Game->SetMixerVolumeMasterRight(ui.SoundVolumeMasterRightEdit->value());
        Game->SetMixerVolumeDisneyLeft(ui.SoundVolumeDisneyLeftEdit->value());
        Game->SetMixerVolumeDisneyRight(ui.SoundVolumeDisneyRightEdit->value());
        Game->SetMixerVolumeSpeakerLeft(ui.SoundVolumeSpeakerLeftEdit->value());
        Game->SetMixerVolumeSpeakerRight(ui.SoundVolumeSpeakerRightEdit->value());
        Game->SetMixerVolumeGUSLeft(ui.SoundVolumeGUSLeftEdit->value());
        Game->SetMixerVolumeGUSRight(ui.SoundVolumeGUSRightEdit->value());
        Game->SetMixerVolumeSBLeft(ui.SoundVolumeSBLeftEdit->value());
        Game->SetMixerVolumeSBRight(ui.SoundVolumeSBRightEdit->value());
        Game->SetMixerVolumeFMLeft(ui.SoundVolumeFMLeftEdit->value());
        Game->SetMixerVolumeFMRight(ui.SoundVolumeFMRightEdit->value());

        // Autoexec Sheet

        Game->SetAutoexecOverridegamestart(ui.AutoexecOverrideGameStartCheckBox->isChecked());
        Game->SetAutoexecOverrideMount(ui.AutoexecOverrideMountingCheckBox->isChecked());
        Game->SetUse4DOS(ui.AutoexecUse4DOSCheckBox->isChecked());
        {
            TStringList L = MemoLines(ui.AutoexecMemo);
            Game->SetAutoexec(StringListToString(&L));
            L = MemoLines(ui.FinalizationMemo);
            Game->SetAutoexecFinalization(StringListToString(&L));
        }
        if (ui.AutoexecBootNormal->isChecked()) Game->SetAutoexecBootImage("");
        if (ui.AutoexecBootHDImage->isChecked()) {
            if (ui.AutoexecBootHDImageComboBox->currentIndex() == 0) Game->SetAutoexecBootImage("2"); else Game->SetAutoexecBootImage("3");
        }
        if (ui.AutoexecBootFloppyImage->isChecked()) {
            S = GridCell(ui.AutoexecBootFloppyImageTab, 0);
            for (I = 1; I <= ui.AutoexecBootFloppyImageTab->rowCount() - 1; I++) S = S + "$" + GridCell(ui.AutoexecBootFloppyImageTab, I);
            Game->SetAutoexecBootImage(S);
        }

        // Custom Sets

        {
            TStringList L = MemoLines(ui.CustomSetsMemo);
            Game->SetCustomSettings(StringListToString(&L));
        }
        St = new TStringList();
        for (I = 0; I <= ui.CustomSetsValueListEditor->rowCount() - 1; I++) St->Add(Replace(VLEString(ui.CustomSetsValueListEditor, I), "=", "[" + IntToStr('=') + "]"));
        Game->SetEnvironment(StringListToString(St));
        delete St;

        // Sheet Nr

        switch (ui.PageControl->currentIndex()) {
            case 0: case 1: case 2: case 3: case 4: case 5: Game->SetLastOpenTab(ui.PageControl->currentIndex()); break;
            case 6: Game->SetLastOpenTab(7); break;
            case 7: Game->SetLastOpenTab(6); break;
        }
        Game->SetLastOpenTabModern(-1);

        Game->StoreAllValues();
        Game->LoadCache();

        if ((OldFileName != "") && (VFI(ui.ProfileSettingsValueListEditor, 1) != ChangeFileExt(ExtractFileName(OldFileName), ""))) {
            S = VFI(ui.ProfileSettingsValueListEditor, 1);
            if (ExtUpperCase(Copy(S, Length(S) - 4, 5)) != ".PROF") S = S + ".prof";
            NewFileName = IncludeTrailingPathDelimiter(ExtractFilePath(OldFileName)) + S;
            Game->RenameINI(NewFileName);
        }

        if (Trim(Game->CaptureFolder()) != "") ForceDirectories(MakeAbsPath(Game->CaptureFolder(), PrgSetup->BaseDir()));
    } catch (const Exception &Ex) {
        MessageDlg(Ex.Message, mtError, mbOK, 0);
    }
}

void TProfileEditorForm::AutoexecBootHDImageComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.AutoexecBootHDImage->setChecked(true);
}

void TProfileEditorForm::ButtonWork(QObject* Sender)
{
    QString S, T, U;
    int I;
    TStringList* St;
    TValueListEditorWidget *E = ui.ProfileSettingsValueListEditor;

    switch (Sender->property("tag").toInt()) {
        // Icon
        case 0:
            S = "";
            if (VFI(E, 3) != RemoveUnderline(LanguageSetup->Yes())) S = MakeAbsPath(VFI(E, 2), PrgSetup->BaseDir());
            if ((S != "") && (VFI(E, 6) != RemoveUnderline(LanguageSetup->Yes()))) S = MakeAbsPath(VFI(E, 4), PrgSetup->BaseDir());
            if (ShowIconManager(this, IconName, ExtractFilePath(S))) LoadIcon();
            break;
        case 1: IconName = ""; LoadIcon(); break;
        // ExtraFiles
        case 27:
            if (Trim(PrgSetup->GameDir()) == "") S = PrgSetup->BaseDir(); else S = PrgSetup->GameDir();
            OpenDialog->DefaultExt = "";
            OpenDialog->Title = LanguageSetup->ProfileEditorExtraFilesCaption();
            OpenDialog->Filter = LanguageSetup->ProfileEditorExtraFilesFilter();
            OpenDialog->InitialDir = S;
            if (!OpenDialog->Execute()) return;
            S = OpenDialog->FileName;
            ui.ExtraFilesListBox->addItem(MakeRelPath(S, PrgSetup->BaseDir(), true));
            ui.ExtraFilesListBox->setCurrentRow(ui.ExtraFilesListBox->count() - 1);
            ExtraFilesListBoxClick(Sender);
            break;
        case 28:
            if (ui.ExtraFilesListBox->currentRow() < 0) return;
            S = MakeAbsPath(ui.ExtraFilesListBox->item(ui.ExtraFilesListBox->currentRow())->text(), PrgSetup->BaseDir());
            OpenDialog->DefaultExt = "";
            OpenDialog->Title = LanguageSetup->ProfileEditorExtraFilesCaption();
            OpenDialog->Filter = LanguageSetup->ProfileEditorExtraFilesFilter();
            OpenDialog->InitialDir = ExtractFilePath(S);
            if (!OpenDialog->Execute()) return;
            S = OpenDialog->FileName;
            ui.ExtraFilesListBox->item(ui.ExtraFilesListBox->currentRow())->setText(MakeRelPath(S, PrgSetup->BaseDir()));
            break;
        case 29:
            I = ui.ExtraFilesListBox->currentRow();
            if (I < 0) return;
            delete ui.ExtraFilesListBox->takeItem(I);
            if (ui.ExtraFilesListBox->count() > 0) ui.ExtraFilesListBox->setCurrentRow(Max(0, I - 1));
            ExtraFilesListBoxClick(Sender);
            break;
        // ExtraDirs
        case 2:
            if (Trim(PrgSetup->GameDir()) == "") S = PrgSetup->BaseDir(); else S = PrgSetup->GameDir();
            if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
            ui.ExtraDirsListBox->addItem(MakeRelPath(S, PrgSetup->BaseDir(), true));
            ui.ExtraDirsListBox->setCurrentRow(ui.ExtraDirsListBox->count() - 1);
            ExtraDirsListBoxClick(Sender);
            break;
        case 3:
            if (ui.ExtraDirsListBox->currentRow() >= 0) {
                S = MakeAbsPath(ui.ExtraDirsListBox->item(ui.ExtraDirsListBox->currentRow())->text(), PrgSetup->BaseDir());
                if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
                ui.ExtraDirsListBox->item(ui.ExtraDirsListBox->currentRow())->setText(MakeRelPath(S, PrgSetup->BaseDir(), true));
            }
            break;
        case 4:
            I = ui.ExtraDirsListBox->currentRow();
            if (I < 0) return;
            delete ui.ExtraDirsListBox->takeItem(I);
            if (ui.ExtraDirsListBox->count() > 0) ui.ExtraDirsListBox->setCurrentRow(Max(0, I - 1));
            ExtraDirsListBoxClick(Sender);
            break;
        // Mounting
        case 5:
            if (Mounting->Count() < 10) {
                S = "";
                if (VFI(E, 3) == RemoveUnderline(LanguageSetup->Yes()))
                    T = PrgSetup->GameDir();
                else
                    T = VFI(E, 2);
                U = VFI(E, 0);
                if (!ShowProfileMountEditorDialog(this, S, UsedDriveLetters(), IncludeTrailingPathDelimiter(ExtractFilePath(T)), U, GameDB, nullptr, QString(NextFreeDriveLetter()))) return;
                Mounting->Add(S);
                LoadMountingList();
                ui.MountingListView->SetItemIndex(ui.MountingListView->Items()->Count() - 1);
            }
            break;
        case 6:
            I = ui.MountingListView->ItemIndex();
            if (I < 0) return;
            S = (*Mounting)[I];
            if (VFI(E, 3) == RemoveUnderline(LanguageSetup->Yes()))
                T = PrgSetup->GameDir();
            else
                T = VFI(E, 2);
            U = VFI(E, 0);
            if (!ShowProfileMountEditorDialog(this, S, UsedDriveLetters(I), IncludeTrailingPathDelimiter(ExtractFilePath(T)), U, GameDB)) return;
            (*Mounting)[I] = S;
            LoadMountingList();
            ui.MountingListView->SetItemIndex(I);
            break;
        case 7:
            I = ui.MountingListView->ItemIndex();
            if (I < 0) return;
            Mounting->Delete(I);
            LoadMountingList();
            if (Mounting->Count() > 0) ui.MountingListView->SetItemIndex(Max(0, I - 1));
            break;
        case 8:
            if ((Mounting->Count() > 0) && (MessageDlg(LanguageSetup->ProfileEditorMountingDeleteAllMessage(), mtConfirmation, mbYes | mbNo, 0) == mrYes)) {
                Mounting->Clear();
                LoadMountingList();
            }
            break;
        case 20:
            I = 0;
            while (I < Mounting->Count()) {
                St = ValueToList((*Mounting)[I]);
                if ((St->Count() >= 3) && ((*St)[2] == "C")) { Mounting->Delete(I); delete St; continue; }
                I++;
                delete St;
            }
            if (Mounting->Count() < 10)
                Mounting->Insert(0, MakeRelPath(PrgSetup->GameDir(), PrgSetup->BaseDir(), true) + ";Drive;C;false;");
            LoadMountingList();
            break;
        // GenerateScreenshotFolderName
        case 9:
            if ((Trim(VFI(E, 10)) != "") && DirectoryExists(MakeAbsPath(VFI(E, 10), PrgSetup->BaseDir()))) return;
            S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(VFI(E, 0)) + PathDelim;
            I = 0;
            while ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(MakeAbsPath(S, PrgSetup->BaseDir()))) {
                I++;
                S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(VFI(E, 0)) + IntToStr(I) + PathDelim;
            }
            // Note: index 0 (profile name) as in the original code
            SetVFI(E, 0, S);
            break;
        // Autoexec
        case 10: ui.AutoexecMemo->clear(); break;
        case 11:
            OpenDialog->DefaultExt = "txt";
            OpenDialog->Filter = LanguageSetup->ProfileEditorAutoexecFilter();
            OpenDialog->Title = LanguageSetup->ProfileEditorAutoexecLoadTitle();
            OpenDialog->InitialDir = PrgDataDir();
            if (!OpenDialog->Execute()) return;
            {
                TStringList L;
                if (L.LoadFromFile(OpenDialog->FileName)) SetMemoLines(ui.AutoexecMemo, L);
                else MessageDlg(Format(LanguageSetup->MessageCouldNotOpenFile(), {OpenDialog->FileName}), mtError, mbOK, 0);
            }
            break;
        case 12:
            SaveDialog->DefaultExt = "txt";
            SaveDialog->Filter = LanguageSetup->ProfileEditorAutoexecFilter();
            SaveDialog->Title = LanguageSetup->ProfileEditorAutoexecSaveTitle();
            SaveDialog->InitialDir = PrgDataDir();
            if (!SaveDialog->Execute()) return;
            if (!MemoLines(ui.AutoexecMemo).SaveToFile(SaveDialog->FileName))
                MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {SaveDialog->FileName}), mtError, mbOK, 0);
            break;
        case 13:
            if (ui.AutoexecBootFloppyImageTab->currentRow() < 0) {
                MessageDlg(LanguageSetup->MessageNoImageSelected(), mtError, mbOK, 0);
                return;
            }
            S = MakeAbsPath(GridCell(ui.AutoexecBootFloppyImageTab, ui.AutoexecBootFloppyImageTab->currentRow()), PrgSetup->BaseDir());
            OpenDialog->DefaultExt = "img";
            OpenDialog->InitialDir = ExtractFilePath(S);
            OpenDialog->Title = LanguageSetup->ProfileMountingFile();
            OpenDialog->Filter = LanguageSetup->ProfileMountingFileFilter();
            if (!OpenDialog->Execute()) return;
            SetGridCell(ui.AutoexecBootFloppyImageTab, ui.AutoexecBootFloppyImageTab->currentRow(), MakeRelPath(OpenDialog->FileName, PrgSetup->BaseDir()));
            ui.AutoexecBootFloppyImage->setChecked(true);
            break;
        case 21:
            ui.AutoexecBootFloppyImageTab->setRowCount(ui.AutoexecBootFloppyImageTab->rowCount() + 1);
            ui.AutoexecBootFloppyImageTab->setCurrentCell(ui.AutoexecBootFloppyImageTab->rowCount() - 1, 0);
            break;
        case 22:
            if (ui.AutoexecBootFloppyImageTab->currentRow() < 0) {
                MessageDlg(LanguageSetup->MessageNoImageSelected(), mtError, mbOK, 0);
                return;
            }
            if (ui.AutoexecBootFloppyImageTab->rowCount() == 1) {
                SetGridCell(ui.AutoexecBootFloppyImageTab, 0, "");
            } else {
                for (I = ui.AutoexecBootFloppyImageTab->currentRow() + 1; I <= ui.AutoexecBootFloppyImageTab->rowCount() - 1; I++) SetGridCell(ui.AutoexecBootFloppyImageTab, I - 1, GridCell(ui.AutoexecBootFloppyImageTab, I));
                ui.AutoexecBootFloppyImageTab->setRowCount(ui.AutoexecBootFloppyImageTab->rowCount() - 1);
            }
            break;
        case 24: ui.FinalizationMemo->clear(); break;
        case 25:
            OpenDialog->DefaultExt = "txt";
            OpenDialog->Filter = LanguageSetup->ProfileEditorAutoexecFilter();
            OpenDialog->Title = LanguageSetup->ProfileEditorFinalizationLoadTitle();
            OpenDialog->InitialDir = PrgDataDir();
            if (!OpenDialog->Execute()) return;
            {
                TStringList L;
                if (L.LoadFromFile(OpenDialog->FileName)) SetMemoLines(ui.FinalizationMemo, L);
                else MessageDlg(Format(LanguageSetup->MessageCouldNotOpenFile(), {OpenDialog->FileName}), mtError, mbOK, 0);
            }
            break;
        case 26:
            SaveDialog->DefaultExt = "txt";
            SaveDialog->Filter = LanguageSetup->ProfileEditorAutoexecFilter();
            SaveDialog->Title = LanguageSetup->ProfileEditorFinalizationSaveTitle();
            SaveDialog->InitialDir = PrgDataDir();
            if (!SaveDialog->Execute()) return;
            if (!MemoLines(ui.FinalizationMemo).SaveToFile(SaveDialog->FileName))
                MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {SaveDialog->FileName}), mtError, mbOK, 0);
            break;
        // CustomSets
        case 14: ui.CustomSetsMemo->clear(); break;
        case 15:
            ForceDirectories(PrgDataDir() + CustomConfigsSubDir);

            OpenDialog->DefaultExt = "txt";
            OpenDialog->Filter = LanguageSetup->ProfileEditorCustomSetsFilter();
            OpenDialog->Title = LanguageSetup->ProfileEditorCustomSetsLoadTitle();
            OpenDialog->InitialDir = PrgDataDir() + CustomConfigsSubDir;
            if (!OpenDialog->Execute()) return;
            {
                TStringList L;
                if (L.LoadFromFile(OpenDialog->FileName)) SetMemoLines(ui.CustomSetsMemo, L);
                else MessageDlg(Format(LanguageSetup->MessageCouldNotOpenFile(), {OpenDialog->FileName}), mtError, mbOK, 0);
            }
            break;
        case 16:
            ForceDirectories(PrgDataDir() + CustomConfigsSubDir);

            SaveDialog->DefaultExt = "txt";
            SaveDialog->Filter = LanguageSetup->ProfileEditorCustomSetsFilter();
            SaveDialog->Title = LanguageSetup->ProfileEditorCustomSetsSaveTitle();
            SaveDialog->InitialDir = PrgDataDir() + CustomConfigsSubDir;
            if (!SaveDialog->Execute()) return;
            if (!MemoLines(ui.CustomSetsMemo).SaveToFile(SaveDialog->FileName))
                MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {SaveDialog->FileName}), mtError, mbOK, 0);
            break;
        case 17: TVLE(ui.CustomSetsValueListEditor).Add("Key="); break;
        case 18:
            // Row>0 and RowCount>2 (VCL rows include the title row)
            if ((ui.CustomSetsValueListEditor->currentRow() >= 0) && (ui.CustomSetsValueListEditor->rowCount() > 1)) ui.CustomSetsValueListEditor->deleteRow(ui.CustomSetsValueListEditor->currentRow());
            break;
        // GenerateGameDataFolderName
        case 19:
            if (PrgSetup->DataDir() == "") S = PrgSetup->BaseDir(); else S = PrgSetup->DataDir();
            S = IncludeTrailingPathDelimiter(S) + MakeFileSysOKFolderName(VFI(E, 0)) + PathDelim;
            T = MakeRelPath(S, PrgSetup->BaseDir(), true);
            if (T != "") SetVFI(ui.GameInfoValueListEditor, 6, T);
            if (DirectoryExists(S)) return;
            if (MessageDlg(Format(LanguageSetup->MessageConfirmationCreateDir(), {S}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
            ForceDirectories(S);
            break;
        // Userdefined meta information
        case 23: ShowUserInfoDialog(this, Game, GameDB); break;
    }
}

QChar TProfileEditorForm::NextFreeDriveLetter()
{
    int I;
    bool B;
    TStringList* St;
    QChar result = 'C';

    do {
        B = true;
        for (I = 0; I <= Mounting->Count() - 1; I++) {
            St = ValueToList((*Mounting)[I]);
            if (St->Count() < 3) { delete St; continue; }
            if (UpperCase((*St)[2]) == QString(result)) { result = QChar(result.unicode() + 1); B = false; delete St; break; }
            delete St;
        }
    } while (!B);
    return result;
}

QString TProfileEditorForm::UsedDriveLetters(int AllowedNr)
{
    int I;
    TStringList* St;
    QString result = "";

    for (I = 0; I <= Mounting->Count() - 1; I++) if (I != AllowedNr) {
        St = ValueToList((*Mounting)[I]);
        if (St->Count() >= 3) result = result + UpperCase((*St)[2]);
        delete St;
    }
    return result;
}

void TProfileEditorForm::ProfileSettingsValueListEditorEditButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    TValueListEditorWidget *E = ui.ProfileSettingsValueListEditor;

    // Note: the value indices of the rows 6 and 11 are used as in the original code
    switch (E->currentRow() + 1) {
        case 3:
            if (VFI(E, 3) == RemoveUnderline(LanguageSetup->Yes())) return;
            S = VFI(E, 2);
            if (SelectProgramFile(S, VFI(E, 2), VFI(E, 4), false, -1, this)) SetVFI(E, 2, S);
            break;
        case 6:
            if (VFI(E, 6) == RemoveUnderline(LanguageSetup->Yes())) return;
            S = VFI(E, 4);
            if (SelectProgramFile(S, VFI(E, 4), VFI(E, 2), false, -1, this)) SetVFI(E, 4, S);
            break;
        case 11:
            S = PrgSetup->BaseDir();
            if (Trim(VFI(E, 8)) == "")
                S = MakeAbsPath(PrgSetup->CaptureDir(), PrgSetup->BaseDir());
            else
                S = MakeAbsPath(IncludeTrailingPathDelimiter(VFI(E, 8)), S);
            if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
            S = MakeRelPath(S, PrgSetup->BaseDir(), true);
            if (S == "") return;
            SetVFI(E, 8, IncludeTrailingPathDelimiter(S));
            break;
    }
}

void TProfileEditorForm::ProfileSettingsValueListEditorKeyUp(QObject* Sender, int& Key, TShiftState Shift)
{
    Q_UNUSED(Sender); Q_UNUSED(Key); Q_UNUSED(Shift);
    QString S;

    if (ui.ProfileSettingsValueListEditor->rowCount() < 2) S = ""; else S = VFI(ui.ProfileSettingsValueListEditor, 0);
    if (Trim(S) == "") S = LanguageSetup->NotSet();
    if ((S == LanguageSetup->ProfileEditorNoFilename()) || (S == LanguageSetup->NotSet()))
        setWindowTitle(LanguageSetup->ProfileEditor());
    else
        setWindowTitle(LanguageSetup->ProfileEditor() + " [" + S + "]");
}

void TProfileEditorForm::ProfileSettingsValueListEditorSetEditText(QObject* Sender, int ACol, int ARow, const QString& Value)
{
    Q_UNUSED(Sender); Q_UNUSED(ACol); Q_UNUSED(ARow); Q_UNUSED(Value);
    QString S;

    if (ui.ProfileSettingsValueListEditor->rowCount() < 2) S = ""; else S = VFI(ui.ProfileSettingsValueListEditor, 0);
    if (Trim(S) == "") S = LanguageSetup->NotSet();
    if ((S == LanguageSetup->ProfileEditorNoFilename()) || (S == LanguageSetup->NotSet()))
        setWindowTitle(LanguageSetup->ProfileEditor());
    else
        setWindowTitle(LanguageSetup->ProfileEditor() + " [" + S + "]");
}

void TProfileEditorForm::SearchClick(QObject* Sender)
{
    switch (Sender->property("tag").toInt()) {
        case 0:
            if (auto *A = qobject_cast<QAction *>(Sender)) LinkFile->MoveToTop(A->text());
            LoadLinks();
            break;
        case 1: if (LinkFile->EditFile(false)) LoadLinks(); break;
        case 2: OpenLink(LinkFile->Link(0), "<GAMENAME>", VFI(ui.ProfileSettingsValueListEditor, 0)); break;
    }
}

void TProfileEditorForm::SetExeFileName(const QString& NewExeFileName)
{
    if (Trim(NewExeFileName) == "") return;
    // Note: called before the form is shown; the row does not exist yet in this case (the original
    // code raises a list index error then), SetVFI ignores it.
    SetVFI(ui.ProfileSettingsValueListEditor, 2, NewExeFileName);
}

void TProfileEditorForm::GameInfoValueListEditorEditButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    TValueListEditorWidget *E = ui.GameInfoValueListEditor;

    switch (E->currentRow() + 1) {
        case 7:
            if (PrgSetup->DataDir() == "") S = PrgSetup->BaseDir(); else S = PrgSetup->DataDir();
            if (Trim(VFI(E, 6)) != "")
                S = MakeAbsPath(VFI(E, 6), S);
            if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
            S = MakeRelPath(S, PrgSetup->BaseDir(), true);
            if (S == "") return;
            SetVFI(E, 6, S);
            break;
    }
}

void TProfileEditorForm::GeneralValueListEditorEditButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    int ShiftNr;
    TValueListEditorWidget *E = ui.GeneralValueListEditor;

    if (PrgSetup->AllowTextModeLineChange()) ShiftNr = 1; else ShiftNr = 0;

    if (E->currentRow() == 15 + ShiftNr) {
        S = Trim(VFI(E, 15 + ShiftNr));
        if ((S == "") || (ExtUpperCase(S) == "DEFAULT")) S = PrgSetup->DOSBoxSettings(0)->DosBoxDir();
        S = MakeAbsPath(S, PrgSetup->BaseDir());
        if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
        S = MakeRelPath(S, PrgSetup->BaseDir(), true);
        if (S == "") return;
        SetVFI(E, 15 + ShiftNr, IncludeTrailingPathDelimiter(S));
    }

    if (E->currentRow() == 16 + ShiftNr) {
        S = Trim(VFI(E, 16 + ShiftNr));
        if ((S == "") || (ExtUpperCase(S) == "DEFAULT")) S = PrgSetup->DOSBoxSettings(0)->DosBoxMapperFile();
        DosBoxTxtOpenDialog->Title = LanguageSetup->SetupFormDOSBoxMapperFileTitle();
        DosBoxTxtOpenDialog->Filter = LanguageSetup->SetupFormDOSBoxMapperFileFilter();
        if (Trim(S) == "")
            DosBoxTxtOpenDialog->InitialDir = PrgDataDir();
        else
            DosBoxTxtOpenDialog->InitialDir = ExtractFilePath(MakeAbsPath(S, PrgSetup->BaseDir()));
        if (!DosBoxTxtOpenDialog->Execute()) return;
        S = MakeRelPath(DosBoxTxtOpenDialog->FileName, PrgSetup->BaseDir());
        if (S == "") return;
        SetVFI(E, 16 + ShiftNr, S);
    }
}

void TProfileEditorForm::EnvironmentValueListEditorEditButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    TValueListEditorWidget *E = ui.EnvironmentValueListEditor;

    S = VFI(E, E->currentRow());
    if (!ShowSerialEditDialog(this, S)) return;
    if (S != "") SetVFI(E, E->currentRow(), S);
}

void TProfileEditorForm::ExtraDirsListBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.ExtraDirsEditButton->setEnabled(ui.ExtraDirsListBox->currentRow() >= 0);
    ui.ExtraDirsDelButton->setEnabled(ui.ExtraDirsListBox->currentRow() >= 0);
}

void TProfileEditorForm::ExtraDirsListBoxDblClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ButtonWork(ui.ExtraDirsEditButton);
}

void TProfileEditorForm::ExtraDirsListBoxKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if (Shift != Qt::NoModifier) return;
    switch (Key) {
        case Qt::Key_Insert: ButtonWork(ui.ExtraDirsAddButton); break;
        case Qt::Key_Return: case Qt::Key_Enter: ButtonWork(ui.ExtraDirsEditButton); break;
        case Qt::Key_Delete: ButtonWork(ui.ExtraDirsDelButton); break;
    }
}

void TProfileEditorForm::ExtraFilesListBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.ExtraFilesEditButton->setEnabled(ui.ExtraFilesListBox->currentRow() >= 0);
    ui.ExtraFilesDelButton->setEnabled(ui.ExtraFilesListBox->currentRow() >= 0);
}

void TProfileEditorForm::ExtraFilesListBoxDblClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ButtonWork(ui.ExtraFilesEditButton);
}

void TProfileEditorForm::ExtraFilesListBoxKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if (Shift != Qt::NoModifier) return;
    switch (Key) {
        case Qt::Key_Insert: ButtonWork(ui.ExtraFilesAddButton); break;
        case Qt::Key_Return: case Qt::Key_Enter: ButtonWork(ui.ExtraFilesEditButton); break;
        case Qt::Key_Delete: ButtonWork(ui.ExtraFilesDelButton); break;
    }
}

void TProfileEditorForm::MountingListViewDblClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ButtonWork(ui.MountingEditButton);
}

void TProfileEditorForm::MountingListViewKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if ((Shift == Qt::ControlModifier) && ((Key == Qt::Key_Return) || (Key == Qt::Key_Enter))) { ButtonWork(ui.MountingEditButton); return; }
    if (Shift != Qt::NoModifier) return;
    switch (Key) {
        case Qt::Key_Insert: ButtonWork(ui.MountingAddButton); break;
        case Qt::Key_Return: case Qt::Key_Enter: ButtonWork(ui.MountingEditButton); break;
        case Qt::Key_Delete: ButtonWork(ui.MountingDelButton); break;
    }
}

void TProfileEditorForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ProfileWithWizard);
}

void TProfileEditorForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

// global

namespace ProfileEditorFormUnit {

static int EditGameProfilInt(QWidget* AOwner, TGameDB* AGameDB, TGame*& AGame, TGame* ADefaultGame, TLinkFile* ASearchLinkFile, TStringList* ADeleteOnExit, bool PrevButton, bool NextButton, bool RestorePos, bool EditingTemplate, const QString& ANewProfileName, const QString& ANewExeFile, bool HideGameInfoPage)
{
    double D;
    QString S;
    int I;
    int result;

    if (ScummVMMode(AGame) || ScummVMMode(ADefaultGame) || WindowsExeMode(AGame) || WindowsExeMode(ADefaultGame)) {
        return ::EditGameProfilInt(AOwner, AGameDB, AGame, ADefaultGame, ASearchLinkFile, ADeleteOnExit, PrevButton, NextButton, RestorePos, EditingTemplate, ANewProfileName, ANewExeFile, HideGameInfoPage);
    }

    if ((AGame != nullptr) && (Trim(ExtUpperCase(AGame->VideoCard())) == "VGA")) {
        S = CheckDOSBoxVersion(GetDOSBoxNr(AGame)); for (I = 0; I < S.length(); I++) if ((S[I] == ',') || (S[I] == '.')) S[I] = '.';   // DecimalSeparator
        if (!TryStrToFloat(S, D)) D = 0.72;
        if (D > 0.72) { AGame->SetVideoCard("svga_s3"); AGame->StoreAllValues(); }
    }

    ProfileEditorForm = new TProfileEditorForm(AOwner);

    ProfileEditorForm->RestoreLastPosition = RestorePos;
    if (RestorePos && ((LastTop > 0) || (LastLeft > 0))) ProfileEditorForm->setProperty("vclPosition", "poDesigned");
    ProfileEditorForm->GameDB = AGameDB;
    ProfileEditorForm->Game = AGame;
    ProfileEditorForm->LoadTemplate = ADefaultGame;
    ProfileEditorForm->ui.PreviousButton->setVisible(PrevButton);
    ProfileEditorForm->ui.NextButton->setVisible(NextButton);
    ProfileEditorForm->EditingTemplate = EditingTemplate;
    ProfileEditorForm->LinkFile = ASearchLinkFile;
    if (ANewExeFile != "") ProfileEditorForm->SetExeFileName(ANewExeFile);
    ProfileEditorForm->HideGameInfoPage = HideGameInfoPage;
    if (ProfileEditorForm->ShowModal() == mrOk) {
        result = ProfileEditorForm->MoveStatus;
        AGame = ProfileEditorForm->Game;
        LastPage = ProfileEditorForm->ui.PageControl->currentIndex();
        LastTop = ProfileEditorForm->y();
        LastLeft = ProfileEditorForm->x();
    } else {
        result = -2;
    }
    delete ProfileEditorForm;
    ProfileEditorForm = nullptr;
    return result;
}

}  // namespace ProfileEditorFormUnit

bool EditGameProfil(QWidget* AOwner, TGameDB* AGameDB, TGame*& AGame, TGame* ADefaultGame, TLinkFile* ASearchLinkFile, TStringList* ADeleteOnExit, const QString& ANewProfileName, const QString& ANewExeFile, TList* GameList)
{
    int I, J;
    bool PrevButton, NextButton, RestorePos;
    QString S, T;

    I = 0; RestorePos = false;
    do {
        if (GameList == nullptr) {
            NextButton = false;
            PrevButton = false;
        } else {
            if (I == 1) {
                J = GameList->indexOf(AGame);
                if ((J >= 0) && (J < GameList->size() - 1)) AGame = static_cast<TGame *>((*GameList)[J + 1]);
            }
            if (I == -1) {
                J = GameList->indexOf(AGame);
                if (J > 0) AGame = static_cast<TGame *>((*GameList)[J - 1]);
            }
            J = GameList->indexOf(AGame);
            NextButton = (J >= 0) && (J < GameList->size() - 1);
            PrevButton = (J > 0);
        }
        if ((GameList == nullptr) || (GameList->size() == 0)) { S = ANewProfileName; T = ANewExeFile; } else { S = ""; T = ""; }
        I = ProfileEditorFormUnit::EditGameProfilInt(AOwner, AGameDB, AGame, ADefaultGame, ASearchLinkFile, ADeleteOnExit, PrevButton, NextButton, RestorePos, false, S, T, false);
        RestorePos = true;
    } while (!((I == 0) || (I == -2)));
    return (I != -2);
}

bool EditGameTemplate(QWidget* AOwner, TGameDB* AGameDB, TGame*& AGame, TGame* ADefaultGame, TLinkFile* ASearchLinkFile, TStringList* ADeleteOnExit, TList* GameList, TList* TemplateType)
{
    int I, J;
    bool PrevButton, NextButton, RestorePos;
    int TemplateMode;

    I = 0; RestorePos = false;
    do {
        if (GameList == nullptr) {
            NextButton = false;
            PrevButton = false;
        } else {
            if (I == 1) {
                J = GameList->indexOf(AGame);
                if ((J >= 0) && (J < GameList->size() - 1)) AGame = static_cast<TGame *>((*GameList)[J + 1]);
            }
            if (I == -1) {
                J = GameList->indexOf(AGame);
                if (J > 0) AGame = static_cast<TGame *>((*GameList)[J - 1]);
            }
            J = GameList->indexOf(AGame);
            NextButton = (J >= 0) && (J < GameList->size() - 1);
            PrevButton = (J > 0);
        }

        if (TemplateType != nullptr) {
            TemplateMode = static_cast<int>(reinterpret_cast<intptr_t>((*TemplateType)[GameList->indexOf(AGame)]));
        } else {
            TemplateMode = 0;
        }
        // TemplateMode: 0=normal template, 1=default template, 2=ScummVM default template

        if (TemplateMode == 2) AGame->SetProfileMode("ScummVM");
        try {
            I = ProfileEditorFormUnit::EditGameProfilInt(AOwner, AGameDB, AGame, ADefaultGame, ASearchLinkFile, ADeleteOnExit, PrevButton, NextButton, RestorePos, true, "", "", TemplateMode != 0);
        } catch (...) {
            if (TemplateMode == 2) AGame->SetProfileMode("DOSBox");
            throw;
        }
        if (TemplateMode == 2) AGame->SetProfileMode("DOSBox");
        RestorePos = true;
    } while (!((I == 0) || (I == -2)));
    return (I != -2);
}
