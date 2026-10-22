// Automated tests of the D-Fend Reborn windows.
//
// Every dialog is opened (with a test profile where it needs one), checked
// and closed again with Cancel:
//   * it opens without crashing,
//   * no text of a label, button, check box or radio button is cut off, no
//     such control lies outside of its parent and positioned buttons do not
//     overlap (all pages of tab controls and of the page trees of the profile
//     editor and the program options).
// Each dialog is tested with the normal font, with a larger font (as with
// display scaling or a larger system font) and with dark colors; with dark
// colors the texts must also be readable on their background (contrast).
//
// Run: ctest (or the DFRDialogTests executable). The tests work in a
// temporary program folder; the data of a D-Fend Reborn installation is not
// touched. With the environment variable DFR_DEBUG set, the sizes of the
// dialogs and of the controls with cut off texts (and their parents) are
// printed.
#include <QtTest>
#include <QTemporaryDir>
#include <cmath>

#include "CommonTools.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "LanguageSetupUnit.h"
#include "LoggingUnit.h"
#include "DOSBoxCountUnit.h"
#include "GameDBUnit.h"
#include "HistoryUnit.h"
#include "IconLoaderUnit.h"
#include "ImageCacheUnit.h"
#include "ScreenshotsCacheUnit.h"
#include "ListViewImageUnit.h"
#include "RunPrgManagerUnit.h"
#include "ScreensaverControlUnit.h"
#include "ZipManagerUnit.h"
#include "CheatDBUnit.h"
#include "ProgramUpdateCheckUnit.h"
#include "PackageManagerToolsUnit.h"
#include "TextEditPopupUnit.h"
#include "HelpTools.h"
#include "LinkFileUnit.h"
#include "HelpConsts.h"
#include "MainUnit.h"
#include "ColorScheme.h"
#include "Process.h"

#include "BuildImageFromFolderFormUnit.h"
#include "BuildZipPackagesFormUnit.h"
#include "CheatDBEditFormUnit.h"
#include "ChecksumFormUnit.h"
#include "CopyProfileFormUnit.h"
#include "CreateISOImageFormUnit.h"
#include "CreateImageUnit.h"
#include "CreateShortcutFormUnit.h"
#include "CreateShortcutsFormUnit.h"
#include "DOSBoxFailedFormUnit.h"
#include "DOSBoxLangStartFormUnit.h"
#include "DataReaderFormUnit.h"
#include "DragNDropErrorFormUnit.h"
#include "ExpandImageFormUnit.h"
#include "ExportGamesListFormUnit.h"
#include "ExtraExeEditFormUnit.h"
#include "FirstRunWizardFormUnit.h"
#include "FullscreenInfoFormUnit.h"
#include "HistoryFormUnit.h"
#include "ImportSelectTemplateFormUnit.h"
#include "InfoFormUnit.h"
#include "InstallationSupportFormUnit.h"
#include "LanguageEditorStartFormUnit.h"
#include "LinkFileEditFormUnit.h"
#include "MakeBootImageFromProfileFormUnit.h"
#include "MiniRunFormUnit.h"
#include "ModernProfileEditorFormUnit.h"
#include "MissingGamesFormUnit.h"
#include "MultipleProfilesEditorFormUnit.h"
#include "OperationModeInfoFormUnit.h"
#include "PackageCreationFormUnit.h"
#include "PackageManagerRepositoriesEditFormUnit.h"
#include "PackageManagerRepositoriesEditURLFormUnit.h"
#include "ProfileEditorFormUnit.h"
#include "ProfileMountEditorFormUnit.h"
#include "ProviderFormUnit.h"
#include "ResetProfilesFormUnit.h"
#include "ScanGamesFolderFormUnit.h"
#include "SelectAutoSetupFormUnit.h"
#include "SelectCDDriveToMountFormUnit.h"
#include "SelectProfilesFormUnit.h"
#include "SerialEditFormUnit.h"
#include "SetupFormUnit.h"
#include "SetupFrameDOSBoxFormUnit.h"
#include "StatisticsFormUnit.h"
#include "TemplateFormUnit.h"
#include "TextViewerFormUnit.h"
#include "UserInfoFormUnit.h"
#include "WallpaperStyleFormUnit.h"
#include "WindowsFileWarningFormUnit.h"
#include "WizardFormUnit.h"
#include "ZipPackageDBGLFormUnit.h"

namespace {

// What the menu command test records (messages, windows, started programs)
QStringList* Record = nullptr;

// Message boxes of the program code: no windows in the tests (questions are
// answered with Cancel / No)
int TestMessageDlg(const QString& Caption, const QString& Msg, TMsgDlgType DlgType, TMsgDlgButtons Buttons)
{
    Q_UNUSED(Caption); Q_UNUSED(DlgType);
    if (Record != nullptr) *Record << "message: " + Msg.simplified().left(80);
    if (Buttons & mbCancel) return mrCancel;
    if (Buttons & mbNo) return mrNo;
    return mrOk;
}

void CopyFolder(const QString& From, const QString& To)
{
    QDirIterator It(From, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (It.hasNext()) {
        const QString F = It.next();
        const QString T = To + "/" + QDir(From).relativeFilePath(F);
        QDir().mkpath(QFileInfo(T).absolutePath());
        QFile::copy(F, T);
    }
}

QString Describe(const QWidget* W)
{
    QString Text;
    if (auto* L = qobject_cast<const QLabel*>(W)) Text = L->text();
    if (auto* B = qobject_cast<const QAbstractButton*>(W)) Text = B->text();
    return QStringLiteral("%1 %2 \"%3\"").arg(W->metaObject()->className(), W->objectName(), Text.left(40));
}

// Is W inside a scrolling area (its contents may be larger than the visible part)?
bool InScrollArea(const QWidget* W, const QWidget* Top)
{
    for (const QWidget* P = W->parentWidget(); P != nullptr && P != Top; P = P->parentWidget())
        if (qobject_cast<const QAbstractScrollArea*>(P)) return true;
    return false;
}

// Labels, buttons, check boxes and radio buttons of the visible part of Dlg
// whose text is cut off or which lie outside of their parent
void CheckVisibleControls(QWidget* Dlg, const QString& Where, QStringList& Problems)
{
    const QList<QWidget*> Children = Dlg->findChildren<QWidget*>();
    for (QWidget* W : Children) {
        if (!W->isVisibleTo(Dlg) || W->width() <= 0) continue;
        QString Text;
        if (auto* L = qobject_cast<QLabel*>(W)) {
            if (L->wordWrap() || L->textFormat() == Qt::RichText || L->pixmap().isNull() == false) continue;
            Text = L->text();
        } else if (auto* B = qobject_cast<QAbstractButton*>(W)) {
            if (qobject_cast<QToolButton*>(W)) continue;   // icon buttons
            Text = B->text();
        } else {
            continue;
        }
        if (Text.trimmed().isEmpty() || InScrollArea(W, Dlg)) continue;
        if (W->property("vclPanelCaptionOf").isValid()) continue;   // caption drawn by its panel
        const int Need = W->sizeHint().width();
        if (W->width() + 2 < Need && qEnvironmentVariableIsSet("DFR_DEBUG"))
            for (QWidget* P = W; P; P = P->parentWidget()) qDebug() << P->metaObject()->className() << P->objectName() << P->size() << P->minimumSizeHint() << P->minimumSize() << P->sizePolicy();
        if (W->width() + 2 < Need)
            Problems << QStringLiteral("%1: %2 is cut off (width %3, needs %4)").arg(Where, Describe(W)).arg(W->width()).arg(Need);
        if (QWidget* P = W->parentWidget()) {
            const QRect R = W->geometry(), PR = P->rect().adjusted(-2, -2, 2, 2);
            if (!PR.contains(R))
                Problems << QStringLiteral("%1: %2 lies outside of %3 (%4,%5 %6x%7 in %8x%9)").arg(Where, Describe(W), P->objectName())
                                .arg(R.x()).arg(R.y()).arg(R.width()).arg(R.height()).arg(P->width()).arg(P->height());
        }
    }
}

// Contrast ratio of two colours (WCAG: 1 = same, 21 = black on white)
double ContrastRatio(const QColor& A, const QColor& B)
{
    auto L = [](const QColor& C) {
        auto Ch = [](double V) { return (V <= 0.03928) ? V / 12.92 : std::pow((V + 0.055) / 1.055, 2.4); };
        return 0.2126 * Ch(C.redF()) + 0.7152 * Ch(C.greenF()) + 0.0722 * Ch(C.blueF());
    };
    const double LA = L(A), LB = L(B);
    return (qMax(LA, LB) + 0.05) / (qMin(LA, LB) + 0.05);
}

// Texts of labels, buttons, check boxes and radio buttons must be readable on
// their background (e.g. no fixed dark blue link colour with dark colours)
void CheckContrast(QWidget* Dlg, const QString& Where, QStringList& Problems)
{
    for (QWidget* W : Dlg->findChildren<QWidget*>()) {
        if (!W->isVisibleTo(Dlg) || !W->isEnabled()) continue;
        QString Text;
        if (auto* L = qobject_cast<QLabel*>(W)) { if (!L->pixmap().isNull()) continue; Text = L->text(); }
        else if (auto* B = qobject_cast<QAbstractButton*>(W)) Text = B->text();
        else continue;
        if (Text.trimmed().isEmpty()) continue;
        const QColor Fg = W->palette().color(W->foregroundRole());
        QColor Bg;
        if (qobject_cast<QPushButton*>(W)) Bg = W->palette().color(QPalette::Button);
        else for (QWidget* P = W; P != nullptr; P = P->parentWidget()) {
            if (P->autoFillBackground() || P->isWindow() || (P->parentWidget() && qobject_cast<QAbstractScrollArea*>(P->parentWidget()->parentWidget()))) {
                Bg = P->palette().color(P->backgroundRole());
                break;
            }
        }
        const double R = ContrastRatio(Fg, Bg);
        if (R < 3.0)
            Problems << QStringLiteral("%1: %2 is hard to read (text %3 on %4, contrast %5)").arg(Where, Describe(W), Fg.name(), Bg.name()).arg(R, 0, 'f', 1);
    }
}

// Buttons positioned like in the original (not in a Qt layout) must not overlap
// (e.g. after a button got wider for its text)
void CheckButtonOverlaps(QWidget* Dlg, const QString& Where, QStringList& Problems)
{
    QList<QPushButton*> Buttons;
    for (QPushButton* B : Dlg->findChildren<QPushButton*>())
        if (B->isVisibleTo(Dlg) && B->parentWidget() && !B->parentWidget()->layout()) Buttons << B;
    for (int I = 0; I < Buttons.size(); I++)
        for (int J = I + 1; J < Buttons.size(); J++)
            if (Buttons[I]->parentWidget() == Buttons[J]->parentWidget() && Buttons[I]->geometry().intersects(Buttons[J]->geometry()))
                Problems << QStringLiteral("%1: %2 overlaps %3").arg(Where, Describe(Buttons[I]), Describe(Buttons[J]));
}

void Settle()
{
    for (int I = 0; I < 5; I++) { QCoreApplication::processEvents(); QTest::qWait(10); }
}

// Checks the dialog, also every page of its tab controls and, with
// TreePages, every page of its page tree
void CheckDialog(QWidget* Dlg, bool TreePages, QStringList& Problems)
{
    const QString Name = Dlg->objectName();
    if (qEnvironmentVariableIsSet("DFR_DEBUG")) qDebug() << "SIZE" << Name << Dlg->size() << "design" << Dlg->property("vclDesignWidth").toInt() << Dlg->property("vclDesignHeight").toInt();
    CheckVisibleControls(Dlg, Name, Problems);
    CheckButtonOverlaps(Dlg, Name, Problems);
    if (DarkColorScheme()) CheckContrast(Dlg, Name, Problems);

    if (TreePages) {
        for (QTreeWidget* Tree : Dlg->findChildren<QTreeWidget*>()) {
            if (!Tree->isVisibleTo(Dlg)) continue;
            QList<QTreeWidgetItem*> Items;
            for (QTreeWidgetItemIterator It(Tree); *It; ++It) Items << *It;
            for (QTreeWidgetItem* Item : Items) {
                Tree->setCurrentItem(Item);
                Settle();
                CheckVisibleControls(Dlg, Name + " / " + Item->text(0), Problems);
            }
        }
    }

    for (QTabWidget* Tabs : Dlg->findChildren<QTabWidget*>()) {
        if (!Tabs->isVisibleTo(Dlg)) continue;
        const int Old = Tabs->currentIndex();
        for (int I = 0; I < Tabs->count(); I++) {
            if (!Tabs->isTabVisible(I) || !Tabs->isTabEnabled(I) || I == Old) continue;
            Tabs->setCurrentIndex(I);
            Settle();
            CheckVisibleControls(Dlg, Name + " / " + Tabs->tabText(I), Problems);
        }
        Tabs->setCurrentIndex(Old);
    }
}

// Closes all modal windows (Cancel)
void CloseModalWindows()
{
    while (QWidget* M = QApplication::activeModalWidget()) {
        qInfo("closing %s %s", M->metaObject()->className(), qPrintable(M->objectName()));
        if (auto* D = qobject_cast<QDialog*>(M)) D->QDialog::done(QDialog::Rejected); else M->close();
        QCoreApplication::processEvents();
    }
}

struct TDialogTest {
    const char* Name;
    bool TreePages;
    std::function<void()> Open;   // shows the dialog modally
};

}  // namespace

class DialogTests : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir FPrgDir;
    QFont FDefaultFont;
    TGameDB* GameDB = nullptr;
    TGameDB* TemplateDB = nullptr;
    TGameDB* AutoSetupDB = nullptr;
    TLinkFile* SearchLinkFile = nullptr;
    TDFendRebornMainForm* MainForm = nullptr;
    QList<TDialogTest> Tests;

    // the DOSBox test profile
    TGame* TestGame()
    {
        for (int I = 0; I < GameDB->Count(); I++) if (GameDB->Game(I)->Name() == "Test Game") return GameDB->Game(I);
        return nullptr;
    }

    void InitTests()
    {
        auto S = [](const QString& V = QString()) { return V; };
        Tests = {
            {"BuildImageFromFolder", false, [] { ShowBuildImageFromFolderDialog(nullptr); }},
            {"BuildZipPackages", false, [this] { ShowBuildZipPackagesDialog(nullptr, GameDB); }},
            {"CheatDBEdit", false, [] { bool B = false; ShowCheatDBEditDialog(nullptr, B); }},
            {"CheckSumWarning", false, [] { ShowCheckSumWarningDialog(nullptr, "Test"); }},
            {"CopyProfile", false, [S] { QString N = S("Copy"); bool B1 = false, B2 = false; ShowCopyProfileDialog(nullptr, true, N, B1, B2); }},
            // (the Show functions need a CD / floppy drive: the forms are shown directly)
            {"CreateISOImage", false, [] { auto* F = new TCreateISOImageForm(nullptr); F->Drive = 'D'; F->ShowModal(); delete F; }},
            {"CreateFloppyImage", false, [] { auto* F = new TCreateISOImageForm(nullptr); F->Drive = 'A'; F->FloppyMode = true; F->ShowModal(); delete F; }},
            {"WriteIMGImage", false, [] { auto* F = new TCreateISOImageForm(nullptr); F->Drive = 'A'; F->FloppyMode = true; F->WriteMode = true; F->ShowModal(); delete F; }},
            {"CreateImageFile", false, [this] { ShowCreateImageFileDialog(nullptr, true, true, GameDB); }},
            {"CreateShortcut", false, [] { ShowCreateShortcutDialog(nullptr, "Test Game", "Test Game.prof", "", "GAME.EXE", "", false, false); }},
            {"CreateShortcuts", false, [this] { ShowCreateShortcutsDialog(nullptr, GameDB); }},
            {"DOSBoxFailed", false, [this] { ShowDOSBoxFailedDialog(nullptr, GameDB, TestGame()); }},
            {"DOSBoxLangStart", false, [] { QString A, B; ShowDOSBoxLangStartDialog(nullptr, A, B); }},
            {"DataReader", false, [] { QString N, G, D, P, Y, I, No; ShowDataReaderDialog(nullptr, "Test Game", N, G, D, P, Y, I, No, ""); }},
            {"DragNDropError", false, [] { TStringList L; L.Add("Error"); ShowDragNDropErrorDialog(nullptr, &L); }},
            {"ExpandImage", false, [] { ShowExpandImageDialog(nullptr); }},
            {"ExportGamesList", false, [this] { ShowExportGamesListDialog(nullptr, GameDB); }},
            {"ExtraExeEdit", false, [] { TStringList E, P; for (int I = 0; I < 50; I++) { E.Add(""); P.Add(""); } ShowExtraExeEditDialog(nullptr, &E, &P, false, "GAME.EXE", ""); }},
            {"FirstRunWizard", false, [] { bool B = false; ShowFirstRunWizardDialog(nullptr, B); }},
            {"FullscreenInfo", false, [] { ShowFullscreenInfoDialog(nullptr); }},
            {"History", false, [] { ShowHistoryDialog(nullptr); }},
            {"SelectTemplate", false, [this] { TStringList L; L.AddObject("0", (intptr_t)0); L.AddObject("1", (intptr_t)1); ShowSelectTemplateDialog(nullptr, TempPrgDir, AutoSetupDB, &L); }},
            {"Info", false, [] { ShowInfoDialog(nullptr); }},
            {"InstallationSupport", false, [this] { ShowInstallationSupportDialog(nullptr, GameDB); }},
            {"LanguageEditorStart", false, [] { QString F; ShowLanguageEditorStartDialog(nullptr, F); }},
            {"LinkFileEdit", false, [] { TStringList N, L; N.Add("Test"); L.Add("https://example.com/"); ShowLinkFileEditDialog(nullptr, &N, &L, true, false, 0); }},
            // (the Show function needs FreeDOS and mkfs.fat: the form is shown directly)
            {"MakeBootImageFromProfile", false, [this] { auto* F = new TMakeBootImageFromProfileForm(nullptr); F->GameDB = GameDB; F->SelectGame = TestGame(); F->ShowModal(); delete F; }},
            {"MiniRun", false, [this] { ShowMiniRunDialog(nullptr, GameDB); }},
            {"MissingGames", false, [this] { ShowMissingGamesDialog(nullptr, GameDB); }},
            {"MultipleProfilesEditor", false, [this] { ShowMultipleProfilesEditorDialog(nullptr, GameDB); }},
            {"MultipleTemplatesEditor", false, [this] { ShowMultipleProfilesEditorDialog(nullptr, TemplateDB, true); }},
            {"OperationModeInfo", false, [] { ShowOperationModeInfoDialog(nullptr); }},
            {"PackageCreation", false, [this] { ShowPackageCreationDialog(nullptr, GameDB); }},
            {"PackageManagerRepositoriesEdit", false, [] { ShowPackageManagerRepositoriesEditDialog(nullptr); }},
            {"PackageManagerRepositoriesEditURL", false, [] { QString U = "https://example.com/list.xml"; ShowPackageManagerRepositoriesEditURLDialog(nullptr, U); }},
            {"ProfileMountEditor", false, [this] { QString D = "./VirtualHD/;Drive;C;false;"; ShowProfileMountEditorDialog(nullptr, D, "C", TempPrgDir, TestGame()->SetupFile(), GameDB); }},
            {"Provider", false, [] { TStringList F; F.Add("Test.zip"); ShowProviderDialog(nullptr, &F, "Provider", "Text", "https://example.com/"); }},
            {"ResetProfiles", false, [this] { ShowResetProfilesDialog(nullptr, GameDB, SearchLinkFile); }},
            {"ScanGamesFolderResults", false, [this] { ShowScanGamesFolderResultsDialog(nullptr, GameDB); }},
            {"SelectAutoSetup", false, [this] { ShowSelectAutoSetupDialog(nullptr, AutoSetupDB); }},
            {"SelectCDDriveToMount", false, [] { QString C; ShowSelectCDDriveToMountDialog(nullptr, {"D:\\", "E:\\"}, "D", "Test Game", C); }},
            {"SelectProfiles", false, [this] { TList* L = nullptr; ShowSelectProfilesDialog(nullptr, GameDB, L); delete L; }},
            {"SerialEdit", false, [] { QString P = "dummy"; ShowSerialEditDialog(nullptr, P); }},
            {"Setup", true, [this] { ShowSetupDialog(nullptr, GameDB, SearchLinkFile); }},
            {"SetupFrameDOSBox", false, [this] { TDOSBoxData D; D.Name = "DOSBox"; ShowSetupFrameDOSBoxDialog(nullptr, D, true, GameDB); }},
            {"InfoText", false, [] { TStringList T; T.Add("Text"); ShowInfoTextDialog(nullptr, "Caption", &T); }},
            {"Statistics", false, [this] { ShowStatisticsDialog(nullptr, GameDB); }},
            {"Templates", false, [this] { TStringList L; ShowTemplateDialog(nullptr, GameDB, SearchLinkFile, &L); }},
            {"TextViewer", false, [] { ShowTextViewerDialog(nullptr, "Title", "Text"); }},
            {"UserInfo", false, [this] { ShowUserInfoDialog(nullptr, TestGame(), GameDB); }},
            {"WallpaperStyle", false, [] { TWallpaperStyle W = WSCenter; ShowWallpaperStyleDialog(nullptr, W); }},
            {"WindowsFileWarning", false, [] { bool B = false; ShowWindowsFileWarningDialog(nullptr, "GAME.EXE", B); }},
            {"Wizard", false, [this] { TGame* G = nullptr; bool B1 = false, B2 = false; ShowWizardDialog(nullptr, GameDB, "", false, G, B1, B2, SearchLinkFile); }},
            {"ZipPackageDBGL", false, [] { TStringList P; ShowZipPackageDBGLDialog(nullptr, "Title", "Author", "Notes", &P); }},
            {"ProfileEditor", true, [this] { TGame* G = TestGame(); TStringList L; ModernEditGameProfil(nullptr, GameDB, G, nullptr, SearchLinkFile, &L, "", ""); }},
            {"ClassicProfileEditor", false, [this] { TGame* G = TestGame(); TStringList L; EditGameProfil(nullptr, GameDB, G, nullptr, SearchLinkFile, &L, "", ""); }},
        };
    }

private slots:
    void initTestCase()
    {
        QVERIFY(FPrgDir.isValid());
        // program folder with the program data (languages, icons, templates)
        // and one test profile
        TempPrgDir = IncludeTrailingPathDelimiter(QDir::toNativeSeparators(FPrgDir.path()));
        CopyFolder(DFR_SOURCE_DIR "/data/Lang", FPrgDir.path() + "/Lang");
        CopyFolder(DFR_SOURCE_DIR "/data/IconSets", FPrgDir.path() + "/IconSets");
        CopyFolder(DFR_SOURCE_DIR "/data/NewUserData/Templates", FPrgDir.path() + "/Templates");
        QDir().mkpath(FPrgDir.path() + "/AutoSetup");
        for (const QString& F : QDir(DFR_SOURCE_DIR "/data/NewUserData/AutoSetup").entryList({"*.prof"}, QDir::Files, QDir::Name).mid(0, 3))
            QFile::copy(QString(DFR_SOURCE_DIR "/data/NewUserData/AutoSetup/") + F, FPrgDir.path() + "/AutoSetup/" + F);
        for (const char* F : {"Links.txt", "SearchLinks.txt"}) {
            QDir().mkpath(FPrgDir.path() + "/Settings");
            QFile::copy(QString(DFR_SOURCE_DIR "/data/Bin/") + F, FPrgDir.path() + "/Settings/" + F);
        }
        QDir().mkpath(FPrgDir.path() + "/Confs");
        QDir().mkpath(FPrgDir.path() + "/VirtualHD/GAME");

        MessageDlgHook = TestMessageDlg;
        // no programs, documents, web pages or folders are opened
        ExternalStartHook = [](const QString& What) {
            // (the end of the path / URL: the temporary folder differs on each run)
            QString W = QDir::fromNativeSeparators(What);
            while (W.endsWith('/')) W.chop(1);
            if (Record != nullptr) *Record << "start: " + W.mid(W.lastIndexOf('/') + 1).left(80);
        };
        PrgConsts_Initialization();
        LoggingUnit_Initialization();
        PrgSetupUnit_Initialization();
        // Not the first start: no DOSBox search on the whole disk, no first run
        // wizard, no searches for tools and no update of the program data
        PrgSetup->SetDOSBoxFullSearchDone(true);
        PrgSetup->SetDFendVersion(GetNormalFileVersionAsString());
        PrgSetupUnit_Finalization();   // (writes Settings/DFend.ini)
        PrgSetupUnit_Initialization();
        QVERIFY(!PrgSetup->FirstRun());
        LanguageSetupUnit_Initialization();
        DOSBoxCountUnit_Initialization();
        GameDBUnit_Initialization();
        HistoryUnit_Initialization();
        IconLoaderUnit_Initialization();
        ImageCacheUnit_Initialization();
        ScreenshotsCacheUnit_Initialization();
        ListViewImageUnit_Initialization();
        RunPrgManagerUnit_Initialization();
        ScreensaverControlUnit_Initialization();
        ZipManagerUnit_Initialization();
        CheatDBUnit_Initialization();
        ProgramUpdateCheckUnit_Initialization();
        PackageManagerToolsUnit_Initialization();
        TextEditPopupUnit_Initialization();
        HelpTools_Initialization();

        {
            TGame G(TempPrgDir + "Confs" + PathDelim + "Test Game.prof");
            G.SetName("Test Game");
            G.SetGameExe("VirtualHD\\GAME\\GAME.EXE");
            G.SetGenre("Adventure");
            G.SetNrOfMounts(1);
            G.SetMount0("./VirtualHD/;Drive;C;false;");
            G.StoreAllValues();
            G.UpdateFile();
        }
        // a ScummVM and a Windows profile (for the menu commands, which work
        // differently for them)
        QDir().mkpath(FPrgDir.path() + "/VirtualHD/MONKEY");
        QDir().mkpath(FPrgDir.path() + "/VirtualHD/WINGAME");
        QFile(FPrgDir.path() + "/VirtualHD/WINGAME/WINGAME.EXE").open(QIODevice::WriteOnly);
        {
            TGame G(TempPrgDir + "Confs" + PathDelim + "Test ScummVM Game.prof");
            G.SetName("Test ScummVM Game");
            G.SetProfileMode("ScummVM");
            G.SetScummVMGame("monkey");
            G.SetScummVMPath(".\\VirtualHD\\MONKEY\\");
            G.StoreAllValues();
            G.UpdateFile();
        }
        {
            TGame G(TempPrgDir + "Confs" + PathDelim + "Test Windows Game.prof");
            G.SetName("Test Windows Game");
            G.SetProfileMode("Windows");
            G.SetGameExe(".\\VirtualHD\\WINGAME\\WINGAME.EXE");
            G.StoreAllValues();
            G.UpdateFile();
        }
        GameDB = new TGameDB(TempPrgDir + "Confs" + PathDelim);
        QCOMPARE(GameDB->Count(), 3);
        QVERIFY(TestGame() != nullptr);
        TemplateDB = new TGameDB(TempPrgDir + "Templates" + PathDelim);
        AutoSetupDB = new TGameDB(TempPrgDir + "AutoSetup" + PathDelim);
        SearchLinkFile = new TLinkFile("SearchLinks.txt", false, ID_ProfileSearchGame);
        FDefaultFont = QApplication::font();

        // The main window (several dialogs use its image lists); windows it
        // opens while starting (first run wizard) are closed
        QTimer Closer;
        connect(&Closer, &QTimer::timeout, this, [] { CloseModalWindows(); });
        Closer.start(200);
        MainForm = new TDFendRebornMainForm();
        Closer.stop();
        QVERIFY(!MainForm->FormCreateAborted());

        InitTests();
    }

    void cleanupTestCase()
    {
        delete MainForm;
        delete SearchLinkFile;
        delete AutoSetupDB;
        delete TemplateDB;
        delete GameDB;
        HelpTools_Finalization();
        TextEditPopupUnit_Finalization();
        PackageManagerToolsUnit_Finalization();
        ProgramUpdateCheckUnit_Finalization();
        ZipManagerUnit_Finalization();
        ScreensaverControlUnit_Finalization();
        RunPrgManagerUnit_Finalization();
        ListViewImageUnit_Finalization();
        ScreenshotsCacheUnit_Finalization();
        ImageCacheUnit_Finalization();
        IconLoaderUnit_Finalization();
        HistoryUnit_Finalization();
        GameDBUnit_Finalization();
        DOSBoxCountUnit_Finalization();
        LanguageSetupUnit_Finalization();
        PrgSetupUnit_Finalization();
        TempPrgDir.clear();
    }

    // ----- main window -----------------------------------------------------

    void mainWindow_data()
    {
        QTest::addColumn<int>("FontIncrease");
        QTest::addColumn<bool>("Dark");
        QTest::newRow("normal font") << 0 << false;
        QTest::newRow("large font") << 4 << false;
        QTest::newRow("dark colors") << 0 << true;
    }

    void mainWindow()
    {
        QFETCH(int, FontIncrease);
        QFETCH(bool, Dark);
        QFont F = FDefaultFont;
        F.setPointSizeF(F.pointSizeF() + FontIncrease);
        QApplication::setFont(F);
        ApplyColorScheme(Dark ? csDark : csLight);
        MainForm->show();
        Settle();
        CloseModalWindows();
        QStringList Problems;
        CheckVisibleControls(MainForm, MainForm->objectName(), Problems);
        if (Dark) CheckContrast(MainForm, MainForm->objectName(), Problems);
        MainForm->hide();
        QApplication::setFont(FDefaultFont);
        ApplyColorScheme(csLight);
        QVERIFY2(Problems.isEmpty(), qPrintable("\n" + Problems.join("\n")));
    }

    // ----- dialogs ---------------------------------------------------------

    void dialogs_data()
    {
        QTest::addColumn<int>("Index");
        QTest::addColumn<int>("FontIncrease");
        QTest::addColumn<bool>("Dark");
        for (int I = 0; I < Tests.size(); I++) {
            QTest::newRow(QByteArray(Tests[I].Name) + "/normal font") << I << 0 << false;
            QTest::newRow(QByteArray(Tests[I].Name) + "/large font") << I << 4 << false;
            QTest::newRow(QByteArray(Tests[I].Name) + "/dark colors") << I << 0 << true;
        }
    }

    void dialogs()
    {
        QFETCH(int, Index);
        QFETCH(int, FontIncrease);
        QFETCH(bool, Dark);
        const TDialogTest& T = Tests[Index];

        QFont F = FDefaultFont;
        F.setPointSizeF(F.pointSizeF() + FontIncrease);
        QApplication::setFont(F);
        ApplyColorScheme(Dark ? csDark : csLight);

        // The dialog runs modally: a timer checks and closes it (with Cancel)
        QStringList Problems;
        bool Opened = false;
        QTimer Timer;
        Timer.setInterval(50);
        int Seen = 0;
        connect(&Timer, &QTimer::timeout, this, [&]() {
            QWidget* M = QApplication::activeModalWidget();
            if (M == nullptr) return;
            if (++Seen < 3) return;   // let the dialog finish its OnShow work
            Timer.stop();
            Opened = true;
            CheckDialog(M, T.TreePages, Problems);
            if (auto* D = qobject_cast<QDialog*>(M)) D->QDialog::done(QDialog::Rejected); else M->close();
        });
        Timer.start();
        T.Open();
        Timer.stop();
        // a second modal window (e.g. opened by the closing code) is closed too
        CloseModalWindows();
        QApplication::setFont(FDefaultFont);
        ApplyColorScheme(csLight);

        QVERIFY2(Opened, "the dialog was not shown");
        QVERIFY2(Problems.isEmpty(), qPrintable("\n" + Problems.join("\n")));
    }

    // ----- menu commands ---------------------------------------------------
    //
    // Every command of the main window (menus, tool bar and popup menus; one
    // action for each command number) is run once with the DOSBox test
    // profile selected, the commands of the Run and Profile menus also with
    // the ScummVM and the Windows test profile. Dialogs are closed with
    // Cancel, questions answered with No, nothing is started. The test checks
    // that no command crashes or leaves a window open, and prints what each
    // command did (windows, messages, programs / documents it would start):
    // with DFR_DEBUG set, these lines can be compared before and after
    // changes of the main window code.

    // The actions of a command (menu, tool bar, popup menus)
    QList<QAction*> CommandActions(int Tag)
    {
        QList<QAction*> L;
        for (QAction* A : MainForm->findChildren<QAction*>())
            if ((A->property("tag").toInt() == Tag) && (A->menu() == nullptr)) L << A;
        return L;
    }

    void menuCommands_data()
    {
        QTest::addColumn<QString>("Profile");
        QTest::addColumn<int>("Command");
        // not run: Quit, package manager and update check (internet)
        const QSet<int> Skip = {1007, 1011, 6010};
        QMap<int, QString> Commands;
        for (QAction* A : MainForm->findChildren<QAction*>()) {
            const int Tag = A->property("tag").toInt();
            if ((Tag < 1000) || (Tag > 6999) || Skip.contains(Tag) || A->menu() != nullptr) continue;
            const QString Name = A->objectName().isEmpty() ? A->text().remove('&') : A->objectName();
            if (!Commands.contains(Tag) || (Name < Commands[Tag])) Commands[Tag] = Name;
        }
        for (auto It = Commands.cbegin(); It != Commands.cend(); ++It)
            QTest::newRow(qPrintable(QString::number(It.key()) + " " + It.value())) << "Test Game" << It.key();
        for (const char* Profile : {"Test ScummVM Game", "Test Windows Game"})
            for (auto It = Commands.cbegin(); It != Commands.cend(); ++It)
                if ((It.key() >= 3000) && (It.key() <= 4999))
                    QTest::newRow(qPrintable(QString(Profile) + "/" + QString::number(It.key()) + " " + It.value())) << QString(Profile) << It.key();
    }

    void menuCommands()
    {
        QFETCH(QString, Profile);
        QFETCH(int, Command);
        MainForm->show();
        Settle();
        TListItems* Items = MainForm->ui.ListView->Items();
        for (int I = 0; I < Items->Count(); I++) {
            TGame* G = static_cast<TGame*>(Items->Item(I)->Data());
            if ((G != nullptr) && (G->Name() == Profile)) { MainForm->ui.ListView->SetSelected(Items->Item(I)); break; }
        }
        Settle();
        TListItem* Sel = MainForm->ui.ListView->Selected();
        QVERIFY2((Sel != nullptr) && (Sel->Data() != nullptr) && (static_cast<TGame*>(Sel->Data())->Name() == Profile), "the test profile is not selected");
        // an enabled action of the command (the menu items of a command may
        // be enabled differently)
        const QList<QAction*> Actions = CommandActions(Command);
        QVERIFY(!Actions.isEmpty());
        QAction* A = Actions[0];
        for (QAction* B : Actions) if (B->isEnabled()) { A = B; break; }

        QStringList Done;
        const QList<QWidget*> Before = QApplication::topLevelWidgets();
        QTimer Timer;
        Timer.setInterval(50);
        int Seen = 0;
        connect(&Timer, &QTimer::timeout, this, [&]() {
            QWidget* M = QApplication::activeModalWidget();
            if (M == nullptr) { Seen = 0; return; }
            if (++Seen < 3) return;   // let the dialog finish its OnShow work
            Seen = 0;
            Done << QStringLiteral("window: %1 %2").arg(M->metaObject()->className(), M->objectName());
            if (auto* D = qobject_cast<QDialog*>(M)) D->QDialog::done(QDialog::Rejected); else M->close();
        });
        Record = &Done;
        Timer.start();
        if (A->isEnabled()) A->trigger(); else Done << "disabled";
        Settle();
        Timer.stop();
        CloseModalWindows();
        // windows the command opened (not modal)
        for (QWidget* W : QApplication::topLevelWidgets())
            if (W->isVisible() && !Before.contains(W) && (W != MainForm) && !qobject_cast<QMenu*>(W)) {
                Done << QStringLiteral("window: %1 %2").arg(W->metaObject()->className(), W->objectName());
                W->close();
            }
        for (QMenu* M : MainForm->findChildren<QMenu*>()) if (M->isVisible()) { Done << "menu: " + M->objectName(); M->close(); }
        Settle();
        Record = nullptr;

        if (qEnvironmentVariableIsSet("DFR_DEBUG")) qInfo("COMMAND %s %d: %s", qPrintable(Profile), Command, qPrintable(Done.join(" | ")));
        QVERIFY2(QApplication::activeModalWidget() == nullptr, "a window is still open");
    }
};

QTEST_MAIN(DialogTests)
#include "DialogTests.moc"
