// C++ port of SetupFrameGamesListTreeAppearanceUnit.pas (implementation section).
#include "SetupFrameGamesListTreeAppearanceUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"
#include "TextEditPopupUnit.h"

namespace {
// TColorBox.Selected:=C (TColorToQColor also resolves system colours like clWindowText)
void SetColorBoxSelected(TColorBoxWidget *Box, TColor C)
{
    Box->setSelected(TColorToQColor(C));
}
} // namespace

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameGamesListTreeAppearance::TSetupFrameGamesListTreeAppearance(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DFM event wiring
    connect(ui.TreeViewBackgroundColorBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { TreeViewBackgroundColorBoxChange(ui.TreeViewBackgroundColorBox); });
    connect(ui.UserKeysList, &QPushButton::clicked, this, [this]() { UserKeysListClick(ui.UserKeysList); });
    connect(ui.PopupMenu, &QMenu::aboutToShow, this, [this]() { PopupMenuPopup(ui.PopupMenu); });
}

TSetupFrameGamesListTreeAppearance::~TSetupFrameGamesListTreeAppearance() = default;

// { TSetupFrameGamesListTreeAppearance }

QString TSetupFrameGamesListTreeAppearance::GetName()
{
    return LanguageSetup->SetupFormListViewSheet3();
}

void TSetupFrameGamesListTreeAppearance::InitGUIAndLoadSetup(TInitData &InitData)
{
    QString S;
    TColor C;
    TStringList *St;
    int I;
    /* NoFlicker(TreeViewGroupsEdit); - otherwise the text will disappear */
    SetRichEditPopup(ui.TreeViewGroupsEdit);
    ui.ReselectFilterCheckBox->setChecked(PrgSetup->RestoreFilter());
    S = Trim(PrgSetup->GamesTreeViewBackground());
    if (S.isEmpty()) ui.TreeViewBackgroundRadioButton1->setChecked(true); else {
        try {
            C = StringToColor(S); ui.TreeViewBackgroundRadioButton2->setChecked(true); SetColorBoxSelected(ui.TreeViewBackgroundColorBox, C);
        } catch (...) {
            ui.TreeViewBackgroundRadioButton1->setChecked(true);
        }
    }
    S = Trim(PrgSetup->GamesTreeViewFontColor());
    if (S.isEmpty()) SetColorBoxSelected(ui.TreeViewFontColorBox, clWindowText); else {
        try { SetColorBoxSelected(ui.TreeViewFontColorBox, StringToColor(S)); } catch (...) { SetColorBoxSelected(ui.TreeViewFontColorBox, clWindowText); }
    }
    ui.TreeViewFontSizeEdit->setValue(PrgSetup->GamesTreeViewFontSize());
    S = PrgSetup->DefaultTreeFilter();
    while (Length(S) < 8) S = S + "1";
    for (I = 0; I <= 7 && I < ui.TreeViewDefaultGroups->count(); I++) ui.TreeViewDefaultGroups->item(I)->setCheckState((S[I] != '0') ? Qt::Checked : Qt::Unchecked);
    St = StringToStringList(PrgSetup->UserGroups());
    for (I = 0; I < St->Count(); I++) ui.TreeViewGroupsEdit->append((*St)[I]);
    delete St;
    FGameDB = InitData.GameDB;
}

void TSetupFrameGamesListTreeAppearance::BeforeChangeLanguage()
{
}

void TSetupFrameGamesListTreeAppearance::LoadLanguage()
{
    bool GermanColorNames;
    QString S;
    int I;
    QListWidget *L = ui.TreeViewDefaultGroups;
    ui.ReselectFilterCheckBox->setText(LanguageSetup->SetupFormStartRestoreFilter());
    GermanColorNames = (ExtUpperCase(ExtractFileName(LanguageSetup->SetupFile())) == "DEUTSCH.INI") || (ExtUpperCase(ExtractFileName(LanguageSetup->SetupFile())) == "GERMAN.INI");
    Q_UNUSED(GermanColorNames); // cbPrettyNames: TColorBoxWidget always shows pretty (English) colour names
    ui.TreeViewBackgroundRadioButton1->setText(LanguageSetup->SetupFormBackgroundColorDefault());
    ui.TreeViewBackgroundRadioButton2->setText(LanguageSetup->SetupFormBackgroundColorColor());
    ui.TreeViewFontSizeLabel->setText(LanguageSetup->SetupFormFontSize());
    ui.TreeViewFontColorLabel->setText(LanguageSetup->SetupFormFontColor());
    ui.PredefinedGroupsLabel->setText(LanguageSetup->SetupFormTreeViewDefaultGroupLabel());
    ui.TreeViewGroupsLabel->setText(LanguageSetup->SetupFormTreeViewGroupLabel());
    ui.UserKeysList->setText(LanguageSetup->SetupFormTreeViewGroupAddButton());
    S.clear();
    for (I = 0; I < L->count(); I++) if (L->item(I)->checkState() == Qt::Checked) S = S + "1"; else S = S + "0";
    while (L->count() < 8) {
        auto *Item = new QListWidgetItem(QString(), L);
        Item->setFlags(Item->flags() | Qt::ItemIsUserCheckable);
        Item->setCheckState(Qt::Unchecked);
    }
    L->item(0)->setText(LanguageSetup->GameGenre());
    L->item(1)->setText(LanguageSetup->GameDeveloper());
    L->item(2)->setText(LanguageSetup->GamePublisher());
    L->item(3)->setText(LanguageSetup->GameYear());
    L->item(4)->setText(LanguageSetup->GameLanguage());
    L->item(5)->setText(LanguageSetup->GameLicense());
    L->item(6)->setText(LanguageSetup->GameEmulationType());
    L->item(7)->setText(LanguageSetup->GameRecentlyPlayed());
    for (I = 0; I <= 6 && I < Length(S); I++) L->item(I)->setCheckState((S[I] != '0') ? Qt::Checked : Qt::Unchecked);
    ui.TreeViewGroupsInfoLabel->setText(LanguageSetup->SetupFormTreeViewGroupInfoLabel());
    setProperty("HelpContext", ID_FileOptionsAppearanceTree);
}

void TSetupFrameGamesListTreeAppearance::DOSBoxDirChanged()
{
}

void TSetupFrameGamesListTreeAppearance::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameGamesListTreeAppearance::HideFrame()
{
}

void TSetupFrameGamesListTreeAppearance::RestoreDefaults()
{
    int I;
    ui.ReselectFilterCheckBox->setChecked(true);
    SetColorBoxSelected(ui.TreeViewBackgroundColorBox, clBlack);
    ui.TreeViewBackgroundRadioButton1->setChecked(true);
    ui.TreeViewFontSizeEdit->setValue(9);
    SetColorBoxSelected(ui.TreeViewFontColorBox, clBlack);
    for (I = 0; I < ui.TreeViewDefaultGroups->count(); I++) ui.TreeViewDefaultGroups->item(I)->setCheckState(Qt::Checked);
    ui.TreeViewGroupsEdit->clear();
}

void TSetupFrameGamesListTreeAppearance::SaveSetup()
{
    TStringList *St1, *St2;
    int I;
    QString S;
    PrgSetup->SetRestoreFilter(ui.ReselectFilterCheckBox->isChecked());
    if (ui.TreeViewBackgroundRadioButton1->isChecked()) PrgSetup->SetGamesTreeViewBackground(QString());
    if (ui.TreeViewBackgroundRadioButton2->isChecked()) PrgSetup->SetGamesTreeViewBackground(ColorToString(ui.TreeViewBackgroundColorBox->selectedTColor()));
    PrgSetup->SetGamesTreeViewFontSize(ui.TreeViewFontSizeEdit->value());
    PrgSetup->SetGamesTreeViewFontColor(ColorToString(ui.TreeViewFontColorBox->selectedTColor()));
    S.clear();
    for (I = 0; I < ui.TreeViewDefaultGroups->count(); I++) if (ui.TreeViewDefaultGroups->item(I)->checkState() == Qt::Checked) S = S + "1"; else S = S + "0";
    PrgSetup->SetDefaultTreeFilter(S);
    St1 = new TStringList;
    St2 = new TStringList;
    const QStringList Lines = ui.TreeViewGroupsEdit->toPlainText().split('\n');
    for (I = 0; I < Lines.count(); I++) if (St2->IndexOf(ExtUpperCase(Lines[I])) < 0) {
        St1->Add(Lines[I]);
        St2->Add(ExtUpperCase(Lines[I]));
    }
    PrgSetup->SetUserGroups(StringListToString(St1));
    delete St1;
    delete St2;
}

void TSetupFrameGamesListTreeAppearance::TreeViewBackgroundColorBoxChange(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.TreeViewBackgroundRadioButton2->setChecked(true);
}

void TSetupFrameGamesListTreeAppearance::UserKeysListClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    QPoint P;
    P = mapToGlobal(QPoint(ui.UserKeysList->x() + 5, ui.UserKeysList->y() + 5));
    ui.PopupMenu->popup(P);
}

void TSetupFrameGamesListTreeAppearance::PopupMenuPopup(QObject *Sender)
{
    Q_UNUSED(Sender);
    int I;
    TStringList *St, *St2;
    QAction *M;
    ui.PopupMenu->clear();
    St2 = new TStringList;
    const QStringList Lines = ui.TreeViewGroupsEdit->toPlainText().split('\n');
    for (I = 0; I < Lines.count(); I++) St2->Add(ExtUpperCase(Lines[I]));
    St = FGameDB->GetUserKeys();
    for (I = 0; I < St->Count(); I++) if (St2->IndexOf(ExtUpperCase((*St)[I])) < 0) {
        if (Trim(ExtUpperCase((*St)[I])) == "LICENSE") continue;
        M = new QAction(MaskUnderlineAmpersand((*St)[I]), ui.PopupMenu);
        connect(M, &QAction::triggered, this, [this, M]() { PopupMenuWork(M); });
        ui.PopupMenu->addAction(M);
    }
    delete St;
    delete St2;
}

void TSetupFrameGamesListTreeAppearance::PopupMenuWork(QObject *Sender)
{
    int I;
    ui.TreeViewGroupsEdit->append(RemoveUnderline(qobject_cast<QAction *>(Sender)->text()));
    QStringList Lines = ui.TreeViewGroupsEdit->toPlainText().split('\n');
    const int OldCount = Lines.count();
    I = 0;
    while (I < Lines.count()) if (Trim(Lines[I]).isEmpty()) Lines.removeAt(I); else I++;
    if (Lines.count() != OldCount) ui.TreeViewGroupsEdit->setPlainText(Lines.join("\n"));
}
