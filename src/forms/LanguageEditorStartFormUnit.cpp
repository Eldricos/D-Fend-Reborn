// C++ port of LanguageEditorStartFormUnit.pas (implementation section).
#include "LanguageEditorStartFormUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "CommonTools.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "Process.h"

TLanguageEditorStartForm* LanguageEditorStartForm = nullptr;

TLanguageEditorStartForm::TLanguageEditorStartForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.LanguageNameEdit, &QLineEdit::textChanged, this, [this]() { LanguageNameEditChange(ui.LanguageNameEdit); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);  // OnCreate
}

TLanguageEditorStartForm::~TLanguageEditorStartForm() = default;

void TLanguageEditorStartForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=CharsetNameToFontCharSet(...) - not needed with Qt (Unicode fonts)

    setWindowTitle(LanguageSetup->ProfileEditor());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

    setWindowTitle(LanguageSetup->LanguageEditorCaption());
    ui.EditThisRadioButton->setText(LanguageSetup->LanguageEditorEditThisLanguage());
    ui.EditNewRadioButton->setText(LanguageSetup->LanguageEditorEditNewLanguage());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
}

void TLanguageEditorStartForm::LanguageNameEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.EditNewRadioButton->setChecked(true);
}

void TLanguageEditorStartForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ExtrasLanguageEditor);
}

void TLanguageEditorStartForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

/* global */

bool ShowLanguageEditorStartDialog(QWidget* AOwner, QString& LanguageFile, bool NewOnly)
{
    bool result;
    QString DataLanguageFile;

    LanguageFile = "";

    LanguageEditorStartForm = new TLanguageEditorStartForm(AOwner);
    if (NewOnly) {
        LanguageEditorStartForm->ui.EditThisRadioButton->setEnabled(false);
        LanguageEditorStartForm->ui.EditNewRadioButton->setChecked(true);
    }
    result = (LanguageEditorStartForm->ShowModal() == mrOk);
    if (result) {
        if (LanguageEditorStartForm->ui.EditThisRadioButton->isChecked()) {
            LanguageFile = LanguageSetup->SetupFile();
        } else {
            ForceDirectories(PrgDataDir() + LanguageSubDir);
            LanguageFile = PrgDataDir() + LanguageSubDir + PathDelim + LanguageEditorStartForm->ui.LanguageNameEdit->text() + ".ini";
        }
        if ((PrgDir() != PrgDataDir()) && (ExtUpperCase(PrgDir()) == ExtUpperCase(Copy(LanguageFile, 1, Length(PrgDir()))))) {
            DataLanguageFile = PrgDataDir() + LanguageSubDir + PathDelim + ExtractFileName(LanguageFile);
            if (!FileExists(DataLanguageFile)) {
                ForceDirectories(ExtractFilePath(DataLanguageFile));
                CopyFile(LanguageFile, DataLanguageFile, true);
            }
            LoadLanguage(ExtractFileName(DataLanguageFile));
            LanguageFile = DataLanguageFile;
        }
    }
    delete LanguageEditorStartForm;
    LanguageEditorStartForm = nullptr;
    return result;
}
