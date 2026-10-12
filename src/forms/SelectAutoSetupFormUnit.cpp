// C++ port of SelectAutoSetupFormUnit.pas (implementation section).
#include "SelectAutoSetupFormUnit.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"

TSelectAutoSetupForm* SelectAutoSetupForm = nullptr;

TSelectAutoSetupForm::TSelectAutoSetupForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TSelectAutoSetupForm::~TSelectAutoSetupForm()
{
}

void TSelectAutoSetupForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (E->spontaneous() || FFormShown) return;
    FFormShown = true;
    FormShow(this);
}

void TSelectAutoSetupForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed

    setWindowTitle(LanguageSetup->ProfileEditorToolsResetToAutoSetupSelect());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());

    TStringList St;
    for (int I = 0; I < AutoSetupDB->Count(); I++) St.AddObject(AutoSetupDB->Game(I)->CacheName, intptr_t(I));
    St.Sort();
    for (int I = 0; I < St.Count(); I++) {
        auto *It = new QListWidgetItem(St[I], ui.ListBox);
        It->setData(Qt::UserRole, St.ObjectAsInt(I));   // Items.Objects
    }
}

// global

int ShowSelectAutoSetupDialog(QWidget* AOwner, TGameDB* AAutoSetupDB)
{
    int result;
    SelectAutoSetupForm = new TSelectAutoSetupForm(AOwner);
    SelectAutoSetupForm->AutoSetupDB = AAutoSetupDB;
    if (SelectAutoSetupForm->ShowModal() == mrOk && SelectAutoSetupForm->ui.ListBox->currentItem() != nullptr)
        result = SelectAutoSetupForm->ui.ListBox->currentItem()->data(Qt::UserRole).toInt();
    else
        result = -1;
    delete SelectAutoSetupForm;
    SelectAutoSetupForm = nullptr;
    return result;
}
