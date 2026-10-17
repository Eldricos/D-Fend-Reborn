// C++ port of ModernProfileEditorFormUnit.pas (implementation section).
#include "ModernProfileEditorFormUnit.h"

#include <typeinfo>

#include "LanguageSetupUnit.h"
#include "DosBoxUnit.h"
#include "DOSBoxConfUnit.h"
#include "ModernProfileEditorBaseFrameUnit.h"
#include "ModernProfileEditorGameInfoFrameUnit.h"
#include "ModernProfileEditorDirectoryFrameUnit.h"
#include "ModernProfileEditorDOSBoxFrameUnit.h"
#include "ModernProfileEditorHardwareFrameUnit.h"
#include "ModernProfileEditorCPUFrameUnit.h"
#include "ModernProfileEditorMemoryFrameUnit.h"
#include "ModernProfileEditorGraphicsFrameUnit.h"
#include "ModernProfileEditorKeyboardFrameUnit.h"
#include "ModernProfileEditorMouseFrameUnit.h"
#include "ModernProfileEditorSoundFrameUnit.h"
#include "ModernProfileEditorVolumeFrameUnit.h"
#include "ModernProfileEditorSoundBlasterFrameUnit.h"
#include "ModernProfileEditorGUSFrameUnit.h"
#include "ModernProfileEditorMIDIFrameUnit.h"
#include "ModernProfileEditorJoystickFrameUnit.h"
#include "ModernProfileEditorDrivesFrameUnit.h"
#include "ModernProfileEditorSerialPortsFrameUnit.h"
#include "ModernProfileEditorSerialPortFrameUnit.h"
#include "ModernProfileEditorNetworkFrameUnit.h"
#include "ModernProfileEditorDOSEnvironmentFrameUnit.h"
#include "ModernProfileEditorStartFrameUnit.h"
#include "ModernProfileEditorScummVMGraphicsFrameUnit.h"
#include "ModernProfileEditorScummVMFrameUnit.h"
#include "ModernProfileEditorScummVMSoundFrameUnit.h"
#include "ModernProfileEditorPrinterFrameUnit.h"
#include "ModernProfileEditorStagingFrameUnit.h"
#include "ModernProfileEditorScummVMGameFrameUnit.h"
#include "ModernProfileEditorHelperProgramsFrameUnit.h"
#include "ModernProfileEditorScummVMHardwareFrameUnit.h"
#include "ModernProfileEditorAddtionalChecksumFrameUnit.h"
#include "ModernProfileEditorInnovaFrameUnit.h"
#include "IconLoaderUnit.h"
#include "GameDBToolsUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "DosBoxUnit.h"
#include "PrgConsts.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "SelectAutoSetupFormUnit.h"
#include "ScummVMUnit.h"
#include "WindowsProfileUnit.h"
#include "DOSBoxTempUnit.h"
#include "MainUnit.h"

TModernProfileEditorForm* ModernProfileEditorForm = nullptr;

static int LastPage = 0, LastPageExt = 0, LastTop = 0, LastLeft = 0;

// Frame index stored in TTreeNode.Data
static int NodeData(QTreeWidgetItem* Node)
{
    return Node->data(0, Qt::UserRole).toInt();
}

/* TModernProfileEditorForm */

TModernProfileEditorForm::TModernProfileEditorForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // Non visual DFM components
    // (images from the DFM; the icon set replaces some of them in FormShow)
    ImageList = TImageList::FromResource("ModernProfileEditorFormUnit", "ImageList");
    ToolsImageList = TImageList::FromResource("ModernProfileEditorFormUnit", "ToolsImageList");

    // Frames are aligned alClient inside RightPanel (below TopPanel)
    ui.RightPanel->installEventFilter(this);

    // DFM event wiring
    connect(ui.PreviousButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.PreviousButton); });
    connect(ui.NextButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.NextButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.ToolsButton, &QPushButton::clicked, this, [this]() { ToolsButtonClick(ui.ToolsButton); });
    connect(ui.Tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* Current, QTreeWidgetItem*) { TreeChange(ui.Tree, Current); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return false;
    });

    FormCreate(this);
}

TModernProfileEditorForm::~TModernProfileEditorForm()
{
    // Frames are children of RightPanel and are deleted by Qt
    delete ImageList;
    delete ToolsImageList;
}

void TModernProfileEditorForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    if (E->spontaneous() || FShown) return;
    FShown = true;
    FormShow(this);
}

bool TModernProfileEditorForm::eventFilter(QObject* O, QEvent* E)
{
    if (O == ui.RightPanel && (E->type() == QEvent::Resize || E->type() == QEvent::LayoutRequest)) {
        LayoutFrames();
        QTimer::singleShot(0, this, [this]() { LayoutFrames(); });
    }
    return TVclForm::eventFilter(O, E);
}

void TModernProfileEditorForm::LayoutFrames()
{
    QRect R = ui.RightPanel->contentsRect();
    if (ui.TopPanel->isVisibleTo(ui.RightPanel)) R.setTop(ui.TopPanel->geometry().bottom() + 1);
    for (const TFrameRecord& Rec : FrameList) if (Rec.Frame != nullptr && Rec.Frame->geometry() != R) Rec.Frame->setGeometry(R);
}

QList<QTreeWidgetItem*> TModernProfileEditorForm::TreeItems() const
{
    QList<QTreeWidgetItem*> L;
    for (QTreeWidgetItemIterator It(ui.Tree); *It; ++It) L << *It;
    return L;
}

void TModernProfileEditorForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    LastUsedPageCode = -1;
    NewProfileName = "";
    NewExeFileName = "";
    HideGameInfoPage = false;
    SilentMode = false;
}

QTreeWidgetItem* TModernProfileEditorForm::AddTreeNode(QTreeWidgetItem* ParentTreeNode, QWidget* F, IModernProfileEditorFrame* I, const QString& Name, int PageCode, int ImageIndex)
{
    QTreeWidgetItem* result;
    int C;
    TModernProfileEditorInitData InitData;

    C = FrameList.size();
    F->setVisible(false);
    if (F->parentWidget() != ui.RightPanel) F->setParent(ui.RightPanel);
    F->setProperty("vclAlign", "alClient");
    // Font.Charset / DoubleBuffered: not needed with Qt

    if (ParentTreeNode == nullptr) result = new QTreeWidgetItem(ui.Tree, QStringList{Name});
    else result = new QTreeWidgetItem(ParentTreeNode, QStringList{Name});
    result->setData(0, Qt::UserRole, C);
    result->setData(0, Qt::UserRole + 1, ImageIndex);  // ImageIndex / SelectedIndex

    FrameList.append(TFrameRecord());
    FrameList[C].Frame = F;
    FrameList[C].IFrame = I;
    FrameList[C].ExtPageCode = LastUsedPageCode; LastUsedPageCode++;
    FrameList[C].TreeNode = result;
    FrameList[C].PageCode = PageCode;

    InitData.OnProfileNameChange = [this](QObject* S, const QString& A, const QString& B, const QString& C2, const QString& D, const QString& E, const QString& F2, const QString& G) {
        SetProfileNameEvent(S, A, B, C2, D, E, F2, G);
    };
    InitData.OnResetToDefault = nullptr;
    InitData.OnCheckValue = nullptr;
    InitData.OnShowFrame = nullptr;
    InitData.AllowDefaultValueReset = true;
    InitData.CurrentProfileName = &ProfileName;
    InitData.CurrentProfileExe = &ProfileExe;
    InitData.CurrentProfileSetup = &ProfileSetup;
    InitData.CurrentScummVMGameName = &ProfileScummVMGameName;
    InitData.CurrentScummVMPath = &ProfileScummVMPath;
    InitData.CurrentDOSBoxInstallation = &ProfileDOSBoxInstallation;
    InitData.CurrentCaptureDir = &ProfileCaptureDir;
    InitData.GameDB = GameDB;
    InitData.SearchLinkFile = SearchLinkFile;
    InitData.EditingTemplate = EditingTemplate;
    InitData.GetFrame = [this](TFrameClass FrameClass) { return GetFrame(FrameClass); };
    InitData.OnProfileNameChangedCallback = nullptr;

    I->InitGUI(InitData);
    FrameList[C].ResetToDefault = InitData.OnResetToDefault;
    FrameList[C].CheckValue = InitData.OnCheckValue;
    FrameList[C].ShowFrame = InitData.OnShowFrame;
    FrameList[C].ProfileNameChangedCallback = InitData.OnProfileNameChangedCallback;
    FrameList[C].AllowDefaultValueReset = InitData.AllowDefaultValueReset;

    LayoutFrames();
    return result;
}

void TModernProfileEditorForm::InitToolsMenu()
{
    auto AddThisPageMenuItem = [this](QAction* M) { ResetThisPageMenuItems.append(M); };
    auto NewItem = [this](QMenu* Parent, const QString& Caption, int Tag, int ImageIndex) {
        QAction* M = Parent->addAction(Caption);
        M->setProperty("tag", Tag);
        if (ImageIndex >= 0 && ImageIndex < ToolsImageList->Count()) M->setIcon(ToolsImageList->Icon(ImageIndex));
        connect(M, &QAction::triggered, this, [this, M]() { PopupMenuWork(M); });
        return M;
    };
    QAction* M;
    QMenu *Sub, *Sub2;
    QString S;
    TGameDB* TemplateDB;
    int I;

    UserIconLoader->DialogImage(DI_ViewFile, ToolsImageList, 0);
    UserIconLoader->DialogImage(DI_DOSBox, ToolsImageList, 1);
    UserIconLoader->DialogImage(DI_ScummVM, ToolsImageList, 2);
    UserIconLoader->DialogImage(DI_ExploreFolder, ToolsImageList, 3);

    ui.ToolsPopupMenu->clear();
    ResetThisPageMenuItems.clear();

    /* View DOSBox/ScummVM config file */
    if (!WindowsMode) {
        if (ScummVM) S = LanguageSetup->MenuProfileViewIniFile(); else S = LanguageSetup->MenuProfileViewConfFile();
        while (S != "" && S[Length(S) - 1] == '.') S.chop(1);
        NewItem(ui.ToolsPopupMenu, Trim(S), 0, 0);
    }

    /* Run profile */
    {
        int Img;
        if (WindowsMode) Img = -1; else {
            if (ScummVM) Img = 2; else Img = 1;
        }
        NewItem(ui.ToolsPopupMenu, LanguageSetup->ProfileEditorToolsRun(), 1, Img);
    }

    /* Open games folder */
    NewItem(ui.ToolsPopupMenu, LanguageSetup->MenuProfileOpenFolder(), 6, 3);

    /* Line */
    ui.ToolsPopupMenu->addSeparator();

    /* Reset to default data */
    M = NewItem(ui.ToolsPopupMenu, LanguageSetup->ProfileEditorToolsResetPage(), 2, -1); AddThisPageMenuItem(M);
    M = NewItem(ui.ToolsPopupMenu, LanguageSetup->ProfileEditorToolsResetAllButThis(), 7, -1); AddThisPageMenuItem(M);
    NewItem(ui.ToolsPopupMenu, LanguageSetup->ProfileEditorToolsResetAll(), 3, -1);

    if (ScummVM || WindowsMode) return;

    /* Line */
    ui.ToolsPopupMenu->addSeparator();

    /* Reset to template */
    TemplateDB = new TGameDB(PrgDataDir() + TemplateSubDir, false);
    if (TemplateDB->Count() > 0) {
        Sub = ui.ToolsPopupMenu->addMenu(LanguageSetup->ProfileEditorToolsResetToTemplate());

        for (I = 0; I < TemplateDB->Count(); I++) {
            Sub2 = Sub->addMenu(TemplateDB->Game(I)->Name());

            M = NewItem(Sub2, LanguageSetup->ProfileEditorToolsResetToTemplatePage(), 10000 + I, -1); AddThisPageMenuItem(M);
            M = NewItem(Sub2, LanguageSetup->ProfileEditorToolsResetToTemplateAllButThis(), 30000 + I, -1); AddThisPageMenuItem(M);
            NewItem(Sub2, LanguageSetup->ProfileEditorToolsResetToTemplateAll(), 20000 + I, -1);
        }
    }
    delete TemplateDB;

    Sub = ui.ToolsPopupMenu->addMenu(LanguageSetup->ProfileEditorToolsResetToAutoSetup());
    M = NewItem(Sub, LanguageSetup->ProfileEditorToolsResetToAutoSetupPage(), 4, -1); AddThisPageMenuItem(M);
    NewItem(Sub, LanguageSetup->ProfileEditorToolsResetToAutoSetupAll(), 5, -1);
}

void TModernProfileEditorForm::OpenTempConfigurationFile()
{
    TGame* G;
    QString TempProf;

    TempProf = TempDir() + MakeFileSysOKFolderName(ProfileName);
    G = new TGame(TempProf);
    for (QTreeWidgetItem* N : TreeItems()) FrameList[NodeData(N)].IFrame->GetGame(G);
    G->StoreAllValues(); G->LoadCache(); OpenConfigurationFile(G, DeleteOnExit);
    delete G;
    ExtDeleteFile(TempProf, ftTemp);
}

void TModernProfileEditorForm::RunGameWithCurrentConfig()
{
    TTempGame* TempGame;

    TempGame = new TTempGame();
    for (QTreeWidgetItem* N : TreeItems()) FrameList[NodeData(N)].IFrame->GetGame(TempGame->Game());
    TempGame->Game()->StoreAllValues(); TempGame->Game()->LoadCache();
    if (ScummVMMode(TempGame->Game())) {
        RunScummVMGame(TempGame->Game());
    } else {
        if (WindowsExeMode(TempGame->Game())) {
            RunWindowsGame(TempGame->Game());
        } else {
            RunGame(TempGame->Game(), DFendRebornMainForm->DeleteOnExit);
        }
    }
    Sleep(100);
    delete TempGame;
}

void TModernProfileEditorForm::ResetTo(TGame* Template, TPageResetMode PageResetMode)
{
    int I;

    switch (PageResetMode) {
    case rmThis:
        if (ui.Tree->currentItem() == nullptr) return;
        I = NodeData(ui.Tree->currentItem());
        if (FrameList[I].AllowDefaultValueReset) {
            if (FrameList[I].ResetToDefault) FrameList[I].ResetToDefault(this, Template); else FrameList[I].IFrame->SetGame(Template, true);
        }
        break;
    case rmAll:
        if (MessageDlg(LanguageSetup->ProfileEditorToolsResetAllConfirm(), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
        for (I = 0; I < FrameList.size(); I++) {
            if (!FrameList[I].AllowDefaultValueReset) continue;
            if (FrameList[I].ResetToDefault) FrameList[I].ResetToDefault(this, Template); else FrameList[I].IFrame->SetGame(Template, true);
        }
        break;
    case rmAllButThis:
        if (MessageDlg(LanguageSetup->ProfileEditorToolsResetAllButThisConfirm(), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
        for (I = 0; I < FrameList.size(); I++) {
            if (!FrameList[I].AllowDefaultValueReset) continue;
            if (ui.Tree->currentItem() != nullptr && I == NodeData(ui.Tree->currentItem())) continue;
            if (FrameList[I].ResetToDefault) FrameList[I].ResetToDefault(this, Template); else FrameList[I].IFrame->SetGame(Template, true);
        }
        break;
    }
}

void TModernProfileEditorForm::PopupMenuWork(QObject* Sender)
{
    TGameDB* DB;
    int Nr;
    TGame* G;
    QString S;
    int Tag = Sender->property("tag").toInt();

    switch (Tag) {
    case 0: OpenTempConfigurationFile(); break;
    case 1: RunGameWithCurrentConfig(); break;
    case 2:
        G = new TGame(PrgSetup);
        ResetTo(G, rmThis);
        delete G;
        break;
    case 3:
        G = new TGame(PrgSetup);
        ResetTo(G, rmAll);
        delete G;
        break;
    case 4:
        DB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);
        Nr = ShowSelectAutoSetupDialog(this, DB);
        if (Nr >= 0) ResetTo(DB->Game(Nr), rmThis);
        delete DB;
        break;
    case 5:
        DB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);
        Nr = ShowSelectAutoSetupDialog(this, DB);
        if (Nr >= 0) ResetTo(DB->Game(Nr), rmAll);
        delete DB;
        break;
    case 6:
        if (ScummVM) S = ProfileScummVMPath; else /* Windows or DOSBox mode */ S = ProfileExe;
        if (S == "") S = PrgSetup->GameDir();
        S = ExtractFilePath(MakeAbsPath(S, PrgSetup->BaseDir()));
        OpenFolder(S);
        break;
    case 7:
        G = new TGame(PrgSetup);
        ResetTo(G, rmAllButThis);
        delete G;
        break;
    default:
        if (Tag >= 10000 && Tag <= 19999) {
            DB = new TGameDB(PrgDataDir() + TemplateSubDir, false);
            ResetTo(DB->Game(Tag - 10000), rmThis);
            delete DB;
        } else if (Tag >= 20000 && Tag <= 29999) {
            DB = new TGameDB(PrgDataDir() + TemplateSubDir, false);
            ResetTo(DB->Game(Tag - 20000), rmAll);
            delete DB;
        } else if (Tag >= 30000 && Tag <= 39999) {
            DB = new TGameDB(PrgDataDir() + TemplateSubDir, false);
            ResetTo(DB->Game(Tag - 30000), rmAllButThis);
            delete DB;
        }
        break;
    }
}

void TModernProfileEditorForm::InitGUI()
{
    QWidget* F;
    QTreeWidgetItem *N, *N2;
    QString S;

    // DoubleBuffered, SetVistaFonts, Font.Charset, NoFlicker: not needed with Qt

    LastVisibleFrame = nullptr;
    MoveStatus = 0;

    setWindowTitle(LanguageSetup->ProfileEditor());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.PreviousButton->setText(RemoveUnderline(LanguageSetup->OK()) + " && " + LanguageSetup->Previous());
    ui.NextButton->setText(RemoveUnderline(LanguageSetup->OK()) + " && " + LanguageSetup->Next());

    UserIconLoader->DialogImage(DI_Previous, ui.PreviousButton);
    UserIconLoader->DialogImage(DI_Next, ui.NextButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
    UserIconLoader->DialogImage(DI_Tools, ui.ToolsButton);

    ScummVM = ScummVMMode(Game) || ScummVMMode(LoadTemplate);
    WindowsMode = WindowsExeMode(Game) || WindowsExeMode(LoadTemplate);

    if (ScummVM) S = LanguageSetup->MenuProfileViewIniFile(); else S = LanguageSetup->MenuProfileViewConfFile();
    while (S != "" && S[Length(S) - 1] == '.') S.chop(1);

    ui.ToolsButton->setText(LanguageSetup->ProfileEditorTools());
    InitToolsMenu();

    QWidget* P = ui.RightPanel;
    {
        const QSignalBlocker Blocker(ui.Tree);  // Tree.Items.BeginUpdate
        if (ScummVM) {
            /* ScummVM mode */
            F = new TModernProfileEditorBaseFrame(P); N = AddTreeNode(nullptr, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorProfileSettingsSheet(), 0, 0);
            BaseFrame = F;
            if (!HideGameInfoPage) { F = new TModernProfileEditorGameInfoFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorGameInfoSheet(), 1, 1); }
            F = new TModernProfileEditorDirectoryFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorGameDirectorySheet(), 0, 8);

            F = new TModernProfileEditorHelperProgramsFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorHelperProgramsSheet(), 0, 0);
            F = new TModernProfileEditorScummVMFrame(P); N = AddTreeNode(nullptr, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorScummVMSheet(), 2, 23);

            F = new TModernProfileEditorScummVMGameFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorScummVMGameSheet(), 6, 23);
            F = new TModernProfileEditorScummVMHardwareFrame(P); N = AddTreeNode(nullptr, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorHardwareSheet(), 3, 3);
            F = new TModernProfileEditorScummVMGraphicsFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorGraphicsSheet(), 4, 11);
            F = new TModernProfileEditorScummVMSoundFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorSoundSheet(), 5, 5);
        } else {
            if (WindowsMode) {
                /* Windows mode */
                F = new TModernProfileEditorBaseFrame(P); N = AddTreeNode(nullptr, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorProfileSettingsSheet(), 0, 0);
                BaseFrame = F;
                if (EditingTemplate) { F = new TModernProfileEditorAddtionalChecksumFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorAdditionalChecksums(), 0, 0); }
                if (!HideGameInfoPage) { F = new TModernProfileEditorGameInfoFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorGameInfoSheet(), 1, 1); }
                F = new TModernProfileEditorDirectoryFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorGameDirectorySheet(), 0, 8);
                F = new TModernProfileEditorHelperProgramsFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorHelperProgramsSheet(), 0, 0);
            } else {
                /* DOSBox mode */
                F = new TModernProfileEditorBaseFrame(P); N = AddTreeNode(nullptr, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorProfileSettingsSheet(), 0, 0);
                BaseFrame = F;
                if (EditingTemplate) { F = new TModernProfileEditorAddtionalChecksumFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorAdditionalChecksums(), 0, 0); }
                if (!HideGameInfoPage) { F = new TModernProfileEditorGameInfoFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorGameInfoSheet(), 1, 1); }
                F = new TModernProfileEditorDirectoryFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorGameDirectorySheet(), 0, 8);
                F = new TModernProfileEditorHelperProgramsFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorHelperProgramsSheet(), 0, 0);
                F = new TModernProfileEditorDOSBoxFrame(P); N2 = AddTreeNode(nullptr, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorGeneralSheet(), 2, 2);
                { auto* SF = new TModernProfileEditorStagingFrame(P); AddTreeNode(N2, SF, SF, QStringLiteral("DOSBox Staging"), 2, 2); }
                F = new TModernProfileEditorHardwareFrame(P); N = AddTreeNode(nullptr, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorHardwareSheet(), 3, 3);
                F = new TModernProfileEditorCPUFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorCPUSheet(), 3, 9);
                F = new TModernProfileEditorMemoryFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorMemorySheet(), 4, 10);
                F = new TModernProfileEditorGraphicsFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorGraphicsSheet(), 4, 11);
                F = new TModernProfileEditorKeyboardFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorKeyboardSheet(), 4, 15);
                F = new TModernProfileEditorMouseFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorMouseSheet(), 4, 12);
                F = new TModernProfileEditorSoundFrame(P); N2 = AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorSoundSheet(), 5, 5);
                F = new TModernProfileEditorVolumeFrame(P); AddTreeNode(N2, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorSoundVolumeSheet(), 5, 18);
                F = new TModernProfileEditorSoundBlasterFrame(P); AddTreeNode(N2, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorSoundSoundBlaster(), 5, 19);
                F = new TModernProfileEditorGUSFrame(P); AddTreeNode(N2, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorSoundGUS(), 5, 20);
                F = new TModernProfileEditorMIDIFrame(P); AddTreeNode(N2, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorSoundMIDI(), 5, 21);
                if (PrgSetup->AllowInnova()) {
                    F = new TModernProfileEditorInnovaFrame(P); AddTreeNode(N2, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorSoundInnova(), 5, 5);
                }
                F = new TModernProfileEditorJoystickFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorSoundJoystick(), 5, 16);
                F = new TModernProfileEditorDrivesFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorMountingSheet(), 4, 4);
                F = new TModernProfileEditorSerialPortsFrame(P); N2 = AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorSerialPortsSheet(), 3, 13);
                for (int Port = 1; Port <= 4; Port++) {
                    auto* SP = new TModernProfileEditorSerialPortFrame(P); SP->PortNr = Port;
                    AddTreeNode(N2, SP, SP, LanguageSetup->GameSerial() + " " + IntToStr(Port), 3, 13);
                }
                F = new TModernProfileEditorNetworkFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorNetworkSheet(), 3, 14);
                if (PrgSetup->AllowPrinterSettings()) {
                    F = new TModernProfileEditorPrinterFrame(P); AddTreeNode(N, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorPrinterSheet(), 3, 17);
                }
                F = new TModernProfileEditorDOSEnvironmentFrame(P); AddTreeNode(nullptr, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorDOSEnvironmentSheet(), 7, 22);
                F = new TModernProfileEditorStartFrame(P); AddTreeNode(nullptr, F, dynamic_cast<IModernProfileEditorFrame*>(F), LanguageSetup->ProfileEditorStartingSheet(), 6, 6);
                StartFrame = F;
            }
        }
    }

    ui.Tree->expandAll();

    /* Page names of long translations or larger fonts: the tree (and the
       window) get wider instead of cutting off the names */
    VclLayout::fitTreeWidth(ui.Tree);
    {
        // the window gets larger if a page needs more space
        QList<QWidget*> Pages;
        for (const TFrameRecord& Rec : FrameList) Pages << Rec.Frame;
        LayoutFrames();
        VclLayout::fitPages(ui.RightPanel, ui.TopPanel->isVisibleTo(ui.RightPanel) ? ui.TopPanel->geometry().bottom() + 1 : 0, Pages);
    }

    if (ui.Tree->topLevelItemCount() > 0) {
        ui.Tree->setCurrentItem(ui.Tree->topLevelItem(0));
        ui.Tree->scrollToItem(ui.Tree->currentItem());
    }

    /* Fix height for smaller screens */
    QScreen* Scr = screen();
    if (Scr != nullptr) {
        int WorkAreaHeight = Scr->availableGeometry().height();
        if (height() > WorkAreaHeight) resize(width(), WorkAreaHeight);
        if (y() + frameGeometry().height() > WorkAreaHeight) move(x(), qMax(0, WorkAreaHeight - frameGeometry().height()));
    }
}

void TModernProfileEditorForm::FormShow(QObject* Sender)
{
    int I;

    InitGUI();

    UserIconLoader->DirectLoad(ImageList, "ModernProfileEditor");
    for (QTreeWidgetItem* N : TreeItems()) {
        int Img = N->data(0, Qt::UserRole + 1).toInt();
        if (Img >= 0 && Img < ImageList->Count()) N->setIcon(0, ImageList->Icon(Img));
    }

    if (Game == nullptr && LoadTemplate != nullptr) {
        Game = LoadTemplate;
        LoadData();
        Game = nullptr;

        if (Trim(NewProfileName) != "") ProfileName = NewProfileName; else ProfileName = "";
        if (Trim(NewExeFileName) != "") {
            if (Trim(NewProfileName) == "") ProfileName = ChangeFileExt(ExtractFileName(NewExeFileName), "");
            ProfileExe = NewExeFileName;
        } else {
            ProfileExe = "";
        }
        ProfileSetup = "";
        ProfileScummVMGameName = "";
        ProfileScummVMPath = "";
        ProfileDOSBoxInstallation = "";
        ProfileCaptureDir = "";
    } else {
        LoadData();
        ProfileEditorOpenCheck(Game);
        if (Trim(NewProfileName) != "") {
            if (Trim(Game->Name()) == "") ProfileName = NewProfileName; else ProfileName = Game->Name();
        }
        if (Trim(NewExeFileName) != "") {
            if (Trim(NewProfileName) == "") {
                if (Trim(Game->Name()) == "") ProfileName = ChangeFileExt(ExtractFileName(NewExeFileName), ""); else ProfileName = Game->Name();
            }
            ProfileExe = NewExeFileName;
        } else {
            if (Trim(NewProfileName) == "") ProfileName = Game->Name();
            ProfileExe = Game->GameExe();
        }
        ProfileSetup = Game->SetupExe();
        ProfileScummVMGameName = Game->ScummVMGame();
        ProfileScummVMPath = Game->ScummVMSavePath();
        ProfileDOSBoxInstallation = Game->CustomDOSBoxDir();
        ProfileCaptureDir = Game->CaptureFolder();
    }

    if (Trim(NewExeFileName) != "" || Trim(NewProfileName) != "") {
        static_cast<TModernProfileEditorBaseFrame*>(BaseFrame)->ui.ProfileNameEdit->setText(ProfileName);
        static_cast<TModernProfileEditorBaseFrame*>(BaseFrame)->ui.GameExeEdit->setText(ProfileExe);
    }

    SetProfileNameEvent(this, ProfileName, ProfileExe, ProfileSetup, ProfileScummVMGameName, ProfileScummVMPath, ProfileDOSBoxInstallation, ProfileCaptureDir);

    if (RestoreLastPosition) {
        if (LastPageExt >= 0) {
            for (I = 0; I < FrameList.size(); I++) if (FrameList[I].ExtPageCode == LastPageExt) {
                ui.Tree->setCurrentItem(FrameList[I].TreeNode);
                break;
            }
        } else {
            for (I = 0; I < FrameList.size(); I++) if (FrameList[I].PageCode == LastPage) {
                ui.Tree->setCurrentItem(FrameList[I].TreeNode);
                break;
            }
        }
        if (LastTop > 0) move(x(), LastTop);
        if (LastLeft > 0) move(LastLeft, y());
    } else {
        if (PrgSetup->ReopenLastProfileEditorTab() && Game != nullptr) {
            if (Game->LastOpenTabModern() >= 0) {
                for (I = 0; I < FrameList.size(); I++) if (FrameList[I].ExtPageCode == Game->LastOpenTabModern()) {
                    ui.Tree->setCurrentItem(FrameList[I].TreeNode);
                    break;
                }
            } else {
                for (I = 0; I < FrameList.size(); I++) if (FrameList[I].PageCode == Game->LastOpenTab()) {
                    ui.Tree->setCurrentItem(FrameList[I].TreeNode);
                    break;
                }
            }
        }
    }
    TreeChange(Sender, ui.Tree->currentItem());
}

void TModernProfileEditorForm::LoadData()
{
    for (QTreeWidgetItem* N : TreeItems()) FrameList[NodeData(N)].IFrame->SetGame(Game, LoadTemplate != nullptr);
}

void TModernProfileEditorForm::SetProfileNameEvent(QObject* Sender, const QString& AProfileName, const QString& AProfileExe, const QString& AProfileSetup, const QString& AProfileScummVMGameName, const QString& AProfileScummVMPath, const QString& AProfileDOSBoxInstallation, const QString& AProfileCaptureDir)
{
    Q_UNUSED(Sender);
    QString S;
    bool B;
    int I;

    B = (ProfileName != AProfileName);
    ProfileName = AProfileName;
    if (B) for (I = 0; I < FrameList.size(); I++) if (FrameList[I].ProfileNameChangedCallback) FrameList[I].ProfileNameChangedCallback(this);
    ProfileExe = AProfileExe;
    ProfileSetup = AProfileSetup;
    ProfileScummVMGameName = AProfileScummVMGameName;
    ProfileScummVMPath = AProfileScummVMPath;
    ProfileDOSBoxInstallation = AProfileDOSBoxInstallation;
    ProfileCaptureDir = AProfileCaptureDir;

    if (Trim(ProfileName) == "") {
        if (EditingTemplate && HideGameInfoPage) {
            if (ScummVMMode(Game))
                S = LanguageSetup->TemplateFormDefaultScummVM();
            else
                S = LanguageSetup->TemplateFormDefault();
        } else {
            S = LanguageSetup->NotSet();
        }
    } else {
        S = ProfileName;
    }
    if (S == LanguageSetup->ProfileEditorNoFilename() || S == LanguageSetup->NotSet())
        setWindowTitle(LanguageSetup->ProfileEditor());
    else
        setWindowTitle(LanguageSetup->ProfileEditor() + " [" + S + "]");
}

void TModernProfileEditorForm::ToolsButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QPoint P;
    bool B;
    int I;

    if (ui.Tree->currentItem() == nullptr) B = false; else B = FrameList[NodeData(ui.Tree->currentItem())].AllowDefaultValueReset;
    for (I = 0; I < ResetThisPageMenuItems.size(); I++) ResetThisPageMenuItems[I]->setEnabled(B);

    P = ui.BottomPanel->mapToGlobal(QPoint(ui.ToolsButton->x(), ui.ToolsButton->y()));
    ui.ToolsPopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
}

void TModernProfileEditorForm::TreeChange(QObject* Sender, QTreeWidgetItem* Node)
{
    Q_UNUSED(Sender);
    Q_UNUSED(Node);
    if (LastVisibleFrame != nullptr) {
        LastVisibleFrame->setVisible(false);
        LastVisibleFrame = nullptr;
    }

    QTreeWidgetItem* Sel = ui.Tree->currentItem();
    if (Sel != nullptr) {
        int I = NodeData(Sel);
        if (I < 0 || I >= FrameList.size()) return;
        LastVisibleFrame = FrameList[I].Frame;
        LayoutFrames();
        LastVisibleFrame->setVisible(true);
        LastVisibleFrame->raise();
        if (FrameList[I].ShowFrame) FrameList[I].ShowFrame(this);
        MarkIgnoredDOSBoxSettings(LastVisibleFrame, ProfileDOSBoxInstallation);
        ui.TopPanel_Caption->setText("  " + Sel->text(0));
    }
}

void TModernProfileEditorForm::OKButtonClick(QObject* Sender)
{
    int I;
    QString S, T;
    TStringList* St;
    bool B;
    const QList<QTreeWidgetItem*> Items = TreeItems();
    auto* BF = static_cast<TModernProfileEditorBaseFrame*>(BaseFrame);

    for (I = 0; I < Items.size(); I++) {
        if (!FrameList[NodeData(Items[I])].CheckValue) continue;
        B = true;
        FrameList[NodeData(Items[I])].CheckValue(this, B);
        if (!B) { ui.Tree->setCurrentItem(Items[I]); setModalResult(mrNone); return; }
    }

    if (!ScummVM) {
        if ((!SilentMode) && (!EditingTemplate) && LoadTemplate != nullptr && Trim(BF->ui.GameExeEdit->text()) == "" && (WindowsMode || static_cast<TModernProfileEditorStartFrame*>(StartFrame)->ui.AutoexecBootNormal->isChecked())) {
            if (MessageDlg(LanguageSetup->MessageNoGameFileNameWarning(), mtConfirmation, mbYes | mbNo, 0) != mrYes) {
                ui.Tree->setCurrentItem(Items[0]);
                setModalResult(mrNone);
                return;
            }
        }

        if (WindowsMode) {
            if ((!SilentMode) && (!EditingTemplate) && Trim(BF->ui.GameExeEdit->text()) != "") {
                S = MakeAbsPath(BF->ui.GameExeEdit->text(), PrgSetup->BaseDir());
                if (IsDOSExe(S)) {
                    if (MessageDlg(Format(LanguageSetup->MessageDOSExeEditWarning(), {S}), mtConfirmation, mbYes | mbNo, 0) != mrYes) {
                        ui.Tree->setCurrentItem(Items[0]);
                        setModalResult(mrNone);
                        return;
                    }
                }
                if ((!SilentMode) && Trim(BF->ui.SetupExeEdit->text()) != "") {
                    S = MakeAbsPath(BF->ui.SetupExeEdit->text(), PrgSetup->BaseDir());
                    if (IsDOSExe(S)) {
                        if (MessageDlg(Format(LanguageSetup->MessageDOSExeEditWarning(), {S}), mtConfirmation, mbYes | mbNo, 0) != mrYes) {
                            ui.Tree->setCurrentItem(Items[0]);
                            setModalResult(mrNone);
                            return;
                        }
                    }
                }
            }
        } else {
            if ((!SilentMode) && (!EditingTemplate) && static_cast<TModernProfileEditorStartFrame*>(StartFrame)->ui.AutoexecBootNormal->isChecked()) {
                if ((!BF->ui.GameRelPathCheckBox->isChecked()) && Trim(BF->ui.GameExeEdit->text()) != "") {
                    S = MakeAbsPath(BF->ui.GameExeEdit->text(), PrgSetup->BaseDir());
                    if (IsWindowsExe(S) && (!BF->ui.IgnoreWindowsWarningsCheckBox->isChecked())) {
                        if (MessageDlg(Format(LanguageSetup->MessageWindowsExeEditWarning(), {S}), mtConfirmation, mbYes | mbNo, 0) != mrYes) {
                            ui.Tree->setCurrentItem(Items[0]);
                            setModalResult(mrNone);
                            return;
                        }
                    }
                }
                if ((!SilentMode) && (!BF->ui.SetupRelPathCheckBox->isChecked()) && Trim(BF->ui.SetupExeEdit->text()) != "") {
                    S = MakeAbsPath(BF->ui.SetupExeEdit->text(), PrgSetup->BaseDir());
                    if (IsWindowsExe(S) && (!BF->ui.IgnoreWindowsWarningsCheckBox->isChecked())) {
                        if (MessageDlg(Format(LanguageSetup->MessageWindowsExeEditWarning(), {S}), mtConfirmation, mbYes | mbNo, 0) != mrYes) {
                            ui.Tree->setCurrentItem(Items[0]);
                            setModalResult(mrNone);
                            return;
                        }
                    }
                }
            }
        }
    }

    switch (Sender->property("tag").toInt()) {
    case 0: MoveStatus = 0; break;
    case 1: MoveStatus = -1; break;
    case 2: MoveStatus = 1; break;
    }

    if (Game == nullptr) {
        I = GameDB->Add(ProfileName);
        Game = GameDB->Game(I);
    }

    for (I = 0; I < Items.size(); I++) {
        FrameList[NodeData(Items[I])].IFrame->GetGame(Game);
        ProfileName = Game->Name();
    }

    if ((!WindowsMode) && (!ScummVM) && LoadTemplate != nullptr && PrgSetup->AddMountingDataAutomatically()) {
        St = new TStringList();
        for (I = 0; I < Game->NrOfMounts(); I++) St->Add(Game->Mount(I));
        if (Trim(Game->GameExe()) == "") S = ""; else S = ExtractFilePath(Game->GameExe());
        if (ExtUpperCase(Copy(Trim(S), 1, 7)) == "DOSBOX:") S = "";
        if (Trim(Game->SetupExe()) == "") T = ""; else T = ExtractFilePath(Game->SetupExe());
        if (ExtUpperCase(Copy(Trim(T), 1, 7)) == "DOSBOX:") T = "";
        BuildGameDirMountData(St, S, T);
        Game->SetNrOfMounts(St->Count());
        for (I = 0; I < St->Count(); I++) Game->SetMount(I, (*St)[I]);
        for (I = St->Count(); I <= 9; I++) Game->SetMount(I, "");
        delete St;
    }

    if (!SilentMode) {
        if (ui.Tree->currentItem() != nullptr) {
            LastPage = FrameList[NodeData(ui.Tree->currentItem())].PageCode;
            LastPageExt = FrameList[NodeData(ui.Tree->currentItem())].ExtPageCode;
        } else {
            LastPage = 0;
            LastPageExt = 0;
        }
        Game->SetLastOpenTab(LastPage);
        Game->SetLastOpenTabModern(LastPageExt);
    }

    Game->StoreAllValues();
    Game->LoadCache();

    if ((!WindowsMode) && (!ScummVM) && (!EditingTemplate)) {
        St = new TStringList();
        BuildAutoexec(Game, false, St, true, -1, false, false, false);
        delete St;
    }

    if (Trim(Game->CaptureFolder()) != "") ForceDirectories(MakeAbsPath(Game->CaptureFolder(), PrgSetup->BaseDir()));
}

void TModernProfileEditorForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;

    I = 0;
    if (LastVisibleFrame != nullptr) I = LastVisibleFrame->property("HelpContext").toInt();
    if (I == 0) I = ID_FileEdit;
    ApplicationHelpCommand(HELP_CONTEXT, I);
}

void TModernProfileEditorForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if (Key == Qt::Key_F1 && Shift == Qt::NoModifier) HelpButtonClick(Sender);
}

void TModernProfileEditorForm::ShowConfButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    TGame* G;
    QString TempProf;

    TempProf = TempDir() + MakeFileSysOKFolderName(ProfileName);
    G = new TGame(TempProf);
    for (QTreeWidgetItem* N : TreeItems()) FrameList[NodeData(N)].IFrame->GetGame(G);
    G->StoreAllValues();
    G->LoadCache();
    OpenConfigurationFile(G, DeleteOnExit);
    delete G;
    ExtDeleteFile(TempProf, ftTemp);
}

QWidget* TModernProfileEditorForm::GetFrame(TFrameClass FrameClass)
{
    // "FrameList[I].Frame is FrameClass": TFrameClass is a factory, so a
    // temporary instance is created to determine the class.
    if (!FrameClass) return nullptr;
    QWidget* Probe = FrameClass(nullptr);
    if (Probe == nullptr) return nullptr;
    const std::type_info& Wanted = typeid(*Probe);
    QWidget* result = nullptr;
    for (int I = 0; I < FrameList.size(); I++) if (FrameList[I].Frame != nullptr && typeid(*FrameList[I].Frame) == Wanted) { result = FrameList[I].Frame; break; }
    delete Probe;
    return result;
}

/* global */

int EditGameProfilInt(QWidget* AOwner, TGameDB* AGameDB, TGame*& AGame, TGame* ADefaultGame, TLinkFile* ASearchLinkFile, TStringList* ADeleteOnExit, bool PrevButton, bool NextButton, bool RestorePos, bool EditingTemplate, const QString& ANewProfileName, const QString& ANewExeFile, bool HideGameInfoPage)
{
    int result;
    double D;
    QString S;

    if (AGame != nullptr && Trim(ExtUpperCase(AGame->VideoCard())) == "VGA") {
        S = CheckDOSBoxVersion(GetDOSBoxNr(AGame));
        for (int I = 0; I < S.length(); I++) if (S[I] == ',' || S[I] == '.') S[I] = '.';
        if (!TryStrToFloat(S, D)) D = 0.72;
        if (D > 0.72) { AGame->SetVideoCard("svga_s3"); AGame->StoreAllValues(); }
    }

    ModernProfileEditorForm = new TModernProfileEditorForm(AOwner);

    ModernProfileEditorForm->RestoreLastPosition = RestorePos;
    // Position:=poDesigned is emulated by moving the form in FormShow
    ModernProfileEditorForm->GameDB = AGameDB;
    ModernProfileEditorForm->Game = AGame;
    ModernProfileEditorForm->SearchLinkFile = ASearchLinkFile;
    ModernProfileEditorForm->DeleteOnExit = ADeleteOnExit;
    ModernProfileEditorForm->LoadTemplate = ADefaultGame;
    ModernProfileEditorForm->ui.PreviousButton->setVisible(PrevButton);
    ModernProfileEditorForm->ui.NextButton->setVisible(NextButton);
    ModernProfileEditorForm->EditingTemplate = EditingTemplate;
    if (ANewExeFile != "") ModernProfileEditorForm->NewExeFileName = ANewExeFile;
    if (ANewProfileName != "") ModernProfileEditorForm->NewProfileName = ANewProfileName;
    ModernProfileEditorForm->HideGameInfoPage = HideGameInfoPage;
    if (ModernProfileEditorForm->ShowModal() == mrOk) {
        result = ModernProfileEditorForm->MoveStatus;
        AGame = ModernProfileEditorForm->Game;
        /* LastPage set via OKButtonClick */
        LastTop = ModernProfileEditorForm->y();
        LastLeft = ModernProfileEditorForm->x();
    } else {
        result = -2;
    }
    delete ModernProfileEditorForm;
    ModernProfileEditorForm = nullptr;
    return result;
}

bool ModernEditGameProfil(QWidget* AOwner, TGameDB* AGameDB, TGame*& AGame, TGame* ADefaultGame, TLinkFile* ASearchLinkFile, TStringList* ADeleteOnExit, const QString& ANewProfileName, const QString& ANewExeFile, TList* GameList)
{
    int I, J;
    bool PrevButton, NextButton, RestorePos;
    QString S, T;

    I = 0; RestorePos = false;
    do {
        if (GameList == nullptr) {
            NextButton = false;
            PrevButton = false;
        } else {
            if (I == 1) {
                J = GameList->indexOf(AGame);
                if (J >= 0 && J < GameList->size() - 1) AGame = static_cast<TGame*>((*GameList)[J + 1]);
            }
            if (I == -1) {
                J = GameList->indexOf(AGame);
                if (J > 0) AGame = static_cast<TGame*>((*GameList)[J - 1]);
            }
            J = GameList->indexOf(AGame);
            NextButton = (J >= 0 && J < GameList->size() - 1);
            PrevButton = (J > 0);
        }
        if (GameList == nullptr || GameList->size() == 0) { S = ANewProfileName; T = ANewExeFile; } else { S = ""; T = ""; }
        I = EditGameProfilInt(AOwner, AGameDB, AGame, ADefaultGame, ASearchLinkFile, ADeleteOnExit, PrevButton, NextButton, RestorePos, false, S, T, false);
        RestorePos = true;
    } while (!(I == 0 || I == -2));
    return (I != -2);
}

bool ModernEditGameTemplate(QWidget* AOwner, TGameDB* AGameDB, TGame*& AGame, TGame* ADefaultGame, TLinkFile* ASearchLinkFile, TStringList* ADeleteOnExit, TList* GameList, TList* TemplateType)
{
    int I, J;
    bool PrevButton, NextButton, RestorePos;
    int TemplateMode;

    I = 0; RestorePos = false;
    do {
        if (GameList == nullptr) {
            NextButton = false;
            PrevButton = false;
        } else {
            if (I == 1) {
                J = GameList->indexOf(AGame);
                if (J >= 0 && J < GameList->size() - 1) AGame = static_cast<TGame*>((*GameList)[J + 1]);
            }
            if (I == -1) {
                J = GameList->indexOf(AGame);
                if (J > 0) AGame = static_cast<TGame*>((*GameList)[J - 1]);
            }
            J = GameList->indexOf(AGame);
            NextButton = (J >= 0 && J < GameList->size() - 1);
            PrevButton = (J > 0);
        }

        if (TemplateType != nullptr && GameList != nullptr) {
            TemplateMode = int(reinterpret_cast<intptr_t>((*TemplateType)[GameList->indexOf(AGame)]));
        } else {
            TemplateMode = 0;
        }
        /* TemplateMode: 0=normal template, 1=default template, 2=ScummVM default template */

        if (TemplateMode == 2) AGame->SetProfileMode("ScummVM");
        try {
            I = EditGameProfilInt(AOwner, AGameDB, AGame, ADefaultGame, ASearchLinkFile, ADeleteOnExit, PrevButton, NextButton, RestorePos, true, "", "", TemplateMode != 0);
        } catch (...) {
            if (TemplateMode == 2) AGame->SetProfileMode("DOSBox");
            throw;
        }
        if (TemplateMode == 2) AGame->SetProfileMode("DOSBox");
        RestorePos = true;
    } while (!(I == 0 || I == -2));
    return (I != -2);
}

void AddDefaultValueHint(QComboBox* AComboBox)
{
    QString S;
    int I;

    S = LanguageSetup->ProfileEditorDefaultValueHint();
    if (Length(S) > 80) for (I = 80; I >= 1; I--) if (S[I - 1] == QChar(32)) { S[I - 1] = QChar('\n'); break; }
    AComboBox->setToolTip(S);
    // ShowHint:=True is the default in Qt
}

namespace {

struct TIgnoredSettingWidgets {
    const char* Frame;    // object name of the profile editor page
    const char* Section;  // DOSBox 0.74 setting
    const char* Key;
    QStringList Widgets;  // "*": all widgets of the page
};

void SetDOSBoxSettingIgnored(QWidget* W, bool Ignored, const QString& Hint)
{
    if (W->property("DFRIgnoredSetting").toBool() == Ignored) return;
    if (Ignored) {
        W->setProperty("DFRToolTip", W->toolTip());
        W->setToolTip(Hint);
    } else {
        W->setToolTip(W->property("DFRToolTip").toString());
    }
    W->setEnabled(!Ignored);
    W->setProperty("DFRIgnoredSetting", Ignored);
}

} // namespace

void MarkIgnoredDOSBoxSettings(QWidget* Frame, const QString& DOSBoxInstallation)
{
    static const QList<TIgnoredSettingWidgets> Table = {
        {"ModernProfileEditorGraphicsFrame", "sdl", "fulldouble", {QStringLiteral("DoublebufferingCheckBox")}},
        {"ModernProfileEditorGraphicsFrame", "sdl", "fullresolution", {QStringLiteral("FullscreenResolutionComboBox"), QStringLiteral("FullscreenResolutionLabel")}},
        {"ModernProfileEditorGraphicsFrame", "sdl", "output", {QStringLiteral("RenderComboBox"), QStringLiteral("RenderLabel")}},
        {"ModernProfileEditorGraphicsFrame", "sdl", "pixelshader", {QStringLiteral("PixelShaderComboBox"), QStringLiteral("PixelShaderLabel")}},
        {"ModernProfileEditorGraphicsFrame", "render", "scaler", {QStringLiteral("ScaleComboBox"), QStringLiteral("ScaleLabel")}},
        {"ModernProfileEditorGraphicsFrame", "render", "frameskip", {QStringLiteral("FrameSkipEdit"), QStringLiteral("FrameSkipLabel")}},
        {"ModernProfileEditorGraphicsFrame", "vga", "svgachipset", {QStringLiteral("VGAChipsetComboBox"), QStringLiteral("VGAChipsetLabel")}},
        {"ModernProfileEditorGraphicsFrame", "glide", "glide", {QStringLiteral("GlideEmulationComboBox"), QStringLiteral("GlideEmulationLabel"), QStringLiteral("GlideEmulationPortComboBox"), QStringLiteral("GlideEmulationPortLabel"), QStringLiteral("GlideEmulationLFBComboBox"), QStringLiteral("GlideEmulationLFBLabel")}},
        {"ModernProfileEditorDOSBoxFrame", "sdl", "priority", {QStringLiteral("DOSBoxForegroundPriorityComboBox"), QStringLiteral("DOSBoxForegroundPriorityLabel"), QStringLiteral("DOSBoxBackgroundPriorityComboBox"), QStringLiteral("DOSBoxBackgroundPriorityLabel")}},
        {"ModernProfileEditorKeyboardFrame", "sdl", "usescancodes", {QStringLiteral("UseScancodesCheckBox")}},
        {"ModernProfileEditorNetworkFrame", "ne2000", "realnic", {QStringLiteral("NE2000RealNICEdit"), QStringLiteral("NE2000RealNICEdit_EditLabel")}},
        {"ModernProfileEditorPrinterFrame", "printer", "printer", {QStringLiteral("*")}},
        {"ModernProfileEditorSoundBlasterFrame", "sblaster", "oplemu", {QStringLiteral("OplEmuComboBox"), QStringLiteral("OplEmuLabel")}},
        {"ModernProfileEditorSoundBlasterFrame", "sblaster", "oplrate", {QStringLiteral("OplSampleRateComboBox"), QStringLiteral("OplSampleRateLabel")}},
        {"ModernProfileEditorGUSFrame", "gus", "gusrate", {QStringLiteral("SampleRateComboBox"), QStringLiteral("SampleRateLabel")}},
        {"ModernProfileEditorSoundFrame", "speaker", "pcrate", {QStringLiteral("PCSpeakerSampleRateComboBox"), QStringLiteral("PCSpeakerSampleRateLabel")}},
        {"ModernProfileEditorSoundFrame", "speaker", "tandyrate", {QStringLiteral("TandyComboBox"), QStringLiteral("TandySampleRateLabel")}},
        {"ModernProfileEditorInnovaFrame", "innova", "samplerate", {QStringLiteral("InnovaSampleRateComboBox"), QStringLiteral("InnovaSampleRateLabel")}},
        {"ModernProfileEditorInnovaFrame", "innova", "sidbase", {QStringLiteral("InnovaBaseComboBox"), QStringLiteral("InnovaBaseLabel")}},
        {"ModernProfileEditorInnovaFrame", "innova", "quality", {QStringLiteral("InnovaQualityComboBox"), QStringLiteral("InnovaQualityLabel")}},
        {"ModernProfileEditorStagingFrame", "staging", "*", {QStringLiteral("Staging_graphics"), QStringLiteral("Staging_cpu"), QStringLiteral("Staging_sound"), QStringLiteral("Staging_midi"), QStringLiteral("Staging_mouse")}},
    };

    if (Frame == nullptr) return;
    const QString Name = Frame->objectName();
    bool Found = false;
    for (const TIgnoredSettingWidgets& E : Table) if (Name == QLatin1String(E.Frame)) { Found = true; break; }
    if (!Found) return;

    const TDOSBoxKind Kind = DOSBoxKindOfProfileInstallation(DOSBoxInstallation);
    for (const TIgnoredSettingWidgets& E : Table) {
        if (Name != QLatin1String(E.Frame)) continue;
        // ("staging": settings only DOSBox Staging has)
        const bool StagingOnly = (qstrcmp(E.Section, "staging") == 0);
        const bool Ignored = StagingOnly ? (Kind != dkStaging) : DOSBoxSettingIgnored(Kind, QLatin1String(E.Section), QLatin1String(E.Key));
        const QString Hint = StagingOnly ? LanguageSetup->ProfileEditorStagingOnly() : LanguageSetup->ProfileEditorDOSBoxSettingIgnored();
        for (const QString& W : E.Widgets) {
            if (W == QLatin1String("*")) {
                for (QWidget* C : Frame->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) SetDOSBoxSettingIgnored(C, Ignored, Hint);
            } else if (QWidget* C = Frame->findChild<QWidget*>(W)) {
                SetDOSBoxSettingIgnored(C, Ignored, Hint);
            }
        }
    }
}
