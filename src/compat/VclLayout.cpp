#include "VclLayout.h"
#include "VclForm.h"
#include "VclWidgets.h"

#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDialog>
#include <QDirIterator>
#include <QEvent>
#include <QFrame>
#include <QGroupBox>
#include <QHash>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QMainWindow>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QWindow>
#include <algorithm>

namespace VclLayout {

// ---------------------------------------------------------------------------
// Align / Anchors engine
// ---------------------------------------------------------------------------

enum Align { alNone, alTop, alBottom, alLeft, alRight, alClient, alCustom };
enum Anchor { akLeft = 1, akTop = 2, akRight = 4, akBottom = 8 };

static Align parseAlign(const QString &S)
{
    if (S == "alTop") return alTop;
    if (S == "alBottom") return alBottom;
    if (S == "alLeft") return alLeft;
    if (S == "alRight") return alRight;
    if (S == "alClient") return alClient;
    if (S == "alCustom") return alCustom;
    return alNone;
}

static int parseAnchors(const QVariant &V)
{
    if (!V.isValid()) return akLeft | akTop;
    int A = 0;
    for (const QString &S : V.toString().split(',', Qt::SkipEmptyParts)) {
        QString T = S.trimmed();
        if (T == "akLeft") A |= akLeft;
        else if (T == "akTop") A |= akTop;
        else if (T == "akRight") A |= akRight;
        else if (T == "akBottom") A |= akBottom;
    }
    return A;
}

struct ChildRule {
    QPointer<QWidget> W;
    Align A = alNone;
    int Anchors = akLeft | akTop;
    QRect Design;        // bounds relative to DesignParent
    QSize DesignParent;  // parent client size when Design was recorded
    QRect LastSet;       // what the engine assigned the last time
    int Extent = 0;      // width (alLeft/Right) or height (alTop/Bottom)
};

class AlignEngine : public QObject
{
public:
    explicit AlignEngine(QWidget *Container) : QObject(Container), FC(Container)
    {
        setObjectName("vclAlignEngine");
        QVariant DW = Container->property("vclDesignWidth"), DH = Container->property("vclDesignHeight");
        FDesign = (DW.isValid() && DH.isValid()) ? QSize(DW.toInt(), DH.toInt()) : Container->size();
        Container->installEventFilter(this);
    }

    void addChild(QWidget *W)
    {
        for (const ChildRule &R : FRules) if (R.W == W) return;
        ChildRule R;
        R.W = W;
        R.A = parseAlign(W->property("vclAlign").toString());
        R.Anchors = parseAnchors(W->property("vclAnchors"));
        R.Design = W->geometry();
        R.DesignParent = FDesign;
        R.LastSet = W->geometry();
        R.Extent = (R.A == alLeft || R.A == alRight) ? W->width() : W->height();
        FRules.append(R);
        W->installEventFilter(this);
    }

    ChildRule *rule(QWidget *W)
    {
        for (ChildRule &R : FRules) if (R.W == W) return &R;
        return nullptr;
    }

    QSize currentClient() const
    {
        // Use the design size until the container has been laid out once by Qt
        // (pages of a QTabWidget are not resized before they are shown).
        return FC->size();
    }

    void layout()
    {
        if (FInLayout) return;
        FInLayout = true;
        QSize CS = currentClient();
        // ---- anchors (non aligned controls)
        for (ChildRule &R : FRules) {
            if (!R.W || R.A != alNone) continue;
            int L = R.Design.x(), T = R.Design.y(), W = R.Design.width(), H = R.Design.height();
            int DX = CS.width() - R.DesignParent.width();
            int DY = CS.height() - R.DesignParent.height();
            bool AL = R.Anchors & akLeft, AR = R.Anchors & akRight, AT = R.Anchors & akTop, AB = R.Anchors & akBottom;
            if (AL && AR) W = qMax(0, W + DX);
            else if (AR) L += DX;
            else if (!AL) L += DX / 2;
            if (AT && AB) H = qMax(0, H + DY);
            else if (AB) T += DY;
            else if (!AT) T += DY / 2;
            setGeom(R, QRect(L, T, W, H));
        }
        // ---- align (TWinControl.AlignControls order)
        QRect Client = FC->rect();
        if (auto *F = qobject_cast<QFrame *>(FC)) if (!qobject_cast<QAbstractScrollArea *>(FC)) Client = F->contentsRect();
        if (auto *G = qobject_cast<QGroupBox *>(FC)) {
            int Top = G->fontMetrics().height() + 2;
            Client = QRect(2, Top, qMax(0, G->width() - 4), qMax(0, G->height() - Top - 2));
        }
        auto collect = [&](Align A) {
            QList<ChildRule *> L;
            for (ChildRule &R : FRules) if (R.W && R.A == A && !R.W->isHidden()) L << &R;
            return L;
        };
        QList<ChildRule *> Tops = collect(alTop), Bottoms = collect(alBottom), Lefts = collect(alLeft),
                           Rights = collect(alRight), Clients = collect(alClient);
        std::stable_sort(Tops.begin(), Tops.end(), [](ChildRule *A, ChildRule *B) { return A->LastSet.y() < B->LastSet.y(); });
        std::stable_sort(Bottoms.begin(), Bottoms.end(), [](ChildRule *A, ChildRule *B) { return A->LastSet.y() > B->LastSet.y(); });
        std::stable_sort(Lefts.begin(), Lefts.end(), [](ChildRule *A, ChildRule *B) { return A->LastSet.x() < B->LastSet.x(); });
        std::stable_sort(Rights.begin(), Rights.end(), [](ChildRule *A, ChildRule *B) { return A->LastSet.x() > B->LastSet.x(); });
        for (ChildRule *R : Tops) {
            setGeom(*R, QRect(Client.left(), Client.top(), Client.width(), R->Extent));
            Client.setTop(Client.top() + R->Extent);
        }
        for (ChildRule *R : Bottoms) {
            setGeom(*R, QRect(Client.left(), Client.bottom() + 1 - R->Extent, Client.width(), R->Extent));
            Client.setBottom(Client.bottom() - R->Extent);
        }
        for (ChildRule *R : Lefts) {
            setGeom(*R, QRect(Client.left(), Client.top(), R->Extent, qMax(0, Client.height())));
            Client.setLeft(Client.left() + R->Extent);
        }
        for (ChildRule *R : Rights) {
            setGeom(*R, QRect(Client.right() + 1 - R->Extent, Client.top(), R->Extent, qMax(0, Client.height())));
            Client.setRight(Client.right() - R->Extent);
        }
        for (ChildRule *R : Clients) setGeom(*R, QRect(Client.left(), Client.top(), qMax(0, Client.width()), qMax(0, Client.height())));
        FInLayout = false;
    }

    void setGeom(ChildRule &R, const QRect &G)
    {
        R.LastSet = G;
        if (R.W->geometry() != G) R.W->setGeometry(G);
    }

    // External change of a child's geometry (code assigned Left/Width/...)
    void externalChange(ChildRule &R)
    {
        QRect G = R.W->geometry();
        R.LastSet = G;
        if (R.A == alLeft || R.A == alRight) R.Extent = G.width();
        else if (R.A == alTop || R.A == alBottom) R.Extent = G.height();
        else if (R.A == alNone) {
            R.Design = G;
            R.DesignParent = currentClient();
        }
        scheduleLayout();
    }

    void scheduleLayout()
    {
        if (FPending) return;
        FPending = true;
        QTimer::singleShot(0, this, [this]() { FPending = false; layout(); });
    }

    bool eventFilter(QObject *O, QEvent *E) override
    {
        if (O == FC) {
            if (E->type() == QEvent::Resize || E->type() == QEvent::LayoutRequest) layout();
            return false;
        }
        auto *W = qobject_cast<QWidget *>(O);
        if (!W) return false;
        switch (E->type()) {
        case QEvent::ShowToParent:
        case QEvent::HideToParent: {
            ChildRule *R = rule(W);
            if (R && R->A != alNone) layout();
            break;
        }
        case QEvent::Move:
        case QEvent::Resize: {
            if (FInLayout) break;
            ChildRule *R = rule(W);
            if (R && W->geometry() != R->LastSet) externalChange(*R);
            break;
        }
        default:
            break;
        }
        return false;
    }

    QWidget *FC;
    QSize FDesign;
    QList<ChildRule> FRules;
    bool FInLayout = false;
    bool FPending = false;
};

static AlignEngine *engineOf(QWidget *Container, bool Create)
{
    if (!Container) return nullptr;
    for (QObject *O : Container->children())
        if (auto *E = dynamic_cast<AlignEngine *>(O)) return E;
    if (!Create) return nullptr;
    return new AlignEngine(Container);
}

static bool needsEngine(QWidget *W)
{
    QVariant A = W->property("vclAlign");
    if (A.isValid() && parseAlign(A.toString()) != alNone) return true;
    QVariant K = W->property("vclAnchors");
    if (K.isValid() && parseAnchors(K) != (akLeft | akTop)) return true;
    return false;
}

static void installEngines(QWidget *Root)
{
    const QList<QWidget *> All = Root->findChildren<QWidget *>();
    for (QWidget *W : All) {
        if (!needsEngine(W)) continue;
        QWidget *P = W->parentWidget();
        if (!P) continue;
        if (P->layout()) continue;   // positioned by a Qt layout (converted .ui)
        if (qobject_cast<QToolBar *>(P) || qobject_cast<QStatusBar *>(P)) continue;
        if (qobject_cast<QStackedWidget *>(P) || qobject_cast<QTabWidget *>(P)) continue;
        if (P->objectName() == "qt_tabwidget_stackedwidget") continue;
        AlignEngine *E = engineOf(P, true);
        E->addChild(W);
    }
    // Initial layout of every engine (outer containers first)
    QList<AlignEngine *> Engines;
    for (QObject *O : Root->findChildren<QObject *>())
        if (auto *E = dynamic_cast<AlignEngine *>(O)) Engines << E;
    for (AlignEngine *E : Engines) E->layout();
}

void relayout(QWidget *Container)
{
    if (AlignEngine *E = engineOf(Container, false)) E->layout();
}

void setAlignedExtent(QWidget *Control, int Extent)
{
    AlignEngine *E = engineOf(Control->parentWidget(), false);
    ChildRule *R = E ? E->rule(Control) : nullptr;
    if (!R) {
        Control->resize(Control->width(), Extent);
        return;
    }
    R->Extent = Extent;
    E->layout();
}

void setBounds(QWidget *Control, int Left, int Top, int Width, int Height)
{
    Control->setGeometry(Left, Top, Width, Height);
    AlignEngine *E = engineOf(Control->parentWidget(), false);
    if (ChildRule *R = E ? E->rule(Control) : nullptr) E->externalChange(*R);
}

// ---------------------------------------------------------------------------
// Scroll boxes: contents follow the viewport like a VCL TScrollBox client
// ---------------------------------------------------------------------------

class ScrollContentsFollower : public QObject
{
public:
    ScrollContentsFollower(QScrollArea *A, QWidget *C) : QObject(A), FA(A), FC(C)
    {
        FDesign = QSize(C->property("vclDesignWidth").toInt(), C->property("vclDesignHeight").toInt());
        if (FDesign.isEmpty()) FDesign = C->size();
        A->viewport()->installEventFilter(this);
    }
    bool eventFilter(QObject *, QEvent *E) override
    {
        if (E->type() == QEvent::Resize) {
            QSize V = FA->viewport()->size();
            // ("vclContentsHeight": height the form code needs for its controls,
            // e.g. the expandable sections of the multiple profiles editor)
            const int H = qMax(qMax(V.height(), FDesign.height()), FC->property("vclContentsHeight").toInt());
            FC->resize(qMax(V.width(), FDesign.width()), H);
        }
        return false;
    }
    QScrollArea *FA;
    QWidget *FC;
    QSize FDesign;
};

// ---------------------------------------------------------------------------
// Label AutoSize
// ---------------------------------------------------------------------------

void autoSizeLabels(QWidget *Root)
{
    const QList<QLabel *> Labels = Root->findChildren<QLabel *>();
    for (QLabel *L : Labels) {
        if (!L->property("vclAutoSize").toBool()) continue;
        if (L->wordWrap()) continue;
        if (L->property("vclAlign").isValid()) continue;
        if (L->parentWidget() && L->parentWidget()->layout()) continue;  // sized by the layout
        QSize H = L->sizeHint();
        int NewW = H.width();
        if (NewW == L->width()) continue;
        QRect G = L->geometry();
        if (L->alignment() & Qt::AlignRight) G.setLeft(G.right() + 1 - NewW);
        else if (L->alignment() & Qt::AlignHCenter) { int C = G.center().x(); G.setLeft(C - NewW / 2); G.setWidth(NewW); }
        else G.setWidth(NewW);
        if (L->height() < H.height()) G.setHeight(H.height());
        L->setGeometry(G);
        if (AlignEngine *E = engineOf(L->parentWidget(), false))
            if (ChildRule *R = E->rule(L)) { R->Design = G; R->DesignParent = L->parentWidget()->size(); R->LastSet = G; }
    }
}

class AutoSizeOnShow : public QObject
{
public:
    explicit AutoSizeOnShow(QWidget *Top) : QObject(Top), FTop(Top) { Top->installEventFilter(this); }
    bool eventFilter(QObject *O, QEvent *E) override
    {
        if (O == FTop && (E->type() == QEvent::Show || E->type() == QEvent::LanguageChange)) {
            QPointer<QWidget> T = FTop;
            QTimer::singleShot(0, FTop, [T]() { if (T) autoSizeLabels(T); });
        }
        return false;
    }
    QWidget *FTop;
};

// ---------------------------------------------------------------------------
// Form level window settings
// ---------------------------------------------------------------------------

void setClientSize(QWidget *Form, int Width, int Height)
{
    QString BS = Form->property("vclBorderStyle").toString();
    if (BS == "bsDialog" || BS == "bsSingle" || BS == "bsToolWindow") Form->setFixedSize(Width, Height);
    else Form->resize(Width, Height);
}

class CenterOnShow : public QObject
{
public:
    CenterOnShow(QWidget *Top, bool Screen) : QObject(Top), FTop(Top), FScreen(Screen) { Top->installEventFilter(this); }
    bool eventFilter(QObject *O, QEvent *E) override
    {
        if (O == FTop && E->type() == QEvent::Show && !FDone) {
            FDone = true;
            QRect Area;
            QWidget *P = FTop->parentWidget() ? FTop->parentWidget()->window() : nullptr;
            if (!FScreen && P && P->isVisible()) Area = P->frameGeometry();
            else if (QScreen *S = FTop->screen()) Area = S->availableGeometry();
            if (Area.isValid()) {
                QRect F = FTop->frameGeometry();
                F.moveCenter(Area.center());
                FTop->move(F.topLeft());
            }
        }
        return false;
    }
    QWidget *FTop;
    bool FScreen;
    bool FDone = false;
};

// Fixed size dialog with a Qt layout (converted .ui): design size, or larger
// if the controls need more space (scaling, fonts, translations).
class FitOnShow : public QObject
{
public:
    explicit FitOnShow(QWidget *Top) : QObject(Top), FTop(Top) { Top->installEventFilter(this); }
    bool eventFilter(QObject *O, QEvent *E) override
    {
        if (O == FTop && E->type() == QEvent::Polish && !FDone) {   // (before CenterOnShow)
            FDone = true;
            fit(false);
        }
        if (O == FTop && E->type() == QEvent::Show) {
            // some forms set their texts in showEvent (after this filter)
            QPointer<QWidget> T = FTop;
            QTimer::singleShot(0, this, [this, T]() { if (T) fit(true); });
        }
        return false;
    }
    void fit(bool KeepCenter)
    {
        FTop->layout()->activate();
        const QSize Old = FTop->size(), New = Old.expandedTo(FTop->minimumSizeHint());
        if (New == Old && FTop->minimumSize() == Old) return;
        FTop->setFixedSize(New);
        if (KeepCenter && New != Old) FTop->move(FTop->x() - (New.width() - Old.width()) / 2, FTop->y() - (New.height() - Old.height()) / 2);
    }
    QWidget *FTop;
    bool FDone = false;
};

static void applyWindowSettings(QWidget *Top)
{
    if (!Top->isWindow() && !qobject_cast<QDialog *>(Top) && !qobject_cast<QMainWindow *>(Top)) return;
    if (!Top->property("vclDesignWidth").isValid()) return;
    QString BS = Top->property("vclBorderStyle").toString();
    QString Icons = Top->property("vclBorderIcons").toString();
    bool HasIcons = Top->property("vclBorderIcons").isValid();
    QSize Design(Top->property("vclDesignWidth").toInt(), Top->property("vclDesignHeight").toInt());

    if (qobject_cast<QDialog *>(Top)) {
        Qt::WindowFlags F = Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint;
        bool SysMenu = !HasIcons || Icons.contains("biSystemMenu");
        if (SysMenu) F |= Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint;
        if (BS == "bsNone") F = Qt::Dialog | Qt::FramelessWindowHint;
        else if (BS == "bsSizeToolWin" || BS == "bsToolWindow") F = Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint;
        else if (BS != "bsDialog") {
            if (!HasIcons || Icons.contains("biMinimize")) F |= Qt::WindowMinimizeButtonHint;
            if (!HasIcons || Icons.contains("biMaximize")) F |= Qt::WindowMaximizeButtonHint;
        }
        Top->setWindowFlags(F);
    }
    if (BS == "bsDialog" || BS == "bsSingle" || BS == "bsToolWindow") {
        if (Top->layout()) new FitOnShow(Top);   // texts are set after setupForm
        else Top->setFixedSize(Top->size());
    } else {
        int MW = Top->property("vclMinWidth").toInt(), MH = Top->property("vclMinHeight").toInt();
        // Constraints are outer sizes in VCL; subtract a typical frame
        if (MW > 0 || MH > 0) Top->setMinimumSize(qMax(0, MW - 16), qMax(0, MH - 38));
    }
    QString Pos = Top->property("vclPosition").toString();
    if (Pos == "poScreenCenter" || Pos == "poDesktopCenter") new CenterOnShow(Top, true);
    else if (Pos == "poOwnerFormCenter" || Pos == "poMainFormCenter") new CenterOnShow(Top, false);
    if (Top->property("vclWindowState").toString() == "wsMaximized") Top->setWindowState(Qt::WindowMaximized);
    Q_UNUSED(Design);
}

// ---------------------------------------------------------------------------
// Menus, actions, tool bars
// ---------------------------------------------------------------------------

static QMenu *findMenu(QWidget *Top, const QString &Name)
{
    if (Name.isEmpty()) return nullptr;
    return Top->findChild<QMenu *>(Name);
}

static void setupActions(QWidget *Top)
{
    const QList<QAction *> Actions = Top->findChildren<QAction *>(QString(), Qt::FindDirectChildrenOnly);
    QHash<QString, QActionGroup *> Groups;
    for (QAction *A : Actions) {
        if (A->property("vclSeparator").toBool()) A->setSeparator(true);
        if (A->property("vclDefault").toBool()) {
            QFont F = A->font();
            F.setBold(true);
            A->setFont(F);
        }
        if (A->property("vclRadioItem").toBool()) {
            QObject *Owner = nullptr;
            const QList<QObject *> Objs = A->associatedObjects();
            for (QObject *O : Objs) if (qobject_cast<QMenu *>(O)) { Owner = O; break; }
            QString Key = QString("%1/%2").arg(reinterpret_cast<quintptr>(Owner)).arg(A->property("vclGroupIndex").toInt());
            QActionGroup *G = Groups.value(Key);
            if (!G) { G = new QActionGroup(Top); G->setExclusive(true); Groups.insert(Key, G); }
            G->addAction(A);
        }
        if (A->shortcut().isEmpty() == false) A->setShortcutContext(Qt::WindowShortcut);
    }
    const QList<QMenu *> Menus = Top->findChildren<QMenu *>();
    for (QMenu *M : Menus) {
        if (M->property("vclMenuVisible").isValid() && !M->property("vclMenuVisible").toBool()) M->menuAction()->setVisible(false);
        if (M->property("vclMenuEnabled").isValid() && !M->property("vclMenuEnabled").toBool()) M->menuAction()->setEnabled(false);
        M->menuAction()->setObjectName(M->objectName() + "_Action");
        // VCL menus show the shortcut keys and hide disabled icons the same way
        M->setToolTipsVisible(true);
    }
}

static void setupToolBars(QWidget *Top)
{
    const QList<QToolBar *> Bars = Top->findChildren<QToolBar *>();
    for (QToolBar *TB : Bars) {
        // Separators: setupActions marks them via setSeparator() only after
        // uic added them, and QToolBar keeps the (empty) tool button it
        // created for them - re-add them so they become real separators.
        {
            const QList<QAction *> Acts = TB->actions();
            for (int I = 0; I < Acts.size(); I++) {
                QAction *A = Acts[I];
                if (!A->isSeparator() || qobject_cast<QToolButton *>(TB->widgetForAction(A)) == nullptr) continue;
                TB->removeAction(A);
                if (I + 1 < Acts.size()) TB->insertAction(Acts[I + 1], A); else TB->addAction(A);
            }
        }
        // embedded controls (TEdit inside TToolBar ...)
        QList<QWidget *> Embedded;
        for (QObject *O : TB->children()) {
            auto *W = qobject_cast<QWidget *>(O);
            if (W && W->property("vclToolbarPos").isValid()) Embedded << W;
        }
        std::sort(Embedded.begin(), Embedded.end(), [](QWidget *A, QWidget *B) {
            return A->property("vclToolbarPos").toInt() < B->property("vclToolbarPos").toInt();
        });
        for (QWidget *W : Embedded) {
            int Pos = W->property("vclToolbarPos").toInt();
            // count only the form's own tool buttons (Qt may have added
            // unnamed internal actions)
            QList<QAction *> Acts;
            for (QAction *A : TB->actions()) if (!A->objectName().isEmpty()) Acts << A;
            QSize S = W->size();
            QAction *WA;
            if (Pos < Acts.size()) WA = TB->insertWidget(Acts[Pos], W);
            else WA = TB->addWidget(W);
            WA->setObjectName(W->objectName() + "_ToolbarAction");
            W->setFixedWidth(S.width());
            W->setVisible(!W->property("visible").isValid() || W->isVisibleTo(TB) || true);
        }
        int BW = TB->property("vclButtonWidth").toInt();
        int BH = TB->property("vclButtonHeight").toInt();
        for (QAction *A : TB->actions()) {
            QWidget *W = TB->widgetForAction(A);
            auto *Btn = qobject_cast<QToolButton *>(W);
            if (!Btn) continue;
            if (BH > 0 && TB->toolButtonStyle() == Qt::ToolButtonIconOnly) Btn->setMinimumSize(BW, BH);
            else if (BH > 0) Btn->setMinimumHeight(BH);
            QString MenuName = A->property("vclDropdownMenu").toString();
            if (QMenu *M = findMenu(Top, MenuName)) {
                A->setMenu(M);
                Btn->setPopupMode(A->property("vclDropDown").toBool() ? QToolButton::MenuButtonPopup : QToolButton::InstantPopup);
            }
            Btn->setObjectName(A->objectName() + "_Button");
        }
    }
}

static void setupPopupMenus(QWidget *Top)
{
    const QList<QWidget *> All = Top->findChildren<QWidget *>();
    QList<QWidget *> Targets = All;
    Targets << Top;
    for (QWidget *W : Targets) {
        QString Name = W->property("vclPopupMenu").toString();
        if (Name.isEmpty() || qobject_cast<QMenu *>(W)) continue;
        QMenu *M = findMenu(Top, Name);
        if (!M) continue;
        W->setContextMenuPolicy(Qt::CustomContextMenu);
        auto *SA = qobject_cast<QAbstractScrollArea *>(W);
        QObject::connect(W, &QWidget::customContextMenuRequested, M, [W, SA, M](const QPoint &P) {
            QPoint G = SA ? SA->viewport()->mapToGlobal(P) : W->mapToGlobal(P);
            M->popup(G);
        });
    }
}

QMenu *popupMenuOf(QWidget *W)
{
    QString Name = W->property("vclPopupMenu").toString();
    if (Name.isEmpty()) return nullptr;
    return W->window()->findChild<QMenu *>(Name);
}

// ---------------------------------------------------------------------------
// Buttons
// ---------------------------------------------------------------------------

QIcon bitBtnKindIcon(const QString &Kind)
{
    QString Path = QString(":/vcl/%1.png").arg(Kind);
    if (QFile::exists(Path)) return QIcon(Path);
    return QIcon();
}

static int modalResultFromName(const QString &S)
{
    static const QHash<QString, int> M = {
        {"mrOk", mrOk}, {"mrCancel", mrCancel}, {"mrAbort", mrAbort}, {"mrRetry", mrRetry},
        {"mrIgnore", mrIgnore}, {"mrYes", mrYes}, {"mrNo", mrNo}, {"mrAll", mrAll},
        {"mrNoToAll", mrNoToAll}, {"mrYesToAll", mrYesToAll}, {"mrClose", mrClose},
    };
    bool Ok;
    int N = S.toInt(&Ok);
    if (Ok) return N;
    return M.value(S, mrNone);
}

static void setupButtons(QWidget *Top)
{
    auto *Form = qobject_cast<TVclForm *>(Top);
    const QList<QPushButton *> Buttons = Top->findChildren<QPushButton *>();
    for (QPushButton *B : Buttons) {
        QString Kind = B->property("vclKind").toString();
        if (!Kind.isEmpty() && B->icon().isNull()) B->setIcon(bitBtnKindIcon(Kind));
        if (B->property("vclWordWrap").toBool()) B->setStyleSheet("text-align: center;");
        int MR = modalResultFromName(B->property("vclModalResult").toString());
        if (MR != mrNone && Form) {
            // VCL sets Form.ModalResult before OnClick runs, so the OnClick
            // handler can reset it to mrNone to keep the form open.
            QObject::connect(B, &QPushButton::clicked, Form, [Form, MR]() { Form->setModalResult(MR); });
        }
        if (MR == mrNone && Kind == "bkClose" && Form)
            QObject::connect(B, &QPushButton::clicked, Form, [Form]() { Form->close(); });
    }
    // SpeedButton GroupIndex -> exclusive groups
    QHash<QString, QButtonGroup *> Groups;
    const QList<QToolButton *> Tools = Top->findChildren<QToolButton *>();
    for (QToolButton *T : Tools) {
        int GI = T->property("vclGroupIndex").toInt();
        if (GI <= 0) continue;
        QString Key = QString("%1/%2").arg(reinterpret_cast<quintptr>(T->parentWidget())).arg(GI);
        QButtonGroup *G = Groups.value(Key);
        if (!G) { G = new QButtonGroup(T->parentWidget()); G->setExclusive(true); Groups.insert(Key, G); }
        G->addButton(T);
    }
    const QList<QCheckBox *> Checks = Top->findChildren<QCheckBox *>();
    for (QCheckBox *C : Checks)
        if (C->property("vclGrayed").toBool()) C->setCheckState(Qt::PartiallyChecked);
}

// ---------------------------------------------------------------------------
// Misc controls
// ---------------------------------------------------------------------------

static QList<int> parseInts(const QVariant &V)
{
    QList<int> R;
    for (const QString &S : V.toString().split(',', Qt::SkipEmptyParts)) R << S.trimmed().toInt();
    return R;
}

static void setupMisc(QWidget *Top)
{
    // Label FocusControl -> buddy
    const QList<QLabel *> Labels = Top->findChildren<QLabel *>();
    for (QLabel *L : Labels) {
        QString FC = L->property("vclFocusControl").toString();
        if (FC.isEmpty()) continue;
        if (QWidget *B = Top->findChild<QWidget *>(FC)) L->setBuddy(B);
    }
    // Tab sheets with TabVisible=False
    const QList<QTabWidget *> Tabs = Top->findChildren<QTabWidget *>();
    for (QTabWidget *T : Tabs) {
        for (int i = 0; i < T->count(); i++) {
            QWidget *P = T->widget(i);
            QVariant V = P->property("vclTabVisible");
            if (V.isValid() && !V.toBool()) T->setTabVisible(i, false);
        }
    }
    // Column widths of report list views and string grids
    const QList<QTreeWidget *> Trees = Top->findChildren<QTreeWidget *>();
    for (QTreeWidget *T : Trees) {
        QList<int> W = parseInts(T->property("vclColumnWidths"));
        for (int i = 0; i < W.size() && i < T->columnCount(); i++) T->header()->resizeSection(i, W[i] < 0 ? 100 : W[i]);
        if (T->property("vclViewStyle").toString() == "vsReport") T->header()->setStretchLastSection(false);
        if (T->property("vclHideSelection").isValid()) {
            QPalette P = T->palette();
            P.setColor(QPalette::Inactive, QPalette::Highlight, P.color(QPalette::Active, QPalette::Highlight));
            P.setColor(QPalette::Inactive, QPalette::HighlightedText, P.color(QPalette::Active, QPalette::HighlightedText));
            T->setPalette(P);
        }
    }
    const QList<QTableWidget *> Grids = Top->findChildren<QTableWidget *>();
    for (QTableWidget *G : Grids) {
        QList<int> W = parseInts(G->property("vclColumnWidths"));
        for (int i = 0; i < W.size() && i < G->columnCount(); i++) G->setColumnWidth(i, W[i]);
        if (auto *V = qobject_cast<TValueListEditorWidget *>(G)) {
            if (!W.isEmpty()) V->horizontalHeader()->setStretchLastSection(true);
        }
    }
    // Status bars
    const QList<QStatusBar *> Bars = Top->findChildren<QStatusBar *>();
    for (QStatusBar *S : Bars) {
        QList<int> W = parseInts(S->property("vclPanelWidths"));
        for (int i = 0; i < W.size(); i++) {
            auto *L = new QLabel(S);
            L->setObjectName(QString("%1_Panel%2").arg(S->objectName()).arg(i));
            L->setMinimumWidth(W[i]);
            L->setFrameShape(QFrame::Panel);
            L->setFrameShadow(QFrame::Sunken);
            S->addWidget(L, i == W.size() - 1 ? 1 : 0);
        }
        if (W.isEmpty()) {
            auto *L = new QLabel(S);
            L->setObjectName(S->objectName() + "_Simple");
            S->addWidget(L, 1);
        }
    }
    // Scroll boxes
    const QList<QScrollArea *> Areas = Top->findChildren<QScrollArea *>();
    for (QScrollArea *A : Areas)
        if (QWidget *C = A->widget()) if (C->property("vclScrollContents").toBool()) new ScrollContentsFollower(A, C);
    // Panels used as caption holders: let mouse clicks pass through captions
    for (QLabel *L : Labels)
        if (L->property("vclPanelCaptionOf").isValid()) L->setAttribute(Qt::WA_TransparentForMouseEvents);
}

// ---------------------------------------------------------------------------
// Public helpers
// ---------------------------------------------------------------------------

void setStatusText(QStatusBar *Bar, int Panel, const QString &Text)
{
    if (auto *L = Bar->findChild<QLabel *>(QString("%1_Panel%2").arg(Bar->objectName()).arg(Panel))) L->setText(Text);
    else if (Panel == 0) setSimpleText(Bar, Text);
}

QString statusText(QStatusBar *Bar, int Panel)
{
    if (auto *L = Bar->findChild<QLabel *>(QString("%1_Panel%2").arg(Bar->objectName()).arg(Panel))) return L->text();
    return QString();
}

void setSimpleText(QStatusBar *Bar, const QString &Text)
{
    if (auto *L = Bar->findChild<QLabel *>(Bar->objectName() + "_Simple")) L->setText(Text);
    else Bar->showMessage(Text);
}

void setStatusPanelWidth(QStatusBar *Bar, int Panel, int Width)
{
    if (auto *L = Bar->findChild<QLabel *>(QString("%1_Panel%2").arg(Bar->objectName()).arg(Panel))) L->setMinimumWidth(Width);
}

QList<QIcon> imageList(const QString &Unit, const QString &Name)
{
    static QHash<QString, QList<QIcon>> Cache;
    QString Key = Unit + "/" + Name;
    auto It = Cache.find(Key);
    if (It != Cache.end()) return *It;
    QList<QIcon> R;
    for (int i = 0;; i++) {
        QString P = QString(":/imagelists/%1/%2/%3.png").arg(Unit, Name).arg(i);
        if (!QFile::exists(P)) break;
        R << QIcon(P);
    }
    Cache.insert(Key, R);
    return R;
}

QIcon imageListIcon(const QString &Unit, const QString &Name, int Index)
{
    QList<QIcon> L = imageList(Unit, Name);
    return (Index >= 0 && Index < L.size()) ? L[Index] : QIcon();
}

void setTabVisible(QTabWidget *Tabs, QWidget *Page, bool Visible)
{
    int I = Tabs->indexOf(Page);
    if (I >= 0) Tabs->setTabVisible(I, Visible);
}

void setActivePage(QTabWidget *Tabs, QWidget *Page)
{
    int I = Tabs->indexOf(Page);
    if (I >= 0) Tabs->setCurrentIndex(I);
}

class KeyPreviewFilter : public QObject
{
public:
    KeyPreviewFilter(QWidget *Form, std::function<bool(int, Qt::KeyboardModifiers)> H) : QObject(Form), FForm(Form), FH(std::move(H))
    {
        qApp->installEventFilter(this);
    }
    ~KeyPreviewFilter() override { qApp->removeEventFilter(this); }
    bool eventFilter(QObject *O, QEvent *E) override
    {
        if (E->type() != QEvent::KeyPress) return false;
        auto *W = qobject_cast<QWidget *>(O);
        if (!W || W->window() != FForm || !FForm->isActiveWindow()) return false;
        auto *K = static_cast<QKeyEvent *>(E);
        return FH(K->key(), K->modifiers());
    }
    QWidget *FForm;
    std::function<bool(int, Qt::KeyboardModifiers)> FH;
};

void installKeyPreview(QWidget *Form, const std::function<bool(int Key, Qt::KeyboardModifiers Mods)> &Handler)
{
    new KeyPreviewFilter(Form, Handler);
}

class LabelClickFilter : public QObject
{
public:
    LabelClickFilter(QLabel *Label, std::function<void()> Handler) : QObject(Label), FHandler(std::move(Handler)) { Label->installEventFilter(this); }
protected:
    bool eventFilter(QObject *Watched, QEvent *Event) override
    {
        if (Event->type() == QEvent::MouseButtonRelease && static_cast<QMouseEvent *>(Event)->button() == Qt::LeftButton) {
            if (FHandler) FHandler();
            return true;
        }
        return QObject::eventFilter(Watched, Event);
    }
private:
    std::function<void()> FHandler;
};

void onLabelClick(QLabel *Label, std::function<void()> Handler)
{
    if (Label != nullptr) new LabelClickFilter(Label, std::move(Handler));
}

void makeLinkLabel(QLabel *Label)
{
    if (Label == nullptr) return;
    QPalette P = Label->palette();
    P.setColor(QPalette::WindowText, QColor(0, 0, 255));   // clBlue
    Label->setPalette(P);
    QFont F = Label->font();
    F.setUnderline(true);
    Label->setFont(F);
    Label->setCursor(Qt::PointingHandCursor);
}

// ---------------------------------------------------------------------------

void setupForm(QWidget *Top)
{
    setupActions(Top);
    setupToolBars(Top);
    setupPopupMenus(Top);
    setupButtons(Top);
    setupMisc(Top);
    if (auto *MW = qobject_cast<QMainWindow *>(Top)) {
        if (QWidget *C = MW->centralWidget()) installEngines(C);
    } else {
        installEngines(Top);
    }
    applyWindowSettings(Top);
    new AutoSizeOnShow(Top);
}

}  // namespace VclLayout
