// D-Fend Reborn addition - see ExeIconReader.h
#include "ExeIconReader.h"

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QPixmap>
#include <QtEndian>

#include <climits>

namespace {

constexpr quint32 RT_ICON_ = 3;
constexpr quint32 RT_GROUP_ICON_ = 14;
constexpr qint64 MaxFileSize = 512 * 1024 * 1024;

struct TSection { quint32 VirtualAddress, VirtualSize, RawSize, RawOffset; };

class TPEFile
{
public:
    explicit TPEFile(const QByteArray& Data) : D(Data) {}

    bool Init()
    {
        if (D.size() < 0x40 || U16(0) != 0x5A4D) return false;   // "MZ"
        const quint32 PE = U32(0x3C);
        if (!Has(PE, 24) || U32(PE) != 0x00004550) return false;   // "PE\0\0"
        const quint32 Coff = PE + 4;
        const int SectionCount = U16(Coff + 2);
        const quint32 OptSize = U16(Coff + 16);
        const quint32 Opt = Coff + 20;
        if (!Has(Opt, 2)) return false;
        quint32 DataDirs;
        switch (U16(Opt)) {
            case 0x10B: DataDirs = Opt + 96; break;    // PE32
            case 0x20B: DataDirs = Opt + 112; break;   // PE32+
            default: return false;
        }
        if (!Has(DataDirs + 2 * 8, 8) || DataDirs + 3 * 8 > Opt + OptSize) return false;
        ResourceRVA = U32(DataDirs + 2 * 8);   // IMAGE_DIRECTORY_ENTRY_RESOURCE
        if (ResourceRVA == 0) return false;
        quint32 S = Opt + OptSize;
        for (int I = 0; I < SectionCount; I++, S += 40) {
            if (!Has(S, 40)) return false;
            Sections.append({U32(S + 12), U32(S + 8), U32(S + 16), U32(S + 20)});
        }
        ResourceOffset = RVAToOffset(ResourceRVA);
        return ResourceOffset != 0;
    }

    // Entries of a resource directory (offset relative to the resource section):
    // id (or name offset with high bit) and the entry offset (high bit = sub directory)
    QList<QPair<quint32, quint32>> Entries(quint32 DirOffset) const
    {
        QList<QPair<quint32, quint32>> L;
        const quint32 Dir = ResourceOffset + DirOffset;
        if (!Has(Dir, 16)) return L;
        const int Count = U16(Dir + 12) + U16(Dir + 14);
        for (int I = 0; I < Count && I < 4096; I++) {
            const quint32 E = Dir + 16 + I * 8;
            if (!Has(E, 8)) break;
            L.append({U32(E), U32(E + 4)});
        }
        return L;
    }

    // Data of the first language of the resource Type/Id (Id == UINT_MAX: first one)
    QByteArray Resource(quint32 Type, quint32 Id) const
    {
        for (const auto& T : Entries(0)) {
            if (T.first != Type || !(T.second & 0x80000000u)) continue;
            for (const auto& N : Entries(T.second & 0x7FFFFFFFu)) {
                if (Id != UINT_MAX && N.first != Id) continue;
                quint32 Off = N.second;
                // language level (or directly a data entry)
                if (Off & 0x80000000u) {
                    const auto Langs = Entries(Off & 0x7FFFFFFFu);
                    if (Langs.isEmpty() || (Langs[0].second & 0x80000000u)) return QByteArray();
                    Off = Langs[0].second;
                }
                const quint32 Entry = ResourceOffset + Off;
                if (!Has(Entry, 16)) return QByteArray();
                const quint32 DataOffset = RVAToOffset(U32(Entry));
                const quint32 Size = U32(Entry + 4);
                if (DataOffset == 0 || !Has(DataOffset, Size)) return QByteArray();
                return D.mid(DataOffset, Size);
            }
        }
        return QByteArray();
    }

private:
    const QByteArray& D;
    QList<TSection> Sections;
    quint32 ResourceRVA = 0;
    quint32 ResourceOffset = 0;

    bool Has(quint64 Offset, quint64 Length) const { return Offset + Length <= quint64(D.size()); }
    quint16 U16(quint32 Offset) const { return Has(Offset, 2) ? qFromLittleEndian<quint16>(D.constData() + Offset) : 0; }
    quint32 U32(quint32 Offset) const { return Has(Offset, 4) ? qFromLittleEndian<quint32>(D.constData() + Offset) : 0; }

    quint32 RVAToOffset(quint32 RVA) const
    {
        for (const TSection& S : Sections) {
            const quint32 Size = qMax(S.VirtualSize, S.RawSize);
            if (RVA >= S.VirtualAddress && RVA < S.VirtualAddress + Size) {
                const quint32 Off = RVA - S.VirtualAddress;
                if (Off >= S.RawSize) return 0;
                return S.RawOffset + Off;
            }
        }
        return 0;
    }
};

// One RT_ICON resource as image: PNG data or a DIB, which is wrapped into a
// one-image .ico file for Qt's ico reader
QImage IconResourceToImage(const QByteArray& Data, const uchar* GroupEntry)
{
    if (Data.startsWith("\x89PNG")) return QImage::fromData(Data, "PNG");

    QByteArray Ico;
    Ico.reserve(6 + 16 + Data.size());
    const char Header[6] = {0, 0, 1, 0, 1, 0};   // reserved, type 1 (icon), 1 image
    Ico.append(Header, 6);
    Ico.append(reinterpret_cast<const char*>(GroupEntry), 12);   // width ... bytes in resource
    char Offset[4];
    qToLittleEndian<quint32>(6 + 16, Offset);
    Ico.append(Offset, 4);
    Ico.append(Data);

    QBuffer Buffer(&Ico);
    Buffer.open(QIODevice::ReadOnly);
    QImageReader Reader(&Buffer, "ico");
    return Reader.read();
}

} // namespace

QIcon ReadExeFileIcon(const QString& FileName)
{
    QFile F(FileName);
    if (!F.open(QIODevice::ReadOnly) || F.size() > MaxFileSize) return QIcon();
    const QByteArray Data = F.readAll();

    TPEFile PE(Data);
    if (!PE.Init()) return QIcon();

    // GRPICONDIR: reserved, type, count, then 14 byte entries
    // (width, height, colors, reserved, planes, bit count, bytes in resource, id)
    const QByteArray Group = PE.Resource(RT_GROUP_ICON_, UINT_MAX);
    if (Group.size() < 6) return QIcon();
    const int Count = qFromLittleEndian<quint16>(Group.constData() + 4);

    QIcon Result;
    for (int I = 0; I < Count; I++) {
        const int E = 6 + I * 14;
        if (E + 14 > Group.size()) break;
        const uchar* Entry = reinterpret_cast<const uchar*>(Group.constData() + E);
        const quint16 Id = qFromLittleEndian<quint16>(Entry + 12);
        const QByteArray Icon = PE.Resource(RT_ICON_, Id);
        if (Icon.isEmpty()) continue;
        const QImage Img = IconResourceToImage(Icon, Entry);
        if (!Img.isNull()) Result.addPixmap(QPixmap::fromImage(Img));
    }
    return Result;
}
