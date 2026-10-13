// C++ port of SelectCDDriveToMountFormUnit.pas (implementation section).
#include "SelectCDDriveToMountFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include <QStorageInfo>
#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

TSelectCDDriveToMountForm* SelectCDDriveToMountForm = nullptr;

TSelectCDDriveToMountForm::TSelectCDDriveToMountForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
}

TSelectCDDriveToMountForm::~TSelectCDDriveToMountForm() = default;

void TSelectCDDriveToMountForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

void TSelectCDDriveToMountForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    setWindowTitle(LanguageSetup->SelectCDDriveCaption());
    ui.StartLabel1->setText(LanguageSetup->SelectCDDriveProfileToStart());
    ui.StartLabel2->setText(ProfileName);
    QFont F = ui.StartLabel2->font(); F.setBold(true); ui.StartLabel2->setFont(F);
    ui.InfoLabel->setText(Format(LanguageSetup->SelectCDDriveFreeSelect(), {LetterToMountAt}));

    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);

    ui.DriveComboBox->clear();
    for (const QString& D : AvailableDrives) {
        if (GetDriveLabel(D, S) && !S.isEmpty()) {
            ui.DriveComboBox->addItem(D + " (" + S + ")", D);
        } else {
            ui.DriveComboBox->addItem(D, D);
        }
    }
    ui.DriveComboBox->setCurrentIndex(0);
}

void TSelectCDDriveToMountForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ChosenDrive = ui.DriveComboBox->itemData(ui.DriveComboBox->currentIndex()).toString();
}

// { global }

bool ShowSelectCDDriveToMountDialog(QWidget* AOwner, const QStringList& AvailableDrives, const QString& LetterToMountAt, const QString& ProfileName, QString& ChosenDrive)
{
    TSelectCDDriveToMountForm* LocalSelectCDDriveToMountForm = new TSelectCDDriveToMountForm(AOwner);
    LocalSelectCDDriveToMountForm->ProfileName = ProfileName;
    LocalSelectCDDriveToMountForm->AvailableDrives = AvailableDrives;
    LocalSelectCDDriveToMountForm->LetterToMountAt = LetterToMountAt;

    // ForceForegroundWindow(Application.Handle); Application.BringToFront;
    QWidget* Top = (AOwner != nullptr) ? AOwner->window() : QApplication::activeWindow();
    if (Top != nullptr) { ForceForegroundWindow(Top->winId()); Top->raise(); Top->activateWindow(); }

    const bool result = (LocalSelectCDDriveToMountForm->ShowModal() == mrOk);
    if (result) ChosenDrive = LocalSelectCDDriveToMountForm->ChosenDrive;
    delete LocalSelectCDDriveToMountForm;
    return result;
}

bool GetDriveLabel(const QString& DriveLetter, QString& DriveLabel)
{
    bool result;
#ifdef Q_OS_WIN
    wchar_t S1[512], S2[512];
    DWORD C1 = 0, C2 = 0, C3 = 0;
    const QString Root = Copy(DriveLetter, 1, 1) + ":\\";
    S1[0] = 0; S2[0] = 0;
    result = GetVolumeInformationW(reinterpret_cast<LPCWSTR>(Root.utf16()), S1, 510, &C1, &C2, &C3, S2, 510) != 0;
    if (result) DriveLabel = QString::fromWCharArray(S1);
#else
    // Port note: no drive letters outside Windows; DriveLetter is the mount point of
    // the medium (see PhysicalDrivesUnit) - use the volume label of that file system.
    QStorageInfo Info(DriveLetter);
    result = (Length(DriveLetter) > 1) && Info.isValid() && Info.isReady();
    if (result) DriveLabel = Info.name();
#endif
    return result;
}
