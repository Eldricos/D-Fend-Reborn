#include "ModernProfileEditorStagingFrameUnit.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include "CommonTools.h"
#include "GameDBUnit.h"
#include "HelpConsts.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"

TModernProfileEditorStagingFrame::TModernProfileEditorStagingFrame(QWidget* AOwner) : QWidget(AOwner)
{
    setObjectName(QStringLiteral("ModernProfileEditorStagingFrame"));
    setProperty("HelpContext", ID_ProfileEditStaging);
}

void TModernProfileEditorStagingFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    Q_UNUSED(InitData);
    auto* Main = new QVBoxLayout(this);
    Main->setContentsMargins(8, 8, 8, 8);

    auto* Info = new QLabel(LanguageSetup->ProfileEditorStagingInfo(), this);
    Info->setObjectName(QStringLiteral("StagingInfoLabel"));
    Info->setWordWrap(true);
    Main->addWidget(Info);

    auto* Scroll = new QScrollArea(this);
    Scroll->setWidgetResizable(true);
    Scroll->setFrameShape(QFrame::NoFrame);
    auto* Content = new QWidget(Scroll);
    auto* ContentLayout = new QVBoxLayout(Content);
    ContentLayout->setContentsMargins(0, 0, 0, 0);
    Scroll->setWidget(Content);
    Main->addWidget(Scroll, 1);

    // (group boxes: names used by MarkIgnoredDOSBoxSettings)
    const struct { const char* Group; QString Title; } Groups[] = {
        {"graphics", LanguageSetup->ProfileEditorGraphicsSheet()},
        {"cpu", LanguageSetup->ProfileEditorCPUSheet()},
        {"sound", LanguageSetup->ProfileEditorSoundSheet()},
        {"midi", LanguageSetup->ProfileEditorSoundMIDI()},
        {"mouse", LanguageSetup->ProfileEditorMouseSheet()},
    };
    Boxes.clear();
    for (int I = 0; I < StagingSettingCount; I++) Boxes.append(nullptr);
    for (const auto& G : Groups) {
        auto* Box = new QGroupBox(G.Title, Content);
        Box->setObjectName(QStringLiteral("Staging_") + QLatin1String(G.Group));
        auto* Form = new QFormLayout(Box);
        Form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        Form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        for (int I = 0; I < StagingSettingCount; I++) {
            const TStagingSettingInfo& S = StagingSettings[I];
            if (qstrcmp(S.Group, G.Group) != 0) continue;
            auto* C = new QComboBox(Box);
            C->setEditable(S.Kind != skList);
            C->addItems(QString::fromLatin1(S.Values).split(';'));
            C->setToolTip(QStringLiteral("[%1] %2").arg(QLatin1String(S.Section), QLatin1String(S.Key)));
            Boxes[I] = C;
            QWidget* Field = C;
            if ((S.Kind == skFile) || (S.Kind == skFolder)) {
                Field = new QWidget(Box);
                auto* H = new QHBoxLayout(Field);
                H->setContentsMargins(0, 0, 0, 0);
                H->addWidget(C, 1);
                auto* B = new QToolButton(Field);
                B->setText(QStringLiteral("..."));
                connect(B, &QToolButton::clicked, this, [this, I]() { BrowseClick(I); });
                H->addWidget(B);
            }
            auto* L = new QLabel(LanguageSetup->ProfileEditorStagingSetting(I), Box);
            L->setWordWrap(true);
            L->setFixedWidth(230);   // (same label column in all groups)
            L->setBuddy(C);
            Form->addRow(L, Field);
        }
        ContentLayout->addWidget(Box);
    }
    ContentLayout->addStretch(1);
}

void TModernProfileEditorStagingFrame::BrowseClick(int I)
{
    QComboBox* C = Boxes[I];
    QString S = Trim(C->currentText());
    if (ExtUpperCase(S) == "DEFAULT") S.clear();
    const QString Start = S.isEmpty() ? PrgSetup->BaseDir() : MakeAbsPath(S, PrgSetup->BaseDir());
    if (StagingSettings[I].Kind == skFolder) {
        S = QFileDialog::getExistingDirectory(this, LanguageSetup->ProfileEditorStagingSetting(I), Start);
    } else {
        S = QFileDialog::getOpenFileName(this, LanguageSetup->ProfileEditorStagingSetting(I), Start, QStringLiteral("SoundFont (*.sf2 *.sf3);;* (*)"));
    }
    if (S.isEmpty()) return;
    C->setEditText(MakeRelPath(QDir::toNativeSeparators(S), PrgSetup->BaseDir()));
}

void TModernProfileEditorStagingFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    for (int I = 0; I < StagingSettingCount; I++) {
        QComboBox* C = Boxes[I];
        if (C == nullptr) continue;
        QString S = Trim(Game->StagingSetting(I));
        if (S.isEmpty()) S = QStringLiteral("default");
        const int Index = C->findText(S, Qt::MatchFixedString);
        if (Index >= 0) C->setCurrentIndex(Index);
        else if (C->isEditable()) C->setEditText(S);
        else C->setCurrentIndex(0);
    }
}

void TModernProfileEditorStagingFrame::GetGame(TGame* Game)
{
    for (int I = 0; I < StagingSettingCount; I++) {
        QComboBox* C = Boxes[I];
        if (C == nullptr) continue;
        QString S = Trim(C->currentText());
        if (S.isEmpty()) S = QStringLiteral("default");
        Game->SetStagingSetting(I, S);
    }
}
