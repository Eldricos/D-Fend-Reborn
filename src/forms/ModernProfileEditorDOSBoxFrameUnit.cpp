// C++ port of ModernProfileEditorDOSBoxFrameUnit.pas (implementation section).
#include "ModernProfileEditorDOSBoxFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "HelpConsts.h"
#include "IconLoaderUnit.h"
#include "TextEditPopupUnit.h"

static const QString FPriority[4] = {"lower", "normal", "higher", "highest"};
static const QString BPriority[5] = {"pause", "lower", "normal", "higher", "highest"};

// TStrings.IndexOf (case insensitive)
static int ComboIndexOf(QComboBox* C, const QString& S)
{
    for (int I = 0; I < C->count(); I++) if (C->itemText(I).compare(S, Qt::CaseInsensitive) == 0) return I;
    return -1;
}

// Memo.Lines.Assign(St)
static void SetMemoLines(QTextEdit* Memo, TStringList* St)
{
    Memo->setPlainText(St->ToQStringList().join('\n'));
}

// Memo.Lines (as TStringList, caller deletes)
static TStringList* MemoLines(QTextEdit* Memo)
{
    TStringList* St = new TStringList();
    QString T = Memo->toPlainText();
    if (T != "") {
        QStringList L = T.split('\n');
        if (!L.isEmpty() && L.last().isEmpty()) L.removeLast();
        St->AddStrings(L);
    }
    return St;
}

/* TModernProfileEditorDOSBoxFrame */

TModernProfileEditorDOSBoxFrame::TModernProfileEditorDOSBoxFrame(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    OpenDialog = new TOpenDialog(this);
    SaveDialog = new TSaveDialog(this);

    connect(ui.CustomDOSBoxInstallationButton, &QToolButton::clicked, this, [this]() { CustomDOSBoxInstallationButtonClick(ui.CustomDOSBoxInstallationButton); });
    connect(ui.DefaultDOSBoxInstallationRadioButton, &QRadioButton::toggled, this, [this](bool On) { if (On) DOSBoxInstallationTypeClick(ui.DefaultDOSBoxInstallationRadioButton); });
    connect(ui.CustomDOSBoxInstallationRadioButton, &QRadioButton::toggled, this, [this](bool On) { if (On) DOSBoxInstallationTypeClick(ui.CustomDOSBoxInstallationRadioButton); });
    connect(ui.CustomDOSBoxInstallationEdit, &QLineEdit::textChanged, this, [this]() { CustomDOSBoxInstallationEditChange(ui.CustomDOSBoxInstallationEdit); });
    connect(ui.CustomSetsClearButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.CustomSetsClearButton); });
    connect(ui.CustomSetsLoadButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.CustomSetsLoadButton); });
    connect(ui.CustomSetsSaveButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.CustomSetsSaveButton); });
    connect(ui.DOSBoxInstallationComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { DOSBoxInstallationComboBoxChange(ui.DOSBoxInstallationComboBox); });
    connect(ui.UserLanguageCheckBox, &QCheckBox::toggled, this, [this]() { UserLanguageCheckBoxClick(ui.UserLanguageCheckBox); });
    connect(ui.UserConsoleCheckBox, &QCheckBox::toggled, this, [this]() { UserConsoleCheckBoxClick(ui.UserConsoleCheckBox); });

    DosBoxLang = new TStringList();
}

TModernProfileEditorDOSBoxFrame::~TModernProfileEditorDOSBoxFrame()
{
    delete DosBoxLang;
    delete OpenDialog;
    delete SaveDialog;
}

void TModernProfileEditorDOSBoxFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    int I;
    QString S;

    // NoFlicker(...): not needed with Qt

    SetRichEditPopup(ui.CustomSetsMemo);

    ui.DOSBoxForegroundPriorityLabel->setText(LanguageSetup->GamePriorityForeground());
    ui.DOSBoxForegroundPriorityComboBox->clear();
    ui.DOSBoxForegroundPriorityComboBox->addItem(LanguageSetup->GamePriorityLower());
    ui.DOSBoxForegroundPriorityComboBox->addItem(LanguageSetup->GamePriorityNormal());
    ui.DOSBoxForegroundPriorityComboBox->addItem(LanguageSetup->GamePriorityHigher());
    ui.DOSBoxForegroundPriorityComboBox->addItem(LanguageSetup->GamePriorityHighest());
    ui.DOSBoxBackgroundPriorityLabel->setText(LanguageSetup->GamePriorityBackground());
    ui.DOSBoxBackgroundPriorityComboBox->clear();
    ui.DOSBoxBackgroundPriorityComboBox->addItem(LanguageSetup->GamePriorityPause());
    ui.DOSBoxBackgroundPriorityComboBox->addItem(LanguageSetup->GamePriorityLower());
    ui.DOSBoxBackgroundPriorityComboBox->addItem(LanguageSetup->GamePriorityNormal());
    ui.DOSBoxBackgroundPriorityComboBox->addItem(LanguageSetup->GamePriorityHigher());
    ui.DOSBoxBackgroundPriorityComboBox->addItem(LanguageSetup->GamePriorityHighest());
    ui.CloseDOSBoxOnExitCheckBox->setText(LanguageSetup->GameCloseDOSBoxAfterGameExit());
    ui.RunAsAdminCheckBox->setText(LanguageSetup->ProfileEditorRunAsAdmin());
    ui.RunAsAdminCheckBox->setVisible(PrgSetup->OfferRunAsAdmin());
    ui.DefaultDOSBoxInstallationRadioButton->setText(LanguageSetup->GameDOSBoxVersionDefault());
    ui.CustomDOSBoxInstallationRadioButton->setText(LanguageSetup->GameDOSBoxVersionCustom());
    ui.CustomDOSBoxInstallationButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.UserLanguageCheckBox->setText(LanguageSetup->GameDOSBoxLanguageCustom());
    ui.UserConsoleCheckBox->setText(LanguageSetup->GameDOSBoxConsole());
    ui.UserConsoleComboBox->clear();
    ui.UserConsoleComboBox->addItem(LanguageSetup->GameDOSBoxConsoleHide());
    ui.UserConsoleComboBox->addItem(LanguageSetup->GameDOSBoxConsoleShow());
    ui.CustomSetsLabel->setText(LanguageSetup->ProfileEditorCustomSetsSheet());
    {
        QFont F = ui.CustomSetsMemo->font();
        F.setFamily("Courier New");
        F.setStyleHint(QFont::Monospace);
        ui.CustomSetsMemo->setFont(F);
    }
    ui.CustomSetsClearButton->setText(LanguageSetup->Del());
    ui.CustomSetsLoadButton->setText(LanguageSetup->Load());
    ui.CustomSetsSaveButton->setText(LanguageSetup->Save());
    UserIconLoader->DialogImage(DI_SelectFolder, ui.CustomDOSBoxInstallationButton);
    UserIconLoader->DialogImage(DI_Clear, ui.CustomSetsClearButton);
    UserIconLoader->DialogImage(DI_Load, ui.CustomSetsLoadButton);
    UserIconLoader->DialogImage(DI_Save, ui.CustomSetsSaveButton);

    for (I = 0; I < PrgSetup->DOSBoxSettingsCount(); I++) {
        if (I == 0) S = LanguageSetup->Default(); else S = PrgSetup->DOSBoxSettings(I)->Name();
        ui.DOSBoxInstallationComboBox->addItem(S + " (" + MakeRelPath(PrgSetup->DOSBoxSettings(I)->DosBoxDir(), PrgSetup->BaseDir(), true) + ")");
    }
    ui.DOSBoxInstallationComboBox->setCurrentIndex(0);

    ProfileName = InitData.CurrentProfileName;
    ProfileExe = InitData.CurrentProfileExe;
    ProfileSetup = InitData.CurrentProfileSetup;
    ProfileDOSBoxInstallation = InitData.CurrentDOSBoxInstallation;
    ProfileCaptureDir = InitData.CurrentCaptureDir;
    FOnProfileNameChange = InitData.OnProfileNameChange;

    setProperty("HelpContext", ID_ProfileEditDOSBox);  // HelpContext
}

void TModernProfileEditorDOSBoxFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    TStringList* St;
    QString S, T;
    int I;
    bool B;

    St = ValueToList(Game->Priority(), ",");
    if (St->Count() >= 1 && (*St)[0] != "") S = (*St)[0]; else S = FPriority[2];
    if (St->Count() >= 2 && (*St)[1] != "") T = (*St)[1]; else T = BPriority[2];
    delete St;
    S = Trim(ExtUpperCase(S));
    ui.DOSBoxForegroundPriorityComboBox->setCurrentIndex(1);
    for (I = 0; I < ui.DOSBoxForegroundPriorityComboBox->count(); I++) if (ExtUpperCase(FPriority[I]) == S) {
        ui.DOSBoxForegroundPriorityComboBox->setCurrentIndex(I);
        break;
    }
    T = Trim(ExtUpperCase(T));
    ui.DOSBoxBackgroundPriorityComboBox->setCurrentIndex(2);
    for (I = 0; I < ui.DOSBoxBackgroundPriorityComboBox->count(); I++) if (ExtUpperCase(BPriority[I]) == T) {
        ui.DOSBoxBackgroundPriorityComboBox->setCurrentIndex(I);
        break;
    }

    ui.CloseDOSBoxOnExitCheckBox->setChecked(Game->CloseDosBoxAfterGameExit());
    ui.RunAsAdminCheckBox->setChecked(Game->RunAsAdmin());

    S = Trim(ExtUpperCase(Game->CustomDOSBoxDir())); B = false;
    if (S == "") S = "DEFAULT";
    for (I = 0; I < PrgSetup->DOSBoxSettingsCount(); I++) {
        if (I == 0) T = "DEFAULT"; else T = PrgSetup->DOSBoxSettings(I)->Name();
        if (S == Trim(ExtUpperCase(T))) {
            B = true;
            ui.DOSBoxInstallationComboBox->setCurrentIndex(I);
            ui.DefaultDOSBoxInstallationRadioButton->setChecked(true);
            break;
        }
    }
    if (!B) {
        ui.CustomDOSBoxInstallationRadioButton->setChecked(true);
        ui.CustomDOSBoxInstallationEdit->setText(Game->CustomDOSBoxDir());
    }

    switch (Game->ShowConsoleWindow()) {
    case 0: ui.UserConsoleCheckBox->setChecked(true); ui.UserConsoleComboBox->setCurrentIndex(0); break;
    case 2: ui.UserConsoleCheckBox->setChecked(true); ui.UserConsoleComboBox->setCurrentIndex(1); break;
    default: /* 1 : */ ui.UserConsoleCheckBox->setChecked(false); break;
    }
    UserConsoleCheckBoxClick(this);
    if (!ui.UserConsoleCheckBox->isChecked()) SelectInConsoleList();

    UpdateLanguageList();
    S = Trim(ExtUpperCase(Game->CustomDOSBoxLanguage()));
    if (S == "" || S == "DEFAULT") {
        ui.UserLanguageCheckBox->setChecked(false);
        SelectInLanguageList("");
    } else {
        ui.UserLanguageCheckBox->setChecked(true);
        SelectInLanguageList(ChangeFileExt(ExtractFileName(Game->CustomDOSBoxLanguage()), ""));
    }
    UserLanguageCheckBoxClick(this);

    St = StringToStringList(Game->CustomSettings());
    SetMemoLines(ui.CustomSetsMemo, St);
    delete St;
}

static void FindAndAddLngFiles(const QString& Dir, QComboBox* St, TStringList* St2)
{
    TSearchRec Rec;
    int I;

    I = FindFirst(Dir + "*.lng", faAnyFile, Rec);
    while (I == 0) {
        St->addItem(ChangeFileExt(Rec.Name, ""));
        St2->Add(Dir + Rec.Name);
        I = FindNext(Rec);
    }
    FindClose(Rec);
}

void TModernProfileEditorDOSBoxFrame::UpdateDOSBoxInstallationString()
{
    if (ui.DefaultDOSBoxInstallationRadioButton->isChecked()) {
        *ProfileDOSBoxInstallation = PrgSetup->DOSBoxSettings(ui.DOSBoxInstallationComboBox->currentIndex())->Name();
    } else {
        *ProfileDOSBoxInstallation = ui.CustomDOSBoxInstallationEdit->text();
    }
}

void TModernProfileEditorDOSBoxFrame::UpdateLanguageList()
{
    QString Save, DosBoxDir;

    if (ui.DefaultDOSBoxInstallationRadioButton->isChecked()) {
        DosBoxDir = IncludeTrailingPathDelimiter(PrgSetup->DOSBoxSettings(ui.DOSBoxInstallationComboBox->currentIndex())->DosBoxDir());
        FOnProfileNameChange(this, *ProfileName, *ProfileExe, *ProfileSetup, "", "", PrgSetup->DOSBoxSettings(ui.DOSBoxInstallationComboBox->currentIndex())->Name(), *ProfileCaptureDir);
    } else {
        DosBoxDir = IncludeTrailingPathDelimiter(ui.CustomDOSBoxInstallationEdit->text());
        FOnProfileNameChange(this, *ProfileName, *ProfileExe, *ProfileSetup, "", "", ui.CustomDOSBoxInstallationEdit->text(), *ProfileCaptureDir);
    }

    if (ui.UserLanguageComboBox->currentIndex() >= 0) Save = ui.UserLanguageComboBox->itemText(ui.UserLanguageComboBox->currentIndex()); else Save = "";
    {
        const QSignalBlocker Blocker(ui.UserLanguageComboBox);  // Items.BeginUpdate
        ui.UserLanguageComboBox->clear();
        DosBoxLang->Clear();

        ui.UserLanguageComboBox->addItem("English");
        DosBoxLang->Add("English");

        FindAndAddLngFiles(IncludeTrailingPathDelimiter(DosBoxDir), ui.UserLanguageComboBox, DosBoxLang);
        FindAndAddLngFiles(PrgDir() + LanguageSubDir + PathDelim, ui.UserLanguageComboBox, DosBoxLang);
        ui.UserLanguageComboBox->setCurrentIndex(Max(0, ComboIndexOf(ui.UserLanguageComboBox, Save)));
    }

    SelectInLanguageList(Save);
}

void TModernProfileEditorDOSBoxFrame::UserLanguageCheckBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.UserLanguageComboBox->setEnabled(ui.UserLanguageCheckBox->isChecked());
}

void TModernProfileEditorDOSBoxFrame::UserConsoleCheckBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.UserConsoleComboBox->setEnabled(ui.UserConsoleCheckBox->isChecked());
}

void TModernProfileEditorDOSBoxFrame::SelectInLanguageList(const QString& LangName)
{
    int I;
    QString S;

    if (Trim(LangName) != "") {
        I = ComboIndexOf(ui.UserLanguageComboBox, LangName);
        if (I >= 0) { ui.UserLanguageComboBox->setCurrentIndex(I); return; }
    }

    ui.UserLanguageComboBox->setCurrentIndex(0);
    if (!ui.DefaultDOSBoxInstallationRadioButton->isChecked()) return;

    S = ChangeFileExt(ExtractFileName(PrgSetup->DOSBoxSettings(ui.DOSBoxInstallationComboBox->currentIndex())->DosBoxLanguage()), "");
    ui.UserLanguageComboBox->setCurrentIndex(Max(0, ComboIndexOf(ui.UserLanguageComboBox, S)));
}

void TModernProfileEditorDOSBoxFrame::SelectInConsoleList()
{
    ui.UserConsoleComboBox->setCurrentIndex(0);
    if (!ui.DefaultDOSBoxInstallationRadioButton->isChecked()) return;

    if (PrgSetup->DOSBoxSettings(ui.DOSBoxInstallationComboBox->currentIndex())->HideDosBoxConsole())
        ui.UserConsoleComboBox->setCurrentIndex(0);
    else
        ui.UserConsoleComboBox->setCurrentIndex(1);
}

void TModernProfileEditorDOSBoxFrame::GetGame(TGame* Game)
{
    Game->SetPriority(FPriority[ui.DOSBoxForegroundPriorityComboBox->currentIndex()] + "," + BPriority[ui.DOSBoxBackgroundPriorityComboBox->currentIndex()]);
    Game->SetCloseDosBoxAfterGameExit(ui.CloseDOSBoxOnExitCheckBox->isChecked());
    Game->SetRunAsAdmin(ui.RunAsAdminCheckBox->isChecked());
    if (ui.DefaultDOSBoxInstallationRadioButton->isChecked()) {
        Game->SetCustomDOSBoxDir(PrgSetup->DOSBoxSettings(ui.DOSBoxInstallationComboBox->currentIndex())->Name());
    } else {
        Game->SetCustomDOSBoxDir(ui.CustomDOSBoxInstallationEdit->text());
    }

    if (ui.UserLanguageCheckBox->isChecked()) Game->SetCustomDOSBoxLanguage(ExtractFileName((*DosBoxLang)[ui.UserLanguageComboBox->currentIndex()])); else Game->SetCustomDOSBoxLanguage("default");
    if (ui.UserConsoleCheckBox->isChecked()) {
        if (ui.UserConsoleComboBox->currentIndex() == 1) Game->SetShowConsoleWindow(2); else Game->SetShowConsoleWindow(0);
    } else Game->SetShowConsoleWindow(1);

    TStringList* St = MemoLines(ui.CustomSetsMemo);
    Game->SetCustomSettings(StringListToString(St));
    delete St;
}

void TModernProfileEditorDOSBoxFrame::CustomDOSBoxInstallationButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    S = Trim(ui.CustomDOSBoxInstallationEdit->text());
    if (S == "") S = PrgSetup->DOSBoxSettings(0)->DosBoxDir();
    S = MakeAbsPath(S, PrgSetup->BaseDir());
    if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
    S = MakeRelPath(S, PrgSetup->BaseDir(), true);
    if (S == "") return;
    ui.CustomDOSBoxInstallationEdit->setText(IncludeTrailingPathDelimiter(S));
}

void TModernProfileEditorDOSBoxFrame::DOSBoxInstallationTypeClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (!FOnProfileNameChange) return;  // OnClick before InitGUI
    UpdateLanguageList();
    if (!ui.UserLanguageCheckBox->isChecked()) SelectInLanguageList("");
    if (!ui.UserConsoleCheckBox->isChecked()) SelectInConsoleList();
    UpdateDOSBoxInstallationString();
}

void TModernProfileEditorDOSBoxFrame::DOSBoxInstallationComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.DefaultDOSBoxInstallationRadioButton->setChecked(true);
    UpdateLanguageList();
    if (!ui.UserLanguageCheckBox->isChecked()) SelectInLanguageList("");
    if (!ui.UserConsoleCheckBox->isChecked()) SelectInConsoleList();
    UpdateDOSBoxInstallationString();
}

void TModernProfileEditorDOSBoxFrame::CustomDOSBoxInstallationEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (!FOnProfileNameChange) return;  // OnChange before InitGUI
    ui.CustomDOSBoxInstallationRadioButton->setChecked(true);
    UpdateLanguageList();
    if (!ui.UserLanguageCheckBox->isChecked()) SelectInLanguageList("");
    UpdateDOSBoxInstallationString();
}

void TModernProfileEditorDOSBoxFrame::ButtonWork(QObject* Sender)
{
    switch (Sender->property("tag").toInt()) {
    case 0: ui.CustomSetsMemo->clear(); break;
    case 1: {
        ForceDirectories(PrgDataDir() + CustomConfigsSubDir);

        OpenDialog->DefaultExt = "txt";
        OpenDialog->Filter = LanguageSetup->ProfileEditorCustomSetsFilter();
        OpenDialog->Title = LanguageSetup->ProfileEditorCustomSetsLoadTitle();
        OpenDialog->InitialDir = PrgDataDir() + CustomConfigsSubDir;
        if (!OpenDialog->Execute()) return;
        TStringList St;
        if (St.LoadFromFile(OpenDialog->FileName)) {
            SetMemoLines(ui.CustomSetsMemo, &St);
        } else {
            MessageDlg(Format(LanguageSetup->MessageCouldNotOpenFile(), {OpenDialog->FileName}), mtError, mbOK, 0);
        }
        break;
    }
    case 2: {
        ForceDirectories(PrgDataDir() + CustomConfigsSubDir);

        SaveDialog->DefaultExt = "txt";
        SaveDialog->Filter = LanguageSetup->ProfileEditorCustomSetsFilter();
        SaveDialog->Title = LanguageSetup->ProfileEditorCustomSetsSaveTitle();
        SaveDialog->InitialDir = PrgDataDir() + CustomConfigsSubDir;
        if (!SaveDialog->Execute()) return;
        TStringList* St = MemoLines(ui.CustomSetsMemo);
        if (!St->SaveToFile(SaveDialog->FileName)) {
            MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {SaveDialog->FileName}), mtError, mbOK, 0);
        }
        delete St;
        break;
    }
    }
}
