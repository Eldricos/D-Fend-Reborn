// C++ port of WizardFormUnit.pas (implementation section).
#include "WizardFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "GameDBToolsUnit.h"
#include "DosBoxUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "PrgConsts.h"

TWizardForm* WizardForm = nullptr;

namespace {

constexpr quint16 VK_F1 = 0x70;

void RedrawFrameIfVisible(QWidget *F)
{
    if (F == nullptr || F->isHidden()) return;
    F->update();
    F->repaint();
}

}  // namespace

TWizardForm::TWizardForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);

    // The wizard page frames are created in FormShow with Parent:=self and
    // Align:=alClient; they are placed in this alClient container (only one
    // of them is visible at a time).
    FrameHost = new QWidget(this);
    FrameHost->setObjectName("FrameHost");
    FrameHost->setProperty("vclAlign", "alClient");
    FrameHost->setGeometry(0, 0, width(), ui.BottomPanel->y());
    auto *L = new QVBoxLayout(FrameHost);
    L->setContentsMargins(0, 0, 0, 0);
    L->setSpacing(0);

    VclLayout::setupForm(this);

    connect(ui.PreviousButton, &QPushButton::clicked, this, [this]() { StepButtonWork(ui.PreviousButton); });
    connect(ui.NextButton, &QPushButton::clicked, this, [this]() { StepButtonWork(ui.NextButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        quint16 K = (Key == Qt::Key_F1) ? VK_F1 : 0;
        if (K == 0) return false;
        FormKeyDown(this, K, Mods & ~Qt::KeypadModifier);
        return false;
    });

    FormCreate(this);
}

TWizardForm::~TWizardForm()
{
}

void TWizardForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (E->spontaneous() || FFormShown) return;
    FFormShown = true;
    FormShow(this);
}

void TWizardForm::hideEvent(QHideEvent *E)
{
    TVclForm::hideEvent(E);
    if (E->spontaneous() || !FFormShown) return;
    FFormShown = false;
    FormHide(this);
}

void TWizardForm::done(int R)
{
    TCloseAction Action = caHide;
    FormClose(this, Action);
    TVclForm::done(R);
}

QString TWizardForm::CalcProfileName()
{
    if (ScummVM) return WizardScummVMFrame->ui.GameNameComboBox->currentText();

    if (WindowsMode) return CalcProfileNameFromFile();

    if (WizardTemplateFrame->ui.TemplateType1->isChecked()) return WizardTemplateFrame->ui.TemplateType1List->currentText();
    if (WizardTemplateFrame->ui.TemplateType2->isChecked()) return WizardTemplateFrame->ui.TemplateType2List->currentText();
    if (WizardTemplateFrame->ui.TemplateType3->isChecked()) return WizardTemplateFrame->ui.TemplateType3List->currentText();
    return CalcProfileNameFromFile();
}

QString TWizardForm::CalcProfileNameFromFile()
{
    QString S = Trim(WizardPrgFileFrame->ui.ProgramEdit->text());
    if (S.isEmpty()) S = Trim(WizardPrgFileFrame->ui.SetupEdit->text());

    if (S.isEmpty()) return "Game";

    S = ChangeFileExt(ExtractFileName(S), "");
    for (int I = 1; I <= Length(S); I++) if (S[I - 1] != ' ') {
        if (I == 1)
            S[I - 1] = ExtUpperCase(QString(S[I - 1]))[0];
        else
            S[I - 1] = ExtLowerCase(QString(S[I - 1]))[0];
    }
    return S;
}

void TWizardForm::FormClose(QObject* Sender, TCloseAction& Action)
{
    Q_UNUSED(Sender); Q_UNUSED(Action);
    if (WizardBaseFrame != nullptr) WizardBaseFrame->BeforeClose();
}

void TWizardForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed

    TemplateDB = new TGameDB(PrgDataDir() + TemplateSubDir, false);
    AutoSetupDB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);

    setWindowTitle(LanguageSetup->WizardForm());
    ui.PreviousButton->setText(LanguageSetup->WizardFormButtonPrevious());
    ui.NextButton->setText(LanguageSetup->WizardFormButtonNext());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

    UserIconLoader->DialogImage(DI_Previous, ui.PreviousButton);
    UserIconLoader->DialogImage(DI_Next, ui.NextButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    DoOKButtonClick = false;
    ActivePage = 0;
    InsecureTemplate = false;
    OpenEditorNow = false;
    OpenInstallationSupportDialog = false;
}

void TWizardForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    auto *L = static_cast<QVBoxLayout *>(FrameHost->layout());

    WizardBaseFrame = new TWizardBaseFrame(FrameHost); L->addWidget(WizardBaseFrame);
    WizardBaseFrame->setVisible(false); WizardBaseFrame->Init(GameDB, this);

    WizardPrgFileFrame = new TWizardPrgFileFrame(FrameHost); L->addWidget(WizardPrgFileFrame);
    WizardPrgFileFrame->setVisible(false); WizardPrgFileFrame->Init(GameDB);

    WizardTemplateFrame = new TWizardTemplateFrame(FrameHost); L->addWidget(WizardTemplateFrame);
    WizardTemplateFrame->setVisible(false); WizardTemplateFrame->Init(GameDB, TemplateDB, AutoSetupDB);

    WizardGameInfoFrame = new TWizardGameInfoFrame(FrameHost); L->addWidget(WizardGameInfoFrame);
    WizardGameInfoFrame->setVisible(false); WizardGameInfoFrame->Init(GameDB, LinkFile);

    WizardFinishFrame = new TWizardFinishFrame(FrameHost); L->addWidget(WizardFinishFrame);
    WizardFinishFrame->setVisible(false); WizardFinishFrame->Init(GameDB);

    WizardScummVMFrame = new TWizardScummVMFrame(FrameHost); L->addWidget(WizardScummVMFrame);
    WizardScummVMFrame->setVisible(false); WizardScummVMFrame->Init(GameDB);

    WizardScummVMSettingsFrame = new TWizardScummVMSettingsFrame(FrameHost); L->addWidget(WizardScummVMSettingsFrame);
    WizardScummVMSettingsFrame->setVisible(false); WizardScummVMSettingsFrame->Init(GameDB);

    // TabOrder: frames 0..6, BottomPanel 7
    QWidget::setTabOrder(WizardBaseFrame, WizardPrgFileFrame);
    QWidget::setTabOrder(WizardPrgFileFrame, WizardTemplateFrame);
    QWidget::setTabOrder(WizardTemplateFrame, WizardGameInfoFrame);
    QWidget::setTabOrder(WizardGameInfoFrame, WizardFinishFrame);
    QWidget::setTabOrder(WizardFinishFrame, WizardScummVMFrame);
    QWidget::setTabOrder(WizardScummVMFrame, WizardScummVMSettingsFrame);
    QWidget::setTabOrder(WizardScummVMSettingsFrame, ui.PreviousButton);

    if (!Trim(ExeFile).isEmpty()) {
        WizardPrgFileFrame->ui.ProgramEdit->setText(MakeRelPath(ExeFile, PrgSetup->BaseDir()));
        if (WindowsExe) WizardBaseFrame->SetEmulationType(-3);
        WizardBaseFrame->ui.EmulationTypeComboBox->setEnabled(false);
        WizardBaseFrame->ui.ListScummGamesLabel->setVisible(false);
    } else {
        WizardBaseFrame->ui.EmulationTypeComboBox->setCurrentIndex(0);
    }

    SetActivePage(0);
}

void TWizardForm::FormHide(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (WizardTemplateFrame != nullptr) WizardTemplateFrame->Done();
    if (WizardFinishFrame != nullptr) WizardFinishFrame->Done();
}

void TWizardForm::SetActivePage(int NewPage)
{
    QString S;

    switch (ActivePage) {
    case 0:
        // Start page
        ScummVM = (WizardBaseFrame->EmulationType() == -2);
        WindowsMode = (WizardBaseFrame->EmulationType() == -3) || (WizardBaseFrame->EmulationType() >= 0);
        if (ScummVM) WizardScummVMFrame->ShowScummVMFrame();
        WizardMode = WizardBaseFrame->ui.WizardModeComboBox->currentIndex();
        break;
    case 1:
        if (NewPage == 2) {
            // Progam file page -> next page (template or ScummVM settings)
            if (!ScummVM) {
                if (Trim(WizardPrgFileFrame->ui.ProgramEdit->text()).isEmpty()) {
                    if (MessageDlg(LanguageSetup->MessageNoGameFileNameWarning(), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
                }
                if (WindowsMode) {
                    if (WizardBaseFrame->EmulationType() < 0) {
                        if (!Trim(WizardPrgFileFrame->ui.ProgramEdit->text()).isEmpty()) {
                            S = MakeAbsPath(WizardPrgFileFrame->ui.ProgramEdit->text(), PrgSetup->BaseDir());
                            if (IsDOSExe(S)) {
                                if (MessageDlg(Format(LanguageSetup->MessageDOSExeEditWarning(), {S}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
                            }
                        }
                        if (!Trim(WizardPrgFileFrame->ui.SetupEdit->text()).isEmpty()) {
                            S = MakeAbsPath(WizardPrgFileFrame->ui.SetupEdit->text(), PrgSetup->BaseDir());
                            if (IsDOSExe(S)) {
                                if (MessageDlg(Format(LanguageSetup->MessageDOSExeEditWarning(), {S}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
                            }
                        }
                    }
                    WizardGameInfoFrame->SetGameName(CalcProfileName(), nullptr);
                    NewPage = 3;
                    if (WizardMode < 2) DoOKButtonClick = true;
                } else {
                    if (!DoubleProfileCheck(Trim(WizardPrgFileFrame->ui.ProgramEdit->text()))) return;
                    if (!Trim(WizardPrgFileFrame->ui.ProgramEdit->text()).isEmpty()) {
                        S = MakeAbsPath(WizardPrgFileFrame->ui.ProgramEdit->text(), PrgSetup->BaseDir());
                        if (IsWindowsExe(S)) {
                            if (MessageDlg(Format(LanguageSetup->MessageWindowsExeEditWarning(), {S}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
                        }
                    }
                    if (!Trim(WizardPrgFileFrame->ui.SetupEdit->text()).isEmpty()) {
                        S = MakeAbsPath(WizardPrgFileFrame->ui.SetupEdit->text(), PrgSetup->BaseDir());
                        if (IsWindowsExe(S)) {
                            if (MessageDlg(Format(LanguageSetup->MessageWindowsExeEditWarning(), {S}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
                        }
                    }
                    WizardTemplateFrame->SearchTemplates(WizardPrgFileFrame->ui.ProgramEdit->text());

                    if ((WizardMode == 0) || ((WizardTemplateFrame->ui.TemplateType1List->count() > 0) && (WizardMode == 1))) {
                        if (WizardTemplateFrame->ui.TemplateType1List->count() > 0) {
                            WizardTemplateFrame->ui.TemplateType1List->setCurrentIndex(0);
                            WizardTemplateFrame->ui.TemplateType1->setChecked(true);
                        }
                        DoOKButtonClick = true;
                    }
                }
            } else {
                if (!DoubleProfileCheck(Trim(WizardScummVMFrame->ui.ProgramEdit->text()))) return;
                if (Trim(WizardScummVMFrame->ui.ProgramEdit->text()).isEmpty()) {
                    if (MessageDlg(LanguageSetup->MessageNoGameFileNameWarning(), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
                }
                WizardScummVMSettingsFrame->SetScummVMGameName(WizardScummVMFrame->GetScummVMGameName(), WizardScummVMFrame->ui.ProgramEdit->text());
            }
        }
        break;
    case 2:
        if (NewPage == 3) {
            // Template or ScummVM settings page -> game info page
            if (!ScummVM) {
                InsecureTemplate = !WizardTemplateFrame->SelectedProfileSecure();
                if (InsecureTemplate) {
                    if (MessageDlg(LanguageSetup->MessageTemplateInsecureWarning(), mtWarning, mbYes | mbNo, 0) != mrYes) return;
                }
            }
            WizardGameInfoFrame->SetGameName(CalcProfileName(), WizardTemplateFrame->GetTemplate());
        }
        break;
    case 3:
        if (NewPage == 4) {
            // Game info page -> mount page
            WizardFinishFrame->SetInsecureStatus(InsecureTemplate);
            WizardFinishFrame->LoadData(WizardTemplateFrame->GetTemplate(), WizardPrgFileFrame->ui.ProgramEdit->text(), WizardPrgFileFrame->ui.SetupEdit->text(), WizardGameInfoFrame->ui.BaseName->text());
        }
        break;
    }

    if ((NewPage == 2) && WindowsMode) NewPage = 1;

    WizardBaseFrame->setVisible(NewPage == 0);
    WizardPrgFileFrame->setVisible((NewPage == 1) && (!ScummVM));
    if ((NewPage == 1) && (!ScummVM)) {
        WizardPrgFileFrame->ui.GamesFolderEdit->setVisible(!WindowsMode);
        WizardPrgFileFrame->ui.GamesFolderEdit_EditLabel->setVisible(!WindowsMode);
        WizardPrgFileFrame->ui.GamesFolderButton->setVisible(!WindowsMode);
        WizardPrgFileFrame->ui.FolderInfoButton->setVisible(!WindowsMode);
        WizardPrgFileFrame->OtherEmulator = WizardBaseFrame->EmulationType();
        if (WindowsMode)
            WizardPrgFileFrame->ui.InfoLabel->setText(LanguageSetup->WizardFormPage2InfoWindows());
        else
            WizardPrgFileFrame->ui.InfoLabel->setText(LanguageSetup->WizardFormPage2Info());
    }
    WizardScummVMFrame->setVisible((NewPage == 1) && ScummVM);
    WizardTemplateFrame->setVisible((NewPage == 2) && (!ScummVM));
    WizardScummVMSettingsFrame->setVisible((NewPage == 2) && ScummVM);
    WizardGameInfoFrame->setVisible(NewPage == 3);
    WizardFinishFrame->setVisible(NewPage == 4);

    ActivePage = NewPage;

    ui.PreviousButton->setEnabled(ActivePage > 0);
    if (ScummVM) {
        ui.NextButton->setEnabled(ActivePage < 3);
        ui.OKButton->setEnabled(ActivePage >= 2);
    } else {
        if (WindowsMode) {
            ui.NextButton->setEnabled(ActivePage < 3);
            ui.OKButton->setEnabled(ActivePage >= 1);
        } else {
            ui.NextButton->setEnabled(ActivePage < 4);
            ui.OKButton->setEnabled(ActivePage >= 2);
        }
    }
}

void TWizardForm::StepButtonWork(QObject* Sender)
{
    switch (Sender->property("tag").toInt()) {
    case 0: SetActivePage(ActivePage - 1); break;
    case 1: SetActivePage(ActivePage + 1); break;
    }
    if (DoOKButtonClick) {
        OKButtonClick(Sender);
        setModalResult(mrOk);
    }

    RedrawFrameIfVisible(WizardBaseFrame);
    RedrawFrameIfVisible(WizardPrgFileFrame);
    RedrawFrameIfVisible(WizardTemplateFrame);
    RedrawFrameIfVisible(WizardGameInfoFrame);
    RedrawFrameIfVisible(WizardFinishFrame);
    RedrawFrameIfVisible(WizardScummVMFrame);
    RedrawFrameIfVisible(WizardScummVMSettingsFrame);
}

void TWizardForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (ui.NextButton->isEnabled() && (!ScummVM) && (!WindowsMode)) {
        if (ActivePage == 2) {
            // Template page -> game info page
            InsecureTemplate = !WizardTemplateFrame->SelectedProfileSecure();
            if (InsecureTemplate) {
                if (MessageDlg(LanguageSetup->MessageTemplateInsecureWarning(), mtWarning, mbYes | mbNo, 0) != mrYes) { setModalResult(mrNone); return; }
            }
            WizardGameInfoFrame->SetGameName(CalcProfileName(), WizardTemplateFrame->GetTemplate());
        }
        // Game info page -> mount page
        WizardFinishFrame->SetInsecureStatus(InsecureTemplate);
        WizardFinishFrame->LoadData(WizardTemplateFrame->GetTemplate(), WizardPrgFileFrame->ui.ProgramEdit->text(), WizardPrgFileFrame->ui.SetupEdit->text(), WizardGameInfoFrame->ui.BaseName->text());
    }

    if (ScummVM) {
        S = WizardGameInfoFrame->ui.BaseName->text();
        if (S.isEmpty()) S = CalcProfileName();
        Game = WizardScummVMSettingsFrame->CreateGame(S);
    } else {
        S = WizardGameInfoFrame->ui.BaseName->text();
        if (S.isEmpty()) S = CalcProfileName();
        if (WindowsMode)
            Game = WizardTemplateFrame->CreateWindowsGame(S);
        else
            Game = WizardTemplateFrame->CreateGame(S);
    }
    WizardBaseFrame->WriteDataToGame(Game);
    if (ScummVM) {
        WizardScummVMFrame->WriteDataToGame(Game);
    } else {
        WizardPrgFileFrame->WriteDataToGame(Game);
    }
    if (Trim(Game->Name()).isEmpty() && (WizardScummVMFrame->ui.GameNameComboBox->currentIndex() >= 0)) {
        Game->SetName(CalcProfileName());
    }
    WizardGameInfoFrame->WriteDataToGame(Game);

    WizardFinishFrame->WriteDataToGame(Game);
    OpenEditorNow = WizardFinishFrame->ui.ProfileEditorCheckBox->isChecked();
    if (!ScummVM) {
        CreateGameCheckSum(Game, true);
        CreateSetupCheckSum(Game, true);
    }
    Game->LoadCache();
    Game->StoreAllValues();
}

bool TWizardForm::DoubleProfileCheck(const QString& GameFileName)
{
    bool result = true;
    if (GameFileName.isEmpty()) return result;
    QString S = ExtUpperCase(MakeRelPath(GameFileName, PrgSetup->BaseDir()));
    for (int I = 0; I < GameDB->Count(); I++) if (!WindowsExeMode(GameDB->Game(I))) {
        QString T = Trim(GameDB->Game(I)->GameExe());
        if (T.isEmpty()) continue;
        if (S == ExtUpperCase(MakeRelPath(T, PrgSetup->BaseDir()))) {
            if (MessageDlg(Format(LanguageSetup->WizardFormDoubleProfile(), {GameDB->Game(I)->Name()}), mtWarning, mbYes | mbNo, 0) != mrYes) { result = false; return result; }
            break;
        }
    }
    return result;
}

void TWizardForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ProfileWithWizard);
}

void TWizardForm::FormKeyDown(QObject* Sender, quint16& Key, TShiftState Shift)
{
    if ((Key == VK_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

// global

bool ShowWizardDialog(QWidget* AOwner, TGameDB* AGameDB, const QString& AExeFile, bool AWindowsExe, TGame*& AGame, bool& OpenEditorNow, bool& OpenInstallationSupport, TLinkFile* ASearchLinkFile)
{
    bool result;
    OpenEditorNow = false;
    OpenInstallationSupport = false;

    WizardForm = new TWizardForm(AOwner);
    WizardForm->GameDB = AGameDB;
    WizardForm->LinkFile = ASearchLinkFile;
    WizardForm->ExeFile = AExeFile;
    WizardForm->WindowsExe = AWindowsExe;
    result = (WizardForm->ShowModal() == mrOk);
    if ((!result) && WizardForm->OpenInstallationSupportDialog) {
        OpenInstallationSupport = true;
    }
    if (result) {
        AGame = WizardForm->Game;
        OpenEditorNow = WizardForm->OpenEditorNow;
    }
    delete WizardForm;
    WizardForm = nullptr;
    return result;
}
