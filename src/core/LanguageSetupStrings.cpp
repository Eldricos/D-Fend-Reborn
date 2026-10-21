// Default (English) language strings of D-Fend Reborn: registers the strings
// of the table in LanguageStrings.inc (number, section, key, default text).
#include "LanguageSetupUnit.h"
#include "PrgConsts.h"
#include "GameDBUnit.h"

// one name per DOSBox Staging setting of the profiles
static_assert(NR_ProfileEditorStagingSetting15 == NR_ProfileEditorStagingSettingFirst + StagingSettingCount - 1);

namespace {

struct TLanguageStringDef {
    int Nr;
    const char* Section;
    const char* Key;
    const char* Text;          // UTF-8
    QString (*Expression)();   // (LANG_STRING_EXPR)
};

const TLanguageStringDef LanguageStringDefs[] = {
#define LANG_STRING(Name, Accessor, Nr, Section, Key, Text) {Nr, Section, Key, Text, nullptr},
#define LANG_STRING_NO_ACCESSOR(Name, Nr, Section, Key, Text) {Nr, Section, Key, Text, nullptr},
#define LANG_STRING_EXPR(Name, Accessor, Nr, Section, Key, Text) {Nr, Section, Key, nullptr, []() -> QString { return Text; }},
#define LANG_STRING_INFO(Name, Accessor, Nr, Section, Key)
#include "LanguageStrings.inc"
};

}  // namespace

void TLanguageSetup::SetupLanguageStrings()
{
    for (const TLanguageStringDef& D : LanguageStringDefs)
        AddStringRec(D.Nr, QLatin1String(D.Section), QLatin1String(D.Key), D.Expression ? D.Expression() : QString::fromUtf8(D.Text));
}
