// C++ port of CreateXMLFormUnit.pas (implementation section).
#include "CreateXMLFormUnit.h"
#include "LanguageSetupUnit.h"
#include "GameDBToolsUnit.h"
#include "CommonTools.h"
#include "SimpleXMLUnit.h"
#include "IconLoaderUnit.h"

TCreateXMLForm* CreateXMLForm = nullptr;

TCreateXMLForm::TCreateXMLForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM: object SaveDialog: TSaveDialog
    SaveDialog = new TSaveDialog(this);
    SaveDialog->DefaultExt = "xml";
    SaveDialog->Filter = "XML files (*.xml)|*.xml|All files (*.*)|*.*";
    SaveDialog->Title = "Save profiles to xml file";

    connect(ui.SelectFileButton, &QToolButton::clicked, this, [this]() { SelectFileButtonClick(ui.SelectFileButton); });
    // SelectAllButton has no OnClick in the DFM (as in the original)
    connect(ui.SelectNoneButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectNoneButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.SelectGenreButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectGenreButton); });
    FormCreate(this);   // OnCreate
}

TCreateXMLForm::~TCreateXMLForm()
{
    delete SaveDialog;
}

void TCreateXMLForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

void TCreateXMLForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    /*Caption:=LanguageSetup.CreateConfForm;
    InfoLabel.Caption:=LanguageSetup.CreateConfFormInfo;*/
    ui.InfoLabel->setText("Please select the profiles for which shall be included to the xml file:");
    ui.SelectAllButton->setText(LanguageSetup->All());
    ui.SelectNoneButton->setText(LanguageSetup->None());
    ui.SelectGenreButton->setText(LanguageSetup->GameBy());
    /*FileTypeRadioGroup
    FileEdit.EditLabel.Caption:=LanguageSetup.CreateConfFormSelectFolder;*/
    ui.SelectFileButton->setToolTip(LanguageSetup->ChooseFile());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());

    /*SaveDialog.Caption:=LanguageSetup.
    SaveDialog.Filter:=LanguageSetup.*/
    UserIconLoader->DialogImage(DI_SelectFile, ui.SelectFileButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
}

void TCreateXMLForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    BuildCheckList(ui.ListBox, GameDB, true, true);
    BuildSelectPopupMenu(ui.PopupMenu, GameDB, [this](QObject* S) { SelectButtonClick(S); }, true, true);
}

void TCreateXMLForm::SelectButtonClick(QObject* Sender)
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

void TCreateXMLForm::SelectFileButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S = Trim(ui.FileEdit->text());
    if (S.isEmpty()) S = GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY); else S = ExtractFilePath(S);
    SaveDialog->InitialDir = S;
    if (SaveDialog->Execute()) ui.FileEdit->setText(SaveDialog->FileName);
}

void TCreateXMLForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    TList L;
    TSimpleXMLFileStyle FileStyle;

    for (int I = 0; I < ui.ListBox->count(); I++) {
        QListWidgetItem* Item = ui.ListBox->item(I);
        if (Item->checkState() == Qt::Checked) L.append(Item->data(Qt::UserRole).value<void*>());
    }
    if (ui.FileTypeRadioGroup->itemIndex() == 0) FileStyle = sxfsDBGL; else FileStyle = sxfsDOG;
    if (!WriteProfilesToXML(&L, ui.FileEdit->text(), FileStyle)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {ui.FileEdit->text()}), mtError, mbOK, 0);
    }
}

// { global }

bool ExportXMLFiles(QWidget* AOwner, TGameDB* AGameDB)
{
    CreateXMLForm = new TCreateXMLForm(AOwner);
    CreateXMLForm->GameDB = AGameDB;
    const bool result = (CreateXMLForm->ShowModal() == mrOk);
    delete CreateXMLForm;
    CreateXMLForm = nullptr;
    return result;
}
