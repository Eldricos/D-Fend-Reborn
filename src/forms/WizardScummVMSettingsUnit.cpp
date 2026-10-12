// C++ port of WizardScummVMSettingsUnit.pas (implementation section).
#include "WizardScummVMSettingsUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"

// TWizardScummVMSettingsFrame

TWizardScummVMSettingsFrame::TWizardScummVMSettingsFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    connect(ui.LanguageComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { LanguageComboBoxChange(ui.LanguageComboBox); });
    connect(ui.SavePathEditButton, &QToolButton::clicked, this, [this]() { SavePathEditButtonClick(ui.SavePathEditButton); });
    connect(ui.SavePathEdit, &QLineEdit::textChanged, this, [this]() { SavePathEditChange(ui.SavePathEdit); });
}

TWizardScummVMSettingsFrame::~TWizardScummVMSettingsFrame()
{
}

void TWizardScummVMSettingsFrame::Init(TGameDB* GameDB)
{
    // SetVistaFonts(self); - not needed
    FGameDB = GameDB;

    QFont F = ui.InfoLabel->font(); F.setBold(true); ui.InfoLabel->setFont(F);
    ui.InfoLabel->setText(LanguageSetup->WizardFormPage6Info());
    ui.LanguageLabel->setText(LanguageSetup->ProfileEditorScummVMLanguage());
    ui.StartFullscreenCheckBox->setText(LanguageSetup->GameStartFullscreen());
    ui.SavePathGroupBox->setTitle(LanguageSetup->ProfileEditorScummVMSavePath());
    ui.SavePathDefaultRadioButton->setText(LanguageSetup->ProfileEditorScummVMSavePathGameDir() + " (" + LanguageSetup->Default() + ")");
    ui.SavePathCustomRadioButton->setText(LanguageSetup->ProfileEditorScummVMSavePathCustom());
    ui.LanguageInfoLabel->setText(LanguageSetup->WizardFormScummVMLanguageInfo());

    UserIconLoader->DialogImage(DI_SelectFolder, ui.SavePathEditButton);
}

void TWizardScummVMSettingsFrame::LanguageComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.CustomLanguageEdit->setVisible(ui.LanguageComboBox->currentIndex() == ui.LanguageComboBox->count() - 1);
}

void TWizardScummVMSettingsFrame::SetScummVMGameName(const QString& Name, const QString& Path)
{
    QString S;
    int I, J;

    ui.LanguageComboBox->setUpdatesEnabled(false);
    ui.LanguageComboBox->clear();

    TStringList* St = ValueToList(FGameDB->ConfOpt()->ScummVMLanguages(), ";,");
    S = Trim(ExtUpperCase(Name));
    for (I = 0; I < St->Count(); I++) {
        J = Pos(":", (*St)[I]);
        if (J == 0) continue;
        if (Trim(ExtUpperCase(Copy((*St)[I], 1, J - 1))) == S) {
            TStringList* St2 = ValueToList(Copy((*St)[I], J + 1, MaxInt), "-");
            ui.LanguageComboBox->addItems(St2->ToQStringList());
            delete St2;
            break;
        }
    }
    delete St;

    if (ui.LanguageComboBox->count() == 0) ui.LanguageComboBox->addItem(LanguageSetup->Default());

    ui.LanguageComboBox->addItem(LanguageSetup->Custom());
    ui.LanguageComboBox->setUpdatesEnabled(true);

    ui.LanguageComboBox->setCurrentIndex(0);
    LanguageComboBoxChange(this);

    ScummVMPath = Path;
}

TGame* TWizardScummVMSettingsFrame::CreateGame(const QString& GameName)
{
    TGame* result = FGameDB->Game(FGameDB->Add(GameName));
    result->SetProfileMode("ScummVM");
    result->SetName(GameName);

    if (ui.LanguageComboBox->currentIndex() >= 0) {
        result->SetScummVMLanguage(Trim(ExtUpperCase(ui.LanguageComboBox->currentText())));
        if (result->ScummVMLanguage() == Trim(ExtUpperCase(LanguageSetup->Custom()))) result->SetScummVMLanguage(Trim(ExtUpperCase(ui.CustomLanguageEdit->text())));
        if (result->ScummVMLanguage() == Trim(ExtUpperCase(LanguageSetup->Default()))) result->SetScummVMLanguage(QString());
    } else {
        result->SetScummVMLanguage(QString());
    }
    result->SetScummVMLanguage(ExtLowerCase(result->ScummVMLanguage()));

    result->SetStartFullscreen(ui.StartFullscreenCheckBox->isChecked());

    if (ui.SavePathDefaultRadioButton->isChecked()) result->SetScummVMSavePath(QString()); else result->SetScummVMSavePath(Trim(ui.SavePathEdit->text()));
    return result;
}

void TWizardScummVMSettingsFrame::SavePathEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.SavePathDefaultRadioButton->setChecked(Trim(ui.SavePathEdit->text()).isEmpty());
    ui.SavePathCustomRadioButton->setChecked(!Trim(ui.SavePathEdit->text()).isEmpty());
}

void TWizardScummVMSettingsFrame::SavePathEditButtonClick(QObject* Sender)
{
    QString S = Trim(ui.SavePathEdit->text());
    if (S.isEmpty()) S = Trim(ScummVMPath);
    if (S.isEmpty()) S = PrgSetup->GameDir();
    S = MakeAbsPath(S, PrgSetup->BaseDir());
    if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
    ui.SavePathEdit->setText(S);
    SavePathEditChange(Sender);
}
