// C++ port of ClassExtensions.pas (implementation section).
// See ClassExtensions.h for how the runtime class change of the Delphi code
// is emulated (behaviour objects attached to the existing widget).
#include "ClassExtensions.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"

#include <QToolTip>
#include <QStyledItemDelegate>
#include <memory>

namespace {

// ---------------------------------------------------------------------------
// WM_DROPFILES / DragAcceptFiles
// ---------------------------------------------------------------------------

class TFileDropHelper : public QObject
{
public:
    TFileDropHelper(QWidget *Owner, bool PassPoint) : QObject(Owner), FOwner(Owner), FPassPoint(PassPoint) {}
    TDragDropEvent OnDragDrop;
    void AddTarget(QWidget *W)
    {
        if (W == nullptr) return;
        W->setAcceptDrops(true);
        if (FTargets.contains(W)) return;
        FTargets.append(W);
        W->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject *O, QEvent *E) override;
private:
    QWidget *FOwner;
    bool FPassPoint;
    QList<QWidget *> FTargets;
    void WMDropFiles(QDropEvent *Message);
};

bool DroppedLocalFiles(const QMimeData *Data, QStringList *Files)
{
    if (Data == nullptr || !Data->hasUrls()) return false;
    bool result = false;
    const QList<QUrl> Urls = Data->urls();
    for (const QUrl &U : Urls) {
        if (!U.isLocalFile()) continue;
        result = true;
        if (Files != nullptr) Files->append(QDir::toNativeSeparators(U.toLocalFile()));
    }
    return result;
}

bool TFileDropHelper::eventFilter(QObject *O, QEvent *E)
{
    Q_UNUSED(O);
    switch (E->type()) {
        case QEvent::DragEnter: {
            auto *D = static_cast<QDragEnterEvent *>(E);
            if (!DroppedLocalFiles(D->mimeData(), nullptr)) return false;
            D->setDropAction(Qt::CopyAction);
            D->accept();
            return true;
        }
        case QEvent::DragMove: {
            auto *D = static_cast<QDragMoveEvent *>(E);
            if (!DroppedLocalFiles(D->mimeData(), nullptr)) return false;
            D->setDropAction(Qt::CopyAction);
            D->accept();
            return true;
        }
        case QEvent::Drop: {
            auto *D = static_cast<QDropEvent *>(E);
            if (!DroppedLocalFiles(D->mimeData(), nullptr)) return false;
            D->setDropAction(Qt::CopyAction);
            D->accept();
            WMDropFiles(D);
            return true;
        }
        default:
            return false;
    }
}

void TFileDropHelper::WMDropFiles(QDropEvent *Message)
{
    QStringList Files;
    DroppedLocalFiles(Message->mimeData(), &Files);
    QPoint P = Message->position().toPoint();   // DragQueryPoint: client (viewport) coordinates

    TDropFilesSource St;
    for (const QString &S : Files) St.Add(S);
    if (OnDragDrop) {
        if (FPassPoint) OnDragDrop(FOwner, &St, P.x(), P.y()); else OnDragDrop(FOwner, &St, -1, -1);
    }
}

TFileDropHelper *FindDropHelper(const QWidget *W)
{
    for (QObject *C : W->children()) if (auto *H = dynamic_cast<TFileDropHelper *>(C)) return H;
    return nullptr;
}

TFileDropHelper *GetDropHelper(QWidget *W, bool PassPoint)
{
    TFileDropHelper *H = FindDropHelper(W);
    if (H == nullptr) H = new TFileDropHelper(W, PassPoint);
    return H;
}

void ActivateAcceptFilesForScrollArea(QAbstractScrollArea *W, bool PassPoint)
{
    TFileDropHelper *H = GetDropHelper(W, PassPoint);
    W->setAcceptDrops(true);
    H->AddTarget(W->viewport());
}

// ---------------------------------------------------------------------------
// CM_HINTSHOW (TListViewSpecialHint)
// ---------------------------------------------------------------------------

class TSpecialHintHelper : public QObject
{
public:
    explicit TSpecialHintHelper(TListView *AListView) : QObject(AListView), FListView(AListView)
    {
        if (FListView->reportView() != nullptr) FListView->reportView()->viewport()->installEventFilter(this);
        if (FListView->iconView() != nullptr) FListView->iconView()->viewport()->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject *O, QEvent *E) override;
private:
    TListView *FListView;
    bool CMHintShow(QWidget *VP, QHelpEvent *Message);
};

bool TSpecialHintHelper::eventFilter(QObject *O, QEvent *E)
{
    if (E->type() != QEvent::ToolTip) return false;
    auto *VP = qobject_cast<QWidget *>(O);
    if (VP == nullptr) return false;
    if (!CMHintShow(VP, static_cast<QHelpEvent *>(E))) QToolTip::hideText();
    return true;
}

bool TSpecialHintHelper::CMHintShow(QWidget *VP, QHelpEvent *Message)
{
    QAbstractItemView *V = nullptr;
    if (FListView->reportView() != nullptr && FListView->reportView()->viewport() == VP) V = FListView->reportView();
    if (FListView->iconView() != nullptr && FListView->iconView()->viewport() == VP) V = FListView->iconView();
    if (V == nullptr) return false;

    QPoint CursorPos = FListView->mapFrom(VP, Message->pos());
    TListItem *Item = FListView->GetItemAt(CursorPos.x(), CursorPos.y());
    if (Item == nullptr) return false;

    if (!FListView->OnInfoTip) return false;
    QString InfoTip = FListView->OnInfoTip(Item);
    if (InfoTip.isEmpty()) return false;

    // ItemRect:=Item.DisplayRect(drBounds)
    QModelIndex Index = V->indexAt(Message->pos());
    QRect ItemRect = V->visualRect(Index);
    if (Index.isValid() && V->model() != nullptr && V->model()->columnCount() > 1)
        ItemRect = ItemRect.united(V->visualRect(V->model()->index(Index.row(), V->model()->columnCount() - 1)));

    // HintPos.X:=P.X+CursorPos.X; HintPos.Y:=P.Y+CursorPos.Y+(GetSystemMetrics(SM_CYCURSOR) div 2);
    QPoint HintPos = Message->globalPos() + QPoint(0, 16);

    // HideTimeout:=100000; HintWindowClass:=TDFRHintWindow (font charset: not needed with Qt's unicode fonts)
    QToolTip::showText(HintPos, InfoTip, VP, ItemRect, 100000);
    return true;
}

void AttachSpecialHint(TListView *ListView)
{
    for (QObject *C : ListView->children()) if (dynamic_cast<TSpecialHintHelper *>(C) != nullptr) return;
    new TSpecialHintHelper(ListView);
}

// ---------------------------------------------------------------------------
// TStringGridEx (TInplaceEditList with pick list)
// ---------------------------------------------------------------------------

int GridProperty(const QTableWidget *Grid, const char *Name, int Default)
{
    QVariant V = Grid->property(Name);
    return V.isValid() ? V.toInt() : Default;
}

class TStringGridExDelegate : public QStyledItemDelegate
{
public:
    explicit TStringGridExDelegate(QTableWidget *Grid) : QStyledItemDelegate(Grid), FGrid(Grid) {}
    TListForCellEvent FOnListForCell;
    TGetListForCellEvent FOnGetListForCell;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        int ACol = index.column() + GridProperty(FGrid, "vclFixedCols", 1);
        int ARow = index.row() + GridProperty(FGrid, "vclFixedRows", 1);
        if (GetEditStyle(ACol, ARow)) {
            // esPickList
            auto *C = new QComboBox(parent);
            C->setEditable(true);
            C->setFrame(false);
            TStringList Items;
            EditListGetItems(ACol, ARow, &Items);
            for (int I = 0; I < Items.Count(); I++) C->addItem(Items[I]);
            return C;
        }
        // esSimple
        return QStyledItemDelegate::createEditor(parent, option, index);
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *C = qobject_cast<QComboBox *>(editor)) {
            C->setCurrentText(index.data(Qt::EditRole).toString());
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        if (auto *C = qobject_cast<QComboBox *>(editor)) {
            model->setData(index, C->currentText(), Qt::EditRole);
            return;
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }

private:
    QTableWidget *FGrid;

    void EditListGetItems(int ACol, int ARow, TStringList *Items) const
    {
        if (FOnGetListForCell) FOnGetListForCell(ACol, ARow, Items);
    }

    // true = esPickList, false = esSimple
    bool GetEditStyle(int ACol, int ARow) const
    {
        bool B = false;
        if (FOnListForCell) FOnListForCell(ACol, ARow, B);
        return B;
    }
};

TStringGridExDelegate *FindGridDelegate(const QTableWidget *Grid)
{
    return dynamic_cast<TStringGridExDelegate *>(Grid->itemDelegate());
}

TStringGridExDelegate *GetGridDelegate(QTableWidget *Grid)
{
    TStringGridExDelegate *D = FindGridDelegate(Grid);
    if (D == nullptr) {
        D = new TStringGridExDelegate(Grid);
        Grid->setItemDelegate(D);
    }
    return D;
}

} // namespace

// { TTreeViewAcceptingFiles }

void TTreeViewAcceptingFiles::ActivateAcceptFiles()
{
    ActivateAcceptFilesForScrollArea(this, true);
}

TDragDropEvent TTreeViewAcceptingFiles::OnDragDrop() const
{
    TFileDropHelper *H = FindDropHelper(this);
    return H != nullptr ? H->OnDragDrop : TDragDropEvent();
}

void TTreeViewAcceptingFiles::SetOnDragDrop(TDragDropEvent Value)
{
    GetDropHelper(this, true)->OnDragDrop = Value;
}

// { TListViewAcceptingFiles }

void TListViewAcceptingFiles::ActivateAcceptFiles()
{
    TFileDropHelper *H = GetDropHelper(this, false);
    setAcceptDrops(true);
    if (reportView() != nullptr) { reportView()->setAcceptDrops(true); H->AddTarget(reportView()->viewport()); }
    if (iconView() != nullptr) { iconView()->setAcceptDrops(true); H->AddTarget(iconView()->viewport()); }
}

TDragDropEvent TListViewAcceptingFiles::OnDragDrop() const
{
    TFileDropHelper *H = FindDropHelper(this);
    return H != nullptr ? H->OnDragDrop : TDragDropEvent();
}

void TListViewAcceptingFiles::SetOnDragDrop(TDragDropEvent Value)
{
    GetDropHelper(this, false)->OnDragDrop = Value;
}

// { TListViewSpecialHint }

void TListViewSpecialHint::ActivateSpecialHint()
{
    AttachSpecialHint(this);
}

// { TListViewAcceptingFilesAndSpecialHint }

void TListViewAcceptingFilesAndSpecialHint::ActivateSpecialHint()
{
    AttachSpecialHint(this);
}

// { TStringGridEx }

TListForCellEvent TStringGridEx::OnListForCell() const
{
    TStringGridExDelegate *D = FindGridDelegate(this);
    return D != nullptr ? D->FOnListForCell : TListForCellEvent();
}

void TStringGridEx::SetOnListForCell(TListForCellEvent Value)
{
    GetGridDelegate(this)->FOnListForCell = Value;
}

TGetListForCellEvent TStringGridEx::OnGetListForCell() const
{
    TStringGridExDelegate *D = FindGridDelegate(this);
    return D != nullptr ? D->FOnGetListForCell : TGetListForCellEvent();
}

void TStringGridEx::SetOnGetListForCell(TGetListForCellEvent Value)
{
    GetGridDelegate(this)->FOnGetListForCell = Value;
}

void TStringGridEx::ActivatePickListEditor()
{
    GetGridDelegate(this);
}

// { TListBoxAcceptingFiles }

void TListBoxAcceptingFiles::ActivateAcceptFiles()
{
    ActivateAcceptFilesForScrollArea(this, true);
}

TDragDropEvent TListBoxAcceptingFiles::OnDragDrop() const
{
    TFileDropHelper *H = FindDropHelper(this);
    return H != nullptr ? H->OnDragDrop : TDragDropEvent();
}

void TListBoxAcceptingFiles::SetOnDragDrop(TDragDropEvent Value)
{
    GetDropHelper(this, true)->OnDragDrop = Value;
}

// { global }

// ChangeControlClassType: the checks of the Delphi code, then the behaviour
// of the new class is attached to the existing control.
static void ChangeControlClassType(QWidget *Control, QWidget *NewControlClassInstance)
{
    const QMetaObject *NewMeta = NewControlClassInstance->metaObject();
    if (!NewMeta->inherits(Control->metaObject()))
        throw Exception(Format(QStringLiteral("Changing class type: %s is not a parent class of %s"),
                               {QString::fromLatin1(Control->metaObject()->className()), QString::fromLatin1(NewMeta->className())}));

    if ((dynamic_cast<TListViewSpecialHint *>(NewControlClassInstance) != nullptr) ||
        (dynamic_cast<TListViewAcceptingFilesAndSpecialHint *>(NewControlClassInstance) != nullptr)) {
        if (auto *LV = dynamic_cast<TListView *>(Control)) AttachSpecialHint(LV);
    }
    if (dynamic_cast<TStringGridEx *>(NewControlClassInstance) != nullptr) {
        if (auto *G = dynamic_cast<QTableWidget *>(Control)) GetGridDelegate(G);
    }
    // TTreeViewAcceptingFiles, TListViewAcceptingFiles, TListBoxAcceptingFiles:
    // nothing to do until ActivateAcceptFiles is called.
}

QWidget* NewWinControlType(QWidget* OldControl, TWinControlClass NewControlClass, TWinControlTypeChangeMethod ChangeMethod)
{
    if (OldControl == nullptr || !NewControlClass) return OldControl;

    // TODO(port): ctcmStoreAndRestore / ctcmCopyProperties created a new control
    // and copied the DFM properties to it.  A Qt widget created by uic is
    // referenced by the ui struct and by signal connections, so all three
    // methods attach the behaviour of the new class to the existing control
    // (the effect of ctcmDangerousMagic).
    Q_UNUSED(ChangeMethod);

    // The metaclass is a factory: create a temporary instance to learn the class.
    std::unique_ptr<QWidget> Temp(NewControlClass(nullptr));
    if (!Temp) return OldControl;
    ChangeControlClassType(OldControl, Temp.get());
    return OldControl;
}
