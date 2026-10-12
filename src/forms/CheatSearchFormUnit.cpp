// C++ port of CheatSearchFormUnit.pas (implementation section).
#include "CheatSearchFormUnit.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "IconLoaderUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "PrgSetupUnit.h"
#include "CheatDBToolsUnit.h"
#include "Process.h"

TCheatSearchForm* CheatSearchForm = nullptr;

// Application.MainForm
static QWidget* ApplicationMainForm()
{
    for (QWidget* W : QApplication::topLevelWidgets()) if (qobject_cast<QMainWindow*>(W) != nullptr) return W;
    return QApplication::activeWindow();
}

TCheatSearchForm::TCheatSearchForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    OpenDialog = new TOpenDialog(this);
    OpenDialog->Filter = "All files (*.*)|*.*";

    connect(ui.SearchNameEdit, &QLineEdit::textChanged, this, [this]() { SearchNameChange(ui.SearchNameEdit); });
    connect(ui.SearchNameComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { SearchNameChange(ui.SearchNameComboBox); });
    connect(ui.SearchNameDeleteButton, &QPushButton::clicked, this, [this]() { SearchNameDeleteButtonClick(ui.SearchNameDeleteButton); });
    connect(ui.FileNameButton, &QToolButton::clicked, this, [this]() { FileNameButtonClick(ui.FileNameButton); });
    ui.ValueEdit->installEventFilter(this);
    connect(ui.ValueComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { SearchValueChange(ui.ValueComboBox); });
    connect(ui.BytesComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { BytesComboBoxChange(ui.BytesComboBox); });
    connect(ui.UseDialogEdit, &QLineEdit::textChanged, this, [this]() { UseDialogEditChange(ui.UseDialogEdit); });
    connect(ui.NextButton, &QPushButton::clicked, this, [this]() { NextButtonClick(ui.NextButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);  // OnCreate
}

TCheatSearchForm::~TCheatSearchForm()
{
    FormDestroy(this);  // OnDestroy
    delete OpenDialog;
}

void TCheatSearchForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

bool TCheatSearchForm::eventFilter(QObject* Obj, QEvent* E)
{
    if (Obj == ui.ValueEdit && E->type() == QEvent::MouseButtonRelease) SearchValueChange(ui.ValueEdit);
    return TVclForm::eventFilter(Obj, E);
}

int TCheatSearchForm::GameNameComboBoxItemIndex() const
{
    // csDropDown: typing a text that is not in the list means ItemIndex=-1
    return ui.GameNameComboBox->findText(ui.GameNameComboBox->currentText(), Qt::MatchExactly);
}

void TCheatSearchForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    SearchFiles = new TStringList;
    SearchNames = new TStringList;
    GetAddressSearchFiles(SearchFiles, SearchNames);
    AddressSearcher = nullptr;
    CheatDB = nullptr;
}

void TCheatSearchForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    setWindowTitle(LanguageSetup->SearchAddress());
    ui.SearchStartLabel->setText(LanguageSetup->SearchAddressStart());
    ui.SearchStartRadioButton1->setText(LanguageSetup->SearchAddressStartNew());
    ui.SearchNameEdit_EditLabel->setText(LanguageSetup->SearchAddressStartNewName());
    ui.SearchStartRadioButton2->setText(LanguageSetup->SearchAddressStartContinue());

    ui.DoSearchLabel->setText(LanguageSetup->SearchAddressData());
    ui.ValueTypeRadioButton1->setText(LanguageSetup->SearchAddressDataKnown());
    ui.ValueEdit_EditLabel->setText(LanguageSetup->SearchAddressDataKnownValue());
    ui.ValueEdit->setToolTip(LanguageSetup->SearchAddressResultNewValueHint());
    ui.ValueTypeRadioButton2->setText(LanguageSetup->SearchAddressDataUnknown());
    {
        const QSignalBlocker Block(ui.ValueComboBox);
        ui.ValueComboBox->clear();
        ui.ValueComboBox->addItem(LanguageSetup->SearchAddressDataUnknownIncreased());
        ui.ValueComboBox->addItem(LanguageSetup->SearchAddressDataUnknownDescreased());
        ui.ValueComboBox->setCurrentIndex(-1);
    }
    ui.FileNameEdit_EditLabel->setText(LanguageSetup->SearchAddressDataFileName());
    OpenDialog->Filter = LanguageSetup->SearchAddressDataFileNameFilter();
    OpenDialog->Title = LanguageSetup->SearchAddressDataFileNameTitle();
    ui.FileNameButton->setToolTip(LanguageSetup->ChooseFile());
    ui.ActionLabel->setText(LanguageSetup->SearchAddressResult());
    ui.AddressEdit_EditLabel->setText(LanguageSetup->SearchAddressResultAddress());
    ui.NewValueEdit_EditLabel->setText(LanguageSetup->SearchAddressResultNewValue());
    ui.NewValueEdit->setToolTip(LanguageSetup->SearchAddressResultNewValueHint());
    ui.BytesLabel->setText(LanguageSetup->SearchAddressResultBytes());
    ui.GameNameLabel->setText(LanguageSetup->SearchAddressResultGameName());
    ui.DescriptionEdit_EditLabel->setText(LanguageSetup->SearchAddressResultDescription());
    ui.FileMaskEdit_EditLabel->setText(LanguageSetup->SearchAddressResultFileMask());
    ui.UseDialogCheckBox->setText(LanguageSetup->SearchAddressResultUseDialog());
    ui.UseDialogEdit_EditLabel->setText(LanguageSetup->SearchAddressResultUseDialogPrompt());
    ui.SelectActionLabel->setText(LanguageSetup->SearchAddressResultMessageMultipleAddresses());
    ui.SelectActionRadioGroup->setTitle(LanguageSetup->SearchAddressResultMessageMultipleAddressesActions());
    ui.SelectActionRadioGroup->setItems(QStringList()
        << LanguageSetup->SearchAddressResultMessageMultipleAddressesAction1()
        << LanguageSetup->SearchAddressResultMessageMultipleAddressesAction2()
        << LanguageSetup->SearchAddressResultMessageMultipleAddressesAction3());
    ui.SelectActionRadioGroup->setItemIndex(0);
    ui.SearchNameDeleteButton->setText(LanguageSetup->SearchAddressStartDeleteButton());
    ui.NextButton->setText(LanguageSetup->Next());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

    UserIconLoader->DialogImage(DI_Next, ui.NextButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
    UserIconLoader->DialogImage(DI_Delete, ui.SearchNameDeleteButton);
    UserIconLoader->DialogImage(DI_SelectFile, ui.FileNameButton);

    InitPage(0);
}

void TCheatSearchForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    delete SearchFiles; SearchFiles = nullptr;
    delete SearchNames; SearchNames = nullptr;
    delete AddressSearcher; AddressSearcher = nullptr;
    delete CheatDB; CheatDB = nullptr;
}

void TCheatSearchForm::InitGameListAndCheatDB()
{
    int I;
    delete CheatDB;
    CheatDB = SmartLoadCheatsDB();
    ui.GameNameComboBox->clear();
    if (CheatDB != nullptr)
        for (I = 0; I < CheatDB->Count(); I++) ui.GameNameComboBox->addItem(CheatDB->Game(I)->Name(), QVariant::fromValue<void*>(CheatDB->Game(I)));
    ui.GameNameComboBox->setCurrentIndex(-1);
    ui.GameNameComboBox->setCurrentText(AddressSearcher->Name());
}

void TCheatSearchForm::InitPage(int Nr)
{
    TStringList* St;
    ui.Notebook->setCurrentIndex(Nr);
    switch (Nr) {
        case 0: { /* New search / continue search */
            const QSignalBlocker Block(ui.SearchNameComboBox);
            ui.SearchNameComboBox->clear();
            ui.SearchNameComboBox->addItems(SearchNames->ToQStringList());
            if (SearchNames->Count() > 0) {
                ui.SearchNameComboBox->setCurrentIndex(0);
                ui.SearchStartRadioButton2->setChecked(true);
            } else {
                ui.SearchStartRadioButton2->setEnabled(false);
                ui.SearchNameDeleteButton->setEnabled(false);
                ui.SearchNameComboBox->setEnabled(false);
                ui.SearchStartRadioButton1->setChecked(true);
            }
            break;
        }
        case 1: { /* Select file and value */
            const QSignalBlocker Block(ui.ValueComboBox);
            if (ui.SearchStartRadioButton2->isChecked()) {
                AddressSearcher = new TAddressSearcher;
                AddressSearcher->LoadFromFile((*SearchFiles)[ui.SearchNameComboBox->currentIndex()]);
                ui.FileNameEdit->setText(AddressSearcher->LastSavedGameFileName());
                ui.ValueComboBox->setCurrentIndex(0);
            } else {
                ui.ValueComboBox->setCurrentIndex(-1);
                ui.ValueComboBox->setEnabled(false);
            }
            ui.ValueTypeRadioButton1->setChecked(true);
            break;
        }
        case 2: { /* Store result to DB */
            InitGameListAndCheatDB();
            {
                const QSignalBlocker Block(ui.BytesComboBox);
                ui.BytesComboBox->clear(); ui.BytesComboBox->addItem("1");
                if (ResultSize >= 2) ui.BytesComboBox->addItem("2");
                if (ResultSize == 4) ui.BytesComboBox->addItem("4");
                ui.BytesComboBox->setCurrentIndex(ui.BytesComboBox->count() - 1);
            }
            ui.AddressEdit->setText(IntToStr(ResultAddress) + "d=" + IntToHex(ResultAddress, 1) + "h");
            {
                QPalette P = ui.AddressEdit->palette();
                P.setColor(QPalette::Base, palette().color(QPalette::Window));   // AddressEdit.Color:=Color
                ui.AddressEdit->setPalette(P);
            }
            switch (ResultSize) {
                case 1: ui.NewValueEdit->setText("$FF"); break;
                case 2: ui.NewValueEdit->setText("$FFFF"); break;
                case 4: ui.NewValueEdit->setText("$7EFFFFFF"); break;
            }
            ui.LastSavedGameHintLabel->setText(Format(LanguageSetup->SearchAddressResultFileMaskHint(), {ExtractFileName(ui.FileNameEdit->text())}));
            break;
        }
        case 3: { /* Multiple addresses found */
            St = AddressSearcher->GetList();
            ui.SelectActionListBox->clear();
            for (int I = 0; I < St->Count(); I++) {
                QListWidgetItem* It = new QListWidgetItem((*St)[I], ui.SelectActionListBox);
                It->setData(Qt::UserRole, St->ObjectAsInt(I));
            }
            ui.SelectActionListBox->setCurrentRow(0);
            delete St;
            break;
        }
    }
}

void TCheatSearchForm::SearchNameChange(QObject* Sender)
{
    if (Sender == ui.SearchNameEdit) ui.SearchStartRadioButton1->setChecked(true);
    if (Sender == ui.SearchNameComboBox) ui.SearchStartRadioButton2->setChecked(true);
}

void TCheatSearchForm::SearchNameDeleteButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (ui.SearchNameComboBox->currentIndex() < 0) return;

    const int Nr = ui.SearchNameComboBox->currentIndex();
    ExtDeleteFile((*SearchFiles)[Nr], ftTemp);
    SearchFiles->Delete(Nr);
    SearchNames->Delete(Nr);

    InitPage(0);
}

void TCheatSearchForm::SearchValueChange(QObject* Sender)
{
    if (Sender == ui.ValueEdit) ui.ValueTypeRadioButton1->setChecked(true);
    if (Sender == ui.ValueComboBox) ui.ValueTypeRadioButton2->setChecked(true);
}

void TCheatSearchForm::UseDialogEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.UseDialogCheckBox->setChecked(true);
}

void TCheatSearchForm::BytesComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I, J;
    I = StrToIntDef(ui.BytesComboBox->currentText(), 0);
    if (!TryStrToInt(ui.NewValueEdit->text(), J)) return;
    switch (I) {
        case 1: if (J > 0xFF) ui.NewValueEdit->setText("$FF"); break;
        case 2: if (J > 0xFFFF) ui.NewValueEdit->setText("$FFFF"); break;
    }
}

void TCheatSearchForm::FileNameButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    if (Trim(ui.FileNameEdit->text()) != "") S = ExtractFilePath(ui.FileNameEdit->text()); else S = MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir());
    OpenDialog->InitialDir = S;
    if (!OpenDialog->Execute()) return;
    ui.FileNameEdit->setText(OpenDialog->FileName);
}

bool TCheatSearchForm::ValueCheck()
{
    int I;
    QString S;

    switch (ui.Notebook->currentIndex()) {
        case 0: /* New search / continue search */
            if (ui.SearchStartRadioButton1->isChecked() && (Trim(ui.SearchNameEdit->text()) == "")) { MessageDlg(LanguageSetup->SearchAddressStartNewNameMessage(), mtError, mbOK, 0, this); return false; }
            break;
        case 1: /* Select file and value */
            if (ui.ValueTypeRadioButton1->isChecked() && !TryStrToInt(ui.ValueEdit->text(), I)) { MessageDlg(LanguageSetup->SearchAddressDataKnownValueMessage(), mtError, mbOK, 0, this); return false; }
            if (Trim(ui.FileNameEdit->text()) == "") { MessageDlg(LanguageSetup->SearchAddressDataFileNameMessage(), mtError, mbOK, 0, this); return false; }
            S = MakeAbsPath(ui.FileNameEdit->text(), PrgSetup->BaseDir());
            if (!FileExists(S)) { MessageDlg(Format(LanguageSetup->MessageFileNotFound(), {S}), mtError, mbOK, 0, this); return false; }
            break;
        case 2: /* Store result to DB */
            if (Trim(ui.GameNameComboBox->currentText()) == "") { MessageDlg(LanguageSetup->SearchAddressResultGameNameMessage(), mtError, mbOK, 0, this); return false; }
            if (Trim(ui.DescriptionEdit->text()) == "") { MessageDlg(LanguageSetup->SearchAddressResultDescriptionMessage(), mtError, mbOK, 0, this); return false; }
            if (Trim(ui.NewValueEdit->text()) == "") { MessageDlg(LanguageSetup->SearchAddressResultNewValueMessage(), mtError, mbOK, 0, this); return false; }
            if (!ExtTryStrToInt(ui.NewValueEdit->text(), I)) { MessageDlg(LanguageSetup->SearchAddressResultNewValueMessageInvalid(), mtError, mbOK, 0, this); return false; }
            if (Trim(ui.FileMaskEdit->text()) == "") { MessageDlg(LanguageSetup->SearchAddressResultFileMaskMessage(), mtError, mbOK, 0, this); return false; }
            if (Trim(ui.FileMaskEdit->text()) == "*.*") { if (MessageDlg(LanguageSetup->SearchAddressResultFileMaskMessageAllFiles(), mtConfirmation, mbYes | mbNo, 0, this) != mrYes) return false; }
            break;
        case 3: /* Multiple addresses found */
            break;
    }
    return true;
}

void TCheatSearchForm::RunSearch()
{
    QString SavedGameFileName;
    TValueChangeType ValueChangeType;

    SavedGameFileName = MakeAbsPath(ui.FileNameEdit->text(), PrgSetup->BaseDir());

    if (AddressSearcher == nullptr) {
        AddressSearcher = new TAddressSearcher;
        AddressSearcher->SetName(ui.SearchNameEdit->text());
    }
    if (ui.ValueTypeRadioButton1->isChecked()) {
        AddressSearcher->SearchAddress(SavedGameFileName, StrToInt(ui.ValueEdit->text()));
    } else {
        if (ui.ValueComboBox->isEnabled()) {
            if (ui.ValueComboBox->currentIndex() == 0) ValueChangeType = vctUp; else ValueChangeType = vctDown;
            AddressSearcher->SearchAddressIndirect(SavedGameFileName, ValueChangeType);
        } else {
            AddressSearcher->LoadSavedGameFile(SavedGameFileName);
        }
    }
}

void TCheatSearchForm::StoreActionToDB(bool DeleteSearch)
{
    TCheatGameRecord* GameRecord;
    TCheatAction* CheatAction;
    TCheatActionStep* CheatActionStep;

    const int GameIndex = GameNameComboBoxItemIndex();
    if (GameIndex < 0) {
        GameRecord = new TCheatGameRecord(nullptr);
        CheatDB->Add(GameRecord);
        GameRecord->SetName(ui.GameNameComboBox->currentText());
    } else {
        GameRecord = static_cast<TCheatGameRecord*>(ui.GameNameComboBox->itemData(GameIndex).value<void*>());
    }

    CheatAction = new TCheatAction(nullptr);
    GameRecord->Add(CheatAction);
    CheatAction->SetName(ui.DescriptionEdit->text());
    CheatAction->SetFileMask(ui.FileMaskEdit->text());

    if (ui.UseDialogCheckBox->isChecked()) {
        TCheatActionStepChangeAddressWithDialog* S = new TCheatActionStepChangeAddressWithDialog(nullptr);
        CheatActionStep = S;
        S->SetAddresses(IntToStr(ResultAddress));
        S->SetBytes(StrToInt(ui.BytesComboBox->currentText()));
        S->SetDefaultValue(Trim(ui.NewValueEdit->text()));
        S->SetDialogPrompt(ui.UseDialogEdit->text());
    } else {
        TCheatActionStepChangeAddress* S = new TCheatActionStepChangeAddress(nullptr);
        CheatActionStep = S;
        S->SetAddresses(IntToStr(ResultAddress));
        S->SetBytes(ResultSize);
        S->SetNewValue(Trim(ui.NewValueEdit->text()));
    }
    CheatAction->Add(CheatActionStep);

    SmartSaveCheatsDB(CheatDB, this);
    if (DeleteSearch) ExtDeleteFile(AddressSearcher->SearchDataFileName(), ftTemp);

    MessageDlg(LanguageSetup->SearchAddressResultSuccess(), mtInformation, mbOK, 0, this);
}

void TCheatSearchForm::NextButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    TResultStatus R;

    if (!ValueCheck()) return;

    switch (ui.Notebook->currentIndex()) {
        case 0: /* New search / continue search */
            InitPage(1);
            break;
        case 1: /* Select file and value */
            RunSearch();
            R = AddressSearcher->GetResult(ResultAddress, ResultSize);
            if ((R == rsNoResultsYet) || (R == rsMultipleAddresses)) AddressSearcher->SaveToFile("");
            switch (R) {
                case rsNoResultsYet: MessageDlg(LanguageSetup->SearchAddressResultMessageStart(), mtInformation, mbOK, 0, this); break;
                case rsMultipleAddresses: InitPage(3); break;
                case rsNoAddress:
                    MessageDlg(LanguageSetup->SearchAddressResultMessageNotFound(), mtInformation, mbOK, 0, this);
                    ExtDeleteFile(AddressSearcher->SearchDataFileName(), ftTemp);
                    break;
                case rsFound: DeleteSearch = true; InitPage(2); break;
            }
            if ((R == rsNoResultsYet) || (R == rsNoAddress)) Close();
            break;
        case 2: /* Store result to DB */
            StoreActionToDB(DeleteSearch);
            Close();
            break;
        case 3: /* Multiple addresses found */
            switch (ui.SelectActionRadioGroup->itemIndex()) {
                case 0: Close(); return; /* Search results already stored in NextButtonClick for page 1 */
                case 1: DeleteSearch = true; break;
                case 2: DeleteSearch = false; break;
            }
            AddressSearcher->GetResultNr(ui.SelectActionListBox->item(ui.SelectActionListBox->currentRow())->data(Qt::UserRole).toInt(), ResultAddress, ResultSize);
            InitPage(2);
            break;
    }
}

void TCheatSearchForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ExtrasSearchAddress);
}

void TCheatSearchForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

/* global */

void ShowCheatSearchDialog(QWidget* AOwner)
{
    CheatsDBUpdateCheckIfSetup(ApplicationMainForm());
    CheatSearchForm = new TCheatSearchForm(AOwner);
    CheatSearchForm->ShowModal();
    delete CheatSearchForm;
    CheatSearchForm = nullptr;
}
