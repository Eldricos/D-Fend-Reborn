// D-Fend Reborn addition - see DataPackageUnit.h
#include "DataPackageUnit.h"

#include "CommonTools.h"
#include "GameDBUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "ZipInfoFormUnit.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

static const QString DataPackageTempDir = QStringLiteral("DataPackage");

bool IsInstallableDataPackage(const QString& FileName)
{
    const QString Ext = ExtUpperCase(ExtractFileExt(FileName));
    if (Ext == ".7Z" || Ext == ".ZIP") return true;
#ifdef Q_OS_WIN
    return false;   // the original NSIS installers are run directly
#else
    return Ext == ".EXE";
#endif
}

QString FindDataPackageRoot(const QString& Folder)
{
    const QString Base = IncludeTrailingPathDelimiter(Folder);
    if (DirectoryExists(Base + "Confs")) return Base;
    // one top folder ("Package/Confs/..."; extracted NSIS installers: "$_4_/Confs/...")
    for (const QString& Sub : QDir(Base).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)) {
        if (DirectoryExists(Base + Sub + PathDelim + "Confs")) return Base + Sub + PathDelim;
    }
    return QString();
}

bool CopyDataPackageFiles(const QString& SourceFolder, const QString& DestFolder, QStringList& NewProfiles, QString& FailedFile)
{
    const QDir Source(SourceFolder);
    const QString Dest = IncludeTrailingPathDelimiter(DestFolder);
    QDirIterator It(SourceFolder, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (It.hasNext()) {
        It.next();
        const QString Rel = Source.relativeFilePath(It.filePath());
        const QString Target = Dest + QDir::toNativeSeparators(Rel);
        // existing files are kept (profiles, saved games and settings of
        // games that are already installed)
        if (QFileInfo::exists(Target)) continue;
        ForceDirectories(ExtractFilePath(Target));
        if (!QFile::copy(It.filePath(), Target)) { FailedFile = Target; return false; }
        const QStringList Parts = Rel.split('/');
        if (Parts.size() == 2 && Parts[0].compare("Confs", Qt::CaseInsensitive) == 0 && ExtUpperCase(ExtractFileExt(Parts[1])) == ".PROF")
            NewProfiles << Target;
    }
    return true;
}

int InstallDataPackage(QWidget* Owner, const QString& FileName, TGameDB* GameDB)
{
    const QString Temp = TempDir() + DataPackageTempDir + PathDelim;
    if (DirectoryExists(Temp)) ExtDeleteFolder(Temp, ftTemp);
    ForceDirectories(Temp);

    int Result = -1;
    if (ExtractZipFile(Owner, FileName, Temp)) {
        const QString Root = FindDataPackageRoot(Temp);
        if (Root.isEmpty()) {
            MessageDlg(LanguageSetup->MessageZipImportError(), mtError, mbOK, 0);
        } else {
            QStringList NewProfiles;
            QString FailedFile;
            const bool OK = CopyDataPackageFiles(Root, PrgDataDir(), NewProfiles, FailedFile);
            // the profiles copied so far are added even if a later file failed
            for (const QString& Prof : NewProfiles) GameDB->Add(new TGame(Prof));
            if (OK) Result = NewProfiles.size();
            else MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {FailedFile, FailedFile}), mtError, mbOK, 0);
        }
    }

    ExtDeleteFolder(Temp, ftTemp);
    return Result;
}
