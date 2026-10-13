// C++ port of BuildInstallerFormUnit.pas (implementation section).
#include "BuildInstallerFormUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "GameDBToolsUnit.h"
#include "UninstallFormUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#ifdef Q_OS_WIN
#include <QSettings>
#endif

TBuildInstallerForm* BuildInstallerForm = nullptr;

// TCheckListBox helpers
static bool CheckListChecked(QListWidget* L, int I) { return L->item(I)->checkState() == Qt::Checked; }
static void CheckListSetChecked(QListWidget* L, int I, bool B) { L->item(I)->setCheckState(B ? Qt::Checked : Qt::Unchecked); }
// ListBox.Items.Objects[I] (BuildCheckList stores the TGame pointer as Qt::UserRole data)
static TGame* CheckListGame(QListWidget* L, int I) { return static_cast<TGame*>(L->item(I)->data(Qt::UserRole).value<void*>()); }

// '\' in the original code (paths are '\' separated on Windows, '/' elsewhere)
static bool IsDelim(QChar C) { return C == '\\' || C == '/'; }
// "Copy(S,2,2)=':\'" - path could not be made relative to the base dir
static bool PathNotRelative(const QString& S)
{
    if (Copy(S, 2, 2) == ":\\" || Copy(S, 2, 2) == ":/") return true;
#ifndef Q_OS_WIN
    if (Copy(S, 1, 1) == "/") return true;
#endif
    return false;
}

TBuildInstallerForm::TBuildInstallerForm(QWidget* AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    SaveDialog = new TSaveDialog(this);
    SaveDialog->DefaultExt = "exe";

    connect(ui.DestFileButton, &QToolButton::clicked, this, [this]() { DestFileButtonClick(ui.DestFileButton); });
    connect(ui.SelectAllButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectAllButton); });
    connect(ui.SelectNoneButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectNoneButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.SelectGenreButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectGenreButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.DestFileButton3, &QToolButton::clicked, this, [this]() { DestFileButton3Click(ui.DestFileButton3); });
    connect(ui.SelectAllButton3, &QPushButton::clicked, this, [this]() { SelectButtonClick3(ui.SelectAllButton3); });
    connect(ui.SelectNoneButton3, &QPushButton::clicked, this, [this]() { SelectButtonClick3(ui.SelectNoneButton3); });
    connect(ui.SelectGenreButton3, &QPushButton::clicked, this, [this]() { SelectButtonClick3(ui.SelectGenreButton3); });
    connect(ui.OKButton3, &QPushButton::clicked, this, [this]() { OKButton3Click(ui.OKButton3); });
    connect(ui.HelpButton3, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton3); });
    connect(ui.DestFileButton2, &QToolButton::clicked, this, [this]() { DestFileButton2Click(ui.DestFileButton2); });
    connect(ui.OKButton2, &QPushButton::clicked, this, [this]() { OKButton2Click(ui.OKButton2); });
    connect(ui.SelectAllButton2, &QPushButton::clicked, this, [this]() { SelectButtonClick2(ui.SelectAllButton2); });
    connect(ui.SelectNoneButton2, &QPushButton::clicked, this, [this]() { SelectButtonClick2(ui.SelectNoneButton2); });
    connect(ui.SelectGenreButton2, &QPushButton::clicked, this, [this]() { SelectButtonClick2(ui.SelectGenreButton2); });
    connect(ui.HelpButton2, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton2); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });

    FormCreate(this);
}

TBuildInstallerForm::~TBuildInstallerForm()
{
    FormDestroy(this);
    delete SaveDialog;
}

void TBuildInstallerForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TBuildInstallerForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt (Unicode fonts)

    setWindowTitle(LanguageSetup->BuildInstaller());

    /* Games */

    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.GamesSheet), LanguageSetup->BuildInstallerTypeCompleteGames());
    ui.InfoLabel->setText(LanguageSetup->BuildInstallerInfo());
    ui.DestFileEdit_EditLabel->setText(LanguageSetup->BuildInstallerDestFile());
    ui.DestFileButton->setToolTip(LanguageSetup->ChooseFile());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.SelectAllButton->setText(LanguageSetup->All());
    ui.SelectNoneButton->setText(LanguageSetup->None());
    ui.SelectGenreButton->setText(LanguageSetup->GameBy());
    ui.GroupGamesCheckBox->setText(LanguageSetup->BuildInstallerGroupGames());
    ui.InstTypeRadioGroup->setTitle(LanguageSetup->BuildInstallerInstType());
    ui.InstTypeRadioGroup->setItemText(0, LanguageSetup->BuildInstallerInstTypeScriptOnly());
    ui.InstTypeRadioGroup->setItemText(1, LanguageSetup->BuildInstallerInstTypeFullInstaller());
    UserIconLoader->DialogImage(DI_SelectFile, ui.DestFileButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    /* AutoSetup templates */

    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.AutoSetupSheet), LanguageSetup->BuildInstallerTypeAutoSetupProfiles());
    ui.InfoLabel2->setText(LanguageSetup->BuildInstallerInfoAutoSetupTemplates());
    ui.DestFileEdit2_EditLabel->setText(LanguageSetup->BuildInstallerDestFile());
    ui.DestFileButton2->setToolTip(LanguageSetup->ChooseFile());
    ui.OKButton2->setText(LanguageSetup->OK());
    ui.CancelButton2->setText(LanguageSetup->Cancel());
    ui.HelpButton2->setText(LanguageSetup->Help());
    ui.SelectAllButton2->setText(LanguageSetup->All());
    ui.SelectNoneButton2->setText(LanguageSetup->None());
    ui.SelectGenreButton2->setText(LanguageSetup->GameBy());
    ui.GroupGamesCheckBox2->setText(LanguageSetup->BuildInstallerGroupGames());
    ui.InstTypeRadioGroup2->setTitle(LanguageSetup->BuildInstallerInstType());
    ui.InstTypeRadioGroup2->setItemText(0, LanguageSetup->BuildInstallerInstTypeScriptOnly());
    ui.InstTypeRadioGroup2->setItemText(1, LanguageSetup->BuildInstallerInstTypeFullInstaller());
    UserIconLoader->DialogImage(DI_SelectFile, ui.DestFileButton2);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton2);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton2);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton2);

    /* AutoSetup templates */

    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.TemplatesSheet), LanguageSetup->BuildInstallerTypeTemplates());
    ui.InfoLabel3->setText(LanguageSetup->BuildInstallerInfoTemplates());
    ui.DestFileEdit3_EditLabel->setText(LanguageSetup->BuildInstallerDestFile());
    ui.DestFileButton3->setToolTip(LanguageSetup->ChooseFile());
    ui.OKButton3->setText(LanguageSetup->OK());
    ui.CancelButton3->setText(LanguageSetup->Cancel());
    ui.HelpButton3->setText(LanguageSetup->Help());
    ui.SelectAllButton3->setText(LanguageSetup->All());
    ui.SelectNoneButton3->setText(LanguageSetup->None());
    ui.SelectGenreButton3->setText(LanguageSetup->GameBy());
    ui.GroupGamesCheckBox3->setText(LanguageSetup->BuildInstallerGroupGames());
    ui.InstTypeRadioGroup3->setTitle(LanguageSetup->BuildInstallerInstType());
    ui.InstTypeRadioGroup3->setItemText(0, LanguageSetup->BuildInstallerInstTypeScriptOnly());
    ui.InstTypeRadioGroup3->setItemText(1, LanguageSetup->BuildInstallerInstTypeFullInstaller());
    UserIconLoader->DialogImage(DI_SelectFile, ui.DestFileButton3);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton3);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton3);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton3);

    /* Dialog */

    SaveDialog->Title = LanguageSetup->BuildInstallerDestFileTitle();
    SaveDialog->Filter = LanguageSetup->BuildInstallerDestFileFilter();

    TemplateDB = new TGameDB(PrgDataDir() + TemplateSubDir, false);
    AutoSetupDB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);
}

void TBuildInstallerForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    delete TemplateDB; TemplateDB = nullptr;
    delete AutoSetupDB; AutoSetupDB = nullptr;
}

void TBuildInstallerForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    BuildCheckList(ui.ListBox, GameDB, false, false, true);
    BuildCheckList(ui.ListBox3, TemplateDB, false, false, true);
    BuildCheckList(ui.ListBox2, AutoSetupDB, false, false, true);
    BuildSelectPopupMenu(ui.PopupMenu, GameDB, [this](QObject* S) { SelectButtonClick(S); }, false, true);
    BuildSelectPopupMenu(ui.PopupMenu3, TemplateDB, [this](QObject* S) { SelectButtonClick3(S); }, false);
    BuildSelectPopupMenu(ui.PopupMenu2, AutoSetupDB, [this](QObject* S) { SelectButtonClick2(S); }, false);
}

void TBuildInstallerForm::SelectButtonClick(QObject* Sender)
{
    int I;

    if (qobject_cast<QPushButton*>(Sender) != nullptr) {
        switch (Sender->property("tag").toInt()) {
            case 0:
            case 1:
                for (I = 0; I < ui.ListBox->count(); I++) CheckListSetChecked(ui.ListBox, I, Sender->property("tag").toInt() == 0);
                break;
            case 2: {
                QPoint P = ui.SelectGenreButton->mapToGlobal(QPoint(0, 0));
                ui.PopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
                break;
            }
        }
        return;
    }

    SelectGamesByPopupMenu(Sender, ui.ListBox);
}

void TBuildInstallerForm::SelectButtonClick2(QObject* Sender)
{
    int I;

    if (qobject_cast<QPushButton*>(Sender) != nullptr) {
        switch (Sender->property("tag").toInt()) {
            case 0:
            case 1:
                for (I = 0; I < ui.ListBox2->count(); I++) CheckListSetChecked(ui.ListBox2, I, Sender->property("tag").toInt() == 0);
                break;
            case 2: {
                QPoint P = ui.SelectGenreButton2->mapToGlobal(QPoint(0, 0));
                ui.PopupMenu2->popup(QPoint(P.x() + 5, P.y() + 5));
                break;
            }
        }
        return;
    }

    SelectGamesByPopupMenu(Sender, ui.ListBox2);
}

void TBuildInstallerForm::SelectButtonClick3(QObject* Sender)
{
    int I;

    if (qobject_cast<QPushButton*>(Sender) != nullptr) {
        switch (Sender->property("tag").toInt()) {
            case 0:
            case 1:
                for (I = 0; I < ui.ListBox3->count(); I++) CheckListSetChecked(ui.ListBox3, I, Sender->property("tag").toInt() == 0);
                break;
            case 2: {
                QPoint P = ui.SelectGenreButton3->mapToGlobal(QPoint(0, 0));
                ui.PopupMenu3->popup(QPoint(P.x() + 5, P.y() + 5));
                break;
            }
        }
        return;
    }

    SelectGamesByPopupMenu(Sender, ui.ListBox3);
}

bool TBuildInstallerForm::GetNSIFileName(QString& NSIFileName, int Mode)
{
    QString S;

    switch (Mode) {
        case 0: S = ui.DestFileEdit->text(); break;
        case 1: S = ui.DestFileEdit3->text(); break;
        case 2: S = ui.DestFileEdit2->text(); break;
        default: S = ""; break;
    }

    NSIFileName = Trim(S);
    if (NSIFileName == "") {
        MessageDlg(LanguageSetup->MessageNoFileName(), mtError, mbOK, 0);
        return false;
    }
    NSIFileName = ChangeFileExt(NSIFileName, ".nsi");

    if (!CheckPath(ExtractFilePath(NSIFileName))) return false;

    if (FileExists(NSIFileName)) {
        if (MessageDlg(Format(LanguageSetup->MessageConfirmationOverwriteFile(), {NSIFileName}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return false;
    }

    return true;
}

// Common implementation of BuildNSIScript, BuildNSIScript2 and BuildNSIScript3
// (the three Delphi functions only differ in the controls and the Add function).
static bool BuildNSIScriptFromList(const QString& NSIFileName, const QString& DestFile, QListWidget* ListBox, bool GroupGames,
                                   bool (*AddFunc)(TStringList*, TGame*))
{
    TStringList St;
    int I, J;
    QString S;

    St.Add("OutFile \"" + ExtractFileName(DestFile) + "\"");
    St.Add("!include \".\\" + SettingsFolder + "\\" + NSIInstallerHelpFile + "\"");

    if (GroupGames) {
        TStringList GenreList;
        for (I = 0; I < ListBox->count(); I++) {
            if (!CheckListChecked(ListBox, I)) continue;
            S = Trim(ExtUpperCase(CheckListGame(ListBox, I)->Genre()));
            if (GenreList.IndexOf(S) < 0) {
                GenreList.Add(S);
                St.Add("");
                St.Add("SectionGroup \"" + CheckListGame(ListBox, I)->Genre() + "\"");
                for (J = I; J < ListBox->count(); J++) {
                    if (CheckListChecked(ListBox, J) && (Trim(ExtUpperCase(CheckListGame(ListBox, J)->Genre())) == S)) {
                        if (!AddFunc(&St, CheckListGame(ListBox, J))) return false;
                    }
                }
                St.Add("SectionGroupEnd");
            }
        }
    } else {
        for (I = 0; I < ListBox->count(); I++) {
            if (CheckListChecked(ListBox, I)) {
                if (!AddFunc(&St, CheckListGame(ListBox, I))) return false;
            }
        }
    }

    if (!St.SaveToFile(NSIFileName)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {NSIFileName}), mtError, mbOK, 0);
        return false;
    }

    return true;
}

bool TBuildInstallerForm::BuildNSIScript()
{
    QString NSIFileName;
    if (!GetNSIFileName(NSIFileName, 0)) return false;
    return BuildNSIScriptFromList(NSIFileName, ui.DestFileEdit->text(), ui.ListBox, ui.GroupGamesCheckBox->isChecked(), AddGameToNSIScript);
}

bool TBuildInstallerForm::BuildNSIScript2()
{
    QString NSIFileName;
    if (!GetNSIFileName(NSIFileName, 2)) return false;
    return BuildNSIScriptFromList(NSIFileName, ui.DestFileEdit2->text(), ui.ListBox2, ui.GroupGamesCheckBox2->isChecked(), AddAutoSetupToNSIScript);
}

bool TBuildInstallerForm::BuildNSIScript3()
{
    QString NSIFileName;
    if (!GetNSIFileName(NSIFileName, 1)) return false;
    return BuildNSIScriptFromList(NSIFileName, ui.DestFileEdit3->text(), ui.ListBox3, ui.GroupGamesCheckBox3->isChecked(), AddTemplateToNSIScript);
}

void TBuildInstallerForm::DestFileButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    S = Trim(ui.DestFileEdit->text());
    if ((S == "") || (Trim(ExtractFilePath(S)) == "")) SaveDialog->InitialDir = PrgDataDir(); else SaveDialog->InitialDir = Trim(ExtractFilePath(S));
    if (!SaveDialog->Execute()) return;
    ui.DestFileEdit->setText(SaveDialog->FileName);
}

void TBuildInstallerForm::DestFileButton2Click(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    S = Trim(ui.DestFileEdit2->text());
    if ((S == "") || (Trim(ExtractFilePath(S)) == "")) SaveDialog->InitialDir = PrgDataDir(); else SaveDialog->InitialDir = Trim(ExtractFilePath(S));
    if (!SaveDialog->Execute()) return;
    ui.DestFileEdit2->setText(SaveDialog->FileName);
}

void TBuildInstallerForm::DestFileButton3Click(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    S = Trim(ui.DestFileEdit3->text());
    if ((S == "") || (Trim(ExtractFilePath(S)) == "")) SaveDialog->InitialDir = PrgDataDir(); else SaveDialog->InitialDir = Trim(ExtractFilePath(S));
    if (!SaveDialog->Execute()) return;
    ui.DestFileEdit3->setText(SaveDialog->FileName);
}

void TBuildInstallerForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    if ((Trim(ExtractFilePath(ui.DestFileEdit->text())) == "") && (Trim(ui.DestFileEdit->text()) != ""))
        ui.DestFileEdit->setText(PrgDataDir() + ui.DestFileEdit->text());
    if (Trim(ui.DestFileEdit->text()) != "") ui.DestFileEdit->setText(ChangeFileExt(ui.DestFileEdit->text(), ".exe"));

    if (!BuildNSIScript()) { setModalResult(mrNone); return; }
    if (ui.InstTypeRadioGroup->itemIndex() == 1) {
        if (!BuildEXEInstaller(0, ui.DestFileEdit->text())) { setModalResult(mrNone); return; }
    }
}

void TBuildInstallerForm::OKButton2Click(QObject* Sender)
{
    Q_UNUSED(Sender);
    if ((Trim(ExtractFilePath(ui.DestFileEdit2->text())) == "") && (Trim(ui.DestFileEdit2->text()) != ""))
        ui.DestFileEdit2->setText(PrgDataDir() + ui.DestFileEdit2->text());
    if (Trim(ui.DestFileEdit2->text()) != "") ui.DestFileEdit2->setText(ChangeFileExt(ui.DestFileEdit2->text(), ".exe"));

    if (!BuildNSIScript2()) { setModalResult(mrNone); return; }
    if (ui.InstTypeRadioGroup2->itemIndex() == 1) {
        if (!BuildEXEInstaller(0, ui.DestFileEdit2->text())) { setModalResult(mrNone); return; }
    }
}

void TBuildInstallerForm::OKButton3Click(QObject* Sender)
{
    Q_UNUSED(Sender);
    if ((Trim(ExtractFilePath(ui.DestFileEdit3->text())) == "") && (Trim(ui.DestFileEdit3->text()) != ""))
        ui.DestFileEdit3->setText(PrgDataDir() + ui.DestFileEdit3->text());
    if (Trim(ui.DestFileEdit3->text()) != "") ui.DestFileEdit3->setText(ChangeFileExt(ui.DestFileEdit3->text(), ".exe"));

    if (!BuildNSIScript3()) { setModalResult(mrNone); return; }
    if (ui.InstTypeRadioGroup3->itemIndex() == 1) {
        if (!BuildEXEInstaller(0, ui.DestFileEdit3->text())) { setModalResult(mrNone); return; }
    }
}

void TBuildInstallerForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_FileExportBuildInstallers);
}

void TBuildInstallerForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

/* global */

bool BuildInstaller(QWidget* AOwner, TGameDB* AGameDB)
{
    bool result;
    BuildInstallerForm = new TBuildInstallerForm(AOwner);
    BuildInstallerForm->GameDB = AGameDB;
    result = (BuildInstallerForm->ShowModal() == mrOk);
    delete BuildInstallerForm;
    BuildInstallerForm = nullptr;
    return result;
}

bool CheckPath(const QString& NSIPath)
{
    QString S, T;
    S = Trim(IncludeTrailingPathDelimiter(ExtUpperCase(NSIPath)));
    T = Trim(IncludeTrailingPathDelimiter(ExtUpperCase(PrgDataDir())));
    if (S == T) return true;
    return (MessageDlg(Format(LanguageSetup->BuildInstallerPathWarning(), {ExcludeTrailingPathDelimiter(PrgDataDir()), ExcludeTrailingPathDelimiter(NSIPath)}), mtConfirmation, mbYes | mbNo, 0) == mrYes);
}

bool AddTemplateToNSIScript(TStringList* NSI, TGame* Game)
{
    Game->StoreAllValues();

    NSI->Add("");
    NSI->Add("Section \"" + LanguageSetup->BuildInstallerShortInfoTemplate() + ": " + Game->Name() + "\"");
    NSI->Add("  SetOutPath \"$DataInstDir\\Confs\"");
    NSI->Add("  File \".\\" + TemplateSubDir + "\\" + ExtractFileName(Game->SetupFile()) + "\"");
    NSI->Add("SectionEnd");

    return true;
}

bool AddAutoSetupToNSIScript(TStringList* NSI, TGame* Game)
{
    Game->StoreAllValues();

    NSI->Add("");
    NSI->Add("Section \"" + LanguageSetup->BuildInstallerShortInfoAutoSetupTemplate() + ": " + Game->Name() + "\"");
    NSI->Add("  SetOutPath \"$DataInstDir\\Confs\"");
    NSI->Add("  File \".\\" + AutoSetupSubDir + "\\" + ExtractFileName(Game->SetupFile()) + "\"");
    NSI->Add("SectionEnd");

    return true;
}

bool AddGameToNSIScript(TStringList* NSI, TGame* Game)
{
    auto AddFiles = [NSI](QString S) {
        QString T;
        S = MakeRelPath(IncludeTrailingPathDelimiter(S), PrgDataDir()); T = S;
        if ((Copy(T, 1, 1) == ".") && (Length(T) >= 2) && IsDelim(T[1])) T = Copy(T, 3, MaxInt);
        if ((T != "") && IsDelim(T[0])) T = Copy(T, 2, MaxInt);
        NSI->Add("  SetOutPath \"$DataInstDir\\" + T + "\"");
        NSI->Add("  File /nonfatal /r \"" + IncludeTrailingPathDelimiter(S) + "*.*\"");
    };
    // Adds a single file (repeated block of the original code)
    auto AddSingleFile = [NSI](QString S) {
        QString T = ExtractFilePath(S);
        if ((T != "") && (T[0] == '.')) Delete(T, 1, 1);
        if ((T != "") && IsDelim(T[0])) Delete(T, 1, 1);
        if ((T != "") && IsDelim(T[Length(T) - 1])) Delete(T, Length(T), 1);
        NSI->Add("  SetOutPath \"$DataInstDir\\" + T + "\"");
        if ((S != "") && IsDelim(S[0])) S = "." + S;
        NSI->Add("  File /nonfatal \"" + S + "\"");
    };
    auto PathError = [Game](const QString& S) {
        MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {S, Game->Name(), PrgSetup->BaseDir()}), mtError, mbOK, 0);
    };

    QString S;
    int I;
    TStringList *St, *Files, *Folders;

    Game->StoreAllValues();

    NSI->Add("");
    NSI->Add("Section \"" + Game->Name() + "\"");
    NSI->Add("  SetOutPath \"$DataInstDir\\Confs\"");
    NSI->Add("  File \".\\" + GameListSubDir + "\\" + ExtractFileName(Game->SetupFile()) + "\"");

    if (ScummVMMode(Game)) {
        S = Trim(Game->ScummVMPath());
        if (S != "") {
            S = IncludeTrailingPathDelimiter(MakeRelPath(S, PrgSetup->BaseDir()));
            if (PathNotRelative(S)) { PathError(S); return false; }
            AddFiles(S);
        }
        S = Trim(Game->ScummVMZip());
        if (S != "") {
            S = MakeRelPath(S, PrgSetup->BaseDir());
            if (PathNotRelative(S)) { PathError(S); return false; }
            AddSingleFile(S);
        }
        S = Trim(Game->ScummVMSavePath());
        if (S != "") {
            S = IncludeTrailingPathDelimiter(MakeRelPath(S, PrgSetup->BaseDir()));
            if (PathNotRelative(S)) { PathError(S); return false; }
            AddFiles(S);
        }
    } else {
        S = Trim(Game->GameExe());
        if (ExtUpperCase(Copy(S, 1, 7)) == "DOSBOX:") S = "";
        if (S == "") S = Trim(Game->SetupExe());
        if (ExtUpperCase(Copy(S, 1, 7)) == "DOSBOX:") S = "";
        if (S != "") {
            S = IncludeTrailingPathDelimiter(MakeRelPath(ExtractFilePath(S), PrgSetup->BaseDir()));
            if (PathNotRelative(S)) { PathError(S); return false; }
            AddFiles(S);
        }
    }

    S = Trim(Game->CaptureFolder());
    if (S != "") {
        S = IncludeTrailingPathDelimiter(MakeRelPath(S, PrgSetup->BaseDir()));
        if (PathNotRelative(S)) { PathError(S); return false; }
        AddFiles(S);
    }

    S = MakeAbsIconName(Game->Icon());
    if ((S != "") && FileExists(S)) {
        if (ExtractFilePath(Game->Icon()) == "") {
            NSI->Add("  SetOutPath \"$DataInstDir\\" + IconsSubDir + "\"");
            NSI->Add("  File /nonfatal \".\\" + IconsSubDir + "\\" + Trim(Game->Icon()) + "\"");
        } else {
            S = MakeRelPath(S, PrgSetup->BaseDir());
            if (PathNotRelative(S)) { PathError(S); return false; }
            AddSingleFile(S);
        }
    }

    S = Trim(Game->DataDir());
    if (S != "") {
        S = IncludeTrailingPathDelimiter(MakeRelPath(S, PrgSetup->BaseDir()));
        if (PathNotRelative(S)) { PathError(S); return false; }
        AddFiles(S);
    }

    S = Trim(Game->ExtraFiles());
    if (S != "") {
        St = ValueToList(S);
        std::unique_ptr<TStringList> StGuard(St);
        for (I = 0; I < St->Count(); I++) {
            if (Trim((*St)[I]) == "") continue;
            S = MakeRelPath((*St)[I], PrgSetup->BaseDir());
            if (PathNotRelative(S)) { PathError(S); return false; }
            AddSingleFile(S);
        }
    }

    S = Trim(Game->ExtraDirs());
    if (S != "") {
        St = ValueToList(S);
        std::unique_ptr<TStringList> StGuard(St);
        for (I = 0; I < St->Count(); I++) {
            if (Trim((*St)[I]) == "") continue;
            S = IncludeTrailingPathDelimiter(MakeRelPath(IncludeTrailingPathDelimiter((*St)[I]), PrgSetup->BaseDir()));
            if (PathNotRelative(S)) { PathError(S); return false; }
            AddFiles(S);
        }
    }

    FileAndFoldersFromDrives(Game, Files, Folders);
    std::unique_ptr<TStringList> FilesGuard(Files), FoldersGuard(Folders);
    for (I = 0; I < Files->Count(); I++) {
        S = MakeRelPath((*Files)[I], PrgSetup->BaseDir());
        if (PathNotRelative(S)) { PathError(S); return false; }
        AddSingleFile(S);
    }
    for (I = 0; I < Folders->Count(); I++) {
        S = IncludeTrailingPathDelimiter(MakeRelPath(IncludeTrailingPathDelimiter((*Folders)[I]), PrgSetup->BaseDir()));
        if (PathNotRelative(S)) { PathError(S); return false; }
        AddFiles(S);
    }

    NSI->Add("SectionEnd");

    return true;
}

// Name of the NSIS compiler executable inside the NSIS folder
static QString NSISCompilerName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("makensisw.exe");
#else
    return QStringLiteral("makensis");
#endif
}

QString GetNSISPath()
{
    QString result = "";
#ifdef Q_OS_WIN
    QSettings Reg("HKEY_LOCAL_MACHINE\\SOFTWARE\\NSIS", QSettings::NativeFormat);
    if (Reg.contains(".")) {
        result = Reg.value(".").toString();
        if (!FileExists(IncludeTrailingPathDelimiter(result) + "makensisw.exe")) result = "";
    }
#else
    // Port note: no registry - look for makensis in the search path instead
    QString Exe = QStandardPaths::findExecutable(NSISCompilerName());
    if (Exe != "") result = ExtractFilePath(Exe);
#endif
    return result;
}

bool BuildEXEInstaller(THandle Handle, const QString& FileName)
{
    Q_UNUSED(Handle);
    QString S;

    /* Check if NSIS is installed */
    S = GetNSISPath();

    if (S == "") {
        MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {NSISCompilerName()}) + "\r" + LanguageSetup->BuildInstallerNeedNSIS(), mtError, mbOK, 0);
        return false;
    }

    /* Copy D-Fend Reborn DataInstaller.nsi to PrgDataDir if missing */
    if (!FileExists(PrgDataDir() + SettingsFolder + PathDelim + NSIInstallerHelpFile)) {
        if (FileExists(PrgDir() + NSIInstallerHelpFile)) {
            CopyFile(PrgDir() + NSIInstallerHelpFile, PrgDataDir() + SettingsFolder + PathDelim + NSIInstallerHelpFile, false);
        } else {
            if (FileExists(PrgDir() + BinFolder + PathDelim + NSIInstallerHelpFile)) {
                CopyFile(PrgDir() + BinFolder + PathDelim + NSIInstallerHelpFile, PrgDataDir() + SettingsFolder + PathDelim + NSIInstallerHelpFile, false);
            }
        }
    }

    /* Execute NSIS */
    // ShellExecute(Handle,'open',makensisw.exe,'"<file>.nsi"',nil,SW_SHOW)
    StartProcess(IncludeTrailingPathDelimiter(S) + NSISCompilerName(), QStringList{ChangeFileExt(Trim(FileName), ".nsi")}, QString(), swNormal);

    return true;
}
