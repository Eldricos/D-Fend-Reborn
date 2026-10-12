// C++ port of ChecksumScanner.pas (implementation section).
#include "ChecksumScanner.h"

#include "CommonTools.h"
#include "HashCalc.h"
#include "WaitFormUnit.h"
#include "LanguageSetupUnit.h"

// { TChecksumScanner }

TChecksumScanner::TChecksumScanner(const QString& ADirectory, const QString& AFileMask, bool ASmallFiles, bool AShowScanDialog, QWidget* AOwner)
    : TChecksumScanner(ADirectory, AFileMask, ASmallFiles, AShowScanDialog, AOwner, true)
{
}

TChecksumScanner::TChecksumScanner(const QString& ADirectory, const QString& AFileMask, bool ASmallFiles, bool AShowScanDialog, QWidget* AOwner, bool ADoScan)
{
    FDirectory = IncludeTrailingPathDelimiter(ADirectory);
    FFileMask = AFileMask;
    FFileNames = new TStringList;
    FChecksums = new TStringList;
    FSmallFiles = ASmallFiles;
    FShowScanDialog = AShowScanDialog;
    FOwner = AOwner;
    if (ADoScan) ReScan();
}

TChecksumScanner::~TChecksumScanner()
{
    delete FFileNames;
    delete FChecksums;
}

int TChecksumScanner::GetCount()
{
    return FFileNames->Count();
}

QString TChecksumScanner::GetFileChecksum(const QString& Filename)
{
    return GetMD5Sum(Filename, FSmallFiles);
}

void TChecksumScanner::ReScan()
{
    TSearchRec Rec;
    int I;
    TWaitForm* WaitForm;

    FFileNames->Clear();
    FChecksums->Clear();

    I = FindFirst(FDirectory + FFileMask, faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == 0) FFileNames->AddObject(ExtUpperCase(Rec.Name), static_cast<intptr_t>(FFileNames->Count()));
        I = FindNext(Rec);
    }
    FindClose(Rec);

    if (FShowScanDialog && ((FFileNames->Count() > 100) || ((!FSmallFiles) && (FFileNames->Count() > 20))))
        WaitForm = CreateWaitForm(FOwner, LanguageSetup->PackageManagerScanning(), FFileNames->Count());
    else
        WaitForm = nullptr;
    for (I = 0; I < FFileNames->Count(); I++) {
        if (WaitForm != nullptr && (I % 10 == 0)) WaitForm->Step(I);
        // The file names are stored upper case (see above); on case sensitive
        // file systems the original spelling is needed to open the file.
        QString FileName = FDirectory + (*FFileNames)[I];
        if (!FileExists(FileName)) {
            TSearchRec R;
            int J = FindFirst(FDirectory + FFileMask, faAnyFile, R);
            while (J == 0) {
                if (ExtUpperCase(R.Name) == (*FFileNames)[I]) { FileName = FDirectory + R.Name; break; }
                J = FindNext(R);
            }
            FindClose(R);
        }
        FChecksums->Add(GetFileChecksum(FileName));
    }
    if (WaitForm != nullptr) { delete WaitForm; WaitForm = nullptr; }

    FFileNames->Sort();
}

QString TChecksumScanner::GetChecksum(const QString& AFileName)
{
    QString result;
    int I = FFileNames->IndexOf(ExtUpperCase(AFileName));
    if (I < 0) return result;
    result = (*FChecksums)[FFileNames->ObjectAsInt(I)];
    return result;
}

bool TChecksumScanner::ChecksumInList(const QString& AChecksum)
{
    return (FChecksums->IndexOf(AChecksum) >= 0);
}

// { TLanguageFileChecksumScanner }

TLanguageFileChecksumScanner::TLanguageFileChecksumScanner(const QString& ADirectory, bool AShowScanDialog, QWidget* AOwner)
    : TChecksumScanner(ADirectory, "*.ini", true, AShowScanDialog, AOwner, false)
{
    // Delphi calls the overridden GetFileChecksum from the base constructor;
    // in C++ the scan has to be started here.
    ReScan();
}

QString TLanguageFileChecksumScanner::GetFileChecksum(const QString& Filename)
{
    TIniFile Ini(Filename);
    return Ini.ReadString("LanguageFileInfo", "DownloadChecksum", TChecksumScanner::GetFileChecksum(Filename));
}
