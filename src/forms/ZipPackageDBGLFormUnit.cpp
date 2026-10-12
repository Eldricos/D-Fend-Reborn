// C++ port of ZipPackageDBGLFormUnit.pas (implementation section).
#include "ZipPackageDBGLFormUnit.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "IconLoaderUnit.h"

TZipPackageDBGLForm *ZipPackageDBGLForm = nullptr;

static void SetBaseColor(QWidget *W, const QColor &C)
{
    QPalette P = W->palette();
    P.setColor(QPalette::Base, C);
    W->setPalette(P);
}

TZipPackageDBGLForm::TZipPackageDBGLForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    FormCreate(this);   // OnCreate
}

TZipPackageDBGLForm::~TZipPackageDBGLForm() = default;

void TZipPackageDBGLForm::FormCreate(QObject * /*Sender*/)
{
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    const QColor Color = palette().color(QPalette::Window);
    SetBaseColor(ui.TitleEdit, Color);
    SetBaseColor(ui.AuthorEdit, Color);
    SetBaseColor(ui.NotesMemo, Color);

    setWindowTitle(LanguageSetup->ImportDBGLPackage());
    ui.TitleEdit_EditLabel->setText(LanguageSetup->ImportDBGLPackageTitle());
    ui.AuthorEdit_EditLabel->setText(LanguageSetup->ImportDBGLPackageAuthor());
    ui.NotesLabel->setText(LanguageSetup->ImportDBGLPackageNotes());
    ui.GamesLabel->setText(LanguageSetup->ImportDBGLPackageGames());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
}

// global

bool ShowZipPackageDBGLDialog(QWidget *AOwner, const QString &Title, const QString &Author, const QString &Notes, TStringList *Profiles)
{
    bool result;
    ZipPackageDBGLForm = new TZipPackageDBGLForm(AOwner);

    ZipPackageDBGLForm->ui.TitleEdit->setText(Title);
    ZipPackageDBGLForm->ui.AuthorEdit->setText(Author);
    ZipPackageDBGLForm->ui.NotesMemo->setPlainText(Notes);
    QListWidget *GamesListBox = ZipPackageDBGLForm->ui.GamesListBox;
    GamesListBox->clear();
    for (int I = 0; I < Profiles->Count(); I++) {
        auto *Item = new QListWidgetItem((*Profiles)[I], GamesListBox);
        Item->setFlags(Item->flags() | Qt::ItemIsUserCheckable);
        Item->setData(Qt::UserRole, QVariant::fromValue<void *>(Profiles->Objects(I)));
        Item->setCheckState(Qt::Checked);
    }
    result = (ZipPackageDBGLForm->ShowModal() == mrOk);
    if (result) {
        Profiles->Clear();
        for (int I = 0; I < GamesListBox->count(); I++) if (GamesListBox->item(I)->checkState() == Qt::Checked)
            Profiles->AddObject(GamesListBox->item(I)->text(), GamesListBox->item(I)->data(Qt::UserRole).value<void *>());
    }
    delete ZipPackageDBGLForm;
    ZipPackageDBGLForm = nullptr;
    return result;
}
