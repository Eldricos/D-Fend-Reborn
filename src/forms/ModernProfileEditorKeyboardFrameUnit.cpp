// C++ port of ModernProfileEditorKeyboardFrameUnit.pas (implementation section).
#include "ModernProfileEditorKeyboardFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "IconLoaderUnit.h"
#include "GameDBToolsUnit.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

// Label.Font.Color:=Color
static void SetFontColor(QWidget* W, TColor Color)
{
    QPalette P = W->palette();
    P.setColor(QPalette::WindowText, TColorToQColor(Color));
    P.setColor(QPalette::Text, TColorToQColor(Color));
    W->setPalette(P);
}

TModernProfileEditorKeyboardFrame::TModernProfileEditorKeyboardFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM: DosBoxTxtOpenDialog: TOpenDialog (DefaultExt = 'txt')
    DosBoxTxtOpenDialog = new TOpenDialog(this);
    DosBoxTxtOpenDialog->DefaultExt = "txt";

    // DFM wiring
    connect(ui.CustomKeyMapperButton, &QToolButton::clicked, this, [this]() { CustomKeyMapperButtonClick(ui.CustomKeyMapperButton); });
    connect(ui.CustomKeyMapperEdit, &QLineEdit::textChanged, this, [this]() { CustomKeyMapperEditChange(ui.CustomKeyMapperEdit); });
}

TModernProfileEditorKeyboardFrame::~TModernProfileEditorKeyboardFrame()
{
    delete DosBoxTxtOpenDialog;
}

// { TModernProfileEditorKeyboardFrame }

void TModernProfileEditorKeyboardFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    TStringList* St;

    ui.KeyboardLayoutLabel->setText(LanguageSetup->GameKeyboardLayout());
    St = ValueToList(InitData.GameDB->ConfOpt()->KeyboardLayout(), ";,"); ui.KeyboardLayoutComboBox->addItems(St->ToQStringList()); delete St;
    ui.CodepageLabel->setText(LanguageSetup->GameKeyboardCodepage());
    St = ValueToList(InitData.GameDB->ConfOpt()->Codepage(), ";,"); ui.CodepageComboBox->addItems(St->ToQStringList()); delete St;
    ui.KeyboardLayoutInfoLabel->setText(LanguageSetup->GameKeyboardLayoutInfo());
    ui.UseScancodesCheckBox->setText(LanguageSetup->GameUseScanCodes());
    for (TRadioGroupWidget* G : {ui.NumLockRadioGroup, ui.CapsLockRadioGroup, ui.ScrollLockRadioGroup}) {
        if (G == ui.NumLockRadioGroup) G->setTitle(LanguageSetup->GameKeyboardNumLock());
        if (G == ui.CapsLockRadioGroup) G->setTitle(LanguageSetup->GameKeyboardCapsLock());
        if (G == ui.ScrollLockRadioGroup) G->setTitle(LanguageSetup->GameKeyboardScrollLock());
        G->setItemText(0, LanguageSetup->DoNotChange());
        G->setItemText(1, LanguageSetup->Off());
        G->setItemText(2, LanguageSetup->On());
    }
    ui.KeyLockInfoLabel->setText(LanguageSetup->ProfileEditorNeedFreeDOS());
    if (DirectoryExists(IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->PathToFREEDOS(), PrgSetup->BaseDir())))) {
        SetFontColor(ui.KeyLockInfoLabel, clGrayText);
    } else {
        SetFontColor(ui.KeyLockInfoLabel, clRed);
    }

    ui.DefaultKeyMapperFileRadioButton->setText(LanguageSetup->GameKeyMapperDefault());
    ui.CustomKeyMapperFileRadioButton->setText(LanguageSetup->GameKeyMapperCustom());
    ui.CustomKeyMapperButton->setToolTip(LanguageSetup->ChooseFile());
    UserIconLoader->DialogImage(DI_SelectFile, ui.CustomKeyMapperButton);

    AddDefaultValueHint(ui.KeyboardLayoutComboBox);
    AddDefaultValueHint(ui.CodepageComboBox);

    setProperty("helpContext", ID_ProfileEditKeyboard);  // HelpContext:=...
}

bool TModernProfileEditorKeyboardFrame::CheckIsDOSBOXDefaultMapperFile(TGame* Game, const QString& MapperFile)
{
    int I;
    QString S;

    I = GetDOSBoxNr(Game);
    S = PrgSetup->DOSBoxSettings(I)->DosBoxMapperFile();
    if ((Copy(S, 1, 2) == ".\\") && (Copy(MapperFile, 1, 2) != ".\\")) S = Copy(S, 3, MaxInt);
    return ExtUpperCase(MapperFile) == ExtUpperCase(S);
}

static void SetLockRadioGroup(TRadioGroupWidget* G, const QString& S)
{
    G->setItemIndex(0);
    if ((S == "OFF") || (S == "0") || (S == "FALSE")) G->setItemIndex(1);
    if ((S == "ON") || (S == "1") || (S == "TRUE")) G->setItemIndex(2);
}

void TModernProfileEditorKeyboardFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    QString S, T;
    int I;

    if (ui.KeyboardLayoutComboBox->count() > 0) {
        if (Trim(ExtUpperCase(ui.KeyboardLayoutComboBox->itemText(0))) == "DEFAULT")
            ui.KeyboardLayoutComboBox->setCurrentIndex(0);
        else
            ui.KeyboardLayoutComboBox->setCurrentIndex(1);
    }
    if (ui.CodepageComboBox->count() > 0) ui.CodepageComboBox->setCurrentIndex(0);

    if (!Game->KeyboardLayout().isEmpty()) S = Game->KeyboardLayout(); else S = "default";
    S = Trim(ExtUpperCase(S));
    for (I = 0; I < ui.KeyboardLayoutComboBox->count(); I++) {
        if (Trim(ExtUpperCase(ui.KeyboardLayoutComboBox->itemText(I))) == S) { ui.KeyboardLayoutComboBox->setCurrentIndex(I); break; }
        T = ui.KeyboardLayoutComboBox->itemText(I);
        if (Pos("(", T) > 0) {
            T = Copy(T, Pos("(", T) + 1, MaxInt);
            if (Pos(")", T) != 0) T = Copy(T, 1, Pos(")", T) - 1);
            if (Trim(ExtUpperCase(T)) == S) { ui.KeyboardLayoutComboBox->setCurrentIndex(I); break; }
        }
    }

    if (!Game->Codepage().isEmpty()) S = Game->Codepage(); else S = "default";
    S = Trim(ExtUpperCase(S));
    for (I = 0; I < ui.CodepageComboBox->count(); I++) {
        if (Trim(ExtUpperCase(ui.CodepageComboBox->itemText(I))) == S) { ui.CodepageComboBox->setCurrentIndex(I); break; }
        T = ui.CodepageComboBox->itemText(I);
        if (Pos("(", T) > 0) {
            if (Trim(Copy(T, 1, Pos("(", T) - 1)) == S) { ui.CodepageComboBox->setCurrentIndex(I); break; }
        }
    }

    ui.UseScancodesCheckBox->setChecked(Game->UseScanCodes());

    S = Trim(ExtUpperCase(Game->NumLockStatus()));
    SetLockRadioGroup(ui.NumLockRadioGroup, S);

    S = Trim(ExtUpperCase(Game->CapsLockStatus()));
    SetLockRadioGroup(ui.CapsLockRadioGroup, S);

    S = Trim(ExtUpperCase(Game->ScrollLockStatus()));
    SetLockRadioGroup(ui.ScrollLockRadioGroup, S);

    S = Trim(ExtUpperCase(Game->CustomKeyMappingFile()));
    if ((!S.isEmpty()) && (S != "DEFAULT")) {
        if (CheckIsDOSBOXDefaultMapperFile(Game, S)) S = "";
    }
    if ((S.isEmpty()) || (S == "DEFAULT")) {
        ui.DefaultKeyMapperFileRadioButton->setChecked(true);
    } else {
        ui.CustomKeyMapperFileRadioButton->setChecked(true);
        ui.CustomKeyMapperEdit->setText(Game->CustomKeyMappingFile());
    }
}

void TModernProfileEditorKeyboardFrame::GetGame(TGame* Game)
{
    Game->SetKeyboardLayout(ui.KeyboardLayoutComboBox->currentText());
    Game->SetCodepage(ui.CodepageComboBox->currentText());
    Game->SetUseScanCodes(ui.UseScancodesCheckBox->isChecked());
    switch (ui.NumLockRadioGroup->itemIndex()) {
        case 0: Game->SetNumLockStatus(""); break;
        case 1: Game->SetNumLockStatus("off"); break;
        case 2: Game->SetNumLockStatus("on"); break;
    }
    switch (ui.CapsLockRadioGroup->itemIndex()) {
        case 0: Game->SetCapsLockStatus(""); break;
        case 1: Game->SetCapsLockStatus("off"); break;
        case 2: Game->SetCapsLockStatus("on"); break;
    }
    switch (ui.ScrollLockRadioGroup->itemIndex()) {
        case 0: Game->SetScrollLockStatus(""); break;
        case 1: Game->SetScrollLockStatus("off"); break;
        case 2: Game->SetScrollLockStatus("on"); break;
    }
    if (ui.DefaultKeyMapperFileRadioButton->isChecked())
        Game->SetCustomKeyMappingFile("default");
    else
        Game->SetCustomKeyMappingFile(ui.CustomKeyMapperEdit->text());
}

void TModernProfileEditorKeyboardFrame::CustomKeyMapperButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    DosBoxTxtOpenDialog->Title = LanguageSetup->SetupFormDOSBoxMapperFileTitle();
    DosBoxTxtOpenDialog->Filter = LanguageSetup->SetupFormDOSBoxMapperFileFilter();
    if (Trim(ui.CustomKeyMapperEdit->text()).isEmpty())
        DosBoxTxtOpenDialog->InitialDir = PrgDataDir();
    else
        DosBoxTxtOpenDialog->InitialDir = ExtractFilePath(MakeAbsPath(ui.CustomKeyMapperEdit->text(), PrgSetup->BaseDir()));
    if (!DosBoxTxtOpenDialog->Execute()) return;
    ui.CustomKeyMapperEdit->setText(MakeRelPath(DosBoxTxtOpenDialog->FileName, PrgSetup->BaseDir()));
}

void TModernProfileEditorKeyboardFrame::CustomKeyMapperEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.CustomKeyMapperFileRadioButton->setChecked(true);
}
