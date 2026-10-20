// D-Fend Reborn: "DOSBox Staging" page of the modern profile editor
// (DOSBox Staging settings without a DOSBox 0.74 counterpart, StagingSettings
// in GameDBUnit.h). Built in code (no .ui file).
#pragma once

#include <QWidget>
#include <QList>
#include "ModernProfileEditorFormUnit.h"

class QComboBox;

class TModernProfileEditorStagingFrame : public QWidget, public IModernProfileEditorFrame
{
public:
    explicit TModernProfileEditorStagingFrame(QWidget* AOwner);
    void InitGUI(TModernProfileEditorInitData& InitData) override;
    void SetGame(TGame* Game, bool LoadFromTemplate) override;
    void GetGame(TGame* Game) override;
private:
    QList<QComboBox*> Boxes;   // StagingSettings[I]
    QList<QStringList> BaseValues;   // values of the "Default values" page
    PString Installation = nullptr;   // DOSBox installation of the profile
    QString ChoicesInstallation;
    bool ChoicesLoaded = false;
    void UpdateChoices();
protected:
    void showEvent(QShowEvent* Event) override;
private:
    void BrowseClick(int I);
};
