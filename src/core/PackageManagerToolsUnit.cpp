// C++ port of PackageManagerToolsUnit.pas (implementation section).
#include "PackageManagerToolsUnit.h"

#include "PackageDBToolsUnit.h"
#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"

TStringList* FilterSelectLicense = nullptr;
TStringList* FilterSelectGenre = nullptr;
TStringList* FilterSelectDeveloper = nullptr;
TStringList* FilterSelectPublisher = nullptr;
TStringList* FilterSelectYear = nullptr;
TStringList* FilterSelectLanguage = nullptr;

static QString GetPackageManagerToolTip2(TDownloadAutoSetupData* DownloadAutoSetupData)
{
    // Had to split this otherwise the compiler says "the return value is may not defined"
    QString result = "";
    result = result + "\r" + LanguageSetup->GameGenre() + ": " + GetCustomGenreName(DownloadAutoSetupData->Genre())
        + "\r" + LanguageSetup->GameDeveloper() + ": " + DownloadAutoSetupData->Developer()
        + "\r" + LanguageSetup->GamePublisher() + ": " + DownloadAutoSetupData->Publisher()
        + "\r" + LanguageSetup->GameYear() + ": " + DownloadAutoSetupData->Year()
        + "\r" + LanguageSetup->GameLanguage() + ": " + GetCustomLanguageName(DownloadAutoSetupData->Language())
        + "\r" + LanguageSetup->PackageManagerDownloadSize() + ": " + GetNiceFileSize(DownloadAutoSetupData->Size());
    return result;
}

static QString GetPackageManagerToolTip3(TDownloadLanguageData* DownloadLanguageData)
{
    return DownloadLanguageData->Name() + "\r" +
        LanguageSetup->PackageManagerListDescription() + ": " + DownloadLanguageData->Description() + "\r" +
        LanguageSetup->PackageManagerListAuthor() + ": " + DownloadLanguageData->Author() + "\r" +
        LanguageSetup->PackageManagerDownloadSize() + ": " + GetNiceFileSize(DownloadLanguageData->Size());
}

QString GetPackageManagerToolTip(TDownloadData* DownloadData)
{
    QString result = "";

    if (TDownloadAutoSetupData* DownloadAutoSetupData = dynamic_cast<TDownloadAutoSetupData*>(DownloadData)) {
        result = DownloadAutoSetupData->Name();
        if (TDownloadZipData* Z = dynamic_cast<TDownloadZipData*>(DownloadAutoSetupData)) result = result + "\r" + LanguageSetup->PackageManagerListLicense() + ": " + Z->License();
        result = result + GetPackageManagerToolTip2(DownloadAutoSetupData);
        if (Trim(DownloadAutoSetupData->PackageList()->ProviderText()) != "") {
            result = result + "\r\r" + Format(LanguageSetup->PackageManagerProviderProfileString(), {DownloadAutoSetupData->PackageList()->ProviderName()});
        }
    }

    if (TDownloadLanguageData* L = dynamic_cast<TDownloadLanguageData*>(DownloadData)) {
        result = GetPackageManagerToolTip3(L);
    }

    return result;
}

static void AddColumn(TListView* AListView, const QString& Caption)
{
    TListColumn* C = AListView->Columns()->Add(); C->SetWidth(-1); C->SetCaption(Caption);
}

void InitPackageManagerListView(TListView* AListView, bool AutoSetups)
{
    AddColumn(AListView, LanguageSetup->GameName());
    if (!AutoSetups) AddColumn(AListView, LanguageSetup->PackageManagerListLicense());
    AddColumn(AListView, LanguageSetup->GameGenre());
    AddColumn(AListView, LanguageSetup->PackageManagerDownloadSize());
    AddColumn(AListView, LanguageSetup->GameDeveloper());
    AddColumn(AListView, LanguageSetup->GamePublisher());
    AddColumn(AListView, LanguageSetup->GameYear());
    AddColumn(AListView, LanguageSetup->GameLanguage());
}

void InitPackageManagerIconsListView(TListView* AListView)
{
    AddColumn(AListView, LanguageSetup->GameName());
    AddColumn(AListView, LanguageSetup->PackageManagerDownloadSize());
}

void InitPackageManagerIconSetsListView(TListView* AListView)
{
    AddColumn(AListView, LanguageSetup->GameName());
    AddColumn(AListView, LanguageSetup->PackageManagerDownloadSize());
}

void InitPackageManagerLanguagessListView(TListView* AListView)
{
    AddColumn(AListView, LanguageSetup->GameName());
    AddColumn(AListView, LanguageSetup->PackageManagerListDescription());
}

static bool GameAlreadyInstalled(TDownloadAutoSetupData* Data, TGameDB* GameDB)
{
    QString S = ExtUpperCase(Data->Name());
    for (int I = 0; I < GameDB->Count(); I++) if (ExtUpperCase(GameDB->Game(I)->CacheName) == S) {
        if ((Data->GameExeChecksum() != "") && (GameDB->Game(I)->GameExeMD5() != "")) {
            if (Data->GameExeChecksum() == GameDB->Game(I)->GameExeMD5()) return true;
        } else {
            return true;
        }
    }
    return false;
}

static bool SpecialCheck(TDownloadAutoSetupData* Data)
{
    // GetLocaleInfo(LOCALE_USER_DEFAULT,LOCALE_SLANGUAGE) -> native name of the user's language
    auto GetLocalInfo = []() -> QString { return QLocale::system().nativeLanguageName(); };
    const QString Filter = "DEUTSCH";
    QString S, T;
    S = ExtUpperCase(Data->URL()); if ((Copy(S, 1, 7) != "HTTP:/" "/") && (Copy(S, 1, 8) != "HTTPS:/" "/") && (Copy(S, 1, 6) != "FTP:/" "/")) return false;
    S = ExtUpperCase(GetLocalInfo()); T = ExtUpperCase(LanguageSetup->LocalLanguageName()); if ((Pos(Filter, S) == 0) && (Pos(Filter, T) == 0)) return false;
    S = ExtUpperCase(Data->Genre()); T = ExtUpperCase(Data->Name());
    if ((Pos("FIRST PERSON", S) != 0) || (Pos("DOOM", T) != 0) || (Pos("DUKE NUKE", T) != 0) || (Pos("QUAKE", T) != 0) || (Pos("WOLFENSTEIN", T) != 0)) return true;
    return false;
}

static void AutoSizeColumns(TListView* AListView)
{
    AListView->Columns()->BeginUpdate();
    for (int I = 0; I < AListView->Columns()->Count(); I++) if (AListView->Items()->Count() > 0) {
        AListView->Column(I)->SetWidth(-2);
        AListView->Column(I)->SetWidth(-1);
    } else {
        AListView->Column(I)->SetWidth(-2);
    }
    AListView->Columns()->EndUpdate();
}

void LoadListView(TListView* AListView, TPackageDB* PackageDB, const TFilter& Filter, TChecksumScanner* AutoSetupChecksumScanner, bool AutoSetups, TGameDB* GameDB, TGameDB* AutoSetupDB, bool HideIfAlreadyInstalled)
{
    int I, J, K;
    TListItem* Item;
    TDownloadAutoSetupData* Data;
    QString S;
    TStringList Selected;
    int Ver;

    Ver = VersionToInt(GetNormalFileVersionAsString());
    for (I = 0; I < AListView->Items()->Count(); I++) if (AListView->Items()->Item(I)->Checked()) Selected.Add(AListView->Items()->Item(I)->Caption());

    AListView->Items()->BeginUpdate();
    AListView->Items()->Clear();

    AListView->Columns()->BeginUpdate();
    for (I = 0; I < AListView->Columns()->Count(); I++) AListView->Column(I)->SetWidth(1);
    AListView->Columns()->EndUpdate();

    {
        TStringList St;
        for (I = 0; I < PackageDB->Count(); I++) {
            if (AutoSetups) K = PackageDB->List(I)->AutoSetupCount(); else K = PackageDB->List(I)->GamesCount();
            for (J = 0; J < K; J++) {
                if (AutoSetups) Data = PackageDB->List(I)->AutoSetup(J); else { Data = PackageDB->List(I)->Game(J); if (SpecialCheck(Data)) continue; }
                // Already installed ?
                if (HideIfAlreadyInstalled) {
                    if (AutoSetups) {
                        if (AutoSetupChecksumScanner->GetChecksum(ExtractFileNameFromURL(Data->URL(), ".prof", true)) == Data->PackageChecksum()) continue;
                        if (GameAlreadyInstalled(Data, AutoSetupDB)) continue;
                        if (Trim(Data->MaxVersion()) != "") {
                            if (VersionToInt(Data->MaxVersion()) < Ver) continue;
                        }
                    } else {
                        if (GameAlreadyInstalled(Data, GameDB)) continue;
                    }
                }
                // Filter
                switch (Filter.Key) {
                    case fkName: S = Data->Name(); break;
                    case fkLicense: S = static_cast<TDownloadZipData*>(Data)->License(); break;
                    case fkGenre: S = Data->Genre(); break;
                    case fkPublisher: S = Data->Publisher(); break;
                    case fkDeveloper: S = Data->Developer(); break;
                    case fkYear: S = Data->Year(); break;
                    case fkLanguage: S = Data->Language(); break;
                    default: S = ""; break;
                }
                if ((Filter.Key != fkNoFilter) && (S != "") && (Pos(Filter.UpperValue, ExtUpperCase(S)) == 0)) continue;
                // Add to string list
                if (AutoSetups)
                    St.AddObject(PackageDB->List(I)->AutoSetup(J)->Name(), static_cast<intptr_t>(I * 65536 + J));
                else
                    St.AddObject(PackageDB->List(I)->Game(J)->Name(), static_cast<intptr_t>(I * 65536 + J));
            }
        }
        St.Sort();

        // Add to listview
        for (I = 0; I < St.Count(); I++) {
            Item = AListView->Items()->Add();
            Item->SetCaption(St[I]);
            J = St.ObjectAsInt(I) / 65536;
            K = St.ObjectAsInt(I) % 65536;

            Item->SubItems().BeginUpdate();
            if (AutoSetups) {
                Data = PackageDB->List(J)->AutoSetup(K);
            } else {
                Data = PackageDB->List(J)->Game(K);
                Item->SubItems().Add(PackageDB->List(J)->Game(K)->License());
            }

            Item->SubItems().Add(GetCustomGenreName(Data->Genre()));
            Item->SubItems().Add(GetNiceFileSize(Data->Size()));
            Item->SubItems().Add(Data->Developer());
            Item->SubItems().Add(Data->Publisher());
            Item->SubItems().Add(Data->Year());
            Item->SubItems().Add(GetCustomLanguageName(Data->Language()));
            Item->SubItems().EndUpdate();

            Item->SetData(static_cast<TDownloadData*>(Data));
        }
    }
    AListView->Items()->EndUpdate();

    for (I = 0; I < AListView->Items()->Count(); I++) AListView->Items()->Item(I)->SetChecked(Selected.IndexOf(AListView->Items()->Item(I)->Caption()) >= 0);

    AutoSizeColumns(AListView);

    AListView->update();
}

void LoadIconsListView(TListView* AListView, TPackageDB* PackageDB, TChecksumScanner* IconChecksumScanner, bool HideIfAlreadyInstalled)
{
    int I, J, K;
    TStringList Selected;
    TListItem* Item;
    TDownloadIconData* Data;

    for (I = 0; I < AListView->Items()->Count(); I++) if (AListView->Items()->Item(I)->Checked()) Selected.Add(AListView->Items()->Item(I)->Caption());

    AListView->Items()->BeginUpdate();
    AListView->Items()->Clear();

    {
        TStringList St;
        for (I = 0; I < PackageDB->Count(); I++) for (J = 0; J < PackageDB->List(I)->IconCount(); J++) {
            Data = PackageDB->List(I)->Icon(J);
            // Already installed ?
            if (HideIfAlreadyInstalled) {
                if (IconChecksumScanner->GetChecksum(ExtractFileNameFromURL(Data->URL(), ".ico", true)) == Data->PackageChecksum()) continue;
            }
            // Add to string list
            St.AddObject(Data->Name(), static_cast<intptr_t>(I * 65536 + J));
        }
        St.Sort();

        // Add to listview
        for (I = 0; I < St.Count(); I++) {
            Item = AListView->Items()->Add();
            Item->SetCaption(St[I]);
            J = St.ObjectAsInt(I) / 65536;
            K = St.ObjectAsInt(I) % 65536;
            Data = PackageDB->List(J)->Icon(K);
            Item->SubItems().Add(GetNiceFileSize(Data->Size()));
            Item->SetData(static_cast<TDownloadData*>(Data));
        }
    }
    AListView->Items()->EndUpdate();

    for (I = 0; I < AListView->Items()->Count(); I++) AListView->Items()->Item(I)->SetChecked(Selected.IndexOf(AListView->Items()->Item(I)->Caption()) >= 0);

    AutoSizeColumns(AListView);
}

static bool IconSetAlreadyInstalledInDir(const QString& Dir, const QString& S1, const QString& S2, const QString& S3)
{
    TSearchRec Rec;
    int I = FindFirst(Dir + "*.*", faDirectory, Rec);
    while (I == 0) {
        if (((Rec.Attr & faDirectory) != 0) && (Rec.Name != ".") && (Rec.Name != "..")) {
            TIniFile Ini(Dir + Rec.Name + PathDelim + IconsConfFile);
            if ((S1 == Trim(ExtUpperCase(Ini.ReadString("Information", "Name", "")))) && (S2 == Trim(ExtUpperCase(Ini.ReadString("Information", "MinVersion", "")))) && (S3 == Trim(ExtUpperCase(Ini.ReadString("Information", "MaxVersion", ""))))) {
                FindClose(Rec);
                return true;
            }
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return false;
}

static bool IconSetAlreadyInstalled(const QString& Name, const QString& MinVersion, const QString& MaxVersion)
{
    QString S1, S2, S3;

    S1 = Trim(ExtUpperCase(Name));
    S2 = Trim(ExtUpperCase(MinVersion));
    S3 = Trim(ExtUpperCase(MaxVersion));

    if (IconSetAlreadyInstalledInDir(PrgDataDir() + IconSetsFolder + PathDelim, S1, S2, S3)) return true;

    if (PrgDir() == PrgDataDir()) return false;

    return IconSetAlreadyInstalledInDir(PrgDir() + IconSetsFolder + PathDelim, S1, S2, S3);
}

void LoadIconSetsListView(TListView* AListView, TPackageDB* PackageDB, bool HideIfAlreadyInstalled)
{
    int I, J, K;
    TStringList Selected;
    TDownloadIconSetData* Data;
    TListItem* Item;

    for (I = 0; I < AListView->Items()->Count(); I++) if (AListView->Items()->Item(I)->Checked()) Selected.Add(AListView->Items()->Item(I)->Caption());

    AListView->Items()->BeginUpdate();
    AListView->Items()->Clear();

    {
        TStringList St;
        for (I = 0; I < PackageDB->Count(); I++) for (J = 0; J < PackageDB->List(I)->IconSetCount(); J++) {
            Data = PackageDB->List(I)->IconSet(J);
            if ((VersionToInt(Data->MinVersion()) > VersionToInt(GetNormalFileVersionAsString())) || (VersionToInt(Data->MaxVersion()) < VersionToInt(GetNormalFileVersionAsString()))) continue;
            // Already installed ?
            if (HideIfAlreadyInstalled) {
                if (IconSetAlreadyInstalled(Data->Name(), Data->MinVersion(), Data->MaxVersion())) continue;
            }
            // Add to string list
            St.AddObject(Data->Name(), static_cast<intptr_t>(I * 65536 + J));
        }
        St.Sort();

        // Add to listview
        for (I = 0; I < St.Count(); I++) {
            Item = AListView->Items()->Add();
            Item->SetCaption(St[I]);
            J = St.ObjectAsInt(I) / 65536;
            K = St.ObjectAsInt(I) % 65536;
            Data = PackageDB->List(J)->IconSet(K);
            Item->SubItems().Add(GetNiceFileSize(Data->Size()));
            Item->SetData(static_cast<TDownloadData*>(Data));
        }
    }
    AListView->Items()->EndUpdate();

    for (I = 0; I < AListView->Items()->Count(); I++) AListView->Items()->Item(I)->SetChecked(Selected.IndexOf(AListView->Items()->Item(I)->Caption()) >= 0);

    AutoSizeColumns(AListView);
}

void LoadLanguagesListView(TListView* AListView, TPackageDB* PackageDB, bool HideIfAlreadyInstalled, TChecksumScanner* LanguageChecksumScanner1, TChecksumScanner* LanguageChecksumScanner2)
{
    int I, J, K;
    TStringList Selected;
    TDownloadLanguageData* Data;
    TListItem* Item;

    for (I = 0; I < AListView->Items()->Count(); I++) if (AListView->Items()->Item(I)->Checked()) Selected.Add(AListView->Items()->Item(I)->Caption());

    AListView->Items()->BeginUpdate();
    AListView->Items()->Clear();

    {
        TStringList St;
        for (I = 0; I < PackageDB->Count(); I++) for (J = 0; J < PackageDB->List(I)->LanguageCount(); J++) {
            Data = PackageDB->List(I)->Language(J);
            if ((VersionToInt(Data->MinVersion()) > VersionToInt(GetNormalFileVersionAsString())) || (VersionToInt(Data->MaxVersion()) < VersionToInt(GetNormalFileVersionAsString()))) continue;
            if (HideIfAlreadyInstalled) {
                if (LanguageChecksumScanner1->GetChecksum(ExtractFileNameFromURL(Data->URL(), ".ini", true)) == Data->PackageChecksum()) continue;
                if ((LanguageChecksumScanner2 != nullptr) && (LanguageChecksumScanner2->GetChecksum(ExtractFileNameFromURL(Data->URL(), ".ini", true)) == Data->PackageChecksum())) continue;
            }
            // Add to string list
            St.AddObject(Data->Name(), static_cast<intptr_t>(I * 65536 + J));
        }
        St.Sort();

        // Add to listview
        for (I = 0; I < St.Count(); I++) {
            Item = AListView->Items()->Add();
            Item->SetCaption(St[I]);
            J = St.ObjectAsInt(I) / 65536;
            K = St.ObjectAsInt(I) % 65536;
            Data = PackageDB->List(J)->Language(K);
            Item->SubItems().Add(Data->Description());
            Item->SetData(static_cast<TDownloadData*>(Data));
        }
    }
    AListView->Items()->EndUpdate();

    for (I = 0; I < AListView->Items()->Count(); I++) AListView->Items()->Item(I)->SetChecked(Selected.IndexOf(AListView->Items()->Item(I)->Caption()) >= 0);

    AutoSizeColumns(AListView);
}

// TMenuItem.Create + Parent.Add + Tag + OnClick + Caption
static QAction* AddMenuItem(QMenu* Parent, const QString& Caption, int Tag, int ParentTag, const TNotifyEvent& OnClick)
{
    QAction* M = new QAction(Caption, Parent);
    Parent->addAction(M);
    M->setProperty("tag", Tag);
    if (ParentTag != INT_MIN) M->setProperty("parentTag", ParentTag);
    if (OnClick) QObject::connect(M, &QAction::triggered, M, [M, OnClick]() { OnClick(M); });
    return M;
}

static void AddSubMenuItems(QMenu* PopupMenu, QMenu* Parent, TStringList* St, const TNotifyEvent& OnClick, bool UseObjectAsTag = false)
{
    Q_UNUSED(PopupMenu);
    const int ParentTag = Parent->menuAction()->property("tag").toInt();
    for (int I = 0; I < St->Count(); I++) {
        AddMenuItem(Parent, (*St)[I], UseObjectAsTag ? St->ObjectAsInt(I) : I, ParentTag, OnClick);
    }
}

static void CheckAndAdd(const QString& S, TStringList* ListUpper, TStringList* List)
{
    int I;
    QString T, U;
    T = S;
    I = Pos(";", T);
    while (T != "") {
        if (I > 0) {
            U = Trim(Copy(T, 1, I - 1)); T = Trim(Copy(T, I + 1, MaxInt));
        } else {
            U = T; T = "";
        }
        I = Pos(";", T);
        if (ListUpper->IndexOf(ExtUpperCase(U)) < 0) { List->Add(U); ListUpper->Add(ExtUpperCase(U)); }
    }
}

// Creates a sub menu (TMenuItem with sub items)
static QMenu* AddSubMenu(QMenu* PopupMenu, const QString& Caption, int Tag)
{
    QMenu* M = new QMenu(Caption, PopupMenu);
    M->menuAction()->setProperty("tag", Tag);
    PopupMenu->addMenu(M);
    return M;
}

void OpenFilterPopup(QPoint OpenPos, bool AutoSetups, QMenu* PopupMenu, TPackageDB* PackageDB, TNotifyEvent OnClick, bool HideIfAlreadyInstalled, TChecksumScanner* AutoSetupChecksumScanner, TGameDB* GameDB)
{
    int I, J, K;
    TDownloadAutoSetupData* Data;
    QMenu* M;

    // PopupMenu.Items.Clear (including the sub menus)
    PopupMenu->clear();
    for (QMenu* Sub : PopupMenu->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly)) Sub->deleteLater();

    if (FilterSelectLicense != nullptr) FilterSelectLicense->Clear(); else FilterSelectLicense = new TStringList;
    if (FilterSelectGenre != nullptr) FilterSelectGenre->Clear(); else FilterSelectGenre = new TStringList;
    if (FilterSelectDeveloper != nullptr) FilterSelectDeveloper->Clear(); else FilterSelectDeveloper = new TStringList;
    if (FilterSelectPublisher != nullptr) FilterSelectPublisher->Clear(); else FilterSelectPublisher = new TStringList;
    if (FilterSelectYear != nullptr) FilterSelectYear->Clear(); else FilterSelectYear = new TStringList;
    if (FilterSelectLanguage != nullptr) FilterSelectLanguage->Clear(); else FilterSelectLanguage = new TStringList;

    TStringList FilterSelectLicenseUpper, FilterSelectGenreUpper, FilterSelectDeveloperUpper, FilterSelectPublisherUpper, FilterSelectYearUpper, FilterSelectLanguageUpper;

    for (I = 0; I < PackageDB->Count(); I++) {
        if (AutoSetups) K = PackageDB->List(I)->AutoSetupCount(); else K = PackageDB->List(I)->GamesCount();
        for (J = 0; J < K; J++) {
            if (AutoSetups) Data = PackageDB->List(I)->AutoSetup(J); else Data = PackageDB->List(I)->Game(J);
            // Only add records to filter list that are available in unfiltered list
            if (HideIfAlreadyInstalled) {
                if (AutoSetups) {
                    if (AutoSetupChecksumScanner->GetChecksum(ExtractFileNameFromURL(Data->URL(), ".prof", true)) == Data->PackageChecksum()) continue;
                }
                if (GameAlreadyInstalled(Data, GameDB)) continue;
            }
            if (!AutoSetups) CheckAndAdd(static_cast<TDownloadZipData*>(Data)->License(), &FilterSelectLicenseUpper, FilterSelectLicense);
            CheckAndAdd(Data->Genre(), &FilterSelectGenreUpper, FilterSelectGenre);
            CheckAndAdd(Data->Developer(), &FilterSelectDeveloperUpper, FilterSelectDeveloper);
            CheckAndAdd(Data->Publisher(), &FilterSelectPublisherUpper, FilterSelectPublisher);
            CheckAndAdd(Data->Year(), &FilterSelectYearUpper, FilterSelectYear);
            CheckAndAdd(Data->Language(), &FilterSelectLanguageUpper, FilterSelectLanguage);
        }
    }

    {
        TStringList FilterSelectGenreTranslated, FilterSelectLanguageTranslated;

        for (I = 0; I < FilterSelectGenre->Count(); I++) FilterSelectGenreTranslated.Add(GetCustomGenreName((*FilterSelectGenre)[I]));
        for (I = 0; I < FilterSelectLanguage->Count(); I++) FilterSelectLanguageTranslated.Add(GetCustomLanguageName((*FilterSelectLanguage)[I]));

        PopupMenu->setProperty("tag", AutoSetups ? 1 : 0);

        AddMenuItem(PopupMenu, LanguageSetup->PackageManagerNoFilter(), -1, INT_MIN, OnClick);

        PopupMenu->addSeparator();

        if ((!AutoSetups) && (FilterSelectLicense->Count() > 1)) {
            FilterSelectLicense->Sort();
            M = AddSubMenu(PopupMenu, LanguageSetup->PackageManagerListLicense(), 1);
            AddSubMenuItems(PopupMenu, M, FilterSelectLicense, OnClick);
        }

        if (FilterSelectGenre->Count() > 1) {
            for (I = 0; I < FilterSelectGenreTranslated.Count(); I++) FilterSelectGenreTranslated.SetObjects(I, reinterpret_cast<void*>(static_cast<intptr_t>(I)));
            FilterSelectGenreTranslated.Sort();
            M = AddSubMenu(PopupMenu, LanguageSetup->GameGenre(), 2);
            AddSubMenuItems(PopupMenu, M, &FilterSelectGenreTranslated, OnClick, true);
        }

        if (FilterSelectDeveloper->Count() > 1) {
            FilterSelectDeveloper->Sort();
            M = AddSubMenu(PopupMenu, LanguageSetup->GameDeveloper(), 3);
            AddSubMenuItems(PopupMenu, M, FilterSelectDeveloper, OnClick);
        }

        if (FilterSelectPublisher->Count() > 1) {
            FilterSelectPublisher->Sort();
            M = AddSubMenu(PopupMenu, LanguageSetup->GamePublisher(), 4);
            AddSubMenuItems(PopupMenu, M, FilterSelectPublisher, OnClick);
        }

        if (FilterSelectYear->Count() > 1) {
            FilterSelectYear->Sort();
            M = AddSubMenu(PopupMenu, LanguageSetup->GameYear(), 5);
            AddSubMenuItems(PopupMenu, M, FilterSelectYear, OnClick);
        }

        if (FilterSelectLanguage->Count() > 1) {
            for (I = 0; I < FilterSelectLanguageTranslated.Count(); I++) FilterSelectLanguageTranslated.SetObjects(I, reinterpret_cast<void*>(static_cast<intptr_t>(I)));
            FilterSelectLanguageTranslated.Sort();
            M = AddSubMenu(PopupMenu, LanguageSetup->GameLanguage(), 6);
            AddSubMenuItems(PopupMenu, M, &FilterSelectLanguageTranslated, OnClick, true);
        }
    }

    PopupMenu->popup(QPoint(OpenPos.x() + 5, OpenPos.y() + 5));
}

TStringList* ExePackageInfo(void* AObject, TPackageDBCache* PackageDBCache)
{
    QString S;
    TStringList* result = new TStringList;

    // "AObject is TDownloadExeData": cached packages are owned by PackageDBCache
    bool IsCachedPackage = false;
    for (int I = 0; I < PackageDBCache->Count(); I++) if (PackageDBCache->Package(I) == AObject) { IsCachedPackage = true; break; }

    if (!IsCachedPackage) {
        TDownloadExeData* DownloadExeData = static_cast<TDownloadExeData*>(static_cast<TDownloadData*>(AObject));
        result->SetText(ReplaceBRs(DownloadExeData->Description()));
        result->Add("");
        result->Add(LanguageSetup->PackageManagerDownloadSize() + ": " + GetNiceFileSize(DownloadExeData->Size()));
        S = PackageDBCache->FindChecksum(DownloadExeData->Name());
        if (S != "") {
            result->Add("");
            if (S == DownloadExeData->PackageChecksum()) {
                result->Add(LanguageSetup->PackageManagerPackageAlreadyDownloaded());
            } else {
                result->Add(LanguageSetup->PackageManagerOldPackageAlreadyDownloaded());
            }
        }
    } else {
        TCachedPackage* CachedPackage = static_cast<TCachedPackage*>(AObject);
        result->SetText(ReplaceBRs(CachedPackage->Description()));
        result->Add("");
        result->Add(LanguageSetup->PackageManagerDownloadSize() + ": " + GetNiceFileSize(CachedPackage->Size()));
        result->Add("");
        result->Add(LanguageSetup->PackageManagerPackageOnlyInCache());
    }
    return result;
}

namespace {
struct TListRec {
    bool Checked = false;
    bool Selected = false;
    QStringList Cols;
    void* Data = nullptr;
};
}

void SortListView(TListView* AListView, int ColNr, bool Reverse, bool IsSize)
{
    QList<TListRec> Data;
    int I, J, Nr, NewSelIndex;
    qint64 Size = 0;
    TListItem* Item;
    bool B, Rebuild;
    QString S, T;

    // Save list
    Data.resize(AListView->Items()->Count());
    for (I = 0; I < AListView->Items()->Count(); I++) {
        TListItem* LI = AListView->Items()->Item(I);
        Data[I].Checked = LI->Checked();
        Data[I].Selected = false;
        Data[I].Cols.clear();
        Data[I].Cols.append(LI->Caption());
        for (J = 1; J <= LI->SubItems().Count(); J++) Data[I].Cols.append(LI->SubItems()[J - 1]);
        Data[I].Data = LI->Data();
    }
    if (AListView->ItemIndex() >= 0) Data[AListView->ItemIndex()].Selected = true;

    TStringList St;
    // Build list to sort
    for (I = 0; I < Data.count(); I++) {
        S = (ColNr >= 0 && ColNr < Data[I].Cols.count()) ? Data[I].Cols[ColNr] : QString();
        if (IsSize) {
            T = Copy(S, Length(S) - 1, MaxInt);
            int V;
            if (TryStrToInt(Trim(Copy(S, 1, Length(S) - 2)), V)) {
                Size = V;
                if (T == "KB") Size = Size * 1024;
                if (T == "MB") Size = Size * 1024 * 1024;
                if (T == "GB") Size = Size * 1024 * 1024 * 1024;
            }
            S = IntToStr(Size);
            while (Length(S) < 10) S = "0" + S;
        }
        St.AddObject(S, static_cast<intptr_t>(I));
    }

    // Sort
    St.Sort();

    // Rebuild needed?
    B = false;
    if (Reverse) {
        for (I = 0; I < St.Count(); I++) if (St.ObjectAsInt(I) != (St.Count() - 1) - I) { B = true; break; }
    } else {
        for (I = 0; I < St.Count(); I++) if (St.ObjectAsInt(I) != I) { B = true; break; }
    }
    if (!B) return;

    // Create new list
    NewSelIndex = -1;
    AListView->Items()->BeginUpdate();
    AListView->Columns()->BeginUpdate();
    for (I = 0; I < AListView->Columns()->Count(); I++) AListView->Column(I)->SetWidth(1);
    AListView->Columns()->EndUpdate();

    Rebuild = AListView->Items()->Count() == St.Count();
    if (!Rebuild) AListView->Items()->Clear();
    for (I = 0; I < St.Count(); I++) {
        if (Reverse) Nr = St.ObjectAsInt((St.Count() - 1) - I); else Nr = St.ObjectAsInt(I);
        if (Rebuild) {
            Item = AListView->Items()->Item(I);
            Item->SetChecked(Data[Nr].Checked);
            Item->SetCaption(Data[Nr].Cols[0]);
            for (J = 1; J < Data[Nr].Cols.count(); J++) Item->SubItems().SetStrings(J - 1, Data[Nr].Cols[J]);
        } else {
            Item = AListView->Items()->Add();
            Item->SetChecked(Data[Nr].Checked);
            Item->SetCaption(Data[Nr].Cols[0]);
            for (J = 1; J < Data[Nr].Cols.count(); J++) Item->SubItems().Add(Data[Nr].Cols[J]);
        }
        // Port: the original saved Data but never restored it, so Item.Data
        // pointed to the wrong package after sorting.
        Item->SetData(Data[Nr].Data);
        if (Data[Nr].Selected) NewSelIndex = I;
    }
    AListView->Items()->EndUpdate();

    AutoSizeColumns(AListView);
    AListView->update();

    if (NewSelIndex >= 0) AListView->SetItemIndex(NewSelIndex);
}

void PackageManagerToolsUnit_Initialization()
{
}

void PackageManagerToolsUnit_Finalization()
{
    delete FilterSelectLicense; FilterSelectLicense = nullptr;
    delete FilterSelectGenre; FilterSelectGenre = nullptr;
    delete FilterSelectDeveloper; FilterSelectDeveloper = nullptr;
    delete FilterSelectPublisher; FilterSelectPublisher = nullptr;
    delete FilterSelectYear; FilterSelectYear = nullptr;
    delete FilterSelectLanguage; FilterSelectLanguage = nullptr;
}
