// C++ port of DragNDropErrorFormUnit.pas (implementation section).
#include "DragNDropErrorFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"

TDragNDropErrorForm* DragNDropErrorForm = nullptr;

TDragNDropErrorForm::TDragNDropErrorForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    FormCreate(this);   // OnCreate
}

TDragNDropErrorForm::~TDragNDropErrorForm() = default;

void TDragNDropErrorForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

void TDragNDropErrorForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    setWindowTitle(LanguageSetup->DragDropErrorInfoDialogCaption());
    ui.InfoLabel->setText(LanguageSetup->DragDropErrorInfoDialogLabel());
    ui.OKButton->setText(LanguageSetup->OK());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
}

void TDragNDropErrorForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (Errors == nullptr) return;
    for (int I = 0; I < Errors->Count(); I++) ui.ErrorListBox->appendPlainText((*Errors)[I]);
}

// { global }

void ShowDragNDropErrorDialog(QWidget* AOwner, TStringList* AErrors)
{
    DragNDropErrorForm = new TDragNDropErrorForm(AOwner);
    DragNDropErrorForm->Errors = AErrors;
    DragNDropErrorForm->ShowModal();
    delete DragNDropErrorForm;
    DragNDropErrorForm = nullptr;
}
