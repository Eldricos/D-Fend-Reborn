// C++ port of SetupFrameUserInterpreterFrameUnit.pas (implementation section).
#include "SetupFrameUserInterpreterFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"
#include "HelpConsts.h"

// ComboBox.Items.Exchange(I1,I2)
static void ComboExchange(QComboBox *Combo, int I1, int I2)
{
    QString S = Combo->itemText(I1);
    Combo->setItemText(I1, Combo->itemText(I2));
    Combo->setItemText(I2, S);
}

TSetupFrameUserInterpreterFrame::TSetupFrameUserInterpreterFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    OpenDialog = new TOpenDialog(this);
    OpenDialog->DefaultExt = "exe";

    // DFM event wiring
    connect(ui.AddButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.AddButton); });
    connect(ui.DelButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DelButton); });
    connect(ui.UpButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.UpButton); });
    connect(ui.DownButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DownButton); });
    connect(ui.ProgramButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.ProgramButton); });
    connect(ui.ComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int) { ComboBoxChange(ui.ComboBox); });
    connect(ui.ProgramEdit, &QLineEdit::textChanged, this, [this]() { ProgramEditChange(ui.ProgramEdit); });
}

TSetupFrameUserInterpreterFrame::~TSetupFrameUserInterpreterFrame()
{
    delete Programs;
    delete Extensions;
    delete Parameters;
    delete OpenDialog;
}

/* TSetupFrameUserInterpreterFrame */

QString TSetupFrameUserInterpreterFrame::GetName()
{
    return LanguageSetup->SetupFormUserDefinedInterpreters();
}

void TSetupFrameUserInterpreterFrame::InitGUIAndLoadSetup(TInitData& InitData)
{
    Programs = new TStringList;
    Extensions = new TStringList;
    Parameters = new TStringList;

    Programs->Assign(*PrgSetup->DOSBoxBasedUserInterpretersPrograms());
    Extensions->Assign(*PrgSetup->DOSBoxBasedUserInterpretersExtensions());
    Parameters->Assign(*PrgSetup->DOSBoxBasedUserInterpretersParameters());

    PBaseDir = InitData.PBaseDir;

    // NoFlicker(...): not needed in Qt

    UserIconLoader->DialogImage(DI_Add, ui.AddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.DelButton);
    UserIconLoader->DialogImage(DI_Up, ui.UpButton);
    UserIconLoader->DialogImage(DI_Down, ui.DownButton);
    UserIconLoader->DialogImage(DI_SelectFile, ui.ProgramButton);

    JustChanging = false;
    LoadList();
    if (ui.ComboBox->count() > 0) ui.ComboBox->setCurrentIndex(0);
    ComboBoxChange(this);

    setProperty("HelpContext", ID_FileOptionsUserInterpreters);
}

void TSetupFrameUserInterpreterFrame::LoadLanguage()
{
    ui.InterpreterLabel->setText(LanguageSetup->SetupFormUserDefinedInterpretersLabel());

    ui.AddButton->setToolTip(LanguageSetup->SetupFormUserDefinedInterpretersAdd());
    ui.DelButton->setToolTip(LanguageSetup->SetupFormUserDefinedInterpretersDelete());
    ui.UpButton->setToolTip(LanguageSetup->MoveUp());
    ui.DownButton->setToolTip(LanguageSetup->MoveDown());
    ui.ProgramButton->setToolTip(LanguageSetup->ChooseFile());
    ui.ProgramEdit_EditLabel->setText(LanguageSetup->SetupFormUserDefinedInterpretersProgram());
    ui.ParametersEdit_EditLabel->setText(LanguageSetup->SetupFormUserDefinedInterpretersParameters());
    ui.ExtensionsEdit_EditLabel->setText(LanguageSetup->SetupFormUserDefinedInterpretersExtensions());
    ui.InfoLabel->setText(LanguageSetup->SetupFormUserDefinedInterpretersInfo());
}

void TSetupFrameUserInterpreterFrame::BeforeChangeLanguage()
{
}

void TSetupFrameUserInterpreterFrame::DOSBoxDirChanged()
{
}

void TSetupFrameUserInterpreterFrame::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameUserInterpreterFrame::HideFrame()
{
}

void TSetupFrameUserInterpreterFrame::RestoreDefaults()
{
}

void TSetupFrameUserInterpreterFrame::SaveSetup()
{
    ComboBoxChange(this);
    PrgSetup->DOSBoxBasedUserInterpretersPrograms()->Assign(*Programs);
    PrgSetup->DOSBoxBasedUserInterpretersParameters()->Assign(*Parameters);
    PrgSetup->DOSBoxBasedUserInterpretersExtensions()->Assign(*Extensions);
}

void TSetupFrameUserInterpreterFrame::LoadList()
{
    int I;

    LastItem = -1;
    ui.ComboBox->setUpdatesEnabled(false);
    ui.ComboBox->clear();
    for (I = 0; I < Programs->Count(); I++) ui.ComboBox->addItem(ExtractFileName((*Programs)[I]));
    ui.ComboBox->setUpdatesEnabled(true);
}

void TSetupFrameUserInterpreterFrame::ProgramEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;

    if (JustChanging) return;
    I = ui.ComboBox->currentIndex();
    if (I < 0) return;   // (VCL: assignment to Items[-1] raises an exception)
    ui.ComboBox->setItemText(I, ExtractFileName(Trim(ui.ProgramEdit->text())));
    ui.ComboBox->setCurrentIndex(I);
}

void TSetupFrameUserInterpreterFrame::ComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (LastItem >= 0) {
        (*Programs)[LastItem] = Trim(ui.ProgramEdit->text());
        (*Parameters)[LastItem] = Trim(ui.ParametersEdit->text());
        (*Extensions)[LastItem] = Trim(ui.ExtensionsEdit->text());
        ui.ComboBox->setItemText(LastItem, ExtractFileName(Trim(ui.ProgramEdit->text())));
    }

    LastItem = ui.ComboBox->currentIndex();

    ui.DelButton->setEnabled(LastItem >= 0);
    ui.UpButton->setEnabled(LastItem >= 1);
    ui.DownButton->setEnabled((LastItem >= 0) && (LastItem < ui.ComboBox->count() - 1));

    ui.ProgramEdit->setEnabled(LastItem >= 0);
    ui.ParametersEdit->setEnabled(LastItem >= 0);
    ui.ExtensionsEdit->setEnabled(LastItem >= 0);
    ui.ProgramButton->setEnabled(LastItem >= 0);

    JustChanging = true;
    if (LastItem < 0) {
        ui.ProgramEdit->setText("");
        ui.ParametersEdit->setText("");
        ui.ExtensionsEdit->setText("");
        JustChanging = false;
        return;
    }

    ui.ProgramEdit->setText((*Programs)[LastItem]);
    ui.ParametersEdit->setText((*Parameters)[LastItem]);
    ui.ExtensionsEdit->setText((*Extensions)[LastItem]);
    JustChanging = false;
}

void TSetupFrameUserInterpreterFrame::ButtonWork(QObject* Sender)
{
    QString S;
    int I;

    switch (Sender->property("tag").toInt()) {
        case 0:
            Programs->Add("");
            Parameters->Add("%s");
            Extensions->Add("");
            ui.ComboBox->setCurrentIndex(-1); ComboBoxChange(Sender);
            LoadList();
            ui.ComboBox->setCurrentIndex(ui.ComboBox->count() - 1); ComboBoxChange(Sender);
            break;
        case 1:
            I = LastItem;
            if (I < 0) return;
            if (MessageDlg(Format(LanguageSetup->SetupFormUserDefinedInterpretersDeleteWarning(), {ui.ComboBox->itemText(I)}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
            Programs->Delete(I);
            Parameters->Delete(I);
            Extensions->Delete(I);
            LoadList();
            if (ui.ComboBox->count() > 0) ui.ComboBox->setCurrentIndex(qMax(0, I - 1));
            ComboBoxChange(Sender);
            break;
        case 2:
            ComboExchange(ui.ComboBox, LastItem, LastItem - 1);
            Programs->Exchange(LastItem, LastItem - 1);
            Parameters->Exchange(LastItem, LastItem - 1);
            Extensions->Exchange(LastItem, LastItem - 1);
            LastItem--;
            ui.ComboBox->setCurrentIndex(LastItem);
            ui.UpButton->setEnabled(LastItem >= 1);
            ui.DownButton->setEnabled((LastItem >= 0) && (LastItem < ui.ComboBox->count() - 1));
            break;
        case 3:
            ComboExchange(ui.ComboBox, LastItem, LastItem + 1);
            // Delphi exchanges LastItem/LastItem-1 here (copy & paste bug); fixed to move the same entry as the combo box item
            Programs->Exchange(LastItem, LastItem + 1);
            Parameters->Exchange(LastItem, LastItem + 1);
            Extensions->Exchange(LastItem, LastItem + 1);
            LastItem++;
            ui.ComboBox->setCurrentIndex(LastItem);
            ui.UpButton->setEnabled(LastItem >= 1);
            ui.DownButton->setEnabled((LastItem >= 0) && (LastItem < ui.ComboBox->count() - 1));
            break;
        case 4:
            S = MakeAbsPath(ui.ProgramEdit->text(), PrgSetup->BaseDir()); if (S != "") S = ExtractFilePath(S);
            /* If S='' then S:=GetSpecialFolder(Application.MainForm.Handle,CSIDL_PROGRAM_FILES); */
            if (Trim(S) == "") S = IncludeTrailingPathDelimiter(*PBaseDir);
            S = MakeAbsPath(S, IncludeTrailingPathDelimiter(*PBaseDir));
            OpenDialog->Title = LanguageSetup->SetupFormUserDefinedInterpretersProgramTitle();
            OpenDialog->Filter = LanguageSetup->SetupFormUserDefinedInterpretersProgramFilter();
            if (S != "") OpenDialog->InitialDir = S;
            if (OpenDialog->Execute()) ui.ProgramEdit->setText(MakeRelPath(OpenDialog->FileName, PrgSetup->BaseDir()));
            break;
    }
}
