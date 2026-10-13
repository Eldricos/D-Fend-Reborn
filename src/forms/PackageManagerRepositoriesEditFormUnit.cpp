// C++ port of PackageManagerRepositoriesEditFormUnit.pas (implementation section).
#include "PackageManagerRepositoriesEditFormUnit.h"

#include <QTreeView>
#include <QHeaderView>

#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "LanguageSetupUnit.h"
#include "IconLoaderUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "PrgConsts.h"
#include "PackageDBToolsUnit.h"
#include "PackageManagerRepositoriesEditURLFormUnit.h"

TPackageManagerRepositoriesEditForm *PackageManagerRepositoriesEditForm = nullptr;

// TItemChange
static constexpr int ctState = 2;   // ctText = 0, ctImage = 1
// Delphi virtual key codes
#ifndef VK_RETURN_Code
static constexpr quint16 VK_RETURN_Code = 0x0D, VK_INSERT_Code = 0x2D, VK_DELETE_Code = 0x2E, VK_F1_Code = 0x70;
#endif

static quint16 QtKeyToVK(int Key)
{
    switch (Key) {
        case Qt::Key_Return: case Qt::Key_Enter: return VK_RETURN_Code;
        case Qt::Key_Insert: return VK_INSERT_Code;
        case Qt::Key_Delete: return VK_DELETE_Code;
        case Qt::Key_F1: return VK_F1_Code;
        default: return quint16(Key & 0xFFFF);
    }
}

// Column.Width:=-1 / -2 (LVSCW_AUTOSIZE / LVSCW_AUTOSIZE_USEHEADER)
static void AutoSizeColumn(TListView *LV, int I)
{
    LV->reportView()->resizeColumnToContents(I);
}

static int ItemNr(TListItem *L) { return int(reinterpret_cast<intptr_t>(L->Data())); }

TPackageManagerRepositoriesEditForm::TPackageManagerRepositoriesEditForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    ImageList = TImageList::FromResource("PackageManagerRepositoriesEditFormUnit", "ImageList");

    for (QAction *A : {ui.ActivateButton, ui.AddButton, ui.EditButton, ui.RemoveButton, ui.PopupActivate, ui.PopupAdd, ui.PopupEdit, ui.PopupRemove})
        connect(A, &QAction::triggered, this, [this, A]() { ButtonWork(A); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.PageControl, &QTabWidget::currentChanged, this, [this]() { PageControlChange(ui.PageControl); });
    for (TListView *LV : {ui.ListView1, ui.ListView}) {
        connect(LV, &TListView::change, this, [this, LV](TListItem *Item) { ListViewChange(LV, Item, ctState); });
        connect(LV, &TListView::itemChecked, this, [this, LV](TListItem *Item) { ListViewChange(LV, Item, ctState); });
        connect(LV, &TListView::dblClick, this, [this, LV]() { ListViewDblClick(LV); });
        connect(LV, &TListView::keyDown, this, [this, LV](int Key, Qt::KeyboardModifiers Mods) {
            quint16 K = QtKeyToVK(Key);
            ListViewKeyDown(LV, K, Mods);
        });
    }
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        quint16 K = QtKeyToVK(Key);
        FormKeyDown(this, K, Mods);
        return (Key == Qt::Key_F1) && (Mods == Qt::NoModifier);
    });
    FormCreate(this);   // OnCreate
}

TPackageManagerRepositoriesEditForm::~TPackageManagerRepositoriesEditForm()
{
    FormDestroy(this);   // OnDestroy
    delete ImageList;
}

void TPackageManagerRepositoriesEditForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!FShown) { FShown = true; FormShow(this); }
}

void TPackageManagerRepositoriesEditForm::FormCreate(QObject * /*Sender*/)
{
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    JustLoading = false;

    setWindowTitle(LanguageSetup->RepositoriesEditor());
    ui.ActivateButton->setText(LanguageSetup->RepositoriesEditorActivateSource());
    ui.AddButton->setText(LanguageSetup->RepositoriesEditorAddSource());
    ui.EditButton->setText(LanguageSetup->RepositoriesEditorEditSource());
    ui.RemoveButton->setText(LanguageSetup->RepositoriesEditorRemoveSource());
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.TabSheet1), LanguageSetup->RepositoriesEditorSourceOfficial());
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.TabSheet2), LanguageSetup->RepositoriesEditorSourceUserDefined());
    ui.PopupActivate->setText(LanguageSetup->RepositoriesEditorPopupActivateSource());
    ui.PopupAdd->setText(LanguageSetup->RepositoriesEditorPopupAddSource());
    ui.PopupEdit->setText(LanguageSetup->RepositoriesEditorPopupEditSource());
    ui.PopupRemove->setText(LanguageSetup->RepositoriesEditorPopupRemoveSource());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

    UserIconLoader->DialogImage(DI_Activate, ImageList, 0);
    UserIconLoader->DialogImage(DI_Add, ImageList, 1);
    UserIconLoader->DialogImage(DI_Edit, ImageList, 2);
    UserIconLoader->DialogImage(DI_Delete, ImageList, 3);
    // ToolBar.Images / PopupMenu.Images
    ui.ActivateButton->setIcon(ImageList->Icon(0)); ui.PopupActivate->setIcon(ImageList->Icon(0));
    ui.AddButton->setIcon(ImageList->Icon(1)); ui.PopupAdd->setIcon(ImageList->Icon(1));
    ui.EditButton->setIcon(ImageList->Icon(2)); ui.PopupEdit->setIcon(ImageList->Icon(2));
    ui.RemoveButton->setIcon(ImageList->Icon(3)); ui.PopupRemove->setIcon(ImageList->Icon(3));

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    DBDir = IncludeTrailingPathDelimiter(PrgDataDir() + PackageDBSubFolder);

    MainPackageList = new TPackageListFile(true);
    MainPackageList->LoadFromFile(DBDir + PackageDBMainFile);
    if (MainPackageList->Count() == 0) MainPackageList->UpdateFromServer(Format(PackageDBMainFileURL, {GetNormalFileVersionAsString()}), DBDir + PackageDBTempFile, false, false);

    UserPackageList = new TPackageListFile(false);
    UserPackageList->LoadFromFile(DBDir + PackageDBUserFile);
}

void TPackageManagerRepositoriesEditForm::FormShow(QObject * /*Sender*/)
{
    TListColumn *C;
    C = ui.ListView->Columns()->Add(); C->SetWidth(-1); C->SetCaption(LanguageSetup->RepositoriesEditorColumnURL());
    C = ui.ListView->Columns()->Add(); C->SetWidth(-1); C->SetCaption(LanguageSetup->RepositoriesEditorColumnDescription());

    C = ui.ListView1->Columns()->Add(); C->SetWidth(-1); C->SetCaption(LanguageSetup->RepositoriesEditorColumnURL());
    C = ui.ListView1->Columns()->Add(); C->SetWidth(-1); C->SetCaption(LanguageSetup->RepositoriesEditorColumnDescription());

    LoadLists();
}

void TPackageManagerRepositoriesEditForm::FormDestroy(QObject * /*Sender*/)
{
    JustLoading = true;
    delete MainPackageList; MainPackageList = nullptr;
    delete UserPackageList; UserPackageList = nullptr;
}

QString TPackageManagerRepositoriesEditForm::GetDescription(const QString &URL)
{
    QString result;

    QString FileName = DBDir + ExtractFileNameFromURL(URL, ".xml", false);
    if (!FileExists(FileName)) {
        if ((ExtUpperCase(Copy(URL, 1, 4)) != "HTTP") && (FileExists(URL) || FileExists(Replace(URL, "%20", " ")))) {
            if (FileExists(URL)) CopyFile(URL, FileName, true); else CopyFile(Replace(URL, "%20", " "), FileName, true);
            if (!FileExists(FileName)) return result;
        } else {
            return result;
        }
    }

    auto *PackageList = new TPackageList(URL);
    PackageList->LoadFromFile(FileName);
    result = PackageList->Name();
    delete PackageList;
    return result;
}

void TPackageManagerRepositoriesEditForm::LoadLists()
{
    TListItem *L;
    JustLoading = true;

    // Official
    ui.ListView1->Items()->BeginUpdate();
    ui.ListView1->Items()->Clear();
    for (int I = 0; I < MainPackageList->Count(); I++) {
        L = ui.ListView1->Items()->Add();
        L->SetCaption(MainPackageList->URL(I));
        L->SetChecked(MainPackageList->Active(I));
        L->SubItems().Add(GetDescription(MainPackageList->URL(I)));
        L->SetData(reinterpret_cast<void *>(intptr_t(I)));
    }
    ui.ListView1->Items()->EndUpdate();

    for (int I = 0; I < ui.ListView1->Columns()->Count(); I++) AutoSizeColumn(ui.ListView1, I);
    if (ui.ListView1->Items()->Count() > 0) ui.ListView1->SetSelected(ui.ListView1->Items()->Item(0));

    // User
    ui.ListView->Items()->BeginUpdate();
    ui.ListView->Items()->Clear();
    for (int I = 0; I < UserPackageList->Count(); I++) {
        L = ui.ListView->Items()->Add();
        L->SetCaption(UserPackageList->URL(I));
        L->SetChecked(UserPackageList->Active(I));
        L->SubItems().Add(GetDescription(UserPackageList->URL(I)));
        L->SetData(reinterpret_cast<void *>(intptr_t(I)));
    }
    ui.ListView->Items()->EndUpdate();

    for (int I = 0; I < ui.ListView->Columns()->Count(); I++) AutoSizeColumn(ui.ListView, I);
    if (ui.ListView->Items()->Count() > 0) ui.ListView->SetSelected(ui.ListView->Items()->Item(0));

    // Common
    ListViewChange(this, ui.ListView->Selected(), ctState);

    JustLoading = false;
}

void TPackageManagerRepositoriesEditForm::ListViewChange(QObject * /*Sender*/, TListItem *Item, int Change)
{
    if ((MainPackageList == nullptr) || (UserPackageList == nullptr)) return;

    if ((!JustLoading) && (Item != nullptr) && (Change == ctState)) {
        if (Item->ListView() == ui.ListView1)
            MainPackageList->SetActive(ItemNr(Item), Item->Checked());
        else
            UserPackageList->SetActive(ItemNr(Item), Item->Checked());
    }

    if (ui.PageControl->currentIndex() == 0) {
        if (ui.ListView1->Selected() == nullptr) {
            ui.ActivateButton->setEnabled(false);
            ui.ActivateButton->setText(LanguageSetup->RepositoriesEditorActivateSource());
            ui.PopupActivate->setText(LanguageSetup->RepositoriesEditorPopupActivateSource());
        } else {
            ui.ActivateButton->setEnabled(true);
            if (MainPackageList->Active(ItemNr(ui.ListView1->Selected()))) {
                ui.ActivateButton->setText(LanguageSetup->RepositoriesEditorDeactivateSource());
                ui.PopupActivate->setText(LanguageSetup->RepositoriesEditorPopupDeactivateSource());
            } else {
                ui.ActivateButton->setText(LanguageSetup->RepositoriesEditorActivateSource());
                ui.PopupActivate->setText(LanguageSetup->RepositoriesEditorPopupActivateSource());
            }
        }
    } else {
        if (ui.ListView->Selected() == nullptr) {
            ui.ActivateButton->setEnabled(false);
            ui.ActivateButton->setText(LanguageSetup->RepositoriesEditorActivateSource());
            ui.PopupActivate->setText(LanguageSetup->RepositoriesEditorPopupActivateSource());
        } else {
            ui.ActivateButton->setEnabled(true);
            if (UserPackageList->Active(ItemNr(ui.ListView->Selected()))) {
                ui.ActivateButton->setText(LanguageSetup->RepositoriesEditorDeactivateSource());
                ui.PopupActivate->setText(LanguageSetup->RepositoriesEditorPopupDeactivateSource());
            } else {
                ui.ActivateButton->setText(LanguageSetup->RepositoriesEditorActivateSource());
                ui.PopupActivate->setText(LanguageSetup->RepositoriesEditorPopupActivateSource());
            }
        }
    }

    ui.PopupActivate->setEnabled(ui.ActivateButton->isEnabled());
    const bool B = (ui.PageControl->currentIndex() == 1) && (ui.ListView->Selected() != nullptr);
    ui.EditButton->setEnabled(B);
    ui.PopupEdit->setEnabled(B);
    ui.RemoveButton->setEnabled(B);
    ui.PopupRemove->setEnabled(B);
}

void TPackageManagerRepositoriesEditForm::PageControlChange(QObject *Sender)
{
    ListViewChange(Sender, ui.ListView->Selected(), ctState);
}

void TPackageManagerRepositoriesEditForm::ButtonWork(QObject *Sender)
{
    int I;
    QString S;

    switch (Sender->property("tag").toInt()) {
        case 0:
            if (ui.PageControl->currentIndex() == 0) {
                if (ui.ListView1->Selected() != nullptr) {
                    ui.ListView1->Selected()->SetChecked(!ui.ListView1->Selected()->Checked());
                    I = ItemNr(ui.ListView1->Selected());
                    MainPackageList->SetActive(I, !MainPackageList->Active(I));
                    ListViewChange(this, ui.ListView1->Selected(), ctState);
                }
            } else {
                if (ui.ListView->Selected() != nullptr) {
                    ui.ListView->Selected()->SetChecked(!ui.ListView->Selected()->Checked());
                    I = ItemNr(ui.ListView->Selected());
                    UserPackageList->SetActive(I, !UserPackageList->Active(I));
                    ListViewChange(this, ui.ListView->Selected(), ctState);
                }
            }
            break;
        case 1:
            ui.PageControl->setCurrentIndex(1);
            S = "";
            if (!ShowPackageManagerRepositoriesEditURLDialog(this, S)) return;
            UserPackageList->Add(S, true);
            LoadLists();
            ui.ListView->SetSelected(ui.ListView->Items()->Item(ui.ListView->Items()->Count() - 1));
            break;
        case 2:
            if ((ui.PageControl->currentIndex() == 1) && (ui.ListView->Selected() != nullptr)) {
                I = ItemNr(ui.ListView->Selected());
                S = UserPackageList->URL(I);
                if (!ShowPackageManagerRepositoriesEditURLDialog(this, S)) return;
                UserPackageList->SetURL(I, S);
                ui.ListView->Selected()->SetCaption(S);
                ui.ListView->Selected()->SubItems().SetStrings(0, GetDescription(S));
            }
            break;
        case 3:
            if ((ui.PageControl->currentIndex() == 1) && (ui.ListView->Selected() != nullptr)) {
                if (MessageDlg(LanguageSetup->RepositoriesEditorRemoveSourceConfirm(), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
                I = ui.ListView->ItemIndex();
                S = DBDir + ExtractFileNameFromURL(UserPackageList->URL(ItemNr(ui.ListView->Selected())), ".xml", false);
                if (FileExists(S)) ExtDeleteFile(S, ftTemp);
                UserPackageList->Delete(ItemNr(ui.ListView->Selected()));
                LoadLists();
                if (ui.ListView->Items()->Count() > 0) ui.ListView->SetItemIndex(Max(0, I - 1));
                ListViewChange(this, ui.ListView->Selected(), ctState);
            }
            break;
    }
}

void TPackageManagerRepositoriesEditForm::ListViewDblClick(QObject * /*Sender*/)
{
    ButtonWork(ui.EditButton);
}

void TPackageManagerRepositoriesEditForm::ListViewKeyDown(QObject * /*Sender*/, quint16 &Key, TShiftState Shift)
{
    if (Shift != Qt::NoModifier) return;
    switch (Key) {
        case VK_RETURN_Code: ButtonWork(ui.EditButton); break;
        case VK_INSERT_Code: ButtonWork(ui.AddButton); break;
        case VK_DELETE_Code: ButtonWork(ui.RemoveButton); break;
        default: break;
    }
}

void TPackageManagerRepositoriesEditForm::OKButtonClick(QObject * /*Sender*/)
{
    MainPackageList->SaveToFile(this);
    UserPackageList->SetUpdateDate(Now());
    UserPackageList->SaveToFile(this);
}

void TPackageManagerRepositoriesEditForm::FormKeyDown(QObject *Sender, quint16 &Key, TShiftState Shift)
{
    if ((Key == VK_F1_Code) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

void TPackageManagerRepositoriesEditForm::HelpButtonClick(QObject * /*Sender*/)
{
    ApplicationHelpCommand(HELP_CONTEXT, ID_FileImportDownload);
}

// global

bool ShowPackageManagerRepositoriesEditDialog(QWidget *AOwner)
{
    PackageManagerRepositoriesEditForm = new TPackageManagerRepositoriesEditForm(AOwner);
    bool result = (PackageManagerRepositoriesEditForm->ShowModal() == mrOk);
    delete PackageManagerRepositoriesEditForm;
    PackageManagerRepositoriesEditForm = nullptr;
    return result;
}
