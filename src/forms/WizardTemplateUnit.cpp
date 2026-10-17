// C++ port of WizardTemplateUnit.pas (implementation section).
#include "WizardTemplateUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "HashCalc.h"
#include "CommonTools.h"
#include "WizardFormUnit.h"
#include "OperationModeInfoFormUnit.h"
#include "IconLoaderUnit.h"
#include "ZipPackageUnit.h"

namespace {

void SetBold(QWidget *W)
{
    QFont F = W->font(); F.setBold(true); W->setFont(F);
}

// Items.Objects[ItemIndex] of a combo box
TGame *SelectedObject(QComboBox *C)
{
    if (C->currentIndex() < 0) return nullptr;
    return static_cast<TGame *>(C->itemData(C->currentIndex()).value<void *>());
}

void AddObject(QComboBox *C, const QString &S, TGame *G)
{
    C->addItem(S, QVariant::fromValue<void *>(G));
}

// IsEmpty
bool IsEmpty(const QString &S)
{
    TStringList *St = StringToStringList(S);
    for (int I = 0; I < St->Count(); I++) if (!Trim((*St)[I]).isEmpty()) { delete St; return false; }
    delete St;
    return true;
}

}  // namespace

// TWizardTemplateFrame

TWizardTemplateFrame::TWizardTemplateFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    connect(ui.HelpButton, &QToolButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.SpeedButton1, &QToolButton::clicked, this, [this]() { HelpButtonClick(ui.SpeedButton1); });
    connect(ui.SpeedButton2, &QToolButton::clicked, this, [this]() { HelpButtonClick(ui.SpeedButton2); });
    connect(ui.SpeedButton3, &QToolButton::clicked, this, [this]() { HelpButtonClick(ui.SpeedButton3); });
    connect(ui.SpeedButton4, &QToolButton::clicked, this, [this]() { HelpButtonClick(ui.SpeedButton4); });
    connect(ui.TemplateType1List, QOverload<int>::of(&QComboBox::activated), this, [this]() { IndirectTypeChange(ui.TemplateType1List); });
    connect(ui.TemplateType2List, QOverload<int>::of(&QComboBox::activated), this, [this]() { IndirectTypeChange(ui.TemplateType2List); });
    connect(ui.TemplateType4List, QOverload<int>::of(&QComboBox::activated), this, [this]() { IndirectTypeChange(ui.TemplateType4List); });
    connect(ui.TemplateType3List, QOverload<int>::of(&QComboBox::activated), this, [this]() { IndirectTypeChange(ui.TemplateType3List); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.OperationModeInfoButton, &QPushButton::clicked, this, [this]() { OperationModeInfoButtonClick(ui.OperationModeInfoButton); });
    connect(ui.CPUComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { IndirectTypeChange(ui.CPUComboBox); });
    connect(ui.MoreRAMCheckBox, &QCheckBox::toggled, this, [this]() { IndirectTypeChange(ui.MoreRAMCheckBox); });
    connect(ui.CloseDosBoxOnExitCheckBox, &QCheckBox::toggled, this, [this]() { IndirectTypeChange(ui.CloseDosBoxOnExitCheckBox); });
    connect(ui.StartFullscreenCheckBox, &QCheckBox::toggled, this, [this]() { IndirectTypeChange(ui.StartFullscreenCheckBox); });
}

TWizardTemplateFrame::~TWizardTemplateFrame()
{
}

void TWizardTemplateFrame::IndirectTypeChange(QObject* Sender)
{
    switch (Sender->property("tag").toInt()) {
    case 0: ui.TemplateType1->setChecked(true); break;
    case 1: ui.TemplateType2->setChecked(true); break;
    case 2: ui.TemplateType3->setChecked(true); break;
    case 3: ui.TemplateType4->setChecked(true); break;
    case 4: ui.TemplateType5->setChecked(true); break;
    }
}

void TWizardTemplateFrame::Init(TGameDB* AGameDB, TGameDB* ATemplateDB, TGameDB* AAutoSetupDB)
{
    // SetVistaFonts(self); - not needed

    FGameDB = AGameDB;
    TemplateDB = ATemplateDB;
    AutoSetupDB = AAutoSetupDB;

    SetBold(ui.InfoLabel);
    SetBold(ui.TemplateType1);
    SetBold(ui.TemplateType2);
    SetBold(ui.TemplateType3);
    SetBold(ui.TemplateType4);
    SetBold(ui.TemplateType5);

    ui.InfoLabel->setText(LanguageSetup->WizardFormPage3Info());
    ui.TemplateType1->setText(LanguageSetup->WizardFormTemplateType1());
    ui.TemplateType2->setText(LanguageSetup->WizardFormTemplateType2());
    ui.TemplateType3->setText(LanguageSetup->WizardFormTemplateType3());
    ui.TemplateType4->setText(LanguageSetup->WizardFormTemplateType4());
    ui.TemplateType5->setText(LanguageSetup->WizardFormTemplateType5());
    ui.TemplateType5GroupBox->setTitle(LanguageSetup->WizardFormTemplateType5Settings());
    ui.CPULabel->setText(LanguageSetup->WizardFormCPU());
    while (ui.CPUComboBox->count() < 4) ui.CPUComboBox->addItem(QString());
    ui.CPUComboBox->setItemText(0, LanguageSetup->WizardFormCPUType1());
    ui.CPUComboBox->setItemText(1, LanguageSetup->WizardFormCPUType2());
    ui.CPUComboBox->setItemText(2, LanguageSetup->WizardFormCPUType3());
    ui.CPUComboBox->setItemText(3, LanguageSetup->WizardFormCPUType4());
    ui.CPUComboBox->setCurrentIndex(1);
    ui.StartFullscreenCheckBox->setText(LanguageSetup->GameStartFullscreen());
    ui.CloseDosBoxOnExitCheckBox->setText(LanguageSetup->GameCloseDOSBoxAfterGameExit());
    ui.MoreRAMCheckBox->setText(LanguageSetup->WizardFormMoreRAM());

    {
        TStringList *St = StringToStringList(LanguageSetup->WizardFormTemplateInfo());
        ui.PanelInfoLabel->setText(St->Text());
        delete St;
    }
    for (int I = 0; I < AutoSetupDB->Count(); I++) AddObject(ui.TemplateType3List, AutoSetupDB->Game(I)->CacheName, AutoSetupDB->Game(I));
    if (ui.TemplateType3List->count() > 0) ui.TemplateType3List->setCurrentIndex(0);
    ui.TemplateType3->setEnabled(ui.TemplateType3List->count() > 0);
    ui.TemplateType3List->setEnabled(ui.TemplateType3List->count() > 0);

    AddObject(ui.TemplateType4List, LanguageSetup->TemplateFormDefault(), nullptr);
    for (int I = 0; I < TemplateDB->Count(); I++) AddObject(ui.TemplateType4List, TemplateDB->Game(I)->CacheName, TemplateDB->Game(I));
    ui.TemplateType4List->setCurrentIndex(0);

    ui.OKButton->setText(LanguageSetup->OK());
    ui.OperationModeInfoButton->setText(LanguageSetup->WizardFormOperationModeInfo());

    UserIconLoader->DialogImage(DI_ToolbarHelp, ui.HelpButton);
    UserIconLoader->DialogImage(DI_ToolbarHelp, ui.SpeedButton1);
    UserIconLoader->DialogImage(DI_ToolbarHelp, ui.SpeedButton2);
    UserIconLoader->DialogImage(DI_ToolbarHelp, ui.SpeedButton3);
    UserIconLoader->DialogImage(DI_ToolbarHelp, ui.SpeedButton4);
}

void TWizardTemplateFrame::Done()
{
    delete TemplateDB; TemplateDB = nullptr;
    delete AutoSetupDB; AutoSetupDB = nullptr;
}

void TWizardTemplateFrame::SearchTemplates(const QString& GameFile)
{
    int I, J, NeededForList1;
    QString GameCheckSum, GameFileShort;
    bool CompleteMatch;

    ui.TemplateType1List->clear();
    ui.TemplateType2List->clear();

    GameCheckSum = GetMD5Sum(MakeAbsPath(GameFile, PrgSetup->BaseDir()), false);
    if (Trim(GameFile).isEmpty()) GameFileShort.clear(); else GameFileShort = Trim(ExtUpperCase(ExtractFileName(GameFile)));

    // Find matching auto setup templates
    QList<int> ListNr;
    QList<int> ListType;
    CompleteMatch = false;
    if (!GameFileShort.isEmpty()) for (I = 0; I < AutoSetupDB->Count(); I++) {
        if (Trim(ExtUpperCase(ExtractFileName(AutoSetupDB->Game(I)->GameExe()))) != GameFileShort) continue;
        ListNr.append(I);
        ListType.append(0);
        if (AutoSetupDB->Game(I)->GameExeMD5().isEmpty() || (AutoSetupDB->Game(I)->GameExeMD5() != GameCheckSum)) continue;
        ListType[ListType.size() - 1] = 1;
        if (!AdditionalDataMatching(ExtractFilePath(MakeAbsPath(GameFile, PrgSetup->BaseDir())), AutoSetupDB->Game(I))) continue;
        ListType[ListType.size() - 1] = 2;
        CompleteMatch = true;
    }
    if (CompleteMatch) NeededForList1 = 2; else NeededForList1 = 1;
    for (I = 0; I < ListNr.size(); I++) {
        J = ListNr[I];
        if (ListType[I] < NeededForList1)
            AddObject(ui.TemplateType2List, AutoSetupDB->Game(J)->CacheName, AutoSetupDB->Game(J));
        else
            AddObject(ui.TemplateType1List, AutoSetupDB->Game(J)->CacheName, AutoSetupDB->Game(J));
    }

    if (ui.TemplateType1List->count() > 0) ui.TemplateType1List->setCurrentIndex(0);
    if (ui.TemplateType2List->count() > 0) ui.TemplateType2List->setCurrentIndex(0);

    ui.TemplateType1->setEnabled(ui.TemplateType1List->count() > 0);
    ui.TemplateType1List->setEnabled(ui.TemplateType1List->count() > 0);
    ui.TemplateType2->setEnabled(ui.TemplateType2List->count() > 0);
    ui.TemplateType2List->setEnabled(ui.TemplateType2List->count() > 0);

    if (ui.TemplateType1->isEnabled()) { ui.TemplateType1->setChecked(true); return; }
    if (ui.TemplateType2->isEnabled()) { ui.TemplateType2->setChecked(true); return; }
    ui.TemplateType4->setChecked(true);
}

bool TWizardTemplateFrame::SelectedProfileSecure()
{
    bool result = true;
    TGame* Template = GetTemplate();
    if (Template == nullptr) return result;

    result = IsEmpty(Template->Autoexec()) && IsEmpty(Template->AutoexecFinalization()) && IsEmpty(Template->CustomSettings());
    if (!result) return result;

    for (int I = 0; I < Template->NrOfMounts(); I++) {
        QString S = Template->Mount(I);
        TStringList* St = ValueToList(S);
        /* Types to check:
           RealFolder;DRIVE;Letter;False;;FreeSpace
           RealFolder;FLOPPY;Letter;False;;
           RealFolder$ZipFile;PHYSFS;Letter;False;;FreeSpace
           all other are images or read-only */
        if (St->Count() < 2) { delete St; continue; }
        S = Trim(ExtUpperCase((*St)[1]));
        if ((S != "DRIVE") && (S != "FLOPPY") && (S != "PHYSFS")) { delete St; continue; }
        if ((S == "PHYSFS") && (Pos("$", (*St)[0]) > 0)) {
            S = Trim((*St)[0]); S = Trim(Copy(S, 1, Pos("$", S) - 1));
        } else {
            S = Trim((*St)[0]);
        }
        S = MakeRelPath(S, PrgSetup->BaseDir(), true);
        delete St;
        // literal translation of "If Copy(S,2,3)=':\' then" (only matches "X:\")
        if (Copy(S, 2, 3) == ":\\" || Copy(S, 2, 3) == ":/") { result = false; return result; }
    }
    return result;
}

TGame* TWizardTemplateFrame::GetTemplate()
{
    TGame* result = nullptr;
    if (ui.TemplateType1->isChecked()) result = SelectedObject(ui.TemplateType1List);
    if (ui.TemplateType2->isChecked()) result = SelectedObject(ui.TemplateType2List);
    if (ui.TemplateType3->isChecked()) result = SelectedObject(ui.TemplateType3List);
    if (ui.TemplateType4->isChecked()) result = SelectedObject(ui.TemplateType4List);
    return result;
}

TGame* TWizardTemplateFrame::CreateGameFromTemplate(TGame* Template, const QString& GameName)
{
    TGame* result = FGameDB->Game(FGameDB->Add(GameName));
    if (Template == nullptr) {
        TGame* DefaultTemplate = new TGame(PrgSetup);
        result->AssignFrom(DefaultTemplate);
        delete DefaultTemplate;
    } else {
        result->AssignFrom(Template);
    }
    result->SetName(GameName);
    return result;
}

void TWizardTemplateFrame::OperationModeInfoButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ShowOperationModeInfoDialog(this);
}

TGame* TWizardTemplateFrame::CreateGame(const QString& GameName)
{
    TGame* result = nullptr;

    if (ui.TemplateType1->isChecked()) return CreateGameFromTemplate(SelectedObject(ui.TemplateType1List), GameName);
    if (ui.TemplateType2->isChecked()) return CreateGameFromTemplate(SelectedObject(ui.TemplateType2List), GameName);
    if (ui.TemplateType3->isChecked()) return CreateGameFromTemplate(SelectedObject(ui.TemplateType3List), GameName);
    if (ui.TemplateType4->isChecked()) return CreateGameFromTemplate(SelectedObject(ui.TemplateType4List), GameName);

    if (ui.TemplateType5->isChecked()) {
        result = FGameDB->Game(FGameDB->Add(GameName));
        result->SetStartFullscreen(ui.StartFullscreenCheckBox->isChecked());
        result->SetCloseDosBoxAfterGameExit(ui.CloseDosBoxOnExitCheckBox->isChecked());
        if (ui.MoreRAMCheckBox->isChecked()) result->SetMemory(63);
        switch (ui.CPUComboBox->currentIndex()) {
        case 0: result->SetCore("normal"); result->SetCycles("3000"); break;
        case 1: result->SetCore("normal"); result->SetCycles("10000"); break;
        case 2: result->SetCore("dynamic"); result->SetCycles("30000"); break;
        }
    }
    return result;
}

TGame* TWizardTemplateFrame::CreateWindowsGame(const QString& GameName)
{
    TGame* result = FGameDB->Game(FGameDB->Add(GameName));
    TGame* DefaultTemplate = new TGame(PrgSetup);
    result->AssignFrom(DefaultTemplate);
    delete DefaultTemplate;
    result->SetProfileMode("Windows");
    result->SetName(GameName);
    return result;
}

void TWizardTemplateFrame::HelpButtonClick(QObject* Sender)
{
    switch (Sender->property("tag").toInt()) {
    case 0: {
        ui.HelpButton->setEnabled(false);
        ui.SpeedButton1->setVisible(false);
        ui.SpeedButton2->setVisible(false);
        ui.SpeedButton3->setVisible(false);
        ui.SpeedButton4->setVisible(false);
        ui.CPULabel->setVisible(false);
        ui.TemplateType5GroupBox->setVisible(false);
        const int W = width() - 2 * ui.InfoPanel->x();
        const int H = height() - 8 - ui.InfoPanel->y();
        VclLayout::setBounds(ui.InfoPanel, ui.InfoPanel->x(), ui.InfoPanel->y(), W, H);
        ui.InfoPanel->setVisible(true);
        ui.InfoPanel->raise();
        if (auto *F = qobject_cast<TWizardForm *>(window())) F->ui.BottomPanel->setVisible(false);
        break;
    }
    case 1: MessageDlg(LanguageSetup->WizardFormTemplateType1Info(), mtInformation, mbOK, 0); break;
    case 2: MessageDlg(LanguageSetup->WizardFormTemplateType2Info(), mtInformation, mbOK, 0); break;
    case 3: MessageDlg(LanguageSetup->WizardFormTemplateType3Info(), mtInformation, mbOK, 0); break;
    case 4: MessageDlg(LanguageSetup->WizardFormTemplateType4Info(), mtInformation, mbOK, 0); break;
    }
}

void TWizardTemplateFrame::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.InfoPanel->setVisible(false);
    ui.HelpButton->setEnabled(true);
    ui.CPULabel->setVisible(true);
    ui.SpeedButton1->setVisible(true);
    ui.SpeedButton2->setVisible(true);
    ui.SpeedButton3->setVisible(true);
    ui.SpeedButton4->setVisible(true);
    ui.TemplateType5GroupBox->setVisible(true);
    if (auto *F = qobject_cast<TWizardForm *>(window())) F->ui.BottomPanel->setVisible(true);
}
