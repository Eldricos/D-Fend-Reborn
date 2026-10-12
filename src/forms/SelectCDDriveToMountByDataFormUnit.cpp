// C++ port of SelectCDDriveToMountByDataFormUnit.pas (implementation section).
#include "SelectCDDriveToMountByDataFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "SelectCDDriveToMountFormUnit.h"
#include "IconLoaderUnit.h"
#include <QStorageInfo>
#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

TSelectCDDriveToMountByDataForm* SelectCDDriveToMountByDataForm = nullptr;

TSelectCDDriveToMountByDataForm::TSelectCDDriveToMountByDataForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DFM: object Timer: TTimer  Enabled = False  Interval = 200
    Timer = new QTimer(this);
    Timer->setInterval(200);
    connect(Timer, &QTimer::timeout, this, [this]() { TimerTimer(Timer); });
}

TSelectCDDriveToMountByDataForm::~TSelectCDDriveToMountByDataForm() = default;

void TSelectCDDriveToMountByDataForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

void TSelectCDDriveToMountByDataForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    setWindowTitle(LanguageSetup->SelectCDDriveCaption());
    ui.StartLabel1->setText(LanguageSetup->SelectCDDriveProfileToStart());
    ui.StartLabel2->setText(ProfileName);
    QFont F = ui.StartLabel2->font(); F.setBold(true); ui.StartLabel2->setFont(F);
    switch (MountByDataType) {
        case mbLabel: ui.InfoLabel->setText(Format(LanguageSetup->SelectCDDriveLabel(), {Data, LetterToMountAt})); break;
        case mbFile: ui.InfoLabel->setText(Format(LanguageSetup->SelectCDDriveFile(), {Data, LetterToMountAt})); break;
        case mbFolder: ui.InfoLabel->setText(Format(LanguageSetup->SelectCDDriveFolder(), {Data, LetterToMountAt})); break;
    }

    ui.CancelButton->setText(LanguageSetup->Cancel());

    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);

    Timer->start();
}

void TSelectCDDriveToMountByDataForm::TimerTimer(QObject* Sender)
{
    Q_UNUSED(Sender);
    Timer->stop();

    const QString S = CheckDriveData(MountByDataType, Data);
    if (!S.isEmpty()) {
        ChosenDrive = S;
        setModalResult(mrOk);
        return;
    }

    Timer->start();
}

// { global }

bool ShowSelectCDDriveToMountByDataDialog(QWidget* AOwner, TMountByData MountByDataType, const QString& Data, const QString& LetterToMountAt, const QString& ProfileName, QString& ChosenDrive)
{
    const QString S = CheckDriveData(MountByDataType, Data);
    bool result = !S.isEmpty();
    if (result) { ChosenDrive = S; return result; }

    TSelectCDDriveToMountByDataForm* LocalSelectCDDriveToMountByDataForm = new TSelectCDDriveToMountByDataForm(AOwner);
    LocalSelectCDDriveToMountByDataForm->ProfileName = ProfileName;
    LocalSelectCDDriveToMountByDataForm->MountByDataType = MountByDataType;
    LocalSelectCDDriveToMountByDataForm->Data = Data;
    LocalSelectCDDriveToMountByDataForm->LetterToMountAt = LetterToMountAt;

    // ForceForegroundWindow(Application.Handle); Application.BringToFront;
    QWidget* Top = (AOwner != nullptr) ? AOwner->window() : QApplication::activeWindow();
    if (Top != nullptr) { ForceForegroundWindow(Top->winId()); Top->raise(); Top->activateWindow(); }

    result = (LocalSelectCDDriveToMountByDataForm->ShowModal() == mrOk);
    if (result) ChosenDrive = LocalSelectCDDriveToMountByDataForm->ChosenDrive;
    delete LocalSelectCDDriveToMountByDataForm;
    return result;
}

QString CheckDriveData(TMountByData MountByDataType, const QString& Data)
{
    QString result;
    QString S;

#ifdef Q_OS_WIN
    for (char C = 'A'; C <= 'Z'; C++) {
        const QString Root = QString(QChar(C)) + ":\\";
        if (GetDriveTypeW(reinterpret_cast<LPCWSTR>(Root.utf16())) != DRIVE_CDROM) continue;
        const QString Drive = QString(QChar(C));
#else
    // TODO(port): there are no drive letters outside Windows.  Check all mounted optical
    // media (ISO 9660 / UDF file systems) instead; the result is the mount point.
    const QList<QStorageInfo> Volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo& V : Volumes) {
        const QString FS = QString::fromUtf8(V.fileSystemType()).toLower();
        if (FS != "iso9660" && FS != "udf" && FS != "cd9660") continue;
        if (!V.isValid() || !V.isReady()) continue;
        const QString Drive = V.rootPath();
        const QString Root = IncludeTrailingPathDelimiter(Drive);
#endif
        if (!GetDriveLabel(Drive, S)) continue;
        bool Found = false;
        switch (MountByDataType) {
            case mbLabel: Found = (Trim(ExtUpperCase(S)) == Trim(ExtUpperCase(Data))); break;
            case mbFile: Found = FileExists(Root + Data); break;
            case mbFolder: Found = DirectoryExists(Root + Data); break;
        }
        if (Found) { result = Drive; break; }
    }

    return result;
}
