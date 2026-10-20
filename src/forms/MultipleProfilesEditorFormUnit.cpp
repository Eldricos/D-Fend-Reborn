// C++ port of MultipleProfilesEditorFormUnit.pas (implementation section).
#include "MultipleProfilesEditorFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include "GameDBToolsUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "PrgSetupUnit.h"
#include "StagingResourcesUnit.h"

TMultipleProfilesEditorForm* MultipleProfilesEditorForm = nullptr;

// TCheckListBox helpers
static bool CheckListChecked(QListWidget* L, int I) { return L->item(I)->checkState() == Qt::Checked; }
static void CheckListSetChecked(QListWidget* L, int I, bool B) { L->item(I)->setCheckState(B ? Qt::Checked : Qt::Unchecked); }
// ListBox.Items.Objects[I] (BuildCheckList stores the TGame pointer as Qt::UserRole data)
static TGame* CheckListGame(QListWidget* L, int I) { return static_cast<TGame*>(L->item(I)->data(Qt::UserRole).value<void*>()); }

/* Tools */

// Minimum widths of the value controls (as in the original; wider if the texts need it)
static const int ValueWidth = 105;
static const int EditWidth = 200;
static const int ReplaceFolderIndent = 20;

static TStringList* GetAllUserInfoKeys(TGameDB* GameDB)
{
    int I, J, K;
    TStringList *St, Upper;
    QString S;

    TStringList* result = new TStringList;
    for (I = 0; I < GameDB->Count(); I++) {
        St = StringToStringList(GameDB->Game(I)->UserInfo());
        for (J = 0; J < St->Count(); J++) {
            S = Trim((*St)[J]);
            K = Pos('=', S); if (K == 0) continue;
            S = Trim(Copy(S, 1, K - 1));
            if (S == "") continue;
            if (Upper.IndexOf(ExtUpperCase(S)) < 0) {
                result->Add(S);
                Upper.Add(ExtUpperCase(S));
            }
        }
        delete St;
    }
    return result;
}

static void SetUserInfo(TGame* G, const QString& Key, const QString& Value)
{
    TStringList* St;
    bool ValueFound;
    int I, J;
    QString KeyUpper, S;

    KeyUpper = Trim(ExtUpperCase(Key));
    St = StringToStringList(G->UserInfo());
    ValueFound = false;
    for (I = 0; I < St->Count(); I++) {
        S = Trim((*St)[I]);
        J = Pos('=', S); if (J == 0) continue;
        S = Trim(Copy(S, 1, J - 1));
        if (ExtUpperCase(S) == KeyUpper) { (*St)[I] = Key + "=" + Value; ValueFound = true; break; }
    }
    if (!ValueFound) St->Add(Key + "=" + Value);
    G->SetUserInfo(StringListToString(St));
    delete St;
}

static void DelUserInfo(TGame* G, const QString& Key)
{
    TStringList* St;
    QString KeyUpper, S;
    int I, J;

    KeyUpper = Trim(ExtUpperCase(Key));
    St = StringToStringList(G->UserInfo());
    I = 0;
    while (I < St->Count()) {
        S = Trim((*St)[I]);
        J = Pos('=', S); if (J == 0) { I++; continue; }
        S = Trim(Copy(S, 1, J - 1));
        if (ExtUpperCase(S) == KeyUpper) { St->Delete(I); continue; }
        I++;
    }
    G->SetUserInfo(StringListToString(St));
    delete St;
}

static void ChangeMountSetting(TGame* G, bool SetDefault)
{
    int I;
    QString AllGamesDir, GameDir, S;
    TStringList* St;

    /* Only process if normal start (not booting from image */
    if (Trim(G->AutoexecBootImage()) != "") return;

    /* Exit if mounting not used */
    if (G->AutoexecOverrideMount()) return;

    AllGamesDir = IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir()));
    if ((Trim(G->GameExe()) == "") || (Copy(Trim(ExtUpperCase(G->GameExe())), 1, 7) == "DOSBOX:")) GameDir = ""; else GameDir = IncludeTrailingPathDelimiter(MakeAbsPath(ExtractFilePath(G->GameExe()), PrgSetup->BaseDir()));

    for (I = 0; I <= 9; I++) {
        St = ValueToList(G->Mount(I));
        std::unique_ptr<TStringList> StGuard(St);
        /* RealFolder;DRIVE;Letter;False;;FreeSpace */
        if ((St->Count() < 3) || (Trim(ExtUpperCase((*St)[1])) != "DRIVE") || (Trim(ExtUpperCase((*St)[2])) != "C")) continue;
        S = IncludeTrailingPathDelimiter(MakeAbsPath((*St)[0], PrgSetup->BaseDir()));
        if (SetDefault) {
            if (Trim(ExtUpperCase(GameDir)) == Trim(ExtUpperCase(S))) {
                (*St)[0] = MakeRelPath(PrgSetup->GameDir(), PrgSetup->BaseDir());
                G->SetMount(I, ListToValue(St));
                G->StoreAllValues();
                return;
            }
        } else {
            if ((Trim(ExtUpperCase(AllGamesDir)) == Trim(ExtUpperCase(S))) && (GameDir != "")) {
                (*St)[0] = MakeRelPath(GameDir, PrgSetup->BaseDir());
                G->SetMount(I, ListToValue(St));
                G->StoreAllValues();
                return;
            }
        }
    }
}

static TStringList* GetReplaceFolderList(TGameDB* GameDB)
{
    int I, J;
    TGame* G;
    TStringList St;
    TStringList* St2;

    TStringList* result = new TStringList;
    for (I = 0; I < GameDB->Count(); I++) {
        G = GameDB->Game(I);
        if (ScummVMMode(G) || WindowsExeMode(G)) continue;
        for (J = 0; J < G->NrOfMounts(); J++) {
            /* RealFolder;Type;Letter;IO;Label;FreeSpace; Type=DRIVE */
            St2 = ValueToList(G->Mount(J));
            std::unique_ptr<TStringList> St2Guard(St2);
            if (St2->Count() < 2) continue;
            if (Trim(ExtUpperCase((*St2)[1])) != "DRIVE") continue;
            if (St.IndexOf(ExtUpperCase(IncludeTrailingPathDelimiter((*St2)[0]))) >= 0) continue;
            St.Add(ExtUpperCase(IncludeTrailingPathDelimiter((*St2)[0])));
            result->Add(IncludeTrailingPathDelimiter((*St2)[0]));
        }
    }
    return result;
}

static void ReplaceFolderWork(TGame* G, const QString& FromValue, const QString& ToValue)
{
    int I;
    TStringList* St;
    QString S;

    S = IncludeTrailingPathDelimiter(Trim(ExtUpperCase(FromValue)));

    for (I = 0; I < G->NrOfMounts(); I++) {
        /* RealFolder;Type;Letter;IO;Label;FreeSpace; Type=DRIVE */
        St = ValueToList(G->Mount(I));
        std::unique_ptr<TStringList> StGuard(St);
        if (St->Count() < 2) continue;
        if (Trim(ExtUpperCase((*St)[1])) != "DRIVE") continue;
        if (IncludeTrailingPathDelimiter(Trim(ExtUpperCase((*St)[0]))) == S) {
            (*St)[0] = ToValue;
            G->SetMount(I, ListToValue(St));
        }
    }
}

static const char* const PriorityForeground[4] = {"lower", "normal", "higher", "highest"};
static const char* const PriorityBackground[5] = {"pause", "lower", "normal", "higher", "highest"};

/* TEditMultipleProfilesForm */

TMultipleProfilesEditorForm::TMultipleProfilesEditorForm(QWidget* AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);

    // Settings page: the controls are created in InitSettings. The layout has to exist
    // before VclLayout::setupForm, so the scroll box gives its contents the visible width.
    SettingsLayout = new QGridLayout(ui.ScrollBox_Contents);
    SettingsLayout->setContentsMargins(10, 6, 10, 8);
    SettingsLayout->setHorizontalSpacing(8);
    SettingsLayout->setVerticalSpacing(6);
    SettingsLayout->setColumnStretch(2, 1);
    ui.ScrollBox_Contents->setBackgroundRole(QPalette::Base);
    ui.ScrollBox_Contents->setAutoFillBackground(true);

    VclLayout::setupForm(this);

    ImageList = TImageList::FromResource("MultipleProfilesEditorFormUnit", "ImageList");
    if (ImageList == nullptr) ImageList = new TImageList(16, 16);

    connect(ui.SelectAllButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectAllButton); });
    connect(ui.SelectNoneButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectNoneButton); });
    connect(ui.SelectGenreButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectGenreButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    // OnMouseWheelUp/OnMouseWheelDown: see eventFilter
    ui.ScrollBox->viewport()->installEventFilter(this);

    FormCreate(this);
}

TMultipleProfilesEditorForm::~TMultipleProfilesEditorForm()
{
    delete ImageList;
}

void TMultipleProfilesEditorForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

bool TMultipleProfilesEditorForm::eventFilter(QObject* Watched, QEvent* E)
{
    if (E->type() == QEvent::MouseButtonRelease && Watched->property("SectionCaption").toBool()) {
        CaptionClick(Watched);  // Label.OnClick
        return true;
    }

    if (E->type() == QEvent::Wheel && (Watched == ui.ScrollBox->viewport() || Watched->property("ScrollBoxControl").toBool())) {
        QWheelEvent* WE = static_cast<QWheelEvent*>(E);
        bool Handled = false;
        const QPoint MousePos = WE->globalPosition().toPoint();
        if (WE->angleDelta().y() > 0) FormMouseWheelUp(this, WE->modifiers(), MousePos, Handled);
        else if (WE->angleDelta().y() < 0) FormMouseWheelDown(this, WE->modifiers(), MousePos, Handled);
        return true;
    }

    return TVclForm::eventFilter(Watched, E);
}

void TMultipleProfilesEditorForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt (Unicode fonts)

    ui.PageControl->setCurrentIndex(0);
    TemplateMode = false;
}

void TMultipleProfilesEditorForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (TemplateMode) {
        setWindowTitle(LanguageSetup->ChangeProfilesFormCaption2());
        ui.PageControl->setTabText(ui.PageControl->indexOf(ui.TabSheet1), LanguageSetup->ChangeProfilesFormSelectGamesSheet2());
        ui.InfoLabel->setText(LanguageSetup->ChangeProfilesFormInfo2());
    } else {
        setWindowTitle(LanguageSetup->ChangeProfilesFormCaption());
        ui.PageControl->setTabText(ui.PageControl->indexOf(ui.TabSheet1), LanguageSetup->ChangeProfilesFormSelectGamesSheet());
        ui.InfoLabel->setText(LanguageSetup->ChangeProfilesFormInfo());
    }
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.TabSheet2), LanguageSetup->ChangeProfilesFormEditProfileSheet());

    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.SelectAllButton->setText(LanguageSetup->All());
    ui.SelectNoneButton->setText(LanguageSetup->None());
    ui.SelectGenreButton->setText(LanguageSetup->GameBy());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ImageList, 0);
    UserIconLoader->DialogImage(DI_Expand, ImageList, 1);
    UserIconLoader->DialogImage(DI_Collapse, ImageList, 2);

    BuildCheckList(ui.ListBox, GameDB, false, false);
    BuildSelectPopupMenu(ui.PopupMenu, GameDB, [this](QObject* S) { SelectButtonClick(S); }, false);

    InitSettings();
    SettingsLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding), GridRow, 0);  // keeps the rows at the top

    // The value controls of all sections start at the same position (after the longest setting name)
    int W = 0;
    for (const TSectionData& Sec : Sections) for (const TSettingData& Set : Sec.Settings) W = qMax(W, Set.CheckBox->sizeHint().width());
    SettingsLayout->setColumnMinimumWidth(1, W);

    // Window width: enough for the settings of all sections (long setting names or translations)
    for (TSectionData& Sec : Sections) Sec.Open = true;
    CaptionClick(nullptr);
    SettingsLayout->activate();
    VclLayout::fitPages(ui.TabSheet1, 0, {ui.TabSheet1, ui.TabSheet2});
    for (TSectionData& Sec : Sections) Sec.Open = false;

    CaptionClick(nullptr);
    CheckBoxClick(nullptr);
}

void TMultipleProfilesEditorForm::PrepareScrollBoxControl(QWidget* W)
{
    W->setProperty("ScrollBoxControl", true);
    W->installEventFilter(this);
}

// Grid row of a setting: check box (column 1) and the value controls (column 2)
QWidget* TMultipleProfilesEditorForm::AddValueRow(QCheckBox* CheckBox, QWidget* Control, int ValueWidth)
{
    QWidget* Row = new QWidget(ui.ScrollBox_Contents);
    QHBoxLayout* L = new QHBoxLayout(Row);
    L->setContentsMargins(0, 0, 0, 0);
    L->setSpacing(4);
    if (ValueWidth < 0) {
        Control->setSizePolicy(QSizePolicy::Expanding, Control->sizePolicy().verticalPolicy());
        L->addWidget(Control, 1);
    } else {
        Control->setMinimumWidth(ValueWidth);
        L->addWidget(Control);
        L->addStretch(1);
    }
    SettingsLayout->addWidget(CheckBox, GridRow, 1, Qt::AlignLeft | Qt::AlignVCenter);
    SettingsLayout->addWidget(Row, GridRow, 2);
    GridRow++;
    return Row;
}

void TMultipleProfilesEditorForm::AddCaption(const QString& Name)
{
    QLabel *L, *L2;
    TImageWidget* M;
    int I;
    QWidget* Parent = ui.ScrollBox_Contents;

    I = Sections.size();

    M = new TImageWidget(Parent);
    M->setFixedSize(ImageList->Width(), ImageList->Height());
    M->setProperty("tag", I);
    PrepareScrollBoxControl(M);
    connect(M, &TImageWidget::clicked, this, [this, M]() { CaptionClick(M); });

    L = new QLabel(Parent);
    QFont F = L->font(); F.setPointSize(F.pointSize() + 1); F.setBold(true); L->setFont(F);
    L->setProperty("tag", I);
    L->setProperty("SectionCaption", true);
    L->setCursor(Qt::PointingHandCursor);
    PrepareScrollBoxControl(L);
    L->setText(Name);

    L2 = new QLabel(Parent);
    L2->setVisible(false);
    PrepareScrollBoxControl(L2);

    SettingsLayout->addWidget(M, GridRow, 0, Qt::AlignCenter);
    SettingsLayout->addWidget(L, GridRow, 1, 1, 2);
    GridRow++;
    SettingsLayout->addWidget(L2, GridRow, 1, 1, 2);
    GridRow++;
    M->show(); L->show();

    TSectionData D;
    D.Caption = L; D.Image = M; D.Info = L2; D.Open = false;
    Sections.append(D);
}

// Common part of the Add*Setting routines: the check box in front of each setting
static QCheckBox* CreateSettingCheckBox(QWidget* Parent, QObject* Filter, const QString& Name)
{
    QCheckBox* CheckBox = new QCheckBox(Parent);
    CheckBox->setChecked(false);
    CheckBox->setText(Name);
    CheckBox->setProperty("ScrollBoxControl", true);
    CheckBox->installEventFilter(Filter);
    return CheckBox;
}

void TMultipleProfilesEditorForm::AddSetting(int ID, const QString& Name, TStringList* Values, bool Editable, int ValueWidth, bool FreeValues)
{
    QCheckBox* CheckBox;
    QComboBox* ComboBox;
    QWidget* Parent = ui.ScrollBox_Contents;
    TSectionData& S = Sections.last();

    CheckBox = CreateSettingCheckBox(Parent, this, Name);
    connect(CheckBox, &QCheckBox::clicked, this, [this, CheckBox]() { CheckBoxClick(CheckBox); });

    ComboBox = new QComboBox(Parent);
    ComboBox->setEditable(Editable);  // csDropDown / csDropDownList
    // Full width combo boxes: long items (e.g. web addresses) must not make the window wider
    if (ValueWidth < 0) { ComboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); ComboBox->setMinimumContentsLength(8); }
    if (Values == nullptr) { ComboBox->addItem(RemoveUnderline(LanguageSetup->No())); ComboBox->addItem(RemoveUnderline(LanguageSetup->Yes())); } else ComboBox->addItems(Values->ToQStringList());
    if ((!Editable) && (ComboBox->count() > 0)) ComboBox->setCurrentIndex(0); else { ComboBox->setCurrentIndex(-1); if (Editable) ComboBox->setEditText(""); }
    ComboBox->setEnabled(false);
    // OnDropDown: the items do not change, so the drop down width is set once here
    ComboBoxDropDown(ComboBox);
    connect(ComboBox, &QComboBox::currentTextChanged, this, [this, ComboBox]() { ComboBoxChange(ComboBox); });
    PrepareScrollBoxControl(ComboBox);
    SetComboHint(ComboBox);

    TSettingData D;
    D.CheckBox = CheckBox;
    D.Row = AddValueRow(CheckBox, ComboBox, ValueWidth);

    TControlData C;
    C.Control = ComboBox;
    D.Values.append(C);
    D.ID = ID;
    S.Settings.append(D);

    if (FreeValues && (Values != nullptr)) delete Values;
}

void TMultipleProfilesEditorForm::AddEditSetting(int ID, const QString& Name, const QString& Default, int ValueWidth)
{
    QCheckBox* CheckBox;
    QLineEdit* Edit;
    QWidget* Parent = ui.ScrollBox_Contents;
    TSectionData& S = Sections.last();

    CheckBox = CreateSettingCheckBox(Parent, this, Name);
    connect(CheckBox, &QCheckBox::clicked, this, [this, CheckBox]() { CheckBoxClick(CheckBox); });

    Edit = new QLineEdit(Parent);
    Edit->setText(Default); Edit->setEnabled(false);
    PrepareScrollBoxControl(Edit);

    TSettingData D;
    D.CheckBox = CheckBox;
    D.Row = AddValueRow(CheckBox, Edit, ValueWidth);

    TControlData C;
    C.Control = Edit;
    D.Values.append(C);
    D.ID = ID;
    S.Settings.append(D);
}

void TMultipleProfilesEditorForm::AddYesNoSetting(int ID, const QString& Name)
{
    AddSetting(ID, Name, nullptr, false, ValueWidth);
}

void TMultipleProfilesEditorForm::AddSettingWithEdit(int ID, const QString& Name, TStringList* Values, bool Editable, int ValueWidth, bool FreeValues)
{
    QLineEdit* Edit;

    AddSetting(ID, Name, Values, Editable, ValueWidth, FreeValues);

    TSettingData& S = Sections.last().Settings.last();

    // The edit field fills the rest of the row (instead of the stretch after a fixed width combo box)
    QBoxLayout* L = static_cast<QBoxLayout*>(S.Row->layout());
    if (ValueWidth >= 0) delete L->takeAt(L->count() - 1);

    Edit = new QLineEdit(S.Row);
    Edit->setEnabled(false);
    PrepareScrollBoxControl(Edit);
    L->addWidget(Edit, 1);

    TControlData D;
    D.Control = Edit;
    S.Values.append(D);
}

void TMultipleProfilesEditorForm::AddSpinSetting(int ID, const QString& Name, int MinVal, int MaxVal, int ValueWidth, int DefaultVal)
{
    QCheckBox* CheckBox;
    QSpinBox* SpinEdit;
    QWidget* Parent = ui.ScrollBox_Contents;
    TSectionData& S = Sections.last();

    CheckBox = CreateSettingCheckBox(Parent, this, Name);
    connect(CheckBox, &QCheckBox::clicked, this, [this, CheckBox]() { CheckBoxClick(CheckBox); });

    SpinEdit = new QSpinBox(Parent);
    SpinEdit->setMinimum(MinVal); SpinEdit->setMaximum(MaxVal);
    if (DefaultVal == -1) SpinEdit->setValue(MinVal); else SpinEdit->setValue(DefaultVal);
    SpinEdit->setEnabled(false);
    PrepareScrollBoxControl(SpinEdit);

    TSettingData D;
    D.CheckBox = CheckBox;
    D.Row = AddValueRow(CheckBox, SpinEdit, ValueWidth);

    TControlData C;
    C.Control = SpinEdit;
    D.Values.append(C);
    D.ID = ID;
    S.Settings.append(D);
}

bool TMultipleProfilesEditorForm::DefaultValueOnList(const QString& List, const QString& Value)
{
    TStringList* St;
    int I;
    QString S;

    S = Trim(ExtUpperCase(Value));
    St = ValueToList(List, ";,");
    bool result = false;
    for (I = 0; I < St->Count(); I++) if (Trim(ExtUpperCase((*St)[I])) == S) { result = true; break; }
    delete St;
    return result;
}

void TMultipleProfilesEditorForm::AddReplaceMountingSetting(int ID, const QString& Name, const QString& FromLabel, const QString& ToLabel)
{
    QCheckBox* CheckBox;
    QLabel *Label1, *Label2;
    QComboBox* ComboBox;
    QLineEdit* Edit;
    QToolButton* Button;
    TStringList* List;
    QWidget* Parent = ui.ScrollBox_Contents;

    List = GetReplaceFolderList(GameDB);
    std::unique_ptr<TStringList> ListGuard(List);
    if (List->Count() == 0) return;

    TSectionData& S = Sections.last();

    CheckBox = CreateSettingCheckBox(Parent, this, Name);
    connect(CheckBox, &QCheckBox::clicked, this, [this, CheckBox]() { CheckBoxClick(CheckBox); });

    // Below the check box (indented): "from" folder list and "to" folder with the select button
    QWidget* Row = new QWidget(Parent);
    QGridLayout* L = new QGridLayout(Row);
    L->setContentsMargins(ReplaceFolderIndent, 0, 0, 0);
    L->setHorizontalSpacing(10);
    L->setVerticalSpacing(2);
    L->setColumnStretch(1, 1);

    Label1 = new QLabel(Row); Label1->setText(FromLabel);
    Label2 = new QLabel(Row); Label2->setText(ToLabel);

    ComboBox = new QComboBox(Row);
    ComboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    ComboBox->setMinimumWidth(EditWidth);
    ComboBox->addItems(List->ToQStringList()); ComboBox->setCurrentIndex(0); ComboBox->setEnabled(false);
    ComboBoxDropDown(ComboBox);
    SetComboHint(ComboBox);

    Edit = new QLineEdit(Row);
    Edit->setEnabled(false);

    Button = new QToolButton(Row);
    Button->setProperty("tag", ID);
    connect(Button, &QToolButton::clicked, this, [this, Button]() { SelectFolderButtonClick(Button); });
    Button->setToolTip(LanguageSetup->ChooseFolder());
    Button->setEnabled(false);
    Button->setAutoRaise(true);
    Button->setIcon(ImageList->Icon(0));

    Label1->setBuddy(ComboBox);
    Label2->setBuddy(Edit);

    L->addWidget(Label1, 0, 0);
    L->addWidget(Label2, 0, 1, 1, 2);
    L->addWidget(ComboBox, 1, 0);
    L->addWidget(Edit, 1, 1);
    L->addWidget(Button, 1, 2);

    for (QWidget* W : {static_cast<QWidget*>(Label1), static_cast<QWidget*>(Label2), static_cast<QWidget*>(ComboBox), static_cast<QWidget*>(Edit), static_cast<QWidget*>(Button)}) PrepareScrollBoxControl(W);

    SettingsLayout->addWidget(CheckBox, GridRow, 1, 1, 2, Qt::AlignLeft | Qt::AlignVCenter);
    GridRow++;
    SettingsLayout->addWidget(Row, GridRow, 1, 1, 2);
    GridRow++;

    TSettingData D;
    D.CheckBox = CheckBox;
    D.Row = Row;

    TControlData C;
    C.Control = ComboBox; D.Values.append(C);
    C.Control = Edit; D.Values.append(C);
    C.Control = Button; D.Values.append(C);
    C.Control = Label1; D.Values.append(C);
    C.Control = Label2; D.Values.append(C);
    D.ID = ID;
    S.Settings.append(D);
}

void TMultipleProfilesEditorForm::InitSettings()
{
    TStringList* St;
    int I, J;
    QString S;
    TConfOpt* ConfOpt = GameDB->ConfOpt();

    /* Game info */
    AddCaption(LanguageSetup->ProfileEditorGameInfoSheet());
    AddSetting(1001, LanguageSetup->GameGenre(), ExtGenreList(GetCustomGenreName(GameDB->GetGenreList())), true, -1, true);
    AddSetting(1002, LanguageSetup->GameDeveloper(), GameDB->GetDeveloperList(), true, -1, true);
    AddSetting(1003, LanguageSetup->GamePublisher(), GameDB->GetPublisherList(), true, -1, true);
    AddSetting(1004, LanguageSetup->GameYear(), GameDB->GetYearList(), true, -1, true);
    AddSetting(1005, LanguageSetup->GameLanguage(), ExtLanguageList(GetCustomLanguageName(GameDB->GetLanguageList())), true, -1, true);
    AddSetting(1006, LanguageSetup->GameFavorite(), nullptr, false, ValueWidth);
    St = GetAllUserInfoKeys(GameDB);
    AddSettingWithEdit(1007, LanguageSetup->ChangeProfilesFormSetUserInfo(), St, true, ValueWidth);
    if (St->Count() > 0) AddSetting(1008, LanguageSetup->ChangeProfilesFormDelUserInfo(), St, false, ValueWidth);
    delete St;
    for (J = 1; J <= 9; J++) {
        AddSetting(1010 + (J - 1) * 2, LanguageSetup->GameWWWName() + " (" + IntToStr(J) + ")", GameDB->GetWWWNameList(), true, -1, true);
        AddSetting(1011 + (J - 1) * 2, LanguageSetup->GameWWW() + " (" + IntToStr(J) + ")", GameDB->GetWWWList(), true, -1, true);
    }
    AddYesNoSetting(1100, LanguageSetup->CheckSumTurnOffForProfile());

    /* DOSBox */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet());
    St = new TStringList;
    St->Add(LanguageSetup->GamePriorityLower());
    St->Add(LanguageSetup->GamePriorityNormal());
    St->Add(LanguageSetup->GamePriorityHigher());
    St->Add(LanguageSetup->GamePriorityHighest());
    AddSetting(2001, LanguageSetup->GamePriorityForeground(), St, false, ValueWidth);
    St->Insert(0, LanguageSetup->GamePriorityPause());
    AddSetting(2002, LanguageSetup->GamePriorityBackground(), St, false, ValueWidth);
    delete St;
    AddYesNoSetting(2003, LanguageSetup->GameCloseDOSBoxAfterGameExit());
    St = new TStringList;
    for (I = 0; I < PrgSetup->DOSBoxSettingsCount(); I++) St->Add(PrgSetup->DOSBoxSettings(I)->Name());
    AddSetting(2004, LanguageSetup->GameDOSBoxVersionDefault(), St, false, ValueWidth);
    delete St;
    AddEditSetting(2005, LanguageSetup->GameDOSBoxVersionCustom(), "", -1);

    /* CPU */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->ProfileEditorCPUSheet());
    AddSetting(3001, LanguageSetup->GameCore(), ValueToList(ConfOpt->Core(), ";,"), false, ValueWidth, true);
    AddSetting(3002, LanguageSetup->GameCPUType(), ValueToList(ConfOpt->CPUType(), ";,"), false, ValueWidth, true);
    AddSetting(3003, LanguageSetup->GameCycles(), ValueToList(ConfOpt->Cycles(), ";,"), false, ValueWidth, true);

    /* Memory */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->ProfileEditorMemorySheet());
    AddSetting(4001, LanguageSetup->GameMemory(), ValueToList(ConfOpt->Memory(), ";,"), false, ValueWidth, true);
    AddYesNoSetting(4002, LanguageSetup->GameXMS());
    AddYesNoSetting(4003, LanguageSetup->GameEMS());
    AddYesNoSetting(4004, LanguageSetup->GameUMB());
    AddYesNoSetting(4005, LanguageSetup->ProfileEditorLoadFix());
    AddSpinSetting(4006, LanguageSetup->ProfileEditorLoadFixMemory(), 1, 512, ValueWidth, 64);

    /* Graphics */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->ProfileEditorGraphicsSheet());
    AddSetting(5001, LanguageSetup->GameWindowResolution(), ValueToList(ConfOpt->ResolutionWindow(), ";,"), false, ValueWidth, true);
    AddSetting(5002, LanguageSetup->GameFullscreenResolution(), ValueToList(ConfOpt->ResolutionFullscreen(), ";,"), false, ValueWidth, true);
    AddYesNoSetting(5003, LanguageSetup->GameStartFullscreen());
    AddYesNoSetting(5004, LanguageSetup->GameUseDoublebuffering());
    AddYesNoSetting(5005, LanguageSetup->GameAspectCorrection());
    AddSetting(5006, LanguageSetup->GameRender(), ValueToList(ConfOpt->Render(), ";,"), false, ValueWidth, true);
    AddSetting(5007, LanguageSetup->GameVideoCard(), ValueToList(ConfOpt->Video(), ";,"), false, -1, true);
    AddSetting(5008, LanguageSetup->GameScale(), ValueToList(ConfOpt->Scale(), ";,"), false, -1, true);
    AddSpinSetting(5009, LanguageSetup->GameFrameskip(), 0, 10, ValueWidth);
    if (PrgSetup->AllowGlideSettings()) {
        St = ValueToList(ConfOpt->GlideEmulation(), ";,");
        for (I = 0; I < St->Count(); I++) {
            S = Trim(ExtUpperCase((*St)[I]));
            if (S == "FALSE") (*St)[I] = LanguageSetup->Off();
            if (S == "TRUE") (*St)[I] = LanguageSetup->On();
        }
        AddSetting(5101, LanguageSetup->GameGlideEmulation(), St, false, ValueWidth, false);
        delete St;
        AddSetting(5102, LanguageSetup->GameGlideEmulationPort(), ValueToList(ConfOpt->GlideEmulationPort(), ";,"), true, ValueWidth, true);
        AddSetting(5103, LanguageSetup->GameGlideEmulationLFB(), ValueToList(ConfOpt->GlideEmulationLFB(), ";,"), false, ValueWidth, true);
    }
    if (PrgSetup->AllowVGAChipsetSettings()) {
        AddSetting(5301, LanguageSetup->GameVGAChipset(), ValueToList(ConfOpt->VGAChipsets(), ";,"), false, ValueWidth, true);
        AddSetting(5302, LanguageSetup->GameVideoRam(), ValueToList(ConfOpt->VGAVideoRAM(), ";,"), false, ValueWidth, true);
    }
    if (PrgSetup->AllowPixelShader()) {
        St = GetPixelShaders(PrgSetup->DOSBoxSettings(0)->DosBoxDir());
        AddSetting(5401, LanguageSetup->GamePixelShader(), St, false, -1, true);
    }
    if (PrgSetup->AllowTextModeLineChange()) {
        AddSetting(5501, LanguageSetup->GameTextModeLines(), ValueToList("25;28;50", ";,"), false, ValueWidth, true);
    }

    /* Keyboard */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->ProfileEditorKeyboardSheet());
    AddYesNoSetting(6001, LanguageSetup->GameUseScanCodes());
    AddSetting(6002, LanguageSetup->GameKeyboardLayout(), ValueToList(ConfOpt->KeyboardLayout(), ";,"), false, -1, true);
    AddSetting(6003, LanguageSetup->GameKeyboardCodepage(), ValueToList(ConfOpt->Codepage(), ";,"), false, -1, true);
    St = new TStringList;
    St->Add(LanguageSetup->DoNotChange());
    St->Add(LanguageSetup->Off());
    St->Add(LanguageSetup->On());
    AddSetting(6004, LanguageSetup->GameKeyboardNumLock(), St, false, ValueWidth);
    AddSetting(6005, LanguageSetup->GameKeyboardCapsLock(), St, false, ValueWidth);
    AddSetting(6006, LanguageSetup->GameKeyboardScrollLock(), St, false, ValueWidth);
    delete St;

    /* Mouse */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->ProfileEditorMouseSheet());
    AddYesNoSetting(7001, LanguageSetup->GameAutoLockMouse());
    AddSpinSetting(7002, LanguageSetup->GameMouseSensitivity(), 1, 1000, ValueWidth, 100);
    AddYesNoSetting(7003, LanguageSetup->GameForce2ButtonMouseMode());
    AddYesNoSetting(7004, LanguageSetup->GameSwapMouseButtons());

    /* Sound */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->ProfileEditorSoundSheet());
    AddYesNoSetting(8001, LanguageSetup->ProfileEditorSoundEnableSound());
    AddSetting(8002, LanguageSetup->ProfileEditorSoundSampleRate(), ValueToList(ConfOpt->Rate(), ";,"), false, ValueWidth, true);
    AddSetting(8003, LanguageSetup->ProfileEditorSoundBlockSize(), ValueToList(ConfOpt->Blocksize(), ";,"), true, ValueWidth, true);
    St = new TStringList;
    St->Add("1"); St->Add("5"); St->Add("10"); St->Add("15"); St->Add("20"); St->Add("25"); St->Add("30");
    AddSetting(8004, LanguageSetup->ProfileEditorSoundPrebuffer(), St, true, ValueWidth);
    delete St;
    AddYesNoSetting(8005, LanguageSetup->ProfileEditorSoundMiscEnablePCSpeaker());
    AddSetting(8006, LanguageSetup->ProfileEditorSoundMiscPCSpeakerRate(), ValueToList(ConfOpt->PCRate(), ";,"), false, ValueWidth, true);
    St = new TStringList;
    St->Add(LanguageSetup->ProfileEditorSoundMiscEnableTandyAuto());
    St->Add(LanguageSetup->On());
    St->Add(LanguageSetup->Off());
    AddSetting(8007, LanguageSetup->ProfileEditorSoundMiscEnableTandy(), St, false, ValueWidth);
    delete St;
    AddSetting(8008, LanguageSetup->ProfileEditorSoundMiscTandyRate(), ValueToList(ConfOpt->TandyRate(), ";,"), false, ValueWidth, true);
    AddYesNoSetting(8009, LanguageSetup->ProfileEditorSoundMiscEnableDisneySoundsSource());

    /* Volume */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->ProfileEditorSoundVolumeSheet());
    AddSpinSetting(8051, LanguageSetup->ProfileEditorSoundMasterVolume() + " - " + LanguageSetup->Left(), 0, 200, ValueWidth, 100);
    AddSpinSetting(8052, LanguageSetup->ProfileEditorSoundMasterVolume() + " - " + LanguageSetup->Right(), 0, 200, ValueWidth, 100);
    AddSpinSetting(8053, LanguageSetup->ProfileEditorSoundMiscDisneySoundsSource() + " - " + LanguageSetup->Left(), 0, 200, ValueWidth, 100);
    AddSpinSetting(8054, LanguageSetup->ProfileEditorSoundMiscDisneySoundsSource() + " - " + LanguageSetup->Right(), 0, 200, ValueWidth, 100);
    AddSpinSetting(8055, LanguageSetup->ProfileEditorSoundMiscPCSpeaker() + " - " + LanguageSetup->Left(), 0, 200, ValueWidth, 100);
    AddSpinSetting(8056, LanguageSetup->ProfileEditorSoundMiscPCSpeaker() + " - " + LanguageSetup->Right(), 0, 200, ValueWidth, 100);
    AddSpinSetting(8057, LanguageSetup->ProfileEditorSoundGUS() + " - " + LanguageSetup->Left(), 0, 200, ValueWidth, 100);
    AddSpinSetting(8058, LanguageSetup->ProfileEditorSoundGUS() + " - " + LanguageSetup->Right(), 0, 200, ValueWidth, 100);
    AddSpinSetting(8059, LanguageSetup->ProfileEditorSoundSoundBlaster() + " - " + LanguageSetup->Left(), 0, 200, ValueWidth, 100);
    AddSpinSetting(8060, LanguageSetup->ProfileEditorSoundSoundBlaster() + " - " + LanguageSetup->Right(), 0, 200, ValueWidth, 100);
    AddSpinSetting(8061, LanguageSetup->ProfileEditorSoundFM() + " - " + LanguageSetup->Left(), 0, 200, ValueWidth, 100);
    AddSpinSetting(8062, LanguageSetup->ProfileEditorSoundFM() + " - " + LanguageSetup->Right(), 0, 200, ValueWidth, 100);
    AddSpinSetting(8063, LanguageSetup->ProfileEditorSoundCD() + " - " + LanguageSetup->Left(), 0, 200, ValueWidth, 100);
    AddSpinSetting(8064, LanguageSetup->ProfileEditorSoundCD() + " - " + LanguageSetup->Right(), 0, 200, ValueWidth, 100);

    /* SoundBlaster */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->ProfileEditorSoundSoundBlaster());
    AddSetting(8101, LanguageSetup->ProfileEditorSoundSBType(), ValueToList(ConfOpt->Sblaster(), ";,"), false, ValueWidth, true);
    AddSetting(8102, LanguageSetup->ProfileEditorSoundSBAddress(), ValueToList(ConfOpt->SBBase(), ";,"), false, ValueWidth, true);
    AddSetting(8103, LanguageSetup->ProfileEditorSoundSBIRQ(), ValueToList(ConfOpt->IRQ(), ";,"), false, ValueWidth, true);
    AddSetting(8104, LanguageSetup->ProfileEditorSoundSBDMA(), ValueToList(ConfOpt->Dma(), ";,"), false, ValueWidth, true);
    AddSetting(8105, LanguageSetup->ProfileEditorSoundSBHDMA(), ValueToList(ConfOpt->HDMA(), ";,"), false, ValueWidth, true);
    AddSetting(8106, LanguageSetup->ProfileEditorSoundSBOplMode(), ValueToList(ConfOpt->Oplmode(), ";,"), false, ValueWidth, true);
    AddSetting(8107, LanguageSetup->GameOplemu(), ValueToList(ConfOpt->OplEmu(), ";,"), false, ValueWidth, true);
    AddSetting(8108, LanguageSetup->ProfileEditorSoundSBOplRate(), ValueToList(ConfOpt->OPLRate(), ";,"), false, ValueWidth, true);
    AddYesNoSetting(8109, LanguageSetup->ProfileEditorSoundSBUseMixer());

    /* GUS */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->ProfileEditorSoundGUS());
    AddYesNoSetting(8201, LanguageSetup->ProfileEditorSoundGUSEnabled());
    AddSetting(8202, LanguageSetup->ProfileEditorSoundGUSAddress(), ValueToList(ConfOpt->GUSBase(), ";,"), false, ValueWidth, true);
    AddSetting(8203, LanguageSetup->ProfileEditorSoundGUSRate(), ValueToList(ConfOpt->GUSRate(), ";,"), false, ValueWidth, true);
    AddSetting(8204, LanguageSetup->ProfileEditorSoundGUSIRQ(), ValueToList(ConfOpt->GUSIRQ(), ";,"), false, ValueWidth, true);
    AddSetting(8205, LanguageSetup->ProfileEditorSoundGUSDMA(), ValueToList(ConfOpt->GUSDma(), ";,"), false, ValueWidth, true);
    AddEditSetting(8206, LanguageSetup->ProfileEditorSoundGUSPath(), "C:\\ULTRASND", -1);

    /* MIDI */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->ProfileEditorSoundMIDI());
    AddSetting(8301, LanguageSetup->ProfileEditorSoundMIDIType(), ValueToList(ConfOpt->MPU401(), ";,"), false, ValueWidth, true);
    AddSetting(8302, LanguageSetup->ProfileEditorSoundMIDIDevice(), ValueToList(ConfOpt->MIDIDevice(), ";,"), false, ValueWidth, true);
    AddEditSetting(8303, LanguageSetup->ProfileEditorSoundMIDIConfigInfo(), "", -1);

    if (DefaultValueOnList(ConfOpt->MIDIDevice(), "mt32")) {
        AddSetting(8351, "MT32 " + LanguageSetup->ProfileEditorSoundMIDIMT32Mode(), ValueToList(ConfOpt->MT32ReverbMode(), ";,"), false, ValueWidth, true);
        AddSetting(8352, "MT32 " + LanguageSetup->ProfileEditorSoundMIDIMT32Time(), ValueToList(ConfOpt->MT32ReverbTime(), ";,"), false, ValueWidth, true);
        AddSetting(8353, "MT32 " + LanguageSetup->ProfileEditorSoundMIDIMT32Level(), ValueToList(ConfOpt->MT32ReverbLevel(), ";,"), false, ValueWidth, true);
    }

    /* Innova */
    if (PrgSetup->AllowInnova()) {
        AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->ProfileEditorSoundInnova());
        AddYesNoSetting(8401, LanguageSetup->ProfileEditorSoundInnovaEnable());
        AddSetting(8402, LanguageSetup->ProfileEditorSoundInnovaSampleRate(), ValueToList(ConfOpt->InnovaEmulationSampleRate(), ";,"), false, ValueWidth, true);
        AddSetting(8403, LanguageSetup->ProfileEditorSoundInnovaBaseAddress(), ValueToList(ConfOpt->InnovaEmulationBaseAddress(), ";,"), false, ValueWidth, true);
        AddSetting(8404, LanguageSetup->ProfileEditorSoundInnovaQuality(), ValueToList(ConfOpt->InnovaEmulationQuality(), ";,"), false, ValueWidth, true);
    }

    /* Joystick */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->GameJoysticks());
    AddSetting(9001, LanguageSetup->ProfileEditorSoundJoystickType(), ValueToList(ConfOpt->Joysticks(), ";,"), false, ValueWidth, true);
    AddYesNoSetting(9002, LanguageSetup->ProfileEditorSoundJoystickTimed());
    AddYesNoSetting(9003, LanguageSetup->ProfileEditorSoundJoystickAutoFire());
    AddYesNoSetting(9004, LanguageSetup->ProfileEditorSoundJoystickSwap34());
    AddYesNoSetting(9005, LanguageSetup->ProfileEditorSoundJoystickButtonwrap());

    /* Mounting */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->ProfileEditorMountingSheet());
    AddReplaceMountingSetting(10000, LanguageSetup->ChangeProfilesReplaceFolder(), LanguageSetup->ChangeProfilesReplaceFolderFrom(), LanguageSetup->ChangeProfilesReplaceFolderTo());
    AddYesNoSetting(10001, LanguageSetup->ProfileEditorMountingAutoMountCDsShort());
    AddYesNoSetting(10002, LanguageSetup->ProfileEditorMountingSecureModeShort());
    St = new TStringList;
    St->Add(LanguageSetup->ChangeProfilesMountingChangeSettingsGlobal());
    St->Add(LanguageSetup->ChangeProfilesMountingChangeSettingsLocal());
    AddSetting(10003, LanguageSetup->ChangeProfilesMountingChangeSettings(), St, false, -1);
    delete St;

    /* DOS environment */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->ProfileEditorDOSEnvironmentSheet());
    AddSetting(11001, LanguageSetup->GameReportedDOSVersion(), ValueToList(ConfOpt->ReportedDOSVersion(), ";,"), true, ValueWidth, true);
    AddYesNoSetting(11002, LanguageSetup->ProfileEditorAutoexecUse4DOS());

    /* Starting */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->ProfileEditorStartingSheet());
    AddYesNoSetting(12001, LanguageSetup->ProfileEditorAutoexecOverrideGameStart());
    AddYesNoSetting(12002, LanguageSetup->ProfileEditorAutoexecOverrideMounting());

    /* Network */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - " + LanguageSetup->ProfileEditorNetworkSheet());
    AddYesNoSetting(12501, LanguageSetup->GameIPX());
    St = new TStringList;
    St->Add(LanguageSetup->GameIPXEstablishConnectionNone());
    St->Add(LanguageSetup->GameIPXEstablishConnectionClient());
    St->Add(LanguageSetup->GameIPXEstablishConnectionServer());
    AddSetting(12502, LanguageSetup->GameIPXEstablishConnection(), St, false, ValueWidth, false);
    delete St;
    AddEditSetting(12503, LanguageSetup->GameIPXAddress(), "", -1);
    AddSpinSetting(12504, LanguageSetup->GameIPXPort(), 1, 65535, ValueWidth);
    if (PrgSetup->AllowNe2000()) {
        AddYesNoSetting(12601, LanguageSetup->GameNE2000());
        AddSetting(12602, LanguageSetup->GameNE2000BaseAddress(), ValueToList(ConfOpt->NE2000EmulationBaseAddress(), ";,"), true, ValueWidth, true);
        AddSetting(12603, LanguageSetup->GameNE2000Interrupt(), ValueToList(ConfOpt->NE2000EmulationInterrupt(), ";,"), true, ValueWidth, true);
        AddEditSetting(12604, LanguageSetup->GameNE2000MACAddress(), "AC:DE:48:88:99:AA", -1);
        AddEditSetting(12605, LanguageSetup->GameNE2000RealInterface(), "", -1);
    }

    /* DOSBox Staging (settings only DOSBox Staging has; "default": DOSBox Staging decides) */
    AddCaption(LanguageSetup->ProfileEditorGeneralSheet() + " - DOSBox Staging");
    for (I = 0; I < StagingSettingCount; I++) {
        St = ValueToList(ConfOpt->StagingValues(I), ";,");
        for (J = St->Count() - 1; J >= 0; J--) if (Trim((*St)[J]).isEmpty() || (Trim((*St)[J]) == "default")) St->Delete(J);
        St->Insert(0, "default");
        // (shaders, SoundFonts and MT-32 ROM folders found for the default DOSBox installation)
        for (const QString& V : StagingSettingChoices(I, QString())) if (St->IndexOf(V) < 0) St->Add(V);
        AddSetting(16001 + I, LanguageSetup->ProfileEditorStagingSetting(I), St, StagingSettings[I].Kind != skList, -1, true);
        if (auto* C = qobject_cast<QComboBox*>(Sections.last().Settings.last().Values[0].Control)) if (C->isEditable()) C->setEditText("default");
    }

    /* ScummVM */
    AddCaption(LanguageSetup->ProfileEditorScummVMSheet());
    AddSetting(13001, LanguageSetup->ProfileEditorScummVMPlatform(), ValueToList(ConfOpt->ScummVMPlatform(), ";,"), false, ValueWidth, true);
    AddSetting(13002, LanguageSetup->ProfileEditorScummVMFilter(), ValueToList(ConfOpt->ScummVMFilter(), ";,"), false, -1, true);
    AddSetting(13003, LanguageSetup->ProfileEditorScummVMRenderMode(), ValueToList(ConfOpt->ScummVMRenderMode(), ";,"), false, ValueWidth, true);
    AddYesNoSetting(13004, LanguageSetup->GameStartFullscreen());
    AddYesNoSetting(13005, LanguageSetup->GameAspectCorrection());
    AddEditSetting(13006, LanguageSetup->ProfileEditorScummVMCommandLine(), "", -1);
    AddYesNoSetting(13007, LanguageSetup->ProfileEditorScummVMSubtitles());
    AddYesNoSetting(13008, LanguageSetup->ProfileEditorScummVMConfirmExit());
    AddSpinSetting(13009, LanguageSetup->ProfileEditorScummVMAutosave(), 1, 86400, ValueWidth);
    AddSpinSetting(13010, LanguageSetup->ProfileEditorScummVMTextSpeed(), 1, 1000, ValueWidth);

    /* ScummVM sound */
    AddCaption(LanguageSetup->ProfileEditorScummVMSheet() + " - " + LanguageSetup->ProfileEditorSoundSheet());
    AddSpinSetting(14001, LanguageSetup->ProfileEditorScummVMMusicVolume(), 0, 255, ValueWidth, 192);
    AddSpinSetting(14002, LanguageSetup->ProfileEditorScummVMSpeechVolume(), 0, 255, ValueWidth, 192);
    AddSpinSetting(14003, LanguageSetup->ProfileEditorScummVMSFXVolume(), 0, 255, ValueWidth, 192);
    AddSpinSetting(14004, LanguageSetup->ProfileEditorScummVMMIDIGain(), 0, 1000, ValueWidth, 100);
    St = new TStringList;
    St->Add("11025"); St->Add("22050"); St->Add("44100");
    AddSetting(14005, LanguageSetup->ProfileEditorSoundSampleRate(), St, false, ValueWidth);
    delete St;
    AddYesNoSetting(14006, LanguageSetup->ProfileEditorScummVMSpeechMute());
}

void TMultipleProfilesEditorForm::CaptionClick(QObject* Sender)
{
    int I, J, C, VertScrollBarPos;

    VertScrollBarPos = ui.ScrollBox->verticalScrollBar()->value();
    ui.ScrollBox_Contents->setUpdatesEnabled(false);

    if (Sender != nullptr) {
        const int Tag = Sender->property("tag").toInt();
        Sections[Tag].Open = !Sections[Tag].Open;
    }

    for (I = 0; I < Sections.size(); I++) {
        TSectionData& Sec = Sections[I];
        if (Sec.Open) Sec.Image->setPicture(ImageList->Pixmap(2)); else Sec.Image->setPicture(ImageList->Pixmap(1));

        if (!Sec.Open) {
            C = 0; for (J = 0; J < Sec.Settings.size(); J++) if (Sec.Settings[J].CheckBox->isChecked()) C++;
            Sec.Info->setText(Format(LanguageSetup->ChangeProfilesSectionInfo(), {C, (int)Sec.Settings.size()}));
        }
        Sec.Info->setVisible(!Sec.Open);
        for (J = 0; J < Sec.Settings.size(); J++) {
            Sec.Settings[J].CheckBox->setVisible(Sec.Open);
            Sec.Settings[J].Row->setVisible(Sec.Open);
        }
    }

    // New size of the scroll box contents before the old position is restored
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    ui.ScrollBox_Contents->setUpdatesEnabled(true);
    ui.ScrollBox->verticalScrollBar()->setValue(VertScrollBarPos);
}

void TMultipleProfilesEditorForm::CheckBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I, J, K;
    for (I = 0; I < Sections.size(); I++)
        for (J = 0; J < Sections[I].Settings.size(); J++)
            for (K = 0; K < Sections[I].Settings[J].Values.size(); K++)
                Sections[I].Settings[J].Values[K].Control->setEnabled(Sections[I].Settings[J].CheckBox->isChecked());
}

void TMultipleProfilesEditorForm::SelectButtonClick(QObject* Sender)
{
    int I;

    if (qobject_cast<QPushButton*>(Sender) != nullptr) {
        switch (Sender->property("tag").toInt()) {
            case 0:
            case 1:
                for (I = 0; I < ui.ListBox->count(); I++) CheckListSetChecked(ui.ListBox, I, Sender->property("tag").toInt() == 0);
                break;
            case 2: {
                QPoint P = ui.SelectGenreButton->mapToGlobal(QPoint(0, 0));
                ui.PopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
                break;
            }
        }
        return;
    }

    SelectGamesByPopupMenu(Sender, ui.ListBox);
}

void TMultipleProfilesEditorForm::ComboBoxDropDown(QObject* Sender)
{
    if (QComboBox* C = qobject_cast<QComboBox*>(Sender)) SetComboDropDownDropDownWidth(C);
}

void TMultipleProfilesEditorForm::ComboBoxChange(QObject* Sender)
{
    if (QComboBox* C = qobject_cast<QComboBox*>(Sender)) SetComboHint(C);
}

void TMultipleProfilesEditorForm::SelectFolderButtonClick(QObject* Sender)
{
    int I, J, Nr1, Nr2;
    QString S;

    Nr1 = -1; Nr2 = -1;
    for (I = 0; I < Sections.size(); I++) {
        for (J = 0; J < Sections[I].Settings.size(); J++) {
            if (Sections[I].Settings[J].ID == Sender->property("tag").toInt()) { Nr1 = I; Nr2 = J; break; }
        }
    }
    if (Nr1 < 0) return;

    QLineEdit* Edit = qobject_cast<QLineEdit*>(Sections[Nr1].Settings[Nr2].Values[1].Control);
    S = Trim(Edit->text());
    if (S == "") S = PrgSetup->GameDir();
    if (S == "") S = PrgSetup->BaseDir();
    S = MakeAbsPath(S, PrgSetup->BaseDir());
    if (!SelectDirectory((THandle)winId(), LanguageSetup->ChooseFolder(), S)) return;
    Edit->setText(MakeRelPath(S, PrgSetup->BaseDir(), true));
}

void TMultipleProfilesEditorForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int Nr1 = 0, Nr2 = 0;

    auto ValueActive = [this, &Nr1, &Nr2](int ID) -> bool {
        for (int I = 0; I < Sections.size(); I++) {
            for (int J = 0; J < Sections[I].Settings.size(); J++) {
                if (Sections[I].Settings[J].ID == ID) {
                    if (Sections[I].Settings[J].CheckBox->isChecked()) { Nr1 = I; Nr2 = J; return true; }
                    break;
                }
            }
        }
        return false;
    };
    auto GetControl = [this, &Nr1, &Nr2](int Nr) -> QWidget* { return Sections[Nr1].Settings[Nr2].Values[Nr].Control; };
    auto GetComboIndex = [&]() -> int { return qobject_cast<QComboBox*>(GetControl(0))->currentIndex(); };
    auto GetComboText = [&]() -> QString { return qobject_cast<QComboBox*>(GetControl(0))->currentText(); };
    auto GetEditText = [&]() -> QString { return qobject_cast<QLineEdit*>(GetControl(0))->text(); };
    auto GetYesNo = [&]() -> bool { return qobject_cast<QComboBox*>(GetControl(0))->currentIndex() == 1; };
    auto GetSpinValue = [&]() -> int { return qobject_cast<QSpinBox*>(GetControl(0))->value(); };
    // try J:=StrToInt(...); except J:=Default; end;
    auto StrToIntOr = [](const QString& S, int Default) -> int { try { return StrToInt(S); } catch (...) { return Default; } };

    int I, J;
    TGame* G;
    bool ScummVM, WindowsMode, B;
    TStringList* St;
    QString S;

    for (I = 0; I < ui.ListBox->count(); I++) {
        if (!CheckListChecked(ui.ListBox, I)) continue;
        G = CheckListGame(ui.ListBox, I);
        ScummVM = ScummVMMode(G);
        WindowsMode = WindowsExeMode(G);

        /* Game info */
        if (ValueActive(1001)) G->SetGenre(GetEnglishGenreName(GetComboText()));
        if (ValueActive(1002)) G->SetDeveloper(GetComboText());
        if (ValueActive(1003)) G->SetPublisher(GetComboText());
        if (ValueActive(1004)) G->SetYear(GetComboText());
        if (ValueActive(1005)) G->SetLanguage(GetEnglishLanguageName(GetComboText()));
        if (ValueActive(1006)) G->SetFavorite(GetYesNo());
        if (ValueActive(1007)) SetUserInfo(G, GetComboText(), qobject_cast<QLineEdit*>(GetControl(1))->text());
        if (ValueActive(1008)) DelUserInfo(G, GetComboText());
        for (J = 1; J <= 9; J++) {
            if (ValueActive(1010 + (J - 1) * 2)) G->SetWWWName(J, GetComboText());
            if (ValueActive(1011 + (J - 1) * 2)) G->SetWWW(J, GetComboText());
        }
        if (ValueActive(1100)) {
            B = GetYesNo();
            if (B) G->SetGameExeMD5("OFF"); else { if (G->GameExeMD5() == "OFF") G->SetGameExeMD5(""); }
            if (B) G->SetSetupExeMD5("OFF"); else { if (G->SetupExeMD5() == "OFF") G->SetSetupExeMD5(""); }
        }

        if ((!ScummVM) && (!WindowsMode)) {

            /* DOSBox */
            if (ValueActive(2001)) {
                St = ValueToList(G->Priority(), ",");
                while (St->Count() < 2) St->Add("");
                (*St)[0] = PriorityForeground[Max(0, Min(3, GetComboIndex()))];
                G->SetPriority(ListToValue(St, ','));
                delete St;
            }
            if (ValueActive(2002)) {
                St = ValueToList(G->Priority(), ",");
                while (St->Count() < 2) St->Add("");
                (*St)[1] = PriorityBackground[Max(0, Min(4, GetComboIndex()))];
                G->SetPriority(ListToValue(St, ','));
                delete St;
            }
            if (ValueActive(2003)) G->SetCloseDosBoxAfterGameExit(GetYesNo());
            if (ValueActive(2004)) G->SetCustomDOSBoxDir(GetComboText());
            if (ValueActive(2005)) G->SetCustomDOSBoxDir(GetEditText());

            /* CPU */
            if (ValueActive(3001)) G->SetCore(GetComboText());
            if (ValueActive(3002)) G->SetCPUType(GetComboText());
            if (ValueActive(3003)) G->SetCycles(GetComboText());

            /* Memory */
            if (ValueActive(4001)) {
                try { J = Min(63, Max(1, StrToInt(GetComboText()))); } catch (...) { J = 32; }
                G->SetMemory(J);
            }
            if (ValueActive(4002)) G->SetXMS(GetYesNo());
            if (ValueActive(4003)) G->SetEMS(GetYesNo());
            if (ValueActive(4004)) G->SetUMB(GetYesNo());
            if (ValueActive(4005)) G->SetLoadFix(GetYesNo());
            if (ValueActive(4006)) G->SetLoadFixMemory(Max(1, Min(512, GetSpinValue())));

            /* Graphics */
            if (ValueActive(5001)) G->SetWindowResolution(GetComboText());
            if (ValueActive(5002)) G->SetFullscreenResolution(GetComboText());
            if (ValueActive(5003)) G->SetStartFullscreen(GetYesNo());
            if (ValueActive(5004)) G->SetUseDoublebuffering(GetYesNo());
            if (ValueActive(5005)) G->SetAspectCorrection(GetYesNo());

            if (ValueActive(5006)) G->SetRender(GetComboText());
            if (ValueActive(5007)) {
                S = GetComboText();
                if (Pos('(', S) > 0) S = Trim(Copy(S, 1, Pos('(', S) - 1));
                G->SetVideoCard(S);
            }
            if (ValueActive(5008)) {
                S = GetComboText();
                if (Pos('(', S) == 0) G->SetScale(""); else {
                    S = Copy(S, Pos('(', S) + 1, MaxInt); if (Pos(')', S) == 0) G->SetScale(""); else G->SetScale(Copy(S, 1, Pos(')', S) - 1));
                }
            }
            if (ValueActive(5009)) G->SetFrameSkip(Max(0, Min(10, GetSpinValue())));
            if (PrgSetup->AllowGlideSettings()) {
                if (ValueActive(5101)) {
                    S = GetComboText();
                    if (S == LanguageSetup->On()) S = "true";
                    if (S == LanguageSetup->Off()) S = "false";
                    G->SetGlideEmulation(S);
                }
                if (ValueActive(5102)) G->SetGlidePort(GetComboText());
                if (ValueActive(5103)) G->SetGlideLFB(GetComboText());
            }
            if (PrgSetup->AllowVGAChipsetSettings()) {
                if (ValueActive(5301)) G->SetVGAChipset(GetComboText());
                if (ValueActive(5302)) { J = StrToIntOr(GetComboText(), 512); G->SetVideoRam(J); }
            }
            if (PrgSetup->AllowPixelShader()) {
                if (ValueActive(5401)) G->SetPixelShader(GetComboText());
            }
            if (PrgSetup->AllowTextModeLineChange()) {
                if (ValueActive(5501)) { J = StrToIntOr(GetComboText(), 25); G->SetTextModeLines(J); }
            }

            /* Keyboard */
            if (ValueActive(6001)) G->SetUseScanCodes(GetYesNo());
            if (ValueActive(6002)) G->SetKeyboardLayout(GetComboText());
            if (ValueActive(6003)) G->SetCodepage(GetComboText());
            if (ValueActive(6004)) switch (GetComboIndex()) {
                case 0: G->SetNumLockStatus(""); break;
                case 1: G->SetNumLockStatus("off"); break;
                case 2: G->SetNumLockStatus("on"); break;
            }
            if (ValueActive(6005)) switch (GetComboIndex()) {
                case 0: G->SetCapsLockStatus(""); break;
                case 1: G->SetCapsLockStatus("off"); break;
                case 2: G->SetCapsLockStatus("on"); break;
            }
            if (ValueActive(6006)) switch (GetComboIndex()) {
                case 0: G->SetScrollLockStatus(""); break;
                case 1: G->SetScrollLockStatus("off"); break;
                case 2: G->SetScrollLockStatus("on"); break;
            }

            /* Mouse */
            if (ValueActive(7001)) G->SetAutoLockMouse(GetYesNo());
            if (ValueActive(7002)) G->SetMouseSensitivity(Max(1, Min(1000, GetSpinValue())));
            if (ValueActive(7003)) G->SetForce2ButtonMouseMode(GetYesNo());
            if (ValueActive(7004)) G->SetSwapMouseButtons(GetYesNo());

            /* Sound */
            if (ValueActive(8001)) G->SetMixerNosound(!GetYesNo());

            if (ValueActive(8002)) { try { J = Min(65536, Max(1, StrToInt(GetComboText()))); } catch (...) { J = 22050; } G->SetMixerRate(J); }
            if (ValueActive(8003)) { try { J = Min(65536, Max(1, StrToInt(GetComboText()))); } catch (...) { J = 2048; } G->SetMixerBlocksize(J); }
            if (ValueActive(8004)) { try { J = Min(65536, Max(1, StrToInt(GetComboText()))); } catch (...) { J = 10; } G->SetMixerPrebuffer(J); }
            if (ValueActive(8005)) G->SetSpeakerPC(GetYesNo());
            if (ValueActive(8006)) { try { J = Min(65536, Max(1, StrToInt(GetComboText()))); } catch (...) { J = 10; } G->SetSpeakerRate(J); }
            if (ValueActive(8007)) switch (GetComboIndex()) {
                case 0: G->SetSpeakerTandy("auto"); break;
                case 1: G->SetSpeakerTandy("on"); break;
                case 2: G->SetSpeakerTandy("off"); break;
            }
            // Original code checks ID 8006 here (not 8008) - kept as is
            if (ValueActive(8006)) { try { J = Min(65536, Max(1, StrToInt(GetComboText()))); } catch (...) { J = 10; } G->SetSpeakerTandyRate(J); }
            if (ValueActive(8009)) G->SetSpeakerDisney(GetYesNo());

            /* Volume */
            if (ValueActive(8051)) G->SetMixerVolumeMasterLeft(Max(0, Min(200, GetSpinValue())));
            if (ValueActive(8052)) G->SetMixerVolumeMasterRight(Max(0, Min(200, GetSpinValue())));
            if (ValueActive(8053)) G->SetMixerVolumeDisneyLeft(Max(0, Min(200, GetSpinValue())));
            if (ValueActive(8054)) G->SetMixerVolumeDisneyRight(Max(0, Min(200, GetSpinValue())));
            if (ValueActive(8055)) G->SetMixerVolumeSpeakerLeft(Max(0, Min(200, GetSpinValue())));
            if (ValueActive(8056)) G->SetMixerVolumeSpeakerRight(Max(0, Min(200, GetSpinValue())));
            if (ValueActive(8057)) G->SetMixerVolumeGUSLeft(Max(0, Min(200, GetSpinValue())));
            if (ValueActive(8058)) G->SetMixerVolumeGUSRight(Max(0, Min(200, GetSpinValue())));
            if (ValueActive(8059)) G->SetMixerVolumeSBLeft(Max(0, Min(200, GetSpinValue())));
            if (ValueActive(8060)) G->SetMixerVolumeSBRight(Max(0, Min(200, GetSpinValue())));
            if (ValueActive(8061)) G->SetMixerVolumeFMLeft(Max(0, Min(200, GetSpinValue())));
            if (ValueActive(8062)) G->SetMixerVolumeFMRight(Max(0, Min(200, GetSpinValue())));
            if (ValueActive(8063)) G->SetMixerVolumeCDLeft(Max(0, Min(200, GetSpinValue())));
            if (ValueActive(8064)) G->SetMixerVolumeCDRight(Max(0, Min(200, GetSpinValue())));

            /* SoundBlaster */
            if (ValueActive(8101)) G->SetSBType(GetComboText());
            if (ValueActive(8102)) G->SetSBBase(GetComboText());
            if (ValueActive(8103)) { J = StrToIntOr(GetComboText(), 7); G->SetSBIRQ(J); }
            if (ValueActive(8104)) { J = StrToIntOr(GetComboText(), 1); G->SetSBDMA(J); }
            if (ValueActive(8105)) { J = StrToIntOr(GetComboText(), 5); G->SetSBHDMA(J); }
            if (ValueActive(8106)) G->SetSBOplMode(GetComboText());
            if (ValueActive(8107)) G->SetSBOplEmu(GetComboText());
            if (ValueActive(8108)) { J = StrToIntOr(GetComboText(), 22050); G->SetSBOplRate(J); }
            if (ValueActive(8109)) G->SetSBMixer(GetYesNo());

            /* GUS */
            if (ValueActive(8201)) G->SetGUS(GetYesNo());
            if (ValueActive(8202)) G->SetGUSBase(GetComboText());
            if (ValueActive(8203)) { J = StrToIntOr(GetComboText(), 22050); G->SetGUSRate(J); }
            if (ValueActive(8204)) { J = StrToIntOr(GetComboText(), 5); G->SetGUSIRQ(J); }
            if (ValueActive(8205)) { J = StrToIntOr(GetComboText(), 1); G->SetGUSDMA(J); }
            if (ValueActive(8206)) G->SetGUSUltraDir(GetEditText());

            /* MIDI */
            if (ValueActive(8301)) G->SetMIDIType(GetComboText());
            if (ValueActive(8302)) G->SetMIDIDevice(GetComboText());
            if (ValueActive(8303)) G->SetMIDIConfig(GetEditText());

            if (DefaultValueOnList(GameDB->ConfOpt()->MIDIDevice(), "mt32")) {
                if (ValueActive(8351)) G->SetMIDIMT32Mode(GetComboText());
                if (ValueActive(8352)) G->SetMIDIMT32Time(GetComboText());
                if (ValueActive(8353)) G->SetMIDIMT32Level(GetComboText());
            }

            /* Innova */
            if (PrgSetup->AllowInnova()) {
                if (ValueActive(8401)) G->SetInnova(GetYesNo());
                if (ValueActive(8402)) { if (TryStrToInt(GetComboText(), J)) G->SetInnovaRate(J); }
                if (ValueActive(8403)) G->SetInnovaBase(GetComboText());
                if (ValueActive(8404)) { if (TryStrToInt(GetComboText(), J)) G->SetInnovaQuality(J); }
            }

            /* Joystick */
            if (ValueActive(9001)) G->SetJoystickType(GetComboText());
            if (ValueActive(9002)) G->SetJoystickTimed(GetYesNo());
            if (ValueActive(9003)) G->SetJoystickAutoFire(GetYesNo());
            if (ValueActive(9004)) G->SetJoystickSwap34(GetYesNo());
            if (ValueActive(9005)) G->SetJoystickButtonwrap(GetYesNo());

            /* Mounting */
            if (ValueActive(10000)) ReplaceFolderWork(G, qobject_cast<QComboBox*>(GetControl(0))->currentText(), qobject_cast<QLineEdit*>(GetControl(1))->text());
            if (ValueActive(10001)) G->SetAutoMountCDs(GetYesNo());
            if (ValueActive(10002)) G->SetSecureMode(GetYesNo());
            if (ValueActive(10003)) ChangeMountSetting(G, GetComboIndex() == 0);

            /* DOS environment */
            if (ValueActive(11001)) G->SetReportedDOSVersion(GetComboText());
            if (ValueActive(11002)) G->SetUse4DOS(GetYesNo());

            /* Starting */
            if (ValueActive(12001)) G->SetAutoexecOverridegamestart(GetYesNo());
            if (ValueActive(12002)) G->SetAutoexecOverrideMount(GetYesNo());

            /* Network */
            if (ValueActive(12501)) G->SetIPX(GetYesNo());
            if (ValueActive(12502)) switch (GetComboIndex()) {
                case 0: G->SetIPXType("none"); break;
                case 1: G->SetIPXType("client"); break;
                case 2: G->SetIPXType("server"); break;
            }
            if (ValueActive(12503)) G->SetIPXAddress(GetEditText());
            if (ValueActive(12504)) G->SetIPXPort(IntToStr(GetSpinValue()));
            if (PrgSetup->AllowNe2000()) {
                if (ValueActive(12601)) G->SetNE2000(GetYesNo());
                if (ValueActive(12602)) G->SetNE2000Base(GetComboText());
                if (ValueActive(12603)) { if (TryStrToInt(GetComboText(), J)) G->SetNE2000IRQ(J); }
                if (ValueActive(12604)) G->SetNE2000MACAddress(GetEditText());
                if (ValueActive(12605)) G->SetNE2000RealInterface(GetEditText());
            }

            /* DOSBox Staging */
            for (J = 0; J < StagingSettingCount; J++) if (ValueActive(16001 + J)) {
                S = Trim(GetComboText());
                G->SetStagingSetting(J, S.isEmpty() ? QStringLiteral("default") : S);
            }
        }

        if (ScummVM) {
            /* ScummVM */
            if (ValueActive(13001)) G->SetScummVMPlatform(GetComboText());
            if (ValueActive(13002)) {
                S = GetComboText();
                if (Pos('(', S) == 0) G->SetScummVMFilter(""); else {
                    S = Copy(S, Pos('(', S) + 1, MaxInt);
                    if (Pos(')', S) == 0) G->SetScummVMFilter(""); else G->SetScummVMFilter(Copy(S, 1, Pos(')', S) - 1));
                }
            }
            if (ValueActive(13003)) {
                S = GetComboText();
                if (Pos('(', S) == 0) G->SetScummVMRenderMode(S); else {
                    S = Copy(S, Pos('(', S) + 1, MaxInt);
                    if (Pos(')', S) == 0) G->SetScummVMRenderMode(GetComboText()); else G->SetScummVMRenderMode(Copy(S, 1, Pos(')', S) - 1));
                }
            }
            if (ValueActive(13004)) G->SetStartFullscreen(GetYesNo());
            if (ValueActive(13005)) G->SetAspectCorrection(GetYesNo());
            if (ValueActive(13006)) G->SetScummVMParameters(GetEditText());
            if (ValueActive(13007)) G->SetScummVMSubtitles(GetYesNo());
            if (ValueActive(13008)) G->SetScummVMConfirmExit(GetYesNo());
            if (ValueActive(13009)) G->SetScummVMAutosave(GetSpinValue());
            if (ValueActive(13010)) G->SetScummVMTalkSpeed(GetSpinValue());

            /* ScummVM sound */
            if (ValueActive(14001)) G->SetScummVMMusicVolume(GetSpinValue());
            if (ValueActive(14002)) G->SetScummVMSpeechVolume(GetSpinValue());
            if (ValueActive(14003)) G->SetScummVMSFXVolume(GetSpinValue());
            if (ValueActive(14004)) G->SetScummVMMIDIGain(GetSpinValue());
            if (ValueActive(14005)) G->SetScummVMSampleRate(StrToIntOr(GetComboText(), 22050));
            if (ValueActive(14006)) G->SetScummVMSpeechMute(GetYesNo());
        }
    }

    GameDB->StoreAllValues();
    GameDB->LoadCache();
}

void TMultipleProfilesEditorForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ExtrasEditMultipleProfiles);
}

void TMultipleProfilesEditorForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

// Is the scroll box (or one of its controls) active or under the mouse?
static bool ScrollBoxWheelTarget(QScrollArea* ScrollBox, QWidget* Contents, QTabWidget* PageControl, const QPoint& MousePos)
{
    QWidget* Active = QApplication::focusWidget();
    bool B = (Active == ScrollBox) || ((Active != nullptr) && Contents->isAncestorOf(Active));
    if ((!B) && (PageControl->currentIndex() == 1)) {
        QWidget* C = QApplication::widgetAt(MousePos);
        B = (C == ScrollBox) || (C == ScrollBox->viewport()) || (C == Contents) || ((C != nullptr) && Contents->isAncestorOf(C));
    }
    return B;
}

void TMultipleProfilesEditorForm::FormMouseWheelUp(QObject* Sender, TShiftState Shift, QPoint MousePos, bool& Handled)
{
    Q_UNUSED(Sender); Q_UNUSED(Shift); Q_UNUSED(Handled);
    QScrollBar* Bar = ui.ScrollBox->verticalScrollBar();
    if (ScrollBoxWheelTarget(ui.ScrollBox, ui.ScrollBox_Contents, ui.PageControl, MousePos)) Bar->setValue(Max(0, Bar->value() - Bar->singleStep()));
}

void TMultipleProfilesEditorForm::FormMouseWheelDown(QObject* Sender, TShiftState Shift, QPoint MousePos, bool& Handled)
{
    Q_UNUSED(Sender); Q_UNUSED(Shift); Q_UNUSED(Handled);
    QScrollBar* Bar = ui.ScrollBox->verticalScrollBar();
    if (ScrollBoxWheelTarget(ui.ScrollBox, ui.ScrollBox_Contents, ui.PageControl, MousePos)) Bar->setValue(Min(Bar->maximum(), Bar->value() + Bar->singleStep()));
}

/* global */

bool ShowMultipleProfilesEditorDialog(QWidget* AOwner, TGameDB* AGameDB, bool TemplateMode)
{
    bool result;
    MultipleProfilesEditorForm = new TMultipleProfilesEditorForm(AOwner);
    MultipleProfilesEditorForm->GameDB = AGameDB;
    MultipleProfilesEditorForm->TemplateMode = TemplateMode;
    result = (MultipleProfilesEditorForm->ShowModal() == mrOk);
    delete MultipleProfilesEditorForm;
    MultipleProfilesEditorForm = nullptr;
    return result;
}
