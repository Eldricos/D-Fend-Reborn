// C++ port of SetupFormUnit.pas (implementation section).
#include "SetupFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"
#include "LanguageEditorFormUnit.h"
#include "LanguageEditorStartFormUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "SetupFrameBaseUnit.h"
#include "SetupFrameDirectoriesUnit.h"
#include "SetupFrameSurfaceUnit.h"
#include "SetupFrameLanguageUnit.h"
#include "SetupFrameMenubarUnit.h"
#include "SetupFrameToolbarUnit.h"
#include "SetupFrameGamesListColumnsUnit.h"
#include "SetupFrameGamesListAppearanceUnit.h"
#include "SetupFrameGamesListTreeAppearanceUnit.h"
#include "SetupFrameGamesListScreenshotAppearanceUnit.h"
#include "SetupFrameProfileEditorUnit.h"
#include "SetupFrameDefaultValuesUnit.h"
#include "SetupFrameProgramsUnit.h"
#include "SetupFrameDOSBoxUnit.h"
#include "SetupFrameDOSBoxExtUnit.h"
#include "SetupFrameFreeDOSUnit.h"
#include "SetupFrameScummVMUnit.h"
#include "SetupFrameQBasicUnit.h"
#include "SetupFrameWaveEncoderUnit.h"
#include "SetupFrameSecurityUnit.h"
#include "SetupFrameServiceUnit.h"
#include "SetupFrameUpdateUnit.h"
#include "SetupFrameWineUnit.h"
#include "SetupFrameCompressionUnit.h"
#include "SetupFrameGamesListScreenshotModeAppearanceUnit.h"
#include "SetupFrameEditorUnit.h"
#include "SetupFrameViewerUnit.h"
#include "SetupFrameWindowsGamesUnit.h"
#include "SetupFrameZipPrgsUnit.h"
#include "SetupFrameCustomLanguageStringsUnit.h"
#include "SetupFrameGameListIconModeAppearanceUnit.h"
#include "SetupFrameUserInterpreterFrameUnit.h"
#include "SetupFrameImageScalingUnit.h"
#include "SetupFrameMoreEmulatorsUnit.h"
#include "SetupFrameAutomaticConfigurationUnit.h"
#include "SetupFrameDOSBoxGlobalUnit.h"
#include "SetupFrameDataPrivacyUnit.h"

TSetupForm* SetupForm = nullptr;

// DFendReloadedMainForm (MainUnit.h is not included to keep this unit
// independent of the main form header): the application's QMainWindow.
static QWidget* DFendReloadedMainFormWidget()
{
    for (QWidget *W : QApplication::topLevelWidgets()) if (qobject_cast<QMainWindow*>(W) != nullptr) return W;
    return nullptr;
}

// Application.HelpCommand(HELP_CONTEXT,Context)
static void ApplicationHelpContext(int Context)
{
    // TODO(port): Application.HelpCommand(HELP_CONTEXT,...) has no compat
    // equivalent yet (THTMLhelpRouter::OnRouteHelp is private); show the help
    // contents instead.
    Q_UNUSED(Context);
    if (GLOBAL_HELPROUTER != nullptr) GLOBAL_HELPROUTER->HelpContent();
}

static int FrameHelpContext(QWidget *Frame)
{
    if (Frame == nullptr) return 0;
    return Frame->property("HelpContext").toInt();
}

static int NodeData(QTreeWidgetItem *Node)
{
    return Node->data(0, Qt::UserRole).toInt();
}

TSetupForm::TSetupForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    ImageList = TImageList::FromResource("SetupFormUnit", "ImageList");

    // DFM event wiring
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.ModeComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int) { ModeComboBoxChange(ui.ModeComboBox); });
    connect(ui.RestoreDefaultValuesButton, &QPushButton::clicked, this, [this]() { RestoreDefaultValuesButtonClick(ui.RestoreDefaultValuesButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.Tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *Current, QTreeWidgetItem *) { TreeChange(ui.Tree, Current); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });

    // Frames are hosted in RightPanel below TopPanel (Align=alClient)
    ui.RightPanel->installEventFilter(this);
    ui.TopPanel->installEventFilter(this);

    FormCreate(this);   // OnCreate
}

TSetupForm::~TSetupForm()
{
    delete ImageList;
}

void TSetupForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!FirstShowDone) {
        FirstShowDone = true;
        FormShow(this);   // OnShow
    }
    LayoutFrames();
}

bool TSetupForm::eventFilter(QObject *Watched, QEvent *Event)
{
    if ((Watched == ui.RightPanel || Watched == ui.TopPanel) &&
        (Event->type() == QEvent::Resize || Event->type() == QEvent::Move || Event->type() == QEvent::Show)) {
        LayoutFrames();
    }
    return TVclForm::eventFilter(Watched, Event);
}

void TSetupForm::LayoutFrames()
{
    int Top = ui.TopPanel->isVisible() ? ui.TopPanel->geometry().bottom() + 1 : 0;
    QRect R(0, Top, ui.RightPanel->width(), qMax(0, ui.RightPanel->height() - Top));
    for (const TSetupFrameRecord &Rec : Frames) if (Rec.Frame->geometry() != R) Rec.Frame->setGeometry(R);
}

void TSetupForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // DoubleBuffered, SetVistaFonts, NoFlicker: not needed in Qt
    // TopPanel.ControlStyle:=TopPanel.ControlStyle-[csParentBackground]: background is set by the style sheet

    Frames.clear();
    SetAdvanced = false;
    OpenMode = omNormal;
    VisibleFrame = -1;
    LanguageEditorMode = 0;
}

void TSetupForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.ModeLabel->setVisible(!SetAdvanced);
    ui.ModeComboBox->setVisible(!SetAdvanced);
    if (!SetAdvanced) {
        if (PrgSetup->EasySetupMode() && (!SetAdvanced)) ui.ModeComboBox->setCurrentIndex(0); else ui.ModeComboBox->setCurrentIndex(1);
    }

    InternalDOSBoxDir = PrgSetup->DOSBoxSettings(0)->DosBoxDir();
    InternalDOSBoxLang = PrgSetup->DOSBoxSettings(0)->DosBoxLanguage();
    InternalBaseDir = PrgSetup->BaseDir();

    UserIconLoader->DirectLoad(ImageList, "SetupForm");
    UserIconLoader->DialogImage(DI_ResetDefault, ui.RestoreDefaultValuesButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    InitFramesList();
    LoadLanguage();

    switch (OpenMode) {
        case omLanguage: ui.Tree->setCurrentItem(FindNodeFromFrame(LanguageFrame)); break;
        case omTreeList: ui.Tree->setCurrentItem(FindNodeFromFrame(TreeListFrame)); break;
        case omToolbar: ui.Tree->setCurrentItem(FindNodeFromFrame(ToolbarFrame)); break;
        case omIconSet: ui.Tree->setCurrentItem(FindNodeFromFrame(IconSetFrame)); break;
        case omUpdate: ui.Tree->setCurrentItem(FindNodeFromFrame(UpdateFrame)); break;
        default: ui.Tree->setCurrentItem(ui.Tree->topLevelItem(0)); break;
    }
    if (ui.Tree->currentItem() != nullptr) ui.Tree->scrollToItem(ui.Tree->currentItem());
}

QWidget* TSetupForm::GetFrame(TFrameClass FrameClass)
{
    // "Frames[I].Frame is FrameClass": FrameClass is a cast lambda (see header)
    for (int I = 0; I < Frames.count(); I++) if (FrameClass && FrameClass(Frames[I].Frame) != nullptr) return Frames[I].Frame;
    return nullptr;
}

void TSetupForm::AddTreeNode(QWidget* ParentFrame, QWidget* NewFrame, ISetupFrame* NewFrameInterface, bool AdvancedModeOnly, int ImageIndex, bool IsEmpty)
{
    TInitData InitData;
    InitData.LanguageChangeNotify = [this]() { LoadLanguage(); };
    InitData.DosBoxDirChangeNotify = [this]() { DOSBoxDirChanged(); };
    InitData.OpenLanguageEditorEvent = [this](int Mode) { OpenLanguageEditor(Mode); };
    InitData.PDosBoxDir = &InternalDOSBoxDir;
    InitData.PDOSBoxLang = &InternalDOSBoxLang;
    InitData.PBaseDir = &InternalBaseDir;
    InitData.CloseCheckEvent = nullptr;
    InitData.BeforeLanguageChangeNotify = [this]() { BeforeChangeLanguage(); };
    InitData.GameDB = GameDB;
    InitData.GetFrame = [this](TFrameClass FrameClass) { return GetFrame(FrameClass); };
    InitData.SearchLinkFile = SearchLinkFile;

    int C = Frames.count();
    NewFrame->setVisible(false);
    if (NewFrame->parentWidget() != ui.RightPanel) NewFrame->setParent(ui.RightPanel);
    NewFrame->setProperty("vclAlign", "alClient");
    // Font.Charset:=CharsetNameToFontCharSet(LanguageSetup.CharsetName): Qt uses Unicode fonts
    // DoubleBuffered:=True: not needed

    TSetupFrameRecord Rec;
    Rec.Frame = NewFrame;
    Rec.IFrame = NewFrameInterface;
    Rec.AdvancedModeOnly = AdvancedModeOnly;
    Rec.ParentFrame = ParentFrame;
    Rec.ImageIndex = ImageIndex;
    Rec.IsEmpty = IsEmpty;
    Frames.append(Rec);
    LayoutFrames();

    try {
        NewFrameInterface->InitGUIAndLoadSetup(InitData);
        Frames[C].CloseCheckEvent = InitData.CloseCheckEvent;
    } catch (const std::exception &E) {
        MessageDlg(QString::fromUtf8(E.what()), mtError, mbOK, 0);
    } catch (...) {
        MessageDlg("Unknown error", mtError, mbOK, 0);
    }
}

void TSetupForm::InitFramesList()
{
    QWidget *F, *Root, *Root2;
    QWidget *P = ui.RightPanel;
    auto I = [](QWidget *W) { return dynamic_cast<ISetupFrame*>(W); };

    F = new TSetupFrameBase(P); AddTreeNode(nullptr, F, I(F), false, 0); Root = F;
    F = new TSetupFrameDirectories(P); AddTreeNode(Root, F, I(F), false, 1);
    F = new TSetupFrameSecurity(P); AddTreeNode(Root, F, I(F), true, 5);
    F = new TSetupFrameDataPrivacy(P); AddTreeNode(Root, F, I(F), true, 5);
    F = new TSetupFrameCompression(P); AddTreeNode(Root, F, I(F), true, 16);
    F = new TSetupFrameWine(P); AddTreeNode(Root, F, I(F), true, 19);

    F = new TSetupFrameSurface(P); AddTreeNode(nullptr, F, I(F), false, 12); Root = F; IconSetFrame = F;
    F = new TSetupFrameLanguage(P); AddTreeNode(Root, F, I(F), false, 2, true); LanguageFrame = F; Root2 = F;
    F = new TSetupFrameCustomLanguageStrings(P); AddTreeNode(Root2, F, I(F), true, 3, false);
    F = new TSetupFrameMenubar(P); AddTreeNode(Root, F, I(F), true, 13);
    F = new TSetupFrameToolbar(P); AddTreeNode(Root, F, I(F), true, 13); ToolbarFrame = F;
    F = new TSetupFrameGamesListTreeAppearance(P); AddTreeNode(Root, F, I(F), true, 22); TreeListFrame = F;
    F = new TSetupFrameGamesListAppearance(P); AddTreeNode(Root, F, I(F), true, 3); Root2 = F;
    F = new TSetupFrameGamesListColumns(P); AddTreeNode(Root2, F, I(F), true, 3);
    F = new TSetupFrameGamesListScreenshotModeAppearance(P); AddTreeNode(Root2, F, I(F), true, 20);
    F = new TSetupFrameGameListIconModeAppearance(P); AddTreeNode(Root2, F, I(F), true, 21);
    F = new TSetupFrameGamesListScreenshotAppearance(P); AddTreeNode(Root, F, I(F), true, 18);
    F = new TSetupFrameImageScaling(P); AddTreeNode(Root, F, I(F), true, 18);

    F = new TSetupFrameProfileEditor(P); AddTreeNode(nullptr, F, I(F), true, 8); Root = F;
    F = new TSetupFrameDefaultValues(P); AddTreeNode(Root, F, I(F), true, 6);
    F = new TSetupFrameAutomaticConfiguration(P); AddTreeNode(Root, F, I(F), true, 24);

    F = new TSetupFramePrograms(P); AddTreeNode(nullptr, F, I(F), false, 0, true); Root = F;
    F = new TSetupFrameDOSBox(P); AddTreeNode(Root, F, I(F), false, 4); Root2 = F;
    F = new TSetupFrameDOSBoxGlobal(P); AddTreeNode(Root2, F, I(F), true, 4);
    F = new TSetupFrameDOSBoxExt(P); AddTreeNode(Root2, F, I(F), true, 4);
    F = new TSetupFrameFreeDOS(P); AddTreeNode(Root, F, I(F), false, 15, true);
    F = new TSetupFrameQBasic(P); AddTreeNode(Root, F, I(F), false, 11);
    F = new TSetupFrameUserInterpreterFrame(P); AddTreeNode(Root, F, I(F), true, 23);
    F = new TSetupFrameScummVM(P); AddTreeNode(Root, F, I(F), false, 10);
    F = new TSetupFrameMoreEmulators(P); AddTreeNode(Root, F, I(F), true, 23);
    F = new TSetupFrameWaveEncoder(P); AddTreeNode(Root, F, I(F), false, 9, false);
    F = new TSetupFrameZipPrgs(P); AddTreeNode(Root, F, I(F), true, 16, false);
    F = new TSetupFrameEditor(P); AddTreeNode(Root, F, I(F), true, 17, false);
    F = new TSetupFrameViewer(P); AddTreeNode(Root, F, I(F), true, 18, false);
    F = new TSetupFrameWindowsGames(P); AddTreeNode(Root, F, I(F), true, 8, false);

    F = new TSetupFrameService(P); AddTreeNode(nullptr, F, I(F), false, 7, true); Root = F;
    F = new TSetupFrameUpdate(P); AddTreeNode(Root, F, I(F), false, 14); UpdateFrame = F;
}

void TSetupForm::BeforeChangeLanguage()
{
    for (int I = 0; I < Frames.count(); I++) Frames[I].IFrame->BeforeChangeLanguage();
}

void TSetupForm::LoadLanguage()
{
    int I;
    // Font.Charset:=CharsetNameToFontCharSet(LanguageSetup.CharsetName): Qt uses Unicode fonts

    setWindowTitle(LanguageSetup->SetupForm());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.RestoreDefaultValuesButton->setText(LanguageSetup->SetupFormDefaultValueReset());
    ui.ModeLabel->setText(LanguageSetup->SetupFormMode());
    I = ui.ModeComboBox->currentIndex();
    while (ui.ModeComboBox->count() < 2) ui.ModeComboBox->addItem("");
    ui.ModeComboBox->setItemText(0, LanguageSetup->SetupFormModeEasy());
    ui.ModeComboBox->setItemText(1, LanguageSetup->SetupFormModeAdvanced());
    ui.ModeComboBox->setCurrentIndex(qMax(0, qMin(1, I)));
    VclLayout::autoSizeLabels(ui.BottomPanel);

    ModeComboBoxChange(this);

    for (I = 0; I < Frames.count(); I++) {
        Frames[I].IFrame->LoadLanguage();
        // Frames[I].Frame.Font.Charset:=...: not needed
    }
}

void TSetupForm::DOSBoxDirChanged()
{
    for (int I = 0; I < Frames.count(); I++) Frames[I].IFrame->DOSBoxDirChanged();
}

void TSetupForm::OpenLanguageEditor(int Mode)
{
    LanguageEditorMode = Mode;
    setModalResult(mrOk);
    OKButtonClick(this);
}

QTreeWidgetItem* TSetupForm::FindNodeFromFrame(QWidget* F)
{
    int Data = -1;

    for (int I = 0; I < Frames.count(); I++) if (Frames[I].Frame == F) { Data = I; break; }
    if (Data < 0) return nullptr;
    for (QTreeWidgetItemIterator It(ui.Tree); *It; ++It) if (NodeData(*It) == Data) return *It;
    return nullptr;
}

void TSetupForm::ModeComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    bool AdvancedMode;
    int LastFrame, I;
    QTreeWidgetItem *ParentNode, *Node, *LastNode;

    if (ui.Tree->currentItem() == nullptr) LastFrame = -1; else LastFrame = NodeData(ui.Tree->currentItem());
    LastNode = nullptr;
    AdvancedMode = SetAdvanced || (ui.ModeComboBox->currentIndex() == 1);
    ui.Tree->setCurrentItem(nullptr);
    if (VisibleFrame >= 0) TreeChange(ui.Tree, nullptr);   // Tree.Selected:=nil fires OnChange

    ui.Tree->setUpdatesEnabled(false);
    {
        const QSignalBlocker Blocker(ui.Tree);
        ui.Tree->clear();
    }

    for (I = 0; I < Frames.count(); I++) if ((!Frames[I].AdvancedModeOnly) || AdvancedMode) {
        ParentNode = FindNodeFromFrame(Frames[I].ParentFrame);
        if (ParentNode != nullptr) Node = new QTreeWidgetItem(ParentNode, QStringList{Frames[I].IFrame->GetName()});
        else Node = new QTreeWidgetItem(ui.Tree, QStringList{Frames[I].IFrame->GetName()});
        Node->setData(0, Qt::UserRole, I);
        if (ImageList != nullptr && Frames[I].ImageIndex >= 0 && Frames[I].ImageIndex < ImageList->Count())
            Node->setIcon(0, ImageList->Icon(Frames[I].ImageIndex));
        if (I == LastFrame) LastNode = Node;
    }

    for (I = 0; I < ui.Tree->topLevelItemCount(); I++) ui.Tree->topLevelItem(I)->setExpanded(true);

    if (LastNode == nullptr) LastNode = ui.Tree->topLevelItem(0);
    ui.Tree->setCurrentItem(LastNode);
    ui.Tree->setUpdatesEnabled(true);

    if (ui.Tree->currentItem() != nullptr) ui.Tree->scrollToItem(ui.Tree->currentItem());
}

void TSetupForm::SelectFrame(int Nr)
{
    if (Nr == -1) ui.TopPanel_Caption->setText(""); else {
        LayoutFrames();
        Frames[Nr].Frame->setVisible(true);
        Frames[Nr].Frame->raise();
        Frames[Nr].IFrame->ShowFrame(SetAdvanced || (ui.ModeComboBox->currentIndex() == 1));
        ui.TopPanel_Caption->setText(" " + Frames[Nr].IFrame->GetName());
    }
}

void TSetupForm::TreeChange(QObject* Sender, QTreeWidgetItem* Node)
{
    Q_UNUSED(Sender);
    Q_UNUSED(Node);
    QTreeWidgetItem *Selected = ui.Tree->currentItem();
    if ((Selected != nullptr) && (NodeData(Selected) == VisibleFrame)) return;

    if (VisibleFrame >= 0) {
        Frames[VisibleFrame].IFrame->HideFrame();
        Frames[VisibleFrame].Frame->setVisible(false);
    }

    if (Selected == nullptr) VisibleFrame = -1; else VisibleFrame = NodeData(Selected);

    SelectFrame(VisibleFrame);
}

void TSetupForm::PopupClick(QObject* Sender)
{
    int Nr = Sender->property("tag").toInt();
    if (Nr == -2) {
        if (ui.Tree->currentItem() == nullptr) return;
        Nr = NodeData(ui.Tree->currentItem());
    }
    for (int I = 0; I < Frames.count(); I++) if (((!Frames[I].IsEmpty) && (Nr == I)) || (Nr == -1))
        Frames[I].IFrame->RestoreDefaults();
}

void TSetupForm::RestoreDefaultValuesButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QAction *M;
    QMenu *PopupMenu = ui.PopupMenu;

    PopupMenu->clear();

    auto Icon = [this](int Nr) { return (ImageList != nullptr && Nr >= 0 && Nr < ImageList->Count()) ? ImageList->Icon(Nr) : QIcon(); };

    if (ui.Tree->currentItem() != nullptr) {
        /* Add "This page" item */
        M = PopupMenu->addAction(Icon(Frames[NodeData(ui.Tree->currentItem())].ImageIndex), LanguageSetup->SetupFormDefaultValueResetThis());
        M->setProperty("tag", -2);
        connect(M, &QAction::triggered, this, [this, M]() { PopupClick(M); });
    }

    /* Add "All" item */
    M = PopupMenu->addAction(LanguageSetup->SetupFormDefaultValueResetAll());
    M->setProperty("tag", -1);
    connect(M, &QAction::triggered, this, [this, M]() { PopupClick(M); });

    /* Line */
    PopupMenu->addSeparator();

    /* Add pages */
    for (int I = 0; I < Frames.count(); I++) if (!Frames[I].IsEmpty) {
        if (Frames[I].AdvancedModeOnly && (!SetAdvanced) && (ui.ModeComboBox->currentIndex() == 0)) continue;
        M = PopupMenu->addAction(Icon(Frames[I].ImageIndex), Frames[I].IFrame->GetName());
        M->setProperty("tag", I);
        connect(M, &QAction::triggered, this, [this, M]() { PopupClick(M); });
    }

    /* Open Popup */
    QPoint P = ui.BottomPanel->mapToGlobal(ui.RestoreDefaultValuesButton->pos());
    PopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
}

void TSetupForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I, J;
    bool B;

    if (VisibleFrame >= 0) {
        Frames[VisibleFrame].IFrame->HideFrame();
        Frames[VisibleFrame].Frame->setVisible(false);
    }

    for (I = 0; I < Frames.count(); I++) if (Frames[I].CloseCheckEvent) {
        B = true; Frames[I].CloseCheckEvent(B); if (!B) {
            SelectFrame(I);
            J = 0;
            for (QTreeWidgetItemIterator It(ui.Tree); *It; ++It, ++J) if (NodeData(*It) == I) {
                // Delphi: Tree.Selected:=Tree.Items[I] (sic, index I); the node J is meant
                ui.Tree->setCurrentItem(*It);
                break;
            }
            setModalResult(mrNone); return;
        }
    }

    if (!SetAdvanced) PrgSetup->SetEasySetupMode(ui.ModeComboBox->currentIndex() == 0);
    for (I = 0; I < Frames.count(); I++) Frames[I].IFrame->SaveSetup();
}

void TSetupForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I = 0;
    if (VisibleFrame >= 0) I = FrameHelpContext(Frames[VisibleFrame].Frame);
    if (I == 0) I = ID_FileOptions;
    ApplicationHelpContext(I);
}

void TSetupForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

/* global */

static void OpenLanguageEditor(QWidget* AOwner, int LanguageEditorMode)
{
    Q_UNUSED(LanguageEditorMode);
    QString S;
    switch (SetupForm->LanguageEditorMode) {
        case 1:
            ShowLanguageEditorDialog(AOwner, LanguageSetup->SetupFile());
            LanguageSetup->ReloadINI();
            break;
        case 2:
            if (ShowLanguageEditorStartDialog(AOwner, S, true)) {
                ShowLanguageEditorDialog(AOwner, S);
                if (Trim(ExtUpperCase(S)) == Trim(ExtUpperCase(LanguageSetup->SetupFile()))) LanguageSetup->ReloadINI();
            }
            break;
    }
}

bool ShowSetupDialogSpecial(QWidget* AOwner, TGameDB* AGameDB, TLinkFile* ASearchLinkFile, TOpenMode OpenMode)
{
    bool result;
    QWidget *DFendReloadedMainForm = DFendReloadedMainFormWidget();
    if (DFendReloadedMainForm != nullptr) DFendReloadedMainForm->setEnabled(false);
    SetupForm = new TSetupForm(AOwner);
    SetupForm->setEnabled(true);
    SetupForm->GameDB = AGameDB;
    SetupForm->SearchLinkFile = ASearchLinkFile;
    if (OpenMode != omNormal) SetupForm->OpenMode = OpenMode;
    if ((OpenMode != omNormal) && (OpenMode != omLanguage) && (OpenMode != omIconSet) && (OpenMode != omUpdate)) SetupForm->SetAdvanced = true;
    result = (SetupForm->ShowModal() == mrOk);
    if (!result) LoadLanguage(PrgSetup->Language());
    if (result && (SetupForm->LanguageEditorMode > 0)) OpenLanguageEditor(AOwner, SetupForm->LanguageEditorMode);
    delete SetupForm;
    SetupForm = nullptr;
    if (DFendReloadedMainForm != nullptr) DFendReloadedMainForm->setEnabled(true);
    return result;
}

bool ShowSetupDialog(QWidget* AOwner, TGameDB* AGameDB, TLinkFile* ASearchLinkFile, bool OpenLanguageTab)
{
    if (OpenLanguageTab)
        return ShowSetupDialogSpecial(AOwner, AGameDB, ASearchLinkFile, omLanguage);
    else
        return ShowSetupDialogSpecial(AOwner, AGameDB, ASearchLinkFile, omNormal);
}

bool ShowTreeListSetupDialog(QWidget* AOwner, TGameDB* AGameDB, TLinkFile* ASearchLinkFile)
{
    return ShowSetupDialogSpecial(AOwner, AGameDB, ASearchLinkFile, omTreeList);
}

bool ShowToolbarSetupDialog(QWidget* AOwner, TGameDB* AGameDB, TLinkFile* ASearchLinkFile)
{
    return ShowSetupDialogSpecial(AOwner, AGameDB, ASearchLinkFile, omToolbar);
}

bool ShowIconSetSetupDialog(QWidget* AOwner, TGameDB* AGameDB, TLinkFile* ASearchLinkFile)
{
    return ShowSetupDialogSpecial(AOwner, AGameDB, ASearchLinkFile, omIconSet);
}

bool ShowUpdateSetupDialog(QWidget* AOwner, TGameDB* AGameDB, TLinkFile* ASearchLinkFile)
{
    return ShowSetupDialogSpecial(AOwner, AGameDB, ASearchLinkFile, omUpdate);
}
