// C++ port of MIDITools.pas (implementation section).
#include "MIDITools.h"

#include "LanguageSetupUnit.h"
#include "DOSBoxTempUnit.h"
#include "DosBoxUnit.h"
#include "CommonTools.h"
#include "MainUnit.h"

static void ProcessMIDIInfoLine(const QString& Line, TStringList* List)
{
    int I;
    QString S, T;
    I = Pos(" ", Line); if (I == 0) return;
    S = Trim(Copy(Line, 1, I - 1)); T = Trim(Copy(Line, I + 1, MaxInt));
    if (!TryStrToInt(S, I)) return;
    if ((!T.isEmpty()) && (T[0] == '"') && (T[Length(T) - 1] == '"')) T = Trim(Copy(T, 2, Length(T) - 2));
    List->AddObject(T + " (" + LanguageSetup->ProfileEditorSoundMIDIConfigID() + "=" + IntToStr(I) + ")", static_cast<intptr_t>(I));
}

TStringList* GetMIDIDevices()
{
    static const QString MIDIListFile = QStringLiteral("DFR-MIDI.TMP");
    TTempGame* TempGame;
    TStringList* St;
    QString S;
    int I;

    TStringList* result = new TStringList();
    TempGame = new TTempGame();
    TempGame->Game()->SetNrOfMounts(1);
    TempGame->Game()->SetMount0(TempDir() + ";DRIVE;C;False;;");
    St = new TStringList();
    St->Add("mixer /LISTMIDI > C:\\" + MIDIListFile);
    St->Add("exit");
    TempGame->Game()->SetAutoexec(StringListToString(St));
    delete St;
    TempGame->Game()->StoreAllValues();
    RunCommandAndWait(TempGame->Game(), DFendReloadedMainForm ? DFendReloadedMainForm->DeleteOnExit : nullptr, "", true);
    St = new TStringList();
    St->LoadFromFile(TempDir() + MIDIListFile);
    for (I = 0; I < St->Count(); I++) {
        S = Trim((*St)[I]);
        if (S.isEmpty()) continue;
        ProcessMIDIInfoLine(S, result);
    }
    delete St;
    ExtDeleteFile(TempDir() + MIDIListFile, ftTemp);
    delete TempGame;
    return result;
}

QString MakeDOSBoxMIDIString(const QString& ProfileString)
{
    QString S, T;
    int I, J;
    TStringList* St;
    QString result = "";

    S = Trim(ProfileString);
    if (S.isEmpty()) return result;
    if (TryStrToInt(S, I)) { return S; }
    S = ExtUpperCase(S);

    St = GetMIDIDevices();
    for (I = 0; I < St->Count(); I++) {
        T = (*St)[I];
        while ((!T.isEmpty()) && (T[Length(T) - 1] != '(')) T.chop(1);
        if (!T.isEmpty()) T.chop(1);
        if (ExtUpperCase(Trim(T)) == S) {
            T = (*St)[I];
            if (!T.isEmpty()) T.chop(1);
            J = Pos("=", T); while (J > 0) { T = Copy(T, J + 1, MaxInt); J = Pos("=", T); }
            if (TryStrToInt(Trim(T), J)) { result = IntToStr(J); break; }
        }
    }
    delete St;
    return result;
}
