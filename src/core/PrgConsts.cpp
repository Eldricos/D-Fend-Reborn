// C++ port of PrgConsts.pas (implementation section).
#include "PrgConsts.h"

#include <QCoreApplication>

QString MainSetupFile;
QString OperationModeConfig;

void PrgConsts_Initialization()
{
    MainSetupFile = ChangeFileExt(ExtractFileName(QCoreApplication::applicationFilePath()), ".ini");
    OperationModeConfig = ChangeFileExt(ExtractFileName(QCoreApplication::applicationFilePath()), ".dat");
}
