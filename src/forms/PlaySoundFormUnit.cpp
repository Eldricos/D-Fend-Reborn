// C++ port of PlaySoundFormUnit.pas (implementation section).
//
// The original used the external MediaInterface DLL (DirectShow); the port
// plays the files with Qt Multimedia (QMediaPlayer + QAudioOutput).  The
// MediaInterface routines used by the Delphi code are provided as private
// helpers with the same names (positions in milliseconds instead of 100ns).
#include "PlaySoundFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "SetupFrameWaveEncoderUnit.h"
#include "IconLoaderUnit.h"
#include "Process.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QEventLoop>
#include <QUrl>

TPlaySoundForm* PlaySoundForm = nullptr;

TPlaySoundForm::TPlaySoundForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    ImageList = TImageList::FromResource("PlaySoundFormUnit", "ImageList");
    SaveDialog = new TSaveDialog(this);
    SaveDialog->DefaultExt = "wav";
    Timer = new QTimer(this);
    Timer->setInterval(50);

    FPlayer = new QMediaPlayer(this);
    FAudioOutput = new QAudioOutput(this);
    FPlayer->setAudioOutput(FAudioOutput);

    connect(ui.TrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.TrackBar); });
    connect(ui.PreviousButton, &QAction::triggered, this, [this]() { ButtonWork(ui.PreviousButton); });
    connect(ui.NextButton, &QAction::triggered, this, [this]() { ButtonWork(ui.NextButton); });
    connect(ui.SaveButton, &QAction::triggered, this, [this]() { ButtonWork(ui.SaveButton); });
    connect(ui.PlayPauseButton, &QAction::triggered, this, [this]() { ButtonWork(ui.PlayPauseButton); });
    connect(ui.MenuSave, &QAction::triggered, this, [this]() { ButtonWork(ui.MenuSave); });
    connect(ui.MenuSaveMp3, &QAction::triggered, this, [this]() { ButtonWork(ui.MenuSaveMp3); });
    connect(ui.MenuSaveOgg, &QAction::triggered, this, [this]() { ButtonWork(ui.MenuSaveOgg); });
    connect(Timer, &QTimer::timeout, this, [this]() { TimerTimer(Timer); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);
}

TPlaySoundForm::~TPlaySoundForm()
{
    FormDestroy(this);
    delete ImageList;
    delete SaveDialog;
}

void TPlaySoundForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);
}

void TPlaySoundForm::wheelEvent(QWheelEvent *E)
{
    bool Handled = false;
    const int Delta = E->angleDelta().y();
    if (Delta == 0) { TVclForm::wheelEvent(E); return; }
    FormMouseWheel(this, E->modifiers(), Delta, E->position().toPoint(), Handled);
    E->accept();
}

// --- MediaInterface replacement --------------------------------------------

void TPlaySoundForm::loadMediaFile(const QString& AFileName)
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
}

bool TPlaySoundForm::MediaStreamAvailable() const
{
    if (FPlayer->source().isEmpty() || FPlayer->error() != QMediaPlayer::NoError) return false;
    switch (FPlayer->mediaStatus()) {
        case QMediaPlayer::LoadedMedia: case QMediaPlayer::BufferingMedia: case QMediaPlayer::BufferedMedia:
        case QMediaPlayer::EndOfMedia: case QMediaPlayer::StalledMedia: return true;
        default: return false;
    }
}

bool TPlaySoundForm::MediaStreamPlayed() const { return FPlayer->playbackState() == QMediaPlayer::PlayingState; }
void TPlaySoundForm::playMediaStream() { FPlayer->play(); }
void TPlaySoundForm::pauseMediaStream() { FPlayer->pause(); }
void TPlaySoundForm::stopMediaStream() { FPlayer->stop(); }
void TPlaySoundForm::resetMediaStream() { FPlayer->setPosition(0); }
void TPlaySoundForm::freeMediaStream() { if (FPlayer != nullptr) { FPlayer->stop(); FPlayer->setSource(QUrl()); } }

qint64 TPlaySoundForm::getMediaStreamPos() const
{
    // QMediaPlayer stops at the end of the stream and may reset the position
    if (FPlayer->mediaStatus() == QMediaPlayer::EndOfMedia) return FPlayer->duration();
    return FPlayer->position();
}

qint64 TPlaySoundForm::getMediaStreamDuration() const { return FPlayer->duration(); }
void TPlaySoundForm::setMediaStreamPos(qint64 Pos) { FPlayer->setPosition(Pos); }

// ----------------------------------------------------------------------------

void TPlaySoundForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts / Font.Charset: not needed in Qt

    ui.PreviousButton->setText(LanguageSetup->Previous());
    ui.PreviousButton->setToolTip(LanguageSetup->PreviousHintMediaViewer());
    ui.NextButton->setText(LanguageSetup->Next());
    ui.NextButton->setToolTip(LanguageSetup->NextHintMediaViewer());
    ui.SaveButton->setText(LanguageSetup->Save());
    ui.SaveButton->setToolTip(LanguageSetup->SaveHintSoundPlayer());
    ui.PlayPauseButton->setText(LanguageSetup->SoundCapturePlayPause());
    ui.PlayPauseButton->setToolTip(LanguageSetup->SoundCapturePlayPauseHint());
    ui.MenuSave->setText(LanguageSetup->ScreenshotPopupSave());
    ui.MenuSaveMp3->setText(LanguageSetup->SoundsPopupSaveMp3());
    ui.MenuSaveOgg->setText(LanguageSetup->SoundsPopupSaveOgg());

    UserIconLoader->DialogImage(DI_Save, ImageList, 0);
    UserIconLoader->DialogImage(DI_Previous, ImageList, 1);
    UserIconLoader->DialogImage(DI_Next, ImageList, 2);
    UserIconLoader->DialogImage(DI_Run, ImageList, 3);
    // ToolBar.Images = ImageList
    ui.SaveButton->setIcon(ImageList->Icon(0));
    ui.PreviousButton->setIcon(ImageList->Icon(1));
    ui.NextButton->setIcon(ImageList->Icon(2));
    ui.PlayPauseButton->setIcon(ImageList->Icon(3));

    {
        QFont F = ui.CoolBar->font();
        if (PrgSetup->ToolbarFontSize() > 0) F.setPointSize(PrgSetup->ToolbarFontSize());
        ui.CoolBar->setFont(F);
        ui.ToolBar->setFont(F);
    }

    JustChanging = false;

    PrevSounds = new TStringList();
    NextSounds = new TStringList();
}

void TPlaySoundForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    Timer->stop();

    if (MediaStreamAvailable()) {
        stopMediaStream();
        resetMediaStream();
    }

    ui.PreviousButton->setEnabled(PrevSounds->Count() > 0);
    ui.NextButton->setEnabled(NextSounds->Count() > 0);

    setWindowTitle(LanguageSetup->CaptureSounds() + " [" + MakeRelPath(FileName, PrgSetup->BaseDir()) + "]");
    FileDamaged = false;

    VclLayout::setStatusText(ui.StatusBar, 1, FileName);
    loadMediaFile(FileName);
    FileDamaged = !MediaStreamAvailable();

    ui.SaveButton->setEnabled(!FileDamaged);
    ui.PlayPauseButton->setEnabled(!FileDamaged);

    S = ExtUpperCase(ExtractFileExt(FileName));
    ui.MenuSaveMp3->setEnabled((S == ".WAV") && FileExists(PrgSetup->WaveEncMp3()));
    ui.MenuSaveOgg->setEnabled((S == ".WAV") && FileExists(PrgSetup->WaveEncOgg()));

    if (MediaStreamAvailable()) {
        JustChanging = true;
        ui.TrackBar->setValue(0);
        JustChanging = false;
        ButtonWork(ui.PlayPauseButton);
    }

    Timer->start();
}

void TPlaySoundForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    freeMediaStream();
    delete PrevSounds; PrevSounds = nullptr;
    delete NextSounds; NextSounds = nullptr;
}

void TPlaySoundForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    Shift &= ~Qt::KeypadModifier;
    if (Shift == Qt::NoModifier) switch (Key) {
        case Qt::Key_Left: case Qt::Key_Up: case Qt::Key_PageUp: ButtonWork(ui.PreviousButton); break;
        case Qt::Key_Right: case Qt::Key_Down: case Qt::Key_PageDown: ButtonWork(ui.NextButton); break;
        case Qt::Key_Escape: Close(); break;
    }
}

void TPlaySoundForm::FormMouseWheel(QObject* Sender, TShiftState Shift, int WheelDelta, QPoint MousePos, bool& Handled)
{
    Q_UNUSED(Sender); Q_UNUSED(MousePos); Q_UNUSED(Handled);
    if (Shift == Qt::NoModifier) {
        if (WheelDelta > 0) ButtonWork(ui.PreviousButton); else ButtonWork(ui.NextButton);
    }
}

void TPlaySoundForm::TimerTimer(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (FileDamaged) return;
    JustChanging = true;
    try {
        if (MediaStreamAvailable() && (getMediaStreamDuration() > 0)) {
            ui.TrackBar->setValue(Round(100.0 * getMediaStreamPos() / getMediaStreamDuration()));
            VclLayout::setStatusText(ui.StatusBar, 0, QString::number(getMediaStreamPos() / 1000.0, 'f', 3) + "/" + QString::number(getMediaStreamDuration() / 1000.0, 'f', 3));
            if ((getMediaStreamPos() == getMediaStreamDuration()) && MediaStreamPlayed()) pauseMediaStream();
        }
    } catch (...) {}
    JustChanging = false;
}

void TPlaySoundForm::TrackBarChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (JustChanging || FileDamaged || (!MediaStreamAvailable())) return;
    setMediaStreamPos(Round(ui.TrackBar->value() * getMediaStreamDuration() / 100.0));
}

void TPlaySoundForm::ButtonWork(QObject* Sender)
{
    QPoint P;
    QString S;

    switch (Sender->property("tag").toInt()) {
        case 0:
            if (PrevSounds->Count() > 0) {
                Timer->stop();
                NextSounds->Insert(0, FileName);
                FileName = (*PrevSounds)[PrevSounds->Count() - 1];
                PrevSounds->Delete(PrevSounds->Count() - 1);
                FormShow(Sender);
            }
            break;
        case 1:
            if (NextSounds->Count() > 0) {
                Timer->stop();
                PrevSounds->Add(FileName);
                FileName = (*NextSounds)[0];
                NextSounds->Delete(0);
                FormShow(Sender);
            }
            break;
        case 2: {
            QWidget* B = ui.ToolBar->widgetForAction(ui.SaveButton);
            if (B != nullptr) P = B->mapToGlobal(QPoint(5, 5)); else P = ui.ToolBar->mapToGlobal(QPoint(5, 5));
            ui.PopupMenu->popup(P);
            break;
        }
        case 3:
            S = ExtractFileExt(FileName);
            if ((S != "") && (S[0] == '.')) S = Copy(S, 2, MaxInt);
            SaveDialog->DefaultExt = S;
            SaveDialog->Title = LanguageSetup->SoundCaptureSaveTitle();
            S = Trim(ExtUpperCase(S));
            SaveDialog->Filter = LanguageSetup->SoundCaptureSaveWAVFilter();
            if (S == "MP3") SaveDialog->Filter = LanguageSetup->SoundCaptureSaveMP3Filter();
            if (S == "OGG") SaveDialog->Filter = LanguageSetup->SoundCaptureSaveOGGFilter();
            if ((S == "MID") || (S == "MIDI")) SaveDialog->Filter = LanguageSetup->SoundCaptureSaveMIDFilter();
            if (!SaveDialog->Execute()) return;
            S = SaveDialog->FileName;
            if (!CopyFile(FileName, S, true)) MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {FileName, S}), mtError, mbOK, 0);
            break;
        case 4:
            SaveDialog->DefaultExt = "mp3";
            SaveDialog->Title = LanguageSetup->SoundCaptureSaveTitle();
            SaveDialog->Filter = LanguageSetup->SoundCaptureSaveMP3Filter();
            if (!SaveDialog->Execute()) return;
            S = SaveDialog->FileName;
            ShellOpen(PrgSetup->WaveEncMp3(), ProcessEncoderParameters(PrgSetup->WaveEncMp3Parameters(), FileName, S));
            break;
        case 5:
            SaveDialog->DefaultExt = "ogg";
            SaveDialog->Title = LanguageSetup->SoundCaptureSaveTitle();
            SaveDialog->Filter = LanguageSetup->SoundCaptureSaveOGGFilter();
            if (!SaveDialog->Execute()) return;
            S = SaveDialog->FileName;
            ShellOpen(PrgSetup->WaveEncOgg(), ProcessEncoderParameters(PrgSetup->WaveEncOggParameters(), FileName, S));
            break;
        case 6:
            if (MediaStreamAvailable()) {
                if (MediaStreamPlayed() && (getMediaStreamPos() != getMediaStreamDuration())) pauseMediaStream(); else {
                    if (getMediaStreamPos() == getMediaStreamDuration()) setMediaStreamPos(0);
                    playMediaStream();
                }
            }
            break;
    }
}

// global

void PlaySoundDialog(QWidget* AOwner, const QString& AFileName, TStringList* APrevSounds, TStringList* ANextSounds)
{
    QString S;
    if (OpenMediaFile(PrgSetup->SoundPlayer(), AFileName)) return;

    S = ExtUpperCase(ExtractFileExt(AFileName));
    if ((S != ".WAV") && (S != ".MP3") && (S != ".OGG") && (S != ".MID") && (S != ".MIDI")) return;
    PlaySoundForm = new TPlaySoundForm(AOwner);
    PlaySoundForm->FileName = AFileName;
    if (APrevSounds != nullptr) PlaySoundForm->PrevSounds->Assign(*APrevSounds);
    if (ANextSounds != nullptr) PlaySoundForm->NextSounds->Assign(*ANextSounds);
    if ((APrevSounds == nullptr) && (ANextSounds == nullptr)) {
        PlaySoundForm->ui.PreviousButton->setVisible(false);
        PlaySoundForm->ui.NextButton->setVisible(false);
        // ToolButton1 is the separator following NextButton
        const QList<QAction*> Acts = PlaySoundForm->ui.ToolBar->actions();
        const int Idx = Acts.indexOf(PlaySoundForm->ui.NextButton);
        if (Idx >= 0 && Idx + 1 < Acts.size() && Acts[Idx + 1]->isSeparator()) Acts[Idx + 1]->setVisible(false);
        PlaySoundForm->ui.ToolButton1->setVisible(false);
    }
    PlaySoundForm->ShowModal();
    delete PlaySoundForm;
    PlaySoundForm = nullptr;
}
