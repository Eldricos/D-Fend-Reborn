// C++ port of ModernProfileEditorVolumeFrameUnit.pas (implementation section).
#include "ModernProfileEditorVolumeFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TModernProfileEditorVolumeFrame::TModernProfileEditorVolumeFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM wiring
    connect(ui.SoundVolumeMasterLeftEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { SpinEditChange(ui.SoundVolumeMasterLeftEdit); });
    connect(ui.SoundVolumeMasterRightEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { SpinEditChange(ui.SoundVolumeMasterRightEdit); });
    connect(ui.SoundVolumeDisneyLeftEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { SpinEditChange(ui.SoundVolumeDisneyLeftEdit); });
    connect(ui.SoundVolumeDisneyRightEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { SpinEditChange(ui.SoundVolumeDisneyRightEdit); });
    connect(ui.SoundVolumeSpeakerLeftEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { SpinEditChange(ui.SoundVolumeSpeakerLeftEdit); });
    connect(ui.SoundVolumeSpeakerRightEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { SpinEditChange(ui.SoundVolumeSpeakerRightEdit); });
    connect(ui.SoundVolumeGUSLeftEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { SpinEditChange(ui.SoundVolumeGUSLeftEdit); });
    connect(ui.SoundVolumeGUSRightEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { SpinEditChange(ui.SoundVolumeGUSRightEdit); });
    connect(ui.SoundVolumeSBLeftEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { SpinEditChange(ui.SoundVolumeSBLeftEdit); });
    connect(ui.SoundVolumeSBRightEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { SpinEditChange(ui.SoundVolumeSBRightEdit); });
    connect(ui.SoundVolumeFMLeftEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { SpinEditChange(ui.SoundVolumeFMLeftEdit); });
    connect(ui.SoundVolumeFMRightEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { SpinEditChange(ui.SoundVolumeFMRightEdit); });
    connect(ui.SoundVolumeCDLeftEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { SpinEditChange(ui.SoundVolumeCDLeftEdit); });
    connect(ui.SoundVolumeCDRightEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { SpinEditChange(ui.SoundVolumeCDRightEdit); });
    connect(ui.SoundVolumeMasterLeftTrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.SoundVolumeMasterLeftTrackBar); });
    connect(ui.SoundVolumeMasterRightTrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.SoundVolumeMasterRightTrackBar); });
    connect(ui.SoundVolumeDisneyLeftTrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.SoundVolumeDisneyLeftTrackBar); });
    connect(ui.SoundVolumeDisneyRightTrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.SoundVolumeDisneyRightTrackBar); });
    connect(ui.SoundVolumeSpeakerLeftTrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.SoundVolumeSpeakerLeftTrackBar); });
    connect(ui.SoundVolumeSpeakerRightTrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.SoundVolumeSpeakerRightTrackBar); });
    connect(ui.SoundVolumeGUSLeftTrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.SoundVolumeGUSLeftTrackBar); });
    connect(ui.SoundVolumeGUSRightTrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.SoundVolumeGUSRightTrackBar); });
    connect(ui.SoundVolumeSBLeftTrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.SoundVolumeSBLeftTrackBar); });
    connect(ui.SoundVolumeSBRightTrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.SoundVolumeSBRightTrackBar); });
    connect(ui.SoundVolumeFMLeftTrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.SoundVolumeFMLeftTrackBar); });
    connect(ui.SoundVolumeFMRightTrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.SoundVolumeFMRightTrackBar); });
    connect(ui.SoundVolumeCDLeftTrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.SoundVolumeCDLeftTrackBar); });
    connect(ui.SoundVolumeCDRightTrackBar, &QSlider::valueChanged, this, [this]() { TrackBarChange(ui.SoundVolumeCDRightTrackBar); });
}

TModernProfileEditorVolumeFrame::~TModernProfileEditorVolumeFrame() = default;

// { TModernProfileEditorVolumeFrame }

void TModernProfileEditorVolumeFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    Q_UNUSED(InitData);

    ui.SoundVolumeLeftLabel1->setText(LanguageSetup->Left());
    ui.SoundVolumeLeftLabel2->setText(LanguageSetup->Left());
    ui.SoundVolumeLeftLabel3->setText(LanguageSetup->Left());
    ui.SoundVolumeLeftLabel4->setText(LanguageSetup->Left());
    ui.SoundVolumeLeftLabel5->setText(LanguageSetup->Left());
    ui.SoundVolumeLeftLabel6->setText(LanguageSetup->Left());
    ui.SoundVolumeLeftLabel7->setText(LanguageSetup->Left());
    ui.SoundVolumeRightLabel1->setText(LanguageSetup->Right());
    ui.SoundVolumeRightLabel2->setText(LanguageSetup->Right());
    ui.SoundVolumeRightLabel3->setText(LanguageSetup->Right());
    ui.SoundVolumeRightLabel4->setText(LanguageSetup->Right());
    ui.SoundVolumeRightLabel5->setText(LanguageSetup->Right());
    ui.SoundVolumeRightLabel6->setText(LanguageSetup->Right());
    ui.SoundVolumeRightLabel7->setText(LanguageSetup->Right());
    ui.SoundVolumeMasterLabel->setText(LanguageSetup->ProfileEditorSoundMasterVolume());
    ui.SoundVolumeDisneyLabel->setText(LanguageSetup->ProfileEditorSoundMiscDisneySoundsSource());
    ui.SoundVolumeSpeakerLabel->setText(LanguageSetup->ProfileEditorSoundMiscPCSpeaker());
    ui.SoundVolumeGUSLabel->setText(LanguageSetup->ProfileEditorSoundGUS());
    ui.SoundVolumeSBLabel->setText(LanguageSetup->ProfileEditorSoundSoundBlaster());
    ui.SoundVolumeFMLabel->setText(LanguageSetup->ProfileEditorSoundFM());
    ui.SoundVolumeCDLabel->setText(LanguageSetup->ProfileEditorSoundCD());

    JustChanging = false;

    setProperty("helpContext", ID_ProfileEditSoundVolume);  // HelpContext:=...
}

void TModernProfileEditorVolumeFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    ui.SoundVolumeMasterLeftEdit->setValue(Game->MixerVolumeMasterLeft());
    ui.SoundVolumeMasterRightEdit->setValue(Game->MixerVolumeMasterRight());
    ui.SoundVolumeDisneyLeftEdit->setValue(Game->MixerVolumeDisneyLeft());
    ui.SoundVolumeDisneyRightEdit->setValue(Game->MixerVolumeDisneyRight());
    ui.SoundVolumeSpeakerLeftEdit->setValue(Game->MixerVolumeSpeakerLeft());
    ui.SoundVolumeSpeakerRightEdit->setValue(Game->MixerVolumeSpeakerRight());
    ui.SoundVolumeGUSLeftEdit->setValue(Game->MixerVolumeGUSLeft());
    ui.SoundVolumeGUSRightEdit->setValue(Game->MixerVolumeGUSRight());
    ui.SoundVolumeSBLeftEdit->setValue(Game->MixerVolumeSBLeft());
    ui.SoundVolumeSBRightEdit->setValue(Game->MixerVolumeSBRight());
    ui.SoundVolumeFMLeftEdit->setValue(Game->MixerVolumeFMLeft());
    ui.SoundVolumeFMRightEdit->setValue(Game->MixerVolumeFMRight());
    ui.SoundVolumeCDLeftEdit->setValue(Game->MixerVolumeCDLeft());
    ui.SoundVolumeCDRightEdit->setValue(Game->MixerVolumeCDRight());

    SpinEditChange(this);
}

void TModernProfileEditorVolumeFrame::SpinEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (JustChanging) return;
    JustChanging = true;
    ui.SoundVolumeMasterLeftEdit->setValue(std::min(200, std::max(0, ui.SoundVolumeMasterLeftEdit->value())));
    ui.SoundVolumeMasterRightEdit->setValue(std::min(200, std::max(0, ui.SoundVolumeMasterRightEdit->value())));
    ui.SoundVolumeDisneyLeftEdit->setValue(std::min(200, std::max(0, ui.SoundVolumeDisneyLeftEdit->value())));
    ui.SoundVolumeDisneyRightEdit->setValue(std::min(200, std::max(0, ui.SoundVolumeDisneyRightEdit->value())));
    ui.SoundVolumeSpeakerLeftEdit->setValue(std::min(200, std::max(0, ui.SoundVolumeSpeakerLeftEdit->value())));
    ui.SoundVolumeSpeakerRightEdit->setValue(std::min(200, std::max(0, ui.SoundVolumeSpeakerRightEdit->value())));
    ui.SoundVolumeGUSLeftEdit->setValue(std::min(200, std::max(0, ui.SoundVolumeGUSLeftEdit->value())));
    ui.SoundVolumeGUSRightEdit->setValue(std::min(200, std::max(0, ui.SoundVolumeGUSRightEdit->value())));
    ui.SoundVolumeSBLeftEdit->setValue(std::min(200, std::max(0, ui.SoundVolumeSBLeftEdit->value())));
    ui.SoundVolumeSBRightEdit->setValue(std::min(200, std::max(0, ui.SoundVolumeSBRightEdit->value())));
    ui.SoundVolumeFMLeftEdit->setValue(std::min(200, std::max(0, ui.SoundVolumeFMLeftEdit->value())));
    ui.SoundVolumeFMRightEdit->setValue(std::min(200, std::max(0, ui.SoundVolumeFMRightEdit->value())));
    ui.SoundVolumeCDLeftEdit->setValue(std::min(200, std::max(0, ui.SoundVolumeCDLeftEdit->value())));
    ui.SoundVolumeCDRightEdit->setValue(std::min(200, std::max(0, ui.SoundVolumeCDRightEdit->value())));

    ui.SoundVolumeMasterLeftTrackBar->setValue(ui.SoundVolumeMasterLeftTrackBar->maximum() - ui.SoundVolumeMasterLeftEdit->value());
    ui.SoundVolumeMasterRightTrackBar->setValue(ui.SoundVolumeMasterRightTrackBar->maximum() - ui.SoundVolumeMasterRightEdit->value());
    ui.SoundVolumeDisneyLeftTrackBar->setValue(ui.SoundVolumeDisneyLeftTrackBar->maximum() - ui.SoundVolumeDisneyLeftEdit->value());
    ui.SoundVolumeDisneyRightTrackBar->setValue(ui.SoundVolumeDisneyRightTrackBar->maximum() - ui.SoundVolumeDisneyRightEdit->value());
    ui.SoundVolumeSpeakerLeftTrackBar->setValue(ui.SoundVolumeSpeakerLeftTrackBar->maximum() - ui.SoundVolumeSpeakerLeftEdit->value());
    ui.SoundVolumeSpeakerRightTrackBar->setValue(ui.SoundVolumeSpeakerRightTrackBar->maximum() - ui.SoundVolumeSpeakerRightEdit->value());
    ui.SoundVolumeGUSLeftTrackBar->setValue(ui.SoundVolumeGUSLeftTrackBar->maximum() - ui.SoundVolumeGUSLeftEdit->value());
    ui.SoundVolumeGUSRightTrackBar->setValue(ui.SoundVolumeGUSRightTrackBar->maximum() - ui.SoundVolumeGUSRightEdit->value());
    ui.SoundVolumeSBLeftTrackBar->setValue(ui.SoundVolumeSBLeftTrackBar->maximum() - ui.SoundVolumeSBLeftEdit->value());
    ui.SoundVolumeSBRightTrackBar->setValue(ui.SoundVolumeSBRightTrackBar->maximum() - ui.SoundVolumeSBRightEdit->value());
    ui.SoundVolumeFMLeftTrackBar->setValue(ui.SoundVolumeFMLeftTrackBar->maximum() - ui.SoundVolumeFMLeftEdit->value());
    ui.SoundVolumeFMRightTrackBar->setValue(ui.SoundVolumeFMRightTrackBar->maximum() - ui.SoundVolumeFMRightEdit->value());
    ui.SoundVolumeCDLeftTrackBar->setValue(ui.SoundVolumeCDLeftTrackBar->maximum() - ui.SoundVolumeCDLeftEdit->value());
    ui.SoundVolumeCDRightTrackBar->setValue(ui.SoundVolumeCDRightTrackBar->maximum() - ui.SoundVolumeCDRightEdit->value());
    JustChanging = false;
}

void TModernProfileEditorVolumeFrame::TrackBarChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (JustChanging) return;
    JustChanging = true;
    ui.SoundVolumeMasterLeftEdit->setValue(ui.SoundVolumeMasterLeftTrackBar->maximum() - ui.SoundVolumeMasterLeftTrackBar->value());
    ui.SoundVolumeMasterRightEdit->setValue(ui.SoundVolumeMasterRightTrackBar->maximum() - ui.SoundVolumeMasterRightTrackBar->value());
    ui.SoundVolumeDisneyLeftEdit->setValue(ui.SoundVolumeDisneyLeftTrackBar->maximum() - ui.SoundVolumeDisneyLeftTrackBar->value());
    ui.SoundVolumeDisneyRightEdit->setValue(ui.SoundVolumeDisneyRightTrackBar->maximum() - ui.SoundVolumeDisneyRightTrackBar->value());
    ui.SoundVolumeSpeakerLeftEdit->setValue(ui.SoundVolumeSpeakerLeftTrackBar->maximum() - ui.SoundVolumeSpeakerLeftTrackBar->value());
    ui.SoundVolumeSpeakerRightEdit->setValue(ui.SoundVolumeSpeakerRightTrackBar->maximum() - ui.SoundVolumeSpeakerRightTrackBar->value());
    ui.SoundVolumeGUSLeftEdit->setValue(ui.SoundVolumeGUSLeftTrackBar->maximum() - ui.SoundVolumeGUSLeftTrackBar->value());
    ui.SoundVolumeGUSRightEdit->setValue(ui.SoundVolumeGUSRightTrackBar->maximum() - ui.SoundVolumeGUSRightTrackBar->value());
    ui.SoundVolumeSBLeftEdit->setValue(ui.SoundVolumeSBLeftTrackBar->maximum() - ui.SoundVolumeSBLeftTrackBar->value());
    ui.SoundVolumeSBRightEdit->setValue(ui.SoundVolumeSBRightTrackBar->maximum() - ui.SoundVolumeSBRightTrackBar->value());
    ui.SoundVolumeFMLeftEdit->setValue(ui.SoundVolumeFMLeftTrackBar->maximum() - ui.SoundVolumeFMLeftTrackBar->value());
    ui.SoundVolumeFMRightEdit->setValue(ui.SoundVolumeFMRightTrackBar->maximum() - ui.SoundVolumeFMRightTrackBar->value());
    ui.SoundVolumeCDLeftEdit->setValue(ui.SoundVolumeCDLeftTrackBar->maximum() - ui.SoundVolumeCDLeftTrackBar->value());
    ui.SoundVolumeCDRightEdit->setValue(ui.SoundVolumeCDRightTrackBar->maximum() - ui.SoundVolumeCDRightTrackBar->value());
    JustChanging = false;
}

void TModernProfileEditorVolumeFrame::GetGame(TGame* Game)
{
    Game->SetMixerVolumeMasterLeft(ui.SoundVolumeMasterLeftEdit->value());
    Game->SetMixerVolumeMasterRight(ui.SoundVolumeMasterRightEdit->value());
    Game->SetMixerVolumeDisneyLeft(ui.SoundVolumeDisneyLeftEdit->value());
    Game->SetMixerVolumeDisneyRight(ui.SoundVolumeDisneyRightEdit->value());
    Game->SetMixerVolumeSpeakerLeft(ui.SoundVolumeSpeakerLeftEdit->value());
    Game->SetMixerVolumeSpeakerRight(ui.SoundVolumeSpeakerRightEdit->value());
    Game->SetMixerVolumeGUSLeft(ui.SoundVolumeGUSLeftEdit->value());
    Game->SetMixerVolumeGUSRight(ui.SoundVolumeGUSRightEdit->value());
    Game->SetMixerVolumeSBLeft(ui.SoundVolumeSBLeftEdit->value());
    Game->SetMixerVolumeSBRight(ui.SoundVolumeSBRightEdit->value());
    Game->SetMixerVolumeFMLeft(ui.SoundVolumeFMLeftEdit->value());
    Game->SetMixerVolumeFMRight(ui.SoundVolumeFMRightEdit->value());
    Game->SetMixerVolumeCDLeft(ui.SoundVolumeCDLeftEdit->value());
    Game->SetMixerVolumeCDRight(ui.SoundVolumeCDRightEdit->value());
}
