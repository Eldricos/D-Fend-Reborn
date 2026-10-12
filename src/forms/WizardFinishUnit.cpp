// C++ port of WizardFinishUnit.pas (implementation section).
#include "WizardFinishUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "ProfileMountEditorFormUnit.h"
#include "GameDBToolsUnit.h"
#include "IconLoaderUnit.h"
#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

namespace {

// Delphi virtual key codes used by the OnKeyDown handlers
constexpr quint16 VK_RETURN = 0x0D;
constexpr quint16 VK_INSERT = 0x2D;
constexpr quint16 VK_DELETE = 0x2E;

quint16 QtKeyToVK(int Key)
{
    switch (Key) {
    case Qt::Key_Return: case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    default: return 0;
    }
}

void SetBold(QWidget *W)
{
    QFont F = W->font(); F.setBold(true); W->setFont(F);
}

// ShortName (not used by the unit itself, kept for completeness)
[[maybe_unused]] QString ShortName(const QString &LongName)
{
#ifdef Q_OS_WIN
    wchar_t Buf[MAX_PATH + 10];
    if (GetShortPathNameW(reinterpret_cast<const wchar_t *>(LongName.utf16()), Buf, MAX_PATH) == 0) return LongName;
    return QString::fromWCharArray(Buf);
#else
    return LongName;
#endif
}

}  // namespace

// TWizardFinishFrame

TWizardFinishFrame::TWizardFinishFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    connect(ui.MountingListView, &TListView::dblClick, this, [this]() { MountingListViewDblClick(ui.MountingListView); });
    connect(ui.MountingListView, &TListView::keyDown, this, [this](int Key, Qt::KeyboardModifiers Mods) {
        quint16 K = QtKeyToVK(Key);
        MountingListViewKeyDown(ui.MountingListView, K, Mods & ~Qt::KeypadModifier);
    });
    connect(ui.MountingAddButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.MountingAddButton); });
    connect(ui.MountingEditButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.MountingEditButton); });
    connect(ui.MountingDelButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.MountingDelButton); });
    connect(ui.MountingDeleteAllButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.MountingDeleteAllButton); });
    connect(ui.MountingAutoCreateButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.MountingAutoCreateButton); });
    connect(ui.PopupAdd, &QAction::triggered, this, [this]() { ButtonWork(ui.PopupAdd); });
    connect(ui.PopupEdit, &QAction::triggered, this, [this]() { ButtonWork(ui.PopupEdit); });
    connect(ui.PopupDelete, &QAction::triggered, this, [this]() { ButtonWork(ui.PopupDelete); });
}

TWizardFinishFrame::~TWizardFinishFrame()
{
    delete Mounting;
    delete MountingSave;
}

void TWizardFinishFrame::Init(TGameDB* AGameDB)
{
    // SetVistaFonts(self); - not needed

    SetBold(ui.InfoLabel);
    SetBold(ui.ProfileEditorLabel);
    ui.InfoLabel->setText(LanguageSetup->WizardFormPage5Info());
    ui.DriveSetupLabel->setText(LanguageSetup->WizardFormDriveSetupLabel());
    ui.MountingAddButton->setText(LanguageSetup->ProfileEditorMountingAdd());
    ui.MountingEditButton->setText(LanguageSetup->ProfileEditorMountingEdit());
    ui.MountingDelButton->setText(LanguageSetup->ProfileEditorMountingDel());
    ui.MountingDeleteAllButton->setText(LanguageSetup->ProfileEditorMountingDelAll());
    ui.MountingAutoCreateButton->setText(LanguageSetup->ProfileEditorMountingAutoCreate());
    InitMountingListView(ui.MountingListView);
    ui.ProfileEditorCheckBox->setText(LanguageSetup->WizardFormOpenProfileEditor());
    ui.ProfileEditorLabel->setText(LanguageSetup->WizardFormOpenProfileEditorInfo());
    UserIconLoader->DialogImage(DI_Add, ui.MountingAddButton);
    UserIconLoader->DialogImage(DI_Edit, ui.MountingEditButton);
    UserIconLoader->DialogImage(DI_Delete, ui.MountingDelButton);
    UserIconLoader->DialogImage(DI_Wizard, ui.MountingAutoCreateButton);

    ui.PopupAdd->setText(LanguageSetup->ProfileEditorMountingAdd());
    ui.PopupEdit->setText(LanguageSetup->ProfileEditorMountingEdit());
    ui.PopupDelete->setText(LanguageSetup->ProfileEditorMountingDel());
    // PopupEdit.ShortCut:=ShortCut(VK_Return,[]); (the key itself is handled by OnKeyDown)
    ui.PopupEdit->setShortcut(QKeySequence(Qt::Key_Return));
    ui.PopupEdit->setShortcutContext(Qt::WidgetShortcut);
    {
        TImageList *ImageList = TImageList::FromResource("WizardFinishUnit", "ImageList");
        if (ImageList == nullptr) ImageList = new TImageList();
        UserIconLoader->DialogImage(DI_Add, ImageList, 0);
        UserIconLoader->DialogImage(DI_Edit, ImageList, 1);
        UserIconLoader->DialogImage(DI_Delete, ImageList, 2);
        if (ImageList->Count() > 2) {
            ui.PopupAdd->setIcon(ImageList->Icon(0));
            ui.PopupEdit->setIcon(ImageList->Icon(1));
            ui.PopupDelete->setIcon(ImageList->Icon(2));
        }
        delete ImageList;
    }

    delete Mounting; Mounting = new TStringList;
    delete MountingSave; MountingSave = new TStringList;

    GameDB = AGameDB;
}

void TWizardFinishFrame::Done()
{
    delete Mounting; Mounting = nullptr;
    delete MountingSave; MountingSave = nullptr;
}

void TWizardFinishFrame::SetInsecureStatus(bool Insecure)
{
    if (Insecure) {
        ui.ProfileEditorCheckBox->setChecked(true);
        ui.ProfileEditorCheckBox->setEnabled(false);
        ui.ProfileEditorLabel->setVisible(true);
    } else {
        ui.ProfileEditorCheckBox->setEnabled(true);
        ui.ProfileEditorLabel->setVisible(false);
    }
}

void TWizardFinishFrame::LoadData(TGame* Template, const QString& GameFile, const QString& SetupFile, const QString& GameName)
{
    QString S, T;

    CurrentGameName = GameName;
    CurrentGameFile = GameFile;

    delete Mounting;
    if (!Trim(GameFile).isEmpty()) S = ExtractFilePath(GameFile); else S.clear();
    if (ExtUpperCase(Copy(Trim(S), 1, 7)) == "DOSBOX:") S.clear();
    if (!Trim(SetupFile).isEmpty()) T = ExtractFilePath(SetupFile); else T.clear();
    if (ExtUpperCase(Copy(Trim(T), 1, 7)) == "DOSBOX:") T.clear();
    Mounting = BuildGameDirMountData(Template, S, T);

    MountingSave->Assign(*Mounting);
    LoadMountingList();
}

void TWizardFinishFrame::LoadMountingList()
{
    LoadMountingListView(ui.MountingListView, Mounting);
}

QChar TWizardFinishFrame::NextFreeDriveLetter()
{
    QChar result = 'C';
    bool B;
    do {
        B = true;
        for (int I = 0; I < Mounting->Count(); I++) {
            TStringList* St = ValueToList((*Mounting)[I]);
            if (St->Count() < 3) { delete St; continue; }
            if (UpperCase((*St)[2]) == QString(result)) { result = QChar(result.unicode() + 1); B = false; delete St; break; }
            delete St;
        }
    } while (!B);
    return result;
}

QString TWizardFinishFrame::UsedDriveLetters(int AllowedNr)
{
    QString result;
    for (int I = 0; I < Mounting->Count(); I++) if (I != AllowedNr) {
        TStringList* St = ValueToList((*Mounting)[I]);
        if (St->Count() >= 3) result = result + UpperCase((*St)[2]);
        delete St;
    }
    return result;
}

void TWizardFinishFrame::ButtonWork(QObject* Sender)
{
    QString S;
    int I;

    switch (Sender->property("tag").toInt()) {
    case 0:
        if (Mounting->Count() < 10) {
            S.clear();
            if (!ShowProfileMountEditorDialog(this, S, UsedDriveLetters(), IncludeTrailingPathDelimiter(ExtractFilePath(CurrentGameFile)), CurrentGameName, GameDB, nullptr, QString(NextFreeDriveLetter()))) return;
            Mounting->Add(S);
            LoadMountingList();
            ui.MountingListView->SetItemIndex(ui.MountingListView->Items()->Count() - 1);
        }
        break;
    case 1:
        I = ui.MountingListView->ItemIndex();
        if (I < 0) return;
        S = (*Mounting)[I];
        if (!ShowProfileMountEditorDialog(this, S, UsedDriveLetters(I), IncludeTrailingPathDelimiter(ExtractFilePath(CurrentGameFile)), CurrentGameName, GameDB)) return;
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
        if ((Mounting->Count() > 0) && (MessageDlg(LanguageSetup->ProfileEditorMountingDeleteAllMessage(), mtConfirmation, mbYes | mbNo, 0) == mrYes)) {
            Mounting->Clear();
            LoadMountingList();
        }
        break;
    case 4:
        Mounting->Assign(*MountingSave);
        LoadMountingList();
        break;
    }
}

void TWizardFinishFrame::MountingListViewDblClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ButtonWork(ui.MountingEditButton);
}

void TWizardFinishFrame::MountingListViewKeyDown(QObject* Sender, quint16& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if (Shift != Qt::NoModifier) return;
    switch (Key) {
    case VK_INSERT: ButtonWork(ui.MountingAddButton); break;
    case VK_RETURN: ButtonWork(ui.MountingEditButton); break;
    case VK_DELETE: ButtonWork(ui.MountingDelButton); break;
    }
}

void TWizardFinishFrame::WriteDataToGame(TGame* Game)
{
    Game->SetNrOfMounts(Mounting->Count());
    for (int I = 0; I <= 9; I++) {
        const QString V = (Mounting->Count() > I) ? (*Mounting)[I] : QString();
        // Game.Mount[I]:=... (TGame::SetMount is private in GameDBUnit.h, use the public MountN setters)
        switch (I) {
        case 0: Game->SetMount0(V); break;
        case 1: Game->SetMount1(V); break;
        case 2: Game->SetMount2(V); break;
        case 3: Game->SetMount3(V); break;
        case 4: Game->SetMount4(V); break;
        case 5: Game->SetMount5(V); break;
        case 6: Game->SetMount6(V); break;
        case 7: Game->SetMount7(V); break;
        case 8: Game->SetMount8(V); break;
        case 9: Game->SetMount9(V); break;
        }
    }
    Game->SetAutoMountCDs(false);
}
