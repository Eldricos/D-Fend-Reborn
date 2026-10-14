// C++ port of SetupFrameLanguageUnit.pas (implementation section).
#include "SetupFrameLanguageUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "HelpConsts.h"
#include "Process.h"

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

namespace {

// Emulates TComboBox.OnDropDown: called before the list of the combo box opens.
class TDropDownHook : public QObject
{
public:
    TDropDownHook(QComboBox *Combo, std::function<void()> Handler) : QObject(Combo), FHandler(std::move(Handler))
    {
        Combo->installEventFilter(this);
    }
    bool eventFilter(QObject *, QEvent *E) override
    {
        if (E->type() == QEvent::MouseButtonPress) FHandler();
        if (E->type() == QEvent::KeyPress) {
            auto *K = static_cast<QKeyEvent *>(E);
            if (K->key() == Qt::Key_F4 || K->key() == Qt::Key_Space ||
                ((K->modifiers() & Qt::AltModifier) && (K->key() == Qt::Key_Down || K->key() == Qt::Key_Up))) FHandler();
        }
        return false;
    }
private:
    std::function<void()> FHandler;
};

// ComboBox.Items as TStringList (Objects = integer item data)
TStringList ComboItems(QComboBox *Combo)
{
    TStringList St;
    for (int I = 0; I < Combo->count(); I++) St.AddObject(Combo->itemText(I), static_cast<intptr_t>(Combo->itemData(I).toInt()));
    return St;
}

}  // namespace

TSetupFrameLanguage::TSetupFrameLanguage(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
#ifndef Q_OS_WIN
    // D-Fend Reborn: the installer language is only used by the Windows
    // installers (stored in the Windows registry)
    ui.InstallerLangLabel->setVisible(false);
    ui.InstallerLangEditComboBox->setVisible(false);
    ui.InstallerLangInfoLabel->setVisible(false);
#endif

    Timer = new QTimer(this);   // DFM: Enabled = False, Interval = 1000
    Timer->setInterval(1000);

    // DFM event wiring
    connect(ui.LanguageComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int) { LanguageComboBoxChange(ui.LanguageComboBox); });
    new TDropDownHook(ui.LanguageComboBox, [this]() { ComboBoxDropDown(ui.LanguageComboBox); });
    connect(ui.DosBoxLangEditComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int) { DosBoxLangEditComboBoxChange(ui.DosBoxLangEditComboBox); });
    new TDropDownHook(ui.DosBoxLangEditComboBox, [this]() { ComboBoxDropDown(ui.DosBoxLangEditComboBox); });
    connect(ui.LanguageOpenEditor, &QPushButton::clicked, this, [this]() { ButtonWork(ui.LanguageOpenEditor); });
    connect(ui.LanguageNew, &QPushButton::clicked, this, [this]() { ButtonWork(ui.LanguageNew); });
    connect(ui.InstallerLangEditComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int) { InstallerLangEditComboBoxChange(ui.InstallerLangEditComboBox); });
    new TDropDownHook(ui.InstallerLangEditComboBox, [this]() { ComboBoxDropDown(ui.InstallerLangEditComboBox); });
    connect(Timer, &QTimer::timeout, this, [this]() { TimerTimer(Timer); });

    // constructor TSetupFrameLanguage.Create
    DosBoxLang = new TStringList;
    JustLoading = false;
}

TSetupFrameLanguage::~TSetupFrameLanguage()
{
    delete DosBoxLang;
}

/* TSetupFrameLanguage */

QString TSetupFrameLanguage::GetName()
{
    return LanguageSetup->SetupFormLanguageSheet();
}

void TSetupFrameLanguage::InitGUIAndLoadSetup(TInitData& InitData)
{
    TStringList* St;
    int I;

    // NoFlicker(...): not needed in Qt

    PDosBoxDir = InitData.PDosBoxDir;
    PDOSBoxLang = InitData.PDOSBoxLang;
    BeforeLanguageChangeNotify = InitData.BeforeLanguageChangeNotify;
    LanguageChangeNotify = InitData.LanguageChangeNotify;
    OpenLanguageEditor = InitData.OpenLanguageEditorEvent;

    JustLoading = true;
    St = GetLanguageList();
    for (int J = 0; J < St->Count(); J++) {
        ui.LanguageComboBox->addItem((*St)[J], St->ObjectAsInt(J));
        ui.InstallerLangEditComboBox->addItem((*St)[J], St->ObjectAsInt(J));
    }
    delete St;
    TStringList Items = ComboItems(ui.LanguageComboBox);
    I = FindLanguageInList(ChangeFileExt(PrgSetup->Language(), ""), &Items);
    if (I >= 0) ui.LanguageComboBox->setCurrentIndex(I); else {
        I = FindEnglishInList(&Items);
        if (I >= 0) ui.LanguageComboBox->setCurrentIndex(I); else {
            if (ui.LanguageComboBox->count() > 0) ui.LanguageComboBox->setCurrentIndex(0);
        }
    }

    LanguageComboBoxChange(this); /* to set up outdated warning */

    ShowFrame(true); /* set DOSBox languag combo box */

    ReadCurrentInstallerLanguage();
    JustLoading = false;

    SetComboHint(ui.LanguageComboBox);
    SetComboHint(ui.DosBoxLangEditComboBox);
    SetComboHint(ui.InstallerLangEditComboBox);
}

void TSetupFrameLanguage::ComboBoxDropDown(QObject* Sender)
{
    SetComboDropDownDropDownWidth(qobject_cast<QComboBox*>(Sender));
}

void TSetupFrameLanguage::InstallerLangEditComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    Timer->stop();
    if (ui.InstallerLangEditComboBox->currentIndex() < 0) return;
    I = ui.InstallerLangEditComboBox->itemData(ui.InstallerLangEditComboBox->currentIndex()).toInt();
    if ((I == InstallerLang) || (I == -1)) return;

#ifdef Q_OS_WIN
    // ShellExecute (not CreateProcess): SetInstallerLanguage.exe needs admin
    // rights (HKLM), so Windows has to show the UAC prompt
    const QString Exe = FileExists(PrgDir() + "SetInstallerLanguage.exe") ? PrgDir() + "SetInstallerLanguage.exe" : PrgDir() + BinFolder + PathDelim + "SetInstallerLanguage.exe";
    ShellExecuteW(reinterpret_cast<HWND>(window()->winId()), L"open", reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(Exe).utf16()),
                  reinterpret_cast<LPCWSTR>(IntToStr(I).utf16()), nullptr, SW_SHOW);
#endif
    SetComboHint(ui.InstallerLangEditComboBox);
    LangTimerCounter = 10;
    Timer->start();
}

void TSetupFrameLanguage::BeforeChangeLanguage()
{
}

void TSetupFrameLanguage::LoadLanguage()
{
    ui.LanguageLabel->setText(LanguageSetup->SetupFormLanguage());
    ui.LanguageOpenEditor->setText(LanguageSetup->SetupFormLanguageOpenEditor());
    ui.LanguageNew->setText(LanguageSetup->SetupFormLanguageOpenEditorNew());
    ui.DosBoxLangLabel->setText(LanguageSetup->SetupFormDOSBoxLang());
    ui.DosBoxLangEditComboBox->setToolTip(LanguageSetup->SetupFormDOSBoxLangHint());
    ui.InstallerLangLabel->setText(LanguageSetup->SetupFormInstallerLang());
    ui.InstallerLangInfoLabel->setText(LanguageSetup->SetupFormInstallerLangInfo());

    setProperty("HelpContext", ID_FileOptionsLanguage);
}

static void FindAndAddLngFiles(const QString &Dir, TStringList *St, TStringList *St2)
{
    TSearchRec Rec;
    int I = FindFirst(Dir + "*.lng", faAnyFile, Rec);
    while (I == 0) {
        St->Add(ChangeFileExt(Rec.Name, ""));
        St2->Add(Dir + Rec.Name);
        I = FindNext(Rec);
    }
    FindClose(Rec);
}

QString TSetupFrameLanguage::ExtendLanguageName(const QString& ShortName)
{
    int I;
    QString ShortNameUpper;
    QString S, T;

    ShortNameUpper = ExtUpperCase(ShortName);
    for (I = 0; I < ui.LanguageComboBox->count(); I++) {
        S = ui.LanguageComboBox->itemText(I);
        if (Pos("(", S) == 0) continue;
        T = Copy(S, Pos("(", S) + 1, MaxInt);
        if (Pos(")", S) == 0) continue;
        T = Copy(T, 1, Pos(")", T) - 1);
        if (ExtUpperCase(T) == ShortNameUpper) return S;
    }

    return ShortName;
}

void TSetupFrameLanguage::DOSBoxDirChanged()
{
    QString S, S2;
    int I, J;

    S = ui.DosBoxLangEditComboBox->currentText();
    if (Pos("(", S) > 0) {
        S2 = Copy(S, Pos("(", S) + 1);
        if (Pos(")", S2) > 0) S2 = Copy(S2, 1, Pos(")", S2) - 1);
        if (S2 != "") S = S2;
    }

    ui.DosBoxLangEditComboBox->clear();
    DosBoxLang->Clear();

    TStringList St;
    St.Add("English"); DosBoxLang->Add("");
    FindAndAddLngFiles(IncludeTrailingPathDelimiter(*PDosBoxDir), &St, DosBoxLang);
    FindAndAddLngFiles(PrgDir() + LanguageSubDir + PathDelim, &St, DosBoxLang);

    I = St.IndexOf(S);
    if (I < 0) I = St.IndexOf("English");

    for (J = 0; J < St.Count(); J++) ui.DosBoxLangEditComboBox->addItem(ExtendLanguageName(St[J]));
    if (St.Count() > 0) ui.DosBoxLangEditComboBox->setCurrentIndex(qMax(0, I));
}

void TSetupFrameLanguage::DosBoxLangEditComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    *PDOSBoxLang = (*DosBoxLang)[ui.DosBoxLangEditComboBox->currentIndex()];
    SetComboHint(ui.DosBoxLangEditComboBox);
}

void TSetupFrameLanguage::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
    int I, J;
    QString S;

    DOSBoxDirChanged();
    I = -1; S = ExtractFileName(*PDOSBoxLang);
    for (J = 0; J < DosBoxLang->Count(); J++) if (S == ExtractFileName((*DosBoxLang)[J])) { I = J; break; }
    if (I >= 0) ui.DosBoxLangEditComboBox->setCurrentIndex(I); else ui.DosBoxLangEditComboBox->setCurrentIndex(0);
}

void TSetupFrameLanguage::HideFrame()
{
}

void TSetupFrameLanguage::RestoreDefaults()
{
}

void TSetupFrameLanguage::SaveSetup()
{
    PrgSetup->SetLanguage(ShortLanguageName(ui.LanguageComboBox->currentText()) + ".ini");
    PrgSetup->DOSBoxSettings(0)->SetDosBoxLanguage(*PDOSBoxLang);
}

static int VersionStringToInt(QString S)
{
    int result = 0;
    int I;
    QString T;

    S = Trim(S);

    I = Pos(".", S);
    if (I == 0) { T = S; S = ""; } else { T = Trim(Copy(S, 1, I - 1)); S = Trim(Copy(S, I + 1, MaxInt)); }
    try { result = result + StrToInt(T) * 256 * 256; } catch (...) {}

    I = Pos(".", S);
    if (I == 0) { T = S; S = ""; } else { T = Trim(Copy(S, 1, I - 1)); S = Trim(Copy(S, I + 1, MaxInt)); }
    try { result = result + StrToInt(T) * 256; } catch (...) {}

    try { result = result + StrToInt(S); } catch (...) {}

    return result;
}

void TSetupFrameLanguage::LanguageComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (!JustLoading) {
        BeforeLanguageChangeNotify();
        ::LoadLanguage(ShortLanguageName(ui.LanguageComboBox->currentText()) + ".ini");
        LanguageChangeNotify();
    }

    S = Trim(LanguageSetup->MaxVersion());
    if ((S == "") || (S == "0")) {
        ui.LanguageInfoLabel->setVisible(true);
        ui.LanguageInfoLabel->setText(LanguageSetup->LanguageNoVersion());
    } else {
        if ((VersionStringToInt(S) / 256) < (VersionStringToInt(GetNormalFileVersionAsString()) / 256)) {
            ui.LanguageInfoLabel->setVisible(true);
            ui.LanguageInfoLabel->setText(Format(LanguageSetup->LanguageOutdated(), {LanguageSetup->MaxVersion(), GetNormalFileVersionAsString()}));
        } else {
            ui.LanguageInfoLabel->setVisible(false);
        }
    }

    SetComboHint(ui.LanguageComboBox);
}

void TSetupFrameLanguage::ButtonWork(QObject* Sender)
{
    if (MessageDlg(LanguageSetup->SetupFormLanguageOpenEditorConfirmation(), mtConfirmation, mbOK | mbCancel, 0) == mrOk)
        OpenLanguageEditor(Sender->property("tag").toInt() + 1);
}

void TSetupFrameLanguage::ReadCurrentInstallerLanguage()
{
    int I, SelLang;

    InstallerLang = -1;
#ifdef Q_OS_WIN
    {
        // (written by the 32 bit NSIS installers: 32 bit registry view)
        QSettings Reg("HKEY_LOCAL_MACHINE\\Software\\D-Fend Reborn", QSettings::Registry32Format);
        if (Reg.contains("Installer Language")) {
            try { InstallerLang = StrToInt(Reg.value("Installer Language").toString()); } catch (...) {}
        }
    }
#endif

    // InstallerLangEditComboBox.OnChange:=nil: "activated" is not emitted for programmatic changes
    const QSignalBlocker Blocker(ui.InstallerLangEditComboBox);

    if (InstallerLang >= 0) {
        SelLang = -1;
        for (I = 0; I < ui.InstallerLangEditComboBox->count(); I++) if (ui.InstallerLangEditComboBox->itemData(I).toInt() == InstallerLang) {
            SelLang = I; break;
        }
    } else {
        SelLang = -1;
    }

    if ((ui.InstallerLangEditComboBox->count() > 0) && (ui.InstallerLangEditComboBox->itemData(ui.InstallerLangEditComboBox->count() - 1).toInt() == -1)) {
        ui.InstallerLangEditComboBox->removeItem(ui.InstallerLangEditComboBox->count() - 1);
    }

    if (SelLang == -1) {
        ui.InstallerLangEditComboBox->addItem("?", -1);
        ui.InstallerLangEditComboBox->setCurrentIndex(ui.InstallerLangEditComboBox->count() - 1);
    } else {
        ui.InstallerLangEditComboBox->setCurrentIndex(SelLang);
    }

    // InstallerLangEditComboBox.OnChange:=InstallerLangEditComboBoxChange;
}

void TSetupFrameLanguage::TimerTimer(QObject* Sender)
{
    Q_UNUSED(Sender);
    LangTimerCounter--;
    if (LangTimerCounter == 0) Timer->stop();
    ReadCurrentInstallerLanguage();
}
