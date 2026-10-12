// C++ port of BuildInstallerForSingleGameFormUnit.pas (implementation section).
#include "BuildInstallerForSingleGameFormUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "BuildInstallerFormUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"

TBuildInstallerForSingleGameForm* BuildInstallerForSingleGameForm = nullptr;

TBuildInstallerForSingleGameForm::TBuildInstallerForSingleGameForm(QWidget* AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    SaveDialog = new TSaveDialog(this);
    SaveDialog->DefaultExt = "exe";

    connect(ui.DestFileButton, &QToolButton::clicked, this, [this]() { DestFileButtonClick(ui.DestFileButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });

    FormCreate(this);
}

TBuildInstallerForSingleGameForm::~TBuildInstallerForSingleGameForm()
{
    delete SaveDialog;
}

void TBuildInstallerForSingleGameForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TBuildInstallerForSingleGameForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt (Unicode fonts)

    setWindowTitle(LanguageSetup->BuildInstaller());
    ui.DestFileEdit_EditLabel->setText(LanguageSetup->BuildInstallerDestFile());
    ui.DestFileButton->setToolTip(LanguageSetup->ChooseFile());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.InstTypeRadioGroup->setTitle(LanguageSetup->BuildInstallerInstType());
    ui.InstTypeRadioGroup->setItemText(0, LanguageSetup->BuildInstallerInstTypeScriptOnly());
    ui.InstTypeRadioGroup->setItemText(1, LanguageSetup->BuildInstallerInstTypeFullInstaller());
    SaveDialog->Title = LanguageSetup->BuildInstallerDestFileTitle();
    SaveDialog->Filter = LanguageSetup->BuildInstallerDestFileFilter();
    UserIconLoader->DialogImage(DI_SelectFile, ui.DestFileButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
}

void TBuildInstallerForSingleGameForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    setWindowTitle(windowTitle() + " [" + Game->Name() + "]");
    ui.DestFileEdit->setText(ExtractFileName(ChangeFileExt(Game->SetupFile(), "")));
}

void TBuildInstallerForSingleGameForm::DestFileButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    S = Trim(ui.DestFileEdit->text());
    if ((S == "") || (Trim(ExtractFilePath(S)) == "")) SaveDialog->InitialDir = PrgDataDir(); else SaveDialog->InitialDir = Trim(ExtractFilePath(S));
    if (!SaveDialog->Execute()) return;
    ui.DestFileEdit->setText(SaveDialog->FileName);
}

bool TBuildInstallerForSingleGameForm::BuildNSIScript()
{
    QString NSIFileName;

    NSIFileName = Trim(ui.DestFileEdit->text());
    if (NSIFileName == "") {
        MessageDlg(LanguageSetup->MessageNoFileName(), mtError, mbOK, 0);
        return false;
    }
    NSIFileName = ChangeFileExt(NSIFileName, ".nsi");

    if (!CheckPath(ExtractFilePath(NSIFileName))) return false;

    if (FileExists(NSIFileName)) {
        if (MessageDlg(Format(LanguageSetup->MessageConfirmationOverwriteFile(), {NSIFileName}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return false;
    }

    TStringList St;
    St.Add("OutFile \"" + ExtractFileName(ui.DestFileEdit->text()) + "\"");
    St.Add("!include \".\\" + SettingsFolder + "\\" + NSIInstallerHelpFile + "\"");

    if (!AddGameToNSIScript(&St, Game)) return false;

    if (!St.SaveToFile(NSIFileName)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {NSIFileName}), mtError, mbOK, 0);
        return false;
    }

    return true;
}

void TBuildInstallerForSingleGameForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    if ((Trim(ExtractFilePath(ui.DestFileEdit->text())) == "") && (Trim(ui.DestFileEdit->text()) != ""))
        ui.DestFileEdit->setText(PrgDataDir() + ui.DestFileEdit->text());
    if (Trim(ui.DestFileEdit->text()) != "") ui.DestFileEdit->setText(ChangeFileExt(ui.DestFileEdit->text(), ".exe"));

    if (!BuildNSIScript()) { setModalResult(mrNone); return; }
    if (ui.InstTypeRadioGroup->itemIndex() == 1) {
        if (!BuildEXEInstaller(0, ui.DestFileEdit->text())) { setModalResult(mrNone); return; }
    }
}

void TBuildInstallerForSingleGameForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ProfileMakeInstallerPackage);
}

void TBuildInstallerForSingleGameForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

/* global */

bool BuildInstallerForSingleGame(QWidget* AOwner, TGame* AGame)
{
    bool result;
    BuildInstallerForSingleGameForm = new TBuildInstallerForSingleGameForm(AOwner);
    BuildInstallerForSingleGameForm->Game = AGame;
    result = (BuildInstallerForSingleGameForm->ShowModal() == mrOk);
    delete BuildInstallerForSingleGameForm;
    BuildInstallerForSingleGameForm = nullptr;
    return result;
}
