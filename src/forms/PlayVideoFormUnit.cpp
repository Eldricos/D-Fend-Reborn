// C++ port of PlayVideoFormUnit.pas (implementation section).
//
// The original used the external MediaInterface DLL (DirectShow) which drew
// the video into MediaPanel.  The port uses Qt Multimedia: a QMediaPlayer
// renders into a QVideoWidget that is placed inside MediaPanel at the
// position setVideoPos() was called with.  The MediaInterface routines are
// provided as private helpers with the same names (positions in ms).
#include "PlayVideoFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "IconLoaderUnit.h"
#include "Process.h"
#include <QMediaPlayer>
#include <QMediaMetaData>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QVideoSink>
#include <QEventLoop>
#include <QFile>
#include <QUrl>

TPlayVideoForm* PlayVideoForm = nullptr;

TPlayVideoForm::TPlayVideoForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    ImageList = TImageList::FromResource("PlayVideoFormUnit", "ImageList");
    SaveDialog = new TSaveDialog(this);
    SaveDialog->DefaultExt = "wav";
    Timer = new QTimer(this);
    Timer->setInterval(50);

    FPlayer = new QMediaPlayer(this);
    FAudioOutput = new QAudioOutput(this);
    FPlayer->setAudioOutput(FAudioOutput);
    FVideoWidget = new QVideoWidget(ui.MediaPanel);
    FVideoWidget->setAspectRatioMode(Qt::IgnoreAspectRatio);
    FVideoWidget->setGeometry(5, 5, qMax(1, ui.MediaPanel->width() - 10), qMax(1, ui.MediaPanel->height() - 10));
    FVideoWidget->hide();
    FPlayer->setVideoOutput(FVideoWidget);

    connect(ui.PreviousButton, &QAction::triggered, this, [this]() { ButtonWork(ui.PreviousButton); });
    connect(ui.NextButton, &QAction::triggered, this, [this]() { ButtonWork(ui.NextButton); });
    connect(ui.SaveButton, &QAction::triggered, this, [this]() { ButtonWork(ui.SaveButton); });
    connect(ui.PlayPauseButton, &QAction::triggered, this, [this]() { ButtonWork(ui.PlayPauseButton); });
    connect(ui.InstallButton, &QPushButton::clicked, this, [this]() { InstallButtonClick(ui.InstallButton); });
    connect(ui.TrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.TrackBar); });
    connect(Timer, &QTimer::timeout, this, [this]() { TimerTimer(Timer); });
    connect(ui.ZoomPopupMenu, &QMenu::aboutToShow, this, [this]() { ZoomPopupMenuPopup(ui.ZoomPopupMenu); });
    QAction* ZoomItems[] = {ui.ZoomMenuItem1, ui.ZoomMenuItem2, ui.ZoomMenuItem3, ui.ZoomMenuItem4, ui.ZoomMenuItem5, ui.ZoomMenuItem6,
                            ui.ZoomMenuItem7, ui.ZoomMenuItem8, ui.ZoomMenuItem9, ui.ZoomMenuItem10, ui.ZoomMenuItem11, ui.ZoomMenuItem12};
    for (QAction* A : ZoomItems) connect(A, &QAction::triggered, this, [this, A]() { ButtonWork(A); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);
    Timer->start();   // Timer.Enabled = True (DFM default)
}

TPlayVideoForm::~TPlayVideoForm()
{
    FormDestroy(this);
    delete ImageList;
    delete SaveDialog;
}

void TPlayVideoForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);
}

void TPlayVideoForm::resizeEvent(QResizeEvent *E)
{
    TVclForm::resizeEvent(E);
    FormResize(this);
}

void TPlayVideoForm::wheelEvent(QWheelEvent *E)
{
    bool Handled = false;
    const int Delta = E->angleDelta().y();
    if (Delta == 0) { TVclForm::wheelEvent(E); return; }
    FormMouseWheel(this, E->modifiers(), Delta, E->position().toPoint(), Handled);
    E->accept();
}

// --- MediaInterface replacement --------------------------------------------

void TPlayVideoForm::loadMediaFile(const QString& AFileName)
{
    FPlayer->setSource(QUrl::fromLocalFile(AFileName));
    // MediaInterface loaded synchronously: wait until Qt has analysed the file
    if (FPlayer->mediaStatus() == QMediaPlayer::LoadingMedia) {
        QEventLoop Loop;
        QTimer TimeOut;
        TimeOut.setSingleShot(true);
        connect(&TimeOut, &QTimer::timeout, &Loop, &QEventLoop::quit);
        connect(FPlayer, &QMediaPlayer::mediaStatusChanged, &Loop, [&Loop](QMediaPlayer::MediaStatus S) {
            if (S != QMediaPlayer::LoadingMedia) Loop.quit();
        });
        connect(FPlayer, &QMediaPlayer::errorOccurred, &Loop, &QEventLoop::quit);
        TimeOut.start(5000);
        Loop.exec();
    }
    FVideoSize = QSize();
    if (MediaStreamAvailable()) {
        FVideoSize = FPlayer->metaData().value(QMediaMetaData::Resolution).toSize();
        if (!FVideoSize.isValid() || FVideoSize.isEmpty()) {
            const QList<QMediaMetaData> Tracks = FPlayer->videoTracks();
            int T = FPlayer->activeVideoTrack();
            if (T < 0 && !Tracks.isEmpty()) T = 0;
            if (T >= 0 && T < Tracks.size()) FVideoSize = Tracks[T].value(QMediaMetaData::Resolution).toSize();
        }
        if ((!FVideoSize.isValid() || FVideoSize.isEmpty()) && FPlayer->hasVideo()) FVideoSize = QSize(640, 480);
    }
}

bool TPlayVideoForm::MediaStreamAvailable() const
{
    if (FPlayer->source().isEmpty() || FPlayer->error() != QMediaPlayer::NoError) return false;
    switch (FPlayer->mediaStatus()) {
        case QMediaPlayer::LoadedMedia: case QMediaPlayer::BufferingMedia: case QMediaPlayer::BufferedMedia:
        case QMediaPlayer::EndOfMedia: case QMediaPlayer::StalledMedia: return true;
        default: return false;
    }
}

bool TPlayVideoForm::VideoAvailable() const { return MediaStreamAvailable() && FPlayer->hasVideo() && FVideoSize.width() > 0; }
bool TPlayVideoForm::MediaStreamPlayed() const { return FPlayer->playbackState() == QMediaPlayer::PlayingState; }
void TPlayVideoForm::playMediaStream() { FPlayer->play(); }
void TPlayVideoForm::pauseMediaStream() { FPlayer->pause(); }
void TPlayVideoForm::freeMediaStream()
{
    if (FPlayer == nullptr) return;
    FPlayer->stop();
    FPlayer->setSource(QUrl());
    FVideoSize = QSize();
    if (FVideoWidget != nullptr) FVideoWidget->hide();
}

qint64 TPlayVideoForm::getMediaStreamPos() const
{
    // QMediaPlayer stops at the end of the stream and may reset the position
    if (FPlayer->mediaStatus() == QMediaPlayer::EndOfMedia) return FPlayer->duration();
    return FPlayer->position();
}

qint64 TPlayVideoForm::getMediaStreamDuration() const { return FPlayer->duration(); }
void TPlayVideoForm::setMediaStreamPos(qint64 Pos) { FPlayer->setPosition(Pos); }
int TPlayVideoForm::getVideoWidth() const { return FVideoSize.width(); }

int TPlayVideoForm::getCoupledVideoHeight(int Width) const
{
    if (FVideoSize.width() <= 0) return 0;
    return int((qint64(Width) * FVideoSize.height()) / FVideoSize.width());
}

void TPlayVideoForm::setVideoPos(int X, int Y, int W, int H)
{
    FVideoWidget->setGeometry(X, Y, qMax(1, W), qMax(1, H));
    FVideoWidget->show();
    FVideoWidget->raise();
}

// ----------------------------------------------------------------------------

void TPlayVideoForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts / Font.Charset: not needed in Qt

    {
        QPalette P = ui.InfoLabel->palette();
        P.setColor(QPalette::WindowText, Qt::white);   // InfoLabel.Font.Color:=clWhite
        ui.InfoLabel->setPalette(P);
    }

    ui.PreviousButton->setText(LanguageSetup->Previous());
    ui.PreviousButton->setToolTip(LanguageSetup->PreviousHintMediaViewer());
    ui.NextButton->setText(LanguageSetup->Next());
    ui.NextButton->setToolTip(LanguageSetup->NextHintMediaViewer());
    ui.SaveButton->setText(LanguageSetup->Save());
    ui.SaveButton->setToolTip(LanguageSetup->SaveHintVideoPlayer());
    ui.PlayPauseButton->setText(LanguageSetup->SoundCapturePlayPause());
    ui.PlayPauseButton->setToolTip(LanguageSetup->SoundCapturePlayPauseHint());
    ui.ZoomButton->setText(LanguageSetup->ViewImageFormZoomButton());
    ui.ZoomButton->setToolTip(LanguageSetup->ViewImageFormZoomButtonHint());
    ui.InfoLabel->setText(LanguageSetup->CaptureVideosInstallCodecInfo());
    ui.InstallButton->setText(LanguageSetup->CaptureVideosInstallCodec());

    UserIconLoader->DialogImage(DI_Save, ImageList, 0);
    UserIconLoader->DialogImage(DI_Previous, ImageList, 1);
    UserIconLoader->DialogImage(DI_Next, ImageList, 2);
    UserIconLoader->DialogImage(DI_Run, ImageList, 3);
    UserIconLoader->DialogImage(DI_Zoom, ImageList, 4);
    // ToolBar.Images = ImageList
    ui.SaveButton->setIcon(ImageList->Icon(0));
    ui.PreviousButton->setIcon(ImageList->Icon(1));
    ui.NextButton->setIcon(ImageList->Icon(2));
    ui.PlayPauseButton->setIcon(ImageList->Icon(3));
    ui.ZoomButton->setIcon(ImageList->Icon(4));

    {
        QFont F = ui.CoolBar->font();
        if (PrgSetup->ToolbarFontSize() > 0) F.setPointSize(PrgSetup->ToolbarFontSize());
        ui.CoolBar->setFont(F);
        ui.ToolBar->setFont(F);
    }

    JustChanging = false;

    PrevVideos = new TStringList();
    NextVideos = new TStringList();
}

void TPlayVideoForm::FormShow(QObject* Sender)
{
    bool OK;

    ui.PreviousButton->setEnabled(PrevVideos->Count() > 0);
    ui.NextButton->setEnabled(NextVideos->Count() > 0);
    setWindowTitle(LanguageSetup->CaptureVideos() + " [" + MakeRelPath(FileName, PrgSetup->BaseDir()) + "]");

    if (!FileExists(FileName)) {
        ui.InfoLabel->setText(Format(LanguageSetup->MessageCouldNotFindFile(), {FileName}));
        ui.InfoLabel->setVisible(true);
        ui.InstallButton->setVisible(false);
        ui.PlayPauseButton->setEnabled(false);
        ui.Panel1->setVisible(false);
        ui.ZoomButton->setEnabled(false);
        ui.SaveButton->setEnabled(false);
        return;
    } else {
        ui.SaveButton->setEnabled(true);
    }

    if (VideoFileDamaged()) {
        ui.InfoLabel->setText(LanguageSetup->CaptureVideosVideoDamaged());
        ui.InfoLabel->setVisible(true);
        ui.InstallButton->setVisible(false);
        ui.PlayPauseButton->setEnabled(false);
        ui.Panel1->setVisible(false);
        ui.ZoomButton->setEnabled(false);
        return;
    } else {
        ui.InfoLabel->setText(LanguageSetup->CaptureVideosInstallCodecInfo());
    }

    loadMediaFile(FileName);
    OK = MediaStreamAvailable() && VideoAvailable();
    ui.InfoLabel->setVisible(!OK);
#ifdef Q_OS_WIN
    ui.InstallButton->setVisible(!OK);
#else
    ui.InstallButton->setVisible(false);   // InstallVideoCodec.exe only exists on Windows
#endif
    ui.PlayPauseButton->setEnabled(OK);
    ui.Panel1->setVisible(OK);
    ui.ZoomButton->setEnabled(OK);
    if (!OK) FVideoWidget->hide();

    if (OK) {
        resize(getVideoWidth() + (width() - ui.MediaPanel->width() + 10), height());
        resize(width(), getCoupledVideoHeight(getVideoWidth()) + (height() - ui.MediaPanel->height() + 10));
        VclLayout::relayout(this);
        setVideoPos(5, 5, ui.MediaPanel->width() - 10, ui.MediaPanel->height() - 10);
        VclLayout::setStatusText(ui.StatusBar, 1, IntToStr(getVideoWidth()) + "x" + IntToStr(getCoupledVideoHeight(getVideoWidth())));
        playMediaStream();
        TimerTimer(Sender);
    }
}

bool TPlayVideoForm::VideoFileDamaged()
{
    QString S;
    char C[4];
    bool result = false;

    S = MakeAbsPath(FileName, PrgSetup->BaseDir());
    if (!FileExists(S)) return result;

    if (Trim(ExtUpperCase(ExtractFileExt(S))) != ".AVI") return result;

    QFile FSt(S);
    if (!FSt.open(QIODevice::ReadOnly)) return result;
    if (FSt.size() < 4) { result = true; return result; }
    if (FSt.read(C, 4) != 4) { result = true; return result; }
    if ((C[0] != 'R') || (C[1] != 'I') || (C[2] != 'F') || (C[3] != 'F')) { result = true; return result; }
    return result;
}

void TPlayVideoForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    freeMediaStream();
    delete PrevVideos; PrevVideos = nullptr;
    delete NextVideos; NextVideos = nullptr;
}

void TPlayVideoForm::Zoom(int ZoomDirection)
{
    int I, J;
    double F;

    if (isMaximized()) showNormal();

    if (getVideoWidth() == 0) return;

    I = (ui.MediaPanel->width() + 10) * 100 / getVideoWidth();
    J = (ui.MediaPanel->height() + 10) * 100 / Max(1, getCoupledVideoHeight(getVideoWidth()));

    I = Round(Min(I, J) / 25.0) * 25;
    if (ZoomDirection == 0) I = 100; else {
        if (ZoomDirection < 0) I = Max(10, I - 25); else I = Max(10, I + 25);
    }

    F = I / 100.0;
    resize(Round(getVideoWidth() * F) + (width() - ui.MediaPanel->width() + 10), height());
    resize(width(), Round(getCoupledVideoHeight(getVideoWidth()) * F) + (height() - ui.MediaPanel->height() + 10));
    CenterWindow();
}

void TPlayVideoForm::ZoomPopupMenuPopup(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I, J;
    QString S;

    I = getVideoWidth(); if (I == 0) return;

    I = (ui.MediaPanel->width() + 10) * 100 / I;
    J = (ui.MediaPanel->height() + 10) * 100 / Max(1, getCoupledVideoHeight(getVideoWidth()));
    J = Round(Min(I, J) / 25.0) * 25;

    const QList<QAction*> Items = ui.ZoomPopupMenu->actions();
    for (I = 0; I < Items.size(); I++) {
        Items[I]->setShortcut(QKeySequence());
        S = RemoveUnderline(Items[I]->text());
        Items[I]->setEnabled(S != IntToStr(J) + "%");
        if (S == IntToStr(J - 25) + "%") Items[I]->setShortcut(QKeySequence(Qt::Key_Minus));
        if (S == IntToStr(J + 25) + "%") Items[I]->setShortcut(QKeySequence(Qt::Key_Plus));
        if (S == "100%") Items[I]->setShortcut(QKeySequence(Qt::Key_0));
    }
}

void TPlayVideoForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
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

void TPlayVideoForm::FormMouseWheel(QObject* Sender, TShiftState Shift, int WheelDelta, QPoint MousePos, bool& Handled)
{
    Q_UNUSED(Sender); Q_UNUSED(MousePos); Q_UNUSED(Handled);
    if (Shift == Qt::NoModifier) {
        if (WheelDelta > 0) ButtonWork(ui.PreviousButton); else ButtonWork(ui.NextButton);
    } else {
        if (WheelDelta > 0) Zoom(-1); else Zoom(1);
    }
}

void TPlayVideoForm::FormResize(QObject* Sender)
{
    Q_UNUSED(Sender);
    double D;
    int I;

    if (!VideoAvailable()) return;

    D = getCoupledVideoHeight(1000000) / 1000000.0;
    if (ui.MediaPanel->height() - 10 >= Round((ui.MediaPanel->width() - 10) * D)) {
        // use full width
        I = Round((ui.MediaPanel->width() - 10) * D);
        setVideoPos(5, ui.MediaPanel->height() / 2 - I / 2, ui.MediaPanel->width() - 10, I);
        VclLayout::setStatusText(ui.StatusBar, 2, IntToStr(Round((ui.MediaPanel->width() - 10) * 100.0 / getVideoWidth())) + "%");
    } else {
        // use full height
        I = Round((ui.MediaPanel->height() - 10) / D);
        setVideoPos(ui.MediaPanel->width() / 2 - I / 2, 5, I, ui.MediaPanel->height() - 10);
        VclLayout::setStatusText(ui.StatusBar, 2, IntToStr(Round((ui.MediaPanel->height() - 10) * 100.0 / getCoupledVideoHeight(getVideoWidth()))) + "%");
    }
}

void TPlayVideoForm::TimerTimer(QObject* Sender)
{
    Q_UNUSED(Sender);
    JustChanging = true;
    if (getMediaStreamDuration() > 0) {
        ui.TrackBar->setValue(Round(100.0 * getMediaStreamPos() / getMediaStreamDuration()));
        VclLayout::setStatusText(ui.StatusBar, 0, QString::number(getMediaStreamPos() / 1000.0, 'f', 3) + "/" + QString::number(getMediaStreamDuration() / 1000.0, 'f', 3));
        VclLayout::setStatusText(ui.StatusBar, 3, FileName);
        if ((getMediaStreamPos() == getMediaStreamDuration()) && MediaStreamPlayed()) pauseMediaStream();
    }
    JustChanging = false;
}

void TPlayVideoForm::TrackBarChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (JustChanging) return;
    setMediaStreamPos(Round(ui.TrackBar->value() * getMediaStreamDuration() / 100.0));
}

void TPlayVideoForm::CenterWindow()
{
    QWidget* P = parentWidget();
    if (P == nullptr) return;

    if (P->isWindow()) {
        // Owner is TForm
        const QRect R = P->frameGeometry();
        move((R.left() + R.width() / 2) - frameGeometry().width() / 2, (R.top() + R.height() / 2) - frameGeometry().height() / 2);
    } else {
        // Owner is TFrame
        const QRect R = P->geometry();
        move((R.left() + R.width() / 2) - frameGeometry().width() / 2, (R.top() + R.height() / 2) - frameGeometry().height() / 2);
    }
}

void TPlayVideoForm::ButtonWork(QObject* Sender)
{
    QString S;
    double F;

    switch (Sender->property("tag").toInt()) {
        case 0:
            if (PrevVideos->Count() > 0) {
                freeMediaStream();
                NextVideos->Insert(0, FileName);
                FileName = (*PrevVideos)[PrevVideos->Count() - 1];
                PrevVideos->Delete(PrevVideos->Count() - 1);
                FormShow(Sender);
            }
            break;
        case 1:
            if (NextVideos->Count() > 0) {
                freeMediaStream();
                PrevVideos->Add(FileName);
                FileName = (*NextVideos)[0];
                NextVideos->Delete(0);
                FormShow(Sender);
            }
            break;
        case 2:
            S = ExtractFileExt(FileName);
            if ((S != "") && (S[0] == '.')) S = Copy(S, 2, MaxInt);
            SaveDialog->DefaultExt = S;
            SaveDialog->Title = LanguageSetup->VideoCaptureSaveTitle();
            SaveDialog->Filter = LanguageSetup->VideoCaptureSaveFilter();
            if (!SaveDialog->Execute()) return;
            S = SaveDialog->FileName;
            if (!CopyFile(FileName, S, true)) MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {FileName, S}), mtError, mbOK, 0);
            break;
        case 3:
            if (VideoAvailable()) {
                if (MediaStreamPlayed() && (getMediaStreamPos() != getMediaStreamDuration())) pauseMediaStream(); else {
                    if (getMediaStreamPos() == getMediaStreamDuration()) setMediaStreamPos(0);
                    playMediaStream();
                }
            }
            break;
        case 4:
            if (VideoAvailable()) {
                if (isMaximized()) showNormal();
                auto* A = qobject_cast<QAction*>(Sender);
                if (A == nullptr) return;
                S = RemoveUnderline(A->text());
                F = StrToInt(Copy(S, 1, Length(S) - 1)) / 100.0;
                resize(Round(getVideoWidth() * F) + (width() - ui.MediaPanel->width() + 10), height());
                resize(width(), Round(getCoupledVideoHeight(getVideoWidth()) * F) + (height() - ui.MediaPanel->height() + 10));
                CenterWindow();
            }
            break;
    }
}

void TPlayVideoForm::InstallButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    if ((!FileExists(PrgDir() + "InstallVideoCodec.exe")) && (!FileExists(PrgDir() + BinFolder + PathDelim + "InstallVideoCodec.exe"))) {
        MessageDlg(Format(LanguageSetup->MessageFileNotFound(), {PrgDir() + BinFolder + PathDelim + "InstallVideoCodec.exe"}), mtError, mbOK, 0);
        return;
    }

    if (FileExists(PrgDir() + "InstallVideoCodec.exe")) {
        ShellOpen(PrgDir() + "InstallVideoCodec.exe", ExcludeTrailingPathDelimiter(PrgSetup->DOSBoxSettings(0)->DosBoxDir()), PrgDir());
    } else {
        ShellOpen(PrgDir() + BinFolder + PathDelim + "InstallVideoCodec.exe", ExcludeTrailingPathDelimiter(PrgSetup->DOSBoxSettings(0)->DosBoxDir()), PrgDir());
    }

    Close();
}

// global

void PlayVideoDialog(QWidget* AOwner, const QString& AFileName, TStringList* APrevVideos, TStringList* ANextVideos)
{
    if (OpenMediaFile(PrgSetup->VideoPlayer(), AFileName)) return;

    PlayVideoForm = new TPlayVideoForm(AOwner);
    PlayVideoForm->FileName = AFileName;
    if (APrevVideos != nullptr) PlayVideoForm->PrevVideos->Assign(*APrevVideos);
    if (ANextVideos != nullptr) PlayVideoForm->NextVideos->Assign(*ANextVideos);
    if ((APrevVideos == nullptr) && (ANextVideos == nullptr)) {
        PlayVideoForm->ui.PreviousButton->setVisible(false);
        PlayVideoForm->ui.NextButton->setVisible(false);
        // ToolButton1 is the separator following NextButton
        const QList<QAction*> Acts = PlayVideoForm->ui.ToolBar->actions();
        const int Idx = Acts.indexOf(PlayVideoForm->ui.NextButton);
        if (Idx >= 0 && Idx + 1 < Acts.size() && Acts[Idx + 1]->isSeparator()) Acts[Idx + 1]->setVisible(false);
        PlayVideoForm->ui.ToolButton1->setVisible(false);
    }
    PlayVideoForm->ShowModal();
    delete PlayVideoForm;
    PlayVideoForm = nullptr;
}
