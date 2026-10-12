// C++ port of HashCalc.pas (implementation section).
// The original wrapped THashAlgMD5 (HashAlgMD5_U / SDUGeneral); QCryptographicHash
// is used instead.  The result format is identical: 32 upper case hex digits
// (THashValue.ValueAsASCIIHex uses IntToHex).
#include "HashCalc.h"

#include <QCryptographicHash>
#include <QFile>

QString GetMD5Sum(const QString &FileName, bool SmallFile)
{
    QString result = "";
    if ((Trim(FileName) == "") || (!FileExists(FileName))) return result;

    QFile F(FileName);
    if (!F.open(QIODevice::ReadOnly)) return result;

    QCryptographicHash HashAlgMD5(QCryptographicHash::Md5);
    if (SmallFile) {
        // MemoryStream.LoadFromFile
        QByteArray Data = F.readAll();
        HashAlgMD5.addData(Data);
    } else {
        // read from the file stream
        if (!HashAlgMD5.addData(&F)) return result;
    }
    F.close();

    result = QString::fromLatin1(HashAlgMD5.result().toHex()).toUpper();
    return result;
}
