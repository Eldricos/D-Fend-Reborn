// C++ port of CheatDBUnit.pas (implementation section).
#include "CheatDBUnit.h"

#include "CheatDBToolsUnit.h"
#include "PrgConsts.h"
#include "CheatDBInternalUnit.h"
#include "PackageDBToolsUnit.h"
#include "LanguageSetupUnit.h"

#include <QtEndian>

// {DEFINE DebugCheatDB}

// Helpers for TFileStream.WriteBuffer / ReadBuffer with the first ABytes bytes
// of an Integer (little endian, lower order byte first like on x86).
static void WriteIntBuffer(QFile* St, int Value, int ABytes)
{
    char Buf[4];
    qToLittleEndian<qint32>(Value, Buf);
    St->write(Buf, ABytes);
}

static void ReadIntBuffer(QFile* St, int& Value, int ABytes)
{
    char Buf[4] = {0, 0, 0, 0};
    qToLittleEndian<qint32>(Value, Buf);
    St->read(Buf, ABytes);
    Value = qFromLittleEndian<qint32>(Buf);
}

// Element children of an XML node (IXMLNode.ChildNodes; text and comment nodes
// would fail to load anyway and are skipped)
static QList<QDomElement> ChildElements(const QDomElement& XML)
{
    QList<QDomElement> result;
    for (QDomElement E = XML.firstChildElement(); !E.isNull(); E = E.nextSiblingElement()) result.append(E);
    return result;
}

/* global */

static QList<TCheatActionStepClass> CheatActionStepClasses;

static void RegisterCheatActionStep(const TCheatActionStepClass& CheatActionStepClass)
{
    CheatActionStepClasses.append(CheatActionStepClass);
}

static void RegisterCheatActionSteps()
{
    RegisterCheatActionStep(CheatActionStepClassOf<TCheatActionStepChangeAddress>());
    RegisterCheatActionStep(CheatActionStepClassOf<TCheatActionStepChangeAddressWithDialog>());
    RegisterCheatActionStep(CheatActionStepClassOf<TCheatActionStepInternal>());
}

/* TCheatBaseClass */

TCheatBaseClass::TCheatBaseClass(TCheatBaseClass* AOwner)
{
    FOwner = AOwner;
}

void TCheatBaseClass::SetChanged()
{
    if (FOwner != nullptr) FOwner->SetChanged();
}

/* TCheatActionStep */

TCheatActionStep::TCheatActionStep(TCheatBaseClass* AOwner) : TCheatBaseClass(AOwner)
{
}

/* TCheatActionStepChangeAddress */

TCheatActionStepChangeAddress::TCheatActionStepChangeAddress(TCheatBaseClass* AOwner) : TCheatActionStep(AOwner)
{
    FAddressesInt.clear();
    FBytes = -1;
}

QString TCheatActionStepChangeAddress::ClassXMLNodeName()
{
    return "ChangeAddress";
}

void TCheatActionStepChangeAddress::SetString(int Index, const QString& S)
{
    int I;
    switch (Index) {
        case 0:
            if (FAddresses == S) return;
            if (!LoadAddresses(S, FAddressesInt)) return;
            FAddresses = S;
            break;
        case 1:
            if (FNewValue == S) return;
            if (!ExtTryStrToInt(S, I)) return;
            FNewValue = S; FNewValueInt = I;
            break;
    }
    SetChanged();
}

void TCheatActionStepChangeAddress::SetBytes(int Value)
{
    if ((Value != 1) && (Value != 2) && (Value != 4)) return;
    if (Value == FBytes) return;
    FBytes = Value;
    SetChanged();
}

bool TCheatActionStepChangeAddress::LoadFromXML(const QDomElement& XML)
{
    bool result = false;
    if (XML.nodeName() != "ChangeAddress") return result;
    if ((!XML.hasAttribute("Addresses")) || (!XML.hasAttribute("Bytes")) || (!XML.hasAttribute("NewValue"))) return result;

    FAddresses = XML.attribute("Addresses"); if (!LoadAddresses(FAddresses, FAddressesInt)) return result;
    if (!ExtTryStrToInt(XML.attribute("Bytes"), FBytes)) return result;
    if ((FBytes < 1) || (FBytes > 4) || (FBytes == 3)) return result;
    FNewValue = XML.attribute("NewValue"); if (!ExtTryStrToInt(FNewValue, FNewValueInt)) return result;
    result = true;
    return result;
}

void TCheatActionStepChangeAddress::SaveToXML(const QDomElement& XML)
{
    QDomElement P = XML;
    QDomElement N = P.ownerDocument().createElement("ChangeAddress");
    P.appendChild(N);
    N.setAttribute("Addresses", FAddresses);
    N.setAttribute("Bytes", IntToStr(FBytes));
    N.setAttribute("NewValue", FNewValue);
}

void TCheatActionStepChangeAddress::CopyFrom(TCheatActionStep* ACheatActionStep)
{
    auto* A = dynamic_cast<TCheatActionStepChangeAddress*>(ACheatActionStep);
    if (A == nullptr) return;
    FAddresses = A->Addresses();
    FNewValue = A->NewValue();
    FAddressesInt = A->FAddressesInt;
    FBytes = A->Bytes();
    FNewValueInt = A->FNewValueInt;
}

bool TCheatActionStepChangeAddress::Apply(QFile* St)
{
    bool result = false;
    if (FBytes < 0) return result;

    for (int I = 0; I < FAddressesInt.size(); I++) {
        if (!ExtSeek(St, FAddressesInt[I], FBytes)) return result;
        WriteIntBuffer(St, FNewValueInt, FBytes); /* lower order byte first so it's ok just to write the first FBytesInt Bytes */
    }

    result = true;
    return result;
}

/* TCheatActionStepChangeAddressWithDialog */

TCheatActionStepChangeAddressWithDialog::TCheatActionStepChangeAddressWithDialog(TCheatBaseClass* AOwner) : TCheatActionStep(AOwner)
{
    FAddressesInt.clear();
    FBytes = -1;
    FDefaultValueAddressInt = -1;
    FMinValueInt = -1;
    FMaxValueInt = -1;
}

QString TCheatActionStepChangeAddressWithDialog::ClassXMLNodeName()
{
    return "ChangeAddressWithDialog";
}

void TCheatActionStepChangeAddressWithDialog::SetString(int Index, const QString& S)
{
    int I;
    switch (Index) {
        case 0:
            if (FAddresses == S) return;
            if (!LoadAddresses(S, FAddressesInt)) return;
            FAddresses = S;
            break;
        case 1:
            if (FDefaultValue == S) return; else { FDefaultValue = S; FDefaultValueAddress = ""; FDefaultValueAddressInt = -1; }
            break;
        case 2:
            if ((FDefaultValueAddress == S) || (!ExtTryStrToInt(S, I))) return; else { FDefaultValueAddress = S; FDefaultValueAddressInt = I; FDefaultValue = ""; }
            break;
        case 3:
            if (FMinValue == S) return;
            if (Trim(S) == "") { FMinValueInt = -1; FMinValue = ""; } else {
                if (!ExtTryStrToInt(S, I)) return;
                FMinValue = S; FMinValueInt = I;
            }
            break;
        case 4:
            if (FMaxValue == S) return;
            if (Trim(S) == "") { FMaxValueInt = -1; FMaxValue = ""; } else {
                if (!ExtTryStrToInt(S, I)) return;
                FMaxValue = S; FMaxValueInt = I;
            }
            break;
    }
    SetChanged();
}

void TCheatActionStepChangeAddressWithDialog::SetBytes(int Value)
{
    if ((Value != 1) && (Value != 2) && (Value != 4)) return;
    if (Value == FBytes) return;
    FBytes = Value;
    SetChanged();
}

bool TCheatActionStepChangeAddressWithDialog::LoadFromXML(const QDomElement& XML)
{
    bool result = false;
    if (XML.nodeName() != "ChangeAddressWithDialog") return result;
    if ((!XML.hasAttribute("Addresses")) || (!XML.hasAttribute("Bytes")) || (!XML.hasAttribute("Prompt"))) return result;
    if ((!XML.hasAttribute("DefaultValue")) && (!XML.hasAttribute("DefaultValueAddress"))) return result;

    FAddresses = XML.attribute("Addresses"); if (!LoadAddresses(FAddresses, FAddressesInt)) return result;
    if (!ExtTryStrToInt(XML.attribute("Bytes"), FBytes)) return result;
    if ((FBytes < 1) || (FBytes > 4) || (FBytes == 3)) return result;
    if (XML.hasAttribute("DefaultValue")) {
        FDefaultValue = XML.attribute("DefaultValue");
    } else {
        FDefaultValueAddress = XML.attribute("DefaultValueAddress"); if (!ExtTryStrToInt(FDefaultValueAddress, FDefaultValueAddressInt)) return result;
    }
    FPrompt = XML.attribute("Prompt");
    if (XML.hasAttribute("MinValue")) {
        FMinValue = XML.attribute("MinValue"); if (!ExtTryStrToInt(FMinValue, FMinValueInt)) return result;
    }
    if (XML.hasAttribute("MaxValue")) {
        FMaxValue = XML.attribute("MaxValue"); if (!ExtTryStrToInt(FMaxValue, FMaxValueInt)) return result;
    }
    result = true;
    return result;
}

void TCheatActionStepChangeAddressWithDialog::SaveToXML(const QDomElement& XML)
{
    QDomElement P = XML;
    QDomElement N = P.ownerDocument().createElement("ChangeAddressWithDialog");
    P.appendChild(N);
    N.setAttribute("Addresses", FAddresses);
    N.setAttribute("Bytes", IntToStr(FBytes));
    if (FDefaultValue != "") N.setAttribute("DefaultValue", FDefaultValue); else N.setAttribute("DefaultValueAddress", FDefaultValueAddress);
    N.setAttribute("Prompt", FPrompt);
    if (Trim(FMinValue) != "") N.setAttribute("MinValue", FMinValue);
    if (Trim(FMaxValue) != "") N.setAttribute("MaxValue", FMaxValue);
}

void TCheatActionStepChangeAddressWithDialog::CopyFrom(TCheatActionStep* ACheatActionStep)
{
    auto* A = dynamic_cast<TCheatActionStepChangeAddressWithDialog*>(ACheatActionStep);
    if (A == nullptr) return;

    FAddresses = A->Addresses();
    FDefaultValue = A->DefaultValue();
    FDefaultValueAddress = A->DefaultValueAddress();
    FPrompt = A->DialogPrompt();
    FMinValue = A->MinValue();
    FMaxValue = A->MaxValue();
    FAddressesInt = A->FAddressesInt;
    FBytes = A->Bytes();
    FDefaultValueAddressInt = A->FDefaultValueAddressInt;
    FMinValueInt = A->FMinValueInt;
    FMaxValueInt = A->FMaxValueInt;
}

bool TCheatActionStepChangeAddressWithDialog::Apply(QFile* St)
{
    QString Value;
    int I;
    bool result = false;
    if (FBytes < 0) return result;

    if (Trim(FDefaultValue) != "") {
        Value = FDefaultValue;
    } else {
        if (!ExtSeek(St, FDefaultValueAddressInt, FBytes)) return result;
        I = 0; ReadIntBuffer(St, I, FBytes); Value = IntToStr(I);
    }

    auto* GameRecord = dynamic_cast<TCheatGameRecord*>(Owner() != nullptr ? Owner()->Owner() : nullptr);
    auto* Action = dynamic_cast<TCheatAction*>(Owner());
    const QString Caption = (GameRecord != nullptr ? GameRecord->Name() : QString()) + " - " + (Action != nullptr ? Action->Name() : QString());
    if (!InputQuery(Caption, FPrompt, Value)) return result;

    if ((!ExtTryStrToInt(Value, I)) || ((Trim(FMinValue) != "") && (I < FMinValueInt)) || ((Trim(FMaxValue) != "") && (I > FMaxValueInt))) {
        MessageDlg(LanguageSetup->ApplyCheatInvalidValue(), mtError, mbOK, 0);
        return result;
    }

    if (FBytes == 1) {
        if ((I < 0) || (I > 255)) { MessageDlg(LanguageSetup->ApplyCheatInvalidValue(), mtError, mbOK, 0); return result; }
    }
    if (FBytes == 2) {
        if ((I < 0) || (I > 65535)) { MessageDlg(LanguageSetup->ApplyCheatInvalidValue(), mtError, mbOK, 0); return result; }
    }

    for (int J = 0; J < FAddressesInt.size(); J++) {
        if (!ExtSeek(St, FAddressesInt[J], FBytes)) return result;
        WriteIntBuffer(St, I, FBytes); /* lower order byte first so it's ok just to write the first FBytesInt Bytes */
    }

    result = true;
    return result;
}

/* TCheatActionStepInternal */

TCheatActionStepInternal::TCheatActionStepInternal(TCheatBaseClass* AOwner) : TCheatActionStep(AOwner)
{
    SetNr(-1);
}

QString TCheatActionStepInternal::ClassXMLNodeName()
{
    return "Internal";
}

bool TCheatActionStepInternal::LoadFromXML(const QDomElement& XML)
{
    bool result = false;
    if (XML.nodeName() != "Internal") return result;
    if (!XML.hasAttribute("Nr")) return result;

    if (!ExtTryStrToInt(XML.attribute("Nr"), FNr)) return result;
    result = true;
    return result;
}

void TCheatActionStepInternal::SaveToXML(const QDomElement& XML)
{
    QDomElement P = XML;
    QDomElement N = P.ownerDocument().createElement("Internal");
    P.appendChild(N);
    N.setAttribute("Nr", IntToStr(FNr));
}

void TCheatActionStepInternal::CopyFrom(TCheatActionStep* ACheatActionStep)
{
    auto* A = dynamic_cast<TCheatActionStepInternal*>(ACheatActionStep);
    if (A == nullptr) return;
    FNr = A->Nr();
}

void TCheatActionStepInternal::SetNr(int Value)
{
    if (FNr == Value) return;
    FNr = Value;
    SetChanged();
}

bool TCheatActionStepInternal::Apply(QFile* St)
{
    return ApplyBinaryCheat(FNr, St);
}

/* TCheatAction */

TCheatAction::TCheatAction(TCheatBaseClass* AOwner) : TCheatBaseClass(AOwner)
{
    FActionSteps = new TList;
}

TCheatAction::~TCheatAction()
{
    Clear();
    delete FActionSteps;
}

bool TCheatAction::LoadFromXML(const QDomElement& XML)
{
    QString S;
    TCheatActionStep* A;
    bool result = false;
    Clear();
    if (XML.nodeName() != "Action") return result;
    if ((!XML.hasAttribute("Name")) || (!XML.hasAttribute("FileMask"))) return result;
    FName = XML.attribute("Name");
    FFileMask = XML.attribute("FileMask");

    const QList<QDomElement> Children = ChildElements(XML);
    for (int I = 0; I < Children.size(); I++) {
        S = Children[I].nodeName();
        for (int J = 0; J < CheatActionStepClasses.size(); J++) if (CheatActionStepClasses[J].GetXMLNodeName() == S) {
            A = CheatActionStepClasses[J].Create(this);
            if (A->LoadFromXML(Children[I])) FActionSteps->append(A); else {
                delete A;
#ifdef DebugCheatDB
                ShowMessage("Failed to load ActionStep " + IntToStr(I) + " (Action=\"" + FName + "\")");
#endif
            }
        }
    }
    result = true;
    return result;
}

void TCheatAction::SaveToXML(const QDomElement& XML)
{
    QDomElement P = XML;
    QDomElement N = P.ownerDocument().createElement("Action");
    P.appendChild(N);
    N.setAttribute("Name", FName);
    N.setAttribute("FileMask", FFileMask);

    for (int I = 0; I < FActionSteps->size(); I++) static_cast<TCheatActionStep*>((*FActionSteps)[I])->SaveToXML(N);
}

void TCheatAction::CopyFrom(TCheatAction* ACheatAction)
{
    TCheatActionStep* A;
    FName = ACheatAction->Name();
    FFileMask = ACheatAction->FileMask();
    for (int I = 0; I < ACheatAction->Count(); I++) {
        for (int J = 0; J < CheatActionStepClasses.size(); J++) if (CheatActionStepClasses[J].IsInstance(ACheatAction->ActionStep(I))) {
            A = CheatActionStepClasses[J].Create(nullptr);
            A->CopyFrom(ACheatAction->ActionStep(I));
            Add(A);
            break;
        }
    }
}

bool TCheatAction::Apply(QFile* St)
{
    bool result = false;
    for (int I = 0; I < FActionSteps->size(); I++) if (!static_cast<TCheatActionStep*>((*FActionSteps)[I])->Apply(St)) return result;
    result = true;
    return result;
}

bool TCheatAction::Apply(const QString& FileName, bool BackupBefore)
{
    if (BackupBefore) BackupFileBeforeApplyingCheat(FileName);
    QFile St(FileName);
    if (!St.open(QIODevice::ReadWrite)) {
        // Delphi: TFileStream.Create raises EFOpenError
        throw Exception(QString("Cannot open file \"%1\". %2").arg(FileName, St.errorString()));
    }
    bool result = Apply(&St);
    St.close();
    return result;
}

void TCheatAction::SetString(int Index, const QString& S)
{
    switch (Index) {
        case 0: if (S == FName) return; else FName = S; break;
        case 1: if (S == FFileMask) return; else FFileMask = S; break;
    }
    SetChanged();
}

int TCheatAction::GetCount()
{
    return FActionSteps->size();
}

TCheatActionStep* TCheatAction::GetActionStep(int I)
{
    if ((I < 0) || (I >= FActionSteps->size())) return nullptr; else return static_cast<TCheatActionStep*>((*FActionSteps)[I]);
}

int TCheatAction::Add(TCheatActionStep* ACheatActionStep)
{
    FActionSteps->append(ACheatActionStep);
    int result = FActionSteps->size() - 1;
    ACheatActionStep->SetOwner(this);
    SetChanged();
    return result;
}

void TCheatAction::Delete(int Nr)
{
    if ((Nr < 0) || (Nr >= FActionSteps->size())) return;
    delete static_cast<TCheatActionStep*>((*FActionSteps)[Nr]);
    FActionSteps->removeAt(Nr);
    SetChanged();
}

void TCheatAction::Delete(TCheatActionStep* ACheatActionStep)
{
    Delete(static_cast<int>(FActionSteps->indexOf(ACheatActionStep)));
}

void TCheatAction::Clear()
{
    for (int I = 0; I < FActionSteps->size(); I++) delete static_cast<TCheatActionStep*>((*FActionSteps)[I]);
    // The Delphi code did not clear the list (dangling pointers); fixed here.
    FActionSteps->clear();
    SetChanged();
}

void TCheatAction::CompressActionSteps()
{
    TCheatActionStepChangeAddress *A0, *A1;
    QString S;
    int I = 0;
    while (I < FActionSteps->size() - 1) {

        A0 = dynamic_cast<TCheatActionStepChangeAddress*>(static_cast<TCheatActionStep*>((*FActionSteps)[I]));
        if (A0 == nullptr) { I++; continue; }

        while (FActionSteps->size() > I + 1) {
            A1 = dynamic_cast<TCheatActionStepChangeAddress*>(static_cast<TCheatActionStep*>((*FActionSteps)[I + 1]));
            if (A1 == nullptr) break;
            if ((A1->Bytes() != A0->Bytes()) || (A1->NewValue() != A0->NewValue())) break;
            S = A1->Addresses();
            if ((S != "") && ((S[0] == ';') || (S[0] == ','))) S = Copy(S, 2, MaxInt);
            if ((S != "") && ((S[S.length() - 1] == ';') || (S[S.length() - 1] == ','))) S = Copy(S, 1, S.length() - 1);
            A0->SetAddresses(A0->Addresses() + ";" + S);
            Delete(I + 1);
        }

        I++;
    }
}

int TCheatAction::IndexOf(TCheatActionStep* ACheatActionStep)
{
    return FActionSteps->indexOf(ACheatActionStep);
}

bool TCheatAction::SetNewActionStep(TCheatActionStep* ACheatActionStep, int APosition)
{
    bool result = false;
    if ((APosition < 0) || (APosition >= FActionSteps->size())) return result;
    if (ACheatActionStep == nullptr) return result;
    delete static_cast<TCheatActionStep*>((*FActionSteps)[APosition]);
    (*FActionSteps)[APosition] = ACheatActionStep;
    ACheatActionStep->SetOwner(this);
    result = true;
    SetChanged();
    return result;
}

/* TCheatGameRecord */

TCheatGameRecord::TCheatGameRecord(TCheatBaseClass* AOwner) : TCheatBaseClass(AOwner)
{
    FActions = new TList;
}

TCheatGameRecord::~TCheatGameRecord()
{
    Clear();
    delete FActions;
}

bool TCheatGameRecord::LoadFromXML(const QDomElement& XML)
{
    TCheatAction* A;
    bool result = false;
    Clear();
    if (XML.nodeName() != "GameRecord") return result;
    if (!XML.hasAttribute("Name")) return result;

    FName = XML.attribute("Name");
    const QList<QDomElement> Children = ChildElements(XML);
    for (int I = 0; I < Children.size(); I++) {
        A = new TCheatAction(this);
        if (A->LoadFromXML(Children[I])) FActions->append(A); else {
            delete A;
#ifdef DebugCheatDB
            ShowMessage("Failed to load Action " + IntToStr(I) + " Name=\"" + Children[I].attribute("Name") + "\" (Game=\"" + FName + "\")");
#endif
        }
    }
    result = true;
    return result;
}

void TCheatGameRecord::SaveToXML(const QDomElement& XML)
{
    QDomElement P = XML;
    QDomElement N = P.ownerDocument().createElement("GameRecord");
    P.appendChild(N);
    N.setAttribute("Name", FName);
    for (int I = 0; I < FActions->size(); I++) static_cast<TCheatAction*>((*FActions)[I])->SaveToXML(N);
}

void TCheatGameRecord::CopyFrom(TCheatGameRecord* ACheatGameRecord)
{
    TCheatAction* A;
    FName = ACheatGameRecord->Name();
    for (int I = 0; I < ACheatGameRecord->Count(); I++) {
        A = new TCheatAction(nullptr);
        A->CopyFrom(ACheatGameRecord->Action(I));
        Add(A);
    }
}

void TCheatGameRecord::SetName(const QString& Value)
{
    if (FName == Value) return;
    FName = Value;
    SetChanged();
}

int TCheatGameRecord::GetCount()
{
    return FActions->size();
}

TCheatAction* TCheatGameRecord::GetAction(int I)
{
    if ((I < 0) || (I >= FActions->size())) return nullptr; else return static_cast<TCheatAction*>((*FActions)[I]);
}

int TCheatGameRecord::Add(TCheatAction* ACheatAction)
{
    FActions->append(ACheatAction);
    int result = FActions->size() - 1;
    ACheatAction->SetOwner(this);
    SetChanged();
    return result;
}

void TCheatGameRecord::Delete(int Nr)
{
    if ((Nr < 0) || (Nr >= FActions->size())) return;
    delete static_cast<TCheatAction*>((*FActions)[Nr]);
    FActions->removeAt(Nr);
    SetChanged();
}

void TCheatGameRecord::Delete(TCheatAction* ACheatAction)
{
    Delete(static_cast<int>(FActions->indexOf(ACheatAction)));
}

void TCheatGameRecord::Clear()
{
    for (int I = 0; I < FActions->size(); I++) delete static_cast<TCheatAction*>((*FActions)[I]);
    // The Delphi code did not clear the list (dangling pointers); fixed here.
    FActions->clear();
    SetChanged();
}

int TCheatGameRecord::IndexOf(TCheatAction* ACheatAction)
{
    return FActions->indexOf(ACheatAction);
}

int TCheatGameRecord::IndexOf(const QString& ACheatActionName)
{
    for (int I = 0; I < FActions->size(); I++) if (static_cast<TCheatAction*>((*FActions)[I])->Name() == ACheatActionName) {
        return I;
    }
    return -1;
}

void TCheatGameRecord::CompressActionSteps()
{
    for (int I = 0; I < FActions->size(); I++) static_cast<TCheatAction*>((*FActions)[I])->CompressActionSteps();
}

/* TCheatsDB */

TCheatDB::TCheatDB() : TCheatBaseClass(nullptr)
{
    FGameRecords = new TList;
    FChanged = false;
}

TCheatDB::~TCheatDB()
{
    Clear();
    delete FGameRecords;
}

bool TCheatDB::LoadFromXML(const QString& FileName)
{
    QDomDocument* XMLDoc;
    TCheatGameRecord* G;
    bool result = false;
    if (!FileExists(FileName)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {FileName}), mtError, mbOK, 0);
        return result;
    }
    XMLDoc = LoadXMLDoc(FileName, FileName); if (XMLDoc == nullptr) return result;
    Clear();
    {
        const QDomElement DocumentElement = XMLDoc->documentElement();
        if (DocumentElement.nodeName() != "CheatDB") { delete XMLDoc; return result; }

        FVersion = 0;
        if (DocumentElement.hasAttribute("Version")) {
            TryStrToInt(DocumentElement.attribute("Version"), FVersion);
        }

        const QList<QDomElement> Children = ChildElements(DocumentElement);
        for (int I = 0; I < Children.size(); I++) {
            G = new TCheatGameRecord(this);
            if (G->LoadFromXML(Children[I])) FGameRecords->append(G); else {
                delete G;
#ifdef DebugCheatDB
                ShowMessage("Failed to load GameRecord " + IntToStr(I) + " Name=\"" + Children[I].attribute("Name") + "\"");
#endif
            }
        }
    }
    delete XMLDoc;
    FChanged = false;
    result = true;
    return result;
}

void TCheatDB::SaveToXML(const QString& FileName, QWidget* ParentForm)
{
    Q_UNUSED(ParentForm);
    QDomDocument XMLDoc;
    QDomElement DocumentElement = XMLDoc.createElement("CheatDB");
    XMLDoc.appendChild(DocumentElement);
    if (FVersion >= 0) DocumentElement.setAttribute("Version", IntToStr(FVersion));
    for (int I = 0; I < FGameRecords->size(); I++) static_cast<TCheatGameRecord*>((*FGameRecords)[I])->SaveToXML(DocumentElement);
    SaveXMLDoc(XMLDoc, {"<!DOCTYPE CheatDB SYSTEM \"" + DFRHomepage + "Packages/Cheats.dtd\">"}, FileName, false);
    FChanged = false;
}

int TCheatDB::Add(TCheatGameRecord* AGameRecord)
{
    FGameRecords->append(AGameRecord);
    int result = FGameRecords->size() - 1;
    AGameRecord->SetOwner(this);
    FChanged = true;
    return result;
}

void TCheatDB::Delete(int Nr)
{
    if ((Nr < 0) || (Nr >= FGameRecords->size())) return;
    delete static_cast<TCheatGameRecord*>((*FGameRecords)[Nr]);
    FGameRecords->removeAt(Nr);
    FChanged = true;
}

void TCheatDB::Delete(TCheatGameRecord* AGameRecord)
{
    Delete(static_cast<int>(FGameRecords->indexOf(AGameRecord)));
}

void TCheatDB::Clear()
{
    for (int I = 0; I < FGameRecords->size(); I++) delete static_cast<TCheatGameRecord*>((*FGameRecords)[I]);
    // The Delphi code did not clear the list (dangling pointers); fixed here.
    FGameRecords->clear();
    FVersion = -1;
    FChanged = false;
}

void TCheatDB::CompressActionSteps()
{
    for (int I = 0; I < FGameRecords->size(); I++) static_cast<TCheatGameRecord*>((*FGameRecords)[I])->CompressActionSteps();
}

int TCheatDB::IndexOf(TCheatGameRecord* AGameRecord)
{
    return FGameRecords->indexOf(AGameRecord);
}

int TCheatDB::IndexOf(const QString& AGameName)
{
    for (int I = 0; I < FGameRecords->size(); I++) if (static_cast<TCheatGameRecord*>((*FGameRecords)[I])->Name() == AGameName) {
        return I;
    }
    return -1;
}

int TCheatDB::GetCount()
{
    return FGameRecords->size();
}

TCheatGameRecord* TCheatDB::GetGameRecord(int I)
{
    if ((I < 0) || (I >= FGameRecords->size())) return nullptr; else return static_cast<TCheatGameRecord*>((*FGameRecords)[I]);
}

void TCheatDB::SetChanged()
{
    FChanged = true;
}

void TCheatDB::SetVersion(int Value)
{
    if (FVersion == Value) return;
    FVersion = Value;
    FChanged = true;
}

void CheatDBUnit_Initialization()
{
    RegisterCheatActionSteps();
}
