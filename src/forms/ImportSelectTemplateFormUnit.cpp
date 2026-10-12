// C++ port of ImportSelectTemplateFormUnit.pas (implementation section).
#include "ImportSelectTemplateFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"

TImportSelectTemplateForm* ImportSelectTemplateForm = nullptr;

TImportSelectTemplateForm::TImportSelectTemplateForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    FormCreate(this);   // OnCreate
}

TImportSelectTemplateForm::~TImportSelectTemplateForm() = default;

void TImportSelectTemplateForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

void TImportSelectTemplateForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    setWindowTitle(LanguageSetup->SelectAutoSetupTemplate());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);

    SelectedTemplate = -1;
}

void TImportSelectTemplateForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (!GameFolder.isEmpty())
        ui.InfoLabel->setText(Format(LanguageSetup->SelectAutoSetupTemplateScanFolder(), {GameFolder}));
    else
        ui.InfoLabel->setText(LanguageSetup->SelectAutoSetupTemplateImportZip());

    for (int I = 0; I < MatchingTemplates->Count(); I++)
        ui.ComboBox->addItem(TemplateDB->Game(MatchingTemplates->ObjectAsInt(I))->Name());
    ui.ComboBox->setCurrentIndex(0);
}

void TImportSelectTemplateForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    SelectedTemplate = ui.ComboBox->currentIndex();
}

// { global }

int ShowSelectTemplateDialog(QWidget* AOwner, const QString& GameFolder, TGameDB* TemplateDB, TStringList* MatchingTemplates)
{
    int result = -1;
    if (MatchingTemplates->Count() == 0) return result;
    if (MatchingTemplates->Count() == 1) { result = 0; return result; }

    ImportSelectTemplateForm = new TImportSelectTemplateForm(AOwner);
    ImportSelectTemplateForm->TemplateDB = TemplateDB;
    ImportSelectTemplateForm->GameFolder = GameFolder;
    ImportSelectTemplateForm->MatchingTemplates = MatchingTemplates;
    if (ImportSelectTemplateForm->ShowModal() == mrOk) result = ImportSelectTemplateForm->SelectedTemplate;
    delete ImportSelectTemplateForm;
    ImportSelectTemplateForm = nullptr;
    return result;
}
