#include "ComCtrls.h"

#include <QTreeView>
#include <QListView>
#include <QStackedLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <algorithm>
#include <QScopedValueRollback>
#include <QGuiApplication>

namespace {
// gives access to the protected QTreeView::sizeHintForColumn
class TReportView : public QTreeView
{
public:
    using QTreeView::QTreeView;
    int contentWidth(int Column) const { return sizeHintForColumn(Column); }
};
}

// ---------------------------------------------------------------------------
// TSubItems
// ---------------------------------------------------------------------------

int TSubItems::Add(const QString &S)
{
    FList.append(S);
    FOwner->Changed();
    return FList.size() - 1;
}

void TSubItems::Insert(int Index, const QString &S)
{
    FList.insert(qBound(0, Index, FList.size()), S);
    FOwner->Changed();
}

void TSubItems::Delete(int Index)
{
    if (Index >= 0 && Index < FList.size()) { FList.removeAt(Index); FOwner->Changed(); }
}

void TSubItems::Clear()
{
    FList.clear();
    FOwner->Changed();
}

void TSubItems::SetStrings(int I, const QString &S)
{
    while (FList.size() <= I) FList.append(QString());
    FList[I] = S;
    FOwner->Changed();
}

void TSubItems::Assign(const QStringList &L)
{
    FList = L;
    FOwner->Changed();
}

// ---------------------------------------------------------------------------
// TListItem
// ---------------------------------------------------------------------------

TListItem::TListItem(TListView *Owner) : FOwner(Owner), FSubItems(this) {}

void TListItem::Changed()
{
    if (FOwner) FOwner->itemChanged(this);
}

void TListItem::SetCaption(const QString &S) { FCaption = S; Changed(); }
void TListItem::SetImageIndex(int I) { FImageIndex = I; Changed(); }

void TListItem::SetChecked(bool B)
{
    if (FChecked == B) return;
    FChecked = B;
    Changed();
}

int TListItem::Index() const { return FOwner->indexOfItem(this); }

bool TListItem::Selected() const
{
    return FOwner->SelectedItems().contains(const_cast<TListItem *>(this));
}

void TListItem::SetSelected(bool B)
{
    QAbstractItemView *V = FOwner->currentView();
    int Row = Index();
    if (Row < 0) return;
    QModelIndex I = V->model()->index(Row, 0);
    V->selectionModel()->select(I, (B ? QItemSelectionModel::Select : QItemSelectionModel::Deselect) | QItemSelectionModel::Rows);
    if (B && !FOwner->MultiSelect()) V->selectionModel()->setCurrentIndex(I, QItemSelectionModel::NoUpdate);
}

bool TListItem::Focused() const
{
    return FOwner->ItemFocused() == this;
}

void TListItem::SetFocused(bool B)
{
    if (B) FOwner->SetItemFocused(this);
}

void TListItem::MakeVisible(bool PartialOK)
{
    Q_UNUSED(PartialOK);
    int Row = Index();
    if (Row < 0) return;
    QAbstractItemView *V = FOwner->currentView();
    V->scrollTo(V->model()->index(Row, 0));
}

void TListItem::Delete()
{
    int Row = Index();
    if (Row >= 0) FOwner->Items()->Delete(Row);
}

void TListItem::Update() { Changed(); }

void TListItem::SetFont(const QFont &F) { FFont = F; FHasFont = true; Changed(); }
void TListItem::SetTextColor(const QColor &C) { FTextColor = C; Changed(); }
void TListItem::SetBackgroundColor(const QColor &C) { FBackground = C; Changed(); }

// ---------------------------------------------------------------------------
// TListItems
// ---------------------------------------------------------------------------

TListItems::~TListItems()
{
    qDeleteAll(FItems);
}

TListItem *TListItems::Add()
{
    return Insert(FItems.size());
}

TListItem *TListItems::AddItem(TListItem *Item, int Index)
{
    if (Index < 0) Index = FItems.size();
    TListItem *N = Insert(Index);
    if (Item) {
        N->SetCaption(Item->Caption());
        N->SubItems().Assign(Item->SubItems().List());
        N->SetImageIndex(Item->ImageIndex());
        N->SetData(Item->Data());
    }
    return N;
}

TListItem *TListItems::Insert(int Index)
{
    Index = qBound(0, Index, FItems.size());
    auto *I = new TListItem(FOwner);
    if (FUpdateCount == 0) FOwner->FModel->insertRowNotify(Index);
    FItems.insert(Index, I);
    if (FUpdateCount == 0) FOwner->FModel->insertRowDone();
    return I;
}

void TListItems::Delete(int Index)
{
    if (Index < 0 || Index >= FItems.size()) return;
    if (FUpdateCount == 0) FOwner->FModel->removeRowNotify(Index);
    TListItem *I = FItems.takeAt(Index);
    if (FUpdateCount == 0) FOwner->FModel->removeRowDone();
    if (FOwner->FLastCurrent == I) FOwner->FLastCurrent = nullptr;
    delete I;
}

void TListItems::Clear()
{
    if (FUpdateCount == 0) FOwner->FModel->beginReset();
    qDeleteAll(FItems);
    FItems.clear();
    FOwner->FLastCurrent = nullptr;
    if (FUpdateCount == 0) FOwner->FModel->endReset();
}

void TListItems::BeginUpdate()
{
    if (FUpdateCount++ == 0) FOwner->FModel->beginReset();
}

void TListItems::EndUpdate()
{
    if (FUpdateCount == 0) return;
    if (--FUpdateCount == 0) {
        FOwner->FModel->endReset();
        FOwner->applyColumnWidths();
    }
}

void TListItems::Exchange(int A, int B)
{
    if (A < 0 || B < 0 || A >= FItems.size() || B >= FItems.size() || A == B) return;
    FItems.swapItemsAt(A, B);
    FOwner->FModel->rowChanged(A);
    FOwner->FModel->rowChanged(B);
}

// ---------------------------------------------------------------------------
// TListColumn(s)
// ---------------------------------------------------------------------------

void TListColumn::SetCaption(const QString &S)
{
    FCaption = S;
    FOwner->columnsChanged();
}

int TListColumn::Width() const
{
    // like VCL: the stored value is returned (including the auto size values
    // -1/-2); it is updated when the user resizes the column.
    if (FWidth != 0) return FWidth;
    int I = Index();
    if (I >= 0 && FOwner->reportView()->header()->count() > I) return FOwner->reportView()->header()->sectionSize(I);
    return FWidth;
}

void TListColumn::SetWidth(int W)
{
    FWidth = W;
    int I = Index();
    if (I >= 0 && FOwner->reportView()->header()->count() > I) FOwner->applyColumnWidth(I);
}

void TListColumn::SetAlignment(TAlignment A)
{
    FAlignment = A;
    FOwner->refresh();
}

int TListColumn::Index() const
{
    return FOwner->Columns()->FColumns.indexOf(const_cast<TListColumn *>(this));
}

TListColumns::~TListColumns()
{
    qDeleteAll(FColumns);
}

TListColumn *TListColumns::Add()
{
    auto *C = new TListColumn(FOwner);
    FColumns.append(C);
    if (FUpdateCount == 0) FOwner->columnsChanged();
    return C;
}

void TListColumns::Delete(int I)
{
    if (I < 0 || I >= FColumns.size()) return;
    delete FColumns.takeAt(I);
    if (FUpdateCount == 0) FOwner->columnsChanged();
}

void TListColumns::Clear()
{
    qDeleteAll(FColumns);
    FColumns.clear();
    if (FUpdateCount == 0) FOwner->columnsChanged();
}

void TListColumns::BeginUpdate() { FUpdateCount++; }

void TListColumns::EndUpdate()
{
    if (FUpdateCount > 0 && --FUpdateCount == 0) FOwner->columnsChanged();
}

// ---------------------------------------------------------------------------
// Model
// ---------------------------------------------------------------------------

int TListViewModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return FOwner->FItems->FItems.size();
}

int TListViewModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return qMax(1, FOwner->FColumns->Count());
}

QVariant TListViewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= FOwner->FItems->FItems.size()) return QVariant();
    TListItem *I = FOwner->FItems->FItems[index.row()];
    int C = index.column();
    if (FOwner->OnCustomDrawItem && (role == Qt::FontRole || role == Qt::ForegroundRole || role == Qt::BackgroundRole))
        FOwner->OnCustomDrawItem(I);
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return C == 0 ? I->Caption() : I->SubItems().Strings(C - 1);
    case Qt::DecorationRole:
        if (C == 0) {
            TImageList *L = (FOwner->FViewStyle == vsIcon) ? FOwner->FLarge : FOwner->FSmall;
            if (L && I->ImageIndex() >= 0) return L->Icon(I->ImageIndex());
        }
        return QVariant();
    case Qt::CheckStateRole:
        if (C == 0 && FOwner->FCheckboxes) return I->Checked() ? Qt::Checked : Qt::Unchecked;
        return QVariant();
    case Qt::FontRole:
        if (I->HasFont()) return I->Font();
        return QVariant();
    case Qt::ForegroundRole:
        if (I->TextColor().isValid()) return I->TextColor();
        return QVariant();
    case Qt::BackgroundRole:
        if (I->BackgroundColor().isValid()) return I->BackgroundColor();
        return QVariant();
    case Qt::ToolTipRole:
        if (FOwner->OnInfoTip) return FOwner->OnInfoTip(I);
        return QVariant();
    case Qt::TextAlignmentRole: {
        TListColumn *Col = FOwner->FColumns->Item(C);
        if (Col && Col->Alignment() == taRightJustify) return int(Qt::AlignRight | Qt::AlignVCenter);
        if (Col && Col->Alignment() == taCenter) return int(Qt::AlignHCenter | Qt::AlignVCenter);
        if (FOwner->FViewStyle == vsIcon) return int(Qt::AlignHCenter | Qt::AlignTop);
        return QVariant();
    }
    default:
        return QVariant();
    }
}

bool TListViewModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) return false;
    TListItem *I = FOwner->FItems->FItems[index.row()];
    if (role == Qt::CheckStateRole && index.column() == 0) {
        I->SetChecked(value.toInt() == Qt::Checked);
        emit FOwner->itemChecked(I);
        return true;
    }
    if (role == Qt::EditRole && index.column() == 0) {
        QString S = value.toString();
        emit FOwner->itemEdited(I, S);
        I->SetCaption(S);
        return true;
    }
    return false;
}

QVariant TListViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal) return QVariant();
    TListColumn *C = FOwner->FColumns->Item(section);
    if (role == Qt::DisplayRole) return C ? C->Caption() : QString();
    if (role == Qt::TextAlignmentRole && C) {
        if (C->Alignment() == taRightJustify) return int(Qt::AlignRight | Qt::AlignVCenter);
        if (C->Alignment() == taCenter) return int(Qt::AlignHCenter | Qt::AlignVCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }
    return QVariant();
}

Qt::ItemFlags TListViewModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    Qt::ItemFlags F = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
    if (index.column() == 0 && FOwner->FCheckboxes) F |= Qt::ItemIsUserCheckable;
    if (index.column() == 0 && !FOwner->FReadOnly) F |= Qt::ItemIsEditable;
    return F;
}

void TListViewModel::rowChanged(int Row)
{
    emit dataChanged(index(Row, 0), index(Row, columnCount() - 1));
}

// ---------------------------------------------------------------------------
// TListView
// ---------------------------------------------------------------------------

TListView::TListView(QWidget *parent) : QWidget(parent)
{
    FItems = new TListItems(this);
    FColumns = new TListColumns(this);
    FModel = new TListViewModel(this);
    FTree = new TReportView(this);
    FList = new QListView(this);
    FTree->setObjectName("reportView");
    FList->setObjectName("iconView");
    FTree->setModel(FModel);
    FList->setModel(FModel);
    FSel = FTree->selectionModel();
    FList->setSelectionModel(FSel);
    FTree->setRootIsDecorated(false);
    FTree->setUniformRowHeights(true);
    FTree->setAllColumnsShowFocus(true);
    FTree->setItemsExpandable(false);
    FTree->header()->setStretchLastSection(false);
    FTree->header()->setSectionsMovable(false);
    FTree->header()->setSectionsClickable(true);
    FTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    FList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    FList->setMovement(QListView::Static);
    FList->setResizeMode(QListView::Adjust);
    FList->setUniformItemSizes(false);
    FStack = new QStackedLayout(this);
    FStack->setContentsMargins(0, 0, 0, 0);
    FStack->addWidget(FTree);
    FStack->addWidget(FList);
    for (QAbstractItemView *V : {static_cast<QAbstractItemView *>(FTree), static_cast<QAbstractItemView *>(FList)}) {
        V->installEventFilter(this);
        V->viewport()->installEventFilter(this);
        V->setSelectionMode(QAbstractItemView::SingleSelection);
        V->setSelectionBehavior(QAbstractItemView::SelectRows);
        connect(V, &QAbstractItemView::doubleClicked, this, [this](const QModelIndex &) { emit dblClick(); });
        connect(V, &QAbstractItemView::clicked, this, [this](const QModelIndex &) { emit click(); });
    }
    connect(FTree->header(), &QHeaderView::sectionClicked, this, [this](int C) { emit columnClick(C); });
    connect(FTree->header(), &QHeaderView::sectionResized, this, [this](int C, int, int NewSize) {
        // user resized a column (HDN_ENDTRACK in VCL updates the stored width)
        if (FApplyingWidths || !FColumns || !(QGuiApplication::mouseButtons() & Qt::LeftButton)) return;
        if (TListColumn *Col = FColumns->Item(C)) Col->FWidth = NewSize;
    });
    connect(FSel, &QItemSelectionModel::selectionChanged, this, &TListView::onSelectionChanged);
    connect(FSel, &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &, const QModelIndex &) { onCurrentChanged(); });
    setFocusProxy(FTree);
    applyViewStyle();
}

TListView::~TListView()
{
    disconnect(FSel, nullptr, this, nullptr);
    delete FItems;
    FItems = nullptr;
    delete FColumns;
    FColumns = nullptr;
}

QAbstractItemView *TListView::currentView() const
{
    return FViewStyle == vsReport ? static_cast<QAbstractItemView *>(FTree) : static_cast<QAbstractItemView *>(FList);
}

void TListView::onSelectionChanged(const QItemSelection &Sel, const QItemSelection &Desel)
{
    if (!FItems) return;
    for (const QModelIndex &I : Desel.indexes())
        if (I.column() == 0 && I.row() < FItems->FItems.size()) emit selectItem(FItems->FItems[I.row()], false);
    for (const QModelIndex &I : Sel.indexes())
        if (I.column() == 0 && I.row() < FItems->FItems.size()) emit selectItem(FItems->FItems[I.row()], true);
}

void TListView::onCurrentChanged()
{
    if (!FItems) return;
    TListItem *C = ItemFocused();
    if (C != FLastCurrent) {
        FLastCurrent = C;
        emit change(C);
    }
}

TListItem *TListView::Selected() const
{
    QModelIndexList L = FSel->selectedRows(0);
    if (L.isEmpty()) return nullptr;
    // VCL returns the first selected item in list order
    int Best = INT_MAX;
    for (const QModelIndex &I : L) Best = qMin(Best, I.row());
    return FItems->Item(Best);
}

void TListView::SetSelected(TListItem *Item)
{
    if (!Item) { FSel->clearSelection(); return; }
    int Row = Item->Index();
    if (Row < 0) return;
    QModelIndex I = FModel->index(Row, 0);
    FSel->setCurrentIndex(I, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

QList<TListItem *> TListView::SelectedItems() const
{
    QList<TListItem *> R;
    QModelIndexList L = FSel->selectedRows(0);
    std::sort(L.begin(), L.end(), [](const QModelIndex &A, const QModelIndex &B) { return A.row() < B.row(); });
    for (const QModelIndex &I : L) if (TListItem *It = FItems->Item(I.row())) R << It;
    return R;
}

int TListView::SelCount() const { return FSel->selectedRows(0).size(); }

TListItem *TListView::ItemFocused() const
{
    QModelIndex I = FSel->currentIndex();
    return I.isValid() ? FItems->Item(I.row()) : nullptr;
}

void TListView::SetItemFocused(TListItem *Item)
{
    if (!Item) return;
    FSel->setCurrentIndex(FModel->index(Item->Index(), 0), QItemSelectionModel::NoUpdate);
}

int TListView::ItemIndex() const
{
    TListItem *S = Selected();
    return S ? S->Index() : -1;
}

void TListView::SetItemIndex(int I)
{
    SetSelected(FItems->Item(I));
}

TListItem *TListView::GetItemAt(int X, int Y) const
{
    QAbstractItemView *V = currentView();
    QModelIndex I = V->indexAt(V->viewport()->mapFrom(this, QPoint(X, Y)));
    return I.isValid() ? FItems->Item(I.row()) : nullptr;
}

TListItem *TListView::TopItem() const
{
    QAbstractItemView *V = currentView();
    QModelIndex I = V->indexAt(QPoint(2, 2));
    return I.isValid() ? FItems->Item(I.row()) : nullptr;
}

void TListView::ClearSelection() { FSel->clearSelection(); }

void TListView::SelectAll() { currentView()->selectAll(); }

void TListView::SetViewStyle(TViewStyle V)
{
    if (FViewStyle == V && FStack->currentWidget() == currentView()) return;
    FViewStyle = V;
    applyViewStyle();
}

void TListView::applyViewStyle()
{
    QAbstractItemView *V = currentView();
    FStack->setCurrentWidget(V);
    setFocusProxy(V);
    switch (FViewStyle) {
    case vsReport:
        break;
    case vsIcon: {
        FList->setViewMode(QListView::IconMode);
        FList->setFlow(QListView::LeftToRight);
        FList->setWrapping(true);
        int W = FLarge ? FLarge->Width() : 32, H = FLarge ? FLarge->Height() : 32;
        FList->setIconSize(QSize(W, H));
        FList->setGridSize(QSize(qMax(W + 40, 75), H + 2 * fontMetrics().height() + 12));
        FList->setWordWrap(true);
        FList->setSpacing(2);
        break;
    }
    case vsSmallIcon:
        FList->setViewMode(QListView::ListMode);
        FList->setFlow(QListView::LeftToRight);
        FList->setWrapping(true);
        FList->setGridSize(QSize());
        FList->setIconSize(QSize(16, 16));
        break;
    case vsList:
        FList->setViewMode(QListView::ListMode);
        FList->setFlow(QListView::TopToBottom);
        FList->setWrapping(true);
        FList->setGridSize(QSize());
        FList->setIconSize(QSize(16, 16));
        break;
    }
    FModel->reset();
    applyColumnWidths();
}

void TListView::SetSmallImages(TImageList *L)
{
    FSmall = L;
    if (L) FTree->setIconSize(QSize(L->Width(), L->Height()));
    refresh();
}

void TListView::SetLargeImages(TImageList *L)
{
    FLarge = L;
    if (FViewStyle == vsIcon) applyViewStyle(); else refresh();
}

void TListView::SetCheckboxes(bool B) { FCheckboxes = B; refresh(); }

void TListView::SetMultiSelect(bool B)
{
    FMultiSelect = B;
    QAbstractItemView::SelectionMode M = B ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection;
    FTree->setSelectionMode(M);
    FList->setSelectionMode(M);
}

void TListView::SetReadOnly(bool B)
{
    FReadOnly = B;
    QAbstractItemView::EditTriggers T = B ? QAbstractItemView::NoEditTriggers : (QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    FTree->setEditTriggers(T);
    FList->setEditTriggers(T);
}

void TListView::SetShowColumnHeaders(bool B)
{
    FShowHeaders = B;
    FTree->setHeaderHidden(!B);
}

void TListView::SetHideSelection(bool B)
{
    FHideSelection = B;
    for (QAbstractItemView *V : {static_cast<QAbstractItemView *>(FTree), static_cast<QAbstractItemView *>(FList)}) {
        QPalette P = V->palette();
        if (!B) {
            P.setColor(QPalette::Inactive, QPalette::Highlight, P.color(QPalette::Active, QPalette::Highlight));
            P.setColor(QPalette::Inactive, QPalette::HighlightedText, P.color(QPalette::Active, QPalette::HighlightedText));
        }
        V->setPalette(P);
    }
}

void TListView::SetSortType(TSortType S)
{
    FSortType = S;
    if (S != stNone) AlphaSort();
}

void TListView::SetColor(const QColor &C)
{
    for (QAbstractItemView *V : {static_cast<QAbstractItemView *>(FTree), static_cast<QAbstractItemView *>(FList)}) {
        QPalette P = V->palette();
        P.setColor(QPalette::Base, C);
        V->setPalette(P);
    }
}

QColor TListView::Color() const { return FTree->palette().color(QPalette::Base); }

void TListView::SetFontColor(const QColor &C)
{
    for (QAbstractItemView *V : {static_cast<QAbstractItemView *>(FTree), static_cast<QAbstractItemView *>(FList)}) {
        QPalette P = V->palette();
        P.setColor(QPalette::Text, C);
        V->setPalette(P);
    }
}

void TListView::AlphaSort()
{
    if (OnCompare) { CustomSort(OnCompare); return; }
    CustomSort([](TListItem *A, TListItem *B) { return QString::compare(A->Caption(), B->Caption(), Qt::CaseInsensitive); });
}

void TListView::CustomSort(const std::function<int(TListItem *, TListItem *)> &Compare)
{
    QList<TListItem *> Sel = SelectedItems();
    TListItem *Cur = ItemFocused();
    FModel->beginReset();
    std::stable_sort(FItems->FItems.begin(), FItems->FItems.end(), [&](TListItem *A, TListItem *B) { return Compare(A, B) < 0; });
    FModel->endReset();
    for (TListItem *I : Sel) I->SetSelected(true);
    if (Cur) SetItemFocused(Cur);
}

void TListView::Arrange()
{
    FList->doItemsLayout();
}

void TListView::itemChanged(TListItem *Item)
{
    if (!FItems || FItems->FUpdateCount > 0) return;
    int Row = FItems->IndexOf(Item);
    if (Row >= 0) FModel->rowChanged(Row);
}

void TListView::itemsReset() { FModel->reset(); }

void TListView::columnsChanged()
{
    FModel->reset();
    FModel->headersChanged();
    applyColumnWidths();
}

// VCL: Width -1 = LVSCW_AUTOSIZE (fit the contents), -2 = LVSCW_AUTOSIZE_USEHEADER
// (fit contents and header; the last column fills the remaining space).
void TListView::applyColumnWidth(int I)
{
    QHeaderView *H = FTree->header();
    QScopedValueRollback<bool> Guard(FApplyingWidths, true);
    if (I < 0 || I >= FColumns->Count() || I >= H->count()) return;
    int W = FColumns->Item(I)->FWidth;
    if (W >= 0) { H->resizeSection(I, W); return; }
    // measure the texts directly (works before the view has been laid out)
    QFontMetrics FM(FTree->font());
    QFontMetrics FMB([this]() { QFont F = FTree->font(); F.setBold(true); return F; }());
    int Content = 0;
    for (int R = 0; R < FItems->Count(); R++) {
        TListItem *It = FItems->Item(R);
        QString T = (I == 0) ? It->Caption() : It->SubItems().Strings(I - 1);
        int Wt = (It->HasFont() ? QFontMetrics(It->Font()) : (OnCustomDrawItem ? FMB : FM)).horizontalAdvance(T);
        if (I == 0) {
            if (FSmall && It->ImageIndex() >= 0) Wt += FSmall->Width() + 6;
            if (FCheckboxes) Wt += 20;
        }
        Content = qMax(Content, Wt);
    }
    Content += 14;
    int Size = Content;
    if (W == -2) {
        Size = qMax(Content, H->sectionSizeHint(I));
        if (I == FColumns->Count() - 1) {
            int Used = 0;
            for (int J = 0; J < I; J++) Used += H->sectionSize(J);
            Size = qMax(Size, FTree->viewport()->width() - Used);
        }
    }
    H->resizeSection(I, qMax(Size, 20));
}

void TListView::applyColumnWidths()
{
    for (int i = 0; i < FColumns->Count() && i < FTree->header()->count(); i++) applyColumnWidth(i);
}

void TListView::refresh()
{
    if (FItems && FItems->FUpdateCount == 0) FModel->reset();
}

QString TListView::viewStyleName() const
{
    switch (FViewStyle) {
    case vsReport: return "vsReport";
    case vsSmallIcon: return "vsSmallIcon";
    case vsList: return "vsList";
    default: return "vsIcon";
    }
}

void TListView::setViewStyleName(const QString &S)
{
    if (S == "vsReport") SetViewStyle(vsReport);
    else if (S == "vsSmallIcon") SetViewStyle(vsSmallIcon);
    else if (S == "vsList") SetViewStyle(vsList);
    else SetViewStyle(vsIcon);
}

QStringList TListView::columnCaptions() const
{
    QStringList R;
    for (int i = 0; i < FColumns->Count(); i++) R << FColumns->Item(i)->Caption();
    return R;
}

void TListView::setColumnCaptions(const QStringList &L)
{
    FColumns->BeginUpdate();
    FColumns->Clear();
    for (const QString &S : L) FColumns->Add()->FCaption = S;
    FColumns->EndUpdate();
}

QString TListView::columnWidths() const
{
    QStringList R;
    for (int i = 0; i < FColumns->Count(); i++) R << QString::number(FColumns->Item(i)->Width());
    return R.join(',');
}

void TListView::setColumnWidths(const QString &S)
{
    QStringList L = S.split(',', Qt::SkipEmptyParts);
    for (int i = 0; i < L.size() && i < FColumns->Count(); i++) FColumns->Item(i)->FWidth = L[i].trimmed().toInt();
    applyColumnWidths();
}

QString TListView::sortTypeName() const
{
    switch (FSortType) {
    case stData: return "stData";
    case stText: return "stText";
    case stBoth: return "stBoth";
    default: return "stNone";
    }
}

void TListView::setSortTypeName(const QString &S)
{
    if (S == "stData") FSortType = stData;
    else if (S == "stText") FSortType = stText;
    else if (S == "stBoth") FSortType = stBoth;
    else FSortType = stNone;
}

bool TListView::eventFilter(QObject *O, QEvent *E)
{
    if (E->type() == QEvent::KeyPress && (O == FTree || O == FList)) {
        auto *K = static_cast<QKeyEvent *>(E);
        emit keyDown(K->key(), K->modifiers());
    }
    if (E->type() == QEvent::MouseButtonPress && (O == FTree->viewport() || O == FList->viewport())) {
        auto *M = static_cast<QMouseEvent *>(E);
        if (M->button() == Qt::RightButton) {
            // VCL selects the item under the mouse before showing the popup menu
            QAbstractItemView *V = currentView();
            QModelIndex I = V->indexAt(M->position().toPoint());
            if (I.isValid() && !FSel->isSelected(I))
                FSel->setCurrentIndex(I, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
    }
    if (E->type() == QEvent::ContextMenu && (O == FTree->viewport() || O == FList->viewport())) {
        auto *C = static_cast<QContextMenuEvent *>(E);
        emit contextPopup(C->globalPos());
        if (contextMenuPolicy() == Qt::CustomContextMenu) {
            emit customContextMenuRequested(mapFromGlobal(C->globalPos()));
            return true;
        }
    }
    return QWidget::eventFilter(O, E);
}
