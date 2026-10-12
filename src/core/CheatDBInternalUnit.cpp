// C++ port of CheatDBInternalUnit.pas (implementation section).
#include "CheatDBInternalUnit.h"

#include "CheatDBInternalPrivateerPositionUnit.h"
#include "LanguageSetupUnit.h"

#include <QtEndian>

// --- TFileStream helpers -----------------------------------------------------

static void ReadByte(QFile* St, quint8& B)
{
    char C;
    if (St->read(&C, 1) == 1) B = static_cast<quint8>(C);
}

static void WriteByte(QFile* St, quint8 B)
{
    const char C = static_cast<char>(B);
    St->write(&C, 1);
}

static void WriteLongint(QFile* St, qint32 L)
{
    char Buf[4];
    qToLittleEndian<qint32>(L, Buf);
    St->write(Buf, 4);
}

// Pascal Val(S, V, Code) for integer targets: Code = 0 on success, otherwise
// the (1-based) position of the first invalid character.  The result is
// assigned to the (possibly smaller) target type by truncation like Delphi.
static int Val(const QString& S, qint64& V)
{
    int I = 0;
    const int L = S.length();
    while (I < L && S[I] == ' ') I++;
    bool Neg = false;
    if (I < L && (S[I] == '+' || S[I] == '-')) { Neg = (S[I] == '-'); I++; }
    int Base = 10;
    if (I < L && S[I] == '$') { Base = 16; I++; }
    else if (I + 1 < L && S[I] == '0' && (S[I + 1] == 'x' || S[I + 1] == 'X')) { Base = 16; I += 2; }
    if (I >= L) { V = 0; return I + 1; }
    qint64 R = 0;
    for (; I < L; I++) {
        const int D = QString(S[I]).toInt(nullptr, 16);
        const bool IsDigit = (Base == 10) ? S[I].isDigit() : (S[I].isDigit() || (S[I].toUpper() >= 'A' && S[I].toUpper() <= 'F'));
        if (!IsDigit) { V = Neg ? -R : R; return I + 1; }
        R = R * Base + D;
    }
    V = Neg ? -R : R;
    return 0;
}

static void Val(const QString& S, quint8& X, int& Code) { qint64 V = 0; Code = Val(S, V); X = static_cast<quint8>(V); }
static void Val(const QString& S, quint16& X, int& Code) { qint64 V = 0; Code = Val(S, V); X = static_cast<quint16>(V); }

static QString Str(int X) { return QString::number(X); }

static QWidget* ApplicationMainForm()
{
    for (QWidget* W : QApplication::topLevelWidgets()) if (qobject_cast<QMainWindow*>(W) != nullptr) return W;
    return QApplication::activeWindow();
}

// --- implementation ----------------------------------------------------------

static int SearchString(const QByteArray& S, QFile* St)
{
    quint8 C = 0;
    quint8 Nr = 0;
    int L = -1;
    int result = -1;
    while (St->pos() < St->size()) {
        ReadByte(St, C);
        if (static_cast<char>(C) == S[Nr]) {
            if (Nr == 0) L = St->pos() - 1;
            Nr++;
            if (Nr == S.length()) { result = L; return result; }
        } else {
            Nr = 0;
        }
    }
    return result;
}

static int GetNumber(quint16 W, QFile* St)
{
    quint8 B[3]; // B[1..2]
    int result = -1;
    B[1] = 255; B[2] = 0;
    while (St->pos() < St->size()) {
        B[2] = B[1];
        ReadByte(St, B[1]);
        if (static_cast<quint16>(B[1] | (B[2] << 8)) == W) { result = St->pos() - 2; return result; }
    }
    return result;
}

static bool DarkSideOfXeenPosition(QFile* St)
{
    quint8 X = 0, B = 0, Y = 0;
    QString S, T, U, V;
    int I;
    bool H;
    bool result = false;
    St->seek(0x3788); ReadByte(St, X); ReadByte(St, B); S = Str(X); T = Str(B);
    St->seek(0x378A); ReadByte(St, Y);
    H = false;
    if ((Y > 112) || ((Y > 24) && (Y < 89))) {
        V = Str(Y);
        U = "(" + V + ")";
    } else {
        if (Y >= 88) {
            H = true;
            Y = Y - 88;
        }
        V = Str(Y % 4);
        if (V == "0") { V = "4"; Y = Y - 1; }
        U = "?";
        switch (Y / 4) {
            case 0: U = "A"; break;
            case 1: U = "B"; break;
            case 2: U = "C"; break;
            case 3: U = "D"; break;
            case 4: U = "E"; break;
            case 5: U = "F"; break;
        }
        U = U + V;
    }

    if (!InputQuery(LanguageSetup->EditCheatsInternalDarkSideOfXeenPositionX(), LanguageSetup->EditCheatsInternalDarkSideOfXeenPositionPrompt(), S)) return result;
    Val(S, X, I); if (I != 0) { MessageDlg(LanguageSetup->EditCheatsInternalDarkSideOfXeenPositionInvalid(), mtError, mbOK, 0); return result; }
    if (!InputQuery(LanguageSetup->EditCheatsInternalDarkSideOfXeenPositionY(), LanguageSetup->EditCheatsInternalDarkSideOfXeenPositionPrompt(), T)) return result;
    Val(T, B, I); if (I != 0) { MessageDlg(LanguageSetup->EditCheatsInternalDarkSideOfXeenPositionInvalid(), mtError, mbOK, 0); return result; }
    if (!InputQuery(LanguageSetup->EditCheatsInternalDarkSideOfXeenPositionQuadrant(), LanguageSetup->EditCheatsInternalDarkSideOfXeenPositionPrompt(), U)) return result;

    if ((Length(U) > 2) && (U[0] == '(') && (U[U.length() - 1] == ')')) {
        Val(Copy(U, 2, Length(U) - 2), Y, I);
        if ((I != 0) || (Y > 112)) { MessageDlg(LanguageSetup->EditCheatsInternalDarkSideOfXeenPositionInvalid(), mtError, mbOK, 0); return result; }
    } else {
        const QChar U1 = (U.length() >= 1) ? U[0].toUpper() : QChar(0);
        if (U1 == 'A') Y = 0;
        else if (U1 == 'B') Y = 4;
        else if (U1 == 'C') Y = 8;
        else if (U1 == 'D') Y = 12;
        else if (U1 == 'E') Y = 16;
        else if (U1 == 'F') Y = 20;
        else { MessageDlg(LanguageSetup->EditCheatsInternalDarkSideOfXeenPositionInvalid(), mtError, mbOK, 0); return result; }
        const QChar U2 = (U.length() >= 2) ? U[1] : QChar(0);
        if (U2 == '1') Y += 1;
        else if (U2 == '2') Y += 2;
        else if (U2 == '3') Y += 3;
        else if (U2 == '4') Y += 4;
        else { MessageDlg(LanguageSetup->EditCheatsInternalDarkSideOfXeenPositionInvalid(), mtError, mbOK, 0); return result; }
        if (H)
            H = (MessageDlg(LanguageSetup->EditCheatsInternalDarkSideOfXeenPositionSkySet(), mtConfirmation, mbYes | mbNo, 0) == mrYes);
        else
            H = (MessageDlg(LanguageSetup->EditCheatsInternalDarkSideOfXeenPositionSkyNotSet(), mtConfirmation, mbYes | mbNo, 0) == mrYes);
        if (H) Y += 88;
    }
    result = true;
    St->seek(0x3788);
    WriteByte(St, X); WriteByte(St, B);
    St->seek(0x378A);
    WriteByte(St, Y);
    return result;
}

/* (An older, commented out version of DarkSideOfXeenPosition is contained in
   the Delphi source and was not ported.) */

static bool PrivateerMoney(QFile* St)
{
    quint8 X, B;
    bool result = false;
    if (SearchString(QByteArray("CCRGI\0\0\0\x08", 9), St) < 0) {
        MessageDlg(LanguageSetup->EditCheatsInternalPrivateerMoneyNotFound(), mtError, mbOK, 0); return result;
    }
    X = 0xFF; B = 0x5F; result = true;
    WriteByte(St, X); WriteByte(St, X);
    WriteByte(St, B); WriteByte(St, B);
    return result;
}

static bool SimCity2000Money(QFile* St)
{
    quint8 B;
    QString S;
    int I;
    quint16 W = 0;
    int L;
    bool result = false;
    S = "";
    if (!InputQuery(LanguageSetup->EditCheatsInternalSimCity2000(), LanguageSetup->EditCheatsInternalSimCity2000Prompt(), S)) return result;
    Val(S, W, I);
    if ((I != 0) || (W == 0)) {
        MessageDlg(LanguageSetup->EditCheatsInternalSimCity2000InvalidValue(), mtError, mbOK, 0); return result;
    }
    L = GetNumber(W, St); if (L < 0) return result;
    St->seek(L - 2); B = 0x7F; WriteByte(St, B);
    St->seek(L); B = 0xFF; WriteByte(St, B);
    St->seek(L + 1); B = 0xFF; WriteByte(St, B);
    result = true;
    return result;
}

static void BetrayalAtKrondorCharacters(QFile* St)
{
    quint8 B = 0;
    quint16 W;
    int L;
    L = 0xE3;
    for (int Y = 0; Y <= 5; Y++) {
        W = static_cast<quint16>(L + (Y * 95));
        St->seek(W); B = 255; WriteByte(St, B); WriteByte(St, B); WriteByte(St, B);
        St->seek(W + 5); B = 255; WriteByte(St, B); WriteByte(St, B); WriteByte(St, B);
        St->seek(W + 10); B = 0x7F; WriteByte(St, B); WriteByte(St, B); WriteByte(St, B);
        St->seek(W + 15); B = 0x7F; WriteByte(St, B); WriteByte(St, B); WriteByte(St, B);
        for (int X = 1; X <= 12; X++) {
            St->seek(W + 15 + (X * 5)); ReadByte(St, B); if (B != 0) {
                St->seek(W + 15 + (X * 5)); B = 0x64; WriteByte(St, B); WriteByte(St, B); WriteByte(St, B);
            }
        }
    }
}

static void AdvantedTennisParameters(QFile* St)
{
    quint16 W;
    qint64 L;
    quint8 A[61]; // A[1..60]
    St->seek(0x1CF2);
    while (St->pos() < St->size()) {
        L = St->pos();
        const qint64 R = St->read(reinterpret_cast<char*>(&A[1]), 60);
        W = static_cast<quint16>(R < 0 ? 0 : R);
        if (W < 60) break;
        if (A[34] == 137) {
            for (int X = 1; X <= 6; X++) A[41 + (X * 2)] = 255;
            // Literal translation: the Delphi code reads the block again here
            // (ReadBuffer instead of WriteBuffer), so the modification is discarded.
            St->seek(L); St->read(reinterpret_cast<char*>(&A[1]), 60);
        }
    }
}

static void PatchCC1(QFile* St)
{
    qint32 L;
    int I;
    L = 2000000;
    St->seek(0x580); WriteLongint(St, L);
    St->seek(0x584); WriteLongint(St, L);
    I = SearchString(QByteArray("Computer"), St) - 0x46;
    if (I >= 0) { St->seek(I); WriteLongint(St, L); }
}

static void UfoBuildingTime(QFile* St)
{
    quint8 B = 0;
    for (int X = 1; X <= 36; X++) {
        St->seek(0xD10 + X); ReadByte(St, B);
        if ((B != 0) && (B != 255)) { St->seek(0xD10 + X); B = 1; WriteByte(St, B); }
    }
}

static bool PrivateerShip(QFile* St)
{
    quint8 X = 0, B = 0;
    QString S;
    int I;
    bool result = false;
    S = "";
    if (!InputQuery(LanguageSetup->EditCheatsInternalPrivateerShipMissions(), LanguageSetup->EditCheatsInternalPrivateerShipMissionsPrompt(), S)) return result;
    Val(S, B, I);
    if ((I != 0) || (B > 3)) { MessageDlg(LanguageSetup->ApplyCheatInvalidValue(), mtError, mbOK, 0); return result; }
    St->seek(0x28 + (B * 8)); ReadByte(St, X); S = Str(X);
    if (!InputQuery(LanguageSetup->EditCheatsInternalPrivateerShip(), LanguageSetup->EditCheatsInternalPrivateerShipPrompt(), S)) return result;
    Val(S, X, I);
    if ((I != 0) || (X > 4)) { MessageDlg(LanguageSetup->ApplyCheatInvalidValue(), mtError, mbOK, 0); return result; }
    St->seek(0x28 + (B * 8)); WriteByte(St, X); result = true;
    return result;
}

static bool PrivateerPosition(QWidget* AOwner, QFile* St)
{
    quint8 X = 0, B = 0;
    QString S;
    int I;
    bool result = false;
    S = "";
    if (!InputQuery(LanguageSetup->EditCheatsInternalPrivateerPositionMissions(), LanguageSetup->EditCheatsInternalPrivateerPositionMissionsPrompt(), S)) return result;
    Val(S, B, I);
    if ((I != 0) || (B > 3)) { MessageDlg(LanguageSetup->ApplyCheatInvalidValue(), mtError, mbOK, 0); return result; }
    St->seek(0x2A + (B * 8)); ReadByte(St, X);
    if (ShowCheatDBInternalPrivateerPositionDialog(AOwner, X)) {
        St->seek(0x2A + (B * 8)); WriteByte(St, X); result = true;
    }
    return result;
}

bool ApplyBinaryCheat(int Nr, QFile* St)
{
    /* Translated from DOS Pascal code from 1997 */
    bool result = false;
    switch (Nr) {
        case 1: result = DarkSideOfXeenPosition(St); break;
        case 2: result = PrivateerMoney(St); break;
        case 3: result = SimCity2000Money(St); break;
        case 4: BetrayalAtKrondorCharacters(St); result = true; break;
        case 5: AdvantedTennisParameters(St); result = true; break;
        case 6: UfoBuildingTime(St); result = true; break;
        case 7: result = PrivateerShip(St); break;
        case 8: result = PrivateerPosition(ApplicationMainForm(), St); break;
        case 9: PatchCC1(St); result = true; break;
    }
    return result;
}
