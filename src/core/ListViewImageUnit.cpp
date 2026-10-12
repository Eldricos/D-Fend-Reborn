// C++ port of ListViewImageUnit.pas (implementation section).
//
// The original set a tiled background image via LVM_SETBKIMAGE and subclassed
// the list view window to route WM_ERASEBKGND to the default handler.  The
// port uses a tiled texture brush as background (palette role Base) of the
// viewports of both views of the TListView.
#include "ListViewImageUnit.h"

#include <QPointer>

namespace {

class TDummyClass
{
public:
    explicit TDummyClass(TListView *AListView) : FListView(AListView) {}
    ~TDummyClass() {}
    TListView *ListView() const { return FListView; }
    void SetListView(TListView *Value) { FListView = Value; }
private:
    QPointer<TListView> FListView;
};

}

static QList<TDummyClass *> *DummyClassList = nullptr;

static void ListViewSetImage(TListView *ListView, const QString &FileName)
{
    if (ListView == nullptr) return;

    QPixmap Image;
    if (!FileName.isEmpty()) Image = QPixmap(FileName);

    QAbstractItemView *Views[2] = {ListView->reportView(), ListView->iconView()};
    for (QAbstractItemView *View : Views) {
        if (View == nullptr || View->viewport() == nullptr) continue;
        QWidget *VP = View->viewport();
        if (Image.isNull()) {
            // LVBKIF_SOURCE_NONE: back to the normal background of the view
            VP->setPalette(View->palette());
        } else {
            // LVBKIF_SOURCE_URL or LVBKIF_STYLE_TILE
            QPalette Pal = VP->palette();
            Pal.setBrush(QPalette::Base, QBrush(Image));
            VP->setPalette(Pal);
            VP->setAutoFillBackground(true);
        }
        VP->update();
    }
}

void SetListViewImage(TListView* ListView, const QString& FileName)
{
    if (DummyClassList == nullptr) DummyClassList = new QList<TDummyClass *>();

    int Nr = -1;
    for (int I = 0; I < DummyClassList->size(); I++) if ((*DummyClassList)[I]->ListView() == ListView) { Nr = I; break; }

    if (FileName.isEmpty()) {
        if (Nr >= 0) { delete (*DummyClassList)[Nr]; DummyClassList->removeAt(Nr); }
        ListViewSetImage(ListView, QString());
    } else {
        if (Nr < 0) DummyClassList->append(new TDummyClass(ListView));
        ListViewSetImage(ListView, FileName);
    }
}

static void FreeDummyClassListWithoutUninit()
{
    if (DummyClassList == nullptr) return;
    for (int I = 0; I < DummyClassList->size(); I++) {
        (*DummyClassList)[I]->SetListView(nullptr);
        delete (*DummyClassList)[I];
    }
    delete DummyClassList;
    DummyClassList = nullptr;
}

void ListViewImageUnit_Initialization()
{
    DummyClassList = new QList<TDummyClass *>();
}

void ListViewImageUnit_Finalization()
{
    FreeDummyClassListWithoutUninit();
}
