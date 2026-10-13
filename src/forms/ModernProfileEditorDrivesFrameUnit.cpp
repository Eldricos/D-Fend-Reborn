// C++ port of ModernProfileEditorDrivesFrameUnit.pas (implementation section).
#include "ModernProfileEditorDrivesFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "ProfileMountEditorFormUnit.h"
#include "HelpConsts.h"
#include "GameDBToolsUnit.h"
#include "IconLoaderUnit.h"
#include "ModernProfileEditorBaseFrameUnit.h"
#include "PrgConsts.h"
#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

/* TModernProfileEditorDrivesFrame */

TModernProfileEditorDrivesFrame::TModernProfileEditorDrivesFrame(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    ImageList = TImageList::FromResource("ModernProfileEditorDrivesFrameUnit", "ImageList");

    connect(ui.MountingListView, &TListView::dblClick, this, [this]() { MountingListViewDblClick(ui.MountingListView); });
    connect(ui.MountingListView, &TListView::keyDown, this, [this](int Key, Qt::KeyboardModifiers Mods) {
        if (Key == Qt::Key_Enter) Key = Qt::Key_Return;
        MountingListViewKeyDown(ui.MountingListView, Key, Mods & ~Qt::KeypadModifier);
    });
    for (QPushButton* B : {ui.MountingAddButton, ui.MountingEditButton, ui.MountingDelButton, ui.MountingDeleteAllButton, ui.MountingAutoCreateButton})
        connect(B, &QPushButton::clicked, this, [this, B]() { ButtonWork(B); });
    for (QAction* A : {ui.PopupAdd, ui.PopupEdit, ui.PopupDelete}) {
        // The keys are handled by MountingListViewKeyDown; the shortcuts are only shown in the menu
        A->setShortcutContext(Qt::WidgetShortcut);
        connect(A, &QAction::triggered, this, [this, A]() { ButtonWork(A); });
    }

    Mounting = nullptr;
}

void TModernProfileEditorDrivesFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    // NoFlicker(...): not needed with Qt

    ui.MountingAddButton->setText(LanguageSetup->ProfileEditorMountingAdd());
    ui.MountingEditButton->setText(LanguageSetup->ProfileEditorMountingEdit());
    ui.MountingDelButton->setText(LanguageSetup->ProfileEditorMountingDel());
    ui.MountingDeleteAllButton->setText(LanguageSetup->ProfileEditorMountingDelAll());
    ui.MountingAutoCreateButton->setText(LanguageSetup->ProfileEditorMountingAutoCreate());
    InitMountingListView(ui.MountingListView);
    ui.AutoMountCheckBox->setText(LanguageSetup->ProfileEditorMountingAutoMountCDs());
    ui.SecureModeCheckBox->setText(LanguageSetup->ProfileEditorMountingSecureMode());
    UserIconLoader->DialogImage(DI_Add, ui.MountingAddButton);
    UserIconLoader->DialogImage(DI_Edit, ui.MountingEditButton);
    UserIconLoader->DialogImage(DI_Delete, ui.MountingDelButton);
    UserIconLoader->DialogImage(DI_Wizard, ui.MountingAutoCreateButton);

    ui.PopupAdd->setText(LanguageSetup->ProfileEditorMountingAdd());
    ui.PopupEdit->setText(LanguageSetup->ProfileEditorMountingEdit());
    ui.PopupDelete->setText(LanguageSetup->ProfileEditorMountingDel());
    ui.PopupEdit->setShortcut(QKeySequence(Qt::Key_Return));
    UserIconLoader->DialogImage(DI_Add, ImageList, 0);
    UserIconLoader->DialogImage(DI_Edit, ImageList, 1);
    UserIconLoader->DialogImage(DI_Delete, ImageList, 2);
    if (ImageList->Count() > 0) ui.PopupAdd->setIcon(ImageList->Icon(0));
    if (ImageList->Count() > 1) ui.PopupEdit->setIcon(ImageList->Icon(1));
    if (ImageList->Count() > 2) ui.PopupDelete->setIcon(ImageList->Icon(2));

    ProfileExe = InitData.CurrentProfileExe;
    ProfileSetup = InitData.CurrentProfileSetup;
    ProfileName = InitData.CurrentProfileName;

    FGetFrame = InitData.GetFrame;

    GameDB = InitData.GameDB;

    setProperty("HelpContext", ID_ProfileEditDrives);  // HelpContext
}

void TModernProfileEditorDrivesFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    int I;

    if (Mounting != nullptr) { delete Mounting; Mounting = nullptr; }
    Mounting = new TStringList();
    for (I = 0; I <= 9; I++)
        if (Game->NrOfMounts() >= I + 1) Mounting->Add(Game->Mount(I)); else break;
    LoadMountingList();
    ui.AutoMountCheckBox->setChecked(Game->AutoMountCDs());
    ui.SecureModeCheckBox->setChecked(Game->SecureMode());
}

void TModernProfileEditorDrivesFrame::LoadMountingList()
{
    LoadMountingListView(ui.MountingListView, Mounting);
}

QChar TModernProfileEditorDrivesFrame::NextFreeDriveLetter()
{
    QChar result;
    int I;
    bool B;
    TStringList* St;

    result = 'C';
    do {
        B = true;
        for (I = 0; I < Mounting->Count(); I++) {
            St = ValueToList((*Mounting)[I]);
            if (St->Count() < 3) { delete St; continue; }
            if (UpperCase((*St)[2]) == QString(result)) { result = QChar(result.unicode() + 1); B = false; delete St; break; }
            delete St;
        }
    } while (!B);
    return result;
}

QString TModernProfileEditorDrivesFrame::UsedDriveLetters(int AllowedNr)
{
    QString result;
    int I;
    TStringList* St;

    result = "";
    for (I = 0; I < Mounting->Count(); I++) if (I != AllowedNr) {
        St = ValueToList((*Mounting)[I]);
        if (St->Count() >= 3) result = result + UpperCase((*St)[2]);
        delete St;
    }
    return result;
}

static QString ShortName(const QString& LongName)
{
#ifdef Q_OS_WIN
    wchar_t Buf[MAX_PATH + 10];
    std::wstring L = QDir::toNativeSeparators(LongName).toStdWString();
    if (GetShortPathNameW(L.c_str(), Buf, MAX_PATH) == 0)
        return LongName;
    else
        return QString::fromWCharArray(Buf);
#else
    // No 8.3 short names on other systems
    return LongName;
#endif
}

bool TModernProfileEditorDrivesFrame::CanReachFile(const QString& FileName)
{
    bool result;
    QString S, FilePath;
    TStringList* St;
    int I;

    result = false;
    FilePath = Trim(ExtUpperCase(IncludeTrailingPathDelimiter(ShortName(MakeAbsPath(ExtractFilePath(FileName), PrgSetup->BaseDir())))));
    for (I = 0; I < Mounting->Count(); I++) {
        St = ValueToList((*Mounting)[I]);
        /* Types to check:
           RealFolder;DRIVE;Letter;False;;FreeSpace
           RealFolder;FLOPPY;Letter;False;;
           RealFolder;CDROM;Letter;IO;Label;
           all other are images */
        if (St->Count() < 2) { delete St; continue; }
        S = Trim(ExtUpperCase((*St)[1]));
        if (S != "DRIVE" && S != "FLOPPY" && S != "CDROM") { delete St; continue; }
        S = Trim(ExtUpperCase(IncludeTrailingPathDelimiter(ShortName(MakeAbsPath((*St)[0], PrgSetup->BaseDir())))));

        result = (Copy(FilePath, 1, Length(S)) == S);
        delete St;
        if (result) return result;
    }
    return result;
}

void TModernProfileEditorDrivesFrame::ButtonWork(QObject* Sender)
{
    QString S, InitialDir;
    int I;
    TStringList* St;
    TModernProfileEditorBaseFrame* F;

    F = dynamic_cast<TModernProfileEditorBaseFrame*>(FGetFrame([](QWidget* P) -> QWidget* { return new TModernProfileEditorBaseFrame(P); }));
    if ((F != nullptr && F->ui.GameRelPathCheckBox->isChecked()) || Trim(*ProfileExe) == "")
        InitialDir = MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir());
    else
        InitialDir = IncludeTrailingPathDelimiter(ExtractFilePath(*ProfileExe));

    switch (Sender->property("tag").toInt()) {
    case 0:
        if (Mounting->Count() < 10) {
            S = "";
            if (!ShowProfileMountEditorDialog(this, S, UsedDriveLetters(), InitialDir, *ProfileName, GameDB, F, QString(NextFreeDriveLetter()))) return;
            Mounting->Add(S);
            LoadMountingList();
            ui.MountingListView->SetItemIndex(ui.MountingListView->Items()->Count() - 1);
        }
        break;
    case 1:
        I = ui.MountingListView->ItemIndex();
        if (I < 0) return;
        S = (*Mounting)[I];
        if (!ShowProfileMountEditorDialog(this, S, UsedDriveLetters(I), InitialDir, *ProfileName, GameDB, F)) return;
        (*Mounting)[I] = S;
        LoadMountingList();
        ui.MountingListView->SetItemIndex(I);
        break;
    case 2:
        I = ui.MountingListView->ItemIndex();
        if (I < 0) return;
        Mounting->Delete(I);
        LoadMountingList();
        if (Mounting->Count() > 0) ui.MountingListView->SetItemIndex(Max(0, I - 1));
        break;
    case 3:
        if (Mounting->Count() > 0 && MessageDlg(LanguageSetup->ProfileEditorMountingDeleteAllMessage(), mtConfirmation, mbYes | mbNo, 0) == mrYes) {
            Mounting->Clear();
            LoadMountingList();
        }
        break;
    case 4:
        I = 0;
        while (I < Mounting->Count()) {
            St = ValueToList((*Mounting)[I]);
            if (St->Count() >= 3 && (*St)[2] == "C") { Mounting->Delete(I); delete St; continue; }
            I++;
            delete St;
        }
        /* Add VirtualHD dir */
        if (Mounting->Count() < 10) Mounting->Insert(0, MakeRelPath(PrgSetup->GameDir(), PrgSetup->BaseDir(), true) + ";Drive;C;false;");
        /* Add game dir if needed */
        if (Mounting->Count() < 10 && Trim(*ProfileExe) != "" && (!CanReachFile(*ProfileExe)) && ExtUpperCase(Copy(*ProfileExe, 1, 7)) != "DOSBOX:") {
            S = IncludeTrailingPathDelimiter(MakeRelPath(ExtractFilePath(*ProfileExe), PrgSetup->BaseDir()));
            Mounting->Add(S + ";DRIVE;" + NextFreeDriveLetter() + ";False;;" + IntToStr(DefaultFreeHDSize));
        }
        /* Add setup dir if needed */
        if (Mounting->Count() < 10 && Trim(*ProfileSetup) != "" && (!CanReachFile(*ProfileSetup)) && ExtUpperCase(Copy(*ProfileSetup, 1, 7)) != "DOSBOX:") {
            S = IncludeTrailingPathDelimiter(MakeRelPath(ExtractFilePath(*ProfileSetup), PrgSetup->BaseDir()));
            Mounting->Add(S + ";DRIVE;" + NextFreeDriveLetter() + ";False;;" + IntToStr(DefaultFreeHDSize));
        }
        LoadMountingList();
        break;
    }
}

void TModernProfileEditorDrivesFrame::MountingListViewDblClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ButtonWork(ui.MountingEditButton);
}

void TModernProfileEditorDrivesFrame::MountingListViewKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if (Shift == Qt::ControlModifier && Key == Qt::Key_Return) { ButtonWork(ui.MountingEditButton); return; }
    if (Shift != Qt::NoModifier) return;
    switch (Key) {
    case Qt::Key_Insert: ButtonWork(ui.MountingAddButton); break;
    case Qt::Key_Return: ButtonWork(ui.MountingEditButton); break;
    case Qt::Key_Delete: ButtonWork(ui.MountingDelButton); break;
    }
}

void TModernProfileEditorDrivesFrame::GetGame(TGame* Game)
{
    int I;

    Game->SetNrOfMounts(Mounting->Count());
    for (I = 0; I <= 9; I++)
        if (Mounting->Count() > I) Game->SetMount(I, (*Mounting)[I]); else Game->SetMount(I, "");
    Game->SetAutoMountCDs(ui.AutoMountCheckBox->isChecked());
    Game->SetSecureMode(ui.SecureModeCheckBox->isChecked());
}

TModernProfileEditorDrivesFrame::~TModernProfileEditorDrivesFrame()
{
    if (Mounting != nullptr) delete Mounting;
    delete ImageList;
}
