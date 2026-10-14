// DFRPackageTool - builds the game packages of the package manager.
//
// D-Fend Reborn addition (see README.md in this folder). It uses the program
// code itself, so the packages have exactly the format that D-Fend Reborn's
// "Build zip packages" export creates and its package manager installs.
//
//   DFRPackageTool split --out DIR PACKAGE...
//       Installs data packages (several games in one package: the original
//       NSIS installers or .7z/.zip archives with Confs/, VirtualHD/, ...)
//       into an empty working data folder and builds one zip package per
//       game in DIR.
//
//   DFRPackageTool list --out LIST.xml --url-base URL [--name NAME] ZIP...
//       Writes a package list (DFRPackagesFile) with one "Game" entry per
//       zip package (name, genre, ..., checksums and size are read from the
//       package and its profile).
//
// The tool works in a temporary data folder (the data of the program folder
// it is started from is not touched) and shows no windows.
#include <QApplication>
#include <QNetworkProxyFactory>
#include <QSslConfiguration>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUrl>

#include <cstdio>

#include "CommonTools.h"
#include "DataPackageUnit.h"
#include "GameDBUnit.h"
#include "HashCalc.h"
#include "LanguageSetupUnit.h"
#include "LoggingUnit.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "ZipInfoFormUnit.h"
#include "ZipPackageUnit.h"

static void Print(const QString& S) { std::fprintf(stdout, "%s\n", qPrintable(S)); std::fflush(stdout); }
static void Error(const QString& S) { std::fprintf(stderr, "DFRPackageTool: %s\n", qPrintable(S)); }

static bool HadError = false;

// Message boxes of the program code: printed, answered with "cancel"/"no"
// (or "OK" if that is the only button); errors make the tool fail.
static int ConsoleMessageDlg(const QString& Caption, const QString& Msg, TMsgDlgType DlgType, TMsgDlgButtons Buttons)
{
    std::fprintf(stderr, "[%s] %s\n", qPrintable(Caption), qPrintable(QString(Msg).replace('\n', ' ')));
    if (DlgType == mtError) HadError = true;
    if (Buttons & mbAbort) return mrAbort;
    if (Buttons & mbCancel) return mrCancel;
    if (Buttons & mbNo) return mrNo;
    return mrOk;
}

static int Usage()
{
    std::fprintf(stderr,
        "Usage:\n"
        "  DFRPackageTool split --out DIR PACKAGE...\n"
        "      one zip package per game of the data packages (NSIS installers / .7z / .zip)\n"
        "  DFRPackageTool list --out LIST.xml --url-base URL [--name NAME] ZIP...\n"
        "      package list with one \"Game\" entry per zip package\n");
    return 2;
}

// Copies the files the program needs from the real program folder into the
// working folder (language files, helper programs).
static void PrepareWorkDir(const QString& RealPrgDir, const QString& WorkDir)
{
    for (const QString& Sub : {LanguageSubDir, BinFolder}) {
        const QString Src = RealPrgDir + Sub;
        if (!DirectoryExists(Src)) continue;
        QDirIterator It(Src, QDir::Files, QDirIterator::Subdirectories);
        while (It.hasNext()) {
            It.next();
            const QString Dest = WorkDir + Sub + PathDelim + QDir(Src).relativeFilePath(It.filePath());
            ForceDirectories(ExtractFilePath(Dest));
            QFile::copy(It.filePath(), Dest);
        }
    }
    ForceDirectories(WorkDir + GameListSubDir);
}

static QString UniqueFileName(const QString& Dir, const QString& Name, const QString& Ext, QSet<QString>& Used)
{
    // only A-Z, a-z, 0-9, ".", "_" and "-" (GitHub renames release files
    // with other characters, e.g. spaces become dots)
    QString Base = Name;
    static const QRegularExpression Other("[^A-Za-z0-9._-]+");
    Base.replace(Other, "-");
    while (Base.startsWith('-') || Base.startsWith('.')) Base.remove(0, 1);
    while (Base.endsWith('-') || Base.endsWith('.')) Base.chop(1);
    if (Base.isEmpty()) Base = "Game";
    QString F = Base + Ext;
    for (int I = 2; Used.contains(F.toUpper()) || FileExists(Dir + F); I++) F = Base + "-" + QString::number(I) + Ext;
    Used.insert(F.toUpper());
    return F;
}

static int Split(const QString& OutDir, const QStringList& Packages)
{
    ForceDirectories(OutDir);
    const QString Out = IncludeTrailingPathDelimiter(QDir::toNativeSeparators(QFileInfo(OutDir).absoluteFilePath()));
    TGameDB GameDB(PrgDataDir() + GameListSubDir);
    QSet<QString> Used;
    int Total = 0;

    for (const QString& Package : Packages) {
        QSet<TGame*> Before;
        for (int I = 0; I < GameDB.Count(); I++) Before.insert(GameDB.Game(I));
        Print("Installing " + Package);
        const int N = InstallDataPackage(nullptr, QFileInfo(Package).absoluteFilePath(), &GameDB);
        if (N < 0) { Error("could not install " + Package); return 1; }
        for (int I = 0; I < GameDB.Count(); I++) {
            TGame* G = GameDB.Game(I);
            if (Before.contains(G)) continue;
            const QString F = UniqueFileName(Out, G->CacheName, ".zip", Used);
            BuildZipPackage(nullptr, G, Out + F, false);
            if (!FileExists(Out + F)) { Error("could not build the package for " + G->CacheName); return 1; }
            Print("  " + G->CacheName + " -> " + F);
            Total++;
        }
    }
    Print(QString::number(Total) + " game packages");
    return 0;
}

static QString Attr(const QString& S)
{
    return S.toHtmlEscaped();   // (also escapes the double quotes)
}

static int List(const QString& OutFile, const QString& URLBase, const QString& ListName, const QStringList& Zips)
{
    QStringList Entries;
    for (const QString& Zip : Zips) {
        const QString Temp = TempDir() + "DFRPackageToolZip" + PathDelim;
        if (DirectoryExists(Temp)) ExtDeleteFolder(Temp, ftTemp);
        ForceDirectories(Temp);
        if (!ExtractZipFile(nullptr, QFileInfo(Zip).absoluteFilePath(), Temp)) { Error("could not extract " + Zip); return 1; }
        const QStringList Profs = QDir(Temp).entryList({"*.prof"}, QDir::Files);
        if (Profs.size() != 1) { Error(Zip + " is not a game package (one profile expected in the root folder)"); return 1; }
        TGame* G = new TGame(Temp + Profs[0]);
        const QString Name = Trim(G->Name()).isEmpty() ? G->CacheName : Trim(G->Name());
        QString E = QString("  <Game\n"
            "    Name=\"%1\"\n    Genre=\"%2\"\n    Developer=\"%3\"\n    Publisher=\"%4\"\n    Year=\"%5\"\n"
            "    Language=\"%6\"\n    License=\"%7\"\n    PackageChecksum=\"%8\"\n")
            .arg(Attr(Name), Attr(G->Genre()), Attr(G->Developer()), Attr(G->Publisher()), Attr(G->Year()),
                 Attr(G->Language()), Attr(G->License()), GetMD5Sum(Zip, false).toUpper());
        if (!Trim(G->GameExeMD5()).isEmpty()) E += QString("    GameExeChecksum=\"%1\"\n").arg(Trim(G->GameExeMD5()).toUpper());
        E += QString("    Size=\"%1\">%2</Game>").arg(QFileInfo(Zip).size()).arg(Attr(URLBase + QString::fromLatin1(QUrl::toPercentEncoding(QFileInfo(Zip).fileName()))));
        Entries << E;
        Print("  " + Name);
        delete G;
        ExtDeleteFolder(Temp, ftTemp);
    }

    QFile F(OutFile);
    if (!F.open(QIODevice::WriteOnly | QIODevice::Truncate)) { Error("could not write " + OutFile); return 1; }
    QTextStream T(&F);
    T.setEncoding(QStringConverter::Utf8);
    T << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n";
    T << "<DFRPackagesFile Name=\"" << Attr(ListName) << "\" LastUpdateDate=\"" << QDate::currentDate().toString("MM/dd/yyyy") << "\">\n";
    for (const QString& E : Entries) T << E << "\n";
    T << "</DFRPackagesFile>\n";
    Print(QString::number(Entries.size()) + " games in " + OutFile);
    return 0;
}

// Only real problems are shown (the hidden dialogs of the program code cause
// "This plugin does not support ..." messages of the offscreen platform)
static void MessageHandler(QtMsgType Type, const QMessageLogContext&, const QString& Msg)
{
    if (Type == QtCriticalMsg || Type == QtFatalMsg) std::fprintf(stderr, "%s\n", qPrintable(Msg));
}

int main(int argc, char* argv[])
{
    qInstallMessageHandler(MessageHandler);
    MessageDlgHook = ConsoleMessageDlg;
    // no windows (the program code shows progress dialogs)
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication App(argc, argv);

    // D-Fend Reborn: all downloads (package lists and packages, update check,
    // data reader, cheats database) use the proxy settings of the system
    // (Windows: Internet options; Linux: http_proxy / https_proxy / no_proxy
    // and, if Qt supports it, the desktop settings)
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    // Certificates named by SSL_CERT_FILE (the OpenSSL convention, e.g. the
    // certificate of a company proxy) are trusted in addition to the system
    // certificates (Qt itself only reads the system certificate store)
    {
        const QString CertFile = qEnvironmentVariable("SSL_CERT_FILE");
        if (!CertFile.isEmpty() && QFileInfo::exists(CertFile)) {
            QSslConfiguration SSL = QSslConfiguration::defaultConfiguration();
            SSL.addCaCertificates(CertFile);
            QSslConfiguration::setDefaultConfiguration(SSL);
        }
    }

    QStringList Args = App.arguments().mid(1);
    if (Args.isEmpty()) return Usage();
    const QString Command = Args.takeFirst();
    QString Out, URLBase, ListName = "D-Fend Reborn game packages";
    QStringList Files;
    while (!Args.isEmpty()) {
        const QString A = Args.takeFirst();
        if (A == "--out" && !Args.isEmpty()) Out = Args.takeFirst();
        else if (A == "--url-base" && !Args.isEmpty()) URLBase = Args.takeFirst();
        else if (A == "--name" && !Args.isEmpty()) ListName = Args.takeFirst();
        else if (A.startsWith("--")) return Usage();
        else Files << A;
    }
    if (Out.isEmpty() || Files.isEmpty()) return Usage();
    if (Command == "list" && URLBase.isEmpty()) return Usage();
    if (Command != "split" && Command != "list") return Usage();

    // empty working data folder
    QTemporaryDir Work;
    if (!Work.isValid()) { Error("could not create a temporary folder"); return 1; }
    const QString RealPrgDir = PrgDir();
    TempPrgDir = IncludeTrailingPathDelimiter(QDir::toNativeSeparators(Work.path()));
    PrepareWorkDir(RealPrgDir, TempPrgDir);

    PrgConsts_Initialization();
    LoggingUnit_Initialization();
    PrgSetupUnit_Initialization();
    LanguageSetupUnit_Initialization();
    GameDBUnit_Initialization();

    int Result;
    if (Command == "split") Result = Split(Out, Files);
    else Result = List(Out, URLBase, ListName, Files);

    GameDBUnit_Finalization();
    LanguageSetupUnit_Finalization();
    PrgSetupUnit_Finalization();
    TempPrgDir = "";
    if (Result == 0 && HadError) Result = 1;
    return Result;
}
