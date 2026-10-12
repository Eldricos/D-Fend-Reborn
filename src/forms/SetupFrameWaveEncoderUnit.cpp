// C++ port of SetupFrameWaveEncoderUnit.pas (implementation section).
#include "SetupFrameWaveEncoderUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"
#include "SetupDosBoxFormUnit.h"
#include "IconLoaderUnit.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameWaveEncoder::TSetupFrameWaveEncoder(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    PrgOpenDialog = new TOpenDialog(this);
    PrgOpenDialog->DefaultExt = "exe";
    // DFM event wiring
    connect(ui.WaveEncMp3Button1, &QToolButton::clicked, this, [this]() { ButtonWork(ui.WaveEncMp3Button1); });
    connect(ui.WaveEncMp3Button2, &QToolButton::clicked, this, [this]() { ButtonWork(ui.WaveEncMp3Button2); });
    connect(ui.WaveEncOggButton1, &QToolButton::clicked, this, [this]() { ButtonWork(ui.WaveEncOggButton1); });
    connect(ui.WaveEncOggButton2, &QToolButton::clicked, this, [this]() { ButtonWork(ui.WaveEncOggButton2); });
}

TSetupFrameWaveEncoder::~TSetupFrameWaveEncoder() = default;

// { global }

QString UpdateEncoderParameters(const QString &S)
{
    int I;
    QString result = S;
    if ((Pos("%1", result) > 0) && (Pos("%2", result) > 0)) return result;
    I = Pos("%s", result); if (I > 0) result = Copy(result, 1, I - 1) + "%1" + Copy(result, I + 2, MaxInt);
    I = Pos("%s", result); if (I > 0) result = Copy(result, 1, I - 1) + "%2" + Copy(result, I + 2, MaxInt);
    return result;
}

QString ProcessEncoderParameters(const QString &ParameterString, const QString &InFile, const QString &OutFile)
{
    int I;
    QString result = UpdateEncoderParameters(ParameterString);
    I = Pos("%1", result); if (I > 0) result = Copy(result, 1, I - 1) + InFile + Copy(result, I + 2, MaxInt);
    I = Pos("%2", result); if (I > 0) result = Copy(result, 1, I - 1) + OutFile + Copy(result, I + 2, MaxInt);
    return result;
}

// { TSetupFrameWaveEncoder }

QString TSetupFrameWaveEncoder::GetName()
{
    return LanguageSetup->SetupFormWaveEnc();
}

void TSetupFrameWaveEncoder::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    ui.WaveEncMp3Edit->setText(PrgSetup->WaveEncMp3());
    ui.WaveEncMp3ParameterEdit->setText(UpdateEncoderParameters(PrgSetup->WaveEncMp3Parameters()));
    ui.WaveEncOggEdit->setText(PrgSetup->WaveEncOgg());
    ui.WaveEncOggParameterEdit->setText(UpdateEncoderParameters(PrgSetup->WaveEncOggParameters()));
    UserIconLoader->DialogImage(DI_SelectFile, ui.WaveEncMp3Button1);
    UserIconLoader->DialogImage(DI_FindFile, ui.WaveEncMp3Button2);
    UserIconLoader->DialogImage(DI_SelectFile, ui.WaveEncOggButton1);
    UserIconLoader->DialogImage(DI_FindFile, ui.WaveEncOggButton2);
}

void TSetupFrameWaveEncoder::BeforeChangeLanguage()
{
}

void TSetupFrameWaveEncoder::LoadLanguage()
{
    ui.WaveEncMp3Edit_EditLabel->setText(LanguageSetup->SetupFormWaveEncMp3());
    ui.WaveEncMp3Button1->setToolTip(LanguageSetup->ChooseFile());
    ui.WaveEncMp3Button2->setToolTip(LanguageSetup->SetupFormSearchLame());
    ui.WaveEncMp3ParameterEdit_EditLabel->setText(LanguageSetup->SetupFormWaveEncMp3Parameters());
    ui.WaveEncOggEdit_EditLabel->setText(LanguageSetup->SetupFormWaveEncOgg());
    ui.WaveEncOggButton1->setToolTip(LanguageSetup->ChooseFile());
    ui.WaveEncOggButton2->setToolTip(LanguageSetup->SetupFormSearchOggEnc());
    ui.WaveEncOggParameterEdit_EditLabel->setText(LanguageSetup->SetupFormWaveEncOggParameters());
    setProperty("HelpContext", ID_FileOptionsWaveEncoder);
}

void TSetupFrameWaveEncoder::DOSBoxDirChanged()
{
}

void TSetupFrameWaveEncoder::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameWaveEncoder::HideFrame()
{
}

void TSetupFrameWaveEncoder::RestoreDefaults()
{
    ui.WaveEncMp3ParameterEdit->setText("-h -V 0 \"%1\" \"%2\"");
    ui.WaveEncOggParameterEdit->setText("\"%1\" --output=\"%2\" --quality=10");
}

void TSetupFrameWaveEncoder::SaveSetup()
{
    PrgSetup->SetWaveEncMp3(ui.WaveEncMp3Edit->text());
    PrgSetup->SetWaveEncMp3Parameters(UpdateEncoderParameters(ui.WaveEncMp3ParameterEdit->text()));
    PrgSetup->SetWaveEncOgg(ui.WaveEncOggEdit->text());
    PrgSetup->SetWaveEncOggParameters(UpdateEncoderParameters(ui.WaveEncOggParameterEdit->text()));
}

void TSetupFrameWaveEncoder::ButtonWork(QObject *Sender)
{
    QString S;
    switch (Sender->property("tag").toInt()) {
        case 10:
            S = Trim(ui.WaveEncMp3Edit->text()); if (!S.isEmpty()) S = ExtractFilePath(S);
            if (S.isEmpty()) S = GetSpecialFolder(window()->winId(), CSIDL_PROGRAM_FILES);
            PrgOpenDialog->Title = LanguageSetup->SetupFormWaveEncMp3();
            PrgOpenDialog->Filter = LanguageSetup->SetupFormExeFilter();
            if (!S.isEmpty()) PrgOpenDialog->InitialDir = S;
            if (PrgOpenDialog->Execute()) {
                ui.WaveEncMp3Edit->setText(PrgOpenDialog->FileName);
            }
            break;
        case 11:
            if (SearchLame(this)) ui.WaveEncMp3Edit->setText(PrgSetup->WaveEncMp3());
            break;
        case 12:
            S = Trim(ui.WaveEncOggEdit->text()); if (!S.isEmpty()) S = ExtractFilePath(S);
            if (S.isEmpty()) S = GetSpecialFolder(window()->winId(), CSIDL_PROGRAM_FILES);
            PrgOpenDialog->Title = LanguageSetup->SetupFormWaveEncOgg();
            PrgOpenDialog->Filter = LanguageSetup->SetupFormExeFilter();
            if (!S.isEmpty()) PrgOpenDialog->InitialDir = S;
            if (PrgOpenDialog->Execute()) {
                ui.WaveEncOggEdit->setText(PrgOpenDialog->FileName);
            }
            break;
        case 13:
            if (SearchOggEnc(this)) ui.WaveEncOggEdit->setText(PrgSetup->WaveEncOgg());
            break;
    }
}
