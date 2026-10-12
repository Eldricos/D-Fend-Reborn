// C++ port of SelectProfilesFormUnit.pas (implementation section).
#include "SelectProfilesFormUnit.h"
#include "LanguageSetupUnit.h"
#include "GameDBToolsUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"

TSelectProfilesForm* SelectProfilesForm = nullptr;

TSelectProfilesForm::TSelectProfilesForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.SelectAllButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectAllButton); });
    connect(ui.SelectNoneButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectNoneButton); });
    connect(ui.SelectGenreButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectGenreButton); });
    FormCreate(this);   // OnCreate
}

TSelectProfilesForm::~TSelectProfilesForm() = default;

void TSelectProfilesForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

void TSelectProfilesForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    setWindowTitle(LanguageSetup->TemplateFormNewFromProfilesCaption());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.SelectAllButton->setText(LanguageSetup->All());
    ui.SelectNoneButton->setText(LanguageSetup->None());
    ui.SelectGenreButton->setText(LanguageSetup->GameBy());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
}

void TSelectProfilesForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    BuildCheckList(ui.ListBox, GameDB, false, false);
    BuildSelectPopupMenu(ui.PopupMenu, GameDB, [this](QObject* S) { SelectButtonClick(S); }, false);
}

void TSelectProfilesForm::SelectButtonClick(QObject* Sender)
{
    if (qobject_cast<QPushButton*>(Sender) != nullptr) {
        switch (Sender->property("tag").toInt()) {
            case 0:
            case 1:
                for (int I = 0; I < ui.ListBox->count(); I++)
                    ui.ListBox->item(I)->setCheckState((Sender->property("tag").toInt() == 0) ? Qt::Checked : Qt::Unchecked);
                break;
            case 2: {
                const QPoint P = ui.SelectGenreButton->mapToGlobal(QPoint(0, 0));
                ui.PopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
                break;
            }
        }
        return;
    }

    SelectGamesByPopupMenu(Sender, ui.ListBox);
}

// { global }

bool ShowSelectProfilesDialog(QWidget* AOwner, TGameDB* AGameDB, TList*& L)
{
    SelectProfilesForm = new TSelectProfilesForm(AOwner);
    SelectProfilesForm->GameDB = AGameDB;
    const bool result = (SelectProfilesForm->ShowModal() == mrOk);
    if (result) {
        L = new TList;
        for (int I = 0; I < SelectProfilesForm->ui.ListBox->count(); I++) {
            QListWidgetItem* Item = SelectProfilesForm->ui.ListBox->item(I);
            if (Item->checkState() == Qt::Checked) L->append(Item->data(Qt::UserRole).value<void*>());
        }
    }
    delete SelectProfilesForm;
    SelectProfilesForm = nullptr;
    return result;
}
