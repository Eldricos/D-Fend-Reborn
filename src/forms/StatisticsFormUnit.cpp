// C++ port of StatisticsFormUnit.pas (implementation section).
#include "StatisticsFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "Process.h"

TStatisticsForm* StatisticsForm = nullptr;

// Application.HelpCommand(HELP_CONTEXT, ID)
static void ApplicationHelpContext(int ID)
{
    // TODO(port): Application.HelpCommand is routed through HelpTools (THTMLhelpRouter.OnRouteHelp)
    // in the original; there is no TApplication help hook in Qt.  Call HtmlHelp directly if
    // available, otherwise open the help file with the associated viewer.
    if (GLOBAL_HELPROUTER == nullptr) return;
    const QString HFile = ChangeFileExt(GLOBAL_HELPROUTER->Helpfile(), ".chm");
    if (HtmlHelpA && HtmlHelpA(0, HFile, HH_HELP_CONTEXT, ID) != 0) return;
    if (FileExists(HFile)) ShellOpen(HFile);
}

TStatisticsForm::TStatisticsForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    // OnKeyDown with KeyPreview=True
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key; FormKeyDown(this, K, Mods); return K == 0;
    });
    FormCreate(this);   // OnCreate
}

TStatisticsForm::~TStatisticsForm() = default;

void TStatisticsForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    ui.OKButton->setText(LanguageSetup->OK());
    ui.HelpButton->setText(LanguageSetup->Help());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
}

void TStatisticsForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (ui.HelpButton->isHidden()) return;   // StatisticsForm.HelpButton.Visible (StatisticsForm = self)
    ApplicationHelpContext(ID_HelpStatistics);
}

void TStatisticsForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if (Key == Qt::Key_F1 && Shift == Qt::NoModifier) HelpButtonClick(Sender);
}

// { global }

static int GetFileCount(const QString& Mask)
{
    int result = 0;
    TSearchRec Rec;

    int I = FindFirst(Mask, faAnyFile, Rec);
    while (I == 0) { result++; I = FindNext(Rec); }
    FindClose(Rec);
    return result;
}

static int GetNumberOfIcons()
{
    int result = 0;

    const QString Dir = IncludeTrailingPathDelimiter(PrgDataDir() + IconsSubDir);
    if (!DirectoryExists(Dir)) return result;

    result += GetFileCount(Dir + "*.ico");
    result += GetFileCount(Dir + "*.png");
    result += GetFileCount(Dir + "*.jpeg");
    result += GetFileCount(Dir + "*.jpg");
    result += GetFileCount(Dir + "*.gif");
    result += GetFileCount(Dir + "*.bmp");
    return result;
}

static void BuildStatistics(TGameDB* GameDB, TStringList* St)
{
    TGameDB* TemplateDB;
    TStringList* St2;
    int C;
    QString S;

    St->Add("D-Fend Reloaded " + GetFileVersionAsString());
    St->Add("");

    St->Add(LanguageSetup->StatisticsNumberProfiles() + ": " + IntToStr(GameDB->Count()));

    TemplateDB = new TGameDB(PrgDataDir() + TemplateSubDir, false);
    St->Add(LanguageSetup->StatisticsNumberTemplates() + ": " + IntToStr(TemplateDB->Count()));
    delete TemplateDB;

    TemplateDB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);
    St->Add(LanguageSetup->StatisticsNumberAutoSetupTemplates() + ": " + IntToStr(TemplateDB->Count()));
    delete TemplateDB;

    St->Add(LanguageSetup->StatisticsNumberIcons() + ": " + IntToStr(GetNumberOfIcons()));

    St->Add("");
    St->Add(LanguageSetup->StatisticsNumberProfilesByGenres() + ":");

    St2 = GameDB->GetGenreList(true);
    for (int I = 0; I < St2->Count(); I++) {
        C = 0; S = ExtUpperCase((*St2)[I]);
        for (int J = 0; J < GameDB->Count(); J++) if (ExtUpperCase(GameDB->Game(J)->CacheGenre) == S) C++;
        St->Add("  "
        + GetCustomGenreName((*St2)[I]) + ": " + IntToStr(C));
    }
    delete St2;
}

void ShowStatisticsDialog(QWidget* AOwner, TGameDB* AGameDB)
{
    TStringList St;
    BuildStatistics(AGameDB, &St);
    ShowInfoTextDialog(AOwner, LanguageSetup->StatisticsCaption(), &St);
}

void ShowInfoTextDialog(QWidget* AOwner, const QString& ACaption, TStringList* AText, bool WordWrap)
{
    StatisticsForm = new TStatisticsForm(AOwner);
    StatisticsForm->setWindowTitle(ACaption);
    // Memo.Lines.AddStrings(AText)
    QString T = StatisticsForm->ui.Memo->toPlainText();
    for (int I = 0; I < AText->Count(); I++) { if (!T.isEmpty()) T += "\n"; T += (*AText)[I]; }
    StatisticsForm->ui.Memo->setPlainText(T);
    StatisticsForm->ui.Memo->setLineWrapMode(WordWrap ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
    StatisticsForm->ui.HelpButton->setVisible(false);
    StatisticsForm->ShowModal();
    delete StatisticsForm;
    StatisticsForm = nullptr;
}
