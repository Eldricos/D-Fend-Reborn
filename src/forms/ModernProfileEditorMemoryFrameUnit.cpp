// C++ port of ModernProfileEditorMemoryFrameUnit.pas (implementation section).
#include "ModernProfileEditorMemoryFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "Dialogs.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

// Label.Font.Color:=Color
static void SetFontColor(QWidget* W, TColor Color)
{
    QPalette P = W->palette();
    P.setColor(QPalette::WindowText, TColorToQColor(Color));
    P.setColor(QPalette::Text, TColorToQColor(Color));
    W->setPalette(P);
}

TModernProfileEditorMemoryFrame::TModernProfileEditorMemoryFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM: Timer: TTimer (Enabled = False, Interval = 100)
    Timer = new QTimer(this);
    Timer->setInterval(100);
    connect(Timer, &QTimer::timeout, this, [this]() { TimerTimer(Timer); });

    // DFM wiring
    connect(ui.DOS32AInfoButton, &QToolButton::clicked, this, [this]() { DOS32AInfoButtonClick(ui.DOS32AInfoButton); });
    connect(ui.LoadFixCheckBox, &QCheckBox::clicked, this, [this]() { LoadFixCheckBoxClick(ui.LoadFixCheckBox); });
    connect(ui.LoadFixEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { LoadFixEditChange(ui.LoadFixEdit); });
}

TModernProfileEditorMemoryFrame::~TModernProfileEditorMemoryFrame() = default;

// { TModernProfileEditorMemoryFrame }

void TModernProfileEditorMemoryFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    ui.MemoryLabel->setText(LanguageSetup->GameMemory());
    ui.XMSCheckBox->setText(LanguageSetup->GameXMS());
    ui.EMSCheckBox->setText(LanguageSetup->GameEMS());
    ui.UMBCheckBox->setText(LanguageSetup->GameUMB());
    ui.LoadFixCheckBox->setText(LanguageSetup->ProfileEditorLoadFix());
    ui.LoadFixLabel->setText(LanguageSetup->ProfileEditorLoadFixMemory());
    ui.DOS32ACheckBox->setText(LanguageSetup->GameDOS32A());
    ui.DOS32AInfoLabel->setText(LanguageSetup->ProfileEditorNeedFreeDOS());
    if (DirectoryExists(IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->PathToFREEDOS(), PrgSetup->BaseDir())))) {
        SetFontColor(ui.DOS32AInfoLabel, clGrayText);
    } else {
        SetFontColor(ui.DOS32AInfoLabel, clRed);
    }
    LastProfileExe = "-";
    ProfileExe = InitData.CurrentProfileExe;

    setProperty("HelpContext", ID_ProfileEditMemory);  // HelpContext:=...
}

void TModernProfileEditorMemoryFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    ui.MemoryEdit->setValue(Game->Memory());
    ui.XMSCheckBox->setChecked(Game->XMS());
    ui.EMSCheckBox->setChecked(Game->EMS());
    ui.UMBCheckBox->setChecked(Game->UMB());
    ui.LoadFixCheckBox->setChecked(Game->LoadFix());
    ui.LoadFixEdit->setValue(Game->LoadFixMemory());
    ui.DOS32ACheckBox->setChecked(Game->UseDOS32A());
    Timer->start();
    SetFreeMemInfo();
}

void TModernProfileEditorMemoryFrame::SetFreeMemInfo()
{
    int I;
    if (ui.LoadFixCheckBox->isChecked()) I = 632 - 1 - std::min(512, std::max(0, ui.LoadFixEdit->value())); else I = 632;
    ui.FreeMemLabel->setText(Format(LanguageSetup->GameMemoryFree(), {I}));
}

void TModernProfileEditorMemoryFrame::GetGame(TGame* Game)
{
    Game->SetMemory(std::min(63, std::max(1, ui.MemoryEdit->value())));
    Game->SetXMS(ui.XMSCheckBox->isChecked());
    Game->SetEMS(ui.EMSCheckBox->isChecked());
    Game->SetUMB(ui.UMBCheckBox->isChecked());
    Game->SetLoadFix(ui.LoadFixCheckBox->isChecked());
    Game->SetLoadFixMemory(std::min(512, std::max(0, ui.LoadFixEdit->value())));
    Game->SetUseDOS32A(ui.DOS32ACheckBox->isChecked());
    Timer->stop();
}

void TModernProfileEditorMemoryFrame::TimerTimer(QObject* Sender)
{
    Q_UNUSED(Sender);
    bool B;
    QString S;

    if (ProfileExe == nullptr) return;
    if (*ProfileExe == LastProfileExe) return;
    LastProfileExe = *ProfileExe;

    if (Trim(*ProfileExe).isEmpty()) B = false; else {
        S = MakeAbsPath(*ProfileExe, PrgSetup->BaseDir());
        B = FileExists(IncludeTrailingPathDelimiter(ExtractFilePath(S)) + "DOS4GW.EXE");
        if (!B) B = FindStringInFile(MakeAbsPath(*ProfileExe, PrgSetup->BaseDir()), "DOS4GW");
    }
    if (B) {
        ui.DOS32AInfoButton->setIcon(VclLayout::imageListIcon("ModernProfileEditorMemoryFrameUnit", "ImageList", 0));
        ui.DOS32AInfoButton->setToolTip(LanguageSetup->GameDOS32AUseable());
    } else {
        ui.DOS32AInfoButton->setIcon(VclLayout::imageListIcon("ModernProfileEditorMemoryFrameUnit", "ImageList", 1));
        ui.DOS32AInfoButton->setToolTip(LanguageSetup->GameDOS32ANotUseable());
    }
}

void TModernProfileEditorMemoryFrame::LoadFixCheckBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    SetFreeMemInfo();
}

void TModernProfileEditorMemoryFrame::LoadFixEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    SetFreeMemInfo();
}

void TModernProfileEditorMemoryFrame::DOS32AInfoButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    MessageDlg(ui.DOS32AInfoButton->toolTip(), mtInformation, mbOK, 0);
}
