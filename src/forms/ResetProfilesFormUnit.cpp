// C++ port of ResetProfilesFormUnit.pas (implementation section).
#include "ResetProfilesFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "GameDBToolsUnit.h"
#include "ModernProfileEditorFormUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "Process.h"

TResetProfilesForm* ResetProfilesForm = nullptr;

static void AddCheckListItem(QListWidget* ListBox, const QString& S)
{
    QListWidgetItem* It = new QListWidgetItem(S, ListBox);
    It->setFlags(It->flags() | Qt::ItemIsUserCheckable);
    It->setCheckState(Qt::Unchecked);
}

TResetProfilesForm::TResetProfilesForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.SelectAllButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectAllButton); });
    connect(ui.SelectNoneButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectNoneButton); });
    connect(ui.SelectGenreButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectGenreButton); });
    connect(ui.SelectAllButton2, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectAllButton2); });
    connect(ui.SelectNoneButton2, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectNoneButton2); });
    connect(ui.ComboBoxTemplate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { ComboBoxChange(ui.ComboBoxTemplate); });
    connect(ui.ComboBoxAutoSetup, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { ComboBoxChange(ui.ComboBoxAutoSetup); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);  // OnCreate
}

TResetProfilesForm::~TResetProfilesForm()
{
    FormDestroy(this);  // OnDestroy
}

void TResetProfilesForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TResetProfilesForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    TemplateDB = new TGameDB(PrgDataDir() + TemplateSubDir, false);
    AutoSetupDB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);

    ui.PageControl->setCurrentIndex(0);
}

void TResetProfilesForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    delete TemplateDB; TemplateDB = nullptr;
    delete AutoSetupDB; AutoSetupDB = nullptr;
}

void TResetProfilesForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    setWindowTitle(LanguageSetup->ResetProfiles());
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.TabSheet1), LanguageSetup->ResetProfilesProfiles());
    ui.InfoLabel1->setText(LanguageSetup->ResetProfilesProfilesInfo());
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.TabSheet2), LanguageSetup->ResetProfilesSettings());
    ui.InfoLabel2->setText(LanguageSetup->ResetProfilesSettingsInfo());
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.TabSheet3), LanguageSetup->ResetProfilesTemplate());
    ui.RadioButtonTemplate->setText(LanguageSetup->ResetProfilesTemplateTemplate());
    ui.RadioButtonAutoSetup->setText(LanguageSetup->ResetProfilesTemplateAutoSetup());

    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.SelectAllButton->setText(LanguageSetup->All());
    ui.SelectNoneButton->setText(LanguageSetup->None());
    ui.SelectGenreButton->setText(LanguageSetup->GameBy());
    ui.SelectAllButton2->setText(LanguageSetup->All());
    ui.SelectNoneButton2->setText(LanguageSetup->None());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    BuildCheckList(ui.ListBox, GameDB, false, false);
    BuildSelectPopupMenu(ui.PopupMenu, GameDB, [this](QObject* S) { SelectButtonClick(S); }, false);
    BuildSettingsList();
    BuildTemplateList();
}

void TResetProfilesForm::SelectButtonClick(QObject* Sender)
{
    int I;
    QPoint P;

    if (qobject_cast<QPushButton*>(Sender) != nullptr) {
        const int Tag = Sender->property("tag").toInt();
        switch (Tag) {
            case 0:
            case 1:
                for (I = 0; I < ui.ListBox->count(); I++) ui.ListBox->item(I)->setCheckState((Tag == 0) ? Qt::Checked : Qt::Unchecked);
                break;
            case 2:
                // SelectGenreButton lies on TabSheet1: map its position from its parent
                P = ui.SelectGenreButton->parentWidget()->mapToGlobal(QPoint(ui.SelectGenreButton->x(), ui.SelectGenreButton->y()));
                ui.PopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
                break;
            case 3:
            case 4:
                for (I = 0; I < ui.ListBox2->count(); I++) ui.ListBox2->item(I)->setCheckState((Tag == 3) ? Qt::Checked : Qt::Unchecked);
                break;
        }
        return;
    }

    SelectGamesByPopupMenu(Sender, ui.ListBox);
}

void TResetProfilesForm::BuildSettingsList()
{
    int I;
    TGame* DefaultTemplate;
    QTreeWidgetItem* N;
    QString S;

    ModernProfileEditorForm = new TModernProfileEditorForm(this);
    ModernProfileEditorForm->GameDB = GameDB;
    DefaultTemplate = new TGame(PrgSetup);
    ModernProfileEditorForm->SearchLinkFile = SearchLinkFile;
    ModernProfileEditorForm->Game = DefaultTemplate;
    ModernProfileEditorForm->InitGUI();
    for (I = 0; I < ModernProfileEditorForm->FrameList.size(); I++) if (ModernProfileEditorForm->FrameList[I].AllowDefaultValueReset) {
        N = ModernProfileEditorForm->FrameList[I].TreeNode;
        S = "";
        while (N != nullptr) {
            if (S != "") S = " - " + S;
            S = N->text(0) + S;
            N = N->parent();
        }
        AddCheckListItem(ui.ListBox2, S);
    }
    delete DefaultTemplate;
    delete ModernProfileEditorForm;
    ModernProfileEditorForm = nullptr;
}

void TResetProfilesForm::BuildTemplateList()
{
    int I;
    {
        TStringList St;
        St.AddObject(LanguageSetup->TemplateFormDefault(), nullptr);
        for (I = 0; I < TemplateDB->Count(); I++) St.AddObject(TemplateDB->Game(I)->CacheName, TemplateDB->Game(I));
        St.Sort();
        const QSignalBlocker Block1(ui.ComboBoxTemplate);
        for (I = 0; I < St.Count(); I++) ui.ComboBoxTemplate->addItem(St[I], QVariant::fromValue<void*>(St.Objects(I)));

        St.Clear();

        for (I = 0; I < AutoSetupDB->Count(); I++) St.AddObject(AutoSetupDB->Game(I)->CacheName, AutoSetupDB->Game(I));
        St.Sort();
        const QSignalBlocker Block2(ui.ComboBoxAutoSetup);
        for (I = 0; I < St.Count(); I++) ui.ComboBoxAutoSetup->addItem(St[I], QVariant::fromValue<void*>(St.Objects(I)));
    }

    const QSignalBlocker Block1(ui.ComboBoxTemplate);
    const QSignalBlocker Block2(ui.ComboBoxAutoSetup);
    for (I = 0; I < ui.ComboBoxTemplate->count(); I++) if (ui.ComboBoxTemplate->itemData(I).value<void*>() == nullptr) {
        ui.ComboBoxTemplate->setCurrentIndex(I); break;
    }
    if (ui.ComboBoxAutoSetup->count() > 0) ui.ComboBoxAutoSetup->setCurrentIndex(0);
}

void TResetProfilesForm::ComboBoxChange(QObject* Sender)
{
    ui.RadioButtonTemplate->setChecked(Sender == ui.ComboBoxTemplate);
    ui.RadioButtonAutoSetup->setChecked(Sender == ui.ComboBoxAutoSetup);
}

void TResetProfilesForm::ResetGame(TGame* Game, TGame* Template)
{
    int I, C;

    ModernProfileEditorForm = new TModernProfileEditorForm(this);
    ModernProfileEditorForm->GameDB = GameDB;
    ModernProfileEditorForm->Game = Game;
    ModernProfileEditorForm->SilentMode = true;
    ModernProfileEditorForm->SearchLinkFile = SearchLinkFile;
    ModernProfileEditorForm->FormShow(this);
    C = 0;
    for (I = 0; I < ModernProfileEditorForm->FrameList.size(); I++) if (ModernProfileEditorForm->FrameList[I].AllowDefaultValueReset) {
        if (ui.ListBox2->item(C)->checkState() == Qt::Checked) {
            if (ModernProfileEditorForm->FrameList[I].ResetToDefault) ModernProfileEditorForm->FrameList[I].ResetToDefault(this, Template); else ModernProfileEditorForm->FrameList[I].IFrame->SetGame(Template, true);
        }
        C++;
    }
    ModernProfileEditorForm->OKButtonClick(ModernProfileEditorForm);
    delete ModernProfileEditorForm;
    ModernProfileEditorForm = nullptr;
}

void TResetProfilesForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    TGame *Template, *DefaultTemplate;

    if (ui.RadioButtonTemplate->isChecked()) {
        if (ui.ComboBoxTemplate->currentIndex() < 0) return;
        Template = static_cast<TGame*>(ui.ComboBoxTemplate->itemData(ui.ComboBoxTemplate->currentIndex()).value<void*>());
    } else {
        if (ui.ComboBoxAutoSetup->currentIndex() < 0) return;
        Template = static_cast<TGame*>(ui.ComboBoxAutoSetup->itemData(ui.ComboBoxAutoSetup->currentIndex()).value<void*>());
    }
    if (Template == nullptr) {
        DefaultTemplate = new TGame(PrgSetup);
        Template = DefaultTemplate;
    } else {
        DefaultTemplate = nullptr;
    }
    for (I = 0; I < ui.ListBox->count(); I++) if (ui.ListBox->item(I)->checkState() == Qt::Checked)
        ResetGame(static_cast<TGame*>(ui.ListBox->item(I)->data(Qt::UserRole).value<void*>()), Template);
    delete DefaultTemplate;
}

void TResetProfilesForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_FileOptionsResetProfiles);
}

void TResetProfilesForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

/* global */

bool ShowResetProfilesDialog(QWidget* AOwner, TGameDB* AGameDB, TLinkFile* ASearchLinkFile)
{
    bool result;
    ResetProfilesForm = new TResetProfilesForm(AOwner);
    ResetProfilesForm->GameDB = AGameDB;
    ResetProfilesForm->SearchLinkFile = ASearchLinkFile;
    result = (ResetProfilesForm->ShowModal() == mrOk);
    delete ResetProfilesForm;
    ResetProfilesForm = nullptr;
    return result;
}
