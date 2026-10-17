// C++ port of ProfileMountEditorCDDriveFrameUnit.pas (implementation section).
#include "ProfileMountEditorCDDriveFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"

TProfileMountEditorCDDriveFrame::TProfileMountEditorCDDriveFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM event wiring
    connect(ui.CDROMButton, &QToolButton::clicked, this, [this]() { CDROMButtonClick(ui.CDROMButton); });
    connect(ui.CDROMDriveLetterComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { CDROMDriveLetterComboBoxChange(ui.CDROMDriveLetterComboBox); });
    connect(ui.CDMountTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { CDMountTypeComboBoxChange(ui.CDMountTypeComboBox); });
}

TProfileMountEditorCDDriveFrame::~TProfileMountEditorCDDriveFrame()
{
}

// TProfileMountEditorCDDriveFrame

bool TProfileMountEditorCDDriveFrame::Init(const TInfoData& AInfoData)
{
    TStringList* St;
    QString S, T;
    int I;
    bool OK;
    bool result;

    InfoData = AInfoData;

    ui.CDMountTypeLabel->setText(LanguageSetup->ProfileMountingCDDriveType());
    ui.CDMountTypeComboBox->blockSignals(true);
    ui.CDMountTypeComboBox->clear();
    ui.CDMountTypeComboBox->addItem(LanguageSetup->ProfileMountingCDDriveTypeFixedPath());
    ui.CDMountTypeComboBox->addItem(LanguageSetup->ProfileMountingCDDriveTypeNumber());
    ui.CDMountTypeComboBox->addItem(LanguageSetup->ProfileMountingCDDriveTypeAsk());
    ui.CDMountTypeComboBox->addItem(LanguageSetup->ProfileMountingCDDriveTypeLabel());
    ui.CDMountTypeComboBox->addItem(LanguageSetup->ProfileMountingCDDriveTypeFile());
    ui.CDMountTypeComboBox->addItem(LanguageSetup->ProfileMountingCDDriveTypeFolder());
    ui.CDMountTypeComboBox->setCurrentIndex(0);
    ui.CDMountTypeComboBox->blockSignals(false);
    ui.CDDriveNumberLabel->setText(LanguageSetup->ProfileMountingCDDriveTypeNumberLabel());
    ui.CDROMEdit_EditLabel->setText(LanguageSetup->ProfileMountingFolder());
    ui.CDROMLabelEdit_EditLabel->setText(LanguageSetup->ProfileMountingLabel());

    ui.CDROMDriveAccessLabel->setText(LanguageSetup->ProfileMountingCDROMAccess());
    while (ui.CDROMDriveAccessComboBox->count() < 6) ui.CDROMDriveAccessComboBox->addItem("");
    ui.CDROMDriveAccessComboBox->setItemText(0, LanguageSetup->ProfileMountingCDROMAccessNormal());
    ui.CDROMDriveAccessComboBox->setItemText(1, LanguageSetup->ProfileMountingCDROMAccessIOCTL());
    ui.CDROMDriveAccessComboBox->setItemText(2, LanguageSetup->ProfileMountingCDROMAccessIOCTL_DX());
    ui.CDROMDriveAccessComboBox->setItemText(3, LanguageSetup->ProfileMountingCDROMAccessIOCTL_DIO());
    ui.CDROMDriveAccessComboBox->setItemText(4, LanguageSetup->ProfileMountingCDROMAccessIOCTL_MCI());
    ui.CDROMDriveAccessComboBox->setItemText(5, LanguageSetup->ProfileMountingCDROMAccessNOIOCTL());
    ui.CDROMDriveAccessComboBox->setCurrentIndex(0);

    UserIconLoader->DialogImage(DI_SelectFolder, ui.CDROMButton);

    ui.CDROMDriveLetterLabel->setText(LanguageSetup->ProfileMountingLetter());
    ui.CDROMDriveLetterWarningLabel->setText(LanguageSetup->ProfileMountingDriveLetterAlreadyInUse());
    ui.CDROMButton->setToolTip(LanguageSetup->ChooseFolder());

    for (char C = 'A'; C <= 'Y'; C++) ui.CDROMDriveLetterComboBox->addItem(QString(QChar(C)));
    ui.CDROMDriveLetterComboBox->setCurrentIndex(3);

    St = ValueToList(InfoData.Data);
    S = Trim(ExtUpperCase((*St)[1]));
    if (S == "CDROM") {
        result = true;
        // RealFolder;CDROM;Letter;IO;Label; (ASK /  NUMBER:x /  LABEL:x / FILE:x / FOLDER:x)

        OK = false;
        S = Trim(ExtUpperCase((*St)[0]));

        if (S == "ASK") {
            ui.CDMountTypeComboBox->setCurrentIndex(2);
            OK = true;
        }

        if (Pos(':', S) != 0) {
            S = Trim(Copy(S, 1, Pos(':', S) - 1));
            T = (*St)[0]; T = Trim(Copy(T, Pos(':', T) + 1, MaxInt));

            if (S == "NUMBER") {
                if (TryStrToInt(T, I)) {
                    ui.CDDriveNumberEdit->setValue(Max(1, Min(10, I)));
                    ui.CDMountTypeComboBox->setCurrentIndex(1);
                    OK = true;
                }
            }
            if (S == "LABEL") {
                ui.CDLabelEdit->setText(T);
                ui.CDMountTypeComboBox->setCurrentIndex(3);
                OK = true;
            }
            if (S == "FILE") {
                ui.CDLabelEdit->setText(T);
                ui.CDMountTypeComboBox->setCurrentIndex(4);
                OK = true;
            }
            if (S == "FOLDER") {
                ui.CDLabelEdit->setText(T);
                ui.CDMountTypeComboBox->setCurrentIndex(5);
                OK = true;
            }
        }
        if (!OK) {
            ui.CDMountTypeComboBox->setCurrentIndex(0);
            ui.CDROMEdit->setText((*St)[0]);
        }
        CDMountTypeComboBoxChange(this);

        if (St->Count() >= 3) {
            I = ui.CDROMDriveLetterComboBox->findText(Trim(UpperCase((*St)[2])), Qt::MatchFixedString); if (I < 0) I = 2;
            ui.CDROMDriveLetterComboBox->setCurrentIndex(I);
        }
        if (St->Count() >= 4) {
            S = Trim(ExtUpperCase((*St)[3]));
            ui.CDROMDriveAccessComboBox->setCurrentIndex(0);
            if (S == "TRUE") ui.CDROMDriveAccessComboBox->setCurrentIndex(1);
            if (S == "DX") ui.CDROMDriveAccessComboBox->setCurrentIndex(2);
            if (S == "DIO") ui.CDROMDriveAccessComboBox->setCurrentIndex(3);
            if (S == "MCI") ui.CDROMDriveAccessComboBox->setCurrentIndex(4);
            if (S == "NOIOCTL") ui.CDROMDriveAccessComboBox->setCurrentIndex(5);
        }
        if (St->Count() >= 5) {
            ui.CDROMLabelEdit->setText((*St)[4]);
        }
    } else {
        result = false;
        for (I = 2; I <= ui.CDROMDriveLetterComboBox->count() - 1; I++) {
            if (Pos(ui.CDROMDriveLetterComboBox->itemText(I), InfoData.UsedDriveLetters) == 0) { ui.CDROMDriveLetterComboBox->setCurrentIndex(I); break; }
        }
    }
    delete St;

    CDROMDriveLetterComboBoxChange(this);
    return result;
}

void TProfileMountEditorCDDriveFrame::ShowFrame()
{
    CDMountTypeComboBoxChange(this);
}

bool TProfileMountEditorCDDriveFrame::CheckBeforeDone()
{
    return true;
}

QString TProfileMountEditorCDDriveFrame::Done()
{
    QString S, T;

    // RealFolder;CDROM;Letter;IO;Label; (ASK /  NUMBER:x /  LABEL:x / FILE:x / FOLDER:x)
    switch (ui.CDROMDriveAccessComboBox->currentIndex()) {
        case 1: S = "true"; break;
        case 2: S = "dx"; break;
        case 3: S = "dio"; break;
        case 4: S = "mci"; break;
        case 5: S = "NoIOCTL"; break;
        default: S = "false"; break;
    }
    switch (ui.CDMountTypeComboBox->currentIndex()) {
        case 0: T = MakeRelPath(ui.CDROMEdit->text(), PrgSetup->BaseDir()); break;
        case 1: T = "NUMBER:" + IntToStr(ui.CDDriveNumberEdit->value()); break;
        case 2: T = "ASK"; break;
        case 3: T = "LABEL:" + ui.CDLabelEdit->text(); break;
        case 4: T = "FILE:" + ui.CDLabelEdit->text(); break;
        case 5: T = "FOLDER:" + ui.CDLabelEdit->text(); break;
        default: T = ui.CDROMEdit->text(); break;
    }
    return StringReplace(T, ";", "<semicolon>", rfReplaceAll) + ";CDROM;" + ui.CDROMDriveLetterComboBox->currentText() + ";" + S + ";" + ui.CDROMLabelEdit->text() + ";";
}

QString TProfileMountEditorCDDriveFrame::GetName()
{
    return LanguageSetup->ProfileMountingCDROMSheet();
}

void TProfileMountEditorCDDriveFrame::CDMountTypeComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    int Idx = ui.CDMountTypeComboBox->currentIndex();

    // To avoid problems when frame gets visible (TLabeledEdit: the label follows the edit's visibility)
    ui.CDROMEdit->setVisible(Idx == 0);
    ui.CDROMEdit_EditLabel->setVisible(Idx == 0);
    ui.CDROMButton->setVisible(Idx == 0);
    ui.CDDriveNumberEdit->setVisible(Idx == 1);
    ui.CDDriveNumberLabel->setVisible(Idx == 1);
    ui.CDLabelEdit->setVisible(Idx >= 3);
    ui.CDLabelEdit_EditLabel->setVisible(Idx >= 3);

    switch (Idx) {
        case 3: ui.CDLabelEdit_EditLabel->setText(LanguageSetup->ProfileMountingCDDriveTypeLabelLabel()); break;
        case 4: ui.CDLabelEdit_EditLabel->setText(LanguageSetup->ProfileMountingCDDriveTypeFileLabel()); break;
        case 5: ui.CDLabelEdit_EditLabel->setText(LanguageSetup->ProfileMountingCDDriveTypeFolderLabel()); break;
    }
}

void TProfileMountEditorCDDriveFrame::CDROMButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (Trim(ui.CDROMEdit->text()) == "") S = InfoData.DefaultInitialDir; else S = ui.CDROMEdit->text();
    S = MakeAbsPath(S, PrgSetup->BaseDir());
    if (!SelectDirectory(THandle(0), LanguageSetup->ProfileMountingFolder(), S)) return;
    ui.CDROMEdit->setText(MakeRelPath(S, PrgSetup->BaseDir(), true));
}

void TProfileMountEditorCDDriveFrame::CDROMDriveLetterComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.CDROMDriveLetterWarningLabel->setVisible((ui.CDROMDriveLetterComboBox->currentIndex() >= 0) && (Pos(ui.CDROMDriveLetterComboBox->itemText(ui.CDROMDriveLetterComboBox->currentIndex()), InfoData.UsedDriveLetters) > 0));
}
