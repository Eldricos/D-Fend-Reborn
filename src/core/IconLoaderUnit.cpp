// C++ port of IconLoaderUnit.pas (implementation section).
//
// VCL image lists store bitmaps plus monochrome masks.  The Qt TImageList
// stores pixmaps with alpha channel, so a "mask" is applied by making the
// masked pixels transparent.  Images that already contain transparency (PNG
// icons) keep their alpha channel; the color key masks of the original
// (bottom left pixel color / white) are only applied to opaque images.
#include "IconLoaderUnit.h"

#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "ImageStretch.h"

int MI_Count = 0;
TUserIconLoader* UserIconLoader = nullptr;

// ---------------------------------------------------------------------------
// helpers of the port
// ---------------------------------------------------------------------------

// TBitmap.Create + SetSize (empty image)
static QImage BlankImage(int W, int H)
{
    QImage B(qMax(1, W), qMax(1, H), QImage::Format_ARGB32);
    B.fill(Qt::transparent);
    return B;
}

static bool HasTransparency(const QImage &B)
{
    if (!B.hasAlphaChannel()) return false;
    const QImage I = B.convertToFormat(QImage::Format_ARGB32);
    for (int Y = 0; Y < I.height(); Y++) {
        const QRgb *Line = reinterpret_cast<const QRgb *>(I.constScanLine(Y));
        for (int X = 0; X < I.width(); X++) if (qAlpha(Line[X]) != 255) return true;
    }
    return false;
}

// ImageList.Add/Replace(B, Mask): white mask pixels are transparent
static QPixmap MaskedPixmap(const QImage &B, const QImage &Mask)
{
    if (HasTransparency(B) || Mask.isNull()) return QPixmap::fromImage(B);
    QImage I = B.convertToFormat(QImage::Format_ARGB32);
    for (int Y = 0; Y < I.height() && Y < Mask.height(); Y++) {
        QRgb *Line = reinterpret_cast<QRgb *>(I.scanLine(Y));
        for (int X = 0; X < I.width() && X < Mask.width(); X++)
            if ((Mask.pixel(X, Y) & 0xFFFFFF) == 0xFFFFFF) Line[X] = qRgba(0, 0, 0, 0);
    }
    return QPixmap::fromImage(I);
}

// ImageList.GetBitmap(Nr,B)
static bool GetBitmap(TImageList *ImageList, int Nr, QImage &B)
{
    if (ImageList == nullptr || Nr < 0 || Nr >= ImageList->Count()) return false;
    B = ImageList->Pixmap(Nr).toImage().convertToFormat(QImage::Format_ARGB32);
    return !B.isNull();
}

// ImageList.AddImages(Source)
static void AddImages(TImageList *ImageList, TImageList *Source)
{
    if (Source == nullptr) return;
    for (int I = 0; I < Source->Count(); I++) ImageList->Add(Source->Pixmap(I));
}

static void ReplaceImage(TImageList *ImageList, int Nr, const QPixmap &P)
{
    if (Nr < 0 || Nr >= ImageList->Count()) return;
    ImageList->Replace(Nr, P);
}

// ---------------------------------------------------------------------------
// global
// ---------------------------------------------------------------------------

void ListOfIconSets(TStringList* ShortName, TStringList* LongName, TStringList* Author, TStringList* FolderName)
{
    auto AddRec = [&](const QString &Dir, const QString &Sub) {
        TIniFile Ini(Dir + IconSetsFolder + PathDelim + Sub + PathDelim + IconsConfFile);
        ShortName->Add(Sub);
        if (LongName != nullptr) LongName->Add(Ini.ReadString(QStringLiteral("Information"), QStringLiteral("Name"), Sub));
        if (Author != nullptr) Author->Add(Ini.ReadString(QStringLiteral("Information"), QStringLiteral("Author"), QString()));
        if (FolderName != nullptr) FolderName->Add(Dir + IconSetsFolder + PathDelim + Sub + PathDelim);
    };

    TSearchRec Rec;
    int I = FindFirst(PrgDataDir() + IconSetsFolder + PathDelim + QStringLiteral("*.*"), faDirectory, Rec);
    while (I == 0) {
        if (((Rec.Attr & faDirectory) != 0) && (Rec.Name != QLatin1String(".")) && (Rec.Name != QLatin1String("..")) &&
            FileExists(PrgDataDir() + IconSetsFolder + PathDelim + Rec.Name + PathDelim + IconsConfFile)) AddRec(PrgDataDir(), Rec.Name);
        I = FindNext(Rec);
    }
    FindClose(Rec);

    if (PrgDir() == PrgDataDir()) return;

    I = FindFirst(PrgDir() + IconSetsFolder + PathDelim + QStringLiteral("*.*"), faDirectory, Rec);
    while (I == 0) {
        if (((Rec.Attr & faDirectory) != 0) && (Rec.Name != QLatin1String(".")) && (Rec.Name != QLatin1String("..")) &&
            FileExists(PrgDir() + IconSetsFolder + PathDelim + Rec.Name + PathDelim + IconsConfFile)) {
            bool B = true; QString S = ExtUpperCase(Rec.Name);
            for (int J = 0; J < ShortName->Count(); J++) if (ExtUpperCase((*ShortName)[J]) == S) { B = false; break; }
            if (B) AddRec(PrgDir(), Rec.Name);
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
}

QImage GetExampleImage(const QString& ShortName)
{
    TUserIconLoader TempUserIconLoader;
    TImageList *Original = UserIconLoader != nullptr ? UserIconLoader->GetOriginalOfMainIconList() : nullptr;
    TImageList ImageList(Original != nullptr ? Original->Width() : 16, Original != nullptr ? Original->Height() : 16);
    AddImages(&ImageList, Original);
    TempUserIconLoader.SetIconSet(ShortName);
    TempUserIconLoader.DirectLoad(&ImageList, QStringLiteral("Main"));

    QImage result(6 * ImageList.Width(), ImageList.Height(), QImage::Format_ARGB32);
    result.fill(Qt::white);
    QPainter P(&result);
    for (int I = 0; I <= 5; I++) {
        QImage B;
        if (!GetBitmap(&ImageList, I, B)) continue;
        // background color (color of the bottom left pixel / transparency) -> clWhite
        if (!HasTransparency(B)) {
            QRgb C = B.pixel(0, B.height() - 1);
            for (int Y = 0; Y < B.height(); Y++) for (int X = 0; X < B.width(); X++) if (B.pixel(X, Y) == C) B.setPixel(X, Y, qRgb(255, 255, 255));
        }
        P.drawImage(B.width() * I, 0, B);
    }
    P.end();
    return result;
}

// ---------------------------------------------------------------------------
// TUserIconLoader
// ---------------------------------------------------------------------------

TUserIconLoader::TUserIconLoader()
{
    FIconsLoaded = false;
    FIconSet = QString();
    DialogImageList = new TImageList();
    RegisterImageList(DialogImageList, QStringLiteral("Dialogs"), true);
    DialogImageLoaded = ImageListList[0].ImageLoaded;
}

TUserIconLoader::~TUserIconLoader()
{
    while (ImageListList.size() > 0) UnRegisterImageList(ImageListList[ImageListList.size() - 1].ImageList);
    delete DialogImageList;
}

void TUserIconLoader::RegisterImageList(TImageList* ImageList, const QString& Name, bool AddImages)
{
    TImageListRec R;
    R.ImageList = ImageList;
    R.Name = Name;
    R.AddImages = AddImages;
    R.OriginalImageList = new TImageList(ImageList->Width(), ImageList->Height());
    ::AddImages(R.OriginalImageList, ImageList);
    R.IconsFromPath = nullptr;
    R.NeedReinit = false;
    R.ImageLoaded = new TList();
    ImageListList.append(R);
    const int I = ImageListList.size() - 1;

    if (FIconsLoaded) {
        DirectLoad(ImageListList[I].ImageList, ImageListList[I].Name, ImageListList[I].AddImages);
        ImageListList[I].NeedReinit = true;
    }
}

void TUserIconLoader::RegisterImageList(TImageList* ImageList, const QString& Name, TStringList* IconsFromPath, int IconsFromPathDefault)
{
    for (int I = 0; I < IconsFromPath->Count(); I++) ImageList->Add(QPixmap::fromImage(BlankImage(ImageList->Width(), ImageList->Height())));

    TImageListRec R;
    R.ImageList = ImageList;
    R.Name = Name;
    R.AddImages = false;
    R.OriginalImageList = new TImageList(ImageList->Width(), ImageList->Height());
    ::AddImages(R.OriginalImageList, ImageList);
    R.IconsFromPath = new TStringList();
    R.IconsFromPath->AddStrings(*IconsFromPath);
    R.IconsFromPathDefault = IconsFromPathDefault;
    R.NeedReinit = false;
    R.ImageLoaded = new TList();
    ImageListList.append(R);
    const int I = ImageListList.size() - 1;

    if (FIconsLoaded) {
        DirectLoad(ImageListList[I].ImageList, ImageListList[I].Name, ImageListList[I].AddImages);
        LoadIconsFromPath(ImageList, IconsFromPath, IconsFromPathDefault);
        ImageListList[I].NeedReinit = true;
    }
}

void TUserIconLoader::UpdateIconsFromPath(TImageList* ImageList, TStringList* IconsFromPath)
{
    for (int I = 0; I < ImageListList.size(); I++) if (ImageListList[I].ImageList == ImageList) {
        int OldLength;
        if (ImageListList[I].IconsFromPath != nullptr) OldLength = ImageListList[I].IconsFromPath->Count(); else OldLength = 0;

        if (ImageListList[I].IconsFromPath == nullptr) ImageListList[I].IconsFromPath = new TStringList();
        ImageListList[I].IconsFromPath->Clear();
        ImageListList[I].IconsFromPath->AddStrings(*IconsFromPath);

        QPixmap B = QPixmap::fromImage(BlankImage(ImageList->Width(), ImageList->Height()));
        for (int J = OldLength + 1; J <= IconsFromPath->Count(); J++) {
            ImageListList[I].ImageList->Add(B);
            ImageListList[I].OriginalImageList->Add(B);
        }

        LoadIconsFromPath(ImageListList[I].ImageList, ImageListList[I].IconsFromPath, ImageListList[I].IconsFromPathDefault);
    }
}

void TUserIconLoader::SetIconSet(const QString& Value)
{
    if (FIconSet == Value) return;
    FIconSet = Value;
    LoadIcons();
}

void TUserIconLoader::UnRegisterImageList(TImageList* ImageList)
{
    for (int I = 0; I < ImageListList.size(); I++) if (ImageListList[I].ImageList == ImageList) {
        delete ImageListList[I].OriginalImageList;
        delete ImageListList[I].ImageLoaded;
        delete ImageListList[I].IconsFromPath;
        ImageListList.removeAt(I);
        return;
    }
}

void TUserIconLoader::DirectLoad(TImageList* ImageList, const QString& Name, bool AddImages, TList* ImageLoaded)
{
    if (!FIconSetPath.isEmpty()) {
        TIniFile Ini(FIconSetPath + IconsConfFile);
        TImageList IL;
        IL.Assign(*ImageList);
        LoadUserIconsForList(&IL, Name, AddImages, ImageLoaded, &Ini, FIconSetPath);
        ImageList->Assign(IL);
    }

    TIniFile Ini(PrgDataDir() + SettingsFolder + PathDelim + IconsConfFile);
    LoadUserIconsForList(ImageList, Name, AddImages, ImageLoaded, &Ini, PrgDataDir() + SettingsFolder + PathDelim);
}

void TUserIconLoader::DirectLoad(const QString& Name, int Nr, QPushButton* BitBtn)
{
    TImageList ImageList;
    TList List;
    DirectLoad(&ImageList, Name, true, &List);
    if ((Nr < 0) || (Nr >= ImageList.Count())) return;
    QImage B = GetButtonBitmap(&ImageList, &List, Nr);
    if (B.isNull()) return;
    if (!IsImageEmpty(B)) BitBtn->setIcon(QIcon(QPixmap::fromImage(B)));
}

void TUserIconLoader::AddEmptyImages(TImageList* ImageList, TList* ImageLoaded, int MaxNr)
{
    QPixmap B = QPixmap::fromImage(BlankImage(ImageList->Width(), ImageList->Height()));
    while (ImageList->Count() < MaxNr + 1) ImageList->Add(B);

    if (ImageLoaded == nullptr) return;
    while (ImageLoaded->size() < MaxNr + 1) ImageLoaded->append(nullptr);
}

QImage TUserIconLoader::CreateMask(const QImage& AB, bool Transparent)
{
    const QImage B = AB.convertToFormat(QImage::Format_ARGB32);
    QImage result(B.size(), QImage::Format_RGB32);
    QRgb C;
    if (Transparent) {
        C = qRgb(255, 255, 255);
    } else {
        C = B.pixel(0, B.height() - 1);
    }
    for (int X = 0; X < B.width(); X++) for (int Y = 0; Y < B.height(); Y++) {
        QRgb P = B.pixel(X, Y);
        bool Match = Transparent ? ((P | 0xFF000000u) == (C | 0xFF000000u) && qAlpha(P) == 255) : (P == C);
        if (Match) result.setPixel(X, Y, qRgb(255, 255, 255)); else result.setPixel(X, Y, qRgb(128, 128, 128));
    }
    return result;
}

bool TUserIconLoader::LoadIconToImageList(TImageList* ImageList, int Nr, const QString& FileName)
{
    if (!FileExists(FileName)) return false;
    QPixmap P = LoadImageFromFile(FileName);
    if (P.isNull()) return false;

    if (Trim(ExtUpperCase(ExtractFileExt(FileName))) == QLatin1String(".BMP")) {
        QImage B = P.toImage().convertToFormat(QImage::Format_ARGB32);
        QImage B2 = CreateMask(B);
        QPixmap M = MaskedPixmap(B, B2);
        // ImageList.Insert(Nr,..) + Delete(Nr+1); a Qt image list needs images of the list size
        if (M.width() != ImageList->Width() || M.height() != ImageList->Height())
            M = M.scaled(ImageList->Width(), ImageList->Height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        ReplaceImage(ImageList, Nr, M);
    } else {
        QImage B = P.toImage().convertToFormat(QImage::Format_ARGB32);
        if ((B.width() != ImageList->Width()) || (B.height() != ImageList->Height())) {
            QImage B2 = BlankImage(ImageList->Width(), ImageList->Height());
            ScaleImage(B, B2);
            B = B2;
        }
        // B.TransparentColor:=clWhite; B.Transparent:=True;
        QImage B2 = CreateMask(B, true);
        ReplaceImage(ImageList, Nr, MaskedPixmap(B, B2));
    }
    return true;
}

void TUserIconLoader::LoadUserIconsForList(TImageList* ImageList, const QString& IniSectionName, bool AddImages, TList* ImageLoaded, TIniFile* Ini, const QString& RelBase)
{
    TStringList St;
    TImageList IL;
    IL.Assign(*ImageList);
    Ini->ReadSection(IniSectionName, St);
    for (int I = 0; I < St.Count(); I++) {
        int J;
        if (!TryStrToInt(St[I], J)) continue;
        if (J < 1) continue;
        if (J > IL.Count()) {
            if (!AddImages) continue;
            AddEmptyImages(&IL, ImageLoaded, J - 1);
        }
        LoadIconToImageList(&IL, J - 1, MakeExtAbsPath(Ini->ReadString(IniSectionName, IntToStr(J), QString()), RelBase));
        if (AddImages && ImageLoaded != nullptr) {
            while (ImageLoaded->size() < J) ImageLoaded->append(nullptr);
            (*ImageLoaded)[J - 1] = reinterpret_cast<void *>(1);
        }
    }
    ImageList->Assign(IL);
}

void TUserIconLoader::LoadIconsFromPath(TImageList* ImageList, TStringList* IconsFromPath, int IconsFromPathDefault)
{
    if (IconsFromPath == nullptr) return;

    for (int I = 0; I < IconsFromPath->Count(); I++) {
        int Nr = ImageList->Count() - IconsFromPath->Count() + I;

        // Load default icon
        QImage B;
        if (GetBitmap(ImageList, IconsFromPathDefault, B)) {
            QImage Mask = CreateMask(B);
            ReplaceImage(ImageList, Nr, MaskedPixmap(B, Mask));
        }

        // load icon from path
        LoadIconToImageList(ImageList, Nr, (*IconsFromPath)[I]);
    }
}

void TUserIconLoader::LoadIcons()
{
    auto CheckDir = [this](const QString &Name) {
        if (DirectoryExists(PrgDataDir() + IconSetsFolder + PathDelim + Name)) { FIconSetPath = IncludeTrailingPathDelimiter(PrgDataDir() + IconSetsFolder + PathDelim + Name); return; }
        if (DirectoryExists(PrgDir() + IconSetsFolder + PathDelim + Name)) { FIconSetPath = IncludeTrailingPathDelimiter(PrgDir() + IconSetsFolder + PathDelim + Name); return; }
    };

    if (FIconSet.isEmpty()) FIconSet = QStringLiteral("Modern");

    FIconSetPath = QString();
    CheckDir(FIconSet);
    if (FIconSetPath.isEmpty()) CheckDir(QStringLiteral("Modern"));
    if (FIconSetPath.isEmpty()) CheckDir(QStringLiteral("Classic"));

    for (int I = 0; I < ImageListList.size(); I++) if (ImageListList[I].NeedReinit) {
        ImageListList[I].ImageList->Clear();
        if (!ImageListList[I].AddImages) ImageListList[I].ImageList->Assign(*ImageListList[I].OriginalImageList);
    }

    if (!FIconSetPath.isEmpty()) {
        TIniFile Ini(FIconSetPath + IconsConfFile);
        for (int I = 0; I < ImageListList.size(); I++)
            LoadUserIconsForList(ImageListList[I].ImageList, ImageListList[I].Name, ImageListList[I].AddImages, ImageListList[I].ImageLoaded, &Ini, FIconSetPath);
    }

    {
        TIniFile Ini(PrgDataDir() + SettingsFolder + PathDelim + IconsConfFile);
        for (int I = 0; I < ImageListList.size(); I++) {
            LoadUserIconsForList(ImageListList[I].ImageList, ImageListList[I].Name, ImageListList[I].AddImages, ImageListList[I].ImageLoaded, &Ini, PrgDataDir() + SettingsFolder + PathDelim);
            LoadIconsFromPath(ImageListList[I].ImageList, ImageListList[I].IconsFromPath, ImageListList[I].IconsFromPathDefault);
        }
    }

    for (int I = 0; I < ImageListList.size(); I++) ImageListList[I].NeedReinit = true;

    FIconsLoaded = true;
}

QImage TUserIconLoader::GetButtonBitmap(TImageList* ImageList, TList* ImageListLoaded, int Nr)
{
    if ((Nr < 0) || (Nr >= ImageList->Count())) return QImage();
    if ((Nr >= ImageListLoaded->size()) || ((*ImageListLoaded)[Nr] == nullptr)) return QImage();

    // The VCL glyph consisted of the image and its mask side by side; a Qt icon
    // carries the mask as alpha channel.
    QImage B1;
    if (!GetBitmap(ImageList, Nr, B1)) return QImage();
    QImage B2 = CreateMask(B1);
    return MaskedPixmap(B1, B2).toImage();
}

QImage TUserIconLoader::GetDialogButtonBitmap(int Nr)
{
    return GetButtonBitmap(DialogImageList, DialogImageLoaded, Nr);
}

bool TUserIconLoader::IsImageEmpty(const QImage& AB)
{
    // empty = image completely black (and therefore mask completely white) or completely transparent
    const QImage B = AB.convertToFormat(QImage::Format_ARGB32);
    bool AllTransparent = true, AllBlack = true;
    for (int Y = 0; Y < B.height(); Y++) {
        const QRgb *Line = reinterpret_cast<const QRgb *>(B.constScanLine(Y));
        for (int X = 0; X < B.width(); X++) {
            if (qAlpha(Line[X]) != 0) AllTransparent = false;
            if (qAlpha(Line[X]) != 255 || (Line[X] & 0xFFFFFF) != 0) AllBlack = false;
            if (!AllTransparent && !AllBlack) return false;
        }
    }
    return true;
}

void TUserIconLoader::DialogImage(int Nr, QToolButton* SpeedButton)
{
    QImage B = GetDialogButtonBitmap(Nr);
    if (B.isNull()) return;
    if (!IsImageEmpty(B)) SpeedButton->setIcon(QIcon(QPixmap::fromImage(B)));
}

void TUserIconLoader::DialogImage(int Nr, QPushButton* BitBtn)
{
    QImage B = GetDialogButtonBitmap(Nr);
    if (B.isNull()) return;
    if (!IsImageEmpty(B)) BitBtn->setIcon(QIcon(QPixmap::fromImage(B)));
}

void TUserIconLoader::DialogImage(int Nr, TImageList* ImageList, int NrInImageList)
{
    if ((Nr < 0) || (Nr >= DialogImageList->Count())) return;
    QImage B = GetDialogButtonBitmap(Nr);
    if (B.isNull()) return;
    QPixmap P = DialogImageList->Pixmap(Nr);
    if (P.width() != ImageList->Width() || P.height() != ImageList->Height())
        P = P.scaled(ImageList->Width(), ImageList->Height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    ImageList->Add(P);   // ImageList.AddImage(DialogImageList,Nr)
    if (IsImageEmpty(B)) return;
    // ImageList.Move(ImageList.Count-1,NrInImageList); ImageList.Delete(NrInImageList+1);
    if (NrInImageList < 0 || NrInImageList >= ImageList->Count() - 1) return;
    ImageList->Replace(NrInImageList, P);
    ImageList->Delete(ImageList->Count() - 1);
}

TImageList* TUserIconLoader::GetOriginalOfMainIconList()
{
    for (int I = 0; I < ImageListList.size(); I++) if (ExtUpperCase(ImageListList[I].Name) == QLatin1String("MAIN"))
        return ImageListList[I].OriginalImageList;
    return nullptr;
}

void IconLoaderUnit_Initialization()
{
    UserIconLoader = new TUserIconLoader();
}

void IconLoaderUnit_Finalization()
{
    delete UserIconLoader;
    UserIconLoader = nullptr;
}
