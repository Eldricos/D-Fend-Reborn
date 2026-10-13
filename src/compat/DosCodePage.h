// DOS code pages (437 = US, 850 = Western Europe): conversion of Unicode
// text to the bytes DOS programs inside DOSBox display and back.
#pragma once

#include <QByteArray>
#include <QString>

// Code pages with built-in tables; other values fall back to 437.
bool DosCodePageSupported(int CodePage);
// Characters without a representation in the code page become '?'.
QByteArray EncodeDosCodePage(const QString &S, int CodePage = 437);
QString DecodeDosCodePage(const QByteArray &B, int CodePage = 437);

// CharToOem / OemToChar for text that is written with the 8 bit ANSI code
// page (as the Delphi code did): CharToOemAnsi returns the string whose ANSI
// encoding is the OEM encoding of S. Windows uses the system OEM code page,
// other systems code page 437.
QString CharToOemAnsi(const QString &S);
QString OemAnsiToChar(const QString &S);
