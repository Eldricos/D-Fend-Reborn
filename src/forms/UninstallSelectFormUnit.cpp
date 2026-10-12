// C++ port of UninstallSelectFormUnit.pas (implementation section).
#include "UninstallSelectFormUnit.h"

#include "LanguageSetupUnit.h"
#include "UninstallFormUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "WaitFormUnit.h"
#include "GameDBToolsUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"

TUninstallSelectForm* UninstallSelectForm = nullptr;

// TCheckListBox helpers
static bool CheckListChecked(QListWidget* L, int I) { return L->item(I)->checkState() == Qt::Checked; }
static void CheckListSetChecked(QListWidget* L, int I, bool B) { L->item(I)->setCheckState(B ? Qt::Checked : Qt::Unchecked); }
// ListBox.Items.Objects[I] (BuildCheckList stores the TGame pointer as Qt::UserRole data)
static TGame* CheckListGame(QListWidget* L, int I) { return static_cast<TGame*>(L->item(I)->data(Qt::UserRole).value<void*>()); }

TUninstallSelectForm::TUninstallSelectForm(QWidget* AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.SelectAllButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectAllButton); });
    connect(ui.SelectNoneButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectNoneButton); });
    connect(ui.SelectGenreButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectGenreButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });

    FormCreate(this);
}

TUninstallSelectForm::~TUninstallSelectForm() = default;

void TUninstallSelectForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TUninstallSelectForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt (Unicode fonts)

    setWindowTitle(LanguageSetup->UninstallSelectForm());
    ui.InfoLabel->setText(LanguageSetup->UninstallSelectFormInfo());
    ui.ActionsRadioGroup->setTitle(LanguageSetup->UninstallSelectFormActions());
    ui.ActionsRadioGroup->setItemText(0, LanguageSetup->UninstallSelectFormActionDeleteRecord());
    ui.ActionsRadioGroup->setItemText(1, LanguageSetup->UninstallSelectFormActionDeleteProgramDir());
    ui.ActionsRadioGroup->setItemText(2, LanguageSetup->UninstallSelectFormActionDeleteAllData());
    ui.ActionsRadioGroup->setItemText(3, LanguageSetup->UninstallSelectFormActionAsk());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.SelectAllButton->setText(LanguageSetup->All());
    ui.SelectNoneButton->setText(LanguageSetup->None());
    ui.SelectGenreButton->setText(LanguageSetup->GameBy());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
}

void TUninstallSelectForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    BuildCheckList(ui.ListBox, GameDB, false, false, true);
    BuildSelectPopupMenu(ui.PopupMenu, GameDB, [this](QObject* S) { SelectButtonClick(S); }, false, true);
}

void TUninstallSelectForm::SelectButtonClick(QObject* Sender)
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

void TUninstallSelectForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I, J;
    QString S;
    TGame* G;
    TStringList *St, *Files, *Folders;
    bool ContinueNext = false;

    SetCurrentDir(PrgDataDir());

    J = 0;
    const int ItemCount = ui.ListBox->count();
    for (I = 0; I < ItemCount; I++) if (CheckListChecked(ui.ListBox, I)) J++;
    InitProgressWindow(this, J);
    struct ProgressGuard { ~ProgressGuard() { DoneProgressWindow(); } } Guard;  // try/finally

    for (I = 0; I < ItemCount; I++) {
        if (!CheckListChecked(ui.ListBox, I)) continue;
        G = CheckListGame(ui.ListBox, I);

        StepProgressWindow();

        if (ui.ActionsRadioGroup->itemIndex() == 3) {
            if (!UninstallGame(this, GameDB, G)) return;
            continue;
        }

        if (ui.ActionsRadioGroup->itemIndex() > 0) {
            if (ScummVMMode(G)) {
                S = Trim(G->ScummVMPath());
                if ((S != "") && (!UsedByOtherGame(GameDB, G, IncludeTrailingPathDelimiter(S)))) {
                    S = ExtractFilePath(MakeAbsPath(IncludeTrailingPathDelimiter(S), PrgSetup->BaseDir()));
                    if (BaseDirSecuriryCheck(S)) {
                        if ((!ExtDeleteFolder(S, ftUninstall, true, ContinueNext)) && (!ContinueNext)) return;
                    }
                    ProcessMessages();
                }
                S = Trim(G->ScummVMZip());
                if ((S != "") && (!ExtraFileUsedByOtherGame(GameDB, G, S))) {
                    S = MakeAbsPath(S, PrgSetup->BaseDir());
                    if (BaseDirSecuriryCheck(ExtractFilePath(S))) {
                        if ((!ExtDeleteFile(S, ftUninstall, true, ContinueNext)) && (!ContinueNext)) return;
                    }
                    ProcessMessages();
                }
                S = Trim(G->ScummVMSavePath());
                if ((S != "") && (!UsedByOtherGame(GameDB, G, IncludeTrailingPathDelimiter(S)))) {
                    S = ExtractFilePath(MakeAbsPath(IncludeTrailingPathDelimiter(S), PrgSetup->BaseDir()));
                    if (BaseDirSecuriryCheck(S)) {
                        if ((!ExtDeleteFolder(S, ftUninstall, true, ContinueNext)) && (!ContinueNext)) return;
                    }
                    ProcessMessages();
                }
            } else {
                S = Trim(G->GameExe());
                if (ExtUpperCase(Copy(S, 1, 7)) == "DOSBOX:") S = "";
                if (S == "") S = Trim(G->SetupExe());
                if (ExtUpperCase(Copy(S, 1, 7)) == "DOSBOX:") S = "";
                if (S != "") S = ExtractFilePath(S);
                if ((S != "") && (!UsedByOtherGame(GameDB, G, IncludeTrailingPathDelimiter(S)))) {
                    S = ExtractFilePath(MakeAbsPath(IncludeTrailingPathDelimiter(S), PrgSetup->BaseDir()));
                    if (BaseDirSecuriryCheck(S)) {
                        if ((!ExtDeleteFolder(S, ftUninstall, true, ContinueNext)) && (!ContinueNext)) return;
                    }
                    ProcessMessages();
                }
            }
        }

        if (ui.ActionsRadioGroup->itemIndex() > 1) {
            S = Trim(G->CaptureFolder());
            if ((S != "") && (!UsedByOtherGame(GameDB, G, IncludeTrailingPathDelimiter(S)))) {
                S = ExtractFilePath(MakeAbsPath(IncludeTrailingPathDelimiter(S), PrgSetup->BaseDir()));
                if (BaseDirSecuriryCheck(S)) {
                    if ((!ExtDeleteFolder(S, ftUninstall, true, ContinueNext)) && (!ContinueNext)) return;
                }
                ProcessMessages();
            }

            S = MakeAbsIconName(G->Icon());
            if ((S != "") && FileExists(S) && (!IconUsedByOtherGame(GameDB, G, S))) {
                if (BaseDirSecuriryCheck(ExtractFilePath(S))) {
                    if ((!ExtDeleteFile(S, ftUninstall, true, ContinueNext)) && (!ContinueNext)) return;
                }
                ProcessMessages();
            }

            S = Trim(G->DataDir());
            if ((S != "") && (!UsedByOtherGame(GameDB, G, IncludeTrailingPathDelimiter(S)))) {
                S = ExtractFilePath(MakeAbsPath(IncludeTrailingPathDelimiter(S), PrgSetup->BaseDir()));
                if (BaseDirSecuriryCheck(S)) {
                    if ((!ExtDeleteFolder(S, ftUninstall, true, ContinueNext)) && (!ContinueNext)) return;
                }
                ProcessMessages();
            }

            S = Trim(G->ExtraFiles());
            if (S != "") {
                St = ValueToList(S);
                std::unique_ptr<TStringList> StGuard(St);
                for (J = 0; J < St->Count(); J++) {
                    if ((Trim((*St)[J]) != "") && (!ExtraFileUsedByOtherGame(GameDB, G, (*St)[J]))) {
                        S = MakeAbsPath((*St)[J], PrgSetup->BaseDir());
                        if (BaseDirSecuriryCheck(ExtractFilePath(S))) {
                            if (!ExtDeleteFile(S, ftUninstall, true, ContinueNext) && (!ContinueNext)) return;
                        }
                        ProcessMessages();
                    }
                }
            }

            S = Trim(G->ExtraDirs());
            if (S != "") {
                St = ValueToList(S);
                std::unique_ptr<TStringList> StGuard(St);
                for (J = 0; J < St->Count(); J++) {
                    if ((Trim((*St)[J]) != "") && (!UsedByOtherGame(GameDB, G, IncludeTrailingPathDelimiter((*St)[J])))) {
                        S = ExtractFilePath(MakeAbsPath(IncludeTrailingPathDelimiter((*St)[J]), PrgSetup->BaseDir()));
                        if (BaseDirSecuriryCheck(S)) {
                            if ((!ExtDeleteFolder(S, ftUninstall, true, ContinueNext)) && (!ContinueNext)) return;
                        }
                        ProcessMessages();
                    }
                }
            }

            UnusedFileAndFoldersFromDrives(GameDB, G, Files, Folders);
            std::unique_ptr<TStringList> FilesGuard(Files), FoldersGuard(Folders);
            for (J = 0; J < Files->Count(); J++) {
                S = (*Files)[J];
                ui.ListBox->addItem(LanguageSetup->UninstallFormExtraFile() + ": " + S);
                if (BaseDirSecuriryCheck(ExtractFilePath(S))) {
                    if (!ExtDeleteFile(S, ftUninstall, true, ContinueNext) && (!ContinueNext)) return;
                }
                ProcessMessages();
            }
            for (J = 0; J < Folders->Count(); J++) {
                S = (*Folders)[J];  // original: S:=Files[J] (index out of range bug)
                if (BaseDirSecuriryCheck(S)) {
                    if ((!ExtDeleteFolder(S, ftUninstall, true, ContinueNext)) && (!ContinueNext)) return;
                }
                ProcessMessages();
            }
        }

        GameDB->Delete(G);
    }
}

void TUninstallSelectForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ExtrasUninstallMultipleGames);
}

void TUninstallSelectForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

/* global */

bool UninstallMultipleGames(QWidget* AOwner, TGameDB* AGameDB)
{
    bool result;
    UninstallSelectForm = new TUninstallSelectForm(AOwner);
    UninstallSelectForm->GameDB = AGameDB;
    result = (UninstallSelectForm->ShowModal() == mrOk);
    delete UninstallSelectForm;
    UninstallSelectForm = nullptr;
    return result;
}
