// C++ port of SetupFrameZipPrgsUnit.pas (implementation section).
#include "SetupFrameZipPrgsUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"
#include "ZipInfoFormUnit.h"
#include "IconLoaderUnit.h"

static constexpr int CSIDL_PROGRAM_FILES_ = 0x0026;   // ShlObj.CSIDL_PROGRAM_FILES

// ComboBox.Items.Exchange(I1,I2)
static void ComboExchange(QComboBox *Combo, int I1, int I2)
{
    QString S = Combo->itemText(I1);
    Combo->setItemText(I1, Combo->itemText(I2));
    Combo->setItemText(I2, S);
}

struct TPackerDefaultValue {
    QString Name, Extensions, CommandExtract, CommandCreate, CommandUpdate;
    bool TrainlingBackslash;
    QString DefaultFileName;
};

static constexpr int PackerDefaultValuesCount = 5;

static const TPackerDefaultValue PackerDefaultValues[PackerDefaultValuesCount] = {
    {"7z", "GZIP;BZIP2;TAR" /* 7z/zip by default by internal packer */, "e \"%1\" -o\"%2\" -y", "a \"%1\" \"%2*.*\" -r", "u \"%1\" \"%2*.*\" -r", true, "7-Zip\\7z.exe"},
    {"rar", "RAR", "x \"%1\" \"%2\" -y -c-", "a -y -r0 -ep1 \"%1\" \"%2*.*\"", "u -y -r0 -ep1 \"%1\" \"%2*.*\"", true, ""},
    {"winrar", "RAR", "x \"%1\" \"%2\" -y -c-", "a -y -r0 -ep1 \"%1\" \"%2*.*\"", "u -y -r0 -ep1 \"%1\" \"%2*.*\"", true, "winrar\\winrar.exe"},
    {"uha", "UHA", "a -m3 -pe -ph+ -r+ -ed+ \"%1\" \"%2*.*\"", "x -y+ -o+ -t\"%2\" \"%1\"", "x -y+ -o+ -t\"%2\" \"%1\"", false, ""},
    {"arj32", "ARJ", "a -r -e1 -i6 -p1 -v1440 -hk -jm -jyv -vv \"%1\"", "x -r -v -y \"%1\" \"%2 \"", "x -r -v -y \"%1\" \"%2 \"", true, ""}
};

void FirstRunPackerAutoSetup()
{
    QString Prg;
    int I, J;

    Prg = IncludeTrailingPathDelimiter(GetSpecialFolder(0, CSIDL_PROGRAM_FILES_));
    for (I = 0; I < PackerDefaultValuesCount; I++) if ((Trim(PackerDefaultValues[I].DefaultFileName) != "") && FileExists(Prg + PackerDefaultValues[I].DefaultFileName)) {
        J = PrgSetup->AddPackerSettings(PackerDefaultValues[I].Name);
        PrgSetup->PackerSettings(J)->SetZipFileName(Prg + PackerDefaultValues[I].DefaultFileName);
        PrgSetup->PackerSettings(J)->SetFileExtensions(PackerDefaultValues[I].Extensions);
        PrgSetup->PackerSettings(J)->SetExtractFile(PackerDefaultValues[I].CommandExtract);
        PrgSetup->PackerSettings(J)->SetCreateFile(PackerDefaultValues[I].CommandCreate);
        PrgSetup->PackerSettings(J)->SetUpdateFile(PackerDefaultValues[I].CommandUpdate);
        PrgSetup->PackerSettings(J)->SetTrailingBackslash(PackerDefaultValues[I].TrainlingBackslash);
    }
}

TSetupFrameZipPrgs::TSetupFrameZipPrgs(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    PrgOpenDialog = new TOpenDialog(this);
    PrgOpenDialog->DefaultExt = "exe";

    // DFM event wiring
    connect(ui.DeleteButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DeleteButton); });
    connect(ui.UpButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.UpButton); });
    connect(ui.DownButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DownButton); });
    connect(ui.AddButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.AddButton); });
    connect(ui.FilenameButton, &QToolButton::clicked, this, [this]() { FilenameButtonClick(ui.FilenameButton); });
    connect(ui.SelectComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int) { SelectComboBoxChange(ui.SelectComboBox); });
    connect(ui.FilenameEdit, &QLineEdit::textChanged, this, [this]() { FilenameEditChange(ui.FilenameEdit); });
    connect(ui.AutoSetupButton, &QPushButton::clicked, this, [this]() { AutoSetupButtonClick(ui.AutoSetupButton); });
}

TSetupFrameZipPrgs::~TSetupFrameZipPrgs()
{
    delete PrgOpenDialog;
}

/* TSetupFrameZipPrgs */

QString TSetupFrameZipPrgs::GetName()
{
    return LanguageSetup->SetupFormExternalPackers();
}

void TSetupFrameZipPrgs::InitGUIAndLoadSetup(TInitData& InitData)
{
    Q_UNUSED(InitData);
    int I;

    // NoFlicker(...): not needed in Qt

    Packers.clear();
    for (I = 0; I < PrgSetup->PackerSettingsCount(); I++) {
        TPacker P;
        P.Name = ChangeFileExt(ExtractFileName(PrgSetup->PackerSettings(I)->ZipFileName()), "");
        P.FileName = PrgSetup->PackerSettings(I)->ZipFileName();
        P.Extensions = PrgSetup->PackerSettings(I)->FileExtensions();
        P.ExtractFile = PrgSetup->PackerSettings(I)->ExtractFile();
        P.CreateFile = PrgSetup->PackerSettings(I)->CreateFile();
        P.UpdateFile = PrgSetup->PackerSettings(I)->UpdateFile();
        P.TrailingBackslash = PrgSetup->PackerSettings(I)->TrailingBackslash();
        Packers.append(P);
    }

    UserIconLoader->DialogImage(DI_Add, ui.AddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.DeleteButton);
    UserIconLoader->DialogImage(DI_Up, ui.UpButton);
    UserIconLoader->DialogImage(DI_Down, ui.DownButton);
    UserIconLoader->DialogImage(DI_SelectFile, ui.FilenameButton);
    UserIconLoader->DialogImage(DI_ResetDefault, ui.AutoSetupButton);

    LastIndex = -1;
    JustChanging = false;
    for (I = 0; I < Packers.count(); I++) ui.SelectComboBox->addItem(Packers[I].Name);
}

void TSetupFrameZipPrgs::BeforeChangeLanguage()
{
}

void TSetupFrameZipPrgs::LoadLanguage()
{
    ui.SelectLabel->setText(LanguageSetup->SetupFormExternalPackersSelect());
    ui.FilenameEdit_EditLabel->setText(LanguageSetup->SetupFormExternalPackersFileName());
    ui.ExtensionsEdit_EditLabel->setText(LanguageSetup->SetupFormExternalPackersExtensions());
    ui.CommandExtractEdit_EditLabel->setText(LanguageSetup->SetupFormExternalPackersCommandExtract());
    ui.CommandCreateEdit_EditLabel->setText(LanguageSetup->SetupFormExternalPackersCommandCreate());
    ui.CommandAddEdit_EditLabel->setText(LanguageSetup->SetupFormExternalPackersCommandAdd());
    ui.TrailingBackslashCheckBox->setText(LanguageSetup->SetupFormExternalPackersTrailingBackslash());
    ui.InfoLabel->setText(LanguageSetup->SetupFormExternalPackersInfo());

    setProperty("HelpContext", ID_FileOptionsZipPackers);
}

void TSetupFrameZipPrgs::ButtonWork(QObject* Sender)
{
    int I, J;
    TPacker P;

    switch (Sender->property("tag").toInt()) {
        case 0: { /* Add */
            SelectComboBoxChange(Sender);
            TPacker N;
            N.Name = "-"; N.FileName = ""; N.Extensions = ""; N.ExtractFile = ""; N.CreateFile = ""; N.UpdateFile = ""; N.TrailingBackslash = true;
            Packers.append(N);
            ui.SelectComboBox->addItem("-");
            ui.SelectComboBox->setCurrentIndex(ui.SelectComboBox->count() - 1);
            LastIndex = -1;
            SelectComboBoxChange(Sender);
            break;
        }
        case 1: /* Delete */
            I = ui.SelectComboBox->currentIndex();
            if (I < 0) return;
            ui.SelectComboBox->setCurrentIndex(-1);
            LastIndex = -1;
            SelectComboBoxChange(Sender);

            for (J = I + 1; J < Packers.count(); J++) Packers[J - 1] = Packers[J];
            Packers.removeLast();
            ui.SelectComboBox->removeItem(I);

            I = qMax(I - 1, 0);
            if (ui.SelectComboBox->count() > 0) ui.SelectComboBox->setCurrentIndex(I);
            SelectComboBoxChange(Sender);
            break;
        case 2: /* Up */
            I = ui.SelectComboBox->currentIndex();
            if (I <= 0) return;
            ui.SelectComboBox->setCurrentIndex(-1);
            SelectComboBoxChange(Sender);

            ComboExchange(ui.SelectComboBox, I, I - 1);
            P = Packers[I]; Packers[I] = Packers[I - 1]; Packers[I - 1] = P;

            ui.SelectComboBox->setCurrentIndex(I - 1);
            LastIndex = I - 1;
            SelectComboBoxChange(Sender);
            break;
        case 3: /* Down */
            I = ui.SelectComboBox->currentIndex();
            if ((I < 0) || (I == ui.SelectComboBox->count() - 1)) return;
            ui.SelectComboBox->setCurrentIndex(-1);
            SelectComboBoxChange(Sender);

            ComboExchange(ui.SelectComboBox, I, I + 1);
            P = Packers[I]; Packers[I] = Packers[I + 1]; Packers[I + 1] = P;

            ui.SelectComboBox->setCurrentIndex(I + 1);
            LastIndex = I + 1;
            SelectComboBoxChange(Sender);
            break;
    }
}

void TSetupFrameZipPrgs::DOSBoxDirChanged()
{
}

void TSetupFrameZipPrgs::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
    if (ui.SelectComboBox->count() > 0) ui.SelectComboBox->setCurrentIndex(0);
    SelectComboBoxChange(this);
}

void TSetupFrameZipPrgs::HideFrame()
{
}

void TSetupFrameZipPrgs::RestoreDefaults()
{
}

void TSetupFrameZipPrgs::SaveSetup()
{
    int I;

    while (PrgSetup->PackerSettingsCount() > Packers.count()) PrgSetup->DeletePackerSettings(PrgSetup->PackerSettingsCount() - 1);
    while (PrgSetup->PackerSettingsCount() < Packers.count()) PrgSetup->AddPackerSettings("");

    ui.SelectComboBox->setCurrentIndex(-1);
    SelectComboBoxChange(this);
    for (I = 0; I < Packers.count(); I++) {
        PrgSetup->PackerSettings(I)->SetName(ChangeFileExt(ExtractFileName(Packers[I].FileName), ""));
        PrgSetup->PackerSettings(I)->SetZipFileName(Packers[I].FileName);
        PrgSetup->PackerSettings(I)->SetFileExtensions(CheckExtensionsList(Packers[I].Extensions));
        PrgSetup->PackerSettings(I)->SetExtractFile(Packers[I].ExtractFile);
        PrgSetup->PackerSettings(I)->SetCreateFile(Packers[I].CreateFile);
        PrgSetup->PackerSettings(I)->SetUpdateFile(Packers[I].UpdateFile);
        PrgSetup->PackerSettings(I)->SetTrailingBackslash(Packers[I].TrailingBackslash);
    }
}

void TSetupFrameZipPrgs::SelectComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    bool B;
    QString S;
    int I;

    if (JustChanging) return;
    JustChanging = true;
    struct TReset { bool &V; ~TReset() { V = false; } } Reset{JustChanging};   // finally JustChanging:=False

    if (LastIndex >= 0) {
        Packers[LastIndex].Name = ui.SelectComboBox->itemText(LastIndex);
        Packers[LastIndex].FileName = ui.FilenameEdit->text();
        Packers[LastIndex].Extensions = CheckExtensionsList(ui.ExtensionsEdit->text());
        Packers[LastIndex].ExtractFile = ui.CommandExtractEdit->text();
        Packers[LastIndex].CreateFile = ui.CommandCreateEdit->text();
        Packers[LastIndex].UpdateFile = ui.CommandAddEdit->text();
        Packers[LastIndex].TrailingBackslash = ui.TrailingBackslashCheckBox->isChecked();
    }

    ui.SelectComboBox->setEnabled(ui.SelectComboBox->count() > 0);
    ui.DeleteButton->setEnabled(ui.SelectComboBox->currentIndex() >= 0);
    ui.UpButton->setEnabled(ui.SelectComboBox->currentIndex() > 0);
    ui.DownButton->setEnabled((ui.SelectComboBox->currentIndex() >= 0) && (ui.SelectComboBox->currentIndex() < ui.SelectComboBox->count() - 1));

    LastIndex = ui.SelectComboBox->currentIndex();
    B = ui.SelectComboBox->currentIndex() >= 0;
    ui.FilenameEdit->setVisible(B); ui.FilenameEdit_EditLabel->setVisible(B);
    ui.FilenameButton->setVisible(B);
    ui.ExtensionsEdit->setVisible(B); ui.ExtensionsEdit_EditLabel->setVisible(B);
    ui.CommandExtractEdit->setVisible(B); ui.CommandExtractEdit_EditLabel->setVisible(B);
    ui.CommandCreateEdit->setVisible(B); ui.CommandCreateEdit_EditLabel->setVisible(B);
    ui.CommandAddEdit->setVisible(B); ui.CommandAddEdit_EditLabel->setVisible(B);
    ui.TrailingBackslashCheckBox->setVisible(B);
    ui.AutoSetupButton->setVisible(false);
    if (B) {
        S = Trim(ExtUpperCase(Packers[ui.SelectComboBox->currentIndex()].Name));
        for (I = 0; I < PackerDefaultValuesCount; I++) if (Trim(ExtUpperCase(PackerDefaultValues[I].Name)) == S) {
            ui.AutoSetupButton->setVisible(true);
            ui.AutoSetupButton->setText(LanguageSetup->SetupFormExternalPackersSetupButton() + " " + PackerDefaultValues[I].Name);
            break;
        }
    }
    if (!B) return;

    ui.FilenameEdit->setText(Packers[ui.SelectComboBox->currentIndex()].FileName);
    ui.ExtensionsEdit->setText(CheckExtensionsList(Packers[ui.SelectComboBox->currentIndex()].Extensions));
    ui.CommandExtractEdit->setText(Packers[ui.SelectComboBox->currentIndex()].ExtractFile);
    ui.CommandCreateEdit->setText(Packers[ui.SelectComboBox->currentIndex()].CreateFile);
    ui.CommandAddEdit->setText(Packers[ui.SelectComboBox->currentIndex()].UpdateFile);
    ui.TrailingBackslashCheckBox->setChecked(Packers[ui.SelectComboBox->currentIndex()].TrailingBackslash);
}

void TSetupFrameZipPrgs::FilenameButtonClick(QObject* Sender)
{
    QString S;
    bool DoAutoSetup;

    S = Trim(ui.FilenameEdit->text()); if (S != "") S = ExtractFilePath(S);
    if (S == "") S = GetSpecialFolder(0, CSIDL_PROGRAM_FILES_);
    PrgOpenDialog->Title = LanguageSetup->SetupFormExternalPackersFileNameTitle();
    PrgOpenDialog->Filter = LanguageSetup->SetupFormExeFilter();
    if (S != "") PrgOpenDialog->InitialDir = S;
    if (PrgOpenDialog->Execute()) {
        DoAutoSetup = (Trim(ui.FilenameEdit->text()) == "");
        ui.FilenameEdit->setText(PrgOpenDialog->FileName);
        FilenameEditChange(Sender);
        SelectComboBoxChange(Sender);
        if (DoAutoSetup) AutoSetupButtonClick(Sender);
    }
}

void TSetupFrameZipPrgs::FilenameEditChange(QObject* Sender)
{
    int I;

    I = ui.SelectComboBox->currentIndex();
    if (I < 0) return;   // (VCL: assignment to Items[-1] raises an exception)
    if (Trim(ui.FilenameEdit->text()) == "")
        ui.SelectComboBox->setItemText(I, "-");
    else
        ui.SelectComboBox->setItemText(I, ChangeFileExt(ExtractFileName(ui.FilenameEdit->text()), ""));
    ui.SelectComboBox->setCurrentIndex(I);

    SelectComboBoxChange(Sender);
}

void TSetupFrameZipPrgs::AutoSetupButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    int I;

    S = Trim(ExtUpperCase(Packers[ui.SelectComboBox->currentIndex()].Name));
    for (I = 0; I < PackerDefaultValuesCount; I++) if (Trim(ExtUpperCase(PackerDefaultValues[I].Name)) == S) {
        ui.ExtensionsEdit->setText(PackerDefaultValues[I].Extensions);
        ui.CommandExtractEdit->setText(PackerDefaultValues[I].CommandExtract);
        ui.CommandCreateEdit->setText(PackerDefaultValues[I].CommandCreate);
        ui.CommandAddEdit->setText(PackerDefaultValues[I].CommandUpdate);
        ui.TrailingBackslashCheckBox->setChecked(PackerDefaultValues[I].TrainlingBackslash);
        break;
    }
}
