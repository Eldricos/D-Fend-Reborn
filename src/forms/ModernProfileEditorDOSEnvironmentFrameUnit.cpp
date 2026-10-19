// C++ port of ModernProfileEditorDOSEnvironmentFrameUnit.pas (implementation section).
#include "ModernProfileEditorDOSEnvironmentFrameUnit.h"
#include "ColorScheme.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "IconLoaderUnit.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

// Label.Font.Color:=Color
static void SetFontColor(QWidget* W, TColor Color)
{
    QPalette P = W->palette();
    P.setColor(QPalette::WindowText, TextColor(Color));
    P.setColor(QPalette::Text, TextColor(Color));
    W->setPalette(P);
}

TModernProfileEditorDOSEnvironmentFrame::TModernProfileEditorDOSEnvironmentFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM wiring
    connect(ui.CustomSetsEnvAdd, &QPushButton::clicked, this, [this]() { ButtonWork(ui.CustomSetsEnvAdd); });
    connect(ui.CustomSetsEnvDel, &QPushButton::clicked, this, [this]() { ButtonWork(ui.CustomSetsEnvDel); });
}

TModernProfileEditorDOSEnvironmentFrame::~TModernProfileEditorDOSEnvironmentFrame() = default;

// { TModernProfileEditorDOSEnvironmentFrame }

void TModernProfileEditorDOSEnvironmentFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    TStringList* St;

    ui.ReportedDOSVersionLabel->setText(LanguageSetup->GameReportedDOSVersion());
    St = ValueToList(InitData.GameDB->ConfOpt()->ReportedDOSVersion(), ";,"); ui.ReportedDOSVersionComboBox->addItems(St->ToQStringList()); delete St;
    ui.Use4DOSCheckBox->setText(LanguageSetup->ProfileEditorAutoexecUse4DOS());
    ui.Use4DOSInfoLabel->setText(LanguageSetup->ProfileEditorNeedFreeDOS());
    if (DirectoryExists(IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->PathToFREEDOS(), PrgSetup->BaseDir())))) {
        SetFontColor(ui.Use4DOSInfoLabel, clGrayText);
    } else {
        SetFontColor(ui.Use4DOSInfoLabel, clRed);
    }
    ui.CustomSetsEnvLabel->setText(LanguageSetup->ProfileEditorCustomSetsEnvironment());
    ui.CustomSetsValueListEditor->setTitleCaptions(QStringList{LanguageSetup->Key(), LanguageSetup->Value()});
    ui.CustomSetsEnvAdd->setText(LanguageSetup->Add());
    ui.CustomSetsEnvDel->setText(LanguageSetup->Del());
    UserIconLoader->DialogImage(DI_Add, ui.CustomSetsEnvAdd);
    UserIconLoader->DialogImage(DI_Delete, ui.CustomSetsEnvDel);

    AddDefaultValueHint(ui.ReportedDOSVersionComboBox);

    setProperty("HelpContext", ID_ProfileEditDOSEnvironment);  // HelpContext:=...
}

void TModernProfileEditorDOSEnvironmentFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    TStringList* St;
    QStringList Strings;

    ui.ReportedDOSVersionComboBox->setCurrentText(Game->ReportedDOSVersion());
    ui.Use4DOSCheckBox->setChecked(Game->Use4DOS());
    if (Game->Environment().isEmpty() && LoadFromTemplate) {
        Strings.append("PATH=Z:\\");
    } else {
        St = StringToStringList(Game->Environment());
        for (int I = 0; I < St->Count(); I++) if (!Trim((*St)[I]).isEmpty())
            Strings.append(Replace((*St)[I], "[" + IntToStr('=') + "]", "="));
        delete St;
    }
    ui.CustomSetsValueListEditor->setStrings(Strings);  // Strings.Clear + Strings.Add(...)
}

void TModernProfileEditorDOSEnvironmentFrame::ButtonWork(QObject* Sender)
{
    QWidget* W = qobject_cast<QWidget*>(Sender);
    switch (W->property("tag").toInt()) {
        case 0: ui.CustomSetsValueListEditor->insertRow("Key", ""); break;
        // VCL: (Row>0) and (RowCount>2) -> Strings.Delete(Row-1); Row/RowCount include the title row
        case 1: if ((ui.CustomSetsValueListEditor->currentRow() >= 0) && (ui.CustomSetsValueListEditor->rowCount() > 1)) ui.CustomSetsValueListEditor->deleteRow(ui.CustomSetsValueListEditor->currentRow()); break;
    }
}

void TModernProfileEditorDOSEnvironmentFrame::GetGame(TGame* Game)
{
    TStringList St;

    Game->SetReportedDOSVersion(ui.ReportedDOSVersionComboBox->currentText());
    Game->SetUse4DOS(ui.Use4DOSCheckBox->isChecked());
    const QStringList Strings = ui.CustomSetsValueListEditor->strings();
    for (int I = 0; I < Strings.size(); I++) St.Add(Replace(Strings[I], "=", "[" + IntToStr('=') + "]"));
    Game->SetEnvironment(StringListToString(&St));
}
