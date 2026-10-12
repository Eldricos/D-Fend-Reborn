// C++ port of ModernProfileEditorScummVMFrameUnit.pas (implementation section).
#include "ModernProfileEditorScummVMFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "IconLoaderUnit.h"
#include "TextEditPopupUnit.h"

// Memo.Lines.Assign(St)
static void SetMemoLines(QTextEdit* Memo, TStringList* St)
{
    Memo->setPlainText(St->ToQStringList().join('\n'));
}

// Memo.Lines (as TStringList, caller deletes)
static TStringList* MemoLines(QTextEdit* Memo)
{
    TStringList* St = new TStringList();
    QString T = Memo->toPlainText();
    if (T != "") {
        QStringList L = T.split('\n');
        if (!L.isEmpty() && L.last().isEmpty()) L.removeLast();
        St->AddStrings(L);
    }
    return St;
}

/* TModernProfileEditorScummVMFrame */

TModernProfileEditorScummVMFrame::TModernProfileEditorScummVMFrame(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    OpenDialog = new TOpenDialog(this);
    SaveDialog = new TSaveDialog(this);

    connect(ui.ExtraDirButton, &QToolButton::clicked, this, [this]() { ExtraDirButtonClick(ui.ExtraDirButton); });
    connect(ui.LanguageComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { LanguageComboBoxChange(ui.LanguageComboBox); });
    connect(ui.SavePathEditButton, &QToolButton::clicked, this, [this]() { SavePathEditButtonClick(ui.SavePathEditButton); });
    connect(ui.SavePathEdit, &QLineEdit::textChanged, this, [this]() { SavePathEditChange(ui.SavePathEdit); });
    connect(ui.CustomSetsClearButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.CustomSetsClearButton); });
    connect(ui.CustomSetsLoadButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.CustomSetsLoadButton); });
    connect(ui.CustomSetsSaveButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.CustomSetsSaveButton); });
    connect(ui.ExtraDirEdit, &QLineEdit::textChanged, this, [this]() { ExtraDirEditChange(ui.ExtraDirEdit); });
}

TModernProfileEditorScummVMFrame::~TModernProfileEditorScummVMFrame()
{
    delete OpenDialog;
    delete SaveDialog;
}

void TModernProfileEditorScummVMFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    InitData.OnShowFrame = [this](QObject* Sender) { ShowFrame(Sender); };

    LastGameName = "";

    // NoFlicker(...): not needed with Qt

    SetRichEditPopup(ui.CustomSetsMemo);

    ui.LanguageLabel->setText(LanguageSetup->ProfileEditorScummVMLanguage());
    ui.AutosaveLabel->setText(LanguageSetup->ProfileEditorScummVMAutosave());
    ui.TalkSpeedLabel->setText(LanguageSetup->ProfileEditorScummVMTextSpeed());
    ui.SubtitlesCheckBox->setText(LanguageSetup->ProfileEditorScummVMSubtitles());
    ui.ConfirmExitCheckBox->setText(LanguageSetup->ProfileEditorScummVMConfirmExit());
    ui.RunAsAdminCheckBox->setText(LanguageSetup->ProfileEditorRunAsAdmin());
    ui.RunAsAdminCheckBox->setVisible(PrgSetup->OfferRunAsAdmin());
    ui.SavePathGroupBox->setTitle(LanguageSetup->ProfileEditorScummVMSavePath());
    ui.SavePathDefaultRadioButton->setText(LanguageSetup->ProfileEditorScummVMSavePathGameDir() + " (" + LanguageSetup->Default() + ")");
    ui.SavePathCustomRadioButton->setText(LanguageSetup->ProfileEditorScummVMSavePathCustom());
    ui.SavePathEditButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.ExtraDirCheckBox->setText(LanguageSetup->ProfileEditorScummVMExtraPath());
    ui.ExtraDirButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.CommandLineEdit_EditLabel->setText(LanguageSetup->ProfileEditorScummVMCommandLine());
    ui.CustomSetsLabel->setText(LanguageSetup->ProfileEditorCustomSetsSheet());
    {
        QFont F = ui.CustomSetsMemo->font();
        F.setFamily("Courier New");
        F.setStyleHint(QFont::Monospace);
        ui.CustomSetsMemo->setFont(F);
    }
    ui.CustomSetsClearButton->setText(LanguageSetup->Del());
    ui.CustomSetsLoadButton->setText(LanguageSetup->Load());
    ui.CustomSetsSaveButton->setText(LanguageSetup->Save());
    UserIconLoader->DialogImage(DI_SelectFolder, ui.SavePathEditButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.ExtraDirButton);
    UserIconLoader->DialogImage(DI_Clear, ui.CustomSetsClearButton);
    UserIconLoader->DialogImage(DI_Load, ui.CustomSetsLoadButton);
    UserIconLoader->DialogImage(DI_Save, ui.CustomSetsSaveButton);

    CurrentGameName = InitData.CurrentScummVMGameName;
    CurrentScummVMPath = InitData.CurrentScummVMPath;
    ConfOpt = InitData.GameDB->ConfOpt();

    setProperty("HelpContext", ID_ProfileEditScummVM);  // HelpContext
}

void TModernProfileEditorScummVMFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    TStringList* St;

    ui.AutosaveEdit->setValue(Min(86400, Max(1, Game->ScummVMAutosave())));
    ui.TalkSpeedEdit->setValue(Min(1000, Max(1, Game->ScummVMTalkSpeed())));
    ui.SubtitlesCheckBox->setChecked(Game->ScummVMSubtitles());
    ui.ConfirmExitCheckBox->setChecked(Game->ScummVMConfirmExit());
    ui.RunAsAdminCheckBox->setChecked(Game->RunAsAdmin());

    ui.SavePathEdit->setText(Game->ScummVMSavePath());
    SavePathEditChange(this);

    ui.ExtraDirEdit->setText(Game->ScummVMExtraPath());
    ExtraDirEditChange(this);

    SaveLang = Game->ScummVMLanguage();

    ui.CommandLineEdit->setText(Game->ScummVMParameters());

    St = StringToStringList(Game->CustomSettings());
    SetMemoLines(ui.CustomSetsMemo, St);
    delete St;
}

void TModernProfileEditorScummVMFrame::ShowFrame(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString Save, S;
    int I, J;
    TStringList *St, *St2;

    if (LastGameName == *CurrentGameName) return;
    LastGameName = *CurrentGameName;

    if (ui.LanguageComboBox->currentIndex() >= 0) {
        Save = Trim(ExtUpperCase(ui.LanguageComboBox->currentText()));
        if (Save == Trim(ExtUpperCase(LanguageSetup->Custom()))) Save = Trim(ExtUpperCase(ui.CustomLanguageEdit->text()));
        if (Save == Trim(ExtUpperCase(LanguageSetup->Default()))) Save = "";
    } else {
        Save = Trim(ExtUpperCase(SaveLang));
    }

    {
        const QSignalBlocker Blocker(ui.LanguageComboBox);  // Items.BeginUpdate
        ui.LanguageComboBox->clear();

        St = ValueToList(ConfOpt->ScummVMLanguages(), ";,");
        S = Trim(ExtUpperCase(LastGameName));
        for (I = 0; I < St->Count(); I++) {
            J = Pos(":", (*St)[I]);
            if (J == 0) continue;
            if (Trim(ExtUpperCase(Copy((*St)[I], 1, J - 1))) == S) {
                St2 = ValueToList(Copy((*St)[I], J + 1, MaxInt), "-");
                ui.LanguageComboBox->addItems(St2->ToQStringList());
                delete St2;
                break;
            }
        }
        delete St;

        if (ui.LanguageComboBox->count() == 0) ui.LanguageComboBox->addItem(LanguageSetup->Default());

        ui.LanguageComboBox->addItem(LanguageSetup->Custom());
    }

    if (Save == "") ui.LanguageComboBox->setCurrentIndex(0); else {
        I = -1;
        for (J = 0; J < ui.LanguageComboBox->count(); J++) if (Trim(ExtUpperCase(ui.LanguageComboBox->itemText(J))) == Save) { I = J; break; }
        if (I >= 0) {
            ui.LanguageComboBox->setCurrentIndex(I);
            ui.CustomLanguageEdit->setText("");
        } else {
            ui.LanguageComboBox->setCurrentIndex(ui.LanguageComboBox->count() - 1);
            ui.CustomLanguageEdit->setText(ExtLowerCase(Save));
        }
    }

    LanguageComboBoxChange(this);
}

void TModernProfileEditorScummVMFrame::GetGame(TGame* Game)
{
    if (ui.LanguageComboBox->currentIndex() >= 0) {
        Game->SetScummVMLanguage(Trim(ExtUpperCase(ui.LanguageComboBox->currentText())));
        if (Game->ScummVMLanguage() == Trim(ExtUpperCase(LanguageSetup->Custom()))) Game->SetScummVMLanguage(Trim(ExtUpperCase(ui.CustomLanguageEdit->text())));
        if (Game->ScummVMLanguage() == Trim(ExtUpperCase(LanguageSetup->Default()))) Game->SetScummVMLanguage("");
    } else {
        Game->SetScummVMLanguage(Trim(ExtUpperCase(SaveLang)));
    }
    Game->SetScummVMLanguage(ExtLowerCase(Game->ScummVMLanguage()));

    Game->SetScummVMAutosave(ui.AutosaveEdit->value());
    Game->SetScummVMTalkSpeed(ui.TalkSpeedEdit->value());
    Game->SetScummVMSubtitles(ui.SubtitlesCheckBox->isChecked());
    Game->SetScummVMConfirmExit(ui.ConfirmExitCheckBox->isChecked());
    Game->SetRunAsAdmin(ui.RunAsAdminCheckBox->isChecked());
    if (ui.SavePathDefaultRadioButton->isChecked()) Game->SetScummVMSavePath(""); else Game->SetScummVMSavePath(Trim(ui.SavePathEdit->text()));
    if (ui.ExtraDirCheckBox->isChecked()) Game->SetScummVMExtraPath(Trim(ui.ExtraDirEdit->text())); else Game->SetScummVMExtraPath("");
    Game->SetScummVMParameters(ui.CommandLineEdit->text());
    TStringList* St = MemoLines(ui.CustomSetsMemo);
    Game->SetCustomSettings(StringListToString(St));
    delete St;
}

void TModernProfileEditorScummVMFrame::LanguageComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.CustomLanguageEdit->setVisible(ui.LanguageComboBox->currentIndex() == ui.LanguageComboBox->count() - 1);
}

void TModernProfileEditorScummVMFrame::SavePathEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.SavePathDefaultRadioButton->setChecked(Trim(ui.SavePathEdit->text()) == "");
    ui.SavePathCustomRadioButton->setChecked(Trim(ui.SavePathEdit->text()) != "");
}

void TModernProfileEditorScummVMFrame::SavePathEditButtonClick(QObject* Sender)
{
    QString S;

    S = Trim(ui.SavePathEdit->text());
    if (S == "") S = Trim(*CurrentScummVMPath);
    if (S == "") S = PrgSetup->GameDir();
    S = MakeAbsPath(S, PrgSetup->BaseDir());
    if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
    ui.SavePathEdit->setText(MakeRelPath(S, PrgSetup->BaseDir(), true));
    SavePathEditChange(Sender);
}

void TModernProfileEditorScummVMFrame::ExtraDirEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.ExtraDirCheckBox->setChecked(Trim(ui.ExtraDirEdit->text()) != "");
}

void TModernProfileEditorScummVMFrame::ExtraDirButtonClick(QObject* Sender)
{
    QString S;

    S = Trim(ui.ExtraDirEdit->text());
    if (S == "") S = Trim(*CurrentScummVMPath);
    if (S == "") S = PrgSetup->GameDir();
    S = MakeAbsPath(S, PrgSetup->BaseDir());
    if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
    ui.ExtraDirEdit->setText(MakeRelPath(S, PrgSetup->BaseDir(), true));
    ExtraDirEditChange(Sender);
}

void TModernProfileEditorScummVMFrame::ButtonWork(QObject* Sender)
{
    switch (Sender->property("tag").toInt()) {
    case 0: ui.CustomSetsMemo->clear(); break;
    case 1: {
        ForceDirectories(PrgDataDir() + CustomConfigsSubDir);

        OpenDialog->DefaultExt = "txt";
        OpenDialog->Filter = LanguageSetup->ProfileEditorCustomSetsFilter();
        OpenDialog->Title = LanguageSetup->ProfileEditorCustomSetsLoadTitle();
        OpenDialog->InitialDir = PrgDataDir() + CustomConfigsSubDir;
        if (!OpenDialog->Execute()) return;
        TStringList St;
        if (St.LoadFromFile(OpenDialog->FileName)) {
            SetMemoLines(ui.CustomSetsMemo, &St);
        } else {
            MessageDlg(Format(LanguageSetup->MessageCouldNotOpenFile(), {OpenDialog->FileName}), mtError, mbOK, 0);
        }
        break;
    }
    case 2: {
        ForceDirectories(PrgDataDir() + CustomConfigsSubDir);

        SaveDialog->DefaultExt = "txt";
        SaveDialog->Filter = LanguageSetup->ProfileEditorCustomSetsFilter();
        SaveDialog->Title = LanguageSetup->ProfileEditorCustomSetsSaveTitle();
        SaveDialog->InitialDir = PrgDataDir() + CustomConfigsSubDir;
        if (!SaveDialog->Execute()) return;
        TStringList* St = MemoLines(ui.CustomSetsMemo);
        if (!St->SaveToFile(SaveDialog->FileName)) {
            MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {SaveDialog->FileName}), mtError, mbOK, 0);
        }
        delete St;
        break;
    }
    }
}
