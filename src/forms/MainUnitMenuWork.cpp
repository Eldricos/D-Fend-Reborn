// The commands of the main window (menus, tool bar, popup menus): MenuWork
// (the event handler of all these menu items in the original) passes each
// command to the function of its menu; the command number is the Tag of the
// menu item.
#include "MainUnit.h"
#include "MainUnitTools.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "SetupFormUnit.h"
#include "PrgConsts.h"
#include "ProfileEditorFormUnit.h"
#include "IconManagerFormUnit.h"
#include "DosBoxUnit.h"
#include "HistoryFormUnit.h"
#include "TemplateFormUnit.h"
#include "UninstallFormUnit.h"
#include "UninstallSelectFormUnit.h"
#include "CreateConfFormUnit.h"
#include "CreateShortcutFormUnit.h"
#include "CreateImageUnit.h"
#include "InfoFormUnit.h"
#include "TransferFormUnit.h"
#include "BuildInstallerFormUnit.h"
#include "BuildInstallerForSingleGameFormUnit.h"
#include "ModernProfileEditorFormUnit.h"
#include "GOGImportUnit.h"
#include "MissingGamesFormUnit.h"
#include "StatisticsFormUnit.h"
#include "LanguageEditorStartFormUnit.h"
#include "LanguageEditorFormUnit.h"
#include "CreateISOImageFormUnit.h"
#include "CreateXMLFormUnit.h"
#include "ExpandImageFormUnit.h"
#include "BuildImageFromFolderFormUnit.h"
#include "ScummVMUnit.h"
#include "ListScummVMGamesFormUnit.h"
#include "SimpleXMLUnit.h"
#include "OperationModeInfoFormUnit.h"
#include "CopyProfileFormUnit.h"
#include "ZipPackageUnit.h"
#include "WindowsProfileUnit.h"
#include "HelpConsts.h"
#include "BuildZipPackagesFormUnit.h"
#include "QuickStartFormUnit.h"
#include "ZipInfoFormUnit.h"
#include "ScanGamesFolderFormUnit.h"
#include "CreateShortcutsFormUnit.h"
#include "PackageManagerFormUnit.h"
#include "PackageCreationFormUnit.h"
#include "MultipleProfilesEditorFormUnit.h"
#include "InstallationSupportFormUnit.h"
#include "MakeBootImageFromProfileFormUnit.h"
#include "CheatApplyFormUnit.h"
#include "CheatDBEditFormUnit.h"
#include "CheatSearchFormUnit.h"
#include "UpdateCheckFormUnit.h"
#include "DOSBoxLangEditFormUnit.h"
#include "DOSBoxLangStartFormUnit.h"
#include "ExportGamesListFormUnit.h"
#include "DOSBoxOutputTestFormUnit.h"
#include "DOSBoxTempUnit.h"

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

void TDFendRebornMainForm::MenuWork(QObject *Sender)
{
    StopCaptureChangeNotify();
    struct RestartNotify {
        TDFendRebornMainForm *F;
        ~RestartNotify() { F->StartCaptureChangeNotify(); }
    } Finally{this};

    const int ATag = Tag(Sender);
    switch (ATag / 1000) {
        case 1: FileMenuWork(Sender, ATag); break;
        case 2: ViewMenuWork(Sender, ATag); break;
        case 3: RunMenuWork(Sender, ATag); break;
        case 4: ProfileMenuWork(Sender, ATag); break;
        case 5: ExtrasMenuWork(Sender, ATag); break;
        case 6: HelpMenuWork(Sender, ATag); break;
    }
}

// Shows an error message if no game is selected
bool TDFendRebornMainForm::NoGameSelected()
{
    if (ui.ListView->Selected() == nullptr || ui.ListView->Selected()->Data() == nullptr) {
        MessageDlg(LanguageSetup->MessageNoGameSelected(), mtError, mbOK, 0);
        return true;
    }
    return false;
}

// Reloads the tree and the games list (after profiles were added or changed)
// and selects AGame
void TDFendRebornMainForm::ReloadAndSelect(QObject *Sender, TGame *AGame)
{
    InitTreeViewForGamesList(ui.TreeView, GameDB);
    TreeViewChange(Sender, ui.TreeView->currentItem());
    SelectGame(AGame);
}

// Empties the games list before it is filled again (the selected game may
// not exist any more)
void TDFendRebornMainForm::ClearListBeforeReload()
{
    LastSelectedGame = nullptr;
    ui.ListView->SetSelected(nullptr);
    ui.ListView->Items()->Clear();
    ViewFilesFrame->SetGame(nullptr);
}

// ---------------------------------------------------------------------------
// File menu: import, export, program options (commands 1xxx)
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::FileMenuWork(QObject *Sender, int ATag)
{
    TGame *G, *G2;
    QString S, T;
    TGameDB *AutoSetupDB;

    switch (ATag) {
        case 1001: {
            OpenDialog->AllowMultiSelect = true;
            OpenDialog->DefaultExt = "conf";
            OpenDialog->Title = LanguageSetup->MenuFileImportConfTitle();
            OpenDialog->Filter = LanguageSetup->MenuFileImportConfFilter();
            bool Ok = OpenDialog->Execute();
            OpenDialog->AllowMultiSelect = false;
            if (!Ok) return;
            for (int I = 0; I < OpenDialog->Files.Count(); I++) {
                G = ImportConfFile(GameDB, OpenDialog->Files[I]);
                if (G != nullptr) ReloadAndSelect(Sender, G);
            }
            break;
        }
        case 1002:
            UpdateGameNotes();
            ShowExportGamesListDialog(this, GameDB);
            break;
        case 1003: UpdateGameNotes(); ExportConfFiles(this, GameDB); break;
        case 1004: UpdateGameNotes(); ExportProfFiles(this, GameDB); break;
        case 1005: UpdateGameNotes(); ExportXMLFiles(this, GameDB); break;
        case 1006:
            S = GamesListSaveColWidthsToString(ui.ListView);
            T = GetUserCols();
            if (!ShowSetupDialog(this, GameDB, SearchLinkFile)) return;
            AfterSetupDialog(S, T);
            break;
        case 1007: close(); break;
        case 1008: {
            OpenDialog->AllowMultiSelect = true;
            OpenDialog->DefaultExt = "prof";
            OpenDialog->Title = LanguageSetup->MenuFileImportProfTitle();
            OpenDialog->Filter = LanguageSetup->MenuFileImportProfFilter();
            bool Ok = OpenDialog->Execute();
            OpenDialog->AllowMultiSelect = false;
            if (!Ok) return;
            for (int I = 0; I < OpenDialog->Files.Count(); I++) {
                S = GameDB->ProfFileName(ExtractFileName(OpenDialog->Files[I]), true);
                if (!CopyFile(OpenDialog->Files[I], S, true)) {
                    MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {OpenDialog->Files[I], S}), mtError, mbOK, 0);
                    continue;
                }
                G = new TGame(S);
                GameDB->Add(G);
                ReloadAndSelect(Sender, G);
            }
            break;
        }
        case 1009:
            OpenDialog->DefaultExt = "xml";
            OpenDialog->Title = LanguageSetup->MenuFileImportXMLTitle();
            OpenDialog->Filter = LanguageSetup->MenuFileImportXMLFilter();
            if (!OpenDialog->Execute()) return;
            if (ui.ListView->Selected() != nullptr) G = GameOf(ui.ListView->Selected()); else G = nullptr;
            S = ImportXMLFile(GameDB, OpenDialog->FileName);
            if (S != "") { MessageDlg(S, mtError, mbOK, 0); return; }
            ReloadAndSelect(Sender, G);
            break;
        case 1010: {
            OpenDialog->AllowMultiSelect = true;
            OpenDialog->DefaultExt = "zip";
            OpenDialog->Title = LanguageSetup->ProfileMountingZipFileGeneral();
            OpenDialog->Filter = ProcessFileNameFilter(LanguageSetup->ProfileMountingZipFileFilter2(), LanguageSetup->ProfileMountingZipFileFilter2ArchiveFiles());
            bool Ok = OpenDialog->Execute();
            OpenDialog->AllowMultiSelect = false;
            if (!Ok) return;
            G = nullptr;
            for (int I = 0; I < OpenDialog->Files.Count(); I++) {
                G2 = ImportZipPackage(this, OpenDialog->Files[I], GameDB, PrgSetup->ImportZipWithoutDialogIfPossible());
                if (G2 != nullptr) G = G2;
            }
            if (G == nullptr) return;
            ClearListBeforeReload();
            ReloadAndSelect(Sender, G);
            break;
        }
        case 1011:
            SetFormEnabled(false);
            AutoSetupDB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);
            S = ShowPackageManagerDialog(this, GameDB, AutoSetupDB);
            if (S != "") {
                PrgSetup->SetLanguage(ExtractFileName(S));
                AfterSetupDialog("", "");
            }
            InitTreeViewForGamesList(ui.TreeView, GameDB);
            TreeViewChange(Sender, ui.TreeView->currentItem());
            delete AutoSetupDB;
            SetFormEnabled(true);
            TrimWorkingSet();
            break;
        case 1012: ShowPackageCreationDialog(this, GameDB); break;
        case 1013: UpdateGameNotes(); ShowBuildZipPackagesDialog(this, GameDB); break;
        case 1014: UpdateGameNotes(); BuildInstaller(this, GameDB); break;
        case 1015:
            OpenDialog->DefaultExt = "zip";
            OpenDialog->Title = LanguageSetup->ProfileMountingZipFileGeneral();
            OpenDialog->Filter = ProcessFileNameFilter(LanguageSetup->ProfileMountingZipFileFilter2(), LanguageSetup->ProfileMountingZipFileFilter2ArchiveFiles());
            if (!OpenDialog->Execute()) return;
            G = RunInstallationFromZipFile(this, GameDB, OpenDialog->FileName);
            if (G == nullptr) return;
            ClearListBeforeReload();
            ReloadAndSelect(Sender, G);
            break;
        case 1016:
            S = GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY);
            if (!SelectDirectory(THandle(0), LanguageSetup->MenuFileImportFolderCaption(), S)) return;
            G = ImportFolder(this, S, GameDB, PrgSetup->ImportZipWithoutDialogIfPossible());
            if (G == nullptr) return;
            ClearListBeforeReload();
            ReloadAndSelect(Sender, G);
            break;
        case 1017:
            S = GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY);
            if (!SelectDirectory(THandle(0), LanguageSetup->MenuFileImportFolderCaption(), S)) return;
            G = RunInstallationFromFolder(this, GameDB, S);
            if (G == nullptr) return;
            ClearListBeforeReload();
            ReloadAndSelect(Sender, G);
            break;
        case 1018:
        case 1019: {
            // DOS games of GOG.com / Steam: one game folder or a folder with several games
            const bool Steam = (ATag == 1019);
            const QStringList Folders = Steam ? DefaultSteamFolders() : DefaultGOGFolders();
            S = Folders.isEmpty() ? GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY) : QDir::toNativeSeparators(Folders[0]);
            if (!SelectDirectory(THandle(0), Steam ? LanguageSetup->MenuFileImportSteamCaption() : LanguageSetup->MenuFileImportGOGCaption(), S)) return;
            const QList<TGOGGame> Games = Steam ? FindSteamGames(S) : FindGOGGames(S);
            if (Games.isEmpty()) { MessageDlg(Format(Steam ? LanguageSetup->MenuFileImportSteamNone() : LanguageSetup->MenuFileImportGOGNone(), {S}), mtInformation, mbOK, 0); return; }
            QStringList Imported, Skipped;
            G = nullptr;
            for (const TGOGGame& Game : Games) {
                if (GOGGameInList(GameDB, Game)) { Skipped << Game.Name; continue; }
                TGame* NewGame = ImportGOGGame(GameDB, Game);
                if (NewGame == nullptr) continue;
                Imported << Game.Name;
                G = NewGame;
            }
            if ((Games.size() > 1) || !Skipped.isEmpty()) {
                T.clear();
                if (!Imported.isEmpty()) T = (Steam ? LanguageSetup->MenuFileImportSteamImported() : LanguageSetup->MenuFileImportGOGImported()) + "\n" + Imported.join("\n");
                if (!Skipped.isEmpty()) T += (T.isEmpty() ? QString() : QStringLiteral("\n\n")) + LanguageSetup->MenuFileImportGOGSkipped() + "\n" + Skipped.join("\n");
                MessageDlg(T, mtInformation, mbOK, 0);
            }
            if (G == nullptr) return;
            ClearListBeforeReload();
            ReloadAndSelect(Sender, G);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// View menu: parts of the main window, list style (commands 2xxx)
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::ViewMenuWork(QObject *Sender, int ATag)
{
    QString S, T;

    switch (ATag) {
        case 2001:
            PrgSetup->SetShowTree(!PrgSetup->ShowTree());
            ui.TreeView->setVisible(PrgSetup->ShowTree());
            if (ui.TreeView->isVisibleTo(this) && ui.TreeView->width() < 50) VclLayout::setAlignedExtent(ui.TreeView, 50);
            ui.Splitter->setVisible(ui.TreeView->isVisibleTo(this));
            if (ui.Splitter->isVisibleTo(this)) VclLayout::setBounds(ui.Splitter, ui.TreeView->x() + ui.TreeView->width(), ui.Splitter->y(), ui.Splitter->width(), ui.Splitter->height());
            ui.MenuViewsShowTree->setChecked(PrgSetup->ShowTree());
            if (!PrgSetup->ShowTree()) {
                bool Old = FTreeViewOnChange;
                FTreeViewOnChange = false;   // the change is processed by the explicit call below
                ui.TreeView->setCurrentItem(nullptr);
                FTreeViewOnChange = Old;
                TreeViewChange(Sender, nullptr);
            }
            break;
        case 2002:
            PrgSetup->SetShowScreenshots(!PrgSetup->ShowScreenshots());
            ui.CapturePageControl->setVisible(PrgSetup->ShowScreenshots());
            if (ui.CapturePageControl->isVisibleTo(this) && ui.CapturePageControl->height() < 50) VclLayout::setAlignedExtent(ui.CapturePageControl, 200);
            ui.Splitter1->setVisible(ui.CapturePageControl->isVisibleTo(this));
            if (ui.Splitter1->isVisibleTo(this)) VclLayout::setBounds(ui.Splitter1, ui.Splitter1->x(), ui.CapturePageControl->y() - ui.Splitter1->height(), ui.Splitter1->width(), ui.Splitter1->height());
            ui.MenuViewsShowScreenshots->setChecked(PrgSetup->ShowScreenshots());
            ListViewSelectItem(Sender, ui.ListView->Selected(), true);
            break;
        case 2003:
            PrgSetup->SetShowSearchBox(!PrgSetup->ShowSearchBox());
            if (QAction *SA = findChild<QAction *>("SearchEdit_ToolbarAction")) SA->setVisible(PrgSetup->ShowSearchBox()); else ui.SearchEdit->setVisible(PrgSetup->ShowSearchBox());
            ui.MenuViewsShowSearchBox->setChecked(PrgSetup->ShowSearchBox());

            if (FSearchEditGray) {
                SetSearchEditText(LanguageSetup->Search());
            }
            LoadGUISetup(false);
            break;
        case 2004:
            PrgSetup->SetShowExtraInfo(!PrgSetup->ShowExtraInfo());
            ui.MenuViewsShowExtraInfo->setChecked(PrgSetup->ShowExtraInfo());
            S = GamesListSaveColWidthsToString(ui.ListView);
            InitListViewForGamesList(ui.ListView, PrgSetup->ShowExtraInfo());
            GamesListLoadColWidthsFromString(ui.ListView, S);
            TreeViewChange(Sender, ui.TreeView->currentItem());
            break;
        case 2005:
            PrgSetup->SetShowTooltips(!PrgSetup->ShowTooltips());
            ui.MenuViewsShowTooltips->setChecked(PrgSetup->ShowTooltips());
            SetListViewShowHint(PrgSetup->ShowTooltips());
            break;
        case 2006: PrgSetup->SetListViewStyle("SimpleListNoIcons"); InitViewStyle(); break;
        case 2007: PrgSetup->SetListViewStyle("SimpleList"); InitViewStyle(); break;
        case 2008: PrgSetup->SetListViewStyle("ListNoIcons"); InitViewStyle(); break;
        case 2009: PrgSetup->SetListViewStyle("List"); InitViewStyle(); break;
        case 2010: PrgSetup->SetListViewStyle("SmallIcons"); InitViewStyle(); break;
        case 2011: PrgSetup->SetListViewStyle("Icons"); InitViewStyle(); break;
        case 2012: PrgSetup->SetListViewStyle("Screenshots"); InitViewStyle(); break;
        case 2013:
            if (!JustShowingOrHidingQuickStartForm) {
                JustShowingOrHidingQuickStartForm = true;
                if (QuickStartForm) {
                    if (!QuickStartForm->Working) { delete QuickStartForm; QuickStartForm = nullptr; }
                } else {
                    QuickStartForm = new TQuickStartForm(this);
                    QuickStartForm->SetOnAddProfile([this](QObject *S, const QString &F, bool W, bool U, int T) { AddProfileFromQuickStarter(S, F, W, U, T); });
                    QuickStartForm->SetOnMenuCloseNotify([this](QObject *S) { QuickStarterCloseNotify(S); });
                    QuickStartForm->show();
                }
                // MenuViewsQuickStarter.Checked:=not MenuViewsQuickStarter.Checked
                // (the checkable QAction has already toggled itself when it was triggered)
                if (Sender != ui.MenuViewsQuickStarter || !ui.MenuViewsQuickStarter->isCheckable())
                    ui.MenuViewsQuickStarter->setChecked(!ui.MenuViewsQuickStarter->isChecked());
                ProcessMessages();
                JustShowingOrHidingQuickStartForm = false;
            }
            break;
        case 2014:
            if (!PrgSetup->ShowToolbar() && PrgSetup->ShowMainMenu()) {
                MessageDlg(LanguageSetup->MenuViewShowMenubarWarning(), mtError, mbOK, 0);
                if (ui.MenuViewShowMenubar->isCheckable()) ui.MenuViewShowMenubar->setChecked(true);
                return;
            }
            PrgSetup->SetShowMainMenu(!PrgSetup->ShowMainMenu());
            ui.MenuFile->menuAction()->setVisible(PrgSetup->ShowMainMenu());
            ui.MenuView->menuAction()->setVisible(PrgSetup->ShowMainMenu());
            ui.MenuRun->menuAction()->setVisible(PrgSetup->ShowMainMenu());
            ui.MenuProfile->menuAction()->setVisible(PrgSetup->ShowMainMenu());
            ui.MenuExtras->menuAction()->setVisible(PrgSetup->ShowMainMenu());
            ui.MenuHelp->menuAction()->setVisible(PrgSetup->ShowMainMenu());
            ui.ToolbarPopupShowMenubar->setVisible(!PrgSetup->ShowMainMenu());
            if (ui.MenuViewShowMenubar->isCheckable()) ui.MenuViewShowMenubar->setChecked(true);
            if (!PrgSetup->ShowMainMenu()) MessageDlg(LanguageSetup->MenuViewShowMenubarRestoreInfo(), mtInformation, mbOK, 0);
            break;
        case 2015:
            if (!PrgSetup->ShowMainMenu() && PrgSetup->ShowToolbar()) {
                MessageDlg(LanguageSetup->MenuViewShowMenubarWarning(), mtError, mbOK, 0);
                ui.MenuViewShowToolbar->setChecked(PrgSetup->ShowToolbar());
                return;
            }
            PrgSetup->SetShowToolbar(!PrgSetup->ShowToolbar());
            ui.CoolBar->setVisible(PrgSetup->ShowToolbar());
            ui.MenuViewShowToolbar->setChecked(PrgSetup->ShowToolbar());
            break;
    }
}

// ---------------------------------------------------------------------------
// Run menu: games, DOSBox and ScummVM, extra program files of a profile (commands 3xxx)
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::RunMenuWork(QObject *Sender, int ATag)
{
    TGame *G, *DefaultGame;
    QString S;
    const QString DOSBoxExe = DOSBoxExeFileOfInstallation(0);
    Q_UNUSED(Sender);

    switch (ATag) {
        case 3001:
            if (NoGameSelected()) return;
            /*For fixing problems with not hidden tooltips*/ QToolTip::hideText();
            G = GameOf(ui.ListView->Selected());
            if (ScummVMMode(G)) {
                RunScummVMGame(G);
            } else {
                if (WindowsExeMode(G))
                    RunWindowsGame(G);
                else
                    RunGame(G, DeleteOnExit);
            }
            break;
        case 3002:
            if (NoGameSelected()) return;
            /*For fixing problems with not hidden tooltips*/ QToolTip::hideText();
            G = GameOf(ui.ListView->Selected());
            if (WindowsExeMode(G)) RunWindowsGame(G, true); else RunGame(G, DeleteOnExit, true);
            break;
        case 3003: RunDOSBoxWithDefaultConfFile(); break;
        case 3004:
            if (!FileExists(DOSBoxExe)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {DOSBoxExe}), mtError, mbOK, 0);
                return;
            }
            DefaultGame = new TGame(PrgSetup);
            G = new TGame(TempDir() + "TempGameRec.prof");
            G->AssignFrom(DefaultGame); G->SetGameExe(""); G->SetAutoexec(""); G->SetNrOfMounts(0);
            RunWithCommandline(G, DeleteOnExit, "-startmapper", true);
            delete G;
            delete DefaultGame;
            ExtDeleteFile(TempDir() + "TempGameRec.prof", ftTemp);
            break;
        case 3005: OpenDOSBoxDefaultConfFile(); break;
        case 3006:
            if (Trim(PrgSetup->ScummVMPath()).isEmpty())
                MessageDlg(LanguageSetup->MessageScummVMNotConfigured(), mtError, mbOK, 0);
            else if (FileExists(IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + ScummPrgFile))
                StartProcess(IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + ScummPrgFile, QStringList(), IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()));
            else
                MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + ScummPrgFile}), mtError, mbOK, 0);
            break;
        case 3007:
            S = FindScummVMIni();
            if (S != "")
                OpenFileInEditor(S);
            else
                MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {S}), mtError, mbOK, 0);
            break;
        case 3008: ShowDOSBoxOutputTestDialog(this, GameDB->ConfOpt()); break;
        default:
            // extra program files of the profile
            if (ATag >= 3010 && ATag <= 3059) {
                if (ui.ListView->Selected() != nullptr && ui.ListView->Selected()->Data() != nullptr) {
                    G = GameOf(ui.ListView->Selected());
                    if (ScummVMMode(G)) return;
                    if (WindowsExeMode(G))
                        RunWindowssExtraFile(G, ATag - 3010);
                    else
                        RunExtraFile(G, DeleteOnExit, ATag - 3010);
                }
            }
            break;
    }
}

// ---------------------------------------------------------------------------
// Profile menu (and the popup menu of the games list): add, edit, copy, delete, open folders, search links, Windows emulators (commands 4xxx)
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::ProfileMenuWork(QObject *Sender, int ATag)
{
    TGame *G;
    QString S;
    bool B = false, C = false;
    TListItem *Sel;

    switch (ATag) {
        case 4000:
            UpdateGameNotes();
            switch (PrgSetup->AddButtonFunction()) {
                case 0: MenuWork(ui.MenuProfileAdd); break;
                case 1: MenuWork(ui.MenuProfileAddWithWizard); break;
                case 2: {
                    QWidget *Btn = ui.ToolBar->widgetForAction(ui.ButtonAdd);
                    QPoint P = Btn ? Btn->mapToGlobal(QPoint(0, Btn->height())) : QCursor::pos();
                    ui.AddButtonPopupMenu->popup(P);
                    break;
                }
            }
            break;
        case 4001: UpdateGameNotes(); AddProfile("", "", -1); TrimWorkingSet(); break;
        case 4002: UpdateGameNotes(); StartWizard("", false); TrimWorkingSet(); break;
        case 4003: {
            if (NoGameSelected()) return;
            UpdateGameNotes(); LastSelectedGame = nullptr;
            TList L;
            for (int I = 0; I < ui.ListView->Items()->Count(); I++) L.append(ui.ListView->Items()->Item(I)->Data());
            G = GameOf(ui.ListView->Selected());
            SetFormEnabled(false);
            bool Ok;
            if (PrgSetup->DFendStyleProfileEditor()) {
                Ok = EditGameProfil(this, GameDB, G, nullptr, SearchLinkFile, DeleteOnExit, "", "", &L);
                if (!Ok) { SetFormEnabled(true); LoadSearchLinks(); return; }
            } else {
                Ok = ModernEditGameProfil(this, GameDB, G, nullptr, SearchLinkFile, DeleteOnExit, "", "", &L);
                if (!Ok) {
                    SelectGame(G); /*Always update screenshots list (a new image could have been downloaded)*/
                    SetFormEnabled(true); LoadSearchLinks();
                    return;
                }
            }
            SetFormEnabled(true);
            ReloadAndSelect(Sender, G);
            LoadSearchLinks();
            break;
        }
        case 4004:
            if (NoGameSelected()) return;
            UpdateGameNotes(); LastSelectedGame = nullptr;
            Sel = ui.ListView->Selected();
            S = GameOf(Sel)->Name();
            if (!ShowCopyProfileDialog(this, Trim(GameOf(Sel)->DataDir()) != "", S, B, C)) return;
            G = GameDB->Game(GameDB->Add(S));
            G->AssignFrom(GameOf(Sel));
            G->SetName(S);
            if (B) {
                S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(G->Name()) + PathDelim;
                int I = 0;
                while (!PrgSetup->IgnoreDirectoryCollisions() && DirectoryExists(MakeAbsPath(S, PrgSetup->BaseDir()))) {
                    I++;
                    S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(G->Name()) + IntToStr(I) + PathDelim;
                }
                G->SetCaptureFolder(MakeRelPath(S, PrgSetup->BaseDir()));
            }
            if (C) G->SetDataDir("");
            G->LoadCache();
            ReloadAndSelect(Sender, G);
            break;
        case 4005:
            if (NoGameSelected()) return;
            UpdateGameNotes(); LastSelectedGame = nullptr;
            G = GameOf(ui.ListView->Selected());
            if (PrgSetup->AskBeforeDelete()) {
                if (MessageDlg(Format(LanguageSetup->MessageConfirmationDeleteRecordOnly(), {G->Name()}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
            }
            ui.ListView->Items()->Clear();
            GameDB->Delete(G);
            InitTreeViewForGamesList(ui.TreeView, GameDB);
            TreeViewChange(Sender, ui.TreeView->currentItem());
            break;
        case 4006:
            if (NoGameSelected()) return;
            UpdateGameNotes(); LastSelectedGame = nullptr;
            G = GameOf(ui.ListView->Selected());
            if (WindowsExeMode(G)) {
                if (PrgSetup->AskBeforeDelete()) {
                    if (MessageDlg(Format(LanguageSetup->MessageConfirmationDeleteRecordOnly(), {G->Name()}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
                }
                ui.ListView->Items()->Clear();
                GameDB->Delete(G);
                InitTreeViewForGamesList(ui.TreeView, GameDB);
                TreeViewChange(Sender, ui.TreeView->currentItem());
            } else {
                SetListViewCustomDraw(false);
                ViewFilesFrame->SetGame(nullptr);
                if (!UninstallGame(this, GameDB, G)) {
                    if (ui.ListView->Selected() != nullptr) ViewFilesFrame->SetGame(GameOf(ui.ListView->Selected()));
                    SetListViewCustomDraw(true);
                    return;
                }
                LastSelectedGame = nullptr;
                ui.ListView->SetSelected(nullptr);
                ui.ListView->Items()->Clear();
                SetListViewCustomDraw(true);
                ReloadAndSelect(Sender, G);
            }
            break;
        case 4007:
            if (ui.ListView->Selected() != nullptr && ui.ListView->Selected()->Data() != nullptr) {
                S = ""; G = GameOf(ui.ListView->Selected());
                if (DOSBoxMode(G)) S = ExtractFilePath(MakeAbsPath(G->GameExe(), PrgSetup->BaseDir()));
                if (ScummVMMode(G)) S = MakeAbsPath(G->ScummVMPath(), PrgSetup->BaseDir());
                if (WindowsExeMode(G)) S = ExtractFilePath(MakeAbsPath(G->GameExe(), PrgSetup->BaseDir()));
                ShellOpen(S, QString(), S);
            }
            break;
        case 4008:
            if (ui.ListView->Selected() != nullptr && ui.ListView->Selected()->Data() != nullptr) {
                S = MakeAbsPath(GameOf(ui.ListView->Selected())->DataDir(), PrgSetup->BaseDir());
                ShellOpen(S, QString(), S);
            }
            break;
        case 4010:
            if (ui.ListView->Selected() != nullptr && ui.ListView->Selected()->Data() != nullptr) {
                G = GameOf(ui.ListView->Selected());
                G->SetFavorite(!G->Favorite());
                TreeViewChange(Sender, ui.TreeView->currentItem());
            }
            break;
        case 4011:
            if (NoGameSelected()) return;
            if (WineSupportEnabled() && PrgSetup->LinuxLinkMode()) {
                CreateLinuxShortCut(this, GameOf(ui.ListView->Selected()));
            } else {
                G = GameOf(ui.ListView->Selected());
                if (WindowsExeMode(G))
                    ShowCreateShortcutDialog(this, G->Name(), G->SetupFile(), G->Icon(), G->GameExe(), G->GameParameters(), false, true);
                else
                    ShowCreateShortcutDialog(this, G->Name(), G->SetupFile(), G->Icon(), "", "", ScummVMMode(G), false);
            }
            break;
        case 4012:
            if (NoGameSelected()) return;
            UpdateGameNotes();
            BuildInstallerForSingleGame(this, GameOf(ui.ListView->Selected()));
            break;
        case 4013:
            if (NoGameSelected()) return;
            G = GameOf(ui.ListView->Selected());
            OpenConfigurationFile(G, DeleteOnExit);
            break;
        case 4014: AddProfile("", "ScummVM", -1); break;
        case 4015:
            if (ui.ListView->Selected() != nullptr && ui.ListView->Selected()->Data() != nullptr)
                OpenLink(ActionHint(Sender), "<GAMENAME>", GameOf(ui.ListView->Selected())->Name());
            break;
        case 4016: if (SearchLinkFile->EditFile(false)) LoadSearchLinks(); break;
        case 4017:
            if (NoGameSelected()) return;
            UpdateGameNotes();
            SaveDialog->DefaultExt = "zip";
            SaveDialog->FileName = ExtractFileName(ChangeFileExt(GameOf(ui.ListView->Selected())->SetupFile(), ""));
            SaveDialog->Title = LanguageSetup->ProfileMountingZipFileGeneral();
            SaveDialog->Filter = ProcessFileNameFilter(LanguageSetup->ProfileMountingZipFileFilter2(), LanguageSetup->ProfileMountingZipFileFilter2ArchiveFiles());
            SaveDialog->InitialDir = GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY);
            if (!SaveDialog->Execute()) return;
            BuildZipPackage(this, GameOf(ui.ListView->Selected()), SaveDialog->FileName, false);
            break;
        case 4018: AddProfile("", "Windows", -1); break;
        case 4019:
            if (ui.ListView->Selected() != nullptr && ui.ListView->Selected()->Data() != nullptr) {
                G = GameOf(ui.ListView->Selected()); if (Trim(G->CaptureFolder()) == "") return;
                S = MakeAbsPath(G->CaptureFolder(), PrgSetup->BaseDir());
                if (DirectoryExists(S)) OpenFolder(S);
            }
            break;
        case 4020:
            SetFormEnabled(false);
            G = ShowInstallationSupportDialog(this, GameDB);
            if (G != nullptr) {
                InitTreeViewForGamesList(ui.TreeView, GameDB);
                TreeViewChange(this, ui.TreeView->currentItem());
                SelectGame(G);
            }
            SetFormEnabled(true);
            break;
        case 4021:
            if (NoGameSelected()) return;
            S = Trim(GameOf(ui.ListView->Selected())->GameExe());
            if (S == "") S = Trim(GameOf(ui.ListView->Selected())->SetupExe());
            if (S == "") S = MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir()); else S = ExtractFilePath(MakeAbsPath(S, PrgSetup->BaseDir()));
            B = false;
            if (ShowCheatApplyDialog(this, S, B) && B) {
                ShowCheatDBEditDialog(this, B);
                if (B) ShowUpdateCheckDialog(this, GameDB, SearchLinkFile);
            }
            break;
        case 4022:
            if (ui.ListView->Selected() != nullptr && ui.ListView->Selected()->Data() != nullptr) {
                G = GameOf(ui.ListView->Selected());
                RunDOSBoxCommandLineOnFolder(MakeAbsPath(ExtractFilePath(G->GameExe()), PrgSetup->BaseDir()));
            }
            break;
        default:
            // profiles for the Windows emulators
            if (ATag >= 4100 && ATag <= 4199) AddProfileForWindowsEmulator(ATag - 4100);
            break;
    }
}

// ---------------------------------------------------------------------------
// Extras menu: icons, templates, images, collection maintenance, translations, cheats (commands 5xxx)
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::ExtrasMenuWork(QObject *Sender, int ATag)
{
    TGame *G;
    QString S, T;
    bool B = false;

    switch (ATag) {
        case 5001: S = ""; ShowIconManager(this, S, PrgSetup->GameDir(), true); break;
        case 5002: ShowHistoryDialog(this); break;
        case 5003:
            if (PrgSetup->GameDir() == "") S = PrgSetup->BaseDir(); else S = PrgSetup->GameDir();
            OpenFolder(S);
            break;
        case 5005:
            SetFormEnabled(false);
            G = ShowTemplateDialog(this, GameDB, SearchLinkFile, DeleteOnExit);
            SetFormEnabled(true);
            TrimWorkingSet();
            LoadSearchLinks();
            UpdateAddFromTemplateMenu();
            if (G == nullptr) return;
            ui.ListView->Items()->Clear();
            ReloadAndSelect(Sender, G);
            break;
        case 5006:
            SetListViewCustomDraw(false);
            UpdateGameNotes();
            ViewFilesFrame->SetGame(nullptr);
            if (!UninstallMultipleGames(this, GameDB)) {
                if (ui.ListView->Selected() != nullptr) ViewFilesFrame->SetGame(GameOf(ui.ListView->Selected()));
                SetListViewCustomDraw(true);
                return;
            }
            LastSelectedGame = nullptr;
            ui.ListView->SetSelected(nullptr);
            ui.ListView->Items()->Clear();
            SetListViewCustomDraw(true);
            InitTreeViewForGamesList(ui.TreeView, GameDB);
            TreeViewChange(Sender, ui.TreeView->currentItem());
            break;
        case 5007:
            ShowCreateImageFileDialog(this, true, true, GameDB);
            if (NewProfileFromImageCreator != nullptr) ReloadAndSelect(Sender, NewProfileFromImageCreator);
            break;
        case 5008: UpdateGameNotes(); TransferGames(this, GameDB); break;
        case 5010:
            if (ui.ListView->Selected() == nullptr || ui.ListView->Selected()->Data() == nullptr) G = nullptr; else G = GameOf(ui.ListView->Selected());
            if (!ShowMultipleProfilesEditorDialog(this, GameDB)) return;
            ReloadAndSelect(Sender, G);
            break;
        case 5011: ShowCreateISOImageDialog(this, false); break;
        case 5012: ShowCreateISOImageDialog(this, true); break;
        case 5013: ShowWriteIMGImageDialog(this); break;
        case 5014: ShowExpandImageDialog(this); break;
        case 5015: ShowBuildImageFromFolderDialog(this); break;
        case 5016: {
            TStringList *St;
#ifdef CheckDoubleChecksums
            TGameDB *AutoSetupDB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);
            SetFormEnabled(false);
            St = CheckDoubleChecksums(AutoSetupDB);
            SetFormEnabled(true);
            delete AutoSetupDB;
#else
            SetFormEnabled(false);
            St = CheckGameDB(GameDB);
            SetFormEnabled(true);
#endif
            if (St->Count() == 0) {
                MessageDlg(LanguageSetup->MissingFilesOK(), mtInformation, mbOK, 0);
            } else {
                ShowInfoTextDialog(this, LanguageSetup->MissingFilesCaption(), St);
            }
            delete St;
            break;
        }
        case 5026:
            // games moved or uninstalled (e.g. GOG and Steam games)
            UpdateGameNotes(); LastSelectedGame = nullptr;
            if (ShowMissingGamesDialog(this, GameDB)) {
                ui.ListView->Items()->Clear();
                InitTreeViewForGamesList(ui.TreeView, GameDB);
                TreeViewChange(Sender, ui.TreeView->currentItem());
            }
            break;
        case 5018:
            if (ShowScanGamesFolderResultsDialog(this, GameDB)) {
                InitTreeViewForGamesList(ui.TreeView, GameDB);
                TreeViewChange(Sender, ui.TreeView->currentItem());
            }
            break;
        case 5019: ShowCreateShortcutsDialog(this, GameDB); break;
        case 5020:
            if (!ShowLanguageEditorStartDialog(this, S)) return;
            ShowLanguageEditorDialog(this, S);
            if (Trim(ExtUpperCase(S)) == Trim(ExtUpperCase(LanguageSetup->SetupFile()))) {
                LanguageSetup->ReloadINI();
                LoadLanguage(PrgSetup->Language());
                LoadMenuLanguage();
                if (FSearchEditGray) {
                    SetSearchEditText(LanguageSetup->Search());
                }
                LoadGUISetup(false);
            }
            break;
        case 5021:
            SetFormEnabled(false);
            if (ui.ListView->Selected() != nullptr) G = GameOf(ui.ListView->Selected()); else G = nullptr;
            G = ShowMakeBootImageFromProfileDialog(this, GameDB, G);
            SetFormEnabled(true);
            if (G != nullptr) {
                ui.ListView->Items()->Clear();
                ReloadAndSelect(Sender, G);
            }
            break;
        case 5022:
            B = false;
            if (ShowCheatApplyDialog(this, MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir()), B) && B) {
                ShowCheatDBEditDialog(this, B);
                if (B) ShowUpdateCheckDialog(this, GameDB, SearchLinkFile);
            }
            break;
        case 5023:
            B = false;
            ShowCheatDBEditDialog(this, B);
            if (B) ShowUpdateCheckDialog(this, GameDB, SearchLinkFile);
            break;
        case 5024: ShowCheatSearchDialog(this); break;
        case 5025:
            SetFormEnabled(false);
            if (ShowDOSBoxLangStartDialog(this, S, T)) ShowDOSBoxLangEditDialog(this, S, T);
            SetFormEnabled(true);
            break;
    }
}

// ---------------------------------------------------------------------------
// Help menu: web pages, DOSBox / ScummVM help, statistics, about (commands 6xxx)
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::HelpMenuWork(QObject *Sender, int ATag)
{
    TGame *DefaultGame;
    QString S;
    const QString DOSBoxExe = DOSBoxExeFileOfInstallation(0);

    switch (ATag) {
        // DOSBox Staging (the DOSBox included with D-Fend Reborn); the manual
        // pages are versioned, DFR_STAGING_DOCS_VERSION: included version
        case 6001: ShellOpen(QStringLiteral("https://www.dosbox-staging.org/getting-started/introduction/")); break;
        case 6002: ShellOpen(QStringLiteral("https://www.dosbox-staging.org/" DFR_STAGING_DOCS_VERSION "/manual/appendices/shortcuts/")); break;
        case 6003:
            if (FileExists(DOSBoxExe)) {
                DefaultGame = new TGame(PrgSetup);
                RunCommand(DefaultGame, DeleteOnExit, "intro special", true);
                delete DefaultGame;
            } else {
                MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {DOSBoxExe}), mtError, mbOK, 0);
            }
            break;
        case 6004: ShellOpen(QStringLiteral("https://www.dosbox.com/comp_list.php?letter=a")); break;   // (DOSBox Staging has no compatibility list)
        case 6006: ShellOpen(QStringLiteral("https://www.dosbox-staging.org/manual/")); break;
        case 6007:
            if (FileExists(DOSBoxExe)) {
                DefaultGame = new TGame(PrgSetup);
                RunCommand(DefaultGame, DeleteOnExit, "intro", true);
                delete DefaultGame;
            } else {
                MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {DOSBoxExe}), mtError, mbOK, 0);
            }
            break;
        case 6008: ShellOpen(LanguageSetup->MenuHelpHomepageURL()); break;
        case 6009: // D-Fend Reborn: issue tracker of the project (the original opened its forum help page)
            ShellOpen(DFRebornHomepage + "/issues");
            break;
        case 6010: ShowUpdateCheckDialog(this, GameDB, SearchLinkFile); break;
        case 6014: ShowStatisticsDialog(this, GameDB); break;
        case 6015: ShowInfoDialog(this); break;
        case 6016: ShellOpen(QStringLiteral("https://www.dosbox-staging.org/")); break;
        case 6017: {
            // the readme of the ScummVM installation (newer versions: README.md),
            // otherwise the online documentation
            S = "";
            if (Trim(PrgSetup->ScummVMPath()) != "")
                for (const char *F : {"Readme.txt", "README.txt", "README.md", "README"})
                    if (FileExists(IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + F)) { S = IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + F; break; }
            if (S != "") OpenFileInEditor(S); else ShellOpen(QStringLiteral("https://docs.scummvm.org/"));
            break;
        }
        // (the old .php pages of scummvm.org do not exist any more)
        case 6018: ShellOpen(QStringLiteral("https://docs.scummvm.org/en/latest/help/faq.html")); break;
        case 6019: ShellOpen(QStringLiteral("https://www.scummvm.org/")); break;
        case 6020: ShellOpen(QStringLiteral("https://www.scummvm.org/compatibility/")); break;
        case 6021: ShowListScummVMGamesDialog(this); break;
        case 6022: ShowOperationModeInfoDialog(this); break;
        case 6024: ApplicationHelpCommand(HELP_CONTEXT, ID_Menu); break;
        case 6025: ApplicationHelpCommand(HELP_CONTEXT, ID_Index); break;
        case 6026: ApplicationHelpCommand(HELP_CONTEXT, ID_Introduction); break;
        case 6027: ApplicationHelpCommand(HELP_CONTEXT, ID_Introduction2); break;
        case 6028: ApplicationHelpCommand(HELP_CONTEXT, ID_HelpGetGames); break;
        case 6101:
            if (auto *A = qobject_cast<QAction *>(Sender))
                OpenFileInEditor(IncludeTrailingPathDelimiter(PrgSetup->DOSBoxSettings(0)->DosBoxDir()) + RemoveUnderline(A->text()));
            break;
        case 6102:
            if (Trim(ActionHint(Sender)) != "") {
                if (!OldGamesMenuUsed) {
                    MessageDlg(LanguageSetup->MenuHelpAbandonwareInfo(), mtInformation, mbOK, 0);
                    OldGamesMenuUsed = true;
                }
                ShellOpen(ActionHint(Sender));
            }
            break;
        case 6103: if (HelpMenuLinkFile->EditFile(true)) LoadAbandonLinks(); break;
    }
}
