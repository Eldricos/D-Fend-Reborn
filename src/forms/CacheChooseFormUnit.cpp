// C++ port of CacheChooseFormUnit.pas (implementation section).
#include "CacheChooseFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"

TCacheChooseForm* CacheChooseForm = nullptr;

TCacheChooseForm::TCacheChooseForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    FormCreate(this);   // OnCreate
}

TCacheChooseForm::~TCacheChooseForm() = default;

void TCacheChooseForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

void TCacheChooseForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    ui.OKButton->setText(LanguageSetup->OK());

    RestoreDeletedMode = false;

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
}

void TCacheChooseForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (RestoreDeletedMode) {
        setWindowTitle(LanguageSetup->RestoreDeletedCaption());
        ui.InfoLabel->setText(LanguageSetup->RestoreDeletedInfo());
    } else {
        setWindowTitle(LanguageSetup->CacheChooseCaption());
        ui.InfoLabel->setText(LanguageSetup->CacheChooseInfo());
    }

    // ProfileListBox.Items.AddStrings(ProfileList) (copies the objects, too)
    for (int I = 0; I < ProfileList->Count(); I++) {
        QListWidgetItem* Item = new QListWidgetItem((*ProfileList)[I], ui.ProfileListBox);
        Item->setData(Qt::UserRole, QVariant::fromValue<void*>(ProfileList->Objects(I)));
        Item->setFlags(Item->flags() | Qt::ItemIsUserCheckable);
        Item->setCheckState(Qt::Unchecked);
    }
    for (int I = 0; I < ui.ProfileListBox->count(); I++) ui.ProfileListBox->item(I)->setCheckState(Qt::Checked);
}

void TCacheChooseForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    for (int I = 0; I < ui.ProfileListBox->count(); I++) {
        QListWidgetItem* Item = ui.ProfileListBox->item(I);
        TGame* G = static_cast<TGame*>(Item->data(Qt::UserRole).value<void*>());
        if (RestoreDeletedMode) {
            if (Item->checkState() == Qt::Checked) {
                G->StoreAllValues();
                G->RenameINI(G->SetupFile());
            } else {
                GameDB->Delete(G);
            }
        } else {
            if (Item->checkState() == Qt::Checked) {
                G = static_cast<TGame*>(Item->data(Qt::UserRole).value<void*>());
                G->ReloadINI();
                G->LoadCache();
            } else {
                G->StoreAllValues();
            }
        }
    }
}

// { global }

void ShowCacheChooseDialog(QWidget* AOwner, TStringList* AProfileList, TGameDB* AGameDB)
{
    CacheChooseForm = new TCacheChooseForm(AOwner);
    CacheChooseForm->ProfileList = AProfileList;
    CacheChooseForm->GameDB = AGameDB;
    CacheChooseForm->ShowModal();
    delete CacheChooseForm;
    CacheChooseForm = nullptr;
}

void ShowRestoreDeletedDialog(QWidget* AOwner, TStringList* AProfileList, TGameDB* AGameDB)
{
    CacheChooseForm = new TCacheChooseForm(AOwner);
    CacheChooseForm->RestoreDeletedMode = true;
    CacheChooseForm->ProfileList = AProfileList;
    CacheChooseForm->GameDB = AGameDB;
    CacheChooseForm->ShowModal();
    delete CacheChooseForm;
    CacheChooseForm = nullptr;
}
