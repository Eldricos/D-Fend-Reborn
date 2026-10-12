// C++ port of SetupFrameSecurityUnit.pas (implementation section).
#include "SetupFrameSecurityUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameSecurity::TSetupFrameSecurity(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TSetupFrameSecurity::~TSetupFrameSecurity() = default;

namespace {
// while RecycleBinListBox.Items.Count<7 do RecycleBinListBox.Items.Add('');
void FillCheckList(QListWidget *L, int Count)
{
    while (L->count() < Count) {
        auto *Item = new QListWidgetItem(QString(), L);
        Item->setFlags(Item->flags() | Qt::ItemIsUserCheckable);
        Item->setCheckState(Qt::Unchecked);
    }
}
bool IsChecked(QListWidget *L, int I) { return L->item(I)->checkState() == Qt::Checked; }
void SetChecked(QListWidget *L, int I, bool B) { L->item(I)->setCheckState(B ? Qt::Checked : Qt::Unchecked); }
} // namespace

// { TSetupFrameSecurity }

QString TSetupFrameSecurity::GetName()
{
    return LanguageSetup->SetupFormSecuritySheet();
}

void TSetupFrameSecurity::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    QString S;
    QListWidget *L = ui.RecycleBinListBox;
    ui.AskBeforeDeleteCheckBox->setChecked(PrgSetup->AskBeforeDelete());
    ui.DeleteProectionCheckBox->setChecked(PrgSetup->DeleteOnlyInBaseDir());
    ui.UseChecksumsCheckBox->setChecked(PrgSetup->UseCheckSumsForProfiles());
    FillCheckList(L, 7);
    S = Copy(PrgSetup->DeleteToRecycleBin(), 1, 7);
    S = S + Copy("1011110", Length(S) + 1, MaxInt);
    SetChecked(L, 0, S[int(ftProfile)] == '1');
    SetChecked(L, 1, S[int(ftMediaViewer)] == '1');
    SetChecked(L, 2, S[int(ftDataViewer)] == '1');
    SetChecked(L, 3, S[int(ftUninstall)] == '1');
    SetChecked(L, 4, S[int(ftQuickStart)] == '1');
    SetChecked(L, 5, S[int(ftZipOperation)] == '1');
    SetChecked(L, 6, S[int(ftTemp)] == '1');
    ui.OfferRunAsAdminCheckBox->setChecked(PrgSetup->OfferRunAsAdmin());
}

void TSetupFrameSecurity::BeforeChangeLanguage()
{
}

void TSetupFrameSecurity::LoadLanguage()
{
    QListWidget *L = ui.RecycleBinListBox;
    ui.AskBeforeDeleteCheckBox->setText(LanguageSetup->SetupFormAskBeforeDelete());
    ui.DeleteProectionCheckBox->setText(LanguageSetup->SetupFormDeleteOnlyInBaseDir());
    ui.DeleteProectionLabel->setText(LanguageSetup->SetupFormDeleteOnlyInBaseDirLabel());
    ui.UseChecksumsCheckBox->setText(LanguageSetup->SetupFormUseChecksums());
    ui.UseChecksumsLabel->setText(LanguageSetup->SetupFormUseChecksumsInfo());
    ui.RecycleBinLabel->setText(LanguageSetup->SetupFormRecycleBin());
    FillCheckList(L, 7);
    L->item(0)->setText(LanguageSetup->SetupFormRecycleBinProfile());
    L->item(1)->setText(LanguageSetup->SetupFormRecycleBinMediaViewer());
    L->item(2)->setText(LanguageSetup->SetupFormRecycleBinDataViewer());
    L->item(3)->setText(LanguageSetup->SetupFormRecycleBinUninstall());
    L->item(4)->setText(LanguageSetup->SetupFormRecycleBinQuickStart());
    L->item(5)->setText(LanguageSetup->SetupFormRecycleBinZipOperation());
    L->item(6)->setText(LanguageSetup->SetupFormRecycleBinTemp());
    ui.OfferRunAsAdminCheckBox->setText(LanguageSetup->SetupFormOfferRunAsAdmin());
    setProperty("HelpContext", ID_FileOptionsSecurity);
}

void TSetupFrameSecurity::DOSBoxDirChanged()
{
}

void TSetupFrameSecurity::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameSecurity::HideFrame()
{
}

void TSetupFrameSecurity::RestoreDefaults()
{
    int I;
    QListWidget *L = ui.RecycleBinListBox;
    ui.AskBeforeDeleteCheckBox->setChecked(true);
    ui.DeleteProectionCheckBox->setChecked(true);
    ui.UseChecksumsCheckBox->setChecked(true);
    FillCheckList(L, 7);
    for (I = 0; I <= 4; I++) SetChecked(L, I, true);
    for (I = 5; I <= 6; I++) SetChecked(L, I, false);
    ui.OfferRunAsAdminCheckBox->setChecked(false);
}

void TSetupFrameSecurity::SaveSetup()
{
    QString S;
    QListWidget *L = ui.RecycleBinListBox;
    PrgSetup->SetAskBeforeDelete(ui.AskBeforeDeleteCheckBox->isChecked());
    PrgSetup->SetDeleteOnlyInBaseDir(ui.DeleteProectionCheckBox->isChecked());
    PrgSetup->SetUseCheckSumsForProfiles(ui.UseChecksumsCheckBox->isChecked());
    FillCheckList(L, 7);
    S = "1111111";
    if (!IsChecked(L, 0)) S[int(ftProfile)] = '0';
    if (!IsChecked(L, 1)) S[int(ftMediaViewer)] = '0';
    if (!IsChecked(L, 2)) S[int(ftDataViewer)] = '0';
    if (!IsChecked(L, 3)) S[int(ftUninstall)] = '0';
    if (!IsChecked(L, 4)) S[int(ftQuickStart)] = '0';
    if (!IsChecked(L, 5)) S[int(ftZipOperation)] = '0';
    if (!IsChecked(L, 6)) S[int(ftTemp)] = '0';
    PrgSetup->SetDeleteToRecycleBin(S);
    PrgSetup->SetOfferRunAsAdmin(ui.OfferRunAsAdminCheckBox->isChecked());
}
