// C++ port of CreateShortcutFormUnit.pas (implementation section).
#include "CreateShortcutFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "GameDBToolsUnit.h"
#include "DosBoxUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "Process.h"

TCreateShortcutForm* CreateShortcutForm = nullptr;

TCreateShortcutForm::TCreateShortcutForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.StartmenuEdit, &QLineEdit::textChanged, this, [this]() { StartmenuEditChange(ui.StartmenuEdit); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);  // OnCreate
}

TCreateShortcutForm::~TCreateShortcutForm() = default;

void TCreateShortcutForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TCreateShortcutForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // DoubleBuffered:=True; SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    setWindowTitle(LanguageSetup->CreateShortcutForm());
    ui.LinkNameEdit_EditLabel->setText(LanguageSetup->CreateShortcutFormLinkName());
    ui.LinkCommentEdit_EditLabel->setText(LanguageSetup->CreateShortcutFormLinkComment());
    ui.DesktopRadioButton->setText(LanguageSetup->CreateShortcutFormDesktop());
    ui.StartmenuRadioButton->setText(LanguageSetup->CreateShortcutFormStartmenu());
    ui.UseProfileIconCheckBox->setText(LanguageSetup->CreateShortcutFormUseProfileIcon());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
}

void TCreateShortcutForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.UseProfileIconCheckBox->setEnabled((GameIcon != "") && FileExists(MakeAbsIconName(GameIcon)));
    ui.UseProfileIconCheckBox->setChecked(ui.UseProfileIconCheckBox->isEnabled());

    ui.LinkNameEdit->setText(GameName);
    if (GameScummVM) {
        ui.LinkCommentEdit->setText(Format(LanguageSetup->CreateShortcutFormRunGameInScummVM(), {GameName}));
    } else {
        if (GameWindowsExe)
            ui.LinkCommentEdit->setText(Format(LanguageSetup->CreateShortcutFormRunGame(), {GameName}));
        else
            ui.LinkCommentEdit->setText(Format(LanguageSetup->CreateShortcutFormRunGameInDOSBox(), {GameName}));
    }
}

void TCreateShortcutForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString Dir, Icon, FileName;

    if (ui.UseProfileIconCheckBox->isChecked()) Icon = MakeAbsIconName(GameIcon); else Icon = "";

    if (ui.DesktopRadioButton->isChecked()) {
        Dir = IncludeTrailingPathDelimiter(GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY));
    } else {
        Dir = IncludeTrailingPathDelimiter(GetSpecialFolder(winId(), CSIDL_PROGRAMS));
        Dir = Dir + ui.StartmenuEdit->text() + PathDelim;
        if (!ForceDirectories(Dir)) {
            MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {Dir}), mtError, mbOK, 0, this);
            return;
        }
    }

    // CreateLink (CommonTools) creates a Windows shell link; on other systems it
    // is expected to create the platform equivalent for the given file name.
    FileName = Dir + MakeFileSysOKFolderName(GameFile) + ".lnk";

    if (GameWindowsExe) {
        CreateLink(GameExeFile, WinGameParameters, FileName, Icon, ui.LinkCommentEdit->text());
    } else {
        CreateLink(ExpandFileName(QCoreApplication::applicationFilePath()), GameName, FileName, Icon, ui.LinkCommentEdit->text());
    }
}

void TCreateShortcutForm::StartmenuEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.StartmenuRadioButton->setChecked(true);
}

void TCreateShortcutForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ProfileCreateShortCut);
}

void TCreateShortcutForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

/* global */

bool ShowCreateShortcutDialog(QWidget* AOwner, const QString& AGameName, const QString& AGameFileName, const QString& AGameIconFile, const QString& AGammExeName, const QString& AWinGameParameters, bool AGameScummVM, bool AGameWindowsExe)
{
    bool result;
    CreateShortcutForm = new TCreateShortcutForm(AOwner);
    CreateShortcutForm->GameName = AGameName;
    CreateShortcutForm->GameFile = ChangeFileExt(ExtractFileName(AGameFileName), "");
    if (AGameWindowsExe) CreateShortcutForm->GameExeFile = MakeAbsPath(AGammExeName, PrgSetup->BaseDir());
    CreateShortcutForm->GameIcon = AGameIconFile;
    CreateShortcutForm->GameScummVM = AGameScummVM;
    CreateShortcutForm->GameWindowsExe = AGameWindowsExe;
    CreateShortcutForm->WinGameParameters = AWinGameParameters;
    result = (CreateShortcutForm->ShowModal() == mrOk);
    delete CreateShortcutForm;
    CreateShortcutForm = nullptr;
    return result;
}

bool CreateLinuxShortCut(QWidget* AOwner, TGame* Game)
{
    bool result = false;
    TStringList *St2;
    QString ConfFile, S;
    int DOSBoxNr;

    if (ScummVMMode(Game) || WindowsExeMode(Game)) {
        MessageDlg(LanguageSetup->CreateShortcutFormNoLinuxScummVMLinks(), mtError, mbOK, 0, AOwner);
        return result;
    }

    TSaveDialog SaveDialog(AOwner);
    SaveDialog.Filter = LanguageSetup->CreateShortcutFormFilter();
    SaveDialog.Title = LanguageSetup->CreateShortcutForm();
    if (!SaveDialog.Execute()) return result;

    ConfFile = IncludeTrailingPathDelimiter(ExtractFilePath(SaveDialog.FileName)) + ExtractFileName(MakeAbsPath(ChangeFileExt(Game->SetupFile(), ".conf"), PrgSetup->BaseDir()));
    TStringList St;
    St2 = BuildConfFile(Game, false, false, -1, nullptr, false);
    if (St2 == nullptr) return result;
    if (!St2->SaveToFile(ConfFile)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {ConfFile}), mtError, mbOK, 0, AOwner);
        delete St2;
        return result;
    }
    delete St2;
    if (Trim(PrgSetup->LinuxShellScriptPreamble()) != "") St.Add(PrgSetup->LinuxShellScriptPreamble());

    DOSBoxNr = GetDOSBoxNr(Game);
    if (DOSBoxNr >= 0) S = MakeAbsPath(PrgSetup->DOSBoxSettings(DOSBoxNr)->DosBoxDir(), PrgSetup->BaseDir()); else S = Trim(Game->CustomDOSBoxDir());
    St.Add(UnmapDrive(IncludeTrailingPathDelimiter(S) + DosBoxFileName, ptDOSBox) + " -conf \"" + UnmapDrive(ConfFile, ptDOSBox) + "\"");
    if (!St.SaveToFile(SaveDialog.FileName)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {SaveDialog.FileName}), mtError, mbOK, 0, AOwner);
    }
    return result;
}
