// C++ port of SetupFrameImageScalingUnit.pas (implementation section).
#include "SetupFrameImageScalingUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameImageScaling::TSetupFrameImageScaling(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TSetupFrameImageScaling::~TSetupFrameImageScaling() = default;

// { TSetupFrameImageScaling }

QString TSetupFrameImageScaling::GetName()
{
    return LanguageSetup->SetupFormImageScaling();
}

void TSetupFrameImageScaling::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    /* PrgSetup.ImageFilter: 0..8, 0=off, 1=simple&slow, 2=Box, 3=Triangle, 4=Hermite, 5=Bell, 6=B-Spline, 7=Lanczos3, 8=Mitchell */
    while (ui.ImageScalerComboBox->count() < 8) ui.ImageScalerComboBox->addItem(QString());
    const int F = PrgSetup->ImageFilter();
    if (F >= 0 && F <= 1) ui.ImageScalerComboBox->setCurrentIndex(F);
    else if (F >= 2 && F <= 8) ui.ImageScalerComboBox->setCurrentIndex(F - 1);
    else ui.ImageScalerComboBox->setCurrentIndex(1);
    setProperty("HelpContext", ID_FileOptionsImageScaling);
}

void TSetupFrameImageScaling::BeforeChangeLanguage()
{
}

void TSetupFrameImageScaling::LoadLanguage()
{
    int I;
    ui.ImageScalerLabel->setText(LanguageSetup->SetupFormImageScalingLabel());
    I = ui.ImageScalerComboBox->currentIndex();
    QComboBox *C = ui.ImageScalerComboBox;
    while (C->count() < 8) C->addItem(QString());
    C->setItemText(0, LanguageSetup->Off());
    C->setItemText(1, LanguageSetup->SetupFormImageScalingAlgorithmBox());
    C->setItemText(2, LanguageSetup->SetupFormImageScalingAlgorithmTriangle());
    C->setItemText(3, LanguageSetup->SetupFormImageScalingAlgorithmHermite());
    C->setItemText(4, LanguageSetup->SetupFormImageScalingAlgorithmBell());
    C->setItemText(5, LanguageSetup->SetupFormImageScalingAlgorithmBSpline());
    C->setItemText(6, LanguageSetup->SetupFormImageScalingAlgorithmLanczos3());
    C->setItemText(7, LanguageSetup->SetupFormImageScalingAlgorithmMitchell());
    ui.ImageScalerComboBox->setCurrentIndex(I);
}

void TSetupFrameImageScaling::DOSBoxDirChanged()
{
}

void TSetupFrameImageScaling::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameImageScaling::HideFrame()
{
}

void TSetupFrameImageScaling::RestoreDefaults()
{
    ui.ImageScalerComboBox->setCurrentIndex(5);
}

void TSetupFrameImageScaling::SaveSetup()
{
    const int I = ui.ImageScalerComboBox->currentIndex();
    if (I == 0) PrgSetup->SetImageFilter(0);
    else if (I >= 1 && I <= 7) PrgSetup->SetImageFilter(I + 1);
}
