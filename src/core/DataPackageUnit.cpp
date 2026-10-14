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
#include <QStringDecoder>
#ifndef Q_OS_WIN
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#endif

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

#ifndef Q_OS_WIN
// File names in NSIS installers are stored in the Windows code page; 7-Zip
// writes them unchanged on Linux, which gives names that are no valid UTF-8
// (and cannot be handled by Qt). They are renamed to UTF-8.
static void FixNonUtf8FileNames(const QByteArray& Dir)
{
    DIR* D = opendir(Dir.constData());
    if (D == nullptr) return;
    QList<QPair<QByteArray, bool>> Entries;
    while (dirent* E = readdir(D)) {
        const QByteArray Name(E->d_name);
        if (Name == "." || Name == "..") continue;
        bool IsDir = (E->d_type == DT_DIR);
        if (E->d_type == DT_UNKNOWN) { struct stat St; IsDir = (stat((Dir + "/" + Name).constData(), &St) == 0) && S_ISDIR(St.st_mode); }
        Entries << qMakePair(Name, IsDir);
    }
    closedir(D);
    for (auto& [Name, IsDir] : Entries) {
        QStringDecoder Utf8(QStringDecoder::Utf8);
        QString Unused = Utf8(Name);
        if (Utf8.hasError()) {
            QStringDecoder Ansi("windows-1252");
            const QString Fixed = Ansi.isValid() ? QString(Ansi(Name)) : QString::fromLatin1(Name);
            const QByteArray NewName = Fixed.toUtf8();
            if (std::rename((Dir + "/" + Name).constData(), (Dir + "/" + NewName).constData()) == 0) Name = NewName;
        }
        if (IsDir) FixNonUtf8FileNames(Dir + "/" + Name);
    }
}
#endif

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
#ifndef Q_OS_WIN
        FixNonUtf8FileNames(QFile::encodeName(ExcludeTrailingPathDelimiter(Temp)));
#endif
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
