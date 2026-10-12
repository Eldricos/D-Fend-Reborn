// C++ port of SetupFrameGamesListColumnsUnit.pas (implementation section).
#include "SetupFrameGamesListColumnsUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "GameDBToolsUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"

namespace {

// TCheckListBox helpers (Items.Objects = Qt::UserRole)
void AddObject(QListWidget *L, const QString &S, int Obj)
{
    auto *It = new QListWidgetItem(S, L);
    It->setFlags(It->flags() | Qt::ItemIsUserCheckable);
    It->setCheckState(Qt::Unchecked);
    It->setData(Qt::UserRole, Obj);
}
int Obj(QListWidget *L, int I) { return L->item(I)->data(Qt::UserRole).toInt(); }
bool Checked(QListWidget *L, int I) { return L->item(I)->checkState() == Qt::Checked; }
void SetChecked(QListWidget *L, int I, bool B) { L->item(I)->setCheckState(B ? Qt::Checked : Qt::Unchecked); }
int IndexOf(QListWidget *L, const QString &S)
{
    for (int I = 0; I < L->count(); I++) if (L->item(I)->text().compare(S, Qt::CaseInsensitive) == 0) return I;
    return -1;
}
// Items.Exchange (the moved item stays selected)
void Exchange(QListWidget *L, int I1, int I2)
{
    int Cur = L->currentRow();
    int A = qMin(I1, I2), B = qMax(I1, I2);
    QListWidgetItem *ItB = L->takeItem(B);
    QListWidgetItem *ItA = L->takeItem(A);
    L->insertItem(A, ItB);
    L->insertItem(B, ItA);
    if (Cur == I1) L->setCurrentRow(I2); else if (Cur == I2) L->setCurrentRow(I1); else L->setCurrentRow(Cur);
}
// S[I] (1-based) with #0 for indices beyond the string
QChar CharAt(const QString &S, int I) { return (I >= 1 && I <= S.length()) ? S[I - 1] : QChar(); }

}  // namespace

TSetupFrameGamesListColumns::TSetupFrameGamesListColumns(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM event wiring
    connect(ui.ListViewUpButton, &QToolButton::clicked, this, [this]() { ListViewMoveButtonClick(ui.ListViewUpButton); });
    connect(ui.ListViewDownButton, &QToolButton::clicked, this, [this]() { ListViewMoveButtonClick(ui.ListViewDownButton); });
    connect(ui.AddButton, &QToolButton::clicked, this, [this]() { ListViewMoveButtonClick(ui.AddButton); });
    connect(ui.DelButton, &QToolButton::clicked, this, [this]() { ListViewMoveButtonClick(ui.DelButton); });
    connect(ui.ListViewListBox, &QListWidget::currentRowChanged, this, [this](int) { ListViewListBoxClick(ui.ListViewListBox); });
}

TSetupFrameGamesListColumns::~TSetupFrameGamesListColumns()
{
    delete UserDataList;
}

/* TSetupFrameGamesListColumns */

QString TSetupFrameGamesListColumns::GetName()
{
    return LanguageSetup->SetupFormListViewSheet1();
}

void TSetupFrameGamesListColumns::InitGUIAndLoadSetup(TInitData& InitData)
{
    QString ColOrder, ColVisible, ColVisibleUser;
    int I, J, Nr;
    bool B;
    TStringList* ColVisibleUserSt;
    QListWidget *L = ui.ListViewListBox;

    /* NoFlicker(ListViewListBox); - will cause trouble */
    // NoFlicker(...): not needed in Qt

    if (InitData.GameDB != nullptr) { delete UserDataList; UserDataList = GetUserDataList(InitData.GameDB); }

    GetColOrderAndVisible(ColOrder, ColVisible, ColVisibleUser);
    ColVisibleUserSt = ValueToList(ColVisibleUser);
    for (I = 0; I <= 7 + ColVisibleUserSt->Count(); I++) {
        Nr = -1;
        QChar C = CharAt(ColOrder, I + 1);
        if (C >= '1' && C <= '9') Nr = StrToInt(QString(C));
        else if (C >= 'A' && C <= 'Z') Nr = C.unicode() - 'A' + 10;
        else if (C >= 'a' && C <= 'z') Nr = C.unicode() - 'a' + 10;
        if (Nr < 1) continue;
        if (Nr > 8) {
            if (Nr - 8 > ColVisibleUserSt->Count()) continue;
        }
        switch (Nr - 1) {
            case 0: AddObject(L, LanguageSetup->GameSetup(), Nr - 1); break;
            case 1: AddObject(L, LanguageSetup->GameGenre(), Nr - 1); break;
            case 2: AddObject(L, LanguageSetup->GameDeveloper(), Nr - 1); break;
            case 3: AddObject(L, LanguageSetup->GamePublisher(), Nr - 1); break;
            case 4: AddObject(L, LanguageSetup->GameYear(), Nr - 1); break;
            case 5: AddObject(L, LanguageSetup->GameLanguage(), Nr - 1); break;
            case 6: AddObject(L, LanguageSetup->GameNotes(), Nr - 1); break;
            case 7: AddObject(L, LanguageSetup->GameStartCount(), Nr - 1); break;
            default: AddObject(L, (*ColVisibleUserSt)[Nr - 9], 100); break;
        }
        SetChecked(L, L->count() - 1, CharAt(ColVisible, Nr) != '0');
    }
    for (I = 0; I <= 7; I++) {
        B = false;
        for (J = 0; J < L->count(); J++) if (Obj(L, J) == I) {
            B = true; break;
        }
        if (B) continue;
        switch (I) {
            case 0: AddObject(L, LanguageSetup->GameSetup(), I); break;
            case 1: AddObject(L, LanguageSetup->GameGenre(), I); break;
            case 2: AddObject(L, LanguageSetup->GameDeveloper(), I); break;
            case 3: AddObject(L, LanguageSetup->GamePublisher(), I); break;
            case 4: AddObject(L, LanguageSetup->GameYear(), I); break;
            case 5: AddObject(L, LanguageSetup->GameLanguage(), I); break;
            case 6: AddObject(L, LanguageSetup->GameNotes(), I); break;
            case 7: AddObject(L, LanguageSetup->GameStartCount(), I); break;
        }
        SetChecked(L, L->count() - 1, CharAt(ColVisible, I + 1) != '0');
    }
    delete ColVisibleUserSt;

    UserIconLoader->DialogImage(DI_Up, ui.ListViewUpButton);
    UserIconLoader->DialogImage(DI_Down, ui.ListViewDownButton);
    UserIconLoader->DialogImage(DI_Add, ui.AddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.DelButton);

    ui.GridLinesCheckBox->setChecked(PrgSetup->GridLinesInGamesList());
    ui.StoreColumnWidthsCheckBox->setChecked(PrgSetup->StoreColumnWidths());

    ListViewListBoxClick(this);
}

void TSetupFrameGamesListColumns::BeforeChangeLanguage()
{
}

void TSetupFrameGamesListColumns::LoadLanguage()
{
    int I, J;
    QListWidget *L = ui.ListViewListBox;

    ui.ListViewLabel->setText(LanguageSetup->SetupFormListViewInfo());
    ui.ListViewUpButton->setToolTip(RemoveUnderline(LanguageSetup->MoveUp()));
    ui.ListViewDownButton->setToolTip(RemoveUnderline(LanguageSetup->MoveDown()));
    ui.AddButton->setToolTip(RemoveUnderline(LanguageSetup->Add()));
    ui.DelButton->setToolTip(RemoveUnderline(LanguageSetup->Del()));
    ui.GridLinesCheckBox->setText(LanguageSetup->SetupFormShowGridLines());
    ui.StoreColumnWidthsCheckBox->setText(LanguageSetup->SetupFormStoreColumnWidths());

    for (I = 0; I < L->count(); I++) {
        J = Obj(L, I);
        switch (J) {
            case 0: L->item(I)->setText(LanguageSetup->GameSetup()); break;
            case 1: L->item(I)->setText(LanguageSetup->GameGenre()); break;
            case 2: L->item(I)->setText(LanguageSetup->GameDeveloper()); break;
            case 3: L->item(I)->setText(LanguageSetup->GamePublisher()); break;
            case 4: L->item(I)->setText(LanguageSetup->GameYear()); break;
            case 5: L->item(I)->setText(LanguageSetup->GameLanguage()); break;
            case 6: L->item(I)->setText(LanguageSetup->GameNotes()); break;
            case 7: L->item(I)->setText(LanguageSetup->GameStartCount()); break;
        }
    }

    setProperty("HelpContext", ID_FileOptionsColumnsInTheGamesList);
}

void TSetupFrameGamesListColumns::DOSBoxDirChanged()
{
}

void TSetupFrameGamesListColumns::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
    ListViewListBoxClick(this);
    InitPopup();
    ui.AddButton->setVisible(ui.PopupMenu->actions().count() > 0);
}

void TSetupFrameGamesListColumns::HideFrame()
{
}

void TSetupFrameGamesListColumns::RestoreDefaults()
{
    TInitData InitData;
    QString S1, S2;

    S1 = PrgSetup->ColVisible();
    S2 = PrgSetup->ColOrder();
    PrgSetup->SetColVisible("11111100");
    PrgSetup->SetColOrder("12345678");
    ui.ListViewListBox->clear();
    InitData.GameDB = nullptr;
    InitGUIAndLoadSetup(InitData);
    PrgSetup->SetColVisible(S1);
    PrgSetup->SetColOrder(S2);

    ui.GridLinesCheckBox->setChecked(false);
    ui.StoreColumnWidthsCheckBox->setChecked(false);
}

void TSetupFrameGamesListColumns::SaveSetup()
{
    QString S;
    int I, J, UserCount;
    bool B;
    QListWidget *L = ui.ListViewListBox;

    /* Remove unchecked user column from list */
    I = 0;
    while (I <= L->count() - 1) {
        if ((Obj(L, I) >= 8) && (!Checked(L, I))) {
            delete L->takeItem(I);
            continue;
        }
        I++;
    }

    /* Store column order */
    S = ""; UserCount = 0;
    for (I = 0; I < L->count(); I++) {
        J = Obj(L, I) + 1;
        if (J >= 100) { J = 9 + UserCount; UserCount++; }
        if (J >= 1 && J <= 9) S = S + IntToStr(J);
        else if (J >= 10 && J <= 35) S = S + QChar('A' + J - 10);
    }
    PrgSetup->SetColOrder(S);

    /* Store column visible state (of the default columns, invisible user columns have already been removed */
    S = "";
    for (I = 0; I <= 8; I++) {
        B = false;
        for (J = 0; J < L->count(); J++) if (Obj(L, J) == I) {
            B = Checked(L, J); break;
        }
        if (B) S = S + "1"; else S = S + "0";
    }
    for (I = 0; I < L->count(); I++) if (Obj(L, I) >= 8) {
        S = S + ";" + L->item(I)->text();
    }
    PrgSetup->SetColVisible(S);

    PrgSetup->SetGridLinesInGamesList(ui.GridLinesCheckBox->isChecked());
    PrgSetup->SetStoreColumnWidths(ui.StoreColumnWidthsCheckBox->isChecked());
}

void TSetupFrameGamesListColumns::ListViewListBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    QListWidget *L = ui.ListViewListBox;
    I = L->currentRow();
    ui.DelButton->setVisible((I >= 0) && (Obj(L, I) >= 8));
    ui.ListViewUpButton->setEnabled(I > 0);
    ui.ListViewDownButton->setEnabled(I < L->count() - 1);
}

void TSetupFrameGamesListColumns::ListViewMoveButtonClick(QObject* Sender)
{
    QString S;
    QListWidget *L = ui.ListViewListBox;

    switch (Sender->property("tag").toInt()) {
        case 0:
            if (L->currentRow() > 0) {
                Exchange(L, L->currentRow(), L->currentRow() - 1);
                ListViewListBoxClick(Sender);
            }
            break;
        case 1:
            if ((L->currentRow() >= 0) && (L->currentRow() < L->count() - 1)) {
                Exchange(L, L->currentRow(), L->currentRow() + 1);
                ListViewListBoxClick(Sender);
            }
            break;
        case 2: {
            InitPopup();
            QPoint P = mapToGlobal(ui.AddButton->pos());
            ui.PopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
            break;
        }
        case 3:
            if ((L->currentRow() >= 0) && (Obj(L, L->currentRow()) >= 7)) {
                delete L->takeItem(L->currentRow());
                InitPopup();
                ui.AddButton->setVisible(ui.PopupMenu->actions().count() > 0);
                ListViewListBoxClick(Sender);
            }
            break;
        case 4: {
            QAction *A = qobject_cast<QAction*>(Sender);
            S = RemoveUnderline(A ? A->text() : QString());
            AddObject(L, S, 100);
            SetChecked(L, L->count() - 1, true);
            InitPopup();
            ui.AddButton->setVisible(ui.PopupMenu->actions().count() > 0);
            ListViewListBoxClick(Sender);
            break;
        }
    }
}

void TSetupFrameGamesListColumns::InitPopup()
{
    QAction* M;
    int I;

    // PopupMenu.Items.Clear (deferred delete: InitPopup is also called from an item's OnClick)
    for (QAction *A : ui.PopupMenu->actions()) { ui.PopupMenu->removeAction(A); A->deleteLater(); }
    if (UserDataList == nullptr) return;
    for (I = 0; I < UserDataList->Count(); I++) {
        if (IndexOf(ui.ListViewListBox, (*UserDataList)[I]) >= 0) continue;
        M = ui.PopupMenu->addAction(MaskUnderlineAmpersand((*UserDataList)[I]));
        M->setProperty("tag", 4);
        connect(M, &QAction::triggered, this, [this, M]() { ListViewMoveButtonClick(M); });
    }
}
