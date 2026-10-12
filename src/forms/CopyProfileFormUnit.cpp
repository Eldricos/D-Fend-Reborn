// C++ port of CopyProfileFormUnit.pas (implementation section).
#include "CopyProfileFormUnit.h"
#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "Process.h"

TCopyProfileForm* CopyProfileForm = nullptr;

// Application.HelpCommand(HELP_CONTEXT, ID)
static void ApplicationHelpContext(int ID)
{
    // TODO(port): Application.HelpCommand is routed through HelpTools (THTMLhelpRouter.OnRouteHelp)
    // in the original; there is no TApplication help hook in Qt.  Call HtmlHelp directly if
    // available, otherwise open the help file with the associated viewer.
    if (GLOBAL_HELPROUTER == nullptr) return;
    const QString HFile = ChangeFileExt(GLOBAL_HELPROUTER->Helpfile(), ".chm");
    if (HtmlHelpA && HtmlHelpA(0, HFile, HH_HELP_CONTEXT, ID) != 0) return;
    if (FileExists(HFile)) ShellOpen(HFile);
}

TCopyProfileForm::TCopyProfileForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    // OnKeyDown with KeyPreview=True
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key; FormKeyDown(this, K, Mods); return K == 0;
    });
    FormCreate(this);   // OnCreate
}

TCopyProfileForm::~TCopyProfileForm() = default;

void TCopyProfileForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    setWindowTitle(LanguageSetup->MenuProfileCopyTitle());
    ui.NameEdit_EditLabel->setText(LanguageSetup->MenuProfileCopyPrompt());
    ui.UpdateScreenshotFolderCheckBox->setText(LanguageSetup->MenuProfileCopyUpdateScreenshotFolder());
    ui.RemoveDataFolderRecordCheckBox->setText(LanguageSetup->MenuProfileCopyRemoveDataFolderRecord());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
}

void TCopyProfileForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpContext(ID_ProfileCopy);
}

void TCopyProfileForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if (Key == Qt::Key_F1 && Shift == Qt::NoModifier) HelpButtonClick(Sender);
}

// { global }

bool ShowCopyProfileDialog(QWidget* AOwner, bool ADataFolderAvailable, QString& ANewName, bool& AUpdateScreenshotFolder, bool& ARemoveDataFolderRecord)
{
    CopyProfileForm = new TCopyProfileForm(AOwner);
    CopyProfileForm->ui.NameEdit->setText(ANewName);
    CopyProfileForm->ui.RemoveDataFolderRecordCheckBox->setEnabled(ADataFolderAvailable);
    const bool result = (CopyProfileForm->ShowModal() == mrOk);
    if (result) {
        ANewName = CopyProfileForm->ui.NameEdit->text();
        AUpdateScreenshotFolder = CopyProfileForm->ui.UpdateScreenshotFolderCheckBox->isChecked();
        ARemoveDataFolderRecord = ADataFolderAvailable && CopyProfileForm->ui.RemoveDataFolderRecordCheckBox->isChecked();
    }
    delete CopyProfileForm;
    CopyProfileForm = nullptr;
    return result;
}
