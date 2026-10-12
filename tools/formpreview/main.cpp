// formpreview <ui-file> <out.png> [width height]
#include <QApplication>
#include <QFile>
#include <QUiLoader>
#include <QWidget>
#include <QMainWindow>
#include <QDialog>
#include "VclLayout.h"
#include "VclWidgets.h"
#include "ComCtrls.h"

class Loader : public QUiLoader
{
public:
    QWidget *createWidget(const QString &className, QWidget *parent, const QString &name) override
    {
        QWidget *W = nullptr;
        if (className == "TRadioGroupWidget") W = new TRadioGroupWidget(parent);
        else if (className == "TSplitterWidget") W = new TSplitterWidget(parent);
        else if (className == "TColorBoxWidget") W = new TColorBoxWidget(parent);
        else if (className == "TValueListEditorWidget") W = new TValueListEditorWidget(parent);
        else if (className == "TImageWidget") W = new TImageWidget(parent);
        else if (className == "TListView") W = new TListView(parent);
        if (W) { W->setObjectName(name); return W; }
        return QUiLoader::createWidget(className, parent, name);
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QFont F("Tahoma");
    F.setPixelSize(11);
    app.setFont(F);
    Loader L;
    QFile Fi(argv[1]);
    Fi.open(QIODevice::ReadOnly);
    QWidget *W = L.load(&Fi);
    if (!W) { fprintf(stderr, "load failed: %s\n", qPrintable(L.errorString())); return 1; }
    VclLayout::setupForm(W);
    if (argc >= 5) W->resize(atoi(argv[3]), atoi(argv[4]));
    W->setAttribute(Qt::WA_DontShowOnScreen);
    W->show();
    for (int i = 0; i < 5; i++) app.processEvents();
    VclLayout::autoSizeLabels(W);
    app.processEvents();
    QPixmap P = W->grab();
    fprintf(stderr, "size %dx%d null=%d save=%d\n", W->width(), W->height(), P.isNull(), P.save(argv[2]));
    return 0;
}
