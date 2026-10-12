// C++ port of ListScummVMGamesFormUnit.pas (implementation section).
#include "ListScummVMGamesFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "ScummVMToolsUnit.h"
#include "IconLoaderUnit.h"

TListScummVMGamesForm* ListScummVMGamesForm = nullptr;

TListScummVMGamesForm::TListScummVMGamesForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    FormCreate(this);   // OnCreate
}

TListScummVMGamesForm::~TListScummVMGamesForm() = default;

void TListScummVMGamesForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=...; NoFlicker(ListBox); - not needed in Qt

    setWindowTitle(LanguageSetup->MenuHelpScummVMCompatibilityListCaption());
    ui.CloseButton->setText(LanguageSetup->Close());

    UserIconLoader->DialogImage(DI_Close, ui.CloseButton);

    if (ScummVMGamesList->Count() == 0) ScummVMGamesList->LoadListFromScummVM(true);
    ui.ListBox->addItems(ScummVMGamesList->DescriptionList()->ToQStringList());
    ui.ListBox->setSortingEnabled(true);   // ListBox.Sorted:=True
    ui.ListBox->sortItems();
}

// { global }

void ShowListScummVMGamesDialog(QWidget* AOwner)
{
    ListScummVMGamesForm = new TListScummVMGamesForm(AOwner);
    ListScummVMGamesForm->ShowModal();
    delete ListScummVMGamesForm;
    ListScummVMGamesForm = nullptr;
}
