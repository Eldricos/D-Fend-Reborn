// VCL TListView emulation.
//
// TListView keeps the VCL object model (Items / TListItem / SubItems /
// Columns / ViewStyle / SmallImages / LargeImages / Selected ...) so that the
// Delphi code can be translated almost line by line.  Internally one model is
// shown either by a QTreeView (vsReport) or a QListView (vsIcon, vsSmallIcon,
// vsList); both share the same selection.
#pragma once

#include <QWidget>
#include <QAbstractTableModel>
#include <QFont>
#include <QColor>
#include <QStringList>
#include <functional>

#include "Graphics.h"

class QTreeView;
class QListView;
class QStackedLayout;
class QItemSelectionModel;
class QAbstractItemView;
class TListView;
class TListViewModel;

enum TViewStyle { vsIcon, vsSmallIcon, vsList, vsReport };
enum TSortType { stNone, stData, stText, stBoth };
enum TAlignment { taLeftJustify, taRightJustify, taCenter };

class TListItem;

// TListItem.SubItems
class TSubItems
{
public:
    explicit TSubItems(TListItem *Owner) : FOwner(Owner) {}
    int Count() const { return FList.size(); }
    int Add(const QString &S);
    void Insert(int Index, const QString &S);
    void Delete(int Index);
    void Clear();
    QString Strings(int I) const { return (I >= 0 && I < FList.size()) ? FList[I] : QString(); }
    QString operator[](int I) const { return Strings(I); }
    void SetStrings(int I, const QString &S);
    QString Text() const { return FList.join("\n"); }
    const QStringList &List() const { return FList; }
    void Assign(const QStringList &L);
    void BeginUpdate() {}
    void EndUpdate() {}
private:
    TListItem *FOwner;
    QStringList FList;
};

class TListItem
{
public:
    explicit TListItem(TListView *Owner);
    TListView *ListView() const { return FOwner; }

    QString Caption() const { return FCaption; }
    void SetCaption(const QString &S);
    TSubItems &SubItems() { return FSubItems; }
    const TSubItems &SubItems() const { return FSubItems; }
    int ImageIndex() const { return FImageIndex; }
    void SetImageIndex(int I);
    int StateIndex() const { return FStateIndex; }
    void SetStateIndex(int I) { FStateIndex = I; }
    void *Data() const { return FData; }
    void SetData(void *D) { FData = D; }
    template <typename T> T *DataAs() const { return static_cast<T *>(FData); }
    bool Checked() const { return FChecked; }
    void SetChecked(bool B);
    bool Selected() const;
    void SetSelected(bool B);
    bool Focused() const;
    void SetFocused(bool B);
    int Index() const;
    void MakeVisible(bool PartialOK = false);
    void Delete();
    void Update();

    // Replacements for OnCustomDrawItem: per item font and colors
    QFont Font() const { return FFont; }
    void SetFont(const QFont &F);
    bool HasFont() const { return FHasFont; }
    QColor TextColor() const { return FTextColor; }
    void SetTextColor(const QColor &C);
    QColor BackgroundColor() const { return FBackground; }
    void SetBackgroundColor(const QColor &C);

    void Changed();

private:
    TListView *FOwner;
    int FResetDepth = 0;
    QString FCaption;
    TSubItems FSubItems;
    int FImageIndex = -1;
    int FStateIndex = -1;
    void *FData = nullptr;
    bool FChecked = false;
    QFont FFont;
    bool FHasFont = false;
    QColor FTextColor, FBackground;
};

class TListItems
{
public:
    explicit TListItems(TListView *Owner) : FOwner(Owner) {}
    ~TListItems();
    int Count() const { return FItems.size(); }
    TListItem *Add();
    TListItem *AddItem(TListItem *Item = nullptr, int Index = -1);
    TListItem *Insert(int Index);
    TListItem *Item(int I) const { return (I >= 0 && I < FItems.size()) ? FItems[I] : nullptr; }
    TListItem *operator[](int I) const { return Item(I); }
    void Delete(int Index);
    void Clear();
    void BeginUpdate();
    void EndUpdate();
    int IndexOf(const TListItem *Item) const { return FItems.indexOf(const_cast<TListItem *>(Item)); }
    void Exchange(int A, int B);
    QList<TListItem *> &List() { return FItems; }
private:
    friend class TListView;
    friend class TListViewModel;
    TListView *FOwner;
    QList<TListItem *> FItems;
    int FUpdateCount = 0;
};

class TListColumn
{
public:
    explicit TListColumn(TListView *Owner) : FOwner(Owner) {}
    QString Caption() const { return FCaption; }
    void SetCaption(const QString &S);
    int Width() const;
    void SetWidth(int W);
    TAlignment Alignment() const { return FAlignment; }
    void SetAlignment(TAlignment A);
    bool AutoSize() const { return FAutoSize; }
    void SetAutoSize(bool B) { FAutoSize = B; }
    int Index() const;
    int Tag = 0;
private:
    friend class TListView;
    TListView *FOwner;
    QString FCaption;
    int FWidth = 50;
    TAlignment FAlignment = taLeftJustify;
    bool FAutoSize = false;
};

class TListColumns
{
public:
    explicit TListColumns(TListView *Owner) : FOwner(Owner) {}
    ~TListColumns();
    int Count() const { return FColumns.size(); }
    TListColumn *Add();
    TListColumn *Item(int I) const { return (I >= 0 && I < FColumns.size()) ? FColumns[I] : nullptr; }
    TListColumn *operator[](int I) const { return Item(I); }
    void Delete(int I);
    void Clear();
    void BeginUpdate();
    void EndUpdate();
private:
    friend class TListView;
    friend class TListColumn;
    TListView *FOwner;
    QList<TListColumn *> FColumns;
    int FUpdateCount = 0;
};

class TListView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString viewStyle READ viewStyleName WRITE setViewStyleName)
    Q_PROPERTY(QStringList columnCaptions READ columnCaptions WRITE setColumnCaptions)
    Q_PROPERTY(QString columnWidths READ columnWidths WRITE setColumnWidths)
    Q_PROPERTY(bool checkboxes READ Checkboxes WRITE SetCheckboxes)
    Q_PROPERTY(bool multiSelect READ MultiSelect WRITE SetMultiSelect)
    Q_PROPERTY(bool readOnly READ ReadOnly WRITE SetReadOnly)
    Q_PROPERTY(bool showColumnHeaders READ ShowColumnHeaders WRITE SetShowColumnHeaders)
    Q_PROPERTY(bool hideSelection READ HideSelection WRITE SetHideSelection)
    Q_PROPERTY(bool gridLines READ GridLines WRITE SetGridLines)
    Q_PROPERTY(bool rowSelect READ RowSelect WRITE SetRowSelect)
    Q_PROPERTY(QString sortType READ sortTypeName WRITE setSortTypeName)
public:
    explicit TListView(QWidget *parent = nullptr);
    ~TListView() override;

    TListItems *Items() { return FItems; }
    TListColumns *Columns() { return FColumns; }
    TListColumn *Column(int I) { return FColumns->Item(I); }

    TListItem *Selected() const;              // first selected item
    void SetSelected(TListItem *Item);        // nullptr clears selection
    QList<TListItem *> SelectedItems() const;
    int SelCount() const;
    TListItem *ItemFocused() const;
    void SetItemFocused(TListItem *Item);
    int ItemIndex() const;
    void SetItemIndex(int I);
    TListItem *GetItemAt(int X, int Y) const;
    TListItem *TopItem() const;
    void ClearSelection();
    void SelectAll();

    TViewStyle ViewStyle() const { return FViewStyle; }
    void SetViewStyle(TViewStyle V);
    TImageList *SmallImages() const { return FSmall; }
    void SetSmallImages(TImageList *L);
    TImageList *LargeImages() const { return FLarge; }
    void SetLargeImages(TImageList *L);
    TImageList *StateImages() const { return FState; }
    void SetStateImages(TImageList *L) { FState = L; refresh(); }

    bool Checkboxes() const { return FCheckboxes; }
    void SetCheckboxes(bool B);
    bool MultiSelect() const { return FMultiSelect; }
    void SetMultiSelect(bool B);
    bool ReadOnly() const { return FReadOnly; }
    void SetReadOnly(bool B);
    bool ShowColumnHeaders() const { return FShowHeaders; }
    void SetShowColumnHeaders(bool B);
    bool HideSelection() const { return FHideSelection; }
    void SetHideSelection(bool B);
    bool GridLines() const { return FGridLines; }
    void SetGridLines(bool B) { FGridLines = B; }
    bool RowSelect() const { return FRowSelect; }
    void SetRowSelect(bool B) { FRowSelect = B; }
    TSortType SortType() const { return FSortType; }
    void SetSortType(TSortType S);

    void SetColor(const QColor &C);           // Color
    QColor Color() const;
    void SetFontColor(const QColor &C);       // Font.Color

    // Sorting
    void AlphaSort();
    void CustomSort(const std::function<int(TListItem *, TListItem *)> &Compare);
    void Arrange();
    void Clear() { FItems->Clear(); }

    // OnInfoTip: return the tool tip for an item
    std::function<QString(TListItem *)> OnInfoTip;
    // OnCompare (used by AlphaSort if set)
    std::function<int(TListItem *, TListItem *)> OnCompare;
    // OnCustomDrawItem replacement: called before an item is painted,
    // may change font/colors of the item.
    std::function<void(TListItem *)> OnCustomDrawItem;

    QAbstractItemView *currentView() const;
    QTreeView *reportView() const { return FTree; }
    QListView *iconView() const { return FList; }

    // used by uic
    QString viewStyleName() const;
    void setViewStyleName(const QString &S);
    QStringList columnCaptions() const;
    void setColumnCaptions(const QStringList &L);
    QString columnWidths() const;
    void setColumnWidths(const QString &S);
    QString sortTypeName() const;
    void setSortTypeName(const QString &S);

    // internal
    void itemChanged(TListItem *Item);
    void itemsReset();
    void columnsChanged();
    void refresh();
    void applyColumnWidth(int I);
    int indexOfItem(const TListItem *Item) const { return FItems->IndexOf(Item); }

signals:
    void selectItem(TListItem *Item, bool Selected);   // OnSelectItem
    void change(TListItem *Item);                      // OnChange
    void itemChecked(TListItem *Item);                 // OnItemChecked
    void click();                                      // OnClick
    void dblClick();                                   // OnDblClick
    void columnClick(int Column);                      // OnColumnClick
    void keyDown(int Key, Qt::KeyboardModifiers Mods); // OnKeyDown
    void contextPopup(const QPoint &GlobalPos);        // right click
    void itemEdited(TListItem *Item, const QString &NewText);   // OnEdited

protected:
    bool eventFilter(QObject *O, QEvent *E) override;

private:
    friend class TListViewModel;
    friend class TListItems;
    friend class TListColumns;
    TListItems *FItems;
    TListColumns *FColumns;
    TListViewModel *FModel;
    QTreeView *FTree;
    QListView *FList;
    QStackedLayout *FStack;
    QItemSelectionModel *FSel;
    TViewStyle FViewStyle = vsIcon;
    TImageList *FSmall = nullptr, *FLarge = nullptr, *FState = nullptr;
    bool FCheckboxes = false, FMultiSelect = false, FReadOnly = false, FShowHeaders = true;
    bool FHideSelection = true, FGridLines = false, FRowSelect = false;
    TSortType FSortType = stNone;
    TListItem *FLastCurrent = nullptr;
    bool FApplyingWidths = false;
    void applyViewStyle();
    void applyColumnWidths();
    void onCurrentChanged();
    void onSelectionChanged(const class QItemSelection &Sel, const class QItemSelection &Desel);
};

class TListViewModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit TListViewModel(TListView *Owner) : QAbstractTableModel(Owner), FOwner(Owner) {}
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    // Model resets may nest (e.g. Items.BeginUpdate followed by a view style
    // change); only the outermost begin/end pair is passed to Qt.
    void reset() { if (FResetDepth == 0) { beginResetModel(); endResetModel(); } }
    void rowChanged(int Row);
    void headersChanged() { emit headerDataChanged(Qt::Horizontal, 0, qMax(0, columnCount() - 1)); }
    void beginReset() { if (FResetDepth++ == 0) beginResetModel(); }
    void endReset() { if (FResetDepth > 0 && --FResetDepth == 0) endResetModel(); }
    bool resetting() const { return FResetDepth > 0; }
    // Row notifications are not needed (and not allowed) inside a reset.
    void insertRowNotify(int Row) { if (!resetting()) beginInsertRows(QModelIndex(), Row, Row); }
    void insertRowDone() { if (!resetting()) endInsertRows(); }
    void removeRowNotify(int Row) { if (!resetting()) beginRemoveRows(QModelIndex(), Row, Row); }
    void removeRowDone() { if (!resetting()) endRemoveRows(); }
private:
    TListView *FOwner;
    int FResetDepth = 0;
};
