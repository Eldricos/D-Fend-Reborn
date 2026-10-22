// C++ port of LanguageSetupUnit.pas (interface section). The language strings
// (numbers, accessors, default texts) are defined in LanguageStrings.inc.
#pragma once

#include "DelphiCompat.h"
#include "Graphics.h"
#include "ComCtrls.h"
#include <QtWidgets>
#include <QtXml>
#include <QtNetwork>
#include <functional>
#include "CommonComponents.h"

// Language string numbers NR_<Name> (src/core/LanguageStrings.inc)
#define LANG_STRING(Name, Accessor, Nr, Section, Key, Text) constexpr int NR_##Name = Nr;
#define LANG_STRING_NO_ACCESSOR(Name, Nr, Section, Key, Text) constexpr int NR_##Name = Nr;
#define LANG_STRING_EXPR(Name, Accessor, Nr, Section, Key, Text) constexpr int NR_##Name = Nr;
#define LANG_STRING_INFO(Name, Accessor, Nr, Section, Key) constexpr int NR_##Name = Nr;
#include "LanguageStrings.inc"
// names of the DOSBox Staging settings (StagingSettings in GameDBUnit.h): NR_ProfileEditorStagingSettingFirst + I
constexpr int NR_ProfileEditorStagingSettingFirst = NR_ProfileEditorStagingSetting0;

class TLanguageSetup : public TBasePrgSetup
{
public:
    TLanguageSetup(const QString& FileName = QString(), bool MinimalInit = false);
    ~TLanguageSetup() override;
    // D-Fend Reborn: "... in Explorer" texts; on other systems than Windows the
    // ".Linux" variant (file manager) if the language file has one
    QString FileManagerText(int WindowsNr, int OtherNr)
    {
#ifndef Q_OS_WIN
        const QString S = GetString(OtherNr);
        if (!S.trimmed().isEmpty()) return S;
#else
        Q_UNUSED(OtherNr);
#endif
        return GetString(WindowsNr);
    }
    // properties: LanguageStrings.inc (Accessor() / SetAccessor())
#define LANG_STRING(Name, Accessor, Nr, Section, Key, Text) QString Accessor() { return GetString(NR_##Name); } void Set##Accessor(const QString& Value) { SetString(NR_##Name, Value); }
#define LANG_STRING_NO_ACCESSOR(Name, Nr, Section, Key, Text)
#define LANG_STRING_EXPR(Name, Accessor, Nr, Section, Key, Text) LANG_STRING(Name, Accessor, Nr, Section, Key, Text)
#define LANG_STRING_INFO(Name, Accessor, Nr, Section, Key) LANG_STRING(Name, Accessor, Nr, Section, Key, "")
#include "LanguageStrings.inc"
    // texts with a hand-written accessor
    QString MenuExtrasIconManagerToolsExplorer() { return FileManagerText(NR_MenuExtrasIconManagerToolsExplorer, NR_MenuExtrasIconManagerToolsExplorerLinux); }
    void SetMenuExtrasIconManagerToolsExplorer(const QString& Value) { SetString(NR_MenuExtrasIconManagerToolsExplorer, Value); }
    QString ProfileEditorStagingSetting(int I) { return GetString(NR_ProfileEditorStagingSettingFirst + I); }
    QString WizardFormExplorer() { return FileManagerText(NR_WizardFormExplorer, NR_WizardFormExplorerLinux); }
    void SetWizardFormExplorer(const QString& Value) { SetString(NR_WizardFormExplorer, Value); }
    QString ViewDataFilesOpenHint() { return FileManagerText(NR_ViewDataFilesOpenHint, NR_ViewDataFilesOpenHintLinux); }
    void SetViewDataFilesOpenHint(const QString& Value) { SetString(NR_ViewDataFilesOpenHint, Value); }
    QString QuickStarterMenuExplorer() { return FileManagerText(NR_QuickStarterMenuExplorer, NR_QuickStarterMenuExplorerLinux); }
    void SetQuickStarterMenuExplorer(const QString& Value) { SetString(NR_QuickStarterMenuExplorer, Value); }
    QString AutoDetectGamesOpenFolderButtonHint() { return FileManagerText(NR_AutoDetectGamesOpenFolderButtonHint, NR_AutoDetectGamesOpenFolderButtonHintLinux); }
    void SetAutoDetectGamesOpenFolderButtonHint(const QString& Value) { SetString(NR_AutoDetectGamesOpenFolderButtonHint, Value); }
private:
    void SetupLanguageStrings();
};
QString MaskUnderlineAmpersand(const QString& S);
QString RemoveUnderline(const QString& S);
void LoadLanguage(const QString& LanguageFile);
void LoadGameListTranslations();
void GetDefaultGameListTranslations(TStringList*& DefaultGenreEnglish, TStringList*& DefaultGenreCustom, TStringList*& DefaultLanguageEnglish, TStringList*& DefaultLanguageCustom, TStringList*& DefaultLicenseEnglish, TStringList*& DefaultLicenseCustom);
void SaveGameListTranslations();
QString GetCustomGenreName(const QString& S);
TStringList* GetCustomGenreName(TStringList* St);
QString GetCustomLanguageName(const QString& S);
TStringList* GetCustomLanguageName(TStringList* St);
QString GetCustomLicenseName(const QString& S);
TStringList* GetCustomLicenseName(TStringList* St);
QString GetEnglishGenreName(const QString& S);
QString GetEnglishLanguageName(const QString& S);
QString GetEnglishLicenseName(const QString& S);
TStringList* ExtGenreList(TStringList* St);
TStringList* ExtLanguageList(TStringList* St);
TStringList* ExtLicenseList(TStringList* St);
TStringList* GetLanguageList();
QString LanguageFileNameFromName(const QString& Name);
int FindLanguageInList(const QString& Lang, TStringList* List);
int FindEnglishInList(TStringList* List);
QString ShortLanguageName(const QString& LongLanguageName);
extern TLanguageSetup* LanguageSetup;
extern QString LanguageSpecialLowerCase;
extern QString LanguageSpecialUpperCase;
extern QString LanguageSetupFastYes;
extern QString LanguageSetupFastNo;
extern TStringList* LanguageSetupEnglishGenres;
extern TStringList* LanguageSetupEnglishGenresUpper;
extern TStringList* LanguageSetupCustomGenres;
extern TStringList* LanguageSetupCustomGenresUpper;
extern TStringList* LanguageSetupEnglishLanguages;
extern TStringList* LanguageSetupEnglishLanguagesUpper;
extern TStringList* LanguageSetupCustomLanguages;
extern TStringList* LanguageSetupCustomLanguagesUpper;
extern TStringList* LanguageSetupEnglishLicenses;
extern TStringList* LanguageSetupEnglishLicensesUpper;
extern TStringList* LanguageSetupCustomLicenses;
extern TStringList* LanguageSetupCustomLicensesUpper;

// initialization / finalization sections of LanguageSetupUnit.pas
// (requires PrgSetup, i.e. PrgSetupUnit_Initialization() must run first)
void LanguageSetupUnit_Initialization();
void LanguageSetupUnit_Finalization();
