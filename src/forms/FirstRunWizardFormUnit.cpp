// C++ port of FirstRunWizardFormUnit.pas (implementation section).
#include "FirstRunWizardFormUnit.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"
#include "PrgConsts.h"
#include "MainUnit.h"
#include "LoggingUnit.h"
#include "DOSBoxLangTools.h"
#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

TFirstRunWizardForm* FirstRunWizardForm = nullptr;

namespace {

#ifndef VK_F1
constexpr quint16 VK_F1 = 0x70;
#endif

void SetBold(QWidget *W)
{
    QFont F = W->font(); F.setBold(true); W->setFont(F);
}

// GetSystemDefaultLangID
int SystemDefaultLangID()
{
#ifdef Q_OS_WIN
    return GetSystemDefaultLangID();
#else
    // TODO(port): no LANGID outside Windows - map the most common system locales
    static const struct { const char *Name; int ID; } Map[] = {
        {"en_US", 1033}, {"en_GB", 2057}, {"de_DE", 1031}, {"de_AT", 3079}, {"de_CH", 2055}, {"fr_FR", 1036},
        {"es_ES", 3082}, {"it_IT", 1040}, {"nl_NL", 1043}, {"pl_PL", 1045}, {"ru_RU", 1049}, {"pt_BR", 1046},
        {"pt_PT", 2070}, {"cs_CZ", 1029}, {"da_DK", 1030}, {"fi_FI", 1035}, {"sv_SE", 1053}, {"nb_NO", 1044},
        {"hu_HU", 1038}, {"ja_JP", 1041}, {"zh_CN", 2052}, {"zh_TW", 1028}, {"ko_KR", 1042}, {"tr_TR", 1055},
        {"el_GR", 1032}, {"uk_UA", 1058}, {"sk_SK", 1051}, {"sl_SI", 1060}, {"hr_HR", 1050}, {"ro_RO", 1048},
        {"bg_BG", 1026}, {"he_IL", 1037}, {"ar_SA", 1025}, {"ca_ES", 1027}, {"eu_ES", 1069}, {"gl_ES", 1110}
    };
    const QString N = QLocale::system().name();
    for (const auto &M : Map) if (N == QLatin1String(M.Name)) return M.ID;
    const QString L = N.section('_', 0, 0);
    for (const auto &M : Map) if (QString(QLatin1String(M.Name)).section('_', 0, 0) == L) return M.ID;
    return 1033;
#endif
}

int MainVersionStringToInt(QString S)
{
    int I;
    QString T;
    int result = 0;
    bool OK;
    S = Trim(S);

    I = Pos(".", S);
    if (I == 0) { T = S; S.clear(); } else { T = Trim(Copy(S, 1, I - 1)); S = Trim(Copy(S, I + 1, MaxInt)); }
    { int V = T.toInt(&OK); if (OK) result = result + V * 256 * 256; }

    I = Pos(".", S);
    if (I == 0) { T = S; S.clear(); } else { T = Trim(Copy(S, 1, I - 1)); S = Trim(Copy(S, I + 1, MaxInt)); }
    { int V = T.toInt(&OK); if (OK) result = result + V * 256; }
    return result;
}

}  // namespace

TFirstRunWizardForm::TFirstRunWizardForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    connect(ui.DOSBoxButton, &QToolButton::clicked, this, [this]() { DOSBoxButtonClick(ui.DOSBoxButton); });
    connect(ui.ProgramLanguageWarningButton, &QToolButton::clicked, this, [this]() { ProgramLanguageButtonClick(ui.ProgramLanguageWarningButton); });
    connect(ui.DOSBoxWarningButton, &QToolButton::clicked, this, [this]() { ProgramLanguageButtonClick(ui.DOSBoxWarningButton); });
    connect(ui.PortableOKButton, &QToolButton::clicked, this, [this]() { ProgramLanguageButtonClick(ui.PortableOKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.ProgramLanguageComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { ProgramLanguageComboBoxChange(ui.ProgramLanguageComboBox); });
    connect(ui.DOSBoxComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { DOSBoxComboBoxChange(ui.DOSBoxComboBox); });
    connect(ui.DOSBoxEdit, &QLineEdit::textChanged, this, [this]() { DOSBoxEditChange(ui.DOSBoxEdit); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        quint16 K = (Key == Qt::Key_F1) ? VK_F1 : 0;
        if (K == 0) return false;
        FormKeyDown(this, K, Mods & ~Qt::KeypadModifier);
        return false;
    });

    FormCreate(this);
}

TFirstRunWizardForm::~TFirstRunWizardForm()
{
    FormDestroy(this);
}

void TFirstRunWizardForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (E->spontaneous() || FFormShown) return;
    FFormShown = true;
    FormShow(this);
}

void TFirstRunWizardForm::done(int R)
{
    TCloseAction Action = caHide;
    FormClose(this, Action);
    TVclForm::done(R);
}

void TFirstRunWizardForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    LogInfo("First run wizard: FormCreate");

    DosBoxLang = new TStringList;
    JustLoadingLanguage = false;
    DefaultDOSBoxAvailable = Trim(ExtUpperCase(IncludeTrailingPathDelimiter(PrgSetup->DOSBoxSettings(0)->DosBoxDir()))) == Trim(ExtUpperCase(PrgDir() + "DOSBox" + PathDelim));

    // SetVistaFonts(self); - not needed

    UserIconLoader->LoadIcons();
    UserIconLoader->DialogImage(DI_Warning, ui.ProgramLanguageWarningButton);
    UserIconLoader->DialogImage(DI_Warning, ui.DOSBoxWarningButton);
    UserIconLoader->DialogImage(DI_Information, ui.PortableOKButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.DOSBoxButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    LoadGUILanguage();
}

void TFirstRunWizardForm::FormShow(QObject* Sender)
{
    LogInfo("First run wizard: FormShow");

    LoadAndSetupLanguageList();

    ui.DOSBoxEdit->setText(MakeRelPath(PrgSetup->DOSBoxSettings(0)->DosBoxDir(), PrgSetup->BaseDir()));
    DOSBoxEditChange(Sender);

    LoadAndSetupDOSBoxLanguages(true);
}

void TFirstRunWizardForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    delete DosBoxLang; DosBoxLang = nullptr;
}

void TFirstRunWizardForm::LoadGUILanguage()
{
    int I;

    JustLoadingLanguage = true;
    LogInfo("First run wizard: LoadGUILanguage");

    // General GUI elements
    // ParentFont:=True; Font.Charset:=... - not needed
    SetBold(ui.ProgramLanguageLabel);
    SetBold(ui.DOSBoxLabel);
    SetBold(ui.DOSBoxLanguageLabel);
    SetBold(ui.UpdatesCheckBox);

    setWindowTitle(LanguageSetup->FirstRunWizard());
    ui.StartLabel->setText(LanguageSetup->FirstRunWizardInfoOverview());
    ui.ProgramLanguageLabel->setText(LanguageSetup->FirstRunWizardProgramLanguage());
    ui.ProgramLanguageWarningButton->setText(LanguageSetup->LanguageOutdatedShort());
    ui.DOSBoxLabel->setText(LanguageSetup->FirstRunWizardDOSBoxDir());
    ui.DOSBoxButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.DOSBoxLanguageLabel->setText(LanguageSetup->FirstRunWizardDOSBoxLanguage());
    ui.UpdatesCheckBox->setText(LanguageSetup->FirstRunWizardAutoUpdate());
    ui.UpdatesLabel->setText(LanguageSetup->FirstRunWizardAutoUpdateInfo());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.HelpButton->setText(LanguageSetup->Help());

    // DOSBox selection
    I = ui.DOSBoxComboBox->currentIndex();
    ui.DOSBoxComboBox->clear();
    ui.DOSBoxComboBox->addItem(LanguageSetup->FirstRunWizardDOSBoxDirDefault());
    ui.DOSBoxComboBox->addItem(LanguageSetup->FirstRunWizardDOSBoxDirCustom());
    ui.DOSBoxComboBox->setCurrentIndex(Max(0, I));
    ui.DOSBoxEdit->setEnabled((ui.DOSBoxComboBox->count() == 0) || (ui.DOSBoxComboBox->currentIndex() == 1));
    ui.DOSBoxButton->setEnabled((ui.DOSBoxComboBox->count() == 0) || (ui.DOSBoxComboBox->currentIndex() == 1));

    // Special labels
    ui.PortableOKButton->setText(LanguageSetup->FirstRunWizardDOSBoxDirPortableOKShort());
    ui.PortableOKButton->setToolTip(LanguageSetup->FirstRunWizardDOSBoxDirPortableOK());
    SetDOSBoxWarningLanguage();

    JustLoadingLanguage = false;
}

void TFirstRunWizardForm::SetDOSBoxWarningLanguage()
{
    QString DOSBoxVersion, S;

    if (DOSBoxWarningType == wtNoDir) {
        ui.DOSBoxWarningButton->setText(LanguageSetup->FirstRunWizardDOSBoxDirNoDOSBoxShort());
        ui.DOSBoxWarningButton->setToolTip(LanguageSetup->FirstRunWizardDOSBoxDirNoDOSBox());
    }

    if (DOSBoxWarningType == wtOld) {
        DOSBoxVersion = CheckDOSBoxVersion(-1, ui.DOSBoxEdit->text());
        ui.DOSBoxWarningButton->setText(LanguageSetup->MessageDOSBoxOutdatedShort());
        S = FloatToStr(MinSupportedDOSBoxVersion);
        S.replace(',', '.');
        ui.DOSBoxWarningButton->setToolTip(Format(LanguageSetup->MessageDOSBoxOutdated(), {DOSBoxVersion, S}));
    }
}

void TFirstRunWizardForm::LoadAndSetupLanguageList()
{
    int I, J;
    bool InstallerLanguageOK;

    JustLoadingLanguage = true;
    LogInfo("First run wizard: LoadAndSetupLanguageList");

    // Load language list
    {
        TStringList* St = GetLanguageList();
        for (I = 0; I < St->Count(); I++) ui.ProgramLanguageComboBox->addItem((*St)[I], St->ObjectAsInt(I));
        delete St;
    }

    // Read installer language
    J = 1033; InstallerLanguageOK = false;
#ifdef Q_OS_WIN
    {
        QSettings Reg("HKEY_LOCAL_MACHINE\\Software\\D-Fend Reloaded", QSettings::NativeFormat);
        if (Reg.contains("Installer Language")) {
            bool OK = false;
            int V = Reg.value("Installer Language").toString().toInt(&OK);
            if (OK) { J = V; InstallerLanguageOK = true; }
        }
    }
#endif

    // Set program language to installer language
    ui.ProgramLanguageComboBox->setCurrentIndex(0);
    for (I = 0; I < ui.ProgramLanguageComboBox->count(); I++) if (ui.ProgramLanguageComboBox->itemData(I).toInt() == J) {
        ui.ProgramLanguageComboBox->setCurrentIndex(I); break;
    }

    // Set program language to system language
    if (!InstallerLanguageOK) {
        J = SystemDefaultLangID();
        for (I = 0; I < ui.ProgramLanguageComboBox->count(); I++) if (ui.ProgramLanguageComboBox->itemData(I).toInt() == J) {
            ui.ProgramLanguageComboBox->setCurrentIndex(I); break;
        }
    }
    JustLoadingLanguage = false;

    ProgramLanguageComboBoxChange(this);
}

void TFirstRunWizardForm::ProgramLanguageComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (JustLoadingLanguage) return;

    LoadLanguage(ShortLanguageName(ui.ProgramLanguageComboBox->currentText()) + ".ini");
    if (DFendReloadedMainForm != nullptr) DFendReloadedMainForm->SelectHelpFile();
    LoadGUILanguage();

    ui.ProgramLanguageWarningButton->setVisible(false);
    S = Trim(LanguageSetup->MaxVersion());
    if (S.isEmpty() || (S == "0")) {
        ui.ProgramLanguageWarningButton->setVisible(true);
        ui.ProgramLanguageWarningButton->setToolTip(LanguageSetup->LanguageNoVersion());
    } else {
        if (MainVersionStringToInt(S) < MainVersionStringToInt(GetNormalFileVersionAsString())) {
            ui.ProgramLanguageWarningButton->setVisible(true);
            ui.ProgramLanguageWarningButton->setToolTip(Format(LanguageSetup->LanguageOutdated(), {LanguageSetup->MaxVersion(), GetNormalFileVersionAsString()}));
        }
    }

    LoadAndSetupDOSBoxLanguages(true);
}

void TFirstRunWizardForm::ProgramLanguageButtonClick(QObject* Sender)
{
    auto *B = qobject_cast<QToolButton *>(Sender);
    const QString Hint = B ? B->toolTip() : QString();
    if ((Sender == ui.ProgramLanguageWarningButton) || (Sender == ui.DOSBoxWarningButton))
        MessageDlg(Hint, mtWarning, mbOK, 0);
    else
        MessageDlg(Hint, mtInformation, mbOK, 0);
}

void TFirstRunWizardForm::DOSBoxButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S = MakeAbsPath(ui.DOSBoxEdit->text(), PrgSetup->BaseDir());
    if (SelectDirectory(THandle(0), LanguageSetup->SetupFormDOSBoxDir(), S)) ui.DOSBoxEdit->setText(MakeRelPath(IncludeTrailingPathDelimiter(S), PrgSetup->BaseDir()));
}

void TFirstRunWizardForm::DOSBoxComboBoxChange(QObject* Sender)
{
    if (JustLoadingLanguage) return;
    if (ui.DOSBoxComboBox->count() == 0) return;
    ui.DOSBoxEdit->setEnabled(ui.DOSBoxComboBox->currentIndex() == 1);
    ui.DOSBoxButton->setEnabled(ui.DOSBoxComboBox->currentIndex() == 1);

    if (ui.DOSBoxComboBox->currentIndex() == 0) {
        ui.DOSBoxEdit->setText(MakeRelPath(PrgSetup->DOSBoxSettings(0)->DosBoxDir(), PrgSetup->BaseDir()));
        DOSBoxEditChange(Sender);
    }
}

void TFirstRunWizardForm::DOSBoxEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S, T, DOSBoxVersion;

    ui.DOSBoxWarningButton->setVisible(false);
    ui.PortableOKButton->setVisible(false);

    S = IncludeTrailingPathDelimiter(MakeAbsPath(ui.DOSBoxEdit->text(), PrgSetup->BaseDir()));
    if (!FileExists(S + DosBoxFileName)) {
        ui.DOSBoxWarningButton->setVisible(true);
        DOSBoxWarningType = wtNoDir;
        SetDOSBoxWarningLanguage();
    } else {
        DOSBoxVersion = CheckDOSBoxVersion(-1, ui.DOSBoxEdit->text());
        ui.DOSBoxWarningButton->setVisible(OldDOSBoxVersion(DOSBoxVersion));
        if (!ui.DOSBoxWarningButton->isHidden()) {
            DOSBoxWarningType = wtOld;   // (the Delphi code relies on the previous value here)
            SetDOSBoxWarningLanguage();
        } else {
            S = IncludeTrailingPathDelimiter(ExtUpperCase(MakeAbsPath(ui.DOSBoxEdit->text(), PrgSetup->BaseDir())));
            T = IncludeTrailingPathDelimiter(ExtUpperCase(PrgDir()));
            if ((OperationMode == omPortable) && (Copy(S, 1, Length(T)) == T)) {
                ui.PortableOKButton->setVisible(true);
            }
        }
    }

    LoadAndSetupDOSBoxLanguages(false);
}

QString TFirstRunWizardForm::ExtendLanguageName(const QString& ShortName)
{
    QString ShortNameUpper = ExtUpperCase(ShortName);
    QString S, T;
    for (int I = 0; I < ui.ProgramLanguageComboBox->count(); I++) {
        S = ui.ProgramLanguageComboBox->itemText(I);
        if (Pos("(", S) == 0) continue;
        T = Copy(S, Pos("(", S) + 1, MaxInt);
        if (Pos(")", S) == 0) continue;
        T = Copy(T, 1, Pos(")", T) - 1);
        if (ExtUpperCase(T) == ShortNameUpper) return S;
    }

    return ShortName;
}

void TFirstRunWizardForm::LoadAndSetupDOSBoxLanguages(bool ResetLang)
{
    int I, J;
    QString S, Save;

    auto ItemsIndexOf = [this](const QString &Text) { return ui.DOSBoxLanguageComboBox->findText(Text, Qt::MatchFixedString); };
    auto ProgramLanguageText = [this]() {
        const int Idx = ui.ProgramLanguageComboBox->currentIndex();
        return Idx >= 0 ? ui.ProgramLanguageComboBox->itemText(Idx) : QString();
    };

    Save = ui.DOSBoxLanguageComboBox->currentText();
    if (Pos("(", Save) > 0) {
        S = Copy(Save, Pos("(", Save) + 1, MaxInt);
        if (Pos(")", S) > 0) S = Copy(S, 1, Pos(")", S) - 1);
        if (!S.isEmpty()) Save = S;
    }

    I = -1;

    DosBoxLang->Clear();
    ui.DOSBoxLanguageComboBox->clear();
    {
        TStringList St;
        GetDOSBoxLangNamesAndFiles(IncludeTrailingPathDelimiter(MakeAbsPath(ui.DOSBoxEdit->text(), PrgSetup->BaseDir())), &St, DosBoxLang, true);

        // Restore old value
        if ((!Save.isEmpty()) && !ResetLang) I = St.IndexOf(Save);

        // Get value from program language list
        if (I < 0) {
            S = ShortLanguageName(ProgramLanguageText());
            I = St.IndexOf(S);
            if ((I < 0) && (Pos(" ", S) > 0)) {
                TStringList* St2 = ValueToList(S, " ");
                for (J = 0; J < St2->Count(); J++) {
                    I = St.IndexOf((*St2)[J]);
                    if (I >= 0) break;
                }
                delete St2;
            }
        }

        // Get value from program language list for language name "German"
        if ((I < 0) && (ExtUpperCase(S) == "GERMAN")) I = St.IndexOf("Deutsch");

        // Fall back to "English"
        if (I < 0) I = St.IndexOf("English");

        for (J = 0; J < St.Count(); J++) ui.DOSBoxLanguageComboBox->addItem(ExtendLanguageName(St[J]));
        ui.DOSBoxLanguageComboBox->setCurrentIndex(I);
    }

    if (I >= 0) { ui.DOSBoxLanguageComboBox->setCurrentIndex(I); return; }

    if ((!Save.isEmpty()) && !ResetLang) {
        I = ItemsIndexOf(Save);
        if (I >= 0) { ui.DOSBoxLanguageComboBox->setCurrentIndex(I); return; }
    }

    S = ShortLanguageName(ProgramLanguageText());
    I = ItemsIndexOf(S);
    if ((I < 0) && (Pos(" ", S) > 0)) {
        TStringList* St = ValueToList(S, " ");
        for (J = 0; J < St->Count(); J++) {
            I = ItemsIndexOf((*St)[J]);
            if (I >= 0) break;
        }
        delete St;
    }

    if ((I < 0) && (ExtUpperCase(S) == "GERMAN")) I = ItemsIndexOf("Deutsch");

    if (I < 0) I = ItemsIndexOf("English");

    ui.DOSBoxLanguageComboBox->setCurrentIndex(I);
}

void TFirstRunWizardForm::FormClose(QObject* Sender, TCloseAction& Action)
{
    Q_UNUSED(Sender); Q_UNUSED(Action);
    PrgSetup->SetLanguage(ShortLanguageName(ui.ProgramLanguageComboBox->currentText()) + ".ini");
    PrgSetup->DOSBoxSettings(0)->SetDosBoxDir(IncludeTrailingPathDelimiter(ui.DOSBoxEdit->text()));
    const int L = ui.DOSBoxLanguageComboBox->currentIndex();
    if (L >= 0 && L < DosBoxLang->Count()) PrgSetup->DOSBoxSettings(0)->SetDosBoxLanguage((*DosBoxLang)[L]);
    PrgSetup->SetGameDir(PrgDataDir() + "VirtualHD" + PathDelim);

    PrgSetup->SetVersionSpecificUpdateCheck(true);
    if (ui.UpdatesCheckBox->isChecked()) {
        PrgSetup->SetCheckForUpdates(1);
        PrgSetup->SetDataReaderCheckForUpdates(2);
    } else {
        PrgSetup->SetCheckForUpdates(0);
        PrgSetup->SetDataReaderCheckForUpdates(0);
    }
    PrgSetup->SetPackageListsCheckForUpdates(0);
    PrgSetup->SetCheatsDBCheckForUpdates(0);

    PrgSetup->SetValueForNotSet(LanguageSetup->NotSetLanguageDefault());
}

void TFirstRunWizardForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_FirstRunWizard);
}

void TFirstRunWizardForm::FormKeyDown(QObject* Sender, quint16& Key, TShiftState Shift)
{
    if ((Key == VK_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

// global

bool ShowFirstRunWizardDialog(QWidget* AOwner, bool& SearchForUpdatesNow)
{
    bool result;
    FirstRunWizardForm = new TFirstRunWizardForm(AOwner);
    result = (FirstRunWizardForm->ShowModal() == mrOk);
    if (!result) LoadLanguage(PrgSetup->Language());
    SearchForUpdatesNow = result && FirstRunWizardForm->ui.UpdatesCheckBox->isChecked();
    delete FirstRunWizardForm;
    FirstRunWizardForm = nullptr;
    return result;
}
