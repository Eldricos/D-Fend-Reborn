// C++ port of TextViewerFormUnit.pas (implementation section).
#include "TextViewerFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"

TTextViewerForm* TextViewerForm = nullptr;

TTextViewerForm::TTextViewerForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    FormCreate(this);   // OnCreate
}

TTextViewerForm::~TTextViewerForm() = default;

void TTextViewerForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

void TTextViewerForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    ui.CloseButton->setText(LanguageSetup->Close());

    UserIconLoader->DialogImage(DI_Close, ui.CloseButton);
}

void TTextViewerForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.Memo->setPlainText(Text);
}

// { global }

void ShowTextViewerDialog(QWidget* AOwner, const QString& ATitle, const QString& AText)
{
    TextViewerForm = new TTextViewerForm(AOwner);
    TextViewerForm->setWindowTitle(ATitle);
    TextViewerForm->Text = AText;
    TextViewerForm->ShowModal();
    delete TextViewerForm;
    TextViewerForm = nullptr;
}
