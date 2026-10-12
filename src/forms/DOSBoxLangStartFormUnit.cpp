// C++ port of DOSBoxLangStartFormUnit.pas (implementation section).
#include "DOSBoxLangStartFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include "PrgSetupUnit.h"
#include "DOSBoxLangTools.h"
#include "PrgConsts.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "Process.h"

TDOSBoxLangStartForm* DOSBoxLangStartForm = nullptr;

TDOSBoxLangStartForm::TDOSBoxLangStartForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.NewFileComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { NewFileEditChange(ui.NewFileComboBox); });
    connect(ui.EditFileComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { EditFileComboBoxChange(ui.EditFileComboBox); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.NewFileEdit, &QLineEdit::textChanged, this, [this]() { NewFileEditChange(ui.NewFileEdit); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);  // OnCreate
}

TDOSBoxLangStartForm::~TDOSBoxLangStartForm()
{
    FormDestroy(this);  // OnDestroy
}

void TDOSBoxLangStartForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TDOSBoxLangStartForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    LoadFileName = "";
    SaveFileName = "";
}

void TDOSBoxLangStartForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    int I, J;
    bool B;

    // SetVistaFonts(self); Font.Charset:=CharsetNameToFontCharSet(...) - not needed with Qt (Unicode fonts)

    setWindowTitle(LanguageSetup->DOSBoxLangEditor());
    ui.NewFileRadioButton->setText(LanguageSetup->DOSBoxLangEditorStartNew());
    ui.NewFileEdit_EditLabel->setText(LanguageSetup->DOSBoxLangEditorStartNewName());
    ui.NewFileLabel->setText(LanguageSetup->DOSBoxLangEditorStartNewTemplate());
    ui.EditFileRadioButton->setText(LanguageSetup->DOSBoxLangEditorStartEdit());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);

    delete Names; delete NamesWithEnglish; delete Files;
    Names = new TStringList;
    NamesWithEnglish = new TStringList;
    Files = new TStringList;
    GetDOSBoxLangNamesAndFiles(PrgSetup->DOSBoxSettings(0)->DosBoxDir(), Names, Files, false);

    for (I = 0; I < Names->Count(); I++) Names->SetObjects(I, reinterpret_cast<void*>(intptr_t(I)));
    NamesWithEnglish->AddStrings(*Names);
    B = false;
    for (I = 0; I < NamesWithEnglish->Count(); I++) if (ExtUpperCase((*NamesWithEnglish)[I]) == "ENGLISH") { B = true; break; }
    if (!B) NamesWithEnglish->AddObject("English", intptr_t(-1));

    Names->Sort();
    NamesWithEnglish->Sort();

    // Changing the combo boxes must not trigger the OnChange handlers (VCL does not fire OnChange for programmatic changes)
    const QSignalBlocker Block1(ui.NewFileComboBox);
    const QSignalBlocker Block2(ui.EditFileComboBox);

    ui.NewFileComboBox->addItems(NamesWithEnglish->ToQStringList());
    ui.EditFileComboBox->addItems(Names->ToQStringList());

    if (ui.NewFileComboBox->count() == 0) {
        MessageDlg(LanguageSetup->DOSBoxLangEditorStartNoFiles(), mtError, mbOK, 0, this);
        QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);   // PostMessage(Handle,WM_CLOSE,0,0)
        return;
    }

    S = ExtUpperCase(ExtractFileName(PrgSetup->DOSBoxSettings(0)->DosBoxLanguage()));
    J = -1; for (I = 0; I < Names->Count(); I++) if (ExtUpperCase(ExtractFileName((*Files)[I])) == S) { J = I; break; }
    if (J >= 0) {
        for (I = 0; I < Names->Count(); I++) if (Names->ObjectAsInt(I) == J) { ui.EditFileComboBox->setCurrentIndex(I); break; }
        for (I = 0; I < NamesWithEnglish->Count(); I++) if (NamesWithEnglish->ObjectAsInt(I) == J) { ui.NewFileComboBox->setCurrentIndex(I); break; }
    } else {
        for (I = 0; I < NamesWithEnglish->Count(); I++) if (NamesWithEnglish->ObjectAsInt(I) == -1) { ui.NewFileComboBox->setCurrentIndex(I); break; }
        ui.EditFileComboBox->setCurrentIndex(0);
    }
}

void TDOSBoxLangStartForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    int I;

    if (ui.NewFileRadioButton->isChecked()) {
        I = NamesWithEnglish->ObjectAsInt(ui.NewFileComboBox->currentIndex());
        if (I >= 0) LoadFileName = (*Files)[I]; else LoadFileName = "";
        S = RemoveIllegalFileNameChars(Trim(ui.NewFileEdit->text()));
        if (S == "") {
            MessageDlg(LanguageSetup->DOSBoxLangEditorStartNoName(), mtError, mbOK, 0, this);
            setModalResult(mrNone);
            return;
        }
        SaveFileName = PrgDataDir() + LanguageSubDir + PathDelim + S + ".lng";
    } else {
        // Note: the original code uses NewFileComboBox here, too.
        I = NamesWithEnglish->ObjectAsInt(ui.NewFileComboBox->currentIndex());
        LoadFileName = (*Files)[I];
        if ((OperationMode == omUserDir) && (Copy(ExtUpperCase(LoadFileName), 1, Length(PrgDataDir())) != ExtUpperCase(PrgDataDir()))) {
            SaveFileName = PrgDataDir() + LanguageSubDir + PathDelim + ExtractFileName(LoadFileName);
        } else {
            SaveFileName = LoadFileName;
        }
    }
}

void TDOSBoxLangStartForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    delete Names; Names = nullptr;
    delete NamesWithEnglish; NamesWithEnglish = nullptr;
    delete Files; Files = nullptr;
}

void TDOSBoxLangStartForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ExtrasDOSBoxLanguageEditor);
}

void TDOSBoxLangStartForm::NewFileEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.NewFileRadioButton->setChecked(true);
}

void TDOSBoxLangStartForm::EditFileComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.EditFileRadioButton->setChecked(true);
}

void TDOSBoxLangStartForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

/* global */

bool ShowDOSBoxLangStartDialog(QWidget* AOwner, QString& LoadFileName, QString& SaveFileName)
{
    bool result;
    TDOSBoxLangStartForm* DOSBoxLangStartForm = new TDOSBoxLangStartForm(AOwner);
    result = (DOSBoxLangStartForm->ShowModal() == mrOk);
    if (result) {
        LoadFileName = DOSBoxLangStartForm->LoadFileName;
        SaveFileName = DOSBoxLangStartForm->SaveFileName;
    }
    delete DOSBoxLangStartForm;
    return result;
}
