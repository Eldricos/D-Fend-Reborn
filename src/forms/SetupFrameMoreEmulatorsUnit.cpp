// C++ port of SetupFrameMoreEmulatorsUnit.pas (implementation section).
#include "SetupFrameMoreEmulatorsUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"
#include "HelpConsts.h"

static constexpr int CSIDL_PROGRAM_FILES_ = 0x0026;   // ShlObj.CSIDL_PROGRAM_FILES

// ComboBox.Items.Exchange(I1,I2)
static void ComboExchange(QComboBox *Combo, int I1, int I2)
{
    QString S = Combo->itemText(I1);
    Combo->setItemText(I1, Combo->itemText(I2));
    Combo->setItemText(I2, S);
}

TSetupFrameMoreEmulators::TSetupFrameMoreEmulators(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    OpenDialog = new TOpenDialog(this);
    OpenDialog->DefaultExt = "exe";

    // DFM event wiring
    connect(ui.ProgramButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.ProgramButton); });
    connect(ui.DownButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DownButton); });
    connect(ui.UpButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.UpButton); });
    connect(ui.DelButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DelButton); });
    connect(ui.AddButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.AddButton); });
    connect(ui.ComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int) { ComboBoxChange(ui.ComboBox); });
    connect(ui.NameEdit, &QLineEdit::textChanged, this, [this]() { NameEditChange(ui.NameEdit); });
}

TSetupFrameMoreEmulators::~TSetupFrameMoreEmulators()
{
    delete Names;
    delete Programs;
    delete Extensions;
    delete Parameters;
    delete OpenDialog;
}

/* TSetupFrameMoreEmulators */

QString TSetupFrameMoreEmulators::GetName()
{
    return LanguageSetup->SetupFormMoreEmulators();
}

void TSetupFrameMoreEmulators::InitGUIAndLoadSetup(TInitData& InitData)
{
    Q_UNUSED(InitData);
    Names = new TStringList;
    Programs = new TStringList;
    Extensions = new TStringList;
    Parameters = new TStringList;

    Names->Assign(*PrgSetup->WindowsBasedEmulatorsNames());
    Programs->Assign(*PrgSetup->WindowsBasedEmulatorsPrograms());
    Extensions->Assign(*PrgSetup->WindowsBasedEmulatorsExtensions());
    Parameters->Assign(*PrgSetup->WindowsBasedEmulatorsParameters());

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

    setProperty("HelpContext", ID_FileOptionsMoreEmulator);
}

void TSetupFrameMoreEmulators::LoadLanguage()
{
    ui.InterpreterLabel->setText(LanguageSetup->SetupFormMoreEmulators());

    ui.AddButton->setToolTip(LanguageSetup->SetupFormMoreEmulatorsAdd());
    ui.DelButton->setToolTip(LanguageSetup->SetupFormMoreEmulatorsDelete());
    ui.UpButton->setToolTip(LanguageSetup->MoveUp());
    ui.DownButton->setToolTip(LanguageSetup->MoveDown());
    ui.ProgramButton->setToolTip(LanguageSetup->ChooseFile());
    ui.NameEdit_EditLabel->setText(LanguageSetup->SetupFormMoreEmulatorsName());
    ui.ProgramEdit_EditLabel->setText(LanguageSetup->SetupFormMoreEmulatorsProgram());
    ui.ParametersEdit_EditLabel->setText(LanguageSetup->SetupFormMoreEmulatorsParameters());
    ui.ExtensionsEdit_EditLabel->setText(LanguageSetup->SetupFormMoreEmulatorsExtensions());
    ui.InfoLabel->setText(LanguageSetup->SetupFormMoreEmulatorsInfo());
}

void TSetupFrameMoreEmulators::BeforeChangeLanguage()
{
}

void TSetupFrameMoreEmulators::DOSBoxDirChanged()
{
}

void TSetupFrameMoreEmulators::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameMoreEmulators::HideFrame()
{
}

void TSetupFrameMoreEmulators::RestoreDefaults()
{
}

void TSetupFrameMoreEmulators::SaveSetup()
{
    ComboBoxChange(this);
    PrgSetup->WindowsBasedEmulatorsNames()->Assign(*Names);
    PrgSetup->WindowsBasedEmulatorsPrograms()->Assign(*Programs);
    PrgSetup->WindowsBasedEmulatorsParameters()->Assign(*Parameters);
    PrgSetup->WindowsBasedEmulatorsExtensions()->Assign(*Extensions);
}

void TSetupFrameMoreEmulators::LoadList()
{
    int I;

    LastItem = -1;
    ui.ComboBox->setUpdatesEnabled(false);
    ui.ComboBox->clear();
    for (I = 0; I < Names->Count(); I++) ui.ComboBox->addItem((*Names)[I]);
    ui.ComboBox->setUpdatesEnabled(true);
}

void TSetupFrameMoreEmulators::NameEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;

    if (JustChanging) return;
    I = ui.ComboBox->currentIndex();
    if (I < 0) return;   // (VCL: assignment to Items[-1] raises an exception)
    ui.ComboBox->setItemText(I, ExtractFileName(Trim(ui.NameEdit->text())));
    ui.ComboBox->setCurrentIndex(I);
}

void TSetupFrameMoreEmulators::ComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (LastItem >= 0) {
        if (Trim(ui.NameEdit->text()) != "")
            (*Names)[LastItem] = Trim(ui.NameEdit->text());
        else
            (*Names)[LastItem] = Trim(ChangeFileExt(ExtractFileName(ui.ProgramEdit->text()), ""));
        ui.ComboBox->setItemText(LastItem, (*Names)[LastItem]);
        (*Programs)[LastItem] = Trim(ui.ProgramEdit->text());
        (*Parameters)[LastItem] = Trim(ui.ParametersEdit->text());
        (*Extensions)[LastItem] = Trim(ui.ExtensionsEdit->text());
    }

    LastItem = ui.ComboBox->currentIndex();

    ui.DelButton->setEnabled(LastItem >= 0);
    ui.UpButton->setEnabled(LastItem >= 1);
    ui.DownButton->setEnabled((LastItem >= 0) && (LastItem < ui.ComboBox->count() - 1));

    ui.NameEdit->setEnabled(LastItem >= 0);
    ui.ProgramEdit->setEnabled(LastItem >= 0);
    ui.ParametersEdit->setEnabled(LastItem >= 0);
    ui.ExtensionsEdit->setEnabled(LastItem >= 0);
    ui.ProgramButton->setEnabled(LastItem >= 0);

    JustChanging = true;
    if (LastItem < 0) {
        ui.NameEdit->setText("");
        ui.ProgramEdit->setText("");
        ui.ParametersEdit->setText("");
        ui.ExtensionsEdit->setText("");
        JustChanging = false;
        return;
    }

    ui.NameEdit->setText((*Names)[LastItem]);
    ui.ProgramEdit->setText((*Programs)[LastItem]);
    ui.ParametersEdit->setText((*Parameters)[LastItem]);
    ui.ExtensionsEdit->setText((*Extensions)[LastItem]);
    JustChanging = false;
}

void TSetupFrameMoreEmulators::ButtonWork(QObject* Sender)
{
    QString S;
    int I;

    switch (Sender->property("tag").toInt()) {
        case 0:
            Names->Add("");
            Programs->Add("");
            Parameters->Add("\"%s\"");
            Extensions->Add("");
            ui.ComboBox->setCurrentIndex(-1); ComboBoxChange(Sender);
            LoadList();
            ui.ComboBox->setCurrentIndex(ui.ComboBox->count() - 1); ComboBoxChange(Sender);
            break;
        case 1:
            I = LastItem;
            if (I < 0) return;
            if (MessageDlg(Format(LanguageSetup->SetupFormMoreEmulatorsDeleteWarning(), {ui.ComboBox->itemText(I)}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
            Names->Delete(I);
            Programs->Delete(I);
            Parameters->Delete(I);
            Extensions->Delete(I);
            LoadList();
            if (ui.ComboBox->count() > 0) ui.ComboBox->setCurrentIndex(qMax(0, I - 1));
            ComboBoxChange(Sender);
            break;
        case 2:
            ComboExchange(ui.ComboBox, LastItem, LastItem - 1);
            Names->Exchange(LastItem, LastItem - 1);
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
            Names->Exchange(LastItem, LastItem + 1);
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
            if (S == "") S = GetSpecialFolder(0, CSIDL_PROGRAM_FILES_);
            OpenDialog->Title = LanguageSetup->SetupFormMoreEmulatorsProgramTitle();
            OpenDialog->Filter = LanguageSetup->SetupFormMoreEmulatorsProgramFilter();
            if (S != "") OpenDialog->InitialDir = S;
            if (OpenDialog->Execute()) {
                ui.ProgramEdit->setText(MakeRelPath(OpenDialog->FileName, PrgSetup->BaseDir()));
                if (Trim(ui.NameEdit->text()) == "") ui.NameEdit->setText(ChangeFileExt(ExtractFileName(ui.ProgramEdit->text()), ""));
            }
            break;
    }
}
