// C++ port of DataReaderBaseUnit.pas (implementation section).
#include "DataReaderBaseUnit.h"
#include "PrgSetupUnit.h"
#include "DataReaderToolsUnit.h"
#include "Process.h"

/* TDataReader */

TDataReader::TDataReader()
{
    FGameNames = new TStringList;
    FGameURLs = new TStringList;
    FLastListRequest = "";
    FLastUpdateCheckOK = true;
}

TDataReader::~TDataReader()
{
    delete FGameNames;
    delete FGameURLs;
}

bool TDataReader::LoadConfig(const QString& AConfigFile, bool UpdateCheck)
{
    Q_UNUSED(AConfigFile); Q_UNUSED(UpdateCheck);
    return true;
}

void TDataReader::OpenListPage(const QString& ASearchString)
{
    QString URL;
    if (PrgSetup->DataReaderAllPlatforms()) URL = FBrowserURLDOS; else URL = FBrowserURLAll;
    if (URL.isEmpty()) return;
    ShellOpen(Format(URL, {EncodeName(ASearchString)}));
}
