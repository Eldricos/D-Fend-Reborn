// C++ port of SelectTemplateForZipImportFormUnit.pas (implementation section).
#include "SelectTemplateForZipImportFormUnit.h"
#include "ColorScheme.h"
#include "VirtualKeys.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "IconLoaderUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"

TSelectTemplateForZipImportForm *SelectTemplateForZipImportForm = nullptr;

// TComboBox.Items.Objects[I] (stored as Qt::UserRole data)
static int ComboObject(QComboBox *C, int I) { return (I >= 0 && I < C->count()) ? C->itemData(I).toInt() : 0; }

TSelectTemplateForZipImportForm::TSelectTemplateForZipImportForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.TemplateType1RadioButton, &QRadioButton::clicked, this, [this]() { TypeSelectClick(ui.TemplateType1RadioButton); });
    connect(ui.TemplateType2RadioButton, &QRadioButton::clicked, this, [this]() { TypeSelectClick(ui.TemplateType2RadioButton); });
    connect(ui.TemplateType3RadioButton, &QRadioButton::clicked, this, [this]() { TypeSelectClick(ui.TemplateType3RadioButton); });
    connect(ui.ProfileNameEdit, &QLineEdit::textChanged, this, [this]() { ProfileNameEditChange(ui.ProfileNameEdit); });
    connect(ui.TemplateComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { TemplateComboBoxChange(ui.TemplateComboBox); });
    connect(ui.FolderEdit, &QLineEdit::textChanged, this, [this]() { FolderEditChange(ui.FolderEdit); });
    connect(ui.InstallSupportButton, &QPushButton::clicked, this, [this]() { InstallSupportButtonClick(ui.InstallSupportButton); });
    // OnDropDown
    ui.TemplateComboBox->installEventFilter(this);
    ui.ProgramFileComboBox->installEventFilter(this);
    ui.SetupFileComboBox->installEventFilter(this);
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        quint16 K = QtKeyToVK(Key);
        FormKeyDown(this, K, Mods);
        return (Key == Qt::Key_F1) && (Mods == Qt::NoModifier);
    });
    FormCreate(this);   // OnCreate
}

TSelectTemplateForZipImportForm::~TSelectTemplateForZipImportForm() = default;

void TSelectTemplateForZipImportForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

bool TSelectTemplateForZipImportForm::eventFilter(QObject *O, QEvent *E)
{
    if (O == ui.TemplateComboBox || O == ui.ProgramFileComboBox || O == ui.SetupFileComboBox) {
        bool Drop = (E->type() == QEvent::MouseButtonPress);
        if (E->type() == QEvent::KeyPress) {
            auto *KE = static_cast<QKeyEvent *>(E);
            Drop = (KE->key() == Qt::Key_F4) || ((KE->modifiers() & Qt::AltModifier) && (KE->key() == Qt::Key_Down || KE->key() == Qt::Key_Up));
        }
        if (Drop) ComboBoxDropDown(O);
    }
    return TVclForm::eventFilter(O, E);
}

void TSelectTemplateForZipImportForm::FormCreate(QObject * /*Sender*/)
{
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt
    {
        QPalette P = ui.WarningLabel->palette();
        P.setColor(QPalette::WindowText, TextColor(clRed));
        ui.WarningLabel->setPalette(P);
    }

    ui.ProfileNameEdit_EditLabel->setText(LanguageSetup->AutoDetectProfileEditProfileName());
    ui.FolderEdit_EditLabel->setText(LanguageSetup->MenuFileImportZIPDestinationFolder());
    ui.WarningLabel->setText(LanguageSetup->MenuFileImportZIPDestinationFolderWarning());
    ui.TemplateType1RadioButton->setText(LanguageSetup->AutoDetectProfileEditTemplateTypeAutoSetup2());
    ui.TemplateType2RadioButton->setText(LanguageSetup->AutoDetectProfileEditTemplateTypeAutoSetup1());
    ui.TemplateType3RadioButton->setText(LanguageSetup->AutoDetectProfileEditTemplateTypeUser());
    ui.TemplateLabel->setText(LanguageSetup->AutoDetectProfileEditTemplate());
    ui.ProgramFileLabel->setText(LanguageSetup->ProfileEditorGameEXE());
    ui.SetupFileLabel->setText(LanguageSetup->ProfileEditorSetupEXE());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.InstallSupportButton->setText(LanguageSetup->InstallationSupportZipImportUse());
    ui.InstallSupportButton->setToolTip(LanguageSetup->InstallationSupportZipImportUseHint());
    ui.HelpButton->setText(LanguageSetup->Help());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DirectLoad("Main", 0, ui.InstallSupportButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    JustChanging = false;
    ProfileNameChanged = false;
    UseInstallSupport = false;
}

void TSelectTemplateForZipImportForm::FormShow(QObject * /*Sender*/)
{
    QString S;
    if (Trim(ArchivFileName).isEmpty()) S = LanguageSetup->MenuFileImportFolder(); else S = LanguageSetup->MenuFileImportZIPCaption();
    while (!S.isEmpty() && S[S.length() - 1] == '.') S.chop(1);
    setWindowTitle(S);
}

static QString AvoidSomeNames(TStringList *St, int SetupNumber)
{
    QString result;
    bool OK;
    QString S;

    for (int I = 0; I < St->Count(); I++) if (I != SetupNumber) {
        OK = true; S = ExtUpperCase(ChangeFileExt((*St)[I], ""));
        for (const QString &X : IgnoreGameExeFilesIgnore) if (S == X) { OK = false; break; }
        if (!OK) continue;

        for (const QString &X : ProgramExeFiles) if (S == X) {
            result = (*St)[I]; return result;
        }
    }

    for (int I = 0; I < St->Count(); I++) if (I != SetupNumber) {
        OK = true; S = ExtUpperCase(ChangeFileExt((*St)[I], ""));
        for (const QString &X : IgnoreGameExeFilesIgnore) if (S == X) { OK = false; break; }
        if (OK) result = (*St)[I];   // File is ok in level 1

        if (OK) for (const QString &X : SetupExeFilesLevel1) if (S == X) { OK = false; break; }
        if (OK) for (const QString &X : SetupExeFilesLevel2) if (S == X) { OK = false; break; }
        if (OK) for (const QString &X : SetupExeFilesLevel3) if (S == X) { OK = false; break; }
        if (OK) for (const QString &X : SetupExeFilesLevel4) if (S == X) { OK = false; break; }

        if (OK) return result;   // File is ok in level 2
    }
    if (!result.isEmpty()) return result;   // File was ok in level 1

    for (int I = 0; I < St->Count(); I++) if (I != SetupNumber) { result = (*St)[I]; return result; }

    if (St->Count() > 0) result = (*St)[0];
    return result;
}

QString TSelectTemplateForZipImportForm::GetProfileNameFromFile(const QString &TempFolder, const QString &FileName)
{
    QString result;
    QString S = IncludeTrailingPathDelimiter(TempFolder) + FileName;
    if (!FileExists(S)) return result;
    TStringList St;
    if (!St.LoadFromFile(S)) return result;
    for (int I = 0; I < St.Count(); I++) {
        S = Trim(St[I]);
        QString T;
        for (int J = 0; J < S.length(); J++) {
            const QChar C = S[J];
            if (C.unicode() >= 32 && C.unicode() <= 127 && C != '=' && C != '|') T += C;
        }
        T = Trim(T);
        if (T.length() > 5) { result = T; return result; }
    }
    return result;
}

QString TSelectTemplateForZipImportForm::GetProfileNameFromFileIdDiz(const QString &TempFolder)
{
    QString result;
    TStringList *St = ValueToList(PrgSetup->ArchiveIDFiles());
    for (int I = 0; I < St->Count(); I++) {
        result = GetProfileNameFromFile(TempFolder, (*St)[I]);
        if (!result.isEmpty()) break;
    }
    delete St;
    return result;
}

void TSelectTemplateForZipImportForm::Prepare(bool DoNotCopyFolder, const QString &TempFolder)
{
    int I, Nr;
    QString S;

    JustChanging = true;

    if (DoNotCopyFolder) {
        ui.FolderEdit->setVisible(false);
        ui.FolderEdit_EditLabel->setVisible(false);
        VclLayout::fitHeight(this);
    }

    ui.ProgramFileComboBox->addItems(PrgFiles->ToQStringList());
    if (ui.ProgramFileComboBox->count() > 0) ui.ProgramFileComboBox->setCurrentIndex(0);

    ui.SetupFileComboBox->addItem("");
    ui.SetupFileComboBox->addItems(PrgFiles->ToQStringList());
    ui.SetupFileComboBox->setCurrentIndex(0);

    if (Templates->Count() > 0) {
        ui.TemplateType1RadioButton->setChecked(true);
        ui.ProfileNameEdit->setText(AutoSetupDB->Game(Templates->ObjectAsInt(0))->Name());
        FileToStart = (*Templates)[0];
        TemplateNr = 0;

        SelectFilesFromAutoSetupTemplate();
        I = ui.ProgramFileComboBox->findText(FileToStart, Qt::MatchFixedString); if (I >= 0) ui.ProgramFileComboBox->setCurrentIndex(I);
        I = ui.SetupFileComboBox->findText(SetupFileToStart, Qt::MatchFixedString); if (I >= 0) ui.SetupFileComboBox->setCurrentIndex(I);
    } else {
        ui.TemplateType1RadioButton->setEnabled(false);
        ui.TemplateType3RadioButton->setChecked(true);
        S = GetProfileNameFromFileIdDiz(TempFolder);
        if (!S.isEmpty()) ui.ProfileNameEdit->setText(S); else {
            if (!Trim(ArchivFileName).isEmpty())
                ui.ProfileNameEdit->setText(ChangeFileExt(ExtractFileName(ArchivFileName), ""));
            else
                ui.ProfileNameEdit->setText(AlternateProfileNameSource);
        }

        if (PrgFiles->Count() > 0) {
            if (PrgFiles->Count() == 1) {
                FileToStart = (*PrgFiles)[0];
                SetupFileToStart = "";
            } else {
                Nr = -1;
                for (const QStringList *Level : {&SetupExeFilesLevel1, &SetupExeFilesLevel2, &SetupExeFilesLevel3, &SetupExeFilesLevel4}) {
                    if (Nr >= 0) break;
                    for (I = 0; I < PrgFiles->Count(); I++) {
                        S = ExtUpperCase(ChangeFileExt((*PrgFiles)[I], ""));
                        for (const QString &X : *Level) if (S == X) { Nr = I; break; }
                        if (Nr >= 0) break;
                    }
                }

                FileToStart = AvoidSomeNames(PrgFiles, -1);
                if (Nr < 0) SetupFileToStart = ""; else SetupFileToStart = (*PrgFiles)[Nr];

                I = ui.ProgramFileComboBox->findText(FileToStart, Qt::MatchFixedString);
                if (I >= 0) ui.ProgramFileComboBox->setCurrentIndex(I);
                I = ui.SetupFileComboBox->findText(SetupFileToStart, Qt::MatchFixedString);
                if (I >= 0) ui.SetupFileComboBox->setCurrentIndex(I);
            }
        }
        TemplateNr = -1;
    }
    InitialFolderCheck();
    ui.FolderEdit->setText(ProfileFolder);

    JustChanging = false;

    FolderEditChange(this);
    TypeSelectClick(this);
    ProfileNameChanged = false;
}

void TSelectTemplateForZipImportForm::ProfileNameEditChange(QObject * /*Sender*/)
{
    ProfileNameChanged = true;
}

void TSelectTemplateForZipImportForm::SelectFilesFromAutoSetupTemplate()
{
    QString S;

    FileToStart = (*Templates)[0];
    SetupFileToStart = "";
    S = AutoSetupDB->Game(Templates->ObjectAsInt(0))->SetupExe();
    if (!Trim(S).isEmpty()) {
        S = ExtUpperCase(S);
        for (int I = 0; I < PrgFiles->Count(); I++) if (ExtUpperCase((*PrgFiles)[I]) == S) { SetupFileToStart = (*PrgFiles)[I]; return; }
    }

    TStringList St;
    St.AddStrings(*PrgFiles);
    S = ExtUpperCase(FileToStart);
    for (int I = 0; I < St.Count(); I++) if (ExtUpperCase(St[I]) == S) { St.Delete(I); return; }
    if (St.Count() > 0) SetupFileToStart = St[0];
}

void TSelectTemplateForZipImportForm::InitialFolderCheck()
{
    ProfileFolder = MakeFileSysOKFolderName(ui.ProfileNameEdit->text());
    QString GamesDir = MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir());

    if ((!DirectoryExists(GamesDir + ProfileFolder)) || PrgSetup->IgnoreDirectoryCollisions()) return;
    int I = 0;
    do { I++; } while (DirectoryExists(GamesDir + ProfileFolder + IntToStr(I)));
    ProfileFolder = ProfileFolder + IntToStr(I);
}

void TSelectTemplateForZipImportForm::FolderEditChange(QObject * /*Sender*/)
{
    if (JustChanging) return;
    bool B = DirectoryExists(MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir()) + MakeFileSysOKFolderName(ui.FolderEdit->text()));
    const bool FolderEditVisible = !ui.FolderEdit->isHidden();
    ui.OKButton->setEnabled(!(FolderEditVisible && B));
    ui.WarningLabel->setVisible(FolderEditVisible && B);
}

void TSelectTemplateForZipImportForm::ComboBoxDropDown(QObject *Sender)
{
    if (auto *C = qobject_cast<QComboBox *>(Sender)) SetComboDropDownDropDownWidth(C);
}

void TSelectTemplateForZipImportForm::TypeSelectClick(QObject *Sender)
{
    int I, J, K;
    if (JustChanging) return;

    JustChanging = true;

    if (ui.TemplateType1RadioButton->isChecked()) {
        ui.TemplateComboBox->clear();
        J = 0;
        for (I = 0; I < Templates->Count(); I++) {
            K = Templates->ObjectAsInt(I);
            ui.TemplateComboBox->addItem(AutoSetupDB->Game(K)->CacheName, K);
            if (K == TemplateNr) J = I;
        }
        ui.TemplateComboBox->setCurrentIndex(J);
        ui.TemplateComboBox->setVisible(ui.TemplateComboBox->count() > 1);
        ui.ProgramFileComboBox->setVisible(false);
        ui.SetupFileComboBox->setVisible(false);
    }

    if (ui.TemplateType2RadioButton->isChecked()) {
        ui.TemplateComboBox->clear();
        for (I = 0; I < AutoSetupDB->Count(); I++) if (ui.TemplateComboBox->findText(AutoSetupDB->Game(I)->CacheName, Qt::MatchFixedString) < 0)
            ui.TemplateComboBox->addItem(AutoSetupDB->Game(I)->CacheName, I);
        if (TemplateNr < 0) {
            if (Templates->Count() > 0) TemplateNr = Templates->ObjectAsInt(0); else TemplateNr = 0;
        }
        ui.TemplateComboBox->setCurrentIndex(Min(ui.TemplateComboBox->count() - 1, Max(0, TemplateNr)));
        ui.TemplateComboBox->setVisible(true);
        ui.ProgramFileComboBox->setVisible(ui.ProgramFileComboBox->count() > 1);
        ui.SetupFileComboBox->setVisible(ui.SetupFileComboBox->count() > 2);
    }

    if (ui.TemplateType3RadioButton->isChecked()) {
        ui.TemplateComboBox->clear();
        ui.TemplateComboBox->addItem(LanguageSetup->TemplateFormDefault());
        for (I = 0; I < TemplateDB->Count(); I++) ui.TemplateComboBox->addItem(TemplateDB->Game(I)->CacheName);
        ui.TemplateComboBox->setCurrentIndex(Min(ui.TemplateComboBox->count() - 1, Max(0, -TemplateNr - 1)));
        ui.TemplateComboBox->setVisible(true);
        ui.ProgramFileComboBox->setVisible(ui.ProgramFileComboBox->count() > 1);
        ui.SetupFileComboBox->setVisible(ui.SetupFileComboBox->count() > 2);
    }

    JustChanging = false;

    ui.TemplateLabel->setVisible(!ui.TemplateComboBox->isHidden());
    ui.ProgramFileLabel->setVisible(!ui.ProgramFileComboBox->isHidden());
    ui.SetupFileLabel->setVisible(!ui.SetupFileComboBox->isHidden());

    TemplateComboBoxChange(Sender);
}

void TSelectTemplateForZipImportForm::TemplateComboBoxChange(QObject * /*Sender*/)
{
    if (JustChanging) return;

    if (ui.TemplateType3RadioButton->isChecked()) {
        TemplateNr = -ui.TemplateComboBox->currentIndex() - 1;
    } else {
        TemplateNr = ComboObject(ui.TemplateComboBox, ui.TemplateComboBox->currentIndex());
    }

    if ((!ui.TemplateType3RadioButton->isChecked()) && (!ProfileNameChanged)) {
        ui.ProfileNameEdit->setText(ui.TemplateComboBox->currentText());
        ProfileNameChanged = false;
    }
}

void TSelectTemplateForZipImportForm::OKButtonClick(QObject * /*Sender*/)
{
    ProfileName = ui.ProfileNameEdit->text();
    ProfileFolder = ui.FolderEdit->text();
    if (ui.TemplateType1RadioButton->isChecked()) {
        SelectFilesFromAutoSetupTemplate();
    } else {
        FileToStart = ui.ProgramFileComboBox->currentText();
        SetupFileToStart = ui.SetupFileComboBox->currentText();
    }

    if (ui.TemplateType3RadioButton->isChecked()) {
        TemplateNr = -ui.TemplateComboBox->currentIndex() - 1;
    } else {
        TemplateNr = ComboObject(ui.TemplateComboBox, ui.TemplateComboBox->currentIndex());
    }
}

void TSelectTemplateForZipImportForm::InstallSupportButtonClick(QObject * /*Sender*/)
{
    UseInstallSupport = true;
    setModalResult(mrOk);
}

void TSelectTemplateForZipImportForm::HelpButtonClick(QObject * /*Sender*/)
{
    ApplicationHelpCommand(HELP_CONTEXT, ID_FileImportZip);
}

void TSelectTemplateForZipImportForm::FormKeyDown(QObject *Sender, quint16 &Key, TShiftState Shift)
{
    if ((Key == VK_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

// global

bool ShowSelectTemplateForZipImportDialog(QWidget *AOwner, TGameDB *AAutoSetupDB, TGameDB *ATemplateDB, TStringList *APrgFiles, TStringList *ATemplates, const QString &AArchivFileName, const QString &AAlternateProfileNameSource, const QString &ATempFolder, QString &AProfileName, QString &AFileToStart, QString &ASetupFileToStart, QString &AFolder, int &ATemplateNr, bool NoDialogIfAutoSetupIsAvailable, bool DoNotCopyFolder, bool &UseInstallSupport)
{
    bool result;
    UseInstallSupport = false;
    SelectTemplateForZipImportForm = new TSelectTemplateForZipImportForm(AOwner);
    SelectTemplateForZipImportForm->ArchivFileName = AArchivFileName;
    SelectTemplateForZipImportForm->AlternateProfileNameSource = AAlternateProfileNameSource;
    SelectTemplateForZipImportForm->AutoSetupDB = AAutoSetupDB;
    SelectTemplateForZipImportForm->TemplateDB = ATemplateDB;
    SelectTemplateForZipImportForm->PrgFiles = APrgFiles;
    SelectTemplateForZipImportForm->Templates = ATemplates;
    SelectTemplateForZipImportForm->Prepare(DoNotCopyFolder, ATempFolder);
    if (SelectTemplateForZipImportForm->ui.TemplateType1RadioButton->isChecked() && (NoDialogIfAutoSetupIsAvailable || PrgSetup->ImportZipWithoutDialogIfPossible())) {
        result = true;
        SelectTemplateForZipImportForm->OKButtonClick(SelectTemplateForZipImportForm);
    } else {
        result = (SelectTemplateForZipImportForm->ShowModal() == mrOk);
        UseInstallSupport = SelectTemplateForZipImportForm->UseInstallSupport;
    }
    if (result && !UseInstallSupport) {
        AFileToStart = SelectTemplateForZipImportForm->FileToStart;
        ASetupFileToStart = SelectTemplateForZipImportForm->SetupFileToStart;
        ATemplateNr = SelectTemplateForZipImportForm->TemplateNr;
        AProfileName = SelectTemplateForZipImportForm->ProfileName;
        AFolder = SelectTemplateForZipImportForm->ProfileFolder;
    }
    delete SelectTemplateForZipImportForm;
    SelectTemplateForZipImportForm = nullptr;
    return result;
}
