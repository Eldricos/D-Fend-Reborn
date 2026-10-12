// C++ port of TemplateSelectProfileFormUnit.pas (implementation section).
#include "TemplateSelectProfileFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"

TTemplateSelectProfileForm* TemplateSelectProfileForm = nullptr;

TTemplateSelectProfileForm::TTemplateSelectProfileForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    connect(ui.ListView, &TListView::columnClick, this, [this](int Column) { ListViewColumnClick(ui.ListView, ui.ListView->Column(Column)); });
    connect(ui.ListView, &TListView::dblClick, this, [this]() { ListViewDblClick(ui.ListView); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });

    FormCreate(this);
}

TTemplateSelectProfileForm::~TTemplateSelectProfileForm()
{
    ui.ListView->Items()->Clear();
    delete ImageList;
    delete ListViewImageList;
    delete ListviewIconImageList;
}

void TTemplateSelectProfileForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (E->spontaneous() || FFormShown) return;
    FFormShown = true;
    FormShow(this);
}

void TTemplateSelectProfileForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed

    ListSort = slbName;
    ListSortReverse = false;

    ImageList = TImageList::FromResource("TemplateSelectProfileFormUnit", "ImageList");
    if (ImageList == nullptr) ImageList = new TImageList();
    ListViewImageList = new TImageList(16, 16);
    ListviewIconImageList = new TImageList(32, 32);

    setWindowTitle(LanguageSetup->TemplateFormNewFromProfileCaption());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
}

void TTemplateSelectProfileForm::LoadList()
{
    ui.ListView->Items()->BeginUpdate();
    AddGamesToList(ui.ListView, ListViewImageList, ListviewIconImageList, ImageList, GameDB, QString(), QString(), QString(), true, ListSort, ListSortReverse, true, false, false, false);
    ui.ListView->Items()->EndUpdate();
}

void TTemplateSelectProfileForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    InitListViewForGamesList(ui.ListView, true);
    LoadList();
    Game = nullptr;
}

void TTemplateSelectProfileForm::ListViewColumnClick(QObject* Sender, TListColumn* Column)
{
    Q_UNUSED(Sender);
    if (Column == nullptr) return;
    SetSortTypeByListViewCol(Column->Index(), ListSort, ListSortReverse);
    LoadList();
}

void TTemplateSelectProfileForm::ListViewDblClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.OKButton->click();
}

void TTemplateSelectProfileForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    if ((ui.ListView->Selected() == nullptr) || (ui.ListView->Selected()->Data() == nullptr)) {
        MessageDlg(LanguageSetup->MessageNoGameSelected(), mtError, mbOK, 0);
        setModalResult(mrNone); return;
    }

    Game = static_cast<TGame *>(ui.ListView->Selected()->Data());
}

TGame* SelectProfile(QWidget* AOwner, TGameDB* AGameDB)
{
    TGame* result;
    TemplateSelectProfileForm = new TTemplateSelectProfileForm(AOwner);
    TemplateSelectProfileForm->GameDB = AGameDB;
    if (TemplateSelectProfileForm->ShowModal() == mrOk) {
        result = TemplateSelectProfileForm->Game;
    } else {
        result = nullptr;
    }
    delete TemplateSelectProfileForm;
    TemplateSelectProfileForm = nullptr;
    return result;
}
