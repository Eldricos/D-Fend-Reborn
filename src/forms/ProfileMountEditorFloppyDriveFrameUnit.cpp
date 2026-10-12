// C++ port of ProfileMountEditorFloppyDriveFrameUnit.pas (implementation section).
#include "ProfileMountEditorFloppyDriveFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"

TProfileMountEditorFloppyDriveFrame::TProfileMountEditorFloppyDriveFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM event wiring
    connect(ui.FloppyButton, &QToolButton::clicked, this, [this]() { FloppyButtonClick(ui.FloppyButton); });
    connect(ui.FloppyDriveLetterComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { FloppyDriveLetterComboBoxChange(ui.FloppyDriveLetterComboBox); });
}

TProfileMountEditorFloppyDriveFrame::~TProfileMountEditorFloppyDriveFrame()
{
}

// TProfileMountEditorFloppyDriveFrame

bool TProfileMountEditorFloppyDriveFrame::Init(const TInfoData& AInfoData)
{
    TStringList* St;
    QString S;
    int I;
    bool result;

    InfoData = AInfoData;

    ui.FloppyEdit_EditLabel->setText(LanguageSetup->ProfileMountingFolder());
    ui.FloppyDriveLetterLabel->setText(LanguageSetup->ProfileMountingLetter());
    ui.FloppyDriveLetterWarningLabel->setText(LanguageSetup->ProfileMountingDriveLetterAlreadyInUse());
    ui.FloppyButton->setToolTip(LanguageSetup->ChooseFolder());

    for (char C = 'A'; C <= 'Y'; C++) ui.FloppyDriveLetterComboBox->addItem(QString(QChar(C)));
    ui.FloppyDriveLetterComboBox->setCurrentIndex(0);

    UserIconLoader->DialogImage(DI_SelectFolder, ui.FloppyButton);

    St = ValueToList(InfoData.Data);
    S = Trim(ExtUpperCase((*St)[1]));
    if (S == "FLOPPY") {
        result = true;
        // RealFolder;FLOPPY;Letter;False;;
        ui.FloppyEdit->setText((*St)[0]);
        if (St->Count() >= 3) {
            I = ui.FloppyDriveLetterComboBox->findText(Trim(UpperCase((*St)[2])), Qt::MatchFixedString); if (I < 0) I = 2;
            ui.FloppyDriveLetterComboBox->setCurrentIndex(I);
        }
    } else {
        result = false;
        for (I = 0; I <= ui.FloppyDriveLetterComboBox->count() - 1; I++) {
            if (Pos(ui.FloppyDriveLetterComboBox->itemText(I), InfoData.UsedDriveLetters) == 0) { ui.FloppyDriveLetterComboBox->setCurrentIndex(I); break; }
        }
    }
    delete St;

    FloppyDriveLetterComboBoxChange(this);
    return result;
}

void TProfileMountEditorFloppyDriveFrame::ShowFrame()
{
}

bool TProfileMountEditorFloppyDriveFrame::CheckBeforeDone()
{
    return true;
}

QString TProfileMountEditorFloppyDriveFrame::Done()
{
    // RealFolder;FLOPPY;Letter;False;;
    return StringReplace(MakeRelPath(ui.FloppyEdit->text(), PrgSetup->BaseDir()), ";", "<semicolon>", rfReplaceAll) + ";Floppy;" + ui.FloppyDriveLetterComboBox->currentText() + ";False;;";
}

QString TProfileMountEditorFloppyDriveFrame::GetName()
{
    return LanguageSetup->ProfileMountingFloppySheet();
}

void TProfileMountEditorFloppyDriveFrame::FloppyButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (Trim(ui.FloppyEdit->text()) == "") S = InfoData.DefaultInitialDir; else S = ui.FloppyEdit->text();
    S = MakeAbsPath(S, PrgSetup->BaseDir());
    if (!SelectDirectory(THandle(0), LanguageSetup->ProfileMountingFolder(), S)) return;
    ui.FloppyEdit->setText(MakeRelPath(S, PrgSetup->BaseDir(), true));
}

void TProfileMountEditorFloppyDriveFrame::FloppyDriveLetterComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.FloppyDriveLetterWarningLabel->setVisible((ui.FloppyDriveLetterComboBox->currentIndex() >= 0) && (Pos(ui.FloppyDriveLetterComboBox->itemText(ui.FloppyDriveLetterComboBox->currentIndex()), InfoData.UsedDriveLetters) > 0));
}
