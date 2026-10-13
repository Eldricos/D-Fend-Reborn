// C++ port of LinkFileUnit.pas (implementation section).
//
// Menus: a TMenuItem that receives the links becomes a QAction (it gets a sub
// menu at runtime) or is already a QMenu; a TPopupMenu is a QMenu.  The link
// menu items are QActions with the dynamic property "tag" (Tag) and the link
// as "hint" property / status tip (Hint).
#include "LinkFileUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "LinkFileEditFormUnit.h"
#include "PrgConsts.h"
#include "VclLayout.h"

#include <QApplication>
#include <QMainWindow>
#include <QMenu>
#include <QAction>

// Application.MainForm
static QWidget *MainFormWidget()
{
    for (QWidget *W : QApplication::topLevelWidgets()) if (qobject_cast<QMainWindow *>(W) != nullptr) return W;
    return QApplication::activeWindow();
}

// Hint of a menu item
static QString ActionHint(QObject *Sender)
{
    return Sender->property("hint").toString();
}

// Icon for TMenuItem.ImageIndex: the image list ("Images" of the menu, the
// DFM menus use "ImageList") of the form/frame that owns the menu.
static QIcon MenuImage(QMenu *Menu, int ImageIndex)
{
    if (ImageIndex < 0 || Menu == nullptr) return QIcon();
    QString Images;
    for (QObject *O = Menu; O != nullptr; O = O->parent()) {
        if (Images.isEmpty()) Images = O->property("vclImages").toString();
        const QString N = O->objectName();
        if (N.isEmpty()) continue;
        QStringList Units;
        if (N == "DFendRebornMainForm") Units << "MainUnit";
        Units << N + "Unit";
        for (const QString &U : Units) {
            QIcon I = VclLayout::imageListIcon(U, Images.isEmpty() ? QStringLiteral("ImageList") : Images, ImageIndex);
            if (!I.isNull()) return I;
        }
    }
    return QIcon();
}

// Sub menu of a menu item (created on demand, TMenuItem.Add)
static QMenu *SubMenuOf(QAction *A, bool Create)
{
    QMenu *M = A->menu<QMenu *>();
    if (M == nullptr && Create) {
        QWidget *Parent = qobject_cast<QWidget *>(A->parent());
        M = new QMenu(Parent);
        M->setTitle(A->text());
        A->setMenu(M);
    }
    return M;
}

// TMenuItem.Free (items may be freed while their OnClick handler is running,
// so they are removed from the menu at once and deleted later)
static void FreeMenuAction(QMenu *Owner, QAction *A)
{
    Owner->removeAction(A);
    QMenu *Sub = A->menu<QMenu *>();
    if (Sub != nullptr) {
        while (!Sub->actions().isEmpty()) FreeMenuAction(Sub, Sub->actions().first());
        A->setMenu(static_cast<QMenu *>(nullptr));
        Sub->deleteLater();
    }
    A->deleteLater();
}

static void ConnectOnClick(QAction *M, const TNotifyEvent &OnClick)
{
    QObject::disconnect(M, &QAction::triggered, nullptr, nullptr);
    if (OnClick) QObject::connect(M, &QAction::triggered, M, [M, OnClick]() { OnClick(M); });
}

/* TLinkList */

TLinkList::TLinkList(bool AAllowLinesAndSubs, bool AFixedNumber, int AHelpID, QKeySequence AShortCutForFirstLink)
{
    FAllowLinesAndSubs = AAllowLinesAndSubs;
    FFixedNumber = AFixedNumber;
    FHelpID = AHelpID;
    FShortCutForFirstLink = AShortCutForFirstLink;
    FDataNames = new TStringList;
    FDataLinks = new TStringList;
    // FDataNames.OnChange:=DataChanged; FDataLinks.OnChange:=DataChanged; (see header)
    FChanged = false;

    FParentMenuItems = new TList;
    FMenuItemsEditIcons = new TList;
}

TLinkList::~TLinkList()
{
    // "If FChanged then SaveData" is done in the derived destructors
    delete FDataNames;
    delete FDataLinks;
    delete FParentMenuItems;
    delete FMenuItemsEditIcons;
}

void TLinkList::DataChanged(QObject* Sender)
{
    Q_UNUSED(Sender);
    FChanged = true;
}

int TLinkList::GetCount()
{
    return FDataNames->Count();
}

QString TLinkList::GetName(int I)
{
    if ((I < 0) || (I >= FDataNames->Count())) return ""; else return (*FDataNames)[I];
}

QString TLinkList::GetLink(int I)
{
    if ((I < 0) || (I >= FDataLinks->Count())) return ""; else return (*FDataLinks)[I];
}

int TLinkList::IndexOf(const QString& AName)
{
    QString S = RemoveUnderline(AName);
    int result = -1;
    for (int I = 0; I < FDataNames->Count(); I++) if ((*FDataNames)[I] == S) { result = I; break; }
    return result;
}

void TLinkList::MenuClick(QObject* Sender)
{
    QString S;
    switch (Sender->property("tag").toInt()) {
        case 1:
            if (Trim(ActionHint(Sender)) != "") {
                S = Trim(ActionHint(Sender));
                if (ExtUpperCase(Copy(S, 1, 7)) != "HTTP:/" "/") S = "http:/" "/" + S;
                ShellOpen(S);
            }
            break;
        case 2:
            if (EditFile(FAllowLinesAndSubs)) for (int I = 0; I < FParentMenuItems->size(); I++) {
                AddLinksToMenu(static_cast<QMenu *>((*FParentMenuItems)[I]), (int)reinterpret_cast<intptr_t>((*FMenuItemsEditIcons)[I]));
            }
            break;
    }
}

void TLinkList::MoveToTop(const QString& AName)
{
    int I = IndexOf(AName);
    if (I >= 0) { FDataNames->Exchange(0, I); FDataLinks->Exchange(0, I); DataChanged(nullptr); }
}

bool TLinkList::EditFile(bool AllowLines)
{
    const int SaveCount = FDataNames->Count();
    const QStringList OldNames = FDataNames->ToQStringList();
    const QStringList OldLinks = FDataLinks->ToQStringList();

    bool result = ShowLinkFileEditDialog(MainFormWidget(), FDataNames, FDataLinks, AllowLines, FFixedNumber, FHelpID);
    if (FFixedNumber) {
        while (FDataNames->Count() > SaveCount) { FDataNames->Delete(FDataNames->Count() - 1); FDataLinks->Delete(FDataLinks->Count() - 1); }
        while (FDataNames->Count() < SaveCount) { FDataNames->Add(""); FDataLinks->Add(""); }
    }
    if (FDataNames->ToQStringList() != OldNames || FDataLinks->ToQStringList() != OldLinks) DataChanged(nullptr);
    if (result) SaveData();
    return result;
}

static int GetLevel(const QString &S)
{
    int result = 0;
    for (int I = 1; I <= Length(S); I++) if (S[I - 1] == '=') result++; else return result;
    return result;
}

void TLinkList::CreateAndAddUserMenuItems(QWidget* Owner, int Tag, TNotifyEvent OnClick)
{
    QMenu *OwnerMenu = qobject_cast<QMenu *>(Owner);
    if (OwnerMenu == nullptr) return;

    int L;
    QAction *Last = nullptr, *M;
    QList<QAction *> Level;

    auto AddToLevel = [&](QAction *Item) {
        if (Level.isEmpty()) {
            OwnerMenu->addAction(Item);
        } else {
            QObject::disconnect(Level.last(), &QAction::triggered, nullptr, nullptr);   // Level[...].OnClick:=nil
            SubMenuOf(Level.last(), true)->addAction(Item);
        }
    };

    for (int I = 0; I < FDataNames->Count(); I++) {
        if (((Trim(Name(I)) == "") || (Trim(Name(I)) == "-")) && FAllowLinesAndSubs) {
            if (I == FDataNames->Count() - 1) continue;
            while (Level.size() > Min(GetLevel(Name(I + 1)), int(Level.size()))) Level.removeLast();
            M = new QAction(OwnerMenu);
            M->setSeparator(true);
            AddToLevel(M);
            continue;
        }

        if ((Trim(Name(I)) == "") || (Trim(Name(I)) == "-")) continue;

        M = new QAction(OwnerMenu);

        if ((Last != nullptr) && FAllowLinesAndSubs) {
            L = Min(GetLevel(Name(I)), int(Level.size()) + 1);
            if (L == Level.size() + 1) { Level.append(Last); } else { while (Level.size() > L) Level.removeLast(); }
        }

        M->setText(Copy(Name(I), GetLevel(Name(I)) + 1, MaxInt));
        M->setProperty("hint", Link(I));
        M->setStatusTip(Link(I));
        M->setProperty("tag", Tag);
        ConnectOnClick(M, OnClick);

        if ((Last == nullptr) && (!FShortCutForFirstLink.isEmpty())) M->setShortcut(FShortCutForFirstLink);

        AddToLevel(M);

        Last = M;
    }
}

void TLinkList::CreateAndAddEditMenuItem(QWidget* Owner, int EditTag, TNotifyEvent OnClick, int EditImage)
{
    QMenu *OwnerMenu = qobject_cast<QMenu *>(Owner);
    if (OwnerMenu == nullptr) return;

    QAction *M = new QAction(OwnerMenu);
    M->setSeparator(true);
    OwnerMenu->addAction(M);

    M = new QAction(OwnerMenu);
    M->setText(LanguageSetup->MenuProfileSearchGameEditLinks());
    M->setProperty("tag", EditTag);
    ConnectOnClick(M, OnClick);
    M->setProperty("imageIndex", EditImage);
    M->setIcon(MenuImage(OwnerMenu, EditImage));
    OwnerMenu->addAction(M);
}

void TLinkList::ClearLinksMenu(QAction* Menu)
{
    QMenu *M = SubMenuOf(Menu, false);
    if (M != nullptr) ClearLinksMenu(M);
}

void TLinkList::ClearLinksMenu(QMenu* Menu)
{
    while (!Menu->actions().isEmpty()) FreeMenuAction(Menu, Menu->actions().first());
}

void TLinkList::AddLinksToMenu(QAction* Menu, int Tag, int EditTag, TNotifyEvent OnClick, int EditImage)
{
    if (Menu == nullptr) return;
    AddLinksToMenu(SubMenuOf(Menu, true), Tag, EditTag, OnClick, EditImage);
}

void TLinkList::AddLinksToMenu(QMenu* Menu, int Tag, int EditTag, TNotifyEvent OnClick, int EditImage)
{
    if (Menu == nullptr) return;
    if (FParentMenuItems->indexOf(Menu) < 0) {
        FParentMenuItems->append(Menu);
        FMenuItemsEditIcons->append(reinterpret_cast<void *>((intptr_t)EditImage));
    } else {
        (*FMenuItemsEditIcons)[FParentMenuItems->indexOf(Menu)] = reinterpret_cast<void *>((intptr_t)EditImage);
    }
    ClearLinksMenu(Menu);
    CreateAndAddUserMenuItems(Menu, Tag, OnClick);
    CreateAndAddEditMenuItem(Menu, EditTag, OnClick, EditImage);
}

void TLinkList::AddLinksToMenu(QAction* Menu, int EditImage)
{
    AddLinksToMenu(Menu, 1, 2, [this](QObject *Sender) { MenuClick(Sender); }, EditImage);
}

void TLinkList::AddLinksToMenu(QMenu* Menu, int EditImage)
{
    AddLinksToMenu(Menu, 1, 2, [this](QObject *Sender) { MenuClick(Sender); }, EditImage);
}

/* TLinkFile */

TLinkFile::TLinkFile(const QString& AFileNameOnly, bool AAllowLinesAndSubs, int AHelpID)
    : TLinkList(AAllowLinesAndSubs, false, AHelpID, QKeySequence())
{
    FFileNameOnly = AFileNameOnly;
    LoadData();
}

TLinkFile::~TLinkFile()
{
    if (FChanged) SaveData();
}

void TLinkFile::LoadData()
{
    QString S = "";
    if (FileExists(PrgDataDir() + SettingsFolder + PathDelim + FFileNameOnly)) S = PrgDataDir() + SettingsFolder + PathDelim + FFileNameOnly;
    if ((S == "") && FileExists(PrgDataDir() + FFileNameOnly)) S = PrgDataDir() + FFileNameOnly;
    if ((S == "") && FileExists(PrgDir() + FFileNameOnly)) S = PrgDir() + FFileNameOnly;
    if ((S == "") && FileExists(PrgDir() + BinFolder + PathDelim + FFileNameOnly)) S = PrgDir() + BinFolder + PathDelim + FFileNameOnly;
    if (S == "") return;

    TStringList FSt;
    if (!FSt.LoadFromFile(S)) return;
    for (int I = 0; I < FSt.Count(); I++) {
        S = Trim(FSt[I]);
        if (S == "") continue;
        if ((S == "-") || (S == "---")) { FDataNames->Add(""); FDataLinks->Add(""); }
        if (Pos(";", S) == 0) continue;
        FDataNames->Add(Trim(Copy(S, 1, Pos(";", S) - 1)));
        FDataLinks->Add(Trim(Copy(S, Pos(";", S) + 1, MaxInt)));
    }

    FChanged = false;
}

void TLinkFile::SaveData()
{
    TStringList FSt;
    for (int I = 0; I < FDataNames->Count(); I++) if (Trim((*FDataNames)[I]) == "") FSt.Add("---"); else FSt.Add((*FDataNames)[I] + ";" + (*FDataLinks)[I]);
    FSt.SaveToFile(PrgDataDir() + SettingsFolder + PathDelim + FFileNameOnly);
    FChanged = false;
}

/* TGameLinks */

TGameLinks::TGameLinks(TGame* AGame)
    : TLinkList(false, true, -1, QKeySequence(Qt::CTRL | Qt::Key_W))
{
    FGame = AGame;
    LoadData();
}

TGameLinks::~TGameLinks()
{
    if (FChanged) SaveData();
}

void TGameLinks::LoadData()
{
    for (int I = 1; I <= 9; I++) {
        FDataNames->Add(FGame->WWWName(I));
        FDataLinks->Add(FGame->WWW(I));
    }
    FChanged = false;
}

void TGameLinks::SaveData()
{
    for (int I = 1; I <= 9; I++) {
        FGame->SetWWWName(I, (*FDataNames)[I - 1]);
        FGame->SetWWW(I, (*FDataLinks)[I - 1]);
    }
    FGame->StoreAllValues();
    FChanged = false;
}

/* global */

void OpenLink(const QString& Link, const QString& GameNamePlaceHolder, const QString& RealGameName)
{
    QString S, T, U;
    int I;

    S = ExtUpperCase(Link);
    T = ExtUpperCase(GameNamePlaceHolder);
    I = Pos(T, S);
    if (I == 0) return;

    S = Copy(Link, 1, I - 1);
    T = Copy(Link, I + Length(GameNamePlaceHolder), MaxInt);

    // Delphi 2007 strings are ANSI: non ASCII characters are encoded as their
    // ANSI byte value
    for (I = 1; I <= Length(RealGameName); I++) {
        const QChar Ch = RealGameName[I - 1];
        if (Pos(Ch, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890") != 0) {
            U = U + Ch;
        } else {
            const QByteArray B = EncodeAnsi(QString(Ch));
            for (char C : B) U = U + "%" + IntToHex((unsigned char)C, 2);
        }
    }

    ShellOpen(S + U + T);
}
