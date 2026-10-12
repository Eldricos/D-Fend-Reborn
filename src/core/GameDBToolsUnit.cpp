// C++ port of GameDBToolsUnit.pas (implementation section).
#include "GameDBToolsUnit.h"

#include <QThread>
#include <QPainter>
#include <QTreeWidgetItemIterator>
#include <algorithm>

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "ProfileEditorFormUnit.h"
#include "ModernProfileEditorFormUnit.h"
#include "HashCalc.h"
#include "SmallWaitFormUnit.h"
#include "ChecksumFormUnit.h"
#include "WaitFormUnit.h"
#include "ImageCacheUnit.h"
#include "DosBoxUnit.h"
#include "ScummVMUnit.h"
#include "ImageStretch.h"
#include "MainUnit.h"
#include "GameDBFilterUnit.h"
#include "HistoryUnit.h"
#include "IconLoaderUnit.h"
#include "ScreenshotsCacheUnit.h"
#include "LoggingUnit.h"
#include "VclLayout.h"

bool GameDBToolsUnitShowDialogs = false;
bool FreeDOSInitThreadRunning = false;
bool TemplatesInitThreadRunning = false;

static bool GroupMatch(const QString &GameGroupUpper, const QString &SelectedGroupUpper);

// ---------------------------------------------------------------------------
// Local helpers of the port
// ---------------------------------------------------------------------------

// Application.MainForm
static QWidget *MainFormWidget()
{
    return DFendReloadedMainForm;
}

// TGame.Mount[I]:=Value (the C++ TGame header only exposes the read accessor)
static void SetGameMount(TGame *G, int I, const QString &Value)
{
    switch (I) {
        case 0: G->SetMount0(Value); break;
        case 1: G->SetMount1(Value); break;
        case 2: G->SetMount2(Value); break;
        case 3: G->SetMount3(Value); break;
        case 4: G->SetMount4(Value); break;
        case 5: G->SetMount5(Value); break;
        case 6: G->SetMount6(Value); break;
        case 7: G->SetMount7(Value); break;
        case 8: G->SetMount8(Value); break;
        case 9: G->SetMount9(Value); break;
        default: break;
    }
}

// Case O[I+1] of '1'..'9' : ...; 'A'..'Z' : ...; 'a'..'z' : ... end;
static int ColumnCharToNr(QChar C)
{
    int Nr = -1;
    if (C >= QChar('1') && C <= QChar('9')) Nr = C.unicode() - '0';
    else if (C >= QChar('A') && C <= QChar('Z')) Nr = C.unicode() - 'A' + 10;
    else if (C >= QChar('a') && C <= QChar('z')) Nr = C.unicode() - 'a' + 10;
    return Nr;
}

// Node.ImageIndex:=Index; Node.SelectedIndex:=Index;
// TTreeView.Images is not available on QTreeWidget: the image list is taken
// from the dynamic property "vclImageList" (void* to a TImageList, set by the
// owner form) or, as fall back, from the design time image list named in
// "vclImages" (exported by dfm2ui from MainUnit).
static void SetNodeImage(QTreeWidgetItem *N, int Index)
{
    N->setData(0, Qt::UserRole + 1, Index);
    QTreeWidget *T = N->treeWidget();
    if (T == nullptr) return;
    const QVariant V = T->property("vclImageList");
    if (V.isValid()) {
        TImageList *L = static_cast<TImageList *>(V.value<void *>());
        if (L != nullptr && Index >= 0 && Index < L->Count()) N->setIcon(0, L->Icon(Index));
        return;
    }
    const QString Name = T->property("vclImages").toString();
    if (!Name.isEmpty()) N->setIcon(0, VclLayout::imageListIcon(QStringLiteral("MainUnit"), Name, Index));
}

static QImage IconToImage(const QIcon &Icon)
{
    if (Icon.isNull()) return QImage();
    QList<QSize> Sizes = Icon.availableSizes();
    QSize Best(0, 0);
    for (const QSize &S : Sizes) if (S.width() * S.height() > Best.width() * Best.height()) Best = S;
    if (Best.isEmpty()) Best = Icon.actualSize(QSize(256, 256));
    if (Best.isEmpty()) Best = QSize(32, 32);
    return Icon.pixmap(Best).toImage();
}

static int IconWidth(const QIcon &Icon)
{
    int W = 0;
    for (const QSize &S : Icon.availableSizes()) W = qMax(W, S.width());
    if (W == 0) W = Icon.actualSize(QSize(256, 256)).width();
    return W;
}

// ---------------------------------------------------------------------------

static void AddTypeSelector(QTreeWidget *ATreeView, const QString &Name, TStringList *St)
{
    QTreeWidgetItem *N = new QTreeWidgetItem(ATreeView, QStringList{Name});
    SetNodeImage(N, 12);
    if (St != nullptr) for (int I = 0; I < St->Count(); I++) {
        QString S = (*St)[I];
        if (S.isEmpty()) S = LanguageSetup->NotSet();
        QTreeWidgetItem *N2 = new QTreeWidgetItem(N, QStringList{S});
        SetNodeImage(N2, 10);
    }
    delete St;
}

static TStringList *RemoveDoubleEntrys(TStringList *St)   // return value = St
{
    TStringList *result = St;

    TStringList Upper;
    int I = 0;
    while (I < St->Count()) {
        if (Trim((*St)[I]).isEmpty()) { St->Delete(I); continue; }
        QString S = ExtUpperCase((*St)[I]);
        if (Upper.IndexOf(S) >= 0) { St->Delete(I); continue; }
        Upper.Add(S); I++;
    }

    return result;
}

void InitTreeViewForGamesList(QTreeWidget *ATreeView, TGameDB *GameDB)
{
    QString Group, SubGroup, TreeFilter;

    const bool OldBlock = ATreeView->blockSignals(true);   // OnChange:=nil
    ATreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);   // ReadOnly:=True

    QTreeWidgetItem *Sel = ATreeView->currentItem();
    if (Sel == nullptr) { Group = ""; SubGroup = ""; } else {
        if (Sel->parent() == nullptr) {
            Group = Sel->text(0); SubGroup = "";
        } else {
            Group = Sel->parent()->text(0);
            SubGroup = Sel->text(0);
        }
    }

    ATreeView->setUpdatesEnabled(false);
    [&]() {
        ATreeView->clear();

        QTreeWidgetItem *N, *N2;

        // All games
        N = new QTreeWidgetItem(ATreeView, QStringList{RemoveUnderline(LanguageSetup->All())});
        SetNodeImage(N, 8);

        // Favorites
        N = new QTreeWidgetItem(ATreeView, QStringList{LanguageSetup->GameFavorites()});
        SetNodeImage(N, 9);

        // Genre, Developer, Publisher, Year, Language, License
        TreeFilter = PrgSetup->DefaultTreeFilter();
        while (Length(TreeFilter) < 8) TreeFilter = TreeFilter + "1";

        if (TreeFilter[0] != QChar('0')) AddTypeSelector(ATreeView, LanguageSetup->GameGenre(), GetCustomGenreName(GameDB->GetGenreList()));
        if (TreeFilter[1] != QChar('0')) AddTypeSelector(ATreeView, LanguageSetup->GameDeveloper(), GameDB->GetDeveloperList());
        if (TreeFilter[2] != QChar('0')) AddTypeSelector(ATreeView, LanguageSetup->GamePublisher(), GameDB->GetPublisherList());
        if (TreeFilter[3] != QChar('0')) AddTypeSelector(ATreeView, LanguageSetup->GameYear(), GameDB->GetYearList());
        if (TreeFilter[4] != QChar('0')) AddTypeSelector(ATreeView, LanguageSetup->GameLanguage(), GetCustomLanguageName(GameDB->GetLanguageList()));
        if (TreeFilter[5] != QChar('0')) AddTypeSelector(ATreeView, LanguageSetup->GameLicense(), GetCustomLicenseName(GameDB->GetLicenseList()));

        // Emulation type
        if (TreeFilter[6] != QChar('0')) {
            N = new QTreeWidgetItem(ATreeView, QStringList{LanguageSetup->GameEmulationType()});
            SetNodeImage(N, 12);
            N2 = new QTreeWidgetItem(N, QStringList{LanguageSetup->GameEmulationTypeDOSBox()}); SetNodeImage(N2, 16);
            N2 = new QTreeWidgetItem(N, QStringList{LanguageSetup->GameEmulationTypeScummVM()}); SetNodeImage(N2, 37);
            N2 = new QTreeWidgetItem(N, QStringList{LanguageSetup->GameEmulationTypeWindows()}); SetNodeImage(N2, 40);
            for (int I = 0; I < PrgSetup->WindowsBasedEmulatorsNames()->Count(); I++) {
                N2 = new QTreeWidgetItem(N, QStringList{(*PrgSetup->WindowsBasedEmulatorsNames())[I]}); SetNodeImage(N2, MI_Count + I);
            }
        }

        // Recently played
        if (TreeFilter[7] != QChar('0')) AddTypeSelector(ATreeView, LanguageSetup->GameRecentlyPlayed(), nullptr);

        // User categories
        TStringList *UserGroups = RemoveDoubleEntrys(StringToStringList(PrgSetup->UserGroups()));
        for (int I = 0; I < UserGroups->Count(); I++)
            AddTypeSelector(ATreeView, (*UserGroups)[I], GameDB->GetKeyValueList((*UserGroups)[I]));
        delete UserGroups;

        // Select a group
        if (Group.isEmpty()) return;

        for (QTreeWidgetItemIterator It(ATreeView); *It; ++It) {
            QTreeWidgetItem *Item = *It;
            if (SubGroup.isEmpty()) {
                if (Item->parent() == nullptr && Item->text(0) == Group) { ATreeView->setCurrentItem(Item); return; }
            } else {
                if (Item->parent() != nullptr && Item->text(0) == SubGroup && Item->parent()->text(0) == Group) { ATreeView->setCurrentItem(Item); return; }
            }
        }
    }();
    ATreeView->setUpdatesEnabled(true);

    ATreeView->blockSignals(OldBlock);   // OnChange:=OnChange
}

void GetColOrderAndVisible(QString &O, QString &V, QString &VUser)
{
    int I, UserCount;
    QString S;

    V = PrgSetup->ColVisible();

    I = Pos(";", V);
    if (I > 0) { VUser = Trim(Copy(V, I + 1, MaxInt)); V = Trim(Copy(V, 1, I - 1)); } else VUser = "";
    while (Length(V) < 8) V = V + "1";
    if (Length(V) > 8) V = Copy(V, 1, 8);
    if (!VUser.isEmpty()) PrgSetup->SetColVisible(V + ";" + VUser); else PrgSetup->SetColVisible(V);

    O = PrgSetup->ColOrder();
    UserCount = 0; S = VUser;
    while (!S.isEmpty()) {
        UserCount++;
        I = Pos(";", S); if (I == 0) break;
        S = Trim(Copy(S, I + 1, MaxInt));
    }
    while (Length(O) < 8 + UserCount) {
        if (Length(O) < 9)
            O = O + QChar(Length(O) + '1');
        else
            O = O + QChar(Length(O) - 9 + 'A');
    }
    if (Length(O) > 8 + UserCount) O = Copy(O, 1, 8 + UserCount);
    PrgSetup->SetColOrder(O);
}

QString GetUserCols()
{
    QString O, V, result;
    GetColOrderAndVisible(O, V, result);
    return result;
}

void SetSortTypeByListViewCol(int ColumnIndex, TSortListBy &ListSort, bool &ListSortReverse)
{
    auto SetListSort = [&](TSortListBy L) {
        if (ListSort == L) ListSortReverse = !ListSortReverse; else { ListSort = L; ListSortReverse = false; }
    };

    QString O, V, VUser;
    int I, Nr, C;

    GetColOrderAndVisible(O, V, VUser);
    TStringList *VUserSt = ValueToList(VUser);
    if (ColumnIndex == 0) SetListSort(slbName); else {
        if (!PrgSetup->ShowExtraInfo()) SetListSort(slbSetup); else {
            C = 0;
            for (I = 0; I < Length(O); I++) {
                Nr = ColumnCharToNr(O[I]);
                if (Nr < 1) continue;
                if (Nr <= 8) {
                    if (V[Nr - 1] == QChar('0')) continue;
                } else {
                    if (Nr - 8 > VUserSt->Count()) continue;
                }
                C++;
                if (C == ColumnIndex) {
                    switch (Nr - 1) {
                        case 0: SetListSort(slbSetup); break;
                        case 1: SetListSort(slbGenre); break;
                        case 2: SetListSort(slbDeveloper); break;
                        case 3: SetListSort(slbPublisher); break;
                        case 4: SetListSort(slbYear); break;
                        case 5: SetListSort(slbLanguage); break;
                        case 6: SetListSort(slbComment); break;
                        case 7: SetListSort(slbStartCount); break;
                        default: SetListSort(static_cast<TSortListBy>(Nr - 9 + 10)); break;
                    }
                    break;
                }
            }
        }
    }
    delete VUserSt;
}

void InitMountingListView(TListView *MountingListView)
{
    TListColumn *L;
    L = MountingListView->Columns()->Add(); L->SetWidth(-2); L->SetCaption(LanguageSetup->ProfileEditorMountingFolderImage());
    L = MountingListView->Columns()->Add(); L->SetWidth(-2); L->SetCaption(LanguageSetup->ProfileEditorMountingAs());
    L = MountingListView->Columns()->Add(); L->SetWidth(-2); L->SetCaption(LanguageSetup->ProfileEditorMountingLetter());
    L = MountingListView->Columns()->Add(); L->SetWidth(-2); L->SetCaption(LanguageSetup->ProfileEditorMountingLabel());
    L = MountingListView->Columns()->Add(); L->SetWidth(-2); L->SetCaption(LanguageSetup->ProfileEditorMountingIOControl());
}

void LoadMountingListView(TListView *MountingListView, TStringList *Mounting)
{
    QString S, T, U;
    bool B;

    MountingListView->Items()->BeginUpdate();
    MountingListView->Items()->Clear();
    for (int I = 0; I < Mounting->Count(); I++) {
        TStringList *St = ValueToList((*Mounting)[I]);
        if (St->Count() > 0) {
            TListItem *L = MountingListView->Items()->Add();

            S = Trim((*St)[0]);
            if (St->Count() > 1) {
                T = Trim(ExtUpperCase((*St)[1]));
                if (Pos("$", S) != 0) {
                    if (T == "PHYSFS" || T == "ZIP")
                        S = Copy(S, Pos("$", S) + 1, MaxInt) + " (+ " + Copy(S, 1, Pos("$", S) - 1) + ")";
                    else
                        S = Copy(S, 1, Pos("$", S) - 1) + " (+" + LanguageSetup->More() + ")";
                }
                if (T == "CDROM" && S == "ASK") S = LanguageSetup->ProfileMountingCDDriveTypeAskShort();
                if (T == "CDROM" && Pos(":", S) != 0) {
                    U = Trim(Copy(S, 1, Pos(":", S) - 1));
                    if (U == "NUMBER") S = Format(LanguageSetup->ProfileMountingCDDriveTypeNumberShort(), {Copy(S, Pos(":", S) + 1, MaxInt)});
                    if (U == "LABEL") S = Format(LanguageSetup->ProfileMountingCDDriveTypeLabelShort(), {Copy(S, Pos(":", S) + 1, MaxInt)});
                    if (U == "FILE") S = Format(LanguageSetup->ProfileMountingCDDriveTypeFileShort(), {Copy(S, Pos(":", S) + 1, MaxInt)});
                    if (U == "FOLDER") S = Format(LanguageSetup->ProfileMountingCDDriveTypeFolderShort(), {Copy(S, Pos(":", S) + 1, MaxInt)});
                }
            }
            L->SetCaption(S);

            if (St->Count() > 1) {
                S = Trim(ExtUpperCase((*St)[1])); B = false;
                if ((!B) && (S == "DRIVE")) { L->SubItems().Add(LanguageSetup->ProfileEditorMountingDriveTypeDRIVE()); B = true; }
                if ((!B) && (S == "CDROM")) { L->SubItems().Add(LanguageSetup->ProfileEditorMountingDriveTypeCDROM()); B = true; }
                if ((!B) && (S == "CDROMIMAGE")) { L->SubItems().Add(LanguageSetup->ProfileEditorMountingDriveTypeCDROMIMAGE()); B = true; }
                if ((!B) && (S == "FLOPPY")) { L->SubItems().Add(LanguageSetup->ProfileEditorMountingDriveTypeFLOPPY()); B = true; }
                if ((!B) && (S == "FLOPPYIMAGE")) { L->SubItems().Add(LanguageSetup->ProfileEditorMountingDriveTypeFLOPPYIMAGE()); B = true; }
                if ((!B) && (S == "IMAGE")) { L->SubItems().Add(LanguageSetup->ProfileEditorMountingDriveTypeIMAGE()); B = true; }
                if ((!B) && (S == "PHYSFS")) { L->SubItems().Add(LanguageSetup->ProfileEditorMountingDriveTypePHYSFS()); B = true; }
                if ((!B) && (S == "ZIP")) { L->SubItems().Add(LanguageSetup->ProfileEditorMountingDriveTypeZIP()); B = true; }
                if (!B) L->SubItems().Add((*St)[1]);
            } else {
                L->SubItems().Add("");
            }

            if (St->Count() > 2) L->SubItems().Add((*St)[2]); else L->SubItems().Add("");
            if (St->Count() > 4) L->SubItems().Add((*St)[4]); else L->SubItems().Add("");
            if (St->Count() > 3) {
                if (Trim(ExtUpperCase((*St)[3])) == "TRUE") L->SubItems().Add(RemoveUnderline(LanguageSetup->Yes())); else L->SubItems().Add(RemoveUnderline(LanguageSetup->No()));
            } else L->SubItems().Add(RemoveUnderline(LanguageSetup->No()));
        }
        delete St;
    }
    if (MountingListView->Items()->Count() > 0) MountingListView->SetItemIndex(0);
    MountingListView->Items()->EndUpdate();
}

// TMenuItem with sub items -> QMenu (Tag stored in property "tag"),
// TMenuItem without sub items -> QAction (parent = the QMenu it is shown in).
static void BuildSelectPopupSubMenu(QMenu *Popup, const QString &Name, QMenu *MenuSelect, QMenu *MenuUnselect, int Tag, const TNotifyEvent &OnClick, TStringList *Items)
{
    Q_UNUSED(Popup);
    QMenu *SubSelect = MenuSelect->addMenu(MaskUnderlineAmpersand(Name));
    SubSelect->setProperty("tag", Tag);

    QMenu *SubUnselect = MenuUnselect->addMenu(MaskUnderlineAmpersand(Name));
    SubUnselect->setProperty("tag", Tag);

    for (int I = 0; I < Items->Count(); I++) {
        QAction *M = SubSelect->addAction(MaskUnderlineAmpersand((*Items)[I]));
        M->setProperty("tag", I);
        QObject::connect(M, &QAction::triggered, M, [M, OnClick]() { if (OnClick) OnClick(M); });
        M = SubUnselect->addAction(MaskUnderlineAmpersand((*Items)[I]));
        M->setProperty("tag", I);
        QObject::connect(M, &QAction::triggered, M, [M, OnClick]() { if (OnClick) OnClick(M); });
    }
}

// TCheckListBox -> QListWidget: every item is checkable, the TGame pointer
// (Items.Objects[I]) is stored in data(Qt::UserRole) as void*.
void BuildCheckList(QListWidget *CheckListBox, TGameDB *GameDB, bool WithDefaultProfile, bool HideScummVMAndWindowsProfiles, bool HideWindowsProfiles)
{
    TStringList St;
    for (int I = 0; I < GameDB->Count(); I++) if (WithDefaultProfile || (GameDB->Game(I)->Name() != DosBoxDOSProfile)) {
        if (HideScummVMAndWindowsProfiles && (ScummVMMode(GameDB->Game(I)) || WindowsExeMode(GameDB->Game(I)))) continue;
        if (HideWindowsProfiles && WindowsExeMode(GameDB->Game(I))) continue;
        St.AddObject(GameDB->Game(I)->Name(), GameDB->Game(I));
    }
    St.Sort();
    CheckListBox->clear();
    for (int I = 0; I < St.Count(); I++) {
        QListWidgetItem *Item = new QListWidgetItem(St[I], CheckListBox);
        Item->setData(Qt::UserRole, QVariant::fromValue<void *>(St.Objects(I)));
        Item->setFlags(Item->flags() | Qt::ItemIsUserCheckable);
        Item->setCheckState(Qt::Unchecked);
    }
    for (int I = 0; I < CheckListBox->count(); I++) CheckListBox->item(I)->setCheckState(Qt::Checked);
}

struct TUserDataRecord {
    QString Name, NameUpper;
    TStringList *Values = nullptr;
    TStringList *ValuesUpper = nullptr;
};

void BuildSelectPopupMenu(QMenu *Popup, TGameDB *GameDB, TNotifyEvent OnClick, bool WithDefaultProfile, bool HideWindowsProfiles)
{
    TStringList *St;
    int I, J, K, Nr;
    QString S, T, U, SUpper;
    QList<TUserDataRecord> UserData;

    // Base settings

    QMenu *MenuSelect = Popup->addMenu(LanguageSetup->Select());
    MenuSelect->setProperty("tag", 1);
    QMenu *MenuUnselect = Popup->addMenu(LanguageSetup->Unselect());
    MenuUnselect->setProperty("tag", 0);

    // Add normal meta data submenus

    St = GetCustomGenreName(GameDB->GetGenreList(WithDefaultProfile, HideWindowsProfiles));
    BuildSelectPopupSubMenu(Popup, LanguageSetup->GameGenre(), MenuSelect, MenuUnselect, 0, OnClick, St); delete St;

    St = GameDB->GetDeveloperList(WithDefaultProfile, HideWindowsProfiles);
    BuildSelectPopupSubMenu(Popup, LanguageSetup->GameDeveloper(), MenuSelect, MenuUnselect, 1, OnClick, St); delete St;

    St = GameDB->GetPublisherList(WithDefaultProfile, HideWindowsProfiles);
    BuildSelectPopupSubMenu(Popup, LanguageSetup->GamePublisher(), MenuSelect, MenuUnselect, 2, OnClick, St); delete St;

    St = GameDB->GetYearList(WithDefaultProfile, HideWindowsProfiles);
    BuildSelectPopupSubMenu(Popup, LanguageSetup->GameYear(), MenuSelect, MenuUnselect, 3, OnClick, St); delete St;

    St = GetCustomLanguageName(GameDB->GetLanguageList(WithDefaultProfile, HideWindowsProfiles));
    BuildSelectPopupSubMenu(Popup, LanguageSetup->GameLanguage(), MenuSelect, MenuUnselect, 4, OnClick, St); delete St;

    St = GetCustomLicenseName(GameDB->GetLicenseList(WithDefaultProfile, HideWindowsProfiles));
    BuildSelectPopupSubMenu(Popup, LanguageSetup->GameLicense(), MenuSelect, MenuUnselect, 5, OnClick, St); delete St;

    St = new TStringList;
    St->Add(LanguageSetup->GameEmulationTypeDOSBox());
    St->Add(LanguageSetup->GameEmulationTypeScummVM());
    if (!HideWindowsProfiles) St->Add(LanguageSetup->GameEmulationTypeWindows());
    BuildSelectPopupSubMenu(Popup, LanguageSetup->GameEmulationType(), MenuSelect, MenuUnselect, 6, OnClick, St);
    delete St;

    // Collect user data

    for (I = 0; I < GameDB->Count(); I++) if (WithDefaultProfile || (GameDB->Game(I)->Name() != DosBoxDOSProfile)) {
        St = StringToStringList(GameDB->Game(I)->UserInfo());
        for (J = 0; J < St->Count(); J++) {
            S = Trim((*St)[J]);
            if (S.isEmpty() || Pos("=", S) == 0) continue;
            T = Trim(Copy(S, Pos("=", S) + 1, MaxInt));
            S = Trim(Copy(S, 1, Pos("=", S) - 1));
            if (S.isEmpty() || T.isEmpty()) continue;

            SUpper = ExtUpperCase(S);
            if (SUpper == "LICENSE") continue;
            Nr = -1;
            for (K = 0; K < UserData.size(); K++) if (UserData[K].NameUpper == SUpper) { Nr = K; break; }
            if (Nr < 0) {
                Nr = UserData.size(); UserData.append(TUserDataRecord());
                UserData[Nr].Name = S; UserData[Nr].NameUpper = SUpper;
                UserData[Nr].Values = new TStringList;
                UserData[Nr].ValuesUpper = new TStringList;
            }
            K = Pos(";", T);
            while (!T.isEmpty()) {
                if (K > 0) {
                    U = Trim(Copy(T, 1, K - 1));
                    T = Trim(Copy(T, K + 1, MaxInt));
                } else {
                    U = T; T = "";
                }
                if (UserData[Nr].ValuesUpper->IndexOf(ExtUpperCase(U)) < 0) {
                    UserData[Nr].Values->Add(U);
                    UserData[Nr].ValuesUpper->Add(ExtUpperCase(U));
                }
                K = Pos(";", T);
            }
        }
        delete St;
    }

    // Build menu from user data

    for (I = 0; I < UserData.size(); I++) {
        BuildSelectPopupSubMenu(Popup, UserData[I].Name, MenuSelect, MenuUnselect, -1, OnClick, UserData[I].Values);
    }

    // Free records

    for (I = 0; I < UserData.size(); I++) {
        delete UserData[I].Values;
        delete UserData[I].ValuesUpper;
    }
}

static bool UserDataContainValue(const QString &UserInfo, const QString &Key, const QString &Value)
{
    TStringList *St = StringToStringList(UserInfo);
    for (int I = 0; I < St->Count(); I++) {
        QString S = Trim(ExtUpperCase((*St)[I]));
        int J = Pos("=", S);
        if (J == 0) continue;
        QString T = Trim(Copy(S, J + 1, MaxInt));
        S = Trim(Copy(S, 1, J - 1));
        if (S == Key && GroupMatch(T, Value)) { delete St; return true; }
    }
    delete St;
    return false;
}

void SelectGamesByPopupMenu(QObject *Sender, QListWidget *CheckListBox)
{
    bool Select;
    int Category;
    QString CategoryValue;
    QString S, CategoryName;

    QAction *M = qobject_cast<QAction *>(Sender);
    if (M == nullptr) return;
    QMenu *MParent = qobject_cast<QMenu *>(M->parent());
    if (MParent == nullptr) return;
    QObject *MParentParent = MParent->parent();

    CategoryValue = RemoveUnderline(Trim(ExtUpperCase(M->text())));
    if (CategoryValue == Trim(ExtUpperCase(LanguageSetup->NotSet()))) CategoryValue = "";
    Category = MParent->property("tag").toInt();
    Select = (MParentParent != nullptr && MParentParent->property("tag").toInt() == 1);
    if (Category == -1) CategoryName = RemoveUnderline(ExtUpperCase(MParent->title())); else CategoryName = "";

    for (int I = 0; I < CheckListBox->count(); I++) {
        TGame *G = static_cast<TGame *>(CheckListBox->item(I)->data(Qt::UserRole).value<void *>());
        if (G == nullptr) continue;
        if (!CategoryName.isEmpty()) {
            if (!UserDataContainValue(G->UserInfo(), CategoryName, CategoryValue)) continue;
        } else {
            switch (Category) {
                case 0: S = GetCustomGenreName(G->CacheGenre); break;
                case 1: S = G->CacheDeveloper; break;
                case 2: S = G->CachePublisher; break;
                case 3: S = G->CacheYear; break;
                case 4: S = GetCustomLanguageName(G->CacheLanguage); break;
                case 5: S = GetCustomLicenseName(G->License()); break;
                case 6:
                    S = "DOSBox";
                    if (ScummVMMode(G)) S = "ScummVM";
                    if (WindowsExeMode(G)) S = "Windows";
                    break;
                default: break;
            }
            S = Trim(S);
            if (!GroupMatch(ExtUpperCase(S), CategoryValue)) continue;
        }
        CheckListBox->item(I)->setCheckState(Select ? Qt::Checked : Qt::Unchecked);
    }
}

void SelectGamesByPopupMenu(QObject *Sender, TListView *ListView)
{
    bool Select;
    int Category;
    QString CategoryValue;
    QString S, CategoryName;

    QAction *M = qobject_cast<QAction *>(Sender);
    if (M == nullptr) return;
    QMenu *MParent = qobject_cast<QMenu *>(M->parent());
    if (MParent == nullptr) return;
    QObject *MParentParent = MParent->parent();

    CategoryValue = RemoveUnderline(Trim(ExtUpperCase(M->text())));
    if (CategoryValue == Trim(ExtUpperCase(LanguageSetup->NotSet()))) CategoryValue = "";
    Category = MParent->property("tag").toInt();
    Select = (MParentParent != nullptr && MParentParent->property("tag").toInt() == 1);
    if (Category == -1) CategoryName = RemoveUnderline(ExtUpperCase(MParent->title())); else CategoryName = "";

    for (int I = 0; I < ListView->Items()->Count(); I++) {
        TGame *G = ListView->Items()->Item(I)->DataAs<TGame>();
        if (G == nullptr) continue;
        if (!CategoryName.isEmpty()) {
            if (!UserDataContainValue(G->UserInfo(), CategoryName, CategoryValue)) continue;
        } else {
            switch (Category) {
                case 0: S = GetCustomGenreName(G->CacheGenre); break;
                case 1: S = G->CacheDeveloper; break;
                case 2: S = G->CachePublisher; break;
                case 3: S = G->CacheYear; break;
                case 4: S = GetCustomLanguageName(G->CacheLanguage); break;
                case 5:
                    S = "DOSBox";
                    if (ScummVMMode(G)) S = "ScummVM";
                    if (WindowsExeMode(G)) S = "Windows";
                    break;
                default: break;
            }
            S = Trim(S);
            if (ExtUpperCase(S) != CategoryValue) continue;
        }
        ListView->Items()->Item(I)->SetChecked(Select);
    }
}

TStringList *GetUserDataList(TGameDB *GameDB)
{
    QList<TUserDataRecord> UserData;
    QString S, T, SUpper;

    for (int I = 0; I < GameDB->Count(); I++) {
        TStringList *St = StringToStringList(GameDB->Game(I)->UserInfo());
        for (int J = 0; J < St->Count(); J++) {
            S = Trim((*St)[J]);
            if (S.isEmpty() || Pos("=", S) == 0) continue;
            T = Trim(Copy(S, Pos("=", S) + 1, MaxInt));
            S = Trim(Copy(S, 1, Pos("=", S) - 1));
            if (S.isEmpty() || T.isEmpty()) continue;
            SUpper = ExtUpperCase(S);
            int Nr = -1;
            for (int K = 0; K < UserData.size(); K++) if (UserData[K].NameUpper == SUpper) { Nr = K; break; }
            if (Nr < 0) {
                Nr = UserData.size(); UserData.append(TUserDataRecord());
                UserData[Nr].Name = S; UserData[Nr].NameUpper = SUpper;
            }
        }
        delete St;
    }
    TStringList *result = new TStringList;
    for (int I = 0; I < UserData.size(); I++) result->Add(UserData[I].Name);
    return result;
}

void InitListViewForGamesList(TListView *AListView, bool ShowExtraInfo)
{
    TListColumn *C;
    int Nr;
    QString V, O, VUser;

    AListView->SetReadOnly(true);

    GetColOrderAndVisible(O, V, VUser);
    TStringList *VUserSt = ValueToList(VUser);
    AListView->Columns()->BeginUpdate();
    AListView->Items()->BeginUpdate();
    [&]() {
        AListView->Items()->Clear();
        AListView->Columns()->Clear();

        C = AListView->Columns()->Add();
        C->SetCaption(LanguageSetup->GameName());
        C->SetWidth(-1);

        if (!ShowExtraInfo) {
            C = AListView->Columns()->Add();
            C->SetCaption(LanguageSetup->GameSetup());
            C->SetWidth(-2);
            return;
        }

        for (int I = 0; I < Length(O); I++) {
            Nr = ColumnCharToNr(O[I]);
            if (Nr < 1) continue;
            if (Nr <= 8) {
                if (V[Nr - 1] == QChar('0')) continue;
            } else {
                if (Nr - 8 > VUserSt->Count()) continue;
            }

            C = AListView->Columns()->Add();
            switch (Nr - 1) {
                case 0: C->SetCaption(GetCustomGenreName(LanguageSetup->GameSetup())); break;
                case 1: C->SetCaption(LanguageSetup->GameGenre()); break;
                case 2: C->SetCaption(LanguageSetup->GameDeveloper()); break;
                case 3: C->SetCaption(LanguageSetup->GamePublisher()); break;
                case 4: C->SetCaption(LanguageSetup->GameYear()); break;
                case 5: C->SetCaption(GetCustomLanguageName(LanguageSetup->GameLanguage())); break;
                case 6: C->SetCaption(LanguageSetup->GameNotes()); break;
                case 7: C->SetCaption(LanguageSetup->GameStartCount()); break;
                default: C->SetCaption((*VUserSt)[Nr - 9]); break;
            }
            if (Nr - 1 == 0) C->SetWidth(-2); else C->SetWidth(-1);
        }
    }();
    AListView->Columns()->EndUpdate();
    AListView->Items()->EndUpdate();
    delete VUserSt;
}

// Scales Graphic proportionally into a bitmap of the image list's size
// (centered, like ImageStretch.ScaleImage) and adds it to the image list.
static void ScaleGraphicToImageList(const QImage &Graphic, TImageList *ImageList, bool UseFiltering, bool UseBackgroundColor, TColor BackgroundColor)
{
    QImage B(qMax(1, ImageList->Width()), qMax(1, ImageList->Height()), QImage::Format_ARGB32_Premultiplied);
    // B.Transparent:=True
    if (UseBackgroundColor) B.fill(TColorToQColor(BackgroundColor)); else B.fill(Qt::transparent);

    if (!Graphic.isNull() && Graphic.width() > 0 && Graphic.height() > 0) {
        double D1 = double(Graphic.width()) / Graphic.height(), D2 = double(B.width()) / B.height();
        int W, H;
        if (D1 >= D2) { W = B.width(); H = Round(W / D1); } else { H = B.height(); W = Round(H * D1); }

        // ScaleImage(B2,B,W,H,UseFiltering)
        W = qMax(1, Min(W, B.width()));
        H = qMax(1, Min(H, B.height()));
        // Without filtering VCL used SimpleStretch (interpolating) when shrinking
        const bool Smooth = UseFiltering || (W < Graphic.width());
        QImage TempDest = Graphic.scaled(W, H, Qt::IgnoreAspectRatio, Smooth ? Qt::SmoothTransformation : Qt::FastTransformation);
        int DW = B.width() - TempDest.width();
        int DH = B.height() - TempDest.height();
        QPainter P(&B);
        P.setRenderHint(QPainter::SmoothPixmapTransform, Smooth);
        P.drawImage(QRect(DW / 2, DH / 2, B.width() - 2 * (DW / 2), B.height() - 2 * (DH / 2)), TempDest);
        P.end();
    }

    ImageList->Add(QPixmap::fromImage(B));
}

static bool LoadScreenshotToImageLists(TGame *Game, TImageList *ImageList2, bool UseBackgroundColor, TColor BackgroundColor)
{
    QString S, T, U;
    TSearchRec Rec;
    int I;
    bool B;

    S = Trim(Game->CaptureFolder());
    if (S.isEmpty()) return false;
    S = MakeAbsPath(S, PrgSetup->BaseDir());
    if (!DirectoryExists(S)) return false;

    B = false;
    if (!Trim(Game->ScreenshotListScreenshot()).isEmpty()) {
        if (Pos("\\", Game->ScreenshotListScreenshot()) != 0 || Pos("/", Game->ScreenshotListScreenshot()) != 0) {
            T = MakeAbsPath(Game->ScreenshotListScreenshot(), PrgSetup->BaseDir());
        } else {
            T = IncludeTrailingPathDelimiter(S) + Trim(Game->ScreenshotListScreenshot());
        }
        B = FileExists(T);
    }

    if ((!B) && PrgSetup->ScreenshotListUseFirstScreenshot()) {
        T = "";
        TStringList St;
        I = FindFirst(IncludeTrailingPathDelimiter(S) + "*.*", faAnyFile, Rec);
        while (I == 0) {
            if ((Rec.Attr & faDirectory) == 0) {
                U = ExtUpperCase(ExtractFileExt(Rec.Name));
                if (U == ".BMP" || U == ".GIF" || U == ".PNG" || U == ".JPG" || U == ".JPEG") St.Add(Rec.Name);
            }
            I = FindNext(Rec);
        }
        FindClose(Rec);
        St.Sort();

        if (St.Count() > 0) T = IncludeTrailingPathDelimiter(S) + St[Min(St.Count() - 1, Max(0, PrgSetup->ScreenshotListUseFirstScreenshotNr() - 1))];
    }

    if (T.isEmpty()) return false;

    QPixmap P = PictureCache->GetPicture(T);
    bool result = !P.isNull();

    if (result) ScaleGraphicToImageList(P.toImage(), ImageList2, true, UseBackgroundColor, BackgroundColor);
    return result;
}

static QString UserInfoGetValue(const QString &UserInfo, const QString &Key)
{
    QString KeyUpper = ExtUpperCase(Key);

    TStringList *St = StringToStringList(UserInfo);
    for (int I = 0; I < St->Count(); I++) {
        QString S = Trim(ExtUpperCase((*St)[I]));
        int J = Pos("=", S);
        if (J == 0) continue;
        if (Trim(Copy(S, 1, J - 1)) == KeyUpper) {
            QString result = Trim(Copy(Trim((*St)[I]), J + 1, MaxInt));
            delete St;
            return result;
        }
    }
    delete St;
    return QString();
}

void AddGameToList(TListView *AListView, int &ItemsUsed, TImageList *AListViewImageList, TImageList *AListViewIconImageList, TImageList *AImageList, TGame *Game, bool ShowExtraInfo, const QString &O, const QString &V, TStringList *VUserSt, const QString &T, bool ScreenshotViewMode, bool ScummVMTemplate, bool UseBackgroundColor, TColor BackgroundColor)
{
    Q_UNUSED(AImageList);
    int IconNr, I, Nr;
    bool B;
    QString S;
    TListItem *L;
    int SubUsed, SubCount;
    QIcon Icon;

    // Set IconNr to the corrosponding icon in imagelist
    B = false; IconNr = 0;
    if (ScreenshotViewMode) {
        B = LoadScreenshotToImageLists(Game, AListViewIconImageList, UseBackgroundColor, BackgroundColor);
        if (B) IconNr = AListViewIconImageList->Count() - 1;
    }
    if (!B) {
        if (!Trim(Game->Icon()).isEmpty()) S = MakeAbsIconName(Game->Icon()); else S = "";
        if (!S.isEmpty() && !FileExists(S)) S = "";
        if (S.isEmpty() && WindowsExeMode(Game) && PrgSetup->UseWindowsExeIcons())
            S = MakeAbsIconName(MakeAbsPath(Game->GameExe(), PrgSetup->BaseDir()));
        if (!S.isEmpty()) Icon = IconCache->GetIcon(S); else Icon = QIcon();
        B = !Icon.isNull();
        if (B) {
            AListViewImageList->AddIcon(Icon);
            if (ScreenshotViewMode) {
                ScaleGraphicToImageList(IconToImage(Icon), AListViewIconImageList, true, UseBackgroundColor, BackgroundColor);
            } else {
                if (IconWidth(Icon) >= AListViewIconImageList->Width()) {
                    I = AListViewIconImageList->Count();
                    AListViewIconImageList->AddIcon(Icon);
                    if (AListViewIconImageList->Count() == I) ScaleGraphicToImageList(IconToImage(Icon), AListViewIconImageList, false, UseBackgroundColor, BackgroundColor);
                } else {
                    ScaleGraphicToImageList(IconToImage(Icon), AListViewIconImageList, false, UseBackgroundColor, BackgroundColor);
                }
            }
        }
        if (B) IconNr = AListViewIconImageList->Count() - 1;
    }

    // Set L to TListItem to use
    if (ItemsUsed >= AListView->Items()->Count())
        L = AListView->Items()->Add();
    else
        L = AListView->Items()->Item(ItemsUsed);
    ItemsUsed++;

    // Set caption
    if (Game->CacheName.isEmpty() && (!Game->OwnINI())) {
        if (ScummVMTemplate)
            L->SetCaption(LanguageSetup->TemplateFormDefaultScummVM());
        else
            L->SetCaption(LanguageSetup->TemplateFormDefault());
    } else {
        if (L->Caption() != Game->CacheName) L->SetCaption(Game->CacheName);
    }

    // Set additional cols
    L->SetData(Game);

    SubUsed = 0; SubCount = L->SubItems().Count();
    L->SubItems().BeginUpdate();
    if (ShowExtraInfo) {
        for (I = 0; I < Length(O); I++) {
            Nr = ColumnCharToNr(O[I]);
            if (Nr < 1) continue;
            if (Nr <= 8) {
                if (V[Nr - 1] == QChar('0')) continue;
            } else {
                if (Nr - 8 > VUserSt->Count()) continue;
            }
            switch (Nr - 1) {
                case 0: if (!Trim(Game->SetupExe()).isEmpty()) S = LanguageSetupFastYes; else S = LanguageSetupFastNo; break;
                case 1: S = GetCustomGenreName(Game->CacheGenre); if (Trim(S).isEmpty()) S = T; break;
                case 2: S = Game->CacheDeveloper; if (Trim(S).isEmpty()) S = T; break;
                case 3: S = Game->CachePublisher; if (Trim(S).isEmpty()) S = T; break;
                case 4: S = Game->CacheYear; if (Trim(S).isEmpty()) S = T; break;
                case 5: S = GetCustomLanguageName(Game->CacheLanguage); if (Trim(S).isEmpty()) S = T; break;
                case 6: {
                    TStringList *St = StringToStringList(Game->Notes()); S = St->Text(); delete St;
                    for (int J = 0; J < S.length(); J++) if (S[J] == QChar(10) || S[J] == QChar(13)) S[J] = QChar(' ');
                    if (Length(S) > 100) S = Copy(S, 1, 100) + "...";
                    if (Trim(S).isEmpty()) S = T;
                    break;
                }
                case 7: S = IntToStr(History->GameStartCount(Game->CacheName)); break;
                default: S = UserInfoGetValue(Game->UserInfo(), (*VUserSt)[(Nr - 1) - 8]); break;
            }
            if (SubUsed < SubCount) L->SubItems().SetStrings(SubUsed, S); else L->SubItems().Add(S);
            SubUsed++;
        }
    } else {
        if (!Trim(Game->SetupExe()).isEmpty()) S = LanguageSetupFastYes; else S = LanguageSetupFastNo;
        if (SubUsed < SubCount) {
            if (L->SubItems()[SubUsed] != S) L->SubItems().SetStrings(SubUsed, S);
        } else {
            L->SubItems().Add(S);
        }
        SubUsed++;
    }
    while (SubCount > SubUsed) { L->SubItems().Delete(SubCount - 1); SubCount--; }
    L->SubItems().EndUpdate();
    L->SetImageIndex(IconNr);
}

void AddGameToList(TListView *AListView, int &ItemsUsed, TImageList *AListViewImageList, TImageList *AListViewIconImageList, TImageList *AImageList, TGame *Game, bool ShowExtraInfo, bool ScreenshotViewMode, bool ScummVMTemplate, bool UseBackgroundColor, TColor BackgroundColor)
{
    QString O, V, VUser, T;
    GetColOrderAndVisible(O, V, VUser);
    TStringList *VUserSt = ValueToList(VUser);
    if (Trim(PrgSetup->ValueForNotSet()).isEmpty()) T = LanguageSetup->NotSet(); else T = Trim(PrgSetup->ValueForNotSet());
    AddGameToList(AListView, ItemsUsed, AListViewImageList, AListViewIconImageList, AImageList, Game, ShowExtraInfo, O, V, VUserSt, T, ScreenshotViewMode, ScummVMTemplate, UseBackgroundColor, BackgroundColor);
    delete VUserSt;
}

void AddGamesToList(TListView *AListView, TImageList *AListViewImageList, TImageList *AListViewIconImageList, TImageList *AImageList, TGameDB *GameDB, const QString &Group, const QString &SubGroup, const QString &SearchString, bool ShowExtraInfo, TSortListBy SortBy, bool ReverseOrder, bool HideScummVMProfiles, bool HideWindowsProfiles, bool HideDefaultProfile, bool ScreenshotViewMode)
{
    AddGamesToList(AListView, AListViewImageList, AListViewIconImageList, AImageList, GameDB, nullptr, Group, SubGroup, SearchString, ShowExtraInfo, SortBy, ReverseOrder, HideScummVMProfiles, HideWindowsProfiles, HideDefaultProfile, ScreenshotViewMode);
}

static bool GroupMatch(const QString &GameGroupUpper, const QString &SelectedGroupUpper)
{
    if (GameGroupUpper == SelectedGroupUpper) return true;

    QString S = GameGroupUpper;
    int I = Pos(";", S);
    if (I != 0) while (!S.isEmpty()) {
        if (I > 0) {
            if (Trim(Copy(S, 1, I - 1)) == SelectedGroupUpper) return true;
            S = Trim(Copy(S, I + 1, MaxInt));
        } else {
            if (Trim(S) == SelectedGroupUpper) return true;
            break;
        }
        I = Pos(";", S);
    }

    return false;
}

static int SortByStartCount(void *Item1, void *Item2)
{
    return History->GameStartCount(static_cast<TGame *>(Item2)->CacheName) - History->GameStartCount(static_cast<TGame *>(Item1)->CacheName);
}

void AddGamesToList(TListView *AListView, TImageList *AListViewImageList, TImageList *AListViewIconImageList, TImageList *AImageList, TGameDB *GameDB, TGame *Game, const QString &Group, const QString &SubGroup, const QString &SearchString, bool ShowExtraInfo, TSortListBy SortBy, bool ReverseOrder, bool HideScummVMProfiles, bool HideWindowsProfiles, bool HideDefaultProfile, bool ScreenshotViewMode)
{
    int I, J, K, Nr, ItemsUsed;
    QString GroupUpper, SubGroupUpper;
    bool B, FirstDefaultTemplate;
    QList<int> C;
    QString S, T;
    QString O, V, VUser;
    QString EmType1, EmType2, EmType3;
    QStringList EmTypeUser;
    bool UseBackgroundColor;
    TColor BackgroundColor;
    TStringList *RecentlyStarted;

    LogInfo("### Start of AddGamesToList ###");

    S = Trim(PrgSetup->GamesListViewBackground());
    UseBackgroundColor = !S.isEmpty(); BackgroundColor = clWhite;
    if (UseBackgroundColor) { try { BackgroundColor = StringToColor(S); } catch (...) { UseBackgroundColor = false; } }

    LogInfo("Preparing ListView");
    // Prepare ListView
    AListViewImageList->Clear();

    ScaleGraphicToImageList(AImageList->Pixmap(0).toImage(), AListViewImageList, false, UseBackgroundColor, BackgroundColor);

    AListViewIconImageList->Clear();
    if (ScreenshotViewMode) {
        ScaleGraphicToImageList(AImageList->Pixmap(0).toImage(), AListViewIconImageList, false, UseBackgroundColor, BackgroundColor);
    } else {
        ScaleGraphicToImageList(AImageList->Pixmap(0).toImage(), AListViewIconImageList, false, UseBackgroundColor, BackgroundColor);
    }
    AListView->Columns()->BeginUpdate();
    for (I = 0; I < AListView->Columns()->Count(); I++) { C.append(AListView->Columns()->Item(I)->Width()); AListView->Columns()->Item(I)->SetWidth(1); }
    AListView->Columns()->EndUpdate();

    LogInfo("Selecting game to add to ListView");
    RecentlyStarted = nullptr;
    TList List;

    // Select games to add
    {
        TGamesListFilter *Filter = new TGamesListFilter;
        Filter->SetFilterString(SearchString);
        if (SubGroup.isEmpty() && (Group != LanguageSetup->GameRecentlyPlayed())) {
            B = (Group == LanguageSetup->GameFavorites());
            for (I = 0; I < GameDB->Count(); I++) {
                TGame *G = GameDB->Game(I);
                if (B && (!G->Favorite())) continue;
                if (HideDefaultProfile && (G->Name() == DosBoxDOSProfile)) continue;
                if (HideScummVMProfiles && (ScummVMMode(G) || WindowsExeMode(G))) continue;
                if (HideWindowsProfiles && WindowsExeMode(G)) continue;
                if (!SearchString.isEmpty() && (!Filter->FilterOk(G))) continue;
                List.append(G);
            }
        } else {
            GroupUpper = Trim(ExtUpperCase(Group));
            if (SubGroup == LanguageSetup->NotSet()) SubGroupUpper = ""; else SubGroupUpper = ExtUpperCase(SubGroup);
            Nr = 7;
            if (Group == LanguageSetup->GameGenre()) Nr = 0;
            if (Group == LanguageSetup->GameDeveloper()) Nr = 1;
            if (Group == LanguageSetup->GamePublisher()) Nr = 2;
            if (Group == LanguageSetup->GameYear()) Nr = 3;
            if (Group == LanguageSetup->GameLanguage()) Nr = 4;
            if (Group == LanguageSetup->GameEmulationType()) Nr = 5;
            if (Group == LanguageSetup->GameLicense()) Nr = 6;
            if (Group == LanguageSetup->GameRecentlyPlayed()) Nr = 8;

            if (Nr == 8) {
                RecentlyStarted = History->GetRecentlyStarted(50, true);
            }

            EmType1 = ExtUpperCase(LanguageSetup->GameEmulationTypeDOSBox());
            EmType2 = ExtUpperCase(LanguageSetup->GameEmulationTypeScummVM());
            EmType3 = ExtUpperCase(LanguageSetup->GameEmulationTypeWindows());
            TStringList *EmuNames = PrgSetup->WindowsBasedEmulatorsNames();
            TStringList *EmuPrograms = PrgSetup->WindowsBasedEmulatorsPrograms();
            for (I = 0; I < EmuNames->Count(); I++) EmTypeUser.append(ExtUpperCase((*EmuNames)[I]));

            for (I = 0; I < GameDB->Count(); I++) {
                TGame *G = GameDB->Game(I);
                if (HideScummVMProfiles && (ScummVMMode(G) || WindowsExeMode(G))) continue;
                if (HideDefaultProfile && (G->Name() == DosBoxDOSProfile)) continue;
                B = false;
                switch (Nr) {
                    case 0: B = GroupMatch(ExtUpperCase(GetCustomGenreName(G->CacheGenre)), SubGroupUpper); break;
                    case 1: B = GroupMatch(G->CacheDeveloperUpper, SubGroupUpper); break;
                    case 2: B = GroupMatch(G->CachePublisherUpper, SubGroupUpper); break;
                    case 3: B = GroupMatch(G->CacheYearUpper, SubGroupUpper); break;
                    case 4: B = GroupMatch(ExtUpperCase(GetCustomLanguageName(G->CacheLanguage)), SubGroupUpper); break;
                    case 5:
                        if (SubGroupUpper == EmType1) B = DOSBoxMode(G); else {
                            if (SubGroupUpper == EmType2) B = ScummVMMode(G); else {
                                if (SubGroupUpper == EmType3) {
                                    B = WindowsExeMode(G);
                                    if (B) for (J = 0; J < EmTypeUser.size(); J++) {
                                        if (J < EmuPrograms->Count() && ExtUpperCase(MakeAbsPath(G->GameExe(), PrgSetup->BaseDir())) == ExtUpperCase((*EmuPrograms)[J])) {
                                            B = false;
                                            break;
                                        }
                                    }
                                } else {
                                    B = false;
                                    if (WindowsExeMode(G)) for (J = 0; J < EmTypeUser.size(); J++) if (SubGroupUpper == EmTypeUser[J]) {
                                        B = (J < EmuPrograms->Count()) && (ExtUpperCase(MakeAbsPath(G->GameExe(), PrgSetup->BaseDir())) == ExtUpperCase((*EmuPrograms)[J]));
                                        break;
                                    }
                                }
                            }
                        }
                        break;
                    case 6: B = GroupMatch(ExtUpperCase(GetCustomLicenseName(G->License())), SubGroupUpper); break;
                    case 7: {
                        B = SubGroupUpper.isEmpty();
                        TStringList *St2 = StringToStringList(G->CacheUserInfo);
                        for (J = 0; J < St2->Count(); J++) {
                            S = (*St2)[J]; K = Pos("=", S); if (K == 0) T = ""; else { T = Trim(Copy(S, K + 1, MaxInt)); S = Trim(Copy(S, 1, K - 1)); }
                            if (Trim(ExtUpperCase(S)) == GroupUpper) { B = GroupMatch(Trim(ExtUpperCase(T)), SubGroupUpper); break; }
                        }
                        delete St2;
                        break;
                    }
                    case 8:
                        B = (RecentlyStarted->IndexOf(G->CacheNameUpper) >= 0);
                        break;
                    default: break;
                }
                if (!B) continue;
                if (!SearchString.isEmpty() && (!Filter->FilterOk(G))) continue;
                List.append(G);
            }
        }
        delete Filter;
    }

    if (Game != nullptr) {
        List.append(Game); // DOSBox default template
        List.append(Game); // ScummVM default template
    }

    LogInfo("Sorting games");
    TStringList *St = new TStringList;
    {
        // Sort games
        GetColOrderAndVisible(O, V, VUser);
        TStringList *VUserSt = ValueToList(VUser);
        if (SortBy != slbStartCount) {
            for (I = 0; I < List.size(); I++) {
                TGame *G = static_cast<TGame *>(List[I]);
                switch (SortBy) {
                    case slbName: St->AddObject(G->CacheName, G); break;
                    case slbSetup:
                        if (!Trim(G->SetupExe()).isEmpty())
                            St->AddObject(RemoveUnderline(LanguageSetup->Yes()), G);
                        else
                            St->AddObject(RemoveUnderline(LanguageSetup->No()), G);
                        break;
                    case slbGenre: St->AddObject(GetCustomGenreName(G->CacheGenre), G); break;
                    case slbDeveloper: St->AddObject(G->CacheDeveloper, G); break;
                    case slbPublisher: St->AddObject(G->CachePublisher, G); break;
                    case slbYear: St->AddObject(G->CacheYear, G); break;
                    case slbLanguage: St->AddObject(GetCustomLanguageName(G->CacheLanguage), G); break;
                    case slbComment: St->AddObject(G->Notes(), G); break;
                    /* slbStartCount : begin
                                         S:=IntToStr(History.GameStartCount(TGame(List[I]).CacheName));
                                         while length(S)<6 do S:='0'+S;
                                         St.AddObject(S,TGame(List[I]));
                                       end; */
                    default: break;
                }
                if (int(SortBy) >= 10) {
                    St->AddObject(UserInfoGetValue(G->UserInfo(), (*VUserSt)[Min(VUserSt->Count() - 1, int(SortBy) - 10)]), G);
                }
            }
            St->Sort();
        } else {
            std::stable_sort(List.begin(), List.end(), [](void *A, void *B2) { return SortByStartCount(A, B2) < 0; });
            for (I = 0; I < List.size(); I++) St->AddObject("", List[I]);
        }

        LogInfo("Adding game to ListView (start)");
        // Add games to list
        ItemsUsed = 0;
        if (Trim(PrgSetup->ValueForNotSet()).isEmpty()) T = LanguageSetup->NotSet(); else T = Trim(PrgSetup->ValueForNotSet());
        AListView->Items()->BeginUpdate();
        FirstDefaultTemplate = true;

        QWidget *F = nullptr;
        if (St->Count() > 50) {
            QWidget *W = AListView->window();
            if (W != nullptr && W->isWindow()) F = W;
            if (F != nullptr) F->setEnabled(false);
        }

        if (ReverseOrder) {
            for (I = St->Count() - 1; I >= 0; I--) {
                if ((I % 25 == 0) && F != nullptr) ProcessMessages();
                TGame *G = St->ObjectAs<TGame>(I);
                if (St->Count() < 100) LogInfo("Adding " + IntToStr(I + 1) + "/" + IntToStr(St->Count()) + ": " + (G != nullptr ? G->CacheName : QString()));
                if (G == nullptr) continue;
                AddGameToList(AListView, ItemsUsed, AListViewImageList, AListViewIconImageList, AImageList, G, ShowExtraInfo, O, V, VUserSt, T, ScreenshotViewMode, FirstDefaultTemplate, UseBackgroundColor, BackgroundColor);
                if (G->CacheName.isEmpty() && (!G->OwnINI())) FirstDefaultTemplate = false;
            }
        } else {
            for (I = 0; I < St->Count(); I++) {
                if ((I % 25 == 0) && F != nullptr) ProcessMessages();
                TGame *G = St->ObjectAs<TGame>(I);
                if (St->Count() < 100) LogInfo("Adding " + IntToStr(I + 1) + "/" + IntToStr(St->Count()) + ": " + (G != nullptr ? G->CacheName : QString()));
                if (G == nullptr) continue;
                AddGameToList(AListView, ItemsUsed, AListViewImageList, AListViewIconImageList, AImageList, G, ShowExtraInfo, O, V, VUserSt, T, ScreenshotViewMode, !FirstDefaultTemplate, UseBackgroundColor, BackgroundColor);
                if (G->CacheName.isEmpty() && (!G->OwnINI())) FirstDefaultTemplate = false;
            }
        }
        if (F != nullptr) F->setEnabled(true);
        AListView->Items()->EndUpdate();
        delete VUserSt;

        LogInfo("Adding game to ListView (done)");

        while (ItemsUsed < AListView->Items()->Count()) AListView->Items()->Delete(AListView->Items()->Count() - 1);
    }
    delete St;

    if (RecentlyStarted != nullptr) delete RecentlyStarted;
    LogInfo("Updating column widths");
    AListView->Columns()->BeginUpdate();
    for (I = 0; I < Min(int(C.size()), AListView->Columns()->Count()); I++) AListView->Columns()->Item(I)->SetWidth(C[I]);
    AListView->Columns()->EndUpdate();

    LogInfo("### End of AddGamesToList ###");
}

QString GamesListSaveColWidthsToString(TListView *AListView)
{
    QString O, V, VUser, result;
    int Nr, C;

    GetColOrderAndVisible(O, V, VUser);
    TStringList *VUserSt = ValueToList(VUser);
    if (PrgSetup->ShowExtraInfo()) {
        TStringList *St = ValueToList(PrgSetup->ColumnWidths());
        while (St->Count() < 9) St->Add("-1");
        for (int I = 8; I < St->Count(); I++) (*St)[I] = "-1";
        (*St)[0] = IntToStr(AListView->Column(0)->Width());
        C = 0;
        for (int I = 0; I < Length(O); I++) {
            Nr = ColumnCharToNr(O[I]);
            if (Nr < 1) continue;
            if (Nr <= 8) {
                if (V[Nr - 1] == QChar('0')) continue;
            } else {
                if (Nr - 8 > VUserSt->Count()) continue;
            }
            C++;
            if (AListView->Columns()->Count() > C) {
                while (St->Count() < Nr + 1) St->Add("-1");
                (*St)[Nr] = IntToStr(AListView->Column(C)->Width());
            }
        }
        result = ListToValue(St);
        delete St;
    } else {
        TStringList St;
        St.Add(IntToStr(AListView->Column(0) ? AListView->Column(0)->Width() : -1));
        St.Add(IntToStr(AListView->Column(1) ? AListView->Column(1)->Width() : -1));
        result = ListToValue(&St);
    }
    delete VUserSt;
    return result;
}

void GamesListLoadColWidthsFromString(TListView *AListView, const QString &Data)
{
    QString O, V, VUser;
    int Nr, C, W;

    GetColOrderAndVisible(O, V, VUser);
    TStringList *VUserSt = ValueToList(VUser);
    if (PrgSetup->ShowExtraInfo()) {
        TStringList *St = ValueToList(Data);
        while (St->Count() < 9 + VUserSt->Count()) St->Add("");
        if (TryStrToInt((*St)[0], W) && (W >= -2) && (W < 1000) && AListView->Column(0)) AListView->Column(0)->SetWidth(W);
        C = 0;
        for (int I = 0; I < Length(O); I++) {
            Nr = ColumnCharToNr(O[I]);
            if (Nr < 1) continue;
            if (Nr <= 8) {
                if (V[Nr - 1] == QChar('0')) continue;
            } else {
                if (Nr - 8 > VUserSt->Count()) continue;
            }
            C++;
            if (Nr < St->Count()) {
                if (TryStrToInt((*St)[Nr], W) && (W >= -2) && (W < 1000) && AListView->Column(C)) AListView->Column(C)->SetWidth(W);
            }
        }
        delete St;
    } else {
        TStringList *St = ValueToList(Data);
        while (St->Count() < 2) St->Add("");
        if (TryStrToInt((*St)[0], W) && (W >= -2) && (W < 1000) && AListView->Column(0)) AListView->Column(0)->SetWidth(W);
        if (TryStrToInt((*St)[1], W) && (W >= -2) && (W < 1000) && AListView->Column(1)) AListView->Column(1)->SetWidth(W);
        delete St;
    }
    delete VUserSt;
}

void GamesListSaveColWidths(TListView *AListView)
{
    if (PrgSetup->ShowExtraInfo())
        PrgSetup->SetColumnWidths(GamesListSaveColWidthsToString(AListView));
    else
        PrgSetup->SetColumnWidthsNoExtraInfoMode(GamesListSaveColWidthsToString(AListView));
}

void GamesListLoadColWidths(TListView *AListView)
{
    if (PrgSetup->ShowExtraInfo())
        GamesListLoadColWidthsFromString(AListView, PrgSetup->ColumnWidths());
    else
        GamesListLoadColWidthsFromString(AListView, PrgSetup->ColumnWidthsNoExtraInfoMode());
}

void AddScreenshotsToList(TListView *AListView, TImageList *AImageList, const QString &ADir)
{
    static const char *const Exts[5] = {"png", "jpg", "jpeg", "gif", "bmp"};
    TSearchRec Rec;
    int I;

    AListView->SetSortType(stNone);
    QString Dir = IncludeTrailingPathDelimiter(ADir);

    TStringList St;
    // Get files list
    for (int K = 0; K < 5; K++) {
        I = FindFirst(Dir + "*." + QString::fromLatin1(Exts[K]), faAnyFile, Rec);
        while (I == 0) { St.Add(Rec.Name); I = FindNext(Rec); }
        FindClose(Rec);
    }

    // Sort list
    St.Sort();

    // Add games to list view
    for (int K = 0; K < St.Count(); K++) {
        QImage B = ScreenshotsCache->GetThumbnail(Dir + St[K], AImageList->Width(), AImageList->Height());
        if (!B.isNull()) {
            AImageList->Add(QPixmap::fromImage(B));
            TListItem *L = AListView->Items()->Add();
            L->SetCaption(St[K]);
            L->SetImageIndex(AImageList->Count() - 1);
        }
    }

    AListView->SetSortType(stNone);
    AListView->SetSortType(stText);
}

void AddSoundsToList(TListView *AListView, const QString &ADir, int ImageListIndex)
{
    QString Dir = IncludeTrailingPathDelimiter(ADir);

    auto FindAndAddFiles = [&](const QString &Ext) {
        TSearchRec Rec;
        int I = FindFirst(Dir + "*." + Ext, faAnyFile, Rec);
        while (I == 0) {
            TListItem *L = AListView->Items()->Add();
            L->SetCaption(Rec.Name);
            L->SetImageIndex(ImageListIndex);
            I = FindNext(Rec);
        }
        FindClose(Rec);
    };

    FindAndAddFiles("wav");
    FindAndAddFiles("mp3");
    FindAndAddFiles("ogg");
    FindAndAddFiles("mid");
    FindAndAddFiles("midi");

    AListView->SetSortType(stNone);
    AListView->SetSortType(stText);
}

void AddVideosToList(TListView *AListView, const QString &ADir, int ImageListIndex)
{
    QString Dir = IncludeTrailingPathDelimiter(ADir);
    TSearchRec Rec;
    int I;

    for (const QString &Ext : {QStringLiteral("avi"), QStringLiteral("mpg"), QStringLiteral("mpeg"), QStringLiteral("wmv")}) {
        I = FindFirst(Dir + "*." + Ext, faAnyFile, Rec);
        while (I == 0) {
            TListItem *L = AListView->Items()->Add();
            L->SetCaption(Rec.Name);
            L->SetImageIndex(ImageListIndex);
            I = FindNext(Rec);
        }
        FindClose(Rec);
    }

    AListView->SetSortType(stNone);
    AListView->SetSortType(stText);
}

static void DeleteFiles(const QString &ADir, const QString &AMask)
{
    TSearchRec Rec;
    QString Dir = IncludeTrailingPathDelimiter(ADir);
    int I = FindFirst(Dir + AMask, faAnyFile, Rec);
    while (I == 0) {
        ExtDeleteFile(Dir + Rec.Name, ftProfile);
        I = FindNext(Rec);
    }
    FindClose(Rec);
}

void DeleteOldFiles()
{
    if (!PrgSetup->CreateConfFilesForProfiles() || (OperationMode == omPortable)) {
        DeleteFiles(PrgDataDir() + GameListSubDir, "*.conf");
    }
    DeleteFiles(PrgDataDir() + GameListSubDir, "tempprof.*");
    ExtDeleteFile(PrgDataDir() + "Developers.dat", ftProfile);
    ExtDeleteFile(PrgDataDir() + "Genres.dat", ftProfile);
    ExtDeleteFile(PrgDataDir() + "Profiles.dat", ftProfile);
    ExtDeleteFile(PrgDataDir() + "Publisher.dat", ftProfile);
    ExtDeleteFile(PrgDataDir() + "stderr.txt", ftProfile);
    ExtDeleteFile(PrgDataDir() + "stdout.txt", ftProfile);
    DeleteFiles(PrgDataDir(), "temp.*");
    ExtDeleteFile(PrgDataDir() + "Templates.dat", ftProfile);
    ExtDeleteFile(PrgDataDir() + "Years.dat", ftProfile);
}

static QString RemoveBackslash(const QString &S)
{
    if (S == "\\" || S == "/") return QString(); else return S;
}

void ReplaceAbsoluteDirs(TGameDB *GameDB)
{
    QString S, T;
    int J, K;

    for (int I = 0; I < GameDB->Count(); I++) {
        TGame *G = GameDB->Game(I);

        G->SetGameExe(RemoveBackslash(MakeRelPath(G->GameExe(), PrgSetup->BaseDir())));
        G->SetSetupExe(RemoveBackslash(MakeRelPath(G->SetupExe(), PrgSetup->BaseDir())));
        G->SetCaptureFolder(RemoveBackslash(MakeRelPath(G->CaptureFolder(), PrgSetup->BaseDir(), true)));
        G->SetDataDir(RemoveBackslash(MakeRelPath(G->DataDir(), PrgSetup->BaseDir(), true)));

        TStringList *St = ValueToList(G->ExtraFiles());
        for (J = 0; J < St->Count(); J++) (*St)[J] = RemoveBackslash(MakeRelPath((*St)[J], PrgSetup->BaseDir()));
        J = 0; while (J < St->Count()) { if (Trim((*St)[J]).isEmpty()) St->Delete(J); else J++; }
        G->SetExtraFiles(ListToValue(St));
        delete St;

        St = ValueToList(G->ExtraDirs());
        for (J = 0; J < St->Count(); J++) (*St)[J] = RemoveBackslash(MakeRelPath((*St)[J], PrgSetup->BaseDir(), true));
        J = 0; while (J < St->Count()) { if (Trim((*St)[J]).isEmpty()) St->Delete(J); else J++; }
        G->SetExtraDirs(ListToValue(St));
        delete St;

        for (J = 0; J < G->NrOfMounts(); J++) {
            St = ValueToList(G->Mount(J));
            if (St->Count() > 0) {
                S = Trim((*St)[0]); T = "";
                K = Pos("$", S);
                while (K > 0) {
                    T = T + RemoveBackslash(MakeRelPath(Trim(Copy(S, 1, K - 1)), PrgSetup->BaseDir(), true)) + "$";
                    S = Trim(Copy(S, K + 1, MaxInt));
                    K = Pos("$", S);
                }
                T = T + RemoveBackslash(MakeRelPath(Trim(S), PrgSetup->BaseDir(), true));
                (*St)[0] = T;
                SetGameMount(G, J, ListToValue(St));
            }
            delete St;
        }

        G->StoreAllValues();
        G->LoadCache();
    }
}

bool CopyFiles(const QString &ASource, const QString &ADest, bool OverwriteExistingFiles, bool ProcessMessages_)
{
    TSearchRec Rec;
    int I;

    QString Source = IncludeTrailingPathDelimiter(ASource);
    QString Dest = IncludeTrailingPathDelimiter(ADest);
    if (!ForceDirectories(Dest)) return false;

    TStringList Files, Folders;
    I = FindFirst(Source + "*.*", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) != 0) {
            if (Rec.Name != "." && Rec.Name != "..") Folders.Add(Rec.Name);
        } else {
            if (ProcessMessages_ && (Files.Count() % 20 == 0)) ProcessMessages();
            Files.Add(Rec.Name);
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);

    for (I = 0; I < Files.Count(); I++) {
        if (ProcessMessages_) ProcessMessages();
        if (OverwriteExistingFiles) {
            if (!CopyFile(Source + Files[I], Dest + Files[I], false)) return false;
        } else {
            if (!FileExists(Dest + Files[I])) {
                if (!CopyFile(Source + Files[I], Dest + Files[I], false)) return false;
            }
        }
    }

    for (I = 0; I < Folders.Count(); I++) if (!CopyFiles(Source + Folders[I], Dest + Folders[I], OverwriteExistingFiles, ProcessMessages_)) return false;

    return true;
}

static void UpdateTemplate(TGame *G, int LastVersion)
{
    bool B = false;
    if (LastVersion < 900) {
        if (Trim(ExtUpperCase(G->VideoCard())) == "VGA") { G->SetVideoCard("vga_s3"); B = true; }
        if (G->Cycles() == "Max") { G->SetCycles("max"); B = true; }
        if (G->Cycles() == "Auto") { G->SetCycles("auto"); B = true; }
    }
    if (LastVersion < 910) {
        if (G->MixerRate() == 22050) { G->SetMixerRate(44100); B = true; }
        if (G->SBOplRate() == 22050) { G->SetSBOplRate(44100); B = true; }
        if (G->SpeakerRate() == 22050) { G->SetSpeakerRate(44100); B = true; }
        if (G->SpeakerTandyRate() == 22050) { G->SetSpeakerTandyRate(44100); B = true; }
        if (G->JoystickButtonwrap()) { G->SetJoystickButtonwrap(false); B = true; }
    }

    if (LastVersion < 10000) {
        if (!G->UseScanCodesOld()) { G->SetUseScanCodes(false); B = true; }
    }

    if (B) G->StoreAllValues();
}

void UpdateUserDataFolderAndSettingsAfterUpgrade(TGameDB *GameDB, int LastVersion)
{
    QString Source, Dest, S;
    int I;
    bool B;

    const QString NewUserData = PrgDir() + NewUserDataSubDir + PathDelim;
    const QString Settings = PrgDataDir() + SettingsFolder + PathDelim;

    // Copy new and changed NewUserData files to DataDir
    if (PrgDataDir() != PrgDir()) {
        // Update DataReader.xml to version 6 (only if upgrade from below 1.4.0)
        if (LastVersion < 10400) {
            if (FileExists(NewUserData + DataReaderConfigFile)) CopyFile(NewUserData + DataReaderConfigFile, Settings + DataReaderConfigFile, false); // False = overwrite existing file
        }

        // Update installer package base script (only if upgrade from below 0.9)
        if (LastVersion < 900) {
            if (FileExists(PrgDir() + NSIInstallerHelpFile)) {
                CopyFile(PrgDir() + NSIInstallerHelpFile, Settings + NSIInstallerHelpFile, false); // False = overwrite existing file
            } else {
                if (FileExists(PrgDir() + BinFolder + PathDelim + NSIInstallerHelpFile)) {
                    CopyFile(PrgDir() + BinFolder + PathDelim + NSIInstallerHelpFile, Settings + NSIInstallerHelpFile, false); // False = overwrite existing file
                }
            }
        }

        // Copy Icons.ini to settings folder (always)
        if (FileExists(NewUserData + IconsConfFile)) {
            if (FileExists(Settings + IconsConfFile)) {
                if (FileExists(Settings + ChangeFileExt(IconsConfFile, ".old"))) DeleteFile(Settings + ChangeFileExt(IconsConfFile, ".old"));
                RenameFile(Settings + IconsConfFile, Settings + ChangeFileExt(IconsConfFile, ".old"));
            }
            CopyFile(NewUserData + IconsConfFile, Settings + IconsConfFile, true);
        }

        // Add new auto setup templates (always)
        CopyFiles(NewUserData + AutoSetupSubDir, PrgDataDir() + AutoSetupSubDir, false, true); // False = do not overwrite existing file

        // Update DozZip (only if upgrade from below 1.3)
        if (LastVersion < 10300) {
            Source = NewUserData + "DOSZIP" + PathDelim;
            if (DirectoryExists(Source)) CopyFiles(Source, IncludeTrailingPathDelimiter(PrgSetup->GameDir()) + "DOSZIP" + PathDelim, true, true);
        }

        // Update FreeDOS (only if upgrade from below 1.3)
        if (LastVersion < 10300) {
            Source = NewUserData + "FREEDOS";
            Dest = IncludeTrailingPathDelimiter(PrgSetup->GameDir()) + "FREEDOS";
            if (DirectoryExists(Source) && DirectoryExists(Dest)) {
                if ((GetMD5Sum(Dest + PathDelim + "kernel.sys", true) == FD10KernelSys) && (GetMD5Sum(Source + PathDelim + "kernel.sys", true) == FD11KernelSys))
                    CopyFiles(Source, Dest, true, true);
            }
        }

        // Update DOSBox screenshot
        ForceDirectories(PrgDataDir() + CaptureSubDir + PathDelim + DosBoxDOSProfile + PathDelim);
        CopyFiles(NewUserData + CaptureSubDir + PathDelim + DosBoxDOSProfile + PathDelim, PrgDataDir() + CaptureSubDir + PathDelim + DosBoxDOSProfile + PathDelim, true, false);
    }

    // Update game DB: Max->max, Auto->auto (only if upgrade from below 9.0)
    if (LastVersion < 900) {
        for (I = 0; I < GameDB->Count(); I++) {
            B = false;
            if (GameDB->Game(I)->Cycles() == "Max") { GameDB->Game(I)->SetCycles("max"); B = true; }
            if (GameDB->Game(I)->Cycles() == "Auto") { GameDB->Game(I)->SetCycles("auto"); B = true; }
            if (B) { GameDB->Game(I)->StoreAllValues(); GameDB->Game(I)->LoadCache(); }
            ProcessMessages();
        }
    }

    // Update default template
    {
        TGame *G = new TGame(PrgSetup);
        if (LastVersion < 10000) {
            G->SetGUS(false);
            G->SetCyclesUp(10);
            G->SetMixerRate(44100);
            G->SetMixerBlocksize(1024);
            G->SetMixerPrebuffer(20);
            G->SetSBOplRate(44100);
            G->SetGUSRate(44100);
            G->SetSpeakerRate(44100);
            G->SetSpeakerTandyRate(44100);
            G->SetJoystickButtonwrap(false);
        }
        if (LastVersion < 900) {
            if (G->Cycles() == "Max") G->SetCycles("max");
            if (G->Cycles() == "Auto") G->SetCycles("auto");
        }
        G->StoreAllValues();
        delete G;
    }

    // Update templates and auto setup template (see UpdateTemplate)
    {
        TGameDB *DB = new TGameDB(PrgDataDir() + TemplateSubDir, false);
        for (I = 0; I < DB->Count(); I++) {
            UpdateTemplate(DB->Game(I), LastVersion);
            ProcessMessages();
        }
        delete DB;
        DB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);
        for (I = 0; I < DB->Count(); I++) {
            UpdateTemplate(DB->Game(I), LastVersion);
            ProcessMessages();
        }
        delete DB;
        TGame *G = new TGame(PrgSetup);
        UpdateTemplate(G, LastVersion);
        delete G;
    }

    // Change year from 2007 or 2009 to 2010 and "multilingual" to "Multilingual" in "DOSBox DOS" profile (always / only if upgrade from below 9.0)
    I = GameDB->IndexOf(DosBoxDOSProfile);
    if ((I > 0) && ((GameDB->Game(I)->CacheYear == "2007") || (GameDB->Game(I)->CacheYear == "2009"))) {
        GameDB->Game(I)->SetYear("2010"); GameDB->Game(I)->StoreAllValues(); GameDB->Game(I)->LoadCache();
    }
    if (LastVersion < 900) {
        if ((I > 0) && (GameDB->Game(I)->CacheLanguage == "multilingual")) {
            GameDB->Game(I)->SetLanguage("Multilingual"); GameDB->Game(I)->StoreAllValues(); GameDB->Game(I)->LoadCache();
        }
    }

    // Update Autoexec.bat in "DOSBox DOS" profile (only if upgrade from below 1.3)
    if (LastVersion < 10300) {
        BuildDefaultDosProfileAutoexec(GameDB);
    }

    // Add new games list column
    if (LastVersion < 10009) {
        PrgSetup->SetColVisible(Copy(PrgSetup->ColVisible(), 1, 7) + "0" + Copy(PrgSetup->ColVisible(), 8, MaxInt));
        S = Trim(PrgSetup->ColOrder());
        // Delphi loop "For I:=0 to length(S)" - S[0] is no valid character
        for (I = 1; I <= Length(S); I++) {
            if (S[I - 1] == QChar('8')) { S[I - 1] = QChar('9'); continue; }
            if (S[I - 1] == QChar('9')) { S[I - 1] = QChar('A'); continue; }
            if ((ExtUpperCase(QString(S[I - 1])) >= "A") && (ExtUpperCase(QString(S[I - 1])) <= "Y")) S[I - 1] = QChar(S[I - 1].unicode() + 1);
        }
        PrgSetup->SetColOrder(S);
    }

    // Update settings in ConfOpt.dat
    TConfOpt *ConfOpt = GameDB->ConfOpt();
    if (LastVersion < 11100) {
        ConfOpt->SetResolutionFullscreen(DefaultValuesResolutionFullscreen);
        ConfOpt->SetResolutionWindow(DefaultValuesResolutionWindow);
    }
    if (LastVersion < 10000) {
        ConfOpt->SetVideo(DefaultValuesVideo);
        ConfOpt->SetResolutionFullscreen(DefaultValuesResolutionFullscreen);
        ConfOpt->SetResolutionWindow(DefaultValuesResolutionWindow);
        ConfOpt->SetTandyRate(DefaultValuesTandyRate);
        ConfOpt->SetSblaster(DefaultValuesSBlaster);
        ConfOpt->SetReportedDOSVersion(DefaultValuesReportedDOSVersion);
    }
    if (LastVersion < 902) {
        ConfOpt->SetCodepage(DefaultValuesCodepage);
        ConfOpt->SetTandyRate(DefaultValuesTandyRate);
    }
    if (LastVersion < 901) {
        ConfOpt->SetKeyboardLayout(DefaultValuesKeyboardLayout);
        ConfOpt->SetCodepage(DefaultValuesCodepage);
    }
    if (LastVersion < 900) {
        ConfOpt->SetVideo(DefaultValuesVideo);
    }
    if (LastVersion < 801) {
        ConfOpt->SetJoysticks(DefaultValuesJoysticks);
        ConfOpt->SetGUSRate(DefaultValuesGUSRate);
        ConfOpt->SetOPLRate(DefaultValuesOPLRate);
        ConfOpt->SetPCRate(DefaultValuesPCRate);
        ConfOpt->SetRate(DefaultValuesRate);
        ConfOpt->SetOplmode(DefaultValuesOPLModes);
        ConfOpt->SetSBBase(DefaultValuesSBBase);
        ConfOpt->SetGUSBase(DefaultValuesGUSBase);
        ConfOpt->SetDma(DefaultValuesDMA);
        ConfOpt->SetGUSDma(DefaultValuesDMA1);
        ConfOpt->SetHDMA(DefaultValuesHDMA);
        ConfOpt->SetMemory(DefaultValuesMemory);
    }
    if (LastVersion < 800) {
        ConfOpt->SetResolutionFullscreen(DefaultValuesResolutionFullscreen);
        ConfOpt->SetResolutionWindow(DefaultValuesResolutionWindow);
        ConfOpt->SetScale(DefaultValuesScale);
        ConfOpt->SetVideo(DefaultValuesVideo);
    }
    ConfOpt->StoreAllValues();
}

static void MoveDataFile(const QString &FileName)
{
    if (!FileExists(PrgDataDir() + FileName)) return;

    if (FileExists(PrgDataDir() + SettingsFolder + PathDelim + FileName)) {
        // File in new position already exists -> new version of DFR has already used -> delete old file
        DeleteFile(PrgDataDir() + FileName); // Can't use ExtDelete because ExtDelete querys string from PrgSetup which is not loaded by this time
    } else {
        // Move data  file to new position
        if (CopyFile(PrgDataDir() + FileName, PrgDataDir() + SettingsFolder + PathDelim + FileName, true))
            DeleteFile(PrgDataDir() + FileName); // Can't use ExtDelete because ExtDelete querys string from PrgSetup which is not loaded by this time
    }
}

void UpdateSettingsFilesLocation()
{
    // Move configuration files to SettingsFolder
    ForceDirectories(PrgDataDir() + SettingsFolder);

    MoveDataFile(ConfOptFile);
    MoveDataFile(ScummVMConfOptFile);
    MoveDataFile(HistoryFileName);
    MoveDataFile(IconsConfFile);
    MoveDataFile(MainSetupFile);
    MoveDataFile("Links.txt");
    MoveDataFile("SearchLinks.txt");
    MoveDataFile(NSIInstallerHelpFile);
}

static bool RestoreFREEDOSFolder(bool ShowWaitForm)
{
    bool result = true;

    QString Source1 = PrgDir() + NewUserDataSubDir + PathDelim + "DOSZIP";
    QString Source2 = PrgDir() + NewUserDataSubDir + PathDelim + "FREEDOS";
    QString Dest1 = IncludeTrailingPathDelimiter(PrgSetup->GameDir()) + "DOSZIP";
    QString Dest2 = IncludeTrailingPathDelimiter(PrgSetup->GameDir()) + "FREEDOS";
    bool B1 = DirectoryExists(Source1);
    bool B2 = DirectoryExists(Source2);

    if (B1 || B2) {
        if (ShowWaitForm) LoadAndShowSmallWaitForm(LanguageSetup->SetupFormService3WaitInfo());
        if (B1) result = CopyFiles(Source1, Dest1, true, ShowWaitForm);
        if (B2) result = CopyFiles(Source2, Dest2, true, ShowWaitForm);
        if (ShowWaitForm) FreeSmallWaitForm();
    }
    return result;
}

// TThread descendant (FreeOnTerminate = True, started directly)
class TRestoreFreeDOSFolderThread : public QThread
{
public:
    TRestoreFreeDOSFolderThread()
    {
        QObject::connect(this, &QThread::finished, this, &QObject::deleteLater);   // FreeOnTerminate:=True
        FreeDOSInitThreadRunning = true;
        start(QThread::LowPriority);   // THREAD_PRIORITY_BELOW_NORMAL
    }
protected:
    void run() override
    {
        FreeDOSInitThreadRunning = true;
        while (!MainWindowShowComplete) QThread::msleep(100);
        RestoreFREEDOSFolder(false);
        FreeDOSInitThreadRunning = false;
    }
};

void BuildDefaultDosProfileAutoexec(TGameDB *GameDB)
{
    TGame *Game;
    int I = GameDB->IndexOf(DosBoxDOSProfile);
    if (I >= 0) Game = GameDB->Game(I); else return;

    TStringList St;
    St.Add("C:");
    St.Add("echo.");
    St.Add("");
    St.Add("If exist C:\\FREEDOS\\COMMAND.COM goto AddFreeDos");
    St.Add("Goto Next1");
    St.Add(":AddFreeDos");
    St.Add("set path=%PATH%;C:\\FREEDOS");
    St.Add(":Next1");
    St.Add("");
    St.Add("If exist C:\\NC55\\NC.EXE goto AddNC55");
    St.Add("Goto Next2");
    St.Add(":AddNC55");
    St.Add("set path=%PATH%;C:\\NC55");
    St.Add("echo You can start Norton Commander (file manager) by typing \"NC\".");
    St.Add("Goto Next3");
    St.Add(":Next2");
    St.Add("");
    St.Add("If exist C:\\NC551\\NC.EXE goto AddNC551");
    St.Add("Goto Next3");
    St.Add(":AddNC551");
    St.Add("set path=%PATH%;C:\\NC551");
    St.Add("echo You can start Norton Commander (file manager) by typing \"NC\".");
    St.Add(":Next3");
    St.Add("");
    St.Add("If exist C:\\NDN\\NDN.COM goto AddNDN");
    St.Add("Goto Next4");
    St.Add(":AddNDN");
    St.Add("set path=%PATH%;C:\\NDN");
    St.Add("echo You can start Necromancer's Dos Navigator (file manager) by typing \"NDN\".");
    St.Add(":Next4");
    St.Add("");
    St.Add("If exist C:\\DOSZIP\\DZ.EXE goto AddDZ");
    St.Add("Goto Next5");
    St.Add(":AddDZ");
    St.Add("set path=%PATH%;C:\\DOSZIP");
    St.Add("echo You can start Doszip Commander (file manager) by typing \"DZ\".");
    St.Add(":Next5");
    St.Add("");
    St.Add("If exist C:\\FREEDOS\\DOSSHELL.EXE goto AddDosshell");
    St.Add("Goto Next6");
    St.Add(":AddDosshell");
    St.Add("echo You can start FreeDOS Shell (file manager) by typing \"DOSSHELL\".");
    St.Add(":Next6");
    Game->SetAutoexec(StringListToString(&St));
    Game->StoreAllValues();
    Game->LoadCache();
}

TGame *BuildDefaultDosProfile(TGameDB *GameDB, bool CopyFiles_)
{
    TGame *result;
    int I = GameDB->IndexOf(DosBoxDOSProfile);
    if (I >= 0) result = GameDB->Game(I); else result = GameDB->Game(GameDB->Add(DosBoxDOSProfile));

    BuildDefaultDosProfileAutoexec(GameDB);

    result->SetNrOfMounts(1);
    result->SetMount0(MakeRelPath(PrgSetup->GameDir(), PrgSetup->BaseDir(), true) + ";Drive;C;false;");
    result->SetCloseDosBoxAfterGameExit(false);
    result->SetEnvironment("PATH[61]Z:\\[13]");
    result->SetStartFullscreen(false);
    result->SetCaptureFolder(MakeRelPath(IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + DosBoxDOSProfile, PrgSetup->BaseDir()));
    result->SetGenre("Program");
    result->SetWWW(1, QStringLiteral("http:/") + QStringLiteral("/www.dosbox.com"));
    result->SetName(DosBoxDOSProfile);
    result->SetFavorite(true);
    result->SetDeveloper("DOSBox Team");
    result->SetPublisher("DOSBox Team");
    result->SetYear("2010");
    result->SetLanguage("Multilingual");
    result->SetUserInfo("License=GPL[13]");
    result->SetIcon("DOSBox.ico");

    result->StoreAllValues();
    result->LoadCache();

    const QString NewUserData = PrgDir() + NewUserDataSubDir + PathDelim;
    const QString Settings = PrgDataDir() + SettingsFolder + PathDelim;

    if ((PrgDir() != PrgDataDir()) && DirectoryExists(PrgDir() + NewUserDataSubDir)) {
        if (FileExists(NewUserData + IconsSubDir + PathDelim + "DOSBox.ico")) {
            ForceDirectories(PrgDataDir() + IconsSubDir);
            CopyFile(NewUserData + IconsSubDir + PathDelim + "DOSBox.ico", PrgDataDir() + IconsSubDir + PathDelim + "DOSBox.ico", false);
        }
        if (FileExists(NewUserData + CaptureSubDir + PathDelim + "DOSBox DOS" + PathDelim + "dosbox_000.png")) {
            const QString CaptureDir = IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->CaptureDir(), PrgSetup->BaseDir()));
            ForceDirectories(CaptureDir + "DOSBox DOS");
            CopyFile(NewUserData + CaptureSubDir + PathDelim + "DOSBox DOS" + PathDelim + "dosbox_000.png", CaptureDir + "DOSBox DOS" + PathDelim + "dosbox_000.png", false);
        }
    }

    if (FileExists(PrgDir() + NSIInstallerHelpFile)) {
        CopyFile(PrgDir() + NSIInstallerHelpFile, Settings + NSIInstallerHelpFile, false);
    } else {
        if (FileExists(PrgDir() + BinFolder + PathDelim + NSIInstallerHelpFile)) {
            CopyFile(PrgDir() + BinFolder + PathDelim + NSIInstallerHelpFile, Settings + NSIInstallerHelpFile, false);
        }
    }

    if (FileExists(NewUserData + IconsConfFile)) {
        if (FileExists(Settings + IconsConfFile)) {
            if (FileExists(Settings + ChangeFileExt(IconsConfFile, ".old"))) DeleteFile(Settings + ChangeFileExt(IconsConfFile, ".old"));
            RenameFile(Settings + IconsConfFile, Settings + ChangeFileExt(IconsConfFile, ".old"));
        }
        CopyFile(NewUserData + IconsConfFile, Settings + IconsConfFile, true);
    }

    if (CopyFiles_) new TRestoreFreeDOSFolderThread();
    return result;
}

void BuildDefaultProfile()
{
    TGame *DefaultGame = new TGame(PrgSetup);
    DefaultGame->ResetToDefault();
    DefaultGame->SetNrOfMounts(1);
    DefaultGame->SetMount0(MakeRelPath(PrgSetup->GameDir(), PrgSetup->BaseDir(), true) + ";Drive;C;false;");
    DefaultGame->SetEnvironment("PATH[61]Z:\\[13]");
    delete DefaultGame;
}

static void CopyTemplates(const QString &ADir1, const QString &ADir2)
{
    TSearchRec Rec;
    QString Dir1 = IncludeTrailingPathDelimiter(ADir1);
    QString Dir2 = IncludeTrailingPathDelimiter(ADir2);
    int I = FindFirst(Dir1 + "*.prof", faAnyFile, Rec);
    while (I == 0) {
        CopyFile(Dir1 + Rec.Name, Dir2 + Rec.Name, false);
        I = FindNext(Rec);
    }
    FindClose(Rec);
}

// TThread descendant (FreeOnTerminate = True, started directly)
class TReBuildTemplatesThread : public QThread
{
public:
    TReBuildTemplatesThread()
    {
        QObject::connect(this, &QThread::finished, this, &QObject::deleteLater);   // FreeOnTerminate:=True
        TemplatesInitThreadRunning = true;
        start(QThread::LowPriority);   // THREAD_PRIORITY_BELOW_NORMAL
    }
protected:
    void run() override
    {
        TemplatesInitThreadRunning = true;
        ReBuildTemplates(false);
        TemplatesInitThreadRunning = false;
    }
};

void ReBuildTemplates(bool InBackground)
{
    if (PrgDir() == PrgDataDir()) return;

    if (InBackground) {
        new TReBuildTemplatesThread();
        return;
    }

    ForceDirectories(PrgDataDir() + TemplateSubDir);
    if (DirectoryExists(PrgDir() + NewUserDataSubDir + PathDelim + TemplateSubDir))
        CopyTemplates(PrgDir() + NewUserDataSubDir + PathDelim + TemplateSubDir, PrgDataDir() + TemplateSubDir);

    ForceDirectories(PrgDataDir() + AutoSetupSubDir);
    if (DirectoryExists(PrgDir() + NewUserDataSubDir + PathDelim + AutoSetupSubDir))
        CopyTemplates(PrgDir() + NewUserDataSubDir + PathDelim + AutoSetupSubDir, PrgDataDir() + AutoSetupSubDir);
}

TStringList *GetGamesListExportLanguageColumns()
{
    TStringList *result = new TStringList;

    result->Add(LanguageSetup->GameName());
    result->Add(LanguageSetup->GameEmulationType());
    result->Add(LanguageSetup->GameGenre());
    result->Add(LanguageSetup->GameDeveloper());
    result->Add(LanguageSetup->GamePublisher());
    result->Add(LanguageSetup->GameYear());
    result->Add(LanguageSetup->GameLanguage());
    result->Add(LanguageSetup->GameWWW());
    result->Add(LanguageSetup->GameLicense());
    result->Add(LanguageSetup->GameStartCount());
    return result;
}

static TStringList *GetGamesListExportXMLColumns()
{
    TStringList *result = new TStringList;

    result->Add("Name");
    result->Add("EmulationType");
    result->Add("Genre");
    result->Add("Developer");
    result->Add("Publisher");
    result->Add("Year");
    result->Add("Language");
    result->Add("WWW");
    result->Add("License");
    result->Add("StartCount");
    return result;
}

static QString GetGamesListExportColumnValue(TGame *Game, int Column, bool WWWEncode = false)
{
    QString result, S;
    switch (Column) {
        case glecEmulationType:
            if (ScummVMMode(Game)) result = LanguageSetup->GameEmulationTypeScummVM(); else { if (WindowsExeMode(Game)) result = LanguageSetup->GameEmulationTypeWindows(); else result = LanguageSetup->GameEmulationTypeDOSBox(); }
            break;
        case glecGenre: result = GetCustomGenreName(Game->CacheGenre); break;
        case glecDeveloper: result = Game->CacheDeveloper; break;
        case glecPublisher: result = Game->CachePublisher; break;
        case glecYear: result = Game->CacheYear; break;
        case glecLanguage: result = GetCustomLanguageName(Game->CacheLanguage); break;
        case glecWWW:
            for (int I = 1; I <= 9; I++) {
                S = "";
                if (WWWEncode && (ExtUpperCase(Copy(Trim(Game->WWW(I)), 1, 4)) == "HTTP")) result = "<a href=\"" + Game->WWW(I) + "\" target=\"_blank\">" + Game->WWWName(I) + "</a>"; else result = Game->WWWName(I) + " (" + Game->WWW(I) + ")";
                if (S.isEmpty()) continue;
                if (!result.isEmpty()) result = result + "<br>";
                result = result + S;
            }
            break;
        case glecLicense: result = GetCustomLicenseName(Game->License()); break;
        case glecStartCount: result = IntToStr(History->GameStartCount(Game->CacheName)); break;
        default: result = ""; break;
    }
    if (WWWEncode && (Column != glecWWW)) result = EncodeHTMLSymbols(result);
    return result;
}

static void ExportGamesListTextFile(TGameDB *GameDB, const TGamesListExportColumns &Columns, TStringList *St)
{
    int I, J, Len;
    QString S, T;

    TStringList *ColumnNames = GetGamesListExportLanguageColumns();

    // Calculate maximum column name length
    Len = 0;
    for (I = 0; I < ColumnNames->Count(); I++) if (Columns.contains(I)) Len = Max(Len, Length((*ColumnNames)[I]));
    if (Columns.contains(glecUserFields)) for (I = 0; I < GameDB->Count(); I++) {
        TStringList *St2 = StringToStringList(GameDB->Game(I)->UserInfo());
        for (J = 0; J < St2->Count(); J++) if (Pos("=", (*St2)[J]) > 0) {
            S = Trim(Copy((*St2)[J], 1, Pos("=", (*St2)[J]) - 1));
            if (ExtUpperCase(S) == "LICENSE") continue;
            Len = Max(Len, Length(S));
        }
        delete St2;
    }

    // Make all column names have the same length
    for (I = 0; I < ColumnNames->Count(); I++) while (Length((*ColumnNames)[I]) < Len) (*ColumnNames)[I] = (*ColumnNames)[I] + " ";

    // Output list
    TList *L = GameDB->GetSortedGamesList();
    for (I = 0; I < L->size(); I++) {
        TGame *G = static_cast<TGame *>((*L)[I]);
        if (I != 0) St->Add("");

        St->Add((*ColumnNames)[0] + " : " + G->CacheName);

        for (J = 1; J < ColumnNames->Count(); J++) if (Columns.contains(J)) {
            S = (*ColumnNames)[J] + " : " + GetGamesListExportColumnValue(G, J);
            St->Add(S);
        }

        if (Columns.contains(glecUserFields) && (!Trim(G->CacheUserInfo).isEmpty())) {
            TStringList *St2 = StringToStringList(G->CacheUserInfo);
            for (J = 0; J < St2->Count(); J++) if (Pos("=", (*St2)[J]) > 0) {
                S = Trim(Copy((*St2)[J], 1, Pos("=", (*St2)[J]) - 1));
                if (ExtUpperCase(S) == "LICENSE") continue;
                while (Length(S) < Len) S = S + " ";
                St->Add(S + " : " + Copy((*St2)[J], Pos("=", (*St2)[J]) + 1, MaxInt));
            }
            delete St2;
        }

        if (Columns.contains(glecNotes) && (!Trim(G->Notes()).isEmpty())) {
            TStringList *St2 = StringToStringList(Trim(G->Notes()));
            if (St2->Count() == 1) {
                T = LanguageSetup->GameNotes();
                while (Length(T) < Len) T = T + " ";
                St->Add(T + " : " + (*St2)[0]);
            } else {
                St->Add(LanguageSetup->GameNotes() + ":");
                St->AddStrings(*St2);
            }
            delete St2;
        }
    }
    delete L;
    delete ColumnNames;
}

static QString EncodeCSV(QString S)
{
    QString result;
    int I = Pos("\"", S);
    while (I > 0) {
        result = result + Copy(S, 1, I - 1) + "\"\"";
        S = Copy(S, I + 1, MaxInt);
        I = Pos("\"", S);
    }
    result = result + S;
    return result;
}

static void ExportGamesListTableFile(TGameDB *GameDB, const TGamesListExportColumns &Columns, TStringList *St)
{
    int I, J, K;
    QString S, T;

    TStringList *ColumnNames = GetGamesListExportLanguageColumns();
    TStringList UserList, UserListUpper;

    // Output heading line
    S = "\"" + EncodeCSV((*ColumnNames)[0]) + "\"";
    for (I = 1; I < ColumnNames->Count(); I++) if (Columns.contains(I)) S = S + ";\"" + EncodeCSV((*ColumnNames)[I]) + "\"";
    if (Columns.contains(glecUserFields)) {
        for (I = 0; I < GameDB->Count(); I++) {
            TStringList *St2 = StringToStringList(GameDB->Game(I)->CacheUserInfo);
            for (J = 0; J < St2->Count(); J++) if (Pos("=", (*St2)[J]) > 0) {
                T = Trim(Copy((*St2)[J], 1, Pos("=", (*St2)[J]) - 1));
                if (ExtUpperCase(T) == "LICENSE") continue;
                if (UserListUpper.IndexOf(ExtUpperCase(T)) < 0) { UserList.Add(T); UserListUpper.Add(ExtUpperCase(T)); }
            }
            delete St2;
        }
        for (I = 0; I < UserList.Count(); I++) S = S + ";\"" + EncodeCSV(UserList[I]) + "\"";
    }
    if (Columns.contains(glecNotes)) S = S + ";\"" + EncodeCSV(LanguageSetup->GameNotes()) + "\"";
    St->Add(S);

    // Output list
    TList *L = GameDB->GetSortedGamesList();
    for (I = 0; I < L->size(); I++) {
        TGame *G = static_cast<TGame *>((*L)[I]);
        S = "\"" + EncodeCSV(G->CacheName) + "\"";

        for (J = 1; J < ColumnNames->Count(); J++) if (Columns.contains(J))
            S = S + ";\"" + EncodeCSV(GetGamesListExportColumnValue(G, J)) + "\"";

        if (Columns.contains(glecUserFields)) {
            TStringList *St2 = StringToStringList(G->CacheUserInfo);
            for (J = 0; J < UserList.Count(); J++) {
                T = "";
                for (K = 0; K < St2->Count(); K++) if (Pos("=", (*St2)[K]) > 0) {
                    if (Trim(ExtUpperCase(Copy((*St2)[K], 1, Pos("=", (*St2)[K]) - 1))) == UserListUpper[J]) {
                        T = Trim(Copy((*St2)[K], Pos("=", (*St2)[K]) + 1, MaxInt)); break;
                    }
                }
                S = S + ";\"" + EncodeCSV(T) + "\"";
            }
            delete St2;
        }

        if (Columns.contains(glecNotes)) {
            S = S + ";\"";
            TStringList *St2 = StringToStringList(G->Notes());
            for (J = 0; J < St2->Count(); J++) {
                if (J > 0) S = S + " ";
                S = S + EncodeCSV((*St2)[J]);
            }
            delete St2;
            S = S + "\"";
        }

        St->Add(S);
    }
    delete L;
    delete ColumnNames;
}

QString EncodeUserHTMLSymbolsOnly(const QString &S)
{
    QString result = S;

    TStringList *St = ValueToList(LanguageSetup->CharsetHTMLTranslate(), ",");
    if (St->Count() % 2 == 1) St->Add((*St)[St->Count() - 1]);
    for (int I = 0; I < St->Count() / 2; I++) {
        QString A = (*St)[2 * I]; if (Length(A) == 0) continue;
        QString B = (*St)[2 * I + 1]; if (!B.isEmpty() && B[Length(B) - 1] != QChar(';')) B = B + ";";
        if (A[0] == QChar('&') || ExtUpperCase(B) == "&AMP;") continue;
        result = Replace(result, QString(A[0]), B);
    }
    delete St;
    return result;
}

QString EncodeHTMLSymbols(const QString &S)
{
    QString result = S;
    result = Replace(result, "&", "&amp;"); // must be the first, otherweise "ä" -> "&auml;" -> "&amp;auml;" will happen
    result = Replace(result, "<", "&lt;");
    result = Replace(result, ">", "&gt;");
    result = Replace(result, "\"", "&quot;");
    // result:=Replace(result,'''','&apos;'); only valid in XHTML
    result = EncodeUserHTMLSymbolsOnly(result);
    return result;
}

static bool TryStrToHex(const QString &S, int &Hex)
{
    Hex = 0;
    for (int I = 0; I < S.length(); I++) {
        Hex = Hex * 16;
        const QChar C = S[I].toUpper();
        if (C >= QChar('0') && C <= QChar('9')) Hex = Hex + (C.unicode() - '0');
        else if (C >= QChar('A') && C <= QChar('F')) Hex = Hex + (C.unicode() - 'A' + 10);
        else return false;
    }
    return true;
}

QString DecodeHTMLSymbols(const QString &S)
{
    int I, J, K;
    QString result = S;
    result = Replace(result, "&amp;", "&");
    result = Replace(result, "&lt;", "<");
    result = Replace(result, "&gt;", ">");
    result = Replace(result, "&quot;", "\"");
    result = Replace(result, "&apos;", "'");

    // 1-based indices like the Delphi code: result[X] -> result[X-1]
    auto Ch = [&result](int X) -> QChar { return (X >= 1 && X <= result.length()) ? result[X - 1] : QChar(); };
    I = 1;
    while (I < Length(result) - 1) {
        if (Ch(I) == QChar('&') && Ch(I + 1) == QChar('#')) {
            if ((I < Length(result) - 2) && Ch(I + 2) == QChar('x')) {
                // Hex number
                J = 0; while ((I + 3 + J < Length(result)) && (Ch(I + 3 + J) != QChar(';'))) J++;
                if ((J >= 1) && (J <= 3) && TryStrToHex(Copy(result, I + 3, J), K)) result = Copy(result, 1, I - 1) + QChar(K) + Copy(result, I + 3 + J);
            } else {
                // Decimal number
                J = 0; while ((I + 2 + J < Length(result)) && (Ch(I + 2 + J) != QChar(';'))) J++;
                if ((J >= 1) && (J <= 2) && TryStrToInt(Copy(result, I + 3, J), K)) result = Copy(result, 1, I - 1) + QChar(K) + Copy(result, I + 2 + J);
            }
        }
        I++;
    }

    TStringList *St = ValueToList(LanguageSetup->CharsetHTMLTranslate(), ",");
    if (St->Count() % 2 == 1) St->Add((*St)[St->Count() - 1]);
    for (I = 0; I < St->Count() / 2; I++) {
        QString A = (*St)[2 * I]; if (Length(A) == 0) continue;
        QString B = (*St)[2 * I + 1]; if (!B.isEmpty() && B[Length(B) - 1] != QChar(';')) B = B + ";";
        if (A[0] == QChar('&') || ExtUpperCase(B) == "&AMP;") continue;
        result = Replace(result, B, QString(A[0]));
    }
    delete St;
    return result;
}

static void GetUserLists(TGameDB *GameDB, TStringList *&UserList, TStringList *&UserListUpper)
{
    UserList = new TStringList;
    UserListUpper = new TStringList;

    for (int I = 0; I < GameDB->Count(); I++) {
        TStringList *St2 = StringToStringList(GameDB->Game(I)->UserInfo());
        for (int J = 0; J < St2->Count(); J++) if (Pos("=", (*St2)[J]) > 0) {
            QString S = Trim(Copy((*St2)[J], 1, Pos("=", (*St2)[J]) - 1));
            if (ExtUpperCase(S) == "LICENSE") continue;
            if (UserListUpper->IndexOf(ExtUpperCase(S)) < 0) { UserList->Add(S); UserListUpper->Add(ExtUpperCase(S)); }
        }
        delete St2;
    }
}

static void ExportGamesListHTMLFile(TGameDB *GameDB, const TGamesListExportColumns &Columns, TStringList *St)
{
    int I, J, K;
    QString S, T;
    TStringList *UserList, *UserListUpper;

    TStringList *ColumnNames = GetGamesListExportLanguageColumns();
    GetUserLists(GameDB, UserList, UserListUpper);

    St->Add(QStringLiteral("<!DOCTYPE HTML PUBLIC \"-/") + "/W3C/" + "/DTD HTML 4.01 Transitional/" + "/EN\">");
    St->Add("<html>");
    St->Add("  <head>");
    St->Add("    <title>D-Fend Reloaded</title>");
    St->Add("    <meta http-equiv=\"content-type\" content=\"text/html; charset=" + LanguageSetup->CharsetHTMLCharset() + "\">");
    St->Add("    <meta name=\"description\" content=\"D-Fend Reloaded games list\">");
    St->Add("    <meta name=\"DC.creator\" content=\"D-Fend Reloaded " + GetNormalFileVersionAsString() + "\">");
    St->Add("    <style type=\"text/css\">");
    St->Add("    <!--");
    St->Add("      body {font-family: Arial, Helvetica, sans-serif;}");
    St->Add("      table {border-collapse: collapse;}");
    St->Add("      td,th {border: solid 1px black; padding: 2px 4px 2px 4px;}");
    St->Add("    -->");
    St->Add("    </style>");
    St->Add("  </head>");
    St->Add("  <body>");
    St->Add("    <h1>" + EncodeHTMLSymbols(LanguageSetup->MenuFileExportGamesListHTMLTitle()) + "</h1>");
    St->Add("    <table>");
    St->Add("      <tr>");
    for (I = 0; I < ColumnNames->Count(); I++) if ((I == 0) || Columns.contains(I)) St->Add(Format("        <th>%s</th>", {EncodeHTMLSymbols((*ColumnNames)[I])}));
    if (Columns.contains(glecUserFields)) for (I = 0; I < UserList->Count(); I++) St->Add(Format("        <th>%s</th>", {EncodeHTMLSymbols((*UserList)[I])}));
    if (Columns.contains(glecNotes)) St->Add(Format("        <th>%s</th>", {EncodeHTMLSymbols(LanguageSetup->GameNotes())}));
    St->Add("      </tr>");

    TList *L = GameDB->GetSortedGamesList();
    for (I = 0; I < L->size(); I++) {
        TGame *G = static_cast<TGame *>((*L)[I]);
        St->Add("      <tr>");
        St->Add(Format("        <td>%s</td>", {EncodeHTMLSymbols(G->CacheName)}));

        for (J = 1; J < ColumnNames->Count(); J++) if (Columns.contains(J))
            St->Add(Format("        <td>%s</td>", {GetGamesListExportColumnValue(G, J, true)}));

        if (Columns.contains(glecUserFields)) {
            TStringList *St2 = StringToStringList(G->CacheUserInfo);
            for (J = 0; J < UserList->Count(); J++) {
                S = "";
                for (K = 0; K < St2->Count(); K++) if (Pos("=", (*St2)[K]) > 0) {
                    if (Trim(ExtUpperCase(Copy((*St2)[K], 1, Pos("=", (*St2)[K]) - 1))) == (*UserListUpper)[J]) {
                        S = Trim(Copy((*St2)[K], Pos("=", (*St2)[K]) + 1, MaxInt)); break;
                    }
                }
                St->Add(Format("        <td>%s</td>", {EncodeHTMLSymbols(S)}));
            }
            delete St2;
        }

        if (Columns.contains(glecNotes)) {
            T = "        <td>";
            TStringList *St2 = StringToStringList(G->Notes());
            for (J = 0; J < St2->Count(); J++) {
                if (J > 0) T = T + "<br>";
                T = T + EncodeHTMLSymbols((*St2)[J]);
            }
            delete St2;
            St->Add(T + "</td>");
        }

        St->Add("      </tr>");
    }
    delete L;
    St->Add("    </table>");
    St->Add("  </body>");
    St->Add("</html>");

    delete UserList;
    delete UserListUpper;
    delete ColumnNames;
}

static void ExportGamesListXMLFile(TGameDB *GameDB, const TGamesListExportColumns &Columns, TStringList *St)
{
    int I, J, K;
    QString S, T;
    TStringList *UserList, *UserListUpper;
    int Year, Month, Day;

    TStringList *ColumnNames = GetGamesListExportXMLColumns();
    GetUserLists(GameDB, UserList, UserListUpper);

    St->Add("<?xml version=\"1.0\" encoding=\"" + LanguageSetup->CharsetHTMLCharset() + "\" standalone=\"no\"?>");
    St->Add(QStringLiteral("<!DOCTYPE GamesList SYSTEM \"http:/") + "/dfendreloaded.sourceforge.net/GamesListExport/GamesListExport.dtd\">");
    St->Add("");
    DecodeDate(Date(), Year, Month, Day);
    St->Add("<GamesList DFRVersion=\"" + GetNormalFileVersionAsString() + "\" ExportDate=\"" + Format("%d-%.2d-%.2d", {Year, Month, Day}) + "\">");
    TList *L = GameDB->GetSortedGamesList();
    for (I = 0; I < L->size(); I++) {
        TGame *G = static_cast<TGame *>((*L)[I]);
        St->Add("");
        St->Add("  <Game>");

        St->Add(Format("    <Name>%s</Name>", {EncodeHTMLSymbols(G->CacheName)}));

        for (J = 1; J < ColumnNames->Count(); J++) if (Columns.contains(J)) {
            S = GetGamesListExportColumnValue(G, J, false);
            if (J != glecWWW) S = EncodeHTMLSymbols(S);
            if (!Trim(S).isEmpty()) St->Add(Format("    <%s>%s</%s>", {(*ColumnNames)[J], S, (*ColumnNames)[J]}));
        }

        if (Columns.contains(glecUserFields)) {
            TStringList *St2 = StringToStringList(G->CacheUserInfo);
            for (J = 0; J < UserList->Count(); J++) {
                S = "";
                for (K = 0; K < St2->Count(); K++) if (Pos("=", (*St2)[K]) > 0) {
                    if (Trim(ExtUpperCase(Copy((*St2)[K], 1, Pos("=", (*St2)[K]) - 1))) == (*UserListUpper)[J]) {
                        S = Trim(Copy((*St2)[K], Pos("=", (*St2)[K]) + 1, MaxInt)); break;
                    }
                }
                S = EncodeHTMLSymbols(S);
                if (!Trim(S).isEmpty()) {
                    St->Add("    <UserData>");
                    St->Add(Format("      <Name>%s</Name>", {EncodeHTMLSymbols((*UserList)[J])}));
                    St->Add(Format("      <Value>%s</Value>", {S}));
                    St->Add("    </UserData>");
                }
            }
            delete St2;
        }

        if (Columns.contains(glecNotes)) {
            T = "";
            TStringList *St2 = StringToStringList(G->Notes());
            for (J = 0; J < St2->Count(); J++) T = T + EncodeHTMLSymbols((*St2)[J]) + "\r";
            delete St2;
            if (!T.isEmpty()) St->Add(QStringLiteral("    <Notes>") + "\r" + "    <![CDATA[" + "\r" + T + "    ]]>" + "\r" + "    </Notes>");
        }

        St->Add("  </Game>");
    }

    if (L->size() > 0) St->Add("");
    delete L;
    St->Add("</GamesList>");

    delete UserList;
    delete UserListUpper;
    delete ColumnNames;
}

void ExportGamesList(TGameDB *GameDB, const QString &FileName, TGamesListExportColumns Columns, TGamesListExportFormat Format_)
{
    QString S;
    TStringList St;

    if (Format_ == glefAuto) {
        S = ExtUpperCase(Trim(ExtractFileExt(FileName)));
        if (S == ".CSV") ExportGamesListTableFile(GameDB, Columns, &St);
        if (S == ".HTML" || S == ".HTM") ExportGamesListHTMLFile(GameDB, Columns, &St);
        if (S == ".XML") ExportGamesListXMLFile(GameDB, Columns, &St);
        if (St.Count() == 0) ExportGamesListTextFile(GameDB, Columns, &St);
    }
    if (Format_ == glefText) ExportGamesListTextFile(GameDB, Columns, &St);
    if (Format_ == glefTable) ExportGamesListTableFile(GameDB, Columns, &St);
    if (Format_ == glefHTML) ExportGamesListHTMLFile(GameDB, Columns, &St);
    if (Format_ == glefXML) ExportGamesListXMLFile(GameDB, Columns, &St);
    if (!St.SaveToFile(FileName)) {
        MessageDlg(LanguageSetup->MessageCouldNotSaveFile(), mtError, mbOK, 0);
    }
}

void EditDefaultProfile(QWidget *AOwner, TGameDB *GameDB, TLinkFile *ASearchLinkFile, TStringList *ADeleteOnExit)
{
    TGame *DefaultGame = new TGame(PrgSetup);
    TList *L = new TList;
    TList *L2 = new TList;
    L->append(DefaultGame);
    L2->append(reinterpret_cast<void *>(intptr_t(1)));
    TGame *G = DefaultGame;
    if (PrgSetup->DFendStyleProfileEditor()) {
        EditGameTemplate(AOwner, GameDB, G, nullptr, ASearchLinkFile, ADeleteOnExit, L, L2);
    } else {
        ModernEditGameTemplate(AOwner, GameDB, G, nullptr, ASearchLinkFile, ADeleteOnExit, L, L2);
    }
    delete DefaultGame;
    delete L;
    delete L2;
}

static TStringList *GetAllConfFiles(const QString &Dir)
{
    TSearchRec Rec;
    TStringList *result = new TStringList;

    int I = FindFirst(IncludeTrailingPathDelimiter(Dir) + "*.conf", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == 0) result->Add(ExtUpperCase(Rec.Name));
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

static bool NeedUpdate(const QString &ProfFile, const QString &ConfFile)
{
    if (!FileExists(ConfFile)) return true;
    TDateTime D1 = GetFileDate(ProfFile), D2 = GetFileDate(ConfFile);
    return (D1 < 0.001) || (D2 < 0.001) || (D2 < D1 - 5.0 / 86400);
}

void CreateConfFilesForProfiles(TGameDB *GameDB, const QString &Dir)
{
    QString S, T;

    TStringList *St = GetAllConfFiles(Dir);
    for (int I = 0; I < GameDB->Count(); I++) {
        if (!DOSBoxMode(GameDB->Game(I))) continue;
        S = GameDB->Game(I)->SetupFile(); T = ChangeFileExt(S, ".conf");
        if (NeedUpdate(S, T)) {
            TStringList *St2 = BuildConfFile(GameDB->Game(I), false, false, -1, nullptr, false);
            if (St2 != nullptr) { St2->SaveToFile(T); delete St2; }
        }
        int J = St->IndexOf(ExtUpperCase(ExtractFileName(T))); if (J >= 0) St->Delete(J);
    }

    S = IncludeTrailingPathDelimiter(Dir);
    for (int I = 0; I < St->Count(); I++) DeleteFile(S + (*St)[I]);
    delete St;
}

static bool StrToBool(const QString &AS)
{
    QString S = Trim(ExtUpperCase(AS));
    return (S == "TRUE") || (S == "T") || (S == "1");
}

static void AddDrive(TGame *Game, const QString &RealFolder, const QString &Letter, const QString &FreeSpace)
{
    // RealFolder;DRIVE;Letter;False;;FreeSpace
    QString S = MakeRelPath(RealFolder, PrgSetup->BaseDir(), true) + ";Drive;" + Letter + ";;" + FreeSpace;

    SetGameMount(Game, Game->NrOfMounts(), S);
    Game->SetNrOfMounts(Game->NrOfMounts() + 1);
}

static void AddCDImageDrive(TGame *Game, const QString &RealFolder, const QString &Letter)
{
    // ImageFile;CDROMIMAGE;Letter;;;
    QString S = MakeRelPath(RealFolder, PrgSetup->BaseDir(), true) + ";CDRomImage;" + Letter + ";;";

    SetGameMount(Game, Game->NrOfMounts(), S);
    Game->SetNrOfMounts(Game->NrOfMounts() + 1);
}

static bool GetFolderAndDriveLetter(QString &Params, QString &Letter, QString &Folder)
{
    int J, K;

    J = Pos(" ", Params); if (J == 0) return false;
    Letter = Trim(Copy(Params, 1, J - 1)); Params = Trim(Copy(Params, J + 1, MaxInt));

    if (Length(Letter) == 3) { if (Letter[1] != QChar(':') || Letter[2] != QChar('\\')) return false; Letter = QString(Letter[0]); }
    if (Length(Letter) == 2) { if (Letter[1] != QChar(':')) return false; Letter = QString(Letter[0]); }
    Letter = ExtUpperCase(Letter);
    if (Letter < "A" || Letter > "Z") return false;

    if (!Params.isEmpty() && Params[0] == QChar('"')) {
        Params = Trim(Copy(Params, 2, MaxInt));
        K = -1;
        for (J = 1; J <= Length(Params); J++) if (Params[J - 1] == QChar('"')) { K = J; break; }
        if (K == -1) { Folder = ""; Params = "\"" + Params; } else { Folder = Trim(Copy(Params, 1, K - 1)); Params = Trim(Copy(Params, K + 1, MaxInt)); }
    } else {
        J = Pos(" ", Params); if (J > 0) { Folder = Trim(Copy(Params, 1, J - 1)); Params = Trim(Copy(Params, J + 1, MaxInt)); } else { Folder = Params; Params = ""; }
    }

    return true;
}

static void MakeMountsFromAutoexec(TGame *Game, TStringList *Autoexec)
{
    QString S, SUpper, Letter, Folder;
    int I = 0;
    while (I < Autoexec->Count()) {
        if (Game->NrOfMounts() == 10) return;

        S = Trim((*Autoexec)[I]);
        if (!S.isEmpty() && S[0] == QChar('@')) S = Trim(Copy(S, 2, MaxInt));
        SUpper = ExtUpperCase(S);
        if (S.isEmpty() || (Copy(SUpper, 1, 4) == "ECHO") || (Copy(SUpper, 1, 3) == "REM") || (Copy(SUpper, 1, 4) == "SET ") || (Copy(SUpper, 1, 5) == "KEYB ")) { I++; continue; }

        if (Copy(SUpper, 1, 6) == "MOUNT ") {
            S = Trim(Copy(S, 7, MaxInt));
            if (!GetFolderAndDriveLetter(S, Letter, Folder)) { I++; continue; }
            if (Trim(ExtUpperCase(Copy(S, 1, 9))) == "-FREESIZE") S = Trim(Copy(S, 10, MaxInt)); else S = "";
            AddDrive(Game, Folder, Letter, S);
            Autoexec->Delete(I);
            continue;
        }

        if (Copy(SUpper, 1, 9) == "IMGMOUNT ") {
            S = Trim(Copy(S, 10, MaxInt));
            if (!GetFolderAndDriveLetter(S, Letter, Folder)) { I++; continue; }
            S = Trim(ExtUpperCase(S));
            if ((S != "-T ISO -FS ISO") && (S != "-FS ISO -T ISO")) { I++; continue; }
            AddCDImageDrive(Game, Folder, Letter);
            Autoexec->Delete(I);
            continue;
        }

        I++;
    }
}

static void LoadSpecialData(TGame *Game, const QString &FileName)
{
    QString S, T, U;
    TStringList St, Autoexec;

    St.LoadFromFile(FileName);
    int Sec = -1;
    for (int I = 0; I < St.Count(); I++) {
        S = Trim(ExtUpperCase(St[I]));
        if (!S.isEmpty() && S[0] == QChar('[') && Pos("]", S) > 0) {
            S = Trim(Copy(S, 2, Pos("]", S) - 2));
            if (S == "GENERAL") Sec = 0; else {
                if (S == "AUTOEXEC") Sec = 1; else Sec = -1;
            }
            continue;
        }

        if (Sec == 0) {
            S = Trim(ExtUpperCase(St[I]));
            if (S.isEmpty() || S[0] != QChar('#') || Pos("=", S) == 0) continue;
            S = Trim(Copy(Trim(St[I]), 2, MaxInt));
            T = Trim(ExtUpperCase(Copy(S, 1, Pos("=", S) - 1)));
            U = Trim(Copy(S, Pos("=", S) + 1, MaxInt));

            if (T == "SNAPSHOTIMAGE") Game->SetCaptureFolder(U);
            if (T == "EXIT") Game->SetCloseDosBoxAfterGameExit(StrToBool(U));
            if (T == "MANUALFILE") Game->SetDataDir(ExtractFilePath(U));
            if (T == "WWWSITE") Game->SetWWW(1, U);
        }

        if (Sec == 1) {
            T = ExtUpperCase(St[I]);
            if (T == "EXIT") { Game->SetCloseDosBoxAfterGameExit(true); continue; }
            if (Copy(Trim(T), 1, 1) == "#") continue;
            Autoexec.Add(St[I]);
        }
    }

    MakeMountsFromAutoexec(Game, &Autoexec);

    while ((Autoexec.Count() > 0) && Trim(Autoexec[0]).isEmpty()) Autoexec.Delete(0);

    Game->SetAutoexec(StringListToString(&Autoexec));
}

void ImportConfData(TGameDB *AGameDB, TGame *AGame, const QString &Lines)
{
    TStringList St;
    St.SetText(Lines);
    ImportConfData(AGameDB, AGame, &St);
}

void ImportConfData(TGameDB *AGameDB, TGame *AGame, TStringList *St)
{
    QString FileName = TempDir() + "DFRTempConfFile.conf";
    St->SaveToFile(FileName);
    try {
        ImportConfFileData(AGameDB, AGame, FileName);
    } catch (...) {
        ExtDeleteFile(FileName, ftProfile);
        throw;
    }
    ExtDeleteFile(FileName, ftProfile);
}

void ImportConfFileData(TGameDB *AGameDB, TGame *AGame, const QString &AFileName)
{
    bool B;
    int I;
    QString S;

    {
        TIniFile INI(AFileName);
        AGame->SetStartFullscreen(StrToBool(INI.ReadString("sdl", "fullscreen", "false")));
        AGame->SetUseDoublebuffering(StrToBool(INI.ReadString("sdl", "fulldouble", "false")));
        AGame->SetRender(INI.ReadString("sdl", "output", "surface"));
        AGame->SetFullscreenResolution(INI.ReadString("sdl", "fullresolution", "original"));
        AGame->SetWindowResolution(INI.ReadString("sdl", "windowresolution", "original"));
        AGame->SetAutoLockMouse(StrToBool(INI.ReadString("sdl", "autolock", "true")));
        AGame->SetMouseSensitivity(INI.ReadInteger("sdl", "sensitivity", 100));
        AGame->SetPriority(INI.ReadString("sdl", "priority", "higher,normal"));
        AGame->SetUseScanCodes(StrToBool(INI.ReadString("sdl", "usescancodes", "true")));
        AGame->SetCustomKeyMappingFile(INI.ReadString("sdl", "mapperfile", ""));
        if (!Trim(AGame->CustomKeyMappingFile()).isEmpty()) AGame->SetCustomKeyMappingFile(MakeRelPath(AGame->CustomKeyMappingFile(), PrgSetup->BaseDir()));

        AGame->SetVideoCard(INI.ReadString("dosbox", "machine", "vga"));
        AGame->SetMemory(INI.ReadInteger("dosbox", "memsize", 16));
        AGame->SetCaptureFolder(MakeRelPath(INI.ReadString("dosbox", "captures", IncludeTrailingPathDelimiter(PrgSetup->CaptureDir())), PrgSetup->BaseDir(), true));

        AGame->SetFrameSkip(INI.ReadInteger("render", "frameskip", 0));
        AGame->SetAspectCorrection(StrToBool(INI.ReadString("render", "aspect", "false")));
        AGame->SetScale(INI.ReadString("render", "scaler", "normal2x"));

        AGame->SetCore(INI.ReadString("cpu", "core", "auto"));
        AGame->SetCycles(INI.ReadString("cpu", "cycles", "auto"));

        B = TryStrToInt(AGame->Cycles(), I);
        if (!B) {
            S = Trim(ExtUpperCase(AGame->Cycles()));
            TStringList *St = ValueToList(AGameDB->ConfOpt()->Cycles(), ";,");
            for (I = 0; I < St->Count(); I++) if (Trim(ExtUpperCase((*St)[I])) == S) { B = true; break; }
            delete St;
        }
        if (!B) AGame->SetCycles("auto");

        AGame->SetCyclesUp(INI.ReadInteger("cpu", "cycleup", 500));
        AGame->SetCyclesDown(INI.ReadInteger("cpu", "cycledown", 20));

        AGame->SetEMS(StrToBool(INI.ReadString("dos", "ems", "true")));
        AGame->SetXMS(StrToBool(INI.ReadString("dos", "xms", "true")));
        AGame->SetUMB(StrToBool(INI.ReadString("dos", "umb", "true")));
        AGame->SetKeyboardLayout(INI.ReadString("dos", "keyboardlayout", "none"));
        if (ExtUpperCase(AGame->KeyboardLayout()) == "AUTO") AGame->SetKeyboardLayout("default");

        AGame->SetMixerNosound(StrToBool(INI.ReadString("mixer", "nosound", "false")));
        AGame->SetMixerRate(INI.ReadInteger("mixer", "rate", 22050));
        AGame->SetMixerBlocksize(INI.ReadInteger("mixer", "blocksize", 2048));
        AGame->SetMixerPrebuffer(INI.ReadInteger("mixer", "prebuffer", 10));

        AGame->SetSBType(INI.ReadString("sblaster", "sbtype", "sb16"));
        AGame->SetSBBase(INI.ReadString("sblaster", "sbbase", "220"));
        AGame->SetSBIRQ(INI.ReadInteger("sblaster", "irq", 7));
        AGame->SetSBDMA(INI.ReadInteger("sblaster", "dma", 1));
        AGame->SetSBHDMA(INI.ReadInteger("sblaster", "hdma", 5));
        AGame->SetSBMixer(StrToBool(INI.ReadString("mixer", "mixer", "true")));
        AGame->SetSBOplMode(INI.ReadString("sblaster", "oplmode", "auto"));
        AGame->SetSBOplRate(INI.ReadInteger("sblaster", "oplrate", 22050));

        AGame->SetGUS(StrToBool(INI.ReadString("gus", "gus", "true")));
        AGame->SetGUSRate(INI.ReadInteger("gus", "gusrate", 22050));
        AGame->SetGUSBase(INI.ReadString("gus", "gusbase", "240"));
        AGame->SetGUSIRQ(INI.ReadInteger("gus", "irg1", 5));
        AGame->SetGUSDMA(INI.ReadInteger("gus", "dma1", 3));
        AGame->SetGUSUltraDir(INI.ReadString("gus", "ultradir", "C:\\ULTRASND"));

        AGame->SetMIDIType(INI.ReadString("midi", "mpu401", "intelligent"));
        AGame->SetMIDIDevice(INI.ReadString("midi", "device", "default"));
        AGame->SetMIDIConfig(INI.ReadString("midi", "config", ""));

        AGame->SetSpeakerPC(StrToBool(INI.ReadString("speaker", "pcspeaker", "true")));
        AGame->SetSpeakerRate(INI.ReadInteger("speaker", "pcrate", 22050));
        AGame->SetSpeakerTandy(INI.ReadString("speaker", "tandy", "auto"));
        AGame->SetSpeakerRate(INI.ReadInteger("speaker", "tandyrate", 22050));
        AGame->SetSpeakerDisney(StrToBool(INI.ReadString("speaker", "disney", "true")));

        AGame->SetJoystickType(INI.ReadString("joystick", "joysticktype", "auto"));
        AGame->SetJoystickTimed(StrToBool(INI.ReadString("joystick", "timed", "true")));
        AGame->SetJoystickAutoFire(StrToBool(INI.ReadString("joystick", "autofire", "false")));
        AGame->SetJoystickSwap34(StrToBool(INI.ReadString("joystick", "swap34", "false")));
        AGame->SetJoystickButtonwrap(StrToBool(INI.ReadString("joystick", "buttonwrap", "true")));

        AGame->SetSerial1(INI.ReadString("serial", "serial1", "dummy"));
        AGame->SetSerial2(INI.ReadString("serial", "serial2", "dummy"));
        AGame->SetSerial3(INI.ReadString("serial", "serial3", "disabled"));
        AGame->SetSerial4(INI.ReadString("serial", "serial4", "disabled"));

        AGame->SetIPX(StrToBool(INI.ReadString("ipx", "ipx", "false")));
    }
    LoadSpecialData(AGame, AFileName);
}

TGame *ImportConfFile(TGameDB *AGameDB, const QString &AFileName)
{
    TGame *result = AGameDB->Game(AGameDB->Add(ChangeFileExt(ExtractFileName(AGameDB->ProfFileName(ExtractFileName(AFileName), true)), "")));
    ImportConfFileData(AGameDB, result, AFileName);
    return result;
}

static bool GameCheckSumOK(TGame *AGame)
{
    if (AGame->GameExeMD5().isEmpty() || (Trim(ExtUpperCase(AGame->GameExeMD5())) == "OFF")) return true;
    if (AGame->GameExe().isEmpty()) return true;
    QString T = MakeAbsPath(AGame->GameExe(), PrgSetup->BaseDir());
    if (!FileExists(T)) return true;
    QString S = GetMD5Sum(T, false); if (S.isEmpty()) return true;
    return (S == AGame->GameExeMD5());
}

static bool SetupCheckSumOK(TGame *AGame)
{
    if (AGame->SetupExeMD5().isEmpty() || (Trim(ExtUpperCase(AGame->SetupExeMD5())) == "OFF")) return true;
    if (AGame->SetupExe().isEmpty()) return true;
    QString T = MakeAbsPath(AGame->SetupExe(), PrgSetup->BaseDir());
    if (!FileExists(T)) return true;
    QString S = GetMD5Sum(T, false); if (S.isEmpty()) return true;
    return (S == AGame->SetupExeMD5());
}

void CreateGameCheckSum(TGame *AGame, bool OverwriteExistingCheckSum)
{
    if (!AGame->GameExeMD5().isEmpty() && (!OverwriteExistingCheckSum)) return;
    if (Trim(ExtUpperCase(AGame->GameExeMD5())) == "OFF") return;
    if (AGame->GameExe().isEmpty()) return;
    QString S = MakeAbsPath(AGame->GameExe(), PrgSetup->BaseDir());
    if (!FileExists(S)) return;
    AGame->SetGameExeMD5(GetMD5Sum(S, false));
}

void CreateSetupCheckSum(TGame *AGame, bool OverwriteExistingCheckSum)
{
    if (!AGame->SetupExeMD5().isEmpty() && (!OverwriteExistingCheckSum)) return;
    if (Trim(ExtUpperCase(AGame->SetupExeMD5())) == "OFF") return;
    if (AGame->SetupExe().isEmpty()) return;
    QString S = MakeAbsPath(AGame->SetupExe(), PrgSetup->BaseDir());
    if (!FileExists(S)) return;
    AGame->SetSetupExeMD5(GetMD5Sum(S, false));
}

bool ChecksumForGameOK(TGame *AGame)
{
    if (!PrgSetup->UseCheckSumsForProfiles()) return true;
    return GameCheckSumOK(AGame);
}

bool ChecksumForSetupOK(TGame *AGame)
{
    if (!PrgSetup->UseCheckSumsForProfiles()) return true;
    return SetupCheckSumOK(AGame);
}

void ProfileEditorOpenCheck(TGame *AGame)
{
    QString S;

    if (AGame == nullptr || (!PrgSetup->UseCheckSumsForProfiles())) return;

    if (!GameCheckSumOK(AGame)) {
        TStringList *St = StringToStringList(LanguageSetup->CheckSumProfileEditorMismatch()); S = St->Text(); delete St;
        S = Format(S, {AGame->GameExe()});
        switch (ShowCheckSumWarningDialog(MainFormWidget(), S)) {
            case cdYes: CreateGameCheckSum(AGame, true); break;
            case cdNo: break;
            case cdTurnOff: AGame->SetGameExeMD5("Off"); AGame->SetSetupExeMD5("Off"); break;
        }
    }
    if (!SetupCheckSumOK(AGame)) {
        TStringList *St = StringToStringList(LanguageSetup->CheckSumProfileEditorMismatch()); S = St->Text(); delete St;
        S = Format(S, {AGame->SetupExe()});
        switch (ShowCheckSumWarningDialog(MainFormWidget(), S)) {
            case cdYes: CreateSetupCheckSum(AGame, true); break;
            case cdNo: break;
            case cdTurnOff: AGame->SetGameExeMD5("Off"); AGame->SetSetupExeMD5("Off"); break;
        }
    }
}

void ProfileEditorCloseCheck(TGame *AGame, const QString &NewGameExe, const QString &NewSetupExe)
{
    QString NewGameFileName = Trim(ExtUpperCase(ExtractFileName(NewGameExe)));
    QString NewSetupFileName = Trim(ExtUpperCase(ExtractFileName(NewSetupExe)));
    bool B1 = Trim(ExtUpperCase(ExtractFileName(AGame->GameExe()))) != NewGameFileName;
    bool B2 = Trim(ExtUpperCase(ExtractFileName(AGame->SetupExe()))) != NewSetupFileName;
    AGame->SetGameExe(NewGameExe);
    AGame->SetSetupExe(NewSetupFileName);
    CreateGameCheckSum(AGame, B1);
    CreateSetupCheckSum(AGame, B2);
}

bool RunCheck(TGame *AGame, bool RunSetup, int RunExtraFile)
{
    QString S, T;
    bool result = true;
    if (!PrgSetup->UseCheckSumsForProfiles()) return result;

    if (RunExtraFile >= 0) {
        return result;
    } else {
        if (RunSetup) {
            if (SetupCheckSumOK(AGame)) return result;
            S = MakeAbsPath(AGame->SetupExe(), PrgSetup->BaseDir());
        } else {
            if (GameCheckSumOK(AGame)) return result;
            S = MakeAbsPath(AGame->GameExe(), PrgSetup->BaseDir());
        }
    }

    TStringList *St = StringToStringList(LanguageSetup->CheckSumRunMismatch()); T = St->Text(); delete St;
    S = Format(T, {S});
    switch (ShowCheckSumWarningDialog(MainFormWidget(), S)) {
        case cdYes:
            result = true;
            if (RunSetup) CreateSetupCheckSum(AGame, true); else CreateGameCheckSum(AGame, true);
            break;
        case cdNo: result = false; break;
        case cdTurnOff:
            result = true;
            AGame->SetGameExeMD5("Off"); AGame->SetSetupExeMD5("Off");
            break;
    }
    return result;
}

void CreateCheckSumsForAllGames(TGameDB *AGameDB)
{
    InitProgressWindow(MainFormWidget(), AGameDB->Count());
    for (int I = 0; I < AGameDB->Count(); I++) {
        CreateGameCheckSum(AGameDB->Game(I), false);
        CreateSetupCheckSum(AGameDB->Game(I), false);
        StepProgressWindow();
    }
    DoneProgressWindow();
}

static QString CreateUniqueFolder(const QString &GameName, const QString &ShortFolderName, const QString &BaseDir)
{
    QString S;
    int I;

    if (!Trim(ShortFolderName).isEmpty()) {
        S = MakeAbsPath(ShortFolderName, PrgSetup->BaseDir());
        if (!DirectoryExists(S)) ForceDirectories(S);
        return QString();
    }

    S = MakeAbsPath(IncludeTrailingPathDelimiter(BaseDir) + MakeFileSysOKFolderName(GameName) + PathDelim, PrgSetup->BaseDir());
    I = 0;
    while (DirectoryExists(S)) {
        I++;
        S = MakeRelPath(IncludeTrailingPathDelimiter(BaseDir) + MakeFileSysOKFolderName(GameName) + IntToStr(I) + PathDelim, PrgSetup->BaseDir());
    }
    ForceDirectories(S);
    return MakeRelPath(S, PrgSetup->BaseDir());
}

void CreateFoldersForAllGames(TGameDB *AGameDB, TFolderType FolderType)
{
    QString S, T;
    InitProgressWindow(MainFormWidget(), AGameDB->Count());
    if (FolderType == ftCapture) T = PrgSetup->CaptureDir(); else T = PrgSetup->DataDir();
    for (int I = 0; I < AGameDB->Count(); I++) {
        if (FolderType == ftCapture) S = AGameDB->Game(I)->CaptureFolder(); else S = AGameDB->Game(I)->DataDir();
        S = CreateUniqueFolder(AGameDB->Game(I)->Name(), S, T);
        if (!S.isEmpty()) {
            if (FolderType == ftCapture) AGameDB->Game(I)->SetCaptureFolder(S); else AGameDB->Game(I)->SetDataDir(S);
            AGameDB->Game(I)->StoreAllValues();
        }
        StepProgressWindow();
    }
    DoneProgressWindow();
}

int GetAdditionalChecksumData(TGame *AGame, const QString &APath, int DataNeeded, T5StringArray &Files, T5StringArray &Checksums)
{
    QString Exe1, Exe2, S;
    TSearchRec Rec;
    bool OK;
    int result = 0;
    QString Path = IncludeTrailingPathDelimiter(APath);

    if (!Trim(AGame->GameExe()).isEmpty()) Exe1 = Trim(ExtUpperCase(ExtractFileName(MakeAbsPath(AGame->GameExe(), PrgSetup->BaseDir())))); else Exe1 = "";
    if (!Trim(AGame->SetupExe()).isEmpty()) Exe2 = Trim(ExtUpperCase(ExtractFileName(MakeAbsPath(AGame->SetupExe(), PrgSetup->BaseDir())))); else Exe2 = "";

    int I = FindFirst(Path + "*.*", faAnyFile, Rec);
    while ((I == 0) && (DataNeeded > 0)) {
        if ((Rec.Attr & faDirectory) == 0) {
            S = ExtUpperCase(Rec.Name);
            if ((S != Exe1) && (S != Exe2)) {
                S = ExtUpperCase(ExtractFileExt(Rec.Name)); if (!S.isEmpty() && S[0] == QChar('.')) S = Copy(S, 2, MaxInt);
                OK = false;
                for (int J = 0; J < AdditionalChecksumDataGoodFileExt.size(); J++) if (AdditionalChecksumDataGoodFileExt[J] == S) { OK = true; break; }
                if (OK) for (int J = 0; J < AdditionalChecksumDataExcludeFiles.size(); J++) if (Pos(AdditionalChecksumDataExcludeFiles[J], S) != 0) { OK = false; break; }
                if (OK && result <= 4) {
                    Files[result] = Rec.Name;
                    Checksums[result] = GetMD5Sum(Path + Rec.Name, false);
                    result++; DataNeeded--;
                }
            }
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

void AddAdditionalChecksumDataToAutoSetupTemplate(TGame *AGame)
{
    int DataNeeded, DataAvailable, I;
    QString Path;
    T5StringArray Files, Checksums;

    if (Trim(AGame->GameExe()).isEmpty()) return;

    DataNeeded = 0;
    if (Trim(AGame->AddtionalChecksumFile1Checksum()).isEmpty()) DataNeeded++;
    if (Trim(AGame->AddtionalChecksumFile2Checksum()).isEmpty()) DataNeeded++;
    if (Trim(AGame->AddtionalChecksumFile3Checksum()).isEmpty()) DataNeeded++;
    if (Trim(AGame->AddtionalChecksumFile4Checksum()).isEmpty()) DataNeeded++;
    if (Trim(AGame->AddtionalChecksumFile5Checksum()).isEmpty()) DataNeeded++;

    Path = IncludeTrailingPathDelimiter(ExtractFilePath(MakeAbsPath(AGame->GameExe(), PrgSetup->BaseDir())));

    DataAvailable = GetAdditionalChecksumData(AGame, Path, DataNeeded, Files, Checksums);

    I = 0;
    if (Trim(AGame->AddtionalChecksumFile1Checksum()).isEmpty() && (I < DataAvailable)) {
        AGame->SetAddtionalChecksumFile1(Files[I]);
        AGame->SetAddtionalChecksumFile1Checksum(Checksums[I]);
        I++;
    }
    if (Trim(AGame->AddtionalChecksumFile2Checksum()).isEmpty() && (I < DataAvailable)) {
        AGame->SetAddtionalChecksumFile2(Files[I]);
        AGame->SetAddtionalChecksumFile2Checksum(Checksums[I]);
        I++;
    }
    if (Trim(AGame->AddtionalChecksumFile3Checksum()).isEmpty() && (I < DataAvailable)) {
        AGame->SetAddtionalChecksumFile3(Files[I]);
        AGame->SetAddtionalChecksumFile3Checksum(Checksums[I]);
        I++;
    }
    if (Trim(AGame->AddtionalChecksumFile4Checksum()).isEmpty() && (I < DataAvailable)) {
        AGame->SetAddtionalChecksumFile4(Files[I]);
        AGame->SetAddtionalChecksumFile4Checksum(Checksums[I]);
        I++;
    }
    if (Trim(AGame->AddtionalChecksumFile5Checksum()).isEmpty() && (I < DataAvailable)) {
        AGame->SetAddtionalChecksumFile5(Files[I]);
        AGame->SetAddtionalChecksumFile5Checksum(Checksums[I]);
    }
}

QString GetLastModificationDate(TGame *AGame)
{
    QString S, T;
    int I = Pos("-", AGame->LastModification()); if (I == 0) return QString();
    S = ""; try { S = DateToStr(StrToInt(Copy(AGame->LastModification(), 1, I - 1))); } catch (...) { return QString(); }
    T = ""; try { T = TimeToStr(StrToInt(Copy(AGame->LastModification(), I + 1, MaxInt)) / 86400.0); } catch (...) { return QString(); }
    return S + " " + T;
}

bool DOSBoxMode(TGame *Game)
{
    if (Game == nullptr) return false;
    QString S = Trim(ExtUpperCase(Game->ProfileMode()));
    return (S != "SCUMMVM") && (S != "WINDOWS");
}

bool ScummVMMode(TGame *Game)
{
    if (Game == nullptr) return false;
    return Trim(ExtUpperCase(Game->ProfileMode())) == "SCUMMVM";
}

bool WindowsExeMode(TGame *Game)
{
    if (Game == nullptr) return false;
    return Trim(ExtUpperCase(Game->ProfileMode())) == "WINDOWS";
}

TStringList *CheckGameDB(TGameDB *GameDB)
{
    TGame *Game = nullptr;
    QString S;
    bool FirstError = true;
    TStringList *result = new TStringList;

    auto Error = [&](const QString &AS) {
        if (FirstError) { FirstError = false; if (result->Count() > 0) result->Add(""); result->Add(Game->Name()); }
        result->Add("  " + AS);
    };

    InitProgressWindow(MainFormWidget(), GameDB->Count());

    for (int I = 0; I < GameDB->Count(); I++) {
        Game = GameDB->Game(I);
        FirstError = true;

        if (ScummVMMode(Game)) {
            S = IncludeTrailingPathDelimiter(MakeAbsPath(Trim(Game->ScummVMPath()), PrgSetup->BaseDir()));
            if (!DirectoryExists(S)) Error(Format(LanguageSetup->MissingFilesGameDirectory(), {S}));
        } else {
            if (WindowsExeMode(Game)) {
                S = Trim(Game->GameExe());
                if (!S.isEmpty()) {
                    S = MakeAbsPath(S, PrgSetup->BaseDir());
                    if (!FileExists(S)) Error(Format(LanguageSetup->MissingFilesGameFile(), {S})); else {
                        if (!ChecksumForGameOK(Game)) Error(Format(LanguageSetup->MissingFilesGameChecksum(), {S}));
                    }
                }
                S = Trim(Game->SetupExe());
                if (!S.isEmpty()) {
                    S = MakeAbsPath(S, PrgSetup->BaseDir());
                    if (!FileExists(S)) Error(Format(LanguageSetup->MissingFilesSetupFile(), {S})); else {
                        if (!ChecksumForSetupOK(Game)) Error(Format(LanguageSetup->MissingFilesSetupChecksum(), {S}));
                    }
                }
            } else {
                S = Trim(Game->GameExe());
                if (!S.isEmpty() && (ExtUpperCase(Copy(S, 1, 7)) != "DOSBOX:")) {
                    S = MakeAbsPath(S, PrgSetup->BaseDir());
                    if (!FileExists(S)) Error(Format(LanguageSetup->MissingFilesGameFile(), {S})); else {
                        if (!ChecksumForGameOK(Game)) Error(Format(LanguageSetup->MissingFilesGameChecksum(), {S}));
                    }
                }
                S = Trim(Game->SetupExe());
                if (!S.isEmpty() && (ExtUpperCase(Copy(S, 1, 7)) == "DOSBOX:")) {
                    S = MakeAbsPath(S, PrgSetup->BaseDir());
                    if (!FileExists(S)) Error(Format(LanguageSetup->MissingFilesSetupFile(), {S})); else {
                        if (!ChecksumForSetupOK(Game)) Error(Format(LanguageSetup->MissingFilesSetupChecksum(), {S}));
                    }
                }
            }
        }

        S = Trim(Game->CaptureFolder());
        if (!S.isEmpty()) {
            S = IncludeTrailingPathDelimiter(MakeAbsPath(S, PrgSetup->BaseDir()));
            if (!DirectoryExists(S)) Error(Format(LanguageSetup->MissingFilesCaptureFolder(), {S}));
        }

        S = MakeAbsIconName(Game->Icon());
        if (!S.isEmpty()) {
            if (!FileExists(S)) Error(Format(LanguageSetup->MissingFilesIconFile(), {S}));
        }

        S = Trim(Game->DataDir());
        if (!S.isEmpty()) {
            S = IncludeTrailingPathDelimiter(MakeAbsPath(S, PrgSetup->BaseDir()));
            if (!DirectoryExists(S)) Error(Format(LanguageSetup->MissingFilesDataDirectory(), {S}));
        }

        S = Trim(Game->ExtraFiles());
        if (!S.isEmpty()) {
            TStringList *St = ValueToList(S);
            for (int J = 0; J < St->Count(); J++) if (!Trim((*St)[J]).isEmpty()) {
                S = MakeAbsPath((*St)[J], PrgSetup->BaseDir());
                if (!FileExists(S)) Error(Format(LanguageSetup->MissingFilesExtraFile(), {S}));
            }
            delete St;
        }

        S = Trim(Game->ExtraDirs());
        if (!S.isEmpty()) {
            TStringList *St = ValueToList(S);
            for (int J = 0; J < St->Count(); J++) if (!Trim((*St)[J]).isEmpty()) {
                S = IncludeTrailingPathDelimiter(MakeAbsPath(IncludeTrailingPathDelimiter((*St)[J]), PrgSetup->BaseDir()));
                if (!DirectoryExists(S)) Error(Format(LanguageSetup->MissingFilesExtraDirectory(), {S}));
            }
            delete St;
        }

        StepProgressWindow();
    }
    DoneProgressWindow();
    return result;
}

static bool ExtendedChecksumCompare(TGame *A1, TGame *A2)
{
    TStringList St1, St1c, St2, St2c;

    if (!A1->AddtionalChecksumFile1().isEmpty()) { St1.Add(ExtUpperCase(A1->AddtionalChecksumFile1())); St1c.Add(A1->AddtionalChecksumFile1Checksum()); }
    if (!A1->AddtionalChecksumFile2().isEmpty()) { St1.Add(ExtUpperCase(A1->AddtionalChecksumFile2())); St1c.Add(A1->AddtionalChecksumFile2Checksum()); }
    if (!A1->AddtionalChecksumFile3().isEmpty()) { St1.Add(ExtUpperCase(A1->AddtionalChecksumFile3())); St1c.Add(A1->AddtionalChecksumFile3Checksum()); }
    if (!A1->AddtionalChecksumFile4().isEmpty()) { St1.Add(ExtUpperCase(A1->AddtionalChecksumFile4())); St1c.Add(A1->AddtionalChecksumFile4Checksum()); }
    if (!A1->AddtionalChecksumFile5().isEmpty()) { St1.Add(ExtUpperCase(A1->AddtionalChecksumFile5())); St1c.Add(A1->AddtionalChecksumFile5Checksum()); }
    if (!A2->AddtionalChecksumFile1().isEmpty()) { St2.Add(ExtUpperCase(A2->AddtionalChecksumFile1())); St2c.Add(A2->AddtionalChecksumFile1Checksum()); }
    if (!A2->AddtionalChecksumFile2().isEmpty()) { St2.Add(ExtUpperCase(A2->AddtionalChecksumFile2())); St2c.Add(A2->AddtionalChecksumFile2Checksum()); }
    if (!A2->AddtionalChecksumFile3().isEmpty()) { St2.Add(ExtUpperCase(A2->AddtionalChecksumFile3())); St2c.Add(A2->AddtionalChecksumFile3Checksum()); }
    if (!A2->AddtionalChecksumFile4().isEmpty()) { St2.Add(ExtUpperCase(A2->AddtionalChecksumFile4())); St2c.Add(A2->AddtionalChecksumFile4Checksum()); }
    if (!A2->AddtionalChecksumFile5().isEmpty()) { St2.Add(ExtUpperCase(A2->AddtionalChecksumFile5())); St2c.Add(A2->AddtionalChecksumFile5Checksum()); }

    for (int I = 0; I < St1.Count(); I++) {
        int J = St2.IndexOf(St1[I]);
        if ((J >= 0) && (St1c[I] != St2c[J])) return true;
    }
    return false;
}

TStringList *CheckDoubleChecksums(TGameDB *AutoSetupDB)
{
    TStringList *result = new TStringList;
    TStringList Names, Sums;

    for (int I = 0; I < AutoSetupDB->Count(); I++) {
        QString S = Trim(ExtUpperCase(AutoSetupDB->Game(I)->GameExe()));
        if (Trim(S).isEmpty()) continue;
        int J = Names.IndexOf(S);
        if (J < 0) {
            Names.AddObject(S, intptr_t(I));
            Sums.Add(AutoSetupDB->Game(I)->GameExeMD5());
        } else {
            if ((Sums[J] == AutoSetupDB->Game(I)->GameExeMD5()) && (!ExtendedChecksumCompare(AutoSetupDB->Game(Names.ObjectAsInt(J)), AutoSetupDB->Game(I))))
                result->Add(AutoSetupDB->Game(Names.ObjectAsInt(J))->CacheName + " <-> " + AutoSetupDB->Game(I)->CacheName);
        }
    }
    return result;
}

int GetDOSBoxNr(TGame *Game)
{
    QString S = Trim(ExtUpperCase(Game->CustomDOSBoxDir()));
    if (S == "DEFAULT" || S.isEmpty()) return 0;

    for (int I = 1; I < PrgSetup->DOSBoxSettingsCount(); I++) if (Trim(ExtUpperCase(PrgSetup->DOSBoxSettings(I)->Name())) == S) return I;
    return -1;
}

void OpenConfigurationFile(TGame *Game, TStringList *DeleteOnExit)
{
    QString S, T;
    TStringList *St;

    if (WindowsExeMode(Game)) return;
    if (ScummVMMode(Game)) {
        S = TempDir() + ChangeFileExt(ExtractFileName(Game->SetupFile()), ".ini");
        T = "ScummVM";
        St = BuildScummVMIniFile(Game);
        if (St == nullptr) return;
    } else {
        S = TempDir() + ChangeFileExt(ExtractFileName(Game->SetupFile()), ".conf");
        T = "DOSBox";
        St = BuildConfFile(Game, false, false, -1, nullptr, false);
        if (St == nullptr) return;
    }
    St->Insert(0, "# This " + T + " configuration file was automatically created by D-Fend Reloaded.");
    St->Insert(1, "# Changes made to this file will NOT be transfered to D-Rend Reloaded profiles list.");
    St->Insert(2, "# D-Fend Reloaded will delete this file from temp directory on program close.");
    St->Insert(3, "");
    St->Insert(4, "# Command line used when starting " + T + ":");

    if (ScummVMMode(Game)) {
        T = Trim(PrgSetup->ScummVMAdditionalCommandLine());
        if (!Trim(Game->ScummVMParameters()).isEmpty()) {
            if (!T.isEmpty()) T = T + " ";
            T = T + Trim(Game->ScummVMParameters());
        }
        T = GetScummVMCommandLine(S, Game->ScummVMGame(), T, Game->ScummVMRenderMode(), Game->ScummVMPlatform());
    } else {
        T = GetDOSBoxCommandLine(GetDOSBoxNr(Game), S, Game->ShowConsoleWindow(), "");
    }

    St->Insert(5, "# " + T);
    St->Insert(6, "");
    St->Insert(7, "# Config file for profile \"" + Game->Name() + "\"");
    St->Insert(8, "");

    St->SaveToFile(S);
    delete St;
    OpenFileInEditor(S);
    if (DeleteOnExit->IndexOf(S) < 0) DeleteOnExit->Add(S);
}

bool SelectProgramFile(QString &FileName, const QString &HintFirstFile, const QString &HintSecondFile, bool WindowsMode, int OtherEmulator, QWidget *Owner)
{
    QString S, S1, S2, S3, T, U;
    int I;

    TOpenDialog OpenDialog(Owner);
    // Filter, DefaultExt and Title
    if (WindowsMode && (OtherEmulator >= 0)) {
        OpenDialog.Title = LanguageSetup->MenuProfileAddOtherSelectTitle();
        OpenDialog.DefaultExt = "";
        if ((PrgSetup->WindowsBasedEmulatorsNames()->Count() <= OtherEmulator) || (PrgSetup->WindowsBasedEmulatorsPrograms()->Count() <= OtherEmulator) || (PrgSetup->WindowsBasedEmulatorsParameters()->Count() <= OtherEmulator) || (PrgSetup->WindowsBasedEmulatorsExtensions()->Count() <= OtherEmulator)) return false;
        S = Trim((*PrgSetup->WindowsBasedEmulatorsExtensions())[OtherEmulator]);
        while (!S.isEmpty()) {
            I = Pos(";", S);
            if (!T.isEmpty()) T = T + ", ";
            if (!U.isEmpty()) U = U + ";";
            if (I == 0) {
                if (OpenDialog.DefaultExt.isEmpty()) OpenDialog.DefaultExt = Trim(S);
                T = T + "*." + Trim(S);
                U = U + "*." + Trim(S);
                S = "";
            } else {
                if (OpenDialog.DefaultExt.isEmpty()) OpenDialog.DefaultExt = Trim(Copy(S, 1, I - 1));
                T = T + "*." + Trim(Copy(S, 1, I - 1));
                U = U + "*." + Trim(Copy(S, 1, I - 1));
                S = Trim(Copy(S, I + 1, MaxInt));
            }
        }
        OpenDialog.Filter = Format(LanguageSetup->MenuProfileAddOtherSelectFilter(), {(*PrgSetup->WindowsBasedEmulatorsNames())[OtherEmulator], T, U});
    } else {
        OpenDialog.DefaultExt = "exe";
        OpenDialog.Title = LanguageSetup->ProfileEditorEXEDialog();
        if (!Trim(PrgSetup->QBasic()).isEmpty() && FileExists(Trim(PrgSetup->QBasic())) && (!WindowsMode)) S = LanguageSetup->ProfileEditorEXEFilterWithBasic(); else S = LanguageSetup->ProfileEditorEXEFilter();
        S1 = ""; S2 = ""; S3 = "";
        if (!WindowsMode) {
            TStringList St;
            for (I = 0; I < Min(PrgSetup->DOSBoxBasedUserInterpretersPrograms()->Count(), PrgSetup->DOSBoxBasedUserInterpretersExtensions()->Count()); I++) {
                TStringList *St2 = ValueToList((*PrgSetup->DOSBoxBasedUserInterpretersExtensions())[I]);
                T = ""; U = "";
                const int St2Count = St2->Count();   // Delphi evaluates the loop bound only once
                for (int J = 0; J < St2Count; J++) {
                    if (St.IndexOf(ExtUpperCase((*St2)[J])) < 0) {
                        St2->Add(ExtUpperCase((*St2)[J]));
                        S1 = S1 + ", *." + ExtLowerCase((*St2)[J]);
                        S2 = S2 + ";*." + ExtLowerCase((*St2)[J]);
                    }
                    if (T.isEmpty()) T = "*." + ExtLowerCase((*St2)[J]); else T = T + ", *." + ExtLowerCase((*St2)[J]);
                    if (U.isEmpty()) U = "*." + ExtLowerCase((*St2)[J]); else U = U + ";*." + ExtLowerCase((*St2)[J]);
                }
                S3 = S3 + "|" + ChangeFileExt(ExtractFileName((*PrgSetup->DOSBoxBasedUserInterpretersPrograms())[I]), "") + " (" + T + ")|" + U;
                delete St2;
            }
        }
        OpenDialog.Filter = Format(S, {S1, S2, S3});
    }

    // InitialDir
    S = "";
    if (!FileName.isEmpty()) { S = ExtractFilePath(MakeAbsPath(FileName, PrgSetup->BaseDir())); }
    if (S.isEmpty() && !Trim(HintFirstFile).isEmpty()) S = ExtractFilePath(MakeAbsPath(HintFirstFile, PrgSetup->BaseDir()));
    if (S.isEmpty() && !Trim(HintSecondFile).isEmpty()) S = ExtractFilePath(MakeAbsPath(HintSecondFile, PrgSetup->BaseDir()));
    if (S.isEmpty() && WindowsMode) {
        const int CSIDL_PROGRAM_FILES_ = 0x0026;   // CSIDL_PROGRAM_FILES
        S = GetSpecialFolder(MainFormWidget() != nullptr ? MainFormWidget()->winId() : WId(0), CSIDL_PROGRAM_FILES_);
    }
    if (S.isEmpty()) S = PrgSetup->GameDir();
    if (S.isEmpty()) S = PrgSetup->BaseDir();
    OpenDialog.InitialDir = S;

    // Run
    if (!OpenDialog.Execute()) return false;

    S = MakeRelPath(OpenDialog.FileName, PrgSetup->BaseDir());
    if (S.isEmpty()) return false;
    FileName = S;
    return true;
}

static QChar NextFreeDriveLetter(TStringList *Mounting)
{
    QChar result = QChar('C');
    bool B;
    do {
        B = true;
        for (int I = 0; I < Mounting->Count(); I++) {
            TStringList *St = ValueToList((*Mounting)[I]);
            if (St->Count() < 3) { delete St; continue; }
            if (UpperCase((*St)[2]) == QString(result)) { result = QChar(result.unicode() + 1); B = false; delete St; break; }
            delete St;
        }
    } while (!B);
    return result;
}

static bool CanReachFile(TStringList *Mounting, const QString &FileName)
{
    QString S;
    QString FilePath = Trim(ExtUpperCase(IncludeTrailingPathDelimiter(ShortName(MakeAbsPath(ExtractFilePath(FileName), PrgSetup->BaseDir())))));
    for (int I = 0; I < Mounting->Count(); I++) {
        TStringList *St = ValueToList((*Mounting)[I]);
        /* Types to check:
           RealFolder;DRIVE;Letter;False;;FreeSpace
           RealFolder;FLOPPY;Letter;False;;
           RealFolder;CDROM;Letter;IO;Label;
           all other are images */
        if (St->Count() < 2) { delete St; continue; }
        S = Trim(ExtUpperCase((*St)[1]));
        if ((S != "DRIVE") && (S != "FLOPPY") && (S != "CDROM")) { delete St; continue; }
        S = Trim(ExtUpperCase(IncludeTrailingPathDelimiter(ShortName(MakeAbsPath((*St)[0], PrgSetup->BaseDir())))));
        delete St;

        if (Copy(FilePath, 1, Length(S)) == S) return true;
    }
    return false;
}

static void AddPrgDir(TStringList *Mounting, const QString &ProgrammFileDir)
{
    if (CanReachFile(Mounting, ProgrammFileDir)) return;
    if (Mounting->Count() == 10) Mounting->Delete(9);
    Mounting->Add(MakeRelPath(ProgrammFileDir, PrgSetup->BaseDir()) + ";Drive;" + QString(NextFreeDriveLetter(Mounting)) + ";false;");
}

TStringList *BuildGameDirMountData(TGame *Template, const QString &ProgrammFileDir, const QString &SetupFileDir)
{
    TGame *G;
    QString S, T;
    bool B;
    TStringList *result = new TStringList;

    // Load from template
    if (Template == nullptr) G = new TGame(PrgSetup); else G = Template;
    for (int I = 0; I <= 9; I++) {
        if (G->NrOfMounts() >= I + 1) result->Add(G->Mount(I)); else break;
    }
    if (Template == nullptr) delete G;

    // Add GameDir
    B = false;
    T = Trim(ExtUpperCase(IncludeTrailingPathDelimiter(ShortName(MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir())))));
    for (int I = 0; I < result->Count(); I++) {
        TStringList *St = ValueToList((*result)[I]);
        if ((St->Count() < 2) || (Trim(ExtUpperCase((*St)[1])) != "DRIVE")) { delete St; continue; }
        S = Trim(ExtUpperCase(IncludeTrailingPathDelimiter(ShortName(MakeAbsPath((*St)[0], PrgSetup->BaseDir())))));
        delete St;
        B = (T == S); if (B) break;
    }
    if (!B) result->Add(MakeRelPath(PrgSetup->GameDir(), PrgSetup->BaseDir(), true) + ";DRIVE;" + QString(NextFreeDriveLetter(result)) + ";False;;" + IntToStr(DefaultFreeHDSize));

    // Add program file and setup file dir
    if (!ProgrammFileDir.isEmpty()) AddPrgDir(result, ProgrammFileDir);
    if (!SetupFileDir.isEmpty()) AddPrgDir(result, SetupFileDir);
    return result;
}

void BuildGameDirMountData(TStringList *Mounting, const QString &ProgrammFileDir, const QString &SetupFileDir)
{
    bool B;
    QString S, T;

    // Everything already ok ?
    if ((Trim(ProgrammFileDir).isEmpty() || CanReachFile(Mounting, ProgrammFileDir)) &&
        (Trim(SetupFileDir).isEmpty() || CanReachFile(Mounting, SetupFileDir))) return;

    // Add GameDir
    B = false;
    T = Trim(ExtUpperCase(IncludeTrailingPathDelimiter(ShortName(MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir())))));
    for (int I = 0; I < Mounting->Count(); I++) {
        TStringList *St = ValueToList((*Mounting)[I]);
        if ((St->Count() < 2) || (Trim(ExtUpperCase((*St)[1])) != "DRIVE")) { delete St; continue; }
        S = Trim(ExtUpperCase(IncludeTrailingPathDelimiter(ShortName(MakeAbsPath((*St)[0], PrgSetup->BaseDir())))));
        delete St;
        B = (T == S); if (B) break;
    }
    if (!B) Mounting->Add(MakeRelPath(PrgSetup->GameDir(), PrgSetup->BaseDir(), true) + ";DRIVE;" + QString(NextFreeDriveLetter(Mounting)) + ";False;;" + IntToStr(DefaultFreeHDSize));

    // Add program file and setup file dir
    if (!ProgrammFileDir.isEmpty()) AddPrgDir(Mounting, ProgrammFileDir);
    if (!SetupFileDir.isEmpty()) AddPrgDir(Mounting, SetupFileDir);
}

static TStringList *GetProfFileList(const QString &Dir)
{
    TSearchRec Rec;
    TStringList *result = new TStringList;
    int I = FindFirst(IncludeTrailingPathDelimiter(Dir) + "*.prof", faAnyFile, Rec);
    while (I == 0) { result->Add(Rec.Name); I = FindNext(Rec); }
    FindClose(Rec);
    return result;
}

static bool TemplateNeeded(TGameDB *AutoSetupDB, const QString &ExeMD5)
{
    QString S = Trim(ExeMD5);
    if (S.isEmpty()) return true;
    for (int I = 0; I < AutoSetupDB->Count(); I++) if (AutoSetupDB->Game(I)->GameExeMD5() == S) return false;
    return true;
}

void CheckAutoSetupTemplates(TGameDB *AutoSetupDB, const QString &NewAutoSetupTemplatesFolder)
{
    QString Dir = IncludeTrailingPathDelimiter(NewAutoSetupTemplatesFolder);
    TStringList *St = GetProfFileList(Dir);

    for (int I = 0; I < St->Count(); I++) {
        TIniFile *Ini = new TIniFile(Dir + (*St)[I]);
        QString S = Ini->ReadString("Extra", "ExeMD5", "");
        if (!TemplateNeeded(AutoSetupDB, S)) {
            // Delete not needed auto setup file
            delete Ini; Ini = nullptr;
            DeleteFile(Dir + (*St)[I]);
        } else {
            // Fix auto setup template
            Ini->WriteString("Extra", "Exe", ExtractFileName(Ini->ReadString("Extra", "Exe", "")));
            Ini->WriteString("Extra", "Setup", ExtractFileName(Ini->ReadString("Extra", "Setup", "")));
            Ini->WriteString("Extra", "0", ".\\VirtualHD\\;Drive;C;false;");
            for (int J = 1; J <= 9; J++) Ini->DeleteKey("Extra", IntToStr(J));
        }
        delete Ini;
    }
    delete St;
}

TStringList *GetPixelShaders(const QString &DOSBoxDir)
{
    TSearchRec Rec;
    TStringList *result = new TStringList;
    result->Add("none");

    int I = FindFirst(IncludeTrailingPathDelimiter(DOSBoxDir) + "Shaders" + PathDelim + "*.fx", faAnyFile, Rec);
    while (I == 0) {
        result->Add(ChangeFileExt(Rec.Name, ""));
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}
