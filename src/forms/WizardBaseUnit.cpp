// C++ port of WizardBaseUnit.pas (implementation section).
#include "WizardBaseUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "ListScummVMGamesFormUnit.h"
#include "TextViewerFormUnit.h"
#include "IconLoaderUnit.h"
#include "WizardFormUnit.h"

namespace {

// TLabel.OnClick
class LabelClickFilter : public QObject
{
public:
    LabelClickFilter(QLabel *L, std::function<void()> F) : QObject(L), FF(std::move(F)) { L->installEventFilter(this); }
    bool eventFilter(QObject *, QEvent *E) override
    {
        if (E->type() == QEvent::MouseButtonRelease && static_cast<QMouseEvent *>(E)->button() == Qt::LeftButton) {
            FF();
            return true;
        }
        return false;
    }
private:
    std::function<void()> FF;
};

void SetLinkStyle(QLabel *L)
{
    // with Font do begin Color:=clBlue; Style:=[fsUnderline]; end; Cursor:=crHandPoint;
    QFont F = L->font(); F.setUnderline(true); L->setFont(F);
    QPalette P = L->palette(); P.setColor(QPalette::WindowText, TColorToQColor(clBlue)); L->setPalette(P);
    L->setCursor(Qt::PointingHandCursor);
}

void SetBold(QWidget *W)
{
    QFont F = W->font(); F.setBold(true); W->setFont(F);
}

}  // namespace

// TWizardBaseFrame

TWizardBaseFrame::TWizardBaseFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    new LabelClickFilter(ui.ListScummGamesLabel, [this]() { ButtonWork(ui.ListScummGamesLabel); });
    new LabelClickFilter(ui.ShowInfoLabel, [this]() { ButtonWork(ui.ShowInfoLabel); });
    new LabelClickFilter(ui.InfoLabelInstallationSupportLink, [this]() { ButtonWork(ui.InfoLabelInstallationSupportLink); });
}

TWizardBaseFrame::~TWizardBaseFrame()
{
}

void TWizardBaseFrame::BeforeClose()
{
    PrgSetup->SetLastWizardMode(ui.WizardModeComboBox->currentIndex());
}

void TWizardBaseFrame::Init(TGameDB* GameDB, QWidget* ADialog)
{
    Q_UNUSED(GameDB);
    // SetVistaFonts(self); - not needed
    FDialog = ADialog;

    SetBold(ui.InfoLabel);
    ui.InfoLabel->setText(LanguageSetup->WizardFormPage1Info());
    ui.EmulationTypeLabel->setText(LanguageSetup->WizardFormEmulationType());
    while (ui.EmulationTypeComboBox->count() < 3) ui.EmulationTypeComboBox->addItem(QString());
    ui.EmulationTypeComboBox->setItemText(0, LanguageSetup->WizardFormEmulationTypeDOSBox());
    ui.EmulationTypeComboBox->setItemText(1, LanguageSetup->WizardFormEmulationTypeScummVM());
    ui.EmulationTypeComboBox->setItemText(2, LanguageSetup->WizardFormEmulationTypeWindows());
    ui.EmulationTypeComboBox->setItemData(0, -1);
    ui.EmulationTypeComboBox->setItemData(1, -2);
    ui.EmulationTypeComboBox->setItemData(2, -3);
    ui.ListScummGamesLabel->setText(LanguageSetup->WizardFormEmulationTypeListScummVMGames());
    ui.ShowInfoLabel->setText(LanguageSetup->WizardFormMainInfo());
    ui.InfoLabelInstallationSupport->setText(LanguageSetup->WizardFormInstallationSupportInfo1());
    ui.InfoLabelInstallationSupportLink->setText(LanguageSetup->WizardFormInstallationSupportInfo2());
    ui.WizardModeLabel->setText(LanguageSetup->WizardFormWizardMode());
    while (ui.WizardModeComboBox->count() < 3) ui.WizardModeComboBox->addItem(QString());
    ui.WizardModeComboBox->setItemText(0, LanguageSetup->WizardFormWizardModeAlwaysAutomatically());
    ui.WizardModeComboBox->setItemText(1, LanguageSetup->WizardFormWizardModeAutomaticallyIfAutoSetupTemplateExists());
    ui.WizardModeComboBox->setItemText(2, LanguageSetup->WizardFormWizardModeAlwaysAllPages());

    for (int I = 0; I < PrgSetup->WindowsBasedEmulatorsNames()->Count(); I++)
        ui.EmulationTypeComboBox->addItem(Format(LanguageSetup->MenuProfileAddOtherBasedGameDialog(), {(*PrgSetup->WindowsBasedEmulatorsNames())[I]}), I);
    ui.EmulationTypeComboBox->setCurrentIndex(0);
    ui.WizardModeComboBox->setCurrentIndex(Max(0, Min(2, PrgSetup->LastWizardMode())));

    if (Trim(PrgSetup->ScummVMPath()).isEmpty()) ui.EmulationTypeComboBox->removeItem(1);
    ui.ListScummGamesLabel->setVisible(!Trim(PrgSetup->ScummVMPath()).isEmpty());
    if (ui.ListScummGamesLabel->isHidden()) {
        VclLayout::setBounds(ui.ShowInfoLabel, ui.ShowInfoLabel->x(), ui.ListScummGamesLabel->y(), ui.ShowInfoLabel->width(), ui.ShowInfoLabel->height());
    }

    SetLinkStyle(ui.ListScummGamesLabel);
    SetLinkStyle(ui.ShowInfoLabel);
    SetLinkStyle(ui.InfoLabelInstallationSupportLink);
}

int TWizardBaseFrame::GetEmulationType()
{
    if (ui.EmulationTypeComboBox->currentIndex() < 0) return -1;
    return ui.EmulationTypeComboBox->itemData(ui.EmulationTypeComboBox->currentIndex()).toInt();
}

void TWizardBaseFrame::SetEmulationType(int I)
{
    for (int J = 0; J < ui.EmulationTypeComboBox->count(); J++) if (ui.EmulationTypeComboBox->itemData(J).toInt() == I) {
        ui.EmulationTypeComboBox->setCurrentIndex(J);
        break;
    }
}

void TWizardBaseFrame::WriteDataToGame(TGame* Game)
{
    QString S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(Game->Name()) + PathDelim;
    int I = 0;
    while ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(MakeAbsPath(S, PrgSetup->BaseDir()))) {
        I++;
        S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(Game->Name()) + IntToStr(I) + PathDelim;
    }
    Game->SetCaptureFolder(MakeRelPath(S, PrgSetup->BaseDir()));
    CreateDir(MakeAbsPath(Game->CaptureFolder(), PrgSetup->BaseDir()));
}

void TWizardBaseFrame::ButtonWork(QObject* Sender)
{
    switch (Sender->property("tag").toInt()) {
    case 0: ShowListScummVMGamesDialog(this); break;
    case 1:
        ShowTextViewerDialog(
            this,
            LanguageSetup->WizardFormMainInfo(),
            LanguageSetup->WizardFormMainInfo1() + "\n\n" + LanguageSetup->WizardFormMainInfo2() + "\n\n" + LanguageSetup->WizardFormMainInfo3()
        );
        break;
    case 2:
        if (auto *F = qobject_cast<TWizardForm *>(FDialog)) F->OpenInstallationSupportDialog = true;
        // PostMessage(FDialog.Handle,WM_CLOSE,0,0);
        if (FDialog != nullptr) QMetaObject::invokeMethod(FDialog, "close", Qt::QueuedConnection);
        break;
    }
}
