// C++ port of ImageTools.pas (implementation section).
#include "ImageTools.h"

#include "PrgSetupUnit.h"
#include "CommonTools.h"

// Note: ShortName (declared in ImageTools.h) is implemented in DosBoxUnit.cpp,
// which declares the identical routine.

QString GetGeometryFromFile(const QString& FileName)
{
    QFile F(FileName);
    if (!F.open(QIODevice::ReadOnly)) return QString();
    qint64 LoI = F.size();
    F.close();
    LoI = LoI / 512 / 63 / 16;
    return QStringLiteral("512,63,16,") + IntToStr(LoI);
}

QString GetGemoetryFromMB(int Size)
{
    /* Correct way for calculating the size (512*63*16*X=Bytes => X=MBytes*128/63)
    but not the way mkdosfs works:
    I:=Size*128 div 63;
    result:='512,63,16,'+IntToStr(I); */

    // mkdosfs simply does this:
    return QStringLiteral("512,63,16,") + IntToStr(Size * 2);
}

static bool SearchCueSheetKeyWords(const QString& FileName)
{
    static const char *KeyWords[13] = {"CATALOG", "CDTEXTFILE", "FILE", "FLAGS", "INDEX", "ISRC", "PERFORMER", "POSTGAP", "PREGAP", "REM", "SONGWRITER", "TITLE", "TRACK"};
    QFile FSt(FileName);
    if (!FSt.open(QIODevice::ReadOnly)) return false;
    if (FSt.size() > 500 * 1024) return false;
    TStringList St;
    St.SetText(DecodeAnsi(FSt.readAll()));
    for (int I = 0; I <= Min(50, St.Count() - 1); I++) {
        QString S = ExtUpperCase(St[I]);
        for (const char *K : KeyWords) if (Pos(QString::fromLatin1(K), S) > 0) return true;
    }
    return false;
}

static bool CheckISOImage(const QString& FileName)
{
    static const QByteArray ISOID = QByteArray("\x01" "CD001" "\x01", 7);
    QFile FSt(FileName);
    if (!FSt.open(QIODevice::ReadOnly)) return false;
    if (FSt.size() < 0x9400) return false;
    const qint64 Positions[4] = {0x8000, 0x8018, 0x9310, 0x9318};
    for (qint64 P : Positions) {
        FSt.seek(P);
        if (FSt.read(7) == ISOID) return true;
    }
    return false;
}

bool CheckCDImage(const QString& FileName)
{
    if (!FileExists(FileName)) return false;

    QString Ext = ExtUpperCase(ExtractFileExt(FileName));
    if (Ext == QLatin1String(".BIN")) return true;
    if (Ext == QLatin1String(".CUE")) return SearchCueSheetKeyWords(FileName);
    if (Ext == QLatin1String(".ISO")) return CheckISOImage(FileName);

    if (SearchCueSheetKeyWords(FileName)) return true;
    return CheckISOImage(FileName);
}
