// C++ port of ModernProfileEditorPrinterFrameUnit.pas (implementation section).
#include "ModernProfileEditorPrinterFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TModernProfileEditorPrinterFrame::TModernProfileEditorPrinterFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TModernProfileEditorPrinterFrame::~TModernProfileEditorPrinterFrame() = default;

// { TFrame1 }

void TModernProfileEditorPrinterFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    Q_UNUSED(InitData);

    ui.EnablePrinterEmulationCheckBox->setText(LanguageSetup->GameEnablePrinterEmulation());
    ui.ResolutionLabel->setText(LanguageSetup->GamePrinterResolution());
    ui.PaperWidthLabel->setText(LanguageSetup->GamePaperWidth());
    ui.PaperHeightLabel->setText(LanguageSetup->GamePaperHeight());
    ui.PaperSizeInfoLabel->setText(LanguageSetup->GamePaperSizeInfo());
    ui.OutputFormatLabel->setText(LanguageSetup->GamePrinterOutputFormat());
    ui.OutputFormatInfoLabel->setText(LanguageSetup->GamePrinterOutputFormatInfo());
    ui.OutputFormatComboBox->addItem("png"); ui.OutputFormatComboBox->addItem("ps"); ui.OutputFormatComboBox->addItem("bmp"); ui.OutputFormatComboBox->addItem("printer");
    ui.MultiPageCheckBox->setText(LanguageSetup->GamePrinterMultiPage());
    ui.MultiPageInfoLabel->setText(LanguageSetup->GamePrinterMultiPageInfo());

    setProperty("HelpContext", ID_ProfileEditPrinter);  // HelpContext:=...
}

void TModernProfileEditorPrinterFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    QString S;

    ui.EnablePrinterEmulationCheckBox->setChecked(Game->EnablePrinterEmulation());
    ui.ResolutionEdit->setValue(Game->PrinterResolution());
    ui.PaperWidthEdit->setValue(Game->PaperWidth());
    ui.PaperHeightEdit->setValue(Game->PaperHeight());

    S = Trim(ExtUpperCase(Game->PrinterOutputFormat()));
    ui.OutputFormatComboBox->setCurrentIndex(1);
    for (int I = 0; I < ui.OutputFormatComboBox->count(); I++) if (Trim(ExtUpperCase(ui.OutputFormatComboBox->itemText(I))) == S) {
        ui.OutputFormatComboBox->setCurrentIndex(I);
        break;
    }

    ui.MultiPageCheckBox->setChecked(Game->PrinterMultiPage());
}

void TModernProfileEditorPrinterFrame::GetGame(TGame* Game)
{
    Game->SetEnablePrinterEmulation(ui.EnablePrinterEmulationCheckBox->isChecked());
    Game->SetPrinterResolution(ui.ResolutionEdit->value());
    Game->SetPaperWidth(ui.PaperWidthEdit->value());
    Game->SetPaperHeight(ui.PaperHeightEdit->value());
    Game->SetPrinterOutputFormat(ui.OutputFormatComboBox->currentText());
    Game->SetPrinterMultiPage(ui.MultiPageCheckBox->isChecked());
}
