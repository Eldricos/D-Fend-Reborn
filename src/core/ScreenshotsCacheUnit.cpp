// C++ port of ScreenshotsCacheUnit.pas (implementation section).
#include "ScreenshotsCacheUnit.h"

#include "ImageStretch.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "CommonTools.h"

#include <QBuffer>
#include <QImageReader>
#include <QtEndian>

// {$DEFINE DebugThumbnailCache}

TScreenshotsCache* ScreenshotsCache = nullptr;

// TThumbnail
//
// File format (identical to the Delphi version): Integer length + ANSI name,
// Integer Width, Integer Height, TDateTime Date, Integer ImageSize,
// Integer compressed size + zlib compressed image data.  The Delphi version
// stored BMP data, the port stores PNG data (keeps transparency); both are
// detected automatically when reading.

class TThumbnail
{
public:
    QString Name;
    int Width = 0, Height = 0;
    TDateTime Date = 0;
    TThumbnail() {}
    ~TThumbnail() {}
    void Load(QDataStream &Stream);
    void Save(QDataStream &Stream);
    QImage Bitmap() { return GetImage(); }
    void SetBitmap(const QImage &B) { SetImage(B); }
private:
    QByteArray Image, ImageCompressed;
    bool ImageValid = false;
    int ImageSize = 0;
    void SetImage(const QImage &B);
    QImage GetImage();
};

static qint32 ReadInt(QDataStream &Stream)
{
    qint32 I = 0;
    Stream >> I;
    if (Stream.status() != QDataStream::Ok) throw Exception(QStringLiteral("Read error"));
    return I;
}

static QByteArray ReadBytes(QDataStream &Stream, int Count)
{
    if (Count < 0) throw Exception(QStringLiteral("Read error"));
    QByteArray B(Count, Qt::Uninitialized);
    if (Count > 0 && Stream.readRawData(B.data(), Count) != Count) throw Exception(QStringLiteral("Read error"));
    return B;
}

void TThumbnail::Load(QDataStream &Stream)
{
    int I = ReadInt(Stream); Name = DecodeAnsi(ReadBytes(Stream, I));
    Width = ReadInt(Stream);
    Height = ReadInt(Stream);
    double D = 0; Stream >> D; if (Stream.status() != QDataStream::Ok) throw Exception(QStringLiteral("Read error"));
    Date = D;
    ImageSize = ReadInt(Stream);
    I = ReadInt(Stream); ImageCompressed = ReadBytes(Stream, I);
}

void TThumbnail::Save(QDataStream &Stream)
{
    QByteArray N = EncodeAnsi(Name);
    Stream << qint32(N.size()); Stream.writeRawData(N.constData(), N.size());
    Stream << qint32(Width);
    Stream << qint32(Height);
    Stream << double(Date);
    Stream << qint32(ImageSize);
    Stream << qint32(ImageCompressed.size()); Stream.writeRawData(ImageCompressed.constData(), ImageCompressed.size());
}

QImage TThumbnail::GetImage()
{
    if (!ImageValid) {
        // TDecompressionStream: qUncompress expects the uncompressed size as big endian prefix
        QByteArray Data(4, 0);
        qToBigEndian<quint32>(quint32(ImageSize), Data.data());
        Data += ImageCompressed;
        Image = qUncompress(Data);
        ImageValid = true;
    }

    QImage result;
    result.loadFromData(Image);
    return result;
}

void TThumbnail::SetImage(const QImage &B)
{
    Image.clear();
    QBuffer Buffer(&Image);
    Buffer.open(QIODevice::WriteOnly);
    B.save(&Buffer, "PNG");
    Buffer.close();
    ImageValid = true;

    ImageSize = Image.size();
    // TCompressionStream(clFastest): zlib stream = qCompress without the 4 byte size prefix
    ImageCompressed = qCompress(Image, 1).mid(4);
}

// TScreenshotsCache

TScreenshotsCache::TScreenshotsCache()
{
    Thumbnails = new TList();
    DataLoaded = false;
    ThumbnailCacheSize = 500;
}

TScreenshotsCache::~TScreenshotsCache()
{
    if (DataLoaded) SaveData();
    for (int I = 0; I < Thumbnails->size(); I++) delete static_cast<TThumbnail *>((*Thumbnails)[I]);
    delete Thumbnails;
}

QString TScreenshotsCache::GetCacheFile()
{
    return PrgDataDir() + SettingsFolder + PathDelim + ScreenshotsCacheFileName;
}

void TScreenshotsCache::LoadData()
{
    ThumbnailCacheSize = PrgSetup->ThumbnailCacheSize();
    if (!PrgSetup->BinaryCache()) return;
    if (!FileExists(CacheFile())) return;

    QFile F(CacheFile());
    if (!F.open(QIODevice::ReadOnly)) return;
    QDataStream Stream(&F);
    Stream.setByteOrder(QDataStream::LittleEndian);
    Stream.setFloatingPointPrecision(QDataStream::DoublePrecision);

    try {
        const QByteArray Version = CacheVersionString.toLatin1();
        if (F.size() < Version.size()) return;
        if (ReadBytes(Stream, Version.size()) != Version) return;

        while (!Stream.atEnd()) {
            TThumbnail *Thumbnail = new TThumbnail();
            try {
                Thumbnail->Load(Stream);
                Thumbnails->append(Thumbnail);
            } catch (...) {
                delete Thumbnail; throw;
            }
        }
#ifdef DebugThumbnailCache
        ShowMessage(IntToStr(Thumbnails->size()) + QStringLiteral(" cached thumbnails loaded"));
#endif
    } catch (...) {
        return;
    }
}

void TScreenshotsCache::SaveData()
{
    QFile F(CacheFile());
    if (!F.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    QDataStream Stream(&F);
    Stream.setByteOrder(QDataStream::LittleEndian);
    Stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
    const QByteArray S = CacheVersionString.toLatin1();
    Stream.writeRawData(S.constData(), S.size());
    for (int I = Max(0, int(Thumbnails->size()) - ThumbnailCacheSize); I < Thumbnails->size(); I++)
        static_cast<TThumbnail *>((*Thumbnails)[I])->Save(Stream);
}

static void CalcWH(int OldW, int OldH, int MaxW, int MaxH, int &NewW, int &NewH)
{
    double F = double(OldW) / OldH;

    if ((OldW <= MaxW) && (OldH <= MaxH)) {
        // too small scale up
        if (MaxH * F > MaxW) { NewW = MaxW; NewH = Round(NewW / F); } else { NewH = MaxH; NewW = Round(NewH * F); }
        return;
    }

    // scale down
    if (OldW > MaxW) {
        // too wide
        NewW = MaxW; NewH = Round(NewW / F);
        // also to high ?
        if (NewH > MaxH) { NewH = MaxH; NewW = Round(NewH * F); }
        return;
    }

    // too high
    NewH = MaxH; NewW = Round(NewH * F);
    // also too wide ?
    if (NewW > MaxW) { NewW = MaxW; NewH = Round(NewW / F); }
}

static QImage NewBitmap(int Width, int Height)
{
    QImage result(qMax(1, Width), qMax(1, Height), QImage::Format_ARGB32);
    result.fill(Qt::transparent);
    return result;
}

QImage TScreenshotsCache::CalcThumbnail(const QString& ScreenshotFile, int Width, int Height)
{
    QString S = ExtUpperCase(ExtractFileExt(ScreenshotFile));

    // TPNGObject / TJPEGImage / TGIFImage / TBitmap
    if ((S == QLatin1String(".PNG")) || (S == QLatin1String(".JPG")) || (S == QLatin1String(".JPEG")) || (S == QLatin1String(".GIF")) || (S == QLatin1String(".BMP"))) {
        QImageReader Reader(ScreenshotFile);
        Reader.setDecideFormatFromContent(true);
        QImage B = Reader.read();
        if (B.isNull() || B.width() <= 0 || B.height() <= 0) return QImage();
        QImage result = NewBitmap(Width, Height);
        int NewW, NewH;
        CalcWH(B.width(), B.height(), Width, Height, NewW, NewH);
        ScaleImage(B, result, NewW, NewH);
        return result;
    }

    return QImage();
}

QImage TScreenshotsCache::GetThumbnail(const QString& ScreenshotFile, int Width, int Height)
{
    // Check if screenshot file exists
    if (!FileExists(ScreenshotFile)) return NewBitmap(Width, Height);

    if (!DataLoaded) {
        DataLoaded = true;
        LoadData();
    }

    TDateTime D = GetFileDate(ScreenshotFile);
    QString S = ExtractFileName(ScreenshotFile);

    // Try to find cache record
    for (int I = 0; I < Thumbnails->size(); I++) {
        TThumbnail *T = static_cast<TThumbnail *>((*Thumbnails)[I]);
        if ((T->Name != S) || (T->Date != D) || (T->Width != Width) || (T->Height != Height)) continue;
        QImage result = T->Bitmap();
        if (!result.isNull()) return result;
    }

    // Create Thumbnail and Cache it
    QImage result = CalcThumbnail(ScreenshotFile, Width, Height);
    if (!result.isNull()) {
        TThumbnail *T = new TThumbnail();
        T->Name = S; T->Date = D; T->Width = Width; T->Height = Height;
        T->SetBitmap(result);
        Thumbnails->append(T);
    } else {
#ifdef DebugThumbnailCache
        ShowMessage(QStringLiteral("Error: Cannot process ") + ScreenshotFile);
#endif
    }
    return result;
}

void ScreenshotsCacheUnit_Initialization()
{
    ScreenshotsCache = new TScreenshotsCache();
}

void ScreenshotsCacheUnit_Finalization()
{
    delete ScreenshotsCache;
    ScreenshotsCache = nullptr;
}
