// C++ port of TemplateFormUnit.pas (implementation section).
#include "TemplateFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "ProfileEditorFormUnit.h"
#include "PrgSetupUnit.h"
#include "TemplateSelectProfileFormUnit.h"
#include "ModernProfileEditorFormUnit.h"
#include "SelectProfilesFormUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "WaitFormUnit.h"
#include "StatisticsFormUnit.h"
#include "MultipleProfilesEditorFormUnit.h"

TTemplateForm* TemplateForm = nullptr;

namespace {

// Delphi virtual key codes
constexpr quint16 VK_RETURN = 0x0D;
constexpr quint16 VK_INSERT = 0x2D;
constexpr quint16 VK_DELETE = 0x2E;
constexpr quint16 VK_F1 = 0x70;
constexpr quint16 VK_F2 = 0x71;

quint16 QtKeyToVK(int Key)
{
    switch (Key) {
    case Qt::Key_Return: case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_F1: return VK_F1;
    case Qt::Key_F2: return VK_F2;
    default: return 0;
    }
}

// Application.HelpCommand(HELP_CONTEXT,ID)
void ApplicationHelpContext(int ID)
{
    // TODO(port): the VCL routes Application.HelpCommand through THTMLhelpRouter (HelpTools);
    // call the HTML help API directly when it is available.
    if (GLOBAL_HELPROUTER != nullptr && HtmlHelpA)
        HtmlHelpA(0, ChangeFileExt(GLOBAL_HELPROUTER->Helpfile(), ".chm"), HH_HELP_CONTEXT, ID);
}

TGame *ItemGame(TListItem *Item) { return Item ? static_cast<TGame *>(Item->Data()) : nullptr; }

// ToolBar.ClientToScreen(Point(Button.Left+5,Button.Top+5))
QPoint ToolButtonScreenPos(QToolBar *Bar, QAction *Button)
{
    QWidget *W = Bar->widgetForAction(Button);
    if (W == nullptr) return QCursor::pos();
    return Bar->mapToGlobal(QPoint(W->x() + 5, W->y() + 5));
}

}  // namespace

TTemplateForm::TTemplateForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    connect(ui.PageControl, &QTabWidget::currentChanged, this, [this]() { PageControlChange(ui.PageControl); });
    ui.ListView->OnCustomDrawItem = [this](TListItem *Item) {
        bool DefaultDraw = true;
        ListViewAdvancedCustomDrawItem(ui.ListView, Item, 0, 0, DefaultDraw);
    };
    connect(ui.ListView, &TListView::columnClick, this, [this](int Column) { ListViewColumnClick(ui.ListView, ui.ListView->Column(Column)); });
    connect(ui.ListView, &TListView::dblClick, this, [this]() { ListViewDblClick(ui.ListView); });
    connect(ui.ListView, &TListView::keyDown, this, [this](int Key, Qt::KeyboardModifiers Mods) {
        quint16 K = QtKeyToVK(Key);
        ListViewKeyDown(ui.ListView, K, Mods & ~Qt::KeypadModifier);
    });
    connect(ui.ListView, &TListView::selectItem, this, [this](TListItem *Item, bool Selected) { ListViewSelectItem(ui.ListView, Item, Selected); });
    connect(ui.ListView2, &TListView::columnClick, this, [this](int Column) { ListView2ColumnClick(ui.ListView2, ui.ListView2->Column(Column)); });
    connect(ui.ListView2, &TListView::dblClick, this, [this]() { ListView2DblClick(ui.ListView2); });
    connect(ui.ListView2, &TListView::keyDown, this, [this](int Key, Qt::KeyboardModifiers Mods) {
        quint16 K = QtKeyToVK(Key);
        ListView2KeyDown(ui.ListView2, K, Mods & ~Qt::KeypadModifier);
    });
    connect(ui.ListView2, &TListView::selectItem, this, [this](TListItem *Item, bool Selected) { ListView2SelectItem(ui.ListView2, Item, Selected); });
    for (QAction *A : {ui.CloseButton, ui.UseButton, ui.AddButton, ui.EditButton, ui.DeleteButton, ui.HelpButton,
                       ui.CloseButton2, ui.AddButton2, ui.EditButton2, ui.DeleteButton2, ui.HelpButton2,
                       ui.PopupUseProfile, ui.PopupUseDefault, ui.PopupEdit, ui.PopupCopy, ui.PopupDel,
                       ui.PopupUseProfile2, ui.PopupUseDefault2, ui.PopupAddNew, ui.PopupAddFromProfile,
                       ui.PopupAddNew2, ui.PopupAddFromProfile2, ui.PopupEdit2, ui.PopupCopy2, ui.PopupDel2,
                       ui.MenuFileClose, ui.MenuEditAddNewTemplate, ui.MenuEditAddTemplateFromProfile,
                       ui.MenuEditAdd2NewTemplate, ui.MenuEditAdd2TemplateFromProfile, ui.MenuEditEdit, ui.MenuEditEdit2,
                       ui.MenuEditEditMultipleTemplates, ui.MenuEditEditMultipleTemplates2, ui.MenuEditCopy, ui.MenuEditCopy2,
                       ui.MenuEditDelete, ui.MenuEditDelete2, ui.MenuEditCheck, ui.MenuHelpHelp})
        connect(A, &QAction::triggered, this, [this, A]() { ButtonWork(A); });
    // MenuFileUseAsProfile / MenuFileUseAsDefault have Tag 5/6 but no OnClick in the DFM
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        quint16 K = (Key == Qt::Key_F1) ? VK_F1 : 0;
        if (K == 0) return false;
        FormKeyDown(this, K, Mods & ~Qt::KeypadModifier);
        return false;
    });

    FormCreate(this);
}

TTemplateForm::~TTemplateForm()
{
    FormDestroy(this);
    ui.ListView->OnCustomDrawItem = nullptr;
    ui.ListView->Items()->Clear();
    ui.ListView2->Items()->Clear();
    delete ImageList;
    delete ListViewImageList;
    delete ListviewIconImageList;
}

void TTemplateForm::FormCreate(QObject* Sender)
{
    // SetVistaFonts(self); DoubleBuffered:=True; Font.Charset:=...; NoFlicker(...) - not needed

    ListSort = slbName;
    ListSortReverse = false;
    ListSort2 = slbName;
    ListSortReverse2 = false;

    Template = nullptr;
    TemplateScummVM = false;

    setWindowTitle(LanguageSetup->TemplateForm());
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.TemplateTab), LanguageSetup->TemplateForm());
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.AutoSetupSheet), LanguageSetup->TemplateFormAutoSetupCaption());

    if (PrgSetup->ToolbarFontSize() > 0) {
        QFont F = ui.CoolBar->font(); F.setPointSize(PrgSetup->ToolbarFontSize());
        ui.CoolBar->setFont(F); ui.ToolBar->setFont(F);
        ui.CoolBar1->setFont(F); ui.ToolBar2->setFont(F);
    }

    // Menu

    ui.MenuFile->setTitle(LanguageSetup->MenuFile());
    ui.MenuFileUseAsProfile->setText(LanguageSetup->TemplateFormUseAsProfile());
    ui.MenuFileUseAsDefault->setText(LanguageSetup->TemplateFormUseAsDefault());
    ui.MenuFileUse->setTitle(LanguageSetup->Use());
    ui.MenuFileClose->setText(LanguageSetup->Close());
    ui.MenuEdit->setTitle(LanguageSetup->Edit());
    ui.MenuEditAdd->setTitle(LanguageSetup->MenuProfileAdd());
    ui.MenuEditAddNewTemplate->setText(LanguageSetup->TemplateFormNewTemplate());
    ui.MenuEditAddTemplateFromProfile->setText(LanguageSetup->TemplateFormNewFromProfile());
    ui.MenuEditAdd2->setTitle(LanguageSetup->MenuProfileAdd());
    ui.MenuEditAdd2NewTemplate->setText(LanguageSetup->TemplateFormNewTemplate());
    ui.MenuEditAdd2TemplateFromProfile->setText(LanguageSetup->TemplateFormNewFromProfile());
    ui.MenuEditEdit->setText(LanguageSetup->MenuProfileEdit());
    ui.MenuEditEdit2->setText(LanguageSetup->MenuProfileEdit());
    ui.MenuEditCopy->setText(LanguageSetup->TemplateFormCopy());
    ui.MenuEditCopy2->setText(LanguageSetup->TemplateFormCopy());
    ui.MenuEditEditMultipleTemplates->setText(LanguageSetup->TemplateFormEditMultipleTemplates());
    ui.MenuEditEditMultipleTemplates2->setText(LanguageSetup->TemplateFormEditMultipleTemplates());
    ui.MenuEditDelete->setText(LanguageSetup->Del());
    ui.MenuEditDelete2->setText(LanguageSetup->Del());
    ui.MenuEditCheck->setText(LanguageSetup->TemplateFormEditCheckChecksums());
    ui.MenuHelp->setTitle(LanguageSetup->MenuHelp());
    ui.MenuHelpHelp->setText(LanguageSetup->Help());

    // Template sheet

    ui.CloseButton->setText(LanguageSetup->Close());
    ui.CloseButton->setToolTip(LanguageSetup->CloseHintWindow());

    ui.UseButton->setText(LanguageSetup->Use());
    ui.UseButton->setToolTip(LanguageSetup->UseHintTemplate());
    ui.PopupUseProfile2->setText(LanguageSetup->TemplateFormUseAsProfile());
    ui.PopupUseDefault2->setText(LanguageSetup->TemplateFormUseAsDefault());
    ui.AddButton->setText(LanguageSetup->Add());
    ui.AddButton->setToolTip(LanguageSetup->AddHintTemplate());
    ui.EditButton->setText(LanguageSetup->Edit());
    ui.EditButton->setToolTip(LanguageSetup->EditHintTemplate());
    ui.DeleteButton->setText(LanguageSetup->Del());
    ui.DeleteButton->setToolTip(LanguageSetup->DelHintTemplate());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.HelpButton->setToolTip(LanguageSetup->HelpHint());

    ui.PopupUse->setTitle(LanguageSetup->Use());
    ui.PopupUseProfile->setText(LanguageSetup->TemplateFormUseAsProfile());
    ui.PopupUseDefault->setText(LanguageSetup->TemplateFormUseAsDefault());
    ui.PopupEdit->setText(LanguageSetup->MenuProfileEdit());
    ui.PopupCopy->setText(LanguageSetup->TemplateFormCopy());
    ui.PopupDel->setText(LanguageSetup->Del());

    ui.PopupUseProfile2->setText(LanguageSetup->TemplateFormUseAsProfile());
    ui.PopupUseDefault2->setText(LanguageSetup->TemplateFormUseAsDefault());

    ui.PopupAddNew->setText(LanguageSetup->TemplateFormNewTemplate());
    ui.PopupAddFromProfile->setText(LanguageSetup->TemplateFormNewFromProfile());

    // AutoSetup sheet

    ui.CloseButton2->setText(LanguageSetup->Close());
    ui.CloseButton2->setToolTip(LanguageSetup->CloseHintWindow());

    ui.AddButton2->setText(LanguageSetup->Add());
    ui.AddButton2->setToolTip(LanguageSetup->AddHintTemplate());
    ui.EditButton2->setText(LanguageSetup->Edit());
    ui.EditButton2->setToolTip(LanguageSetup->EditHintTemplate());
    ui.DeleteButton2->setText(LanguageSetup->Del());
    ui.DeleteButton2->setToolTip(LanguageSetup->DelHintTemplate());
    ui.HelpButton2->setText(LanguageSetup->Help());
    ui.HelpButton2->setToolTip(LanguageSetup->HelpHint());

    ui.PopupEdit2->setText(LanguageSetup->MenuProfileEdit());
    ui.PopupCopy2->setText(LanguageSetup->TemplateFormCopy());
    ui.PopupDel2->setText(LanguageSetup->Del());

    ui.PopupAddNew2->setText(LanguageSetup->TemplateFormNewTemplate());
    ui.PopupAddFromProfile2->setText(LanguageSetup->TemplateFormNewFromProfile());

    // Init lists

    TemplateDB = new TGameDB(PrgDataDir() + TemplateSubDir, false);
    AutoSetupDB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);
    PrgSetup->UpdateFile();
    DefaultTemplate = new TGame(PrgSetup);
    if (!DefaultTemplate->Name().isEmpty()) DefaultTemplate->SetName(QString());

    ImageList = TImageList::FromResource("TemplateFormUnit", "ImageList");
    if (ImageList == nullptr) ImageList = new TImageList();
    ListViewImageList = new TImageList(16, 16);
    ListviewIconImageList = new TImageList(32, 32);

    UserIconLoader->DialogImage(DI_ImageFloppy, ImageList, 0);
    UserIconLoader->DialogImage(DI_CloseWindow, ImageList, 1);
    UserIconLoader->DialogImage(DI_UseTemplate, ImageList, 2);
    UserIconLoader->DialogImage(DI_Add, ImageList, 3);
    UserIconLoader->DialogImage(DI_Edit, ImageList, 4);
    UserIconLoader->DialogImage(DI_Clear, ImageList, 5);
    UserIconLoader->DialogImage(DI_ToolbarHelp, ImageList, 6);
    {
        // ImageIndex of the tool buttons and menu items (DFM)
        const struct { QAction *A; int Index; } Icons[] = {
            {ui.CloseButton, 1}, {ui.UseButton, 2}, {ui.AddButton, 3}, {ui.EditButton, 4}, {ui.DeleteButton, 5}, {ui.HelpButton, 6},
            {ui.CloseButton2, 1}, {ui.AddButton2, 3}, {ui.EditButton2, 4}, {ui.DeleteButton2, 5}, {ui.HelpButton2, 6},
            {ui.PopupUse->menuAction(), 2}, {ui.PopupEdit, 4}, {ui.PopupDel, 5}, {ui.PopupEdit2, 4}, {ui.PopupDel2, 5},
            {ui.MenuFileUse->menuAction(), 2}, {ui.MenuFileClose, 1}, {ui.MenuEditAdd->menuAction(), 3}, {ui.MenuEditAdd2->menuAction(), 3},
            {ui.MenuEditEdit, 4}, {ui.MenuEditEdit2, 4}, {ui.MenuEditDelete, 5}, {ui.MenuEditDelete2, 5}, {ui.MenuHelpHelp, 6}
        };
        for (const auto &I : Icons) if (I.Index < ImageList->Count()) I.A->setIcon(ImageList->Icon(I.Index));
    }

    InitListViewForGamesList(ui.ListView, true);
    InitListViewForGamesList(ui.ListView2, true);
    LoadList();
    LoadList2();
    PageControlChange(Sender);
}

void TTemplateForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    delete TemplateDB; TemplateDB = nullptr;
    delete AutoSetupDB; AutoSetupDB = nullptr;
    delete DefaultTemplate; DefaultTemplate = nullptr;
}

void TTemplateForm::SelectGame(TGame* AGame, const QString& AGameName)
{
    int I;
    if (AGame != nullptr) {
        for (I = 0; I < ui.ListView->Items()->Count(); I++) if (ItemGame(ui.ListView->Items()->Item(I)) == AGame) { ui.ListView->SetSelected(ui.ListView->Items()->Item(I)); break; }
    } else if (!AGameName.isEmpty()) {
        for (I = 0; I < ui.ListView->Items()->Count(); I++) if ((ItemGame(ui.ListView->Items()->Item(I)) == DefaultTemplate) && (ui.ListView->Items()->Item(I)->Caption() == AGameName)) { ui.ListView->SetSelected(ui.ListView->Items()->Item(I)); break; }
    }
    ListViewSelectItem(this, ui.ListView->Selected(), ui.ListView->Selected() != nullptr);
}

void TTemplateForm::SelectGame2(TGame* AGame)
{
    for (int I = 0; I < ui.ListView2->Items()->Count(); I++) if (ItemGame(ui.ListView2->Items()->Item(I)) == AGame) { ui.ListView2->SetSelected(ui.ListView2->Items()->Item(I)); break; }
    ListView2SelectItem(this, ui.ListView2->Selected(), ui.ListView2->Selected() != nullptr);
}

void TTemplateForm::LoadList()
{
    TGame *G, *OldDefaultTemplate;
    QString GName;

    OldDefaultTemplate = DefaultTemplate;

    ui.ListView->Items()->BeginUpdate();
    if (ui.ListView->Selected() != nullptr) {
        G = ItemGame(ui.ListView->Selected());
        if (G == OldDefaultTemplate) { G = nullptr; GName = ui.ListView->Selected()->Caption(); } else GName.clear();
    } else {
        G = nullptr;
        GName.clear();
    }
    // (the Delphi code recreates the default template before reading the selection; the old object is only compared by address)
    delete DefaultTemplate;
    DefaultTemplate = new TGame(PrgSetup);

    AddGamesToList(ui.ListView, ListViewImageList, ListviewIconImageList, ImageList, TemplateDB, DefaultTemplate, RemoveUnderline(LanguageSetup->All()), QString(), QString(), true, ListSort, ListSortReverse, false, false, false, false);
    SelectGame(G, GName);
    if ((ui.ListView->Selected() == nullptr) && (ui.ListView->Items()->Count() > 0)) ui.ListView->SetSelected(ui.ListView->Items()->Item(0));

    VclLayout::setStatusText(ui.StatusBar, 0, IntToStr(ui.ListView->Items()->Count()) + " " + LanguageSetup->StatisticsNumberTemplates());
    ui.ListView->Items()->EndUpdate();
}

void TTemplateForm::LoadList2()
{
    TGame* G;

    ui.ListView2->Items()->BeginUpdate();
    if (ui.ListView2->Selected() != nullptr) G = ItemGame(ui.ListView2->Selected()); else G = nullptr;
    AddGamesToList(ui.ListView2, ListViewImageList, ListviewIconImageList, ImageList, AutoSetupDB, RemoveUnderline(LanguageSetup->All()), QString(), QString(), true, ListSort2, ListSortReverse2, false, false, false, false);
    SelectGame2(G);
    if ((ui.ListView2->Selected() == nullptr) && (ui.ListView2->Items()->Count() > 0)) ui.ListView2->SetSelected(ui.ListView2->Items()->Item(0));

    VclLayout::setStatusText(ui.StatusBar, 1, IntToStr(ui.ListView2->Items()->Count()) + " " + LanguageSetup->StatisticsNumberAutoSetupTemplates());
    ui.ListView2->Items()->EndUpdate();
}

void TTemplateForm::PageControlChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.CoolBar->setVisible(ui.PageControl->currentIndex() == 0);
    ui.CoolBar1->setVisible(ui.PageControl->currentIndex() == 1);

    const bool V1 = !ui.CoolBar->isHidden(), V2 = !ui.CoolBar1->isHidden();
    ui.MenuFileUse->menuAction()->setVisible(V1);
    ui.MenuEditAdd->menuAction()->setVisible(V1);
    ui.MenuEditEdit->setVisible(V1);
    ui.MenuEditCopy->setVisible(V1);
    ui.MenuEditEditMultipleTemplates->setVisible(V1);
    ui.MenuEditDelete->setVisible(V1);

    ui.MenuEditAdd2->menuAction()->setVisible(V2);
    ui.MenuEditEdit2->setVisible(V2);
    ui.MenuEditCopy2->setVisible(V2);
    ui.MenuEditEditMultipleTemplates2->setVisible(V2);
    ui.MenuEditDelete2->setVisible(V2);
    ui.MenuEditCheck->setVisible(V2);
}

void TTemplateForm::ListViewSelectItem(QObject* Sender, TListItem* Item, bool Selected)
{
    Q_UNUSED(Sender);
    bool B, B2, ScummVM;

    B = Selected && (Item != nullptr) && (Item->Data() != nullptr);

    B2 = B && (ItemGame(Item) != DefaultTemplate);
    if (B && (!B2) && ui.ListView->Selected() != nullptr) {
        ScummVM = (ItemGame(ui.ListView->Selected()) == DefaultTemplate) && (ui.ListView->Selected()->Caption() != LanguageSetup->TemplateFormDefault());
    } else {
        ScummVM = false;
    }

    ui.UseButton->setEnabled(B);
    ui.EditButton->setEnabled(B);
    ui.DeleteButton->setEnabled(B2);

    ui.PopupUse->menuAction()->setEnabled(B);
    ui.PopupEdit->setEnabled(B);
    ui.PopupCopy->setEnabled(B && (!ScummVM));
    ui.PopupDel->setEnabled(B2);

    ui.PopupUseDefault->setEnabled(B2);
    ui.PopupUseDefault2->setEnabled(B2);

    ui.MenuFileUse->menuAction()->setEnabled(B);
    ui.MenuFileUseAsDefault->setEnabled(B2);
    ui.MenuEditEdit->setEnabled(B);
    ui.MenuEditCopy->setEnabled(B && (!ScummVM));
    ui.MenuEditDelete->setEnabled(B2);
}

void TTemplateForm::ListView2SelectItem(QObject* Sender, TListItem* Item, bool Selected)
{
    Q_UNUSED(Sender);
    bool B = Selected && (Item != nullptr) && (Item->Data() != nullptr);

    ui.EditButton2->setEnabled(B);
    ui.DeleteButton2->setEnabled(B);

    ui.PopupEdit2->setEnabled(B);
    ui.PopupCopy2->setEnabled(B);
    ui.PopupDel2->setEnabled(B);

    ui.MenuEditEdit2->setEnabled(B);
    ui.MenuEditCopy2->setEnabled(B);
    ui.MenuEditDelete2->setEnabled(B);
}

void TTemplateForm::ListViewAdvancedCustomDrawItem(TListView* Sender, TListItem* Item, int State, int Stage, bool& DefaultDraw)
{
    Q_UNUSED(Sender); Q_UNUSED(State); Q_UNUSED(Stage);
    DefaultDraw = true;
    if (Item == nullptr) return;
    // Canvas.Font.Color:=clBlue / clWindowText (only assign on change - SetTextColor repaints the item)
    const QColor C = ((Item->Data() != nullptr) && (ItemGame(Item) == DefaultTemplate)) ? TColorToQColor(clBlue) : QColor();
    if (Item->TextColor() != C) Item->SetTextColor(C);
}

void TTemplateForm::ListViewColumnClick(QObject* Sender, TListColumn* Column)
{
    Q_UNUSED(Sender);
    if (Column == nullptr) return;
    SetSortTypeByListViewCol(Column->Index(), ListSort, ListSortReverse);
    LoadList();
}

void TTemplateForm::ListViewDblClick(QObject* Sender)
{
    quint16 Key = VK_F2;
    ListViewKeyDown(Sender, Key, Qt::NoModifier);
}

void TTemplateForm::ListView2ColumnClick(QObject* Sender, TListColumn* Column)
{
    Q_UNUSED(Sender);
    if (Column == nullptr) return;
    SetSortTypeByListViewCol(Column->Index(), ListSort2, ListSortReverse2);
    LoadList2();
}

void TTemplateForm::ListView2DblClick(QObject* Sender)
{
    quint16 Key = VK_F2;
    ListView2KeyDown(Sender, Key, Qt::NoModifier);
}

void TTemplateForm::ListViewKeyDown(QObject* Sender, quint16& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if ((Shift == Qt::ControlModifier) && (Key == VK_RETURN)) { ButtonWork(ui.EditButton); return; }
    if (Shift != Qt::NoModifier) return;
    switch (Key) {
    case VK_INSERT: ButtonWork(ui.AddButton); break;
    case VK_DELETE: ButtonWork(ui.DeleteButton); break;
    case VK_F2: ButtonWork(ui.EditButton); break;
    }
}

void TTemplateForm::ListView2KeyDown(QObject* Sender, quint16& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if ((Shift == Qt::ControlModifier) && (Key == VK_RETURN)) { ButtonWork(ui.EditButton2); return; }
    if (Shift != Qt::NoModifier) return;
    switch (Key) {
    case VK_INSERT: ButtonWork(ui.AddButton2); break;
    case VK_DELETE: ButtonWork(ui.DeleteButton2); break;
    case VK_F2: ButtonWork(ui.EditButton2); break;
    }
}

void TTemplateForm::ButtonWork(QObject* Sender)
{
    TGame *G = nullptr, *G2;
    QString GName;
    QPoint P;
    QString S;
    TList *L;
    int I;

    switch (Sender->property("tag").toInt()) {
    case 0: close(); break;
    case 1:
        // Template: Use button
        P = ToolButtonScreenPos(ui.ToolBar, ui.UseButton);
        ui.UsePopupMenu->popup(P);
        break;
    case 2:
        // Template: Add button
        P = ToolButtonScreenPos(ui.ToolBar, ui.AddButton);
        ui.AddPopupMenu->popup(P);
        break;
    case 3:
        // Template: Edit button
        if ((ui.ListView->Selected() == nullptr) || (ui.ListView->Selected()->Data() == nullptr)) {
            MessageDlg(LanguageSetup->MessageNoGameSelected(), mtError, mbOK, 0); return;
        }
        // Because "Default template" and "Default ScummVM template" are the same object, cannot handle them in one list, IndexOf will not find the right one
        if (ItemGame(ui.ListView->Selected()) == DefaultTemplate) {
            // Default template or ScummVM default template
            TList LL, LL2;
            G = ItemGame(ui.ListView->Selected());
            LL.append(G);
            if (ui.ListView->Selected()->Caption() == LanguageSetup->TemplateFormDefault()) LL2.append(reinterpret_cast<void *>(intptr_t(1))); else LL2.append(reinterpret_cast<void *>(intptr_t(2)));
            GName = ui.ListView->Selected()->Caption();
            if (PrgSetup->DFendStyleProfileEditor()) {
                if (!EditGameTemplate(this, TemplateDB, G, nullptr, SearchLinkFile, DeleteOnExit, &LL, &LL2)) return;
            } else {
                if (!ModernEditGameTemplate(this, TemplateDB, G, nullptr, SearchLinkFile, DeleteOnExit, &LL, &LL2)) return;
            }
            G->LoadCache();
            LoadList();
            SelectGame(nullptr, GName);
        } else {
            // Normal template
            TList LL;
            for (I = 0; I < ui.ListView->Items()->Count(); I++) {
                if (ItemGame(ui.ListView->Items()->Item(I)) == DefaultTemplate) continue;
                LL.append(ui.ListView->Items()->Item(I)->Data());
            }
            G = ItemGame(ui.ListView->Selected());
            if (PrgSetup->DFendStyleProfileEditor()) {
                if (!EditGameTemplate(this, TemplateDB, G, nullptr, SearchLinkFile, DeleteOnExit, &LL, nullptr)) return;
            } else {
                if (!ModernEditGameTemplate(this, TemplateDB, G, nullptr, SearchLinkFile, DeleteOnExit, &LL, nullptr)) return;
            }
            G->LoadCache();
            LoadList();
            SelectGame(G, QString());
        }
        break;
    case 4:
        // Template: Delete buton
        if ((ui.ListView->Selected() == nullptr) || (ui.ListView->Selected()->Data() == nullptr)) {
            MessageDlg(LanguageSetup->MessageNoGameSelected(), mtError, mbOK, 0); return;
        }
        G = ItemGame(ui.ListView->Selected());
        if (PrgSetup->AskBeforeDelete()) {
            if (MessageDlg(Format(LanguageSetup->MessageConfirmationDeleteRecord(), {G->Name()}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
        }
        if (!TemplateDB->Delete(G)) return;
        LoadList();
        break;
    case 5:
        // Template: Use for new profile
        if ((ui.ListView->Selected() == nullptr) || (ui.ListView->Selected()->Data() == nullptr)) {
            MessageDlg(LanguageSetup->MessageNoGameSelected(), mtError, mbOK, 0); return;
        }
        Template = ItemGame(ui.ListView->Selected());
        TemplateScummVM = (ItemGame(ui.ListView->Selected()) == DefaultTemplate) && (ui.ListView->Selected()->Caption() != LanguageSetup->TemplateFormDefault());
        close();
        break;
    case 6:
        // Template: Use a new default template
        if ((ui.ListView->Selected() == nullptr) || (ui.ListView->Selected()->Data() == nullptr)) {
            MessageDlg(LanguageSetup->MessageNoGameSelected(), mtError, mbOK, 0); return;
        }
        DefaultTemplate->AssignFromButKeepScummVMSettings(ItemGame(ui.ListView->Selected()));
        DefaultTemplate->SetName(QString());
        LoadList();
        break;
    case 7:
        // Template: Add new
        G = nullptr;
        if (PrgSetup->DFendStyleProfileEditor()) {
            if (!EditGameTemplate(this, TemplateDB, G, DefaultTemplate, SearchLinkFile, DeleteOnExit)) return;
        } else {
            if (!ModernEditGameTemplate(this, TemplateDB, G, DefaultTemplate, SearchLinkFile, DeleteOnExit)) return;
        }
        G->LoadCache();
        LoadList();
        SelectGame(G, QString());
        break;
    case 8:
        // Template: Add from profile
        G2 = SelectProfile(this, GameDB);
        if (G2 == nullptr) return;
        G = nullptr;
        if (PrgSetup->DFendStyleProfileEditor()) {
            if (!EditGameTemplate(this, TemplateDB, G, G2, SearchLinkFile, DeleteOnExit)) return;
        } else {
            if (!ModernEditGameTemplate(this, TemplateDB, G, G2, SearchLinkFile, DeleteOnExit)) return;
        }
        G->LoadCache();
        LoadList();
        SelectGame(G, QString());
        break;
    case 9:
        // Template: Copy
        if ((ui.ListView->Selected() == nullptr) || (ui.ListView->Selected()->Data() == nullptr)) {
            MessageDlg(LanguageSetup->MessageNoGameSelected(), mtError, mbOK, 0); return;
        }
        if ((ItemGame(ui.ListView->Selected()) == DefaultTemplate) && (ui.ListView->Selected()->Caption() != LanguageSetup->TemplateFormDefault())) return;
        S = ItemGame(ui.ListView->Selected())->CacheName;
        if (!InputQuery(LanguageSetup->MenuProfileCopyTitle(), LanguageSetup->MenuProfileCopyPrompt(), S, this)) return;
        G = TemplateDB->Game(TemplateDB->Add(S));
        G->AssignFrom(ItemGame(ui.ListView->Selected()));
        G->SetName(S);
        G->LoadCache();
        G->StoreAllValues();
        LoadList();
        SelectGame(G, QString());
        break;
    case 10:
        // AutoSetup: Add button
        P = ToolButtonScreenPos(ui.ToolBar2, ui.AddButton2);
        ui.AddPopupMenu2->popup(P);
        break;
    case 11: {
        // AutoSetup: Edit button
        if ((ui.ListView2->Selected() == nullptr) || (ui.ListView2->Selected()->Data() == nullptr)) {
            MessageDlg(LanguageSetup->MessageNoGameSelected(), mtError, mbOK, 0); return;
        }
        TList LL;
        for (I = 0; I < ui.ListView2->Items()->Count(); I++) LL.append(ui.ListView2->Items()->Item(I)->Data());
        G = ItemGame(ui.ListView2->Selected());
        if (PrgSetup->DFendStyleProfileEditor()) {
            if (!EditGameTemplate(this, AutoSetupDB, G, nullptr, SearchLinkFile, DeleteOnExit, &LL)) return;
        } else {
            if (!ModernEditGameTemplate(this, AutoSetupDB, G, nullptr, SearchLinkFile, DeleteOnExit, &LL)) return;
        }
        G->LoadCache();
        LoadList2();
        SelectGame2(G);
        break;
    }
    case 12:
        // AutoSetup: Del button
        if ((ui.ListView2->Selected() == nullptr) || (ui.ListView2->Selected()->Data() == nullptr)) {
            MessageDlg(LanguageSetup->MessageNoGameSelected(), mtError, mbOK, 0); return;
        }
        G = ItemGame(ui.ListView2->Selected());
        if (PrgSetup->AskBeforeDelete()) {
            if (MessageDlg(Format(LanguageSetup->MessageConfirmationDeleteRecord(), {G->Name()}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
        }
        if (!AutoSetupDB->Delete(G)) return;
        LoadList2();
        break;
    case 13:
        // AutoSetup: Add new
        G = nullptr;
        if (PrgSetup->DFendStyleProfileEditor()) {
            if (!EditGameTemplate(this, AutoSetupDB, G, DefaultTemplate, SearchLinkFile, DeleteOnExit)) return;
        } else {
            if (!ModernEditGameTemplate(this, AutoSetupDB, G, DefaultTemplate, SearchLinkFile, DeleteOnExit)) return;
        }
        G->LoadCache();
        LoadList2();
        SelectGame2(G);
        break;
    case 14:
        // AutoSetup: Add from profile
        L = nullptr;
        if (!ShowSelectProfilesDialog(this, GameDB, L)) return;
        for (I = 0; I < L->size(); I++) {
            TGame *LG = static_cast<TGame *>((*L)[I]);
            // Create Checksums if there are none
            CreateGameCheckSum(LG, false);
            CreateSetupCheckSum(LG, false);
            AddAdditionalChecksumDataToAutoSetupTemplate(LG);

            // Copy to template
            G = AutoSetupDB->Game(AutoSetupDB->Add(LG->Name()));
            G->AssignFrom(LG);

            // Delete not template-useable values
            RemoveNonAutoSetupValues(G);
            G->LoadCache();
            G->StoreAllValues();
        }
        LoadList2();
        SelectGame2(G);
        delete L;
        break;
    case 15:
        // AutoSetup: Copy
        if ((ui.ListView2->Selected() == nullptr) || (ui.ListView2->Selected()->Data() == nullptr)) {
            MessageDlg(LanguageSetup->MessageNoGameSelected(), mtError, mbOK, 0); return;
        }
        S = ItemGame(ui.ListView2->Selected())->CacheName;
        if (!InputQuery(LanguageSetup->MenuProfileCopyTitle(), LanguageSetup->MenuProfileCopyPrompt(), S, this)) return;
        G = AutoSetupDB->Game(AutoSetupDB->Add(S));
        G->AssignFrom(ItemGame(ui.ListView2->Selected()));
        G->SetName(S);
        G->LoadCache();
        G->StoreAllValues();
        LoadList2();
        SelectGame2(G);
        break;
    case 16:
        // Template: Multi edit
        if (!ShowMultipleProfilesEditorDialog(this, TemplateDB, true)) return;
        if (ui.ListView->Selected() == nullptr) { G = nullptr; GName.clear(); } else {
            G = ItemGame(ui.ListView->Selected());
            if (G == DefaultTemplate) { G = nullptr; GName = ui.ListView->Selected()->Caption(); } else GName.clear();
        }
        LoadList();
        SelectGame(G, GName);
        break;
    case 17:
        // AutoSetup: Multi edit
        if (!ShowMultipleProfilesEditorDialog(this, AutoSetupDB, true)) return;
        if (ui.ListView2->Selected() == nullptr) G = nullptr; else G = ItemGame(ui.ListView2->Selected());
        LoadList2();
        SelectGame2(G);
        break;
    case 18: ApplicationHelpContext(ID_ExtrasTemplates); break;
    case 19:
        // AutoSetup: Check for duplicate checksums
        DoubleChecksumCheck();
        break;
    }
}

void TTemplateForm::FormKeyDown(QObject* Sender, quint16& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if ((Key == VK_F1) && (Shift == Qt::NoModifier)) ButtonWork(ui.HelpButton);
}

// global

void RemoveNonAutoSetupValues(TGame* G)
{
    QString S, T;
    if (!Trim(G->GameExe()).isEmpty()) G->SetGameExe(ExtractFileName(G->GameExe()));
    if (!Trim(G->SetupExe()).isEmpty()) G->SetSetupExe(ExtractFileName(G->SetupExe()));
    G->SetCaptureFolder(QString());
    G->SetDataDir(QString());
    G->SetExtraDirs(QString());
    G->SetExtraFiles(QString());
    G->SetCustomDOSBoxDir(QString());

    if (!Trim(G->Icon()).isEmpty()) {
        S = ExtUpperCase(MakeAbsPath(G->Icon(), PrgSetup->BaseDir()));
        T = ExtUpperCase(MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir()));
        if (Copy(S, 1, Length(T)) == T) G->SetIcon(QString());
    }
}

TGame* ShowTemplateDialog(QWidget* AOwner, TGameDB* AGameDB, TLinkFile* ASearchLinkFile, TStringList* ADeleteOnExit)
{
    TGame* result = nullptr;
    TemplateForm = new TTemplateForm(AOwner);
    TemplateForm->GameDB = AGameDB;
    TemplateForm->SearchLinkFile = ASearchLinkFile;
    TemplateForm->DeleteOnExit = ADeleteOnExit;
    TemplateForm->ShowModal();
    if (TemplateForm->Template != nullptr) {
        if (TemplateForm->TemplateScummVM) TemplateForm->Template->SetProfileMode("ScummVM");
        if (PrgSetup->DFendStyleProfileEditor() && (!TemplateForm->TemplateScummVM)) {
            EditGameProfil(AOwner, AGameDB, result, TemplateForm->Template, ASearchLinkFile, ADeleteOnExit, QString(), QString());
        } else {
            ModernEditGameProfil(AOwner, AGameDB, result, TemplateForm->Template, ASearchLinkFile, ADeleteOnExit, QString(), QString());
        }
        if (TemplateForm->TemplateScummVM) TemplateForm->Template->SetProfileMode("DOSBox");
    }
    delete TemplateForm;
    TemplateForm = nullptr;
    return result;
}

void TTemplateForm::DoubleChecksumCheck()
{
    TStringList Checksums, Templates, Empty, NotUnique;
    int I, J;
    QString S;
    TWaitForm* WaitForm;

    if (AutoSetupDB->Count() > 50) WaitForm = CreateWaitForm(this, LanguageSetup->TemplateFormEditCheckChecksumsCheckingDB(), AutoSetupDB->Count()); else WaitForm = nullptr;
    for (I = 0; I < AutoSetupDB->Count(); I++) {
        TGame *AG = AutoSetupDB->Game(I);
        if ((WaitForm != nullptr) && ((I % 10) == 0)) WaitForm->Step(I);
        if (Trim(AG->GameExe()).isEmpty()) continue;
        S = AG->GameExeMD5();
        if (S.isEmpty()) { Empty.Add(AG->CacheName); continue; }
        J = Checksums.IndexOf(S);
        if (J < 0) {
            Checksums.Add(S);
            Templates.Add(AG->CacheName);
        } else {
            if (!Trim(AG->AddtionalChecksumFile1Checksum()).isEmpty()) continue;
            if (!Trim(AG->AddtionalChecksumFile2Checksum()).isEmpty()) continue;
            if (!Trim(AG->AddtionalChecksumFile3Checksum()).isEmpty()) continue;
            if (!Trim(AG->AddtionalChecksumFile4Checksum()).isEmpty()) continue;
            if (!Trim(AG->AddtionalChecksumFile5Checksum()).isEmpty()) continue;
            NotUnique.Add(AG->CacheName + " " + Format(LanguageSetup->TemplateFormEditCheckChecksumsSameChecksum(), {Templates[J]}));
        }
    }
    delete WaitForm;

    if (Empty.Count() + NotUnique.Count() == 0) {
        MessageDlg(LanguageSetup->TemplateFormEditCheckChecksumsMessageOK(), mtInformation, mbOK, 0);
    } else {
        TStringList St;
        if (Empty.Count() > 0) {
            St.Add(LanguageSetup->TemplateFormEditCheckChecksumsHeadingMissingChecksums());
            St.AddStrings(Empty);
            St.Add(QString());
        }
        St.Add(LanguageSetup->TemplateFormEditCheckChecksumsHeadingNonUniqueChecksums());
        St.AddStrings(NotUnique);
        ShowInfoTextDialog(this, LanguageSetup->TemplateFormEditCheckChecksumsWarningTitle(), &St, true);
    }
}
