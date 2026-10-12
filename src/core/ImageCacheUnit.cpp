// C++ port of ImageCacheUnit.pas (implementation section).
#include "ImageCacheUnit.h"

#include "CommonTools.h"
#include "ImageStretch.h"

TImageCache* IconCache = nullptr;
TImageCache* PictureCache = nullptr;

// TImageData

TImageData::TImageData()
{
}

TImageData::~TImageData()
{
}

bool TImageData::LoadIcon(const QString& AFileName)
{
    FIcon = LoadIconFromExeFile(AFileName);
    bool result = !FIcon.isNull();
    if (result) FPath = IncludeTrailingPathDelimiter(ExtUpperCase(ExtractFilePath(AFileName)));
    return result;
}

bool TImageData::LoadIcon(const QIcon& AIcon, const QString& AFileName)
{
    FIcon = AIcon;
    FPath = IncludeTrailingPathDelimiter(ExtUpperCase(ExtractFilePath(AFileName)));
    return true;
}

bool TImageData::LoadPicture(const QString& AFileName)
{
    if (!FileExists(AFileName)) return false;
    FPicture = LoadImageFromFile(AFileName);
    if (FPicture.isNull()) return false;
    FPath = IncludeTrailingPathDelimiter(ExtUpperCase(ExtractFilePath(AFileName)));
    return true;
}

// TImageCache

TImageCache::TImageCache()
{
    FImageList = new TStringList();
    FImageList->SetSorted(true);
    FImageList->SetDuplicates(dupAccept);
}

TImageCache::~TImageCache()
{
    Clear();
    delete FImageList;
}

int TImageCache::LoadImageAsIcon(const QString& AFileName)
{
    QIcon I = LoadImageAsIconFromFile(AFileName); if (I.isNull()) return -1;
    TImageData *AImageData = new TImageData();
    if (!AImageData->LoadIcon(I, AFileName)) { delete AImageData; return -1; }
    return FImageList->AddObject(ExtUpperCase(ExtractFileName(AFileName)), AImageData);
}

int TImageCache::LoadIcon(const QString& AFileName)
{
    QString Ext = ExtUpperCase(ExtractFileExt(AFileName));
    if ((Ext != QLatin1String(".ICO")) && (Ext != QLatin1String(".EXE"))) return LoadImageAsIcon(AFileName);

    TImageData *AImageData = new TImageData();
    if (!AImageData->LoadIcon(AFileName)) { delete AImageData; return -1; }
    return FImageList->AddObject(ExtUpperCase(ExtractFileName(AFileName)), AImageData);
}

int TImageCache::LoadPicture(const QString& AFileName)
{
    TImageData *AImageData = new TImageData();
    if (!AImageData->LoadPicture(AFileName)) { delete AImageData; return -1; }
    return FImageList->AddObject(ExtUpperCase(ExtractFileName(AFileName)), AImageData);
}

int TImageCache::FindFile(const QString& AFileName)
{
    QString S = ExtUpperCase(ExtractFileName(AFileName));
    QString T = IncludeTrailingPathDelimiter(ExtUpperCase(ExtractFilePath(AFileName)));

    int MinPos = FImageList->IndexOf(S);
    if (MinPos < 0) return -1;
    while ((MinPos > 0) && ((*FImageList)[MinPos - 1] == S)) MinPos--;
    for (int I = MinPos; I < FImageList->Count(); I++)
        if (((*FImageList)[I] == S) && (FImageList->ObjectAs<TImageData>(I)->Path() == T)) return I;
    return -1;
}

QIcon TImageCache::GetIcon(const QString& AFileName)
{
    int Nr = FindFile(AFileName);
    if (Nr < 0) {
        Nr = LoadIcon(AFileName);
        if (Nr < 0) return QIcon();
    }

    return FImageList->ObjectAs<TImageData>(Nr)->Icon();
}

QPixmap TImageCache::GetPicture(const QString& AFileName)
{
    int Nr = FindFile(AFileName);
    if (Nr < 0) {
        Nr = LoadPicture(AFileName);
        if (Nr < 0) return QPixmap();
    }

    return FImageList->ObjectAs<TImageData>(Nr)->Picture();
}

void TImageCache::Clear()
{
    for (int I = 0; I < FImageList->Count(); I++) delete FImageList->ObjectAs<TImageData>(I);
    FImageList->Clear();
}

void ImageCacheUnit_Initialization()
{
    IconCache = new TImageCache();
    PictureCache = new TImageCache();
}

void ImageCacheUnit_Finalization()
{
    delete IconCache; IconCache = nullptr;
    delete PictureCache; PictureCache = nullptr;
}
