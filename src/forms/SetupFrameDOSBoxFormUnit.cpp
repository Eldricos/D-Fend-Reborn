// C++ port of SetupFrameDOSBoxFormUnit.pas (implementation section).
#include "SetupFrameDOSBoxFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "SetupDosBoxFormUnit.h"
#include "PrgConsts.h"
#include "IconLoaderUnit.h"
#include "DOSBoxLangTools.h"
#include "TextEditPopupUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"

TSetupFrameDOSBoxForm* SetupFrameDOSBoxForm = nullptr;

static constexpr int CSIDL_PROGRAM_FILES_ = 0x0026;   // ShlObj.CSIDL_PROGRAM_FILES

// VistaToolsUnit.IsWindowsVista (Vista or newer)
static bool IsWindowsVista()
{
#ifdef Q_OS_WIN
    return QOperatingSystemVersion::current() >= QOperatingSystemVersion(QOperatingSystemVersion::Windows, 6, 0);
#else
    return false;
#endif
}

TSetupFrameDOSBoxForm::TSetupFrameDOSBoxForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // non visual DFM components
    DosBoxTxtOpenDialog = new TOpenDialog(this);
    DosBoxTxtOpenDialog->DefaultExt = "txt";
    OpenDialog = new TOpenDialog(this);
    SaveDialog = new TSaveDialog(this);

    // DFM event wiring
    connect(ui.DosBoxMapperButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DosBoxMapperButton); });
    connect(ui.DosBoxButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DosBoxButton); });
    connect(ui.FindDosBoxButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.FindDosBoxButton); });
    connect(ui.WarningButton, &QToolButton::clicked, this, [this]() { WarningButtonClick(ui.WarningButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.RestoreDefaultValuesButton, &QPushButton::clicked, this, [this]() { RestoreDefaultValuesButtonClick(ui.RestoreDefaultValuesButton); });
    connect(ui.DosBoxDirEdit, &QLineEdit::textChanged, this, [this]() { DosBoxDirEditChange(ui.DosBoxDirEdit); });
    connect(ui.DosBoxExecutableComboBox, &QComboBox::currentTextChanged, this, [this]() { DosBoxDirEditChange(ui.DosBoxExecutableComboBox); });
    connect(ui.CustomSetsClearButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.CustomSetsClearButton); });
    connect(ui.CustomSetsLoadButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.CustomSetsLoadButton); });
    connect(ui.CustomSetsSaveButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.CustomSetsSaveButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });

    FormCreate(this);   // OnCreate
}

TSetupFrameDOSBoxForm::~TSetupFrameDOSBoxForm()
{
    FormDestroy(this);   // OnDestroy
    delete DosBoxTxtOpenDialog;
    delete OpenDialog;
    delete SaveDialog;
}

void TSetupFrameDOSBoxForm::showEvent(QShowEvent* Event)
{
    TVclForm::showEvent(Event);
    if (!FirstShowDone) {
        FirstShowDone = true;
        FormShow(this);   // OnShow
    }
}

void TSetupFrameDOSBoxForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=...: not needed in Qt

    SetRichEditPopup(ui.CustomSetsMemo);

    DosBoxLang = new TStringList;

    ui.DosBoxDirEdit_EditLabel->setText(LanguageSetup->SetupFormDOSBoxDir());
    ui.WarningButton->setToolTip(LanguageSetup->MsgDlgWarning());
    ui.DosBoxButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.FindDosBoxButton->setToolTip(LanguageSetup->SetupFormSearchDOSBox());
    ui.DosBoxLangLabel->setText(LanguageSetup->SetupFormDOSBoxLang());
    ui.DOSBoxKeyboardLayoutLabel->setText(LanguageSetup->GameKeyboardLayout());
    ui.DOSBoxCodepageLabel->setText(LanguageSetup->GameKeyboardCodepage());
    ui.DosBoxMapperEdit_EditLabel->setText(LanguageSetup->SetupFormDOSBoxMapperFile());
    ui.DosBoxMapperButton->setToolTip(LanguageSetup->ChooseFile());
    ui.CommandLineEdit_EditLabel->setText(LanguageSetup->SetupFormDOSBoxCommandLineParameters());
    // D-Fend Reborn: DOSBox program file (automatic or a given name, e.g.
    // dosbox-x if a folder contains several DOSBox versions)
    ui.DosBoxExecutableLabel->setText(LanguageSetup->SetupFormDOSBoxExecutable());
    {
        const QSignalBlocker Blocker(ui.DosBoxExecutableComboBox);
        ui.DosBoxExecutableComboBox->addItem(LanguageSetup->SetupFormDOSBoxExecutableAuto());
        ui.DosBoxExecutableComboBox->addItem(DosBoxFileName.toLower());
        ui.DosBoxExecutableComboBox->addItem(DosBoxStagingFileName);
        ui.DosBoxExecutableComboBox->addItem(DosBoxXFileName);
    }
    ui.HideDosBoxConsoleCheckBox->setText(LanguageSetup->SetupFormHideDOSBoxConsole());
    ui.CenterDOSBoxCheckBox->setText(LanguageSetup->SetupFormCenterDOSBoxWindow());
    ui.DisableScreensaverCheckBox->setText(LanguageSetup->SetupFormDOSBoxDisableScreensaver());
    ui.WaitOnErrorCheckBox->setText(LanguageSetup->SetupFormDosBoxWaitOnError());
    ui.SDLVideodriverLabel->setText(LanguageSetup->SetupFormDOSBoxSDLVideodriver());
    ui.SDLVideodriverInfoLabel->setText(LanguageSetup->SetupFormDOSBoxSDLVideodriverInfo());
    ui.SDLVideoDriverComboBox->setItemText(0, ui.SDLVideoDriverComboBox->itemText(0) + " (" + LanguageSetup->Default() + ")");

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

    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.RestoreDefaultValuesButton->setText(LanguageSetup->SetupFormDefaultValueReset());

    UserIconLoader->DialogImage(DI_SelectFolder, ui.DosBoxButton);
    UserIconLoader->DialogImage(DI_FindFile, ui.FindDosBoxButton);
    UserIconLoader->DialogImage(DI_SelectFile, ui.DosBoxMapperButton);
    UserIconLoader->DialogImage(DI_ResetDefault, ui.RestoreDefaultValuesButton);
    UserIconLoader->DialogImage(DI_Clear, ui.CustomSetsClearButton);
    UserIconLoader->DialogImage(DI_Load, ui.CustomSetsLoadButton);
    UserIconLoader->DialogImage(DI_Save, ui.CustomSetsSaveButton);
}

void TSetupFrameDOSBoxForm::FormShow(QObject* Sender)
{
    TStringList* St;
    int I;
    QString S, T;

    St = ValueToList(GameDB->ConfOpt()->KeyboardLayout(), ";,"); ui.DOSBoxKeyboardLayoutComboBox->addItems(St->ToQStringList()); delete St;
    St = ValueToList(GameDB->ConfOpt()->Codepage(), ";,"); ui.DOSBoxCodepageComboBox->addItems(St->ToQStringList()); delete St;
    ui.DOSBoxKeyboardLayoutComboBox->setCurrentIndex(0);
    ui.DOSBoxCodepageComboBox->setCurrentIndex(0);

    if (IsDefault) {
        setWindowTitle(Format(LanguageSetup->SetupFormDOSBoxInstallationMoreSettingsCaption(), {LanguageSetup->Default()}));
    } else {
        setWindowTitle(Format(LanguageSetup->SetupFormDOSBoxInstallationMoreSettingsCaption(), {DOSBoxData.Name}));
    }
    {
        // TEdit.Text assignment fires OnChange only when the text changes
        const QSignalBlocker Blocker(ui.DosBoxDirEdit);
        ui.DosBoxDirEdit->setText(DOSBoxData.DosBoxDir);
        const QSignalBlocker Blocker2(ui.DosBoxExecutableComboBox);
        if (Trim(DOSBoxData.DosBoxExecutable).isEmpty()) ui.DosBoxExecutableComboBox->setCurrentIndex(0);
        else ui.DosBoxExecutableComboBox->setCurrentText(Trim(DOSBoxData.DosBoxExecutable));
    }
    DosBoxDirEditChange(Sender);

    ui.DosBoxLangEditComboBox->setCurrentIndex(qMax(0, DosBoxLang->IndexOf(DOSBoxData.DosBoxLanguage)));

    S = Trim(ExtUpperCase(DOSBoxData.KeyboardLayout));
    for (I = 0; I < ui.DOSBoxKeyboardLayoutComboBox->count(); I++) {
        T = ui.DOSBoxKeyboardLayoutComboBox->itemText(I);
        if (Trim(ExtUpperCase(T)) == S) { ui.DOSBoxKeyboardLayoutComboBox->setCurrentIndex(I); break; }
        if (Pos("(", T) != 0) {
            T = Copy(T, Pos("(", T) + 1, MaxInt);
            if (Pos(")", T) != 0) T = Copy(T, 1, Pos(")", T) - 1);
            if (Trim(ExtUpperCase(T)) == S) { ui.DOSBoxKeyboardLayoutComboBox->setCurrentIndex(I); break; }
        }
    }

    S = Trim(ExtUpperCase(DOSBoxData.Codepage));
    for (I = 0; I < ui.DOSBoxCodepageComboBox->count(); I++) {
        T = ui.DOSBoxCodepageComboBox->itemText(I);
        if (Trim(ExtUpperCase(T)) == S) { ui.DOSBoxCodepageComboBox->setCurrentIndex(I); break; }
        if (Pos("(", T) != 0) {
            if (Trim(Copy(T, 1, Pos("(", T) - 1)) == S) { ui.DOSBoxCodepageComboBox->setCurrentIndex(I); break; }
        }
    }

    ui.DosBoxMapperEdit->setText(DOSBoxData.DosBoxMapperFile);
    ui.CommandLineEdit->setText(DOSBoxData.CommandLineParameters);
    ui.HideDosBoxConsoleCheckBox->setChecked(DOSBoxData.HideDosBoxConsole);
    ui.CenterDOSBoxCheckBox->setChecked(DOSBoxData.CenterDOSBoxWindow);
    ui.DisableScreensaverCheckBox->setChecked(DOSBoxData.DisableScreensaver);
    ui.WaitOnErrorCheckBox->setChecked(DOSBoxData.WaitOnError);
    if (Trim(ExtUpperCase(DOSBoxData.SDLVideodriver)) == "WINDIB") ui.SDLVideoDriverComboBox->setCurrentIndex(1); else ui.SDLVideoDriverComboBox->setCurrentIndex(0);

    St = StringToStringList(DOSBoxData.CustomSettings);
    ui.CustomSetsMemo->setPlainText(St->ToQStringList().join('\n'));
    delete St;
}

void TSetupFrameDOSBoxForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    delete DosBoxLang;
    DosBoxLang = nullptr;
}

QString TSetupFrameDOSBoxForm::ExtendLanguageName(const QString& ShortName)
{
    int I;
    QString ShortNameUpper;
    QString S, T;
    TStringList* St;
    QString result;

    ShortNameUpper = ExtUpperCase(ShortName);
    St = GetLanguageList();
    for (I = 0; I < St->Count(); I++) {
        S = (*St)[I];
        if (Pos("(", S) == 0) continue;
        T = Copy(S, Pos("(", S) + 1, MaxInt);
        if (Pos(")", S) == 0) continue;
        T = Copy(T, 1, Pos(")", T) - 1);
        if (ExtUpperCase(T) == ShortNameUpper) { delete St; return S; }
    }

    result = ShortName;
    delete St;
    return result;
}

void TSetupFrameDOSBoxForm::DosBoxDirEditChange(QObject* Sender)
{
    QString S, S2;
    int I, J;

    S = ui.DosBoxLangEditComboBox->currentText();
    if (Pos("(", S) > 0) {
        S2 = Copy(S, Pos("(", S) + 1);
        if (Pos(")", S2) > 0) S2 = Copy(S2, 1, Pos(")", S2) - 1);
        if (S2 != "") S = S2;
    }

    ui.DosBoxLangEditComboBox->clear();
    DosBoxLang->Clear();
    TStringList St;
    GetDOSBoxLangNamesAndFiles(ui.DosBoxDirEdit->text(), &St, DosBoxLang, true);

    I = St.IndexOf(S);
    if (I < 0) I = DosBoxLang->IndexOf(PrgSetup->DOSBoxSettings(0)->DosBoxLanguage());
    if (I < 0) I = St.IndexOf("English");

    for (J = 0; J < St.Count(); J++) ui.DosBoxLangEditComboBox->addItem(ExtendLanguageName(St[J]));
    if (St.Count() > 0) ui.DosBoxLangEditComboBox->setCurrentIndex(qMax(0, I));

    ui.WarningButton->setVisible(OldDOSBoxVersion(CheckDOSBoxVersion(-1, ui.DosBoxDirEdit->text())));
    // (the layout gives the edit the space of the hidden warning button)
}

void TSetupFrameDOSBoxForm::ButtonWork(QObject* Sender)
{
    QString S;

    switch (Sender->property("tag").toInt()) {
        case 0:
            S = ui.DosBoxDirEdit->text();
            if (S == "") S = GetSpecialFolder(0, CSIDL_PROGRAM_FILES_);
            if (SelectDirectory(THandle(0), LanguageSetup->SetupFormDOSBoxDir(), S)) {
                ui.DosBoxDirEdit->setText(S);
            }
            break;
        case 1:
            if (SearchDosBox(this, S)) {
                ui.DosBoxDirEdit->setText(S);
            }
            break;
        case 2:
            DosBoxTxtOpenDialog->Title = LanguageSetup->SetupFormDOSBoxMapperFileTitle();
            DosBoxTxtOpenDialog->Filter = LanguageSetup->SetupFormDOSBoxMapperFileFilter();
            if (Trim(ui.DosBoxMapperEdit->text()) == "")
                DosBoxTxtOpenDialog->InitialDir = PrgDataDir();
            else
                DosBoxTxtOpenDialog->InitialDir = ExtractFilePath(MakeAbsPath(ui.DosBoxMapperEdit->text(), PrgSetup->BaseDir()));
            if (!DosBoxTxtOpenDialog->Execute()) return;
            ui.DosBoxMapperEdit->setText(MakeRelPath(DosBoxTxtOpenDialog->FileName, PrgSetup->BaseDir()));
            break;
        case 3:
            ui.CustomSetsMemo->clear();
            break;
        case 4: {
            ForceDirectories(PrgDataDir() + CustomConfigsSubDir);

            OpenDialog->DefaultExt = "txt";
            OpenDialog->Filter = LanguageSetup->ProfileEditorCustomSetsFilter();
            OpenDialog->Title = LanguageSetup->ProfileEditorCustomSetsLoadTitle();
            OpenDialog->InitialDir = PrgDataDir() + CustomConfigsSubDir;
            if (!OpenDialog->Execute()) return;
            TStringList St;
            if (St.LoadFromFile(OpenDialog->FileName)) {
                ui.CustomSetsMemo->setPlainText(St.ToQStringList().join('\n'));
            } else {
                MessageDlg(Format(LanguageSetup->MessageCouldNotOpenFile(), {OpenDialog->FileName}), mtError, mbOK, 0);
            }
            break;
        }
        case 5: {
            ForceDirectories(PrgDataDir() + CustomConfigsSubDir);

            SaveDialog->DefaultExt = "txt";
            SaveDialog->Filter = LanguageSetup->ProfileEditorCustomSetsFilter();
            SaveDialog->Title = LanguageSetup->ProfileEditorCustomSetsSaveTitle();
            SaveDialog->InitialDir = PrgDataDir() + CustomConfigsSubDir;
            if (!SaveDialog->Execute()) return;
            TStringList St;
            QString Text = ui.CustomSetsMemo->toPlainText();
            if (!Text.isEmpty()) St.Assign(Text.split('\n'));
            if (!St.SaveToFile(SaveDialog->FileName)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {SaveDialog->FileName}), mtError, mbOK, 0);
            }
            break;
        }
    }
}

void TSetupFrameDOSBoxForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    DOSBoxData.DosBoxDir = ui.DosBoxDirEdit->text();
    DOSBoxData.DosBoxExecutable = SelectedExecutable();
    DOSBoxData.DosBoxLanguage = (*DosBoxLang)[ui.DosBoxLangEditComboBox->currentIndex()];
    DOSBoxData.KeyboardLayout = ui.DOSBoxKeyboardLayoutComboBox->itemText(ui.DOSBoxKeyboardLayoutComboBox->currentIndex());
    DOSBoxData.Codepage = ui.DOSBoxCodepageComboBox->itemText(ui.DOSBoxCodepageComboBox->currentIndex());
    DOSBoxData.DosBoxMapperFile = ui.DosBoxMapperEdit->text();
    DOSBoxData.CommandLineParameters = ui.CommandLineEdit->text();
    DOSBoxData.HideDosBoxConsole = ui.HideDosBoxConsoleCheckBox->isChecked();
    DOSBoxData.CenterDOSBoxWindow = ui.CenterDOSBoxCheckBox->isChecked();
    DOSBoxData.DisableScreensaver = ui.DisableScreensaverCheckBox->isChecked();
    DOSBoxData.WaitOnError = ui.WaitOnErrorCheckBox->isChecked();
    if (ui.SDLVideoDriverComboBox->currentIndex() == 1) DOSBoxData.SDLVideodriver = "WinDIB"; else DOSBoxData.SDLVideodriver = "DirectX";
    TStringList St;
    QString Text = ui.CustomSetsMemo->toPlainText();
    if (!Text.isEmpty()) St.Assign(Text.split('\n'));
    DOSBoxData.CustomSettings = StringListToString(&St);
}

void TSetupFrameDOSBoxForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_FileOptionsDOSBox);
}

void TSetupFrameDOSBoxForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

void TSetupFrameDOSBoxForm::RestoreDefaultValuesButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.DOSBoxKeyboardLayoutComboBox->setCurrentIndex(0);
    ui.DOSBoxCodepageComboBox->setCurrentIndex(0);
    ui.DosBoxMapperEdit->setText(".\\mapper.txt");
    ui.CommandLineEdit->setText("");
    ui.HideDosBoxConsoleCheckBox->setChecked(true);
    ui.CenterDOSBoxCheckBox->setChecked(false);
    ui.DisableScreensaverCheckBox->setChecked(false);
    if (IsWindowsVista()) ui.SDLVideoDriverComboBox->setCurrentIndex(1); else ui.SDLVideoDriverComboBox->setCurrentIndex(0);
}

QString TSetupFrameDOSBoxForm::SelectedExecutable()
{
    const QString S = Trim(ui.DosBoxExecutableComboBox->currentText());
    if (S.isEmpty() || (S == ui.DosBoxExecutableComboBox->itemText(0))) return QString();
    return S;
}

void TSetupFrameDOSBoxForm::WarningButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (OldDOSBoxVersion(CheckDOSBoxVersion(-1, ui.DosBoxDirEdit->text()))) DOSBoxOutdatedWarning(ui.DosBoxDirEdit->text());
}

/* global */

bool ShowSetupFrameDOSBoxDialog(QWidget* AOwner, TDOSBoxData& DOSBoxData, bool IsDefault, TGameDB* GameDB)
{
    bool result;
    SetupFrameDOSBoxForm = new TSetupFrameDOSBoxForm(AOwner);
    SetupFrameDOSBoxForm->DOSBoxData = DOSBoxData;
    SetupFrameDOSBoxForm->IsDefault = IsDefault;
    SetupFrameDOSBoxForm->GameDB = GameDB;
    result = (SetupFrameDOSBoxForm->ShowModal() == mrOk);
    if (result) DOSBoxData = SetupFrameDOSBoxForm->DOSBoxData;
    delete SetupFrameDOSBoxForm;
    SetupFrameDOSBoxForm = nullptr;
    return result;
}
