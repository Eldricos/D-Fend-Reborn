// C++ port of ViewImageFormUnit.pas (implementation section).
#include "ViewImageFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "WallpaperStyleFormUnit.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"
#include "ImageStretch.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QScreen>

TViewImageForm* ViewImageForm = nullptr;

// Screen.WorkAreaWidth / WorkAreaHeight / Width / Height
static QRect ScreenWorkArea(QWidget* W)
{
    QScreen* S = W->screen();
    if (S == nullptr) S = QGuiApplication::primaryScreen();
    return S ? S->availableGeometry() : QRect(0, 0, 1024, 768);
}
static QRect ScreenRect(QWidget* W)
{
    QScreen* S = W->screen();
    if (S == nullptr) S = QGuiApplication::primaryScreen();
    return S ? S->geometry() : QRect(0, 0, 1024, 768);
}

TViewImageForm::TViewImageForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    ImageList = TImageList::FromResource("ViewImageFormUnit", "ImageList");
    SaveDialog = new TSaveDialog(this);
    SaveDialog->DefaultExt = "png";
    Timer = new QTimer(this);
    Timer->setInterval(500);
    Timer->setSingleShot(false);

    connect(ui.PreviousButton, &QAction::triggered, this, [this]() { ButtonWork(ui.PreviousButton); });
    connect(ui.NextButton, &QAction::triggered, this, [this]() { ButtonWork(ui.NextButton); });
    connect(ui.CopyButton, &QAction::triggered, this, [this]() { ButtonWork(ui.CopyButton); });
    connect(ui.SaveButton, &QAction::triggered, this, [this]() { ButtonWork(ui.SaveButton); });
    connect(ui.ClearButton, &QAction::triggered, this, [this]() { ButtonWork(ui.ClearButton); });
    connect(ui.BackgroundButton, &QAction::triggered, this, [this]() { ButtonWork(ui.BackgroundButton); });
    connect(ui.ZoomPopupMenu, &QMenu::aboutToShow, this, [this]() { ZoomPopupMenuPopup(ui.ZoomPopupMenu); });
    QAction* ZoomItems[] = {ui.ZoomMenuItem1, ui.ZoomMenuItem2, ui.ZoomMenuItem3, ui.ZoomMenuItem4, ui.ZoomMenuItem5, ui.ZoomMenuItem6,
                            ui.ZoomMenuItem7, ui.ZoomMenuItem8, ui.ZoomMenuItem9, ui.ZoomMenuItem10, ui.ZoomMenuItem11, ui.ZoomMenuItem12};
    for (QAction* A : ZoomItems) connect(A, &QAction::triggered, this, [this, A]() { ButtonWork(A); });
    connect(Timer, &QTimer::timeout, this, [this]() { TimerTimer(Timer); });
    ui.StatusBar->installEventFilter(this);
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);
}

TViewImageForm::~TViewImageForm()
{
    FormDestroy(this);
    delete ImageList;
    delete SaveDialog;
}

void TViewImageForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);
}

void TViewImageForm::resizeEvent(QResizeEvent *E)
{
    TVclForm::resizeEvent(E);
    FormResize(this);
}

void TViewImageForm::closeEvent(QCloseEvent *E)
{
    TVclForm::closeEvent(E);
    if (!E->isAccepted()) return;
    TCloseAction Action = caHide;
    FormClose(this, Action);
    if (Action == caFree) deleteLater();
}

void TViewImageForm::wheelEvent(QWheelEvent *E)
{
    bool Handled = false;
    const int Delta = E->angleDelta().y();
    if (Delta == 0) { TVclForm::wheelEvent(E); return; }
    FormMouseWheel(this, E->modifiers(), Delta, E->position().toPoint(), Handled);
    E->accept();
}

bool TViewImageForm::eventFilter(QObject *O, QEvent *E)
{
    if (O == ui.StatusBar && E->type() == QEvent::MouseButtonPress) {
        auto *ME = static_cast<QMouseEvent*>(E);
        StatusBarMouseDown(ui.StatusBar, ME->button(), ME->modifiers(), ME->position().toPoint().x(), ME->position().toPoint().y());
    }
    return TVclForm::eventFilter(O, E);
}

// TViewImageForm

void TViewImageForm::CenterWindow()
{
    QWidget* P = parentWidget();
    if (P == nullptr) return;

    if (P->isWindow()) {
        // Owner is TForm
        const QRect R = P->frameGeometry();
        move(Max(0, (R.left() + R.width() / 2) - frameGeometry().width() / 2), Max(0, (R.top() + R.height() / 2) - frameGeometry().height() / 2));
    } else {
        // Owner is TFrame
        const QRect R = P->geometry();
        move(Max(0, (R.left() + R.width() / 2) - frameGeometry().width() / 2), Max(0, (R.top() + R.height() / 2) - frameGeometry().height() / 2));
    }
}

void TViewImageForm::FormClose(QObject* Sender, TCloseAction& Action)
{
    Q_UNUSED(Sender);
    if ((Action == caHide) && NonModal) Action = caFree;
}

void TViewImageForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts / DoubleBuffered: not needed in Qt

    PrevImages = new TStringList();
    NextImages = new TStringList();

    NonModal = false;
}

void TViewImageForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    LoadLanguage();
    LoadImage(true);
}

void TViewImageForm::LoadLanguage()
{
    // Font.Charset: not needed in Qt
    setWindowTitle(LanguageSetup->ViewImageForm());
    ui.PreviousButton->setText(LanguageSetup->Previous());
    ui.PreviousButton->setToolTip(LanguageSetup->PreviousHintMediaViewer());
    ui.NextButton->setText(LanguageSetup->Next());
    ui.NextButton->setToolTip(LanguageSetup->NextHintMediaViewer());
    ui.CopyButton->setText(LanguageSetup->Copy());
    ui.CopyButton->setToolTip(LanguageSetup->CopyHintImageViewer());
    ui.SaveButton->setText(LanguageSetup->Save());
    ui.SaveButton->setToolTip(LanguageSetup->SaveHintImageViewer());
    ui.ClearButton->setText(LanguageSetup->Clear());
    ui.ClearButton->setToolTip(LanguageSetup->ClearMediaViewer());
    ui.ZoomButton->setText(LanguageSetup->ViewImageFormZoomButton());
    ui.ZoomButton->setToolTip(LanguageSetup->ViewImageFormZoomButtonHint());
    ui.BackgroundButton->setText(LanguageSetup->ViewImageFormBackgroundButton());
    ui.BackgroundButton->setToolTip(LanguageSetup->ViewImageFormBackgroundButtonHint());

    {
        QFont F = ui.CoolBar->font();
        if (PrgSetup->ToolbarFontSize() > 0) F.setPointSize(PrgSetup->ToolbarFontSize());
        ui.CoolBar->setFont(F);
        ui.ToolBar->setFont(F);
    }

    UserIconLoader->DialogImage(DI_CopyToClipboard, ImageList, 0);
    UserIconLoader->DialogImage(DI_Save, ImageList, 1);
    UserIconLoader->DialogImage(DI_Clear, ImageList, 2);
    UserIconLoader->DialogImage(DI_BackgroundImage, ImageList, 3);
    UserIconLoader->DialogImage(DI_Previous, ImageList, 4);
    UserIconLoader->DialogImage(DI_Next, ImageList, 5);
    UserIconLoader->DialogImage(DI_Zoom, ImageList, 6);

    // ToolBar.Images = ImageList: apply the (possibly user defined) icons
    ui.PreviousButton->setIcon(ImageList->Icon(4));
    ui.NextButton->setIcon(ImageList->Icon(5));
    ui.CopyButton->setIcon(ImageList->Icon(0));
    ui.SaveButton->setIcon(ImageList->Icon(1));
    ui.ClearButton->setIcon(ImageList->Icon(2));
    ui.ZoomButton->setIcon(ImageList->Icon(6));
    ui.BackgroundButton->setIcon(ImageList->Icon(3));
}

void TViewImageForm::LoadImage(bool MoveWindow)
{
    int W, H;

    IntPicture = QPixmap();
    IntBitmap100 = QImage();

    ui.PreviousButton->setEnabled(PrevImages->Count() > 0);
    ui.NextButton->setEnabled(NextImages->Count() > 0);

    if (ImageFile == "") IntPicture = QPixmap(); else IntPicture = LoadImageFromFile(ImageFile);
    if (IntPicture.isNull()) {
        IntPicture = QPixmap(10, 10);
        IntPicture.fill(Qt::white);
    }
    ui.Image->setPicture(IntPicture);
    setWindowTitle(LanguageSetup->ViewImageForm() + " [" + MakeRelPath(ImageFile, PrgSetup->BaseDir()) + "]");

    const QRect WorkArea = ScreenWorkArea(this);
    const QRect Scr = ScreenRect(this);
    const int FrameW = frameGeometry().width() - width();
    const int FrameH = frameGeometry().height() - height();

    W = IntPicture.width(); H = IntPicture.height();
    if ((H < Scr.height() / 3) && (W < Scr.width() / 3)) { W = W * 2; H = H * 2; }
    const int CH = Min(Max(100, H + (height() - ui.Image->height())), WorkArea.height() - 10 - (height() + FrameH - ui.Image->height()));
    VclLayout::setClientSize(this, width(), CH);
    const int CW = Min(Max(640, W), WorkArea.width() - 10 - (width() + FrameW - ui.Image->width()));
    VclLayout::setClientSize(this, CW, height());

    if (MoveWindow || (x() + frameGeometry().width() >= WorkArea.width() - 10) || (y() + frameGeometry().height() >= WorkArea.height() - 10)) CenterWindow();

    FormResize(this);
    TimerTimer(this);
}

void TViewImageForm::UpdateImageList()
{
    LoadImage(false);
}

void TViewImageForm::StatusBarMouseDown(QObject* Sender, Qt::MouseButton Button, TShiftState Shift, int X, int Y)
{
    Q_UNUSED(Sender); Q_UNUSED(Button); Q_UNUSED(Shift);
    QPoint P;
    // StatusBar.Panels[0].Width + StatusBar.Panels[1].Width
    int W01 = 0;
    {
        const QStringList L = ui.StatusBar->property("vclPanelWidths").toString().split(',');
        if (L.size() >= 2) W01 = L[0].toInt() + L[1].toInt(); else W01 = 175;
    }
    if (X > W01) return;
    P = ui.StatusBar->mapToGlobal(QPoint(X, Y));
    ui.ZoomPopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
}

void TViewImageForm::Zoom(int ZoomDirection)
{
    int I, J;
    double F;

    if (IntPicture.isNull() || IntPicture.width() == 0 || IntPicture.height() == 0) return;
    I = Round(double(ui.Image->width()) / IntPicture.width() * 100);
    J = Round(double(ui.Image->height()) / IntPicture.height() * 100);

    I = Round(Min(I, J) / 25.0) * 25;
    if (ZoomDirection == 0) I = 100; else {
        if (ZoomDirection < 0) I = Max(10, I - 25); else I = Max(10, I + 25);
    }

    const QRect WorkArea = ScreenWorkArea(this);
    F = I / 100.0;
    VclLayout::setClientSize(this, width(), Min(Max(100, Round(F * IntPicture.height()) + (height() - ui.Image->height())), WorkArea.height() - 10));
    VclLayout::setClientSize(this, Min(Max(850, Round(F * IntPicture.width())), WorkArea.width() - 10), height());
    CenterWindow();
}

void TViewImageForm::ZoomPopupMenuPopup(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I, J, Z, M;
    QString S;

    if (IntPicture.isNull() || IntPicture.width() == 0 || IntPicture.height() == 0) return;
    I = Round(double(ui.Image->width()) / IntPicture.width() * 100);
    J = Round(double(ui.Image->height()) / IntPicture.height() * 100);
    Z = Min(I, J);

    const QList<QAction*> Items = ui.ZoomPopupMenu->actions();
    for (I = 0; I < Items.size(); I++) {
        Items[I]->setShortcut(QKeySequence());
        S = RemoveUnderline(Items[I]->text());
        M = StrToIntDef(Copy(S, 1, Length(S) - 1), 0);
        Items[I]->setEnabled(M != Z);
        if ((M < Z) && (M + 25 >= Z)) Items[I]->setShortcut(QKeySequence(Qt::Key_Minus));
        if ((M > Z) && (M - 25 <= Z)) Items[I]->setShortcut(QKeySequence(Qt::Key_Plus));
        if (M == 100) Items[I]->setShortcut(QKeySequence(Qt::Key_0));
    }
}

void TViewImageForm::ButtonWork(QObject* Sender)
{
    TWallpaperStyle WPStype = WSCenter;
    QString S;
    double F;

    switch (Sender->property("tag").toInt()) {
        case 1: QGuiApplication::clipboard()->setPixmap(IntPicture); break;
        case 2:
            SaveDialog->FileName = "";
            SaveDialog->Title = LanguageSetup->ViewImageFormSaveTitle();
            SaveDialog->Filter = LanguageSetup->ViewImageFormSaveFilter();
            if (!SaveDialog->Execute()) return;
            SaveImageToFile(ImageFile, SaveDialog->FileName);
            break;
        case 3:
            if (!ExtDeleteFile(ImageFile, ftMediaViewer)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {ImageFile}), mtError, mbOK, 0);
                return;
            }
            Close();
            break;
        case 4:
            if (!ShowWallpaperStyleDialog(this, WPStype)) return;
            SetDesktopWallpaper(ImageFile, WPStype);
            break;
        case 5:
            if (PrevImages->Count() > 0) {
                NextImages->Insert(0, ImageFile);
                ImageFile = (*PrevImages)[PrevImages->Count() - 1];
                PrevImages->Delete(PrevImages->Count() - 1);
                LoadImage(false);
            }
            break;
        case 6:
            if (NextImages->Count() > 0) {
                PrevImages->Add(ImageFile);
                ImageFile = (*NextImages)[0];
                NextImages->Delete(0);
                LoadImage(false);
            }
            break;
        case 7: {
            if (isMaximized()) showNormal();
            auto* A = qobject_cast<QAction*>(Sender);
            if (A == nullptr) return;
            S = RemoveUnderline(A->text());
            F = StrToInt(Copy(S, 1, Length(S) - 1)) / 100.0;
            const QRect WorkArea = ScreenWorkArea(this);
            VclLayout::setClientSize(this, width(), Min(Max(100, Round(F * IntPicture.height()) + (height() - ui.Image->height())), WorkArea.height() - 10));
            VclLayout::setClientSize(this, Min(Max(850, Round(F * IntPicture.width())), WorkArea.width() - 10), height());
            CenterWindow();
            TimerTimer(this);
            break;
        }
    }
}

void TViewImageForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    delete PrevImages; PrevImages = nullptr;
    delete NextImages; NextImages = nullptr;
    IntPicture = QPixmap();
    IntBitmap100 = QImage();
    if (NonModal && ViewImageForm == this) ViewImageForm = nullptr;
}

void TViewImageForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    // Delphi distinguishes the numpad keys by virtual key code; Qt reports them
    // with the KeypadModifier.
    Shift &= ~Qt::KeypadModifier;
    if (Shift == Qt::NoModifier) switch (Key) {
        case Qt::Key_Left: case Qt::Key_Up: case Qt::Key_PageUp: ButtonWork(ui.PreviousButton); break;
        case Qt::Key_Right: case Qt::Key_Down: case Qt::Key_PageDown: ButtonWork(ui.NextButton); break;
        case Qt::Key_Plus: case Qt::Key_Equal: Zoom(1); break;
        case Qt::Key_Minus: Zoom(-1); break;
        case Qt::Key_0: case Qt::Key_Asterisk: Zoom(0); break;
        case Qt::Key_Escape: Close(); break;
    }
}

void TViewImageForm::FormMouseWheel(QObject* Sender, TShiftState Shift, int WheelDelta, QPoint MousePos, bool& Handled)
{
    Q_UNUSED(Sender); Q_UNUSED(MousePos); Q_UNUSED(Handled);
    if (Shift == Qt::NoModifier) {
        if (WheelDelta > 0) ButtonWork(ui.PreviousButton); else ButtonWork(ui.NextButton);
    } else {
        if (WheelDelta > 0) Zoom(-1); else Zoom(1);
    }
}

void TViewImageForm::TimerTimer(QObject* Sender)
{
    Q_UNUSED(Sender);
    double D;
    QImage B;

    Timer->stop();
    if (IntPicture.isNull() || (IntPicture.width() == 0)) return;

    D = Min(double(ui.Image->width()) / IntPicture.width(), double(ui.Image->height()) / IntPicture.height());

    if ((D < 1) || IntBitmap100.isNull()) {
        B = ScaleImage(IntPicture, D);
        ui.Image->setPicture(QPixmap::fromImage(B));
        if (D == 1) {
            IntBitmap100 = B;
        }
        return;
    }

    if (!IntBitmap100.isNull())
        ui.Image->setPicture(QPixmap::fromImage(IntBitmap100));
}

void TViewImageForm::FormResize(QObject* Sender)
{
    Q_UNUSED(Sender);
    double D;
    if (IntPicture.isNull() || IntPicture.width() == 0 || IntPicture.height() == 0) return;

    VclLayout::setStatusText(ui.StatusBar, 0, IntToStr(IntPicture.width()) + "x" + IntToStr(IntPicture.height()));
    D = Min(double(ui.Image->width()) / IntPicture.width(), double(ui.Image->height()) / IntPicture.height());
    VclLayout::setStatusText(ui.StatusBar, 1, IntToStr(Round(D * 100)) + "%");
    VclLayout::setStatusText(ui.StatusBar, 2, ImageFile);

    Timer->stop();
    Timer->start();
}

// global

void ShowNonModalImageDialog(QWidget* AOwner, const QString& AImageFile, TStringList* APrevImages, TStringList* ANextImages)
{
    if (AImageFile != "") {
        if (OpenMediaFile(PrgSetup->ImageViewer(), AImageFile)) return;
    }

    if (ViewImageForm == nullptr) ViewImageForm = new TViewImageForm(AOwner);
    ViewImageForm->ImageFile = AImageFile;
    if (APrevImages != nullptr) ViewImageForm->PrevImages->Assign(*APrevImages);
    if (ANextImages != nullptr) ViewImageForm->NextImages->Assign(*ANextImages);
    ViewImageForm->NonModal = true;
    if (ViewImageForm->isVisible()) ViewImageForm->UpdateImageList(); else ViewImageForm->show();
}

void ShowImageDialog(QWidget* AOwner, const QString& AImageFile, TStringList* APrevImages, TStringList* ANextImages)
{
    if (OpenMediaFile(PrgSetup->ImageViewer(), AImageFile)) return;
    if (ViewImageForm != nullptr) { TViewImageForm* F = ViewImageForm; ViewImageForm = nullptr; delete F; }

    ViewImageForm = new TViewImageForm(AOwner);
    ViewImageForm->ImageFile = AImageFile;
    if (APrevImages != nullptr) ViewImageForm->PrevImages->Assign(*APrevImages);
    if (ANextImages != nullptr) ViewImageForm->NextImages->Assign(*ANextImages);
    if ((APrevImages == nullptr) && (ANextImages == nullptr)) {
        ViewImageForm->ui.PreviousButton->setVisible(false);
        ViewImageForm->ui.NextButton->setVisible(false);
        // ToolButton2 is the first separator of the tool bar (after CopyButton)
        const QList<QAction*> Acts = ViewImageForm->ui.ToolBar->actions();
        const int Idx = Acts.indexOf(ViewImageForm->ui.CopyButton);
        if (Idx >= 0 && Idx + 1 < Acts.size() && Acts[Idx + 1]->isSeparator()) Acts[Idx + 1]->setVisible(false);
        ViewImageForm->ui.ToolButton2->setVisible(false);
    }
    ViewImageForm->ShowModal();
    TViewImageForm* F = ViewImageForm; ViewImageForm = nullptr; delete F;
}
