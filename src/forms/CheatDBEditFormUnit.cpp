// C++ port of CheatDBEditFormUnit.pas (implementation section).
#include "CheatDBEditFormUnit.h"

#include "PrgConsts.h"
#include "CheatDBToolsUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "IconLoaderUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "Process.h"

TCheatDBEditForm* CheatDBEditForm = nullptr;

// Application.HelpCommand(HELP_CONTEXT, Data)
static void HelpCommandContext(int Data)
{
    // TODO(port): no HTML help (hh.exe) context jump available; open the help file instead.
    Q_UNUSED(Data);
    if (GLOBAL_HELPROUTER != nullptr && !GLOBAL_HELPROUTER->Helpfile().isEmpty()) ShellOpen(GLOBAL_HELPROUTER->Helpfile());
}

// Application.MainForm
static QWidget* ApplicationMainForm()
{
    for (QWidget* W : QApplication::topLevelWidgets()) if (qobject_cast<QMainWindow*>(W) != nullptr) return W;
    return QApplication::activeWindow();
}

// ComboBox.ItemIndex:=I (VCL does not fire OnChange for programmatic changes)
static void SetItemIndex(QComboBox* C, int I)
{
    const QSignalBlocker Block(C);
    C->setCurrentIndex(I);
}

// ComboBox.Items.Objects[I]
static void* ItemObject(QComboBox* C, int I)
{
    return C->itemData(I).value<void*>();
}

// ComboBox.Items.Clear; ComboBox.Items.AddStrings(St) (with objects)
static void SetItems(QComboBox* C, const TStringList& St)
{
    const QSignalBlocker Block(C);
    C->clear();
    for (int I = 0; I < St.Count(); I++) C->addItem(St[I], QVariant::fromValue<void*>(St.Objects(I)));
}

TCheatDBEditForm::TCheatDBEditForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    for (QToolButton* B : {ui.GameAddButton, ui.GameEditButton, ui.GameDeleteButton, ui.ActionAddButton, ui.ActionEditButton, ui.ActionDeleteButton, ui.ActionStepAddButton, ui.ActionStepDeleteButton})
        connect(B, &QToolButton::clicked, this, [this, B]() { ButtonWork(B); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.GameComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { GameComboBoxChange(ui.GameComboBox); });
    connect(ui.ActionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { ActionComboBoxChange(ui.ActionComboBox); });
    connect(ui.ActionStepComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { ActionStepComboBoxChange(ui.ActionStepComboBox); });
    connect(ui.ChangeAddressWithDialogDefaultAddressEdit, &QLineEdit::textChanged, this, [this]() { ChangeAddressWithDialogDefaultEditChange(ui.ChangeAddressWithDialogDefaultAddressEdit); });
    connect(ui.ChangeAddressWithDialogDefaultFixedEdit, &QLineEdit::textChanged, this, [this]() { ChangeAddressWithDialogDefaultEditChange(ui.ChangeAddressWithDialogDefaultFixedEdit); });
    connect(ui.UpdateButton, &QPushButton::clicked, this, [this]() { UpdateButtonClick(ui.UpdateButton); });
    connect(ui.AddActionStepPopupChangeAddress, &QAction::triggered, this, [this]() { ButtonWork(ui.AddActionStepPopupChangeAddress); });
    connect(ui.AddActionStepPopupChangeAddressWithDialog, &QAction::triggered, this, [this]() { ButtonWork(ui.AddActionStepPopupChangeAddressWithDialog); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);  // OnCreate
}

TCheatDBEditForm::~TCheatDBEditForm()
{
    FormDestroy(this);  // OnDestroy
}

void TCheatDBEditForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TCheatDBEditForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    SearchForUpdates = false;

    setWindowTitle(LanguageSetup->EditCheats());
    ui.GameLabel->setText(LanguageSetup->EditCheatsGameName());
    ui.GameAddButton->setToolTip(LanguageSetup->EditCheatsGameAdd());
    ui.GameEditButton->setToolTip(LanguageSetup->EditCheatsGameEdit());
    ui.GameDeleteButton->setToolTip(LanguageSetup->EditCheatsGameDelete());
    ui.ActionLabel->setText(LanguageSetup->EditCheatsActionName());
    ui.ActionAddButton->setToolTip(LanguageSetup->EditCheatsActionAdd());
    ui.ActionEditButton->setToolTip(LanguageSetup->EditCheatsActionEdit());
    ui.ActionDeleteButton->setToolTip(LanguageSetup->EditCheatsActionDelete());
    ui.FileMaskEdit_EditLabel->setText(LanguageSetup->EditCheatsFileMask());
    ui.ActionStepLabel->setText(LanguageSetup->EditCheatsActionStepName());
    ui.ActionStepAddButton->setToolTip(LanguageSetup->EditCheatsActionStepAdd());
    ui.ActionStepDeleteButton->setToolTip(LanguageSetup->EditCheatsActionStepDelete());
    ui.AddActionStepPopupChangeAddress->setText(LanguageSetup->EditCheatsActionStepAddChangeAddress());
    ui.AddActionStepPopupChangeAddressWithDialog->setText(LanguageSetup->EditCheatsActionStepAddChangeAddressWithDialog());

    ui.ChangeAddressAddressEdit_EditLabel->setText(LanguageSetup->EditCheatsChangeAddress());
    ui.ChangeAddressAddressLabel->setText(LanguageSetup->EditCheatsChangeAddressInfo());
    ui.ChangeAddressNewValueEdit_EditLabel->setText(LanguageSetup->EditCheatsChangeAddressNewValue());
    ui.ChangeAddressBytesLabel->setText(LanguageSetup->EditCheatsChangeAddressBytes());
    ui.ChangeAddressInfoLabel->setText(LanguageSetup->EditCheatsHexInfo());

    ui.ChangeAddressWithDialogAddressEdit_EditLabel->setText(LanguageSetup->EditCheatsChangeAddress());
    ui.ChangeAddressWithDialogAddressLabel->setText(LanguageSetup->EditCheatsChangeAddressInfo());
    ui.ChangeAddressWithDialogDefaultAddressRadioButton->setText(LanguageSetup->EditCheatsChangeAddressDefaultAdress());
    ui.ChangeAddressWithDialogDefaultFixedRadioButton->setText(LanguageSetup->EditCheatsChangeAddressDefaultFixedValue());
    ui.ChangeAddressWithDialogPromptEdit_EditLabel->setText(LanguageSetup->EditCheatsChangeAddressDialogPrompt());
    ui.ChangeAddressWithDialogMinValueEdit_EditLabel->setText(LanguageSetup->EditCheatsChangeAddressNewValueMin());
    ui.ChangeAddressWithDialogMaxValueEdit_EditLabel->setText(LanguageSetup->EditCheatsChangeAddressNewValueMax());
    ui.ChangeAddressWithDialogBytesLabel->setText(LanguageSetup->EditCheatsChangeAddressBytes());
    ui.ChangeAddressWithDialogInfoLabel->setText(LanguageSetup->EditCheatsChangeAddressNewValueInfo());
    ui.ChangeAddressWithDialogInfoLabel2->setText(LanguageSetup->EditCheatsHexInfo());

    ui.InternalLabel->setText(LanguageSetup->EditCheatsApplyInternal());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.UpdateButton->setText(LanguageSetup->EditCheatsSearchForUpdates());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
    UserIconLoader->DialogImage(DI_Update, ui.UpdateButton);

    UserIconLoader->DialogImage(DI_Add, ui.GameAddButton);
    UserIconLoader->DialogImage(DI_Edit, ui.GameEditButton);
    UserIconLoader->DialogImage(DI_Delete, ui.GameDeleteButton);
    UserIconLoader->DialogImage(DI_Add, ui.ActionAddButton);
    UserIconLoader->DialogImage(DI_Edit, ui.ActionEditButton);
    UserIconLoader->DialogImage(DI_Delete, ui.ActionDeleteButton);
    UserIconLoader->DialogImage(DI_Add, ui.ActionStepAddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.ActionStepDeleteButton);

    CheatDB = SmartLoadCheatsDB();
    if (CheatDB == nullptr) return;

    LastGame = -1; GamesListUpdating = false;
    LastAction = -1; ActionListUpdating = false;
    LastActionStep = -1; ActionStepsListUpdating = false;
    LoadGamesList();
}

void TCheatDBEditForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    delete CheatDB; CheatDB = nullptr;
}

void TCheatDBEditForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (CheatDB == nullptr) {
        MessageDlg(LanguageSetup->ApplyCheatDataBaseError(), mtError, mbOK, 0, this);
        QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);   // PostMessage(Handle,WM_Close,0,0)
        return;
    }
}

void TCheatDBEditForm::LoadGamesList()
{
    int I;

    if (LastGame > 0) {
        SetItemIndex(ui.GameComboBox, -1);
        GameComboBoxChange(this);
    }
    LastGame = -1;

    GamesListUpdating = true;
    {
        TStringList St;
        for (I = 0; I < CheatDB->Count(); I++) St.AddObject(CheatDB->Game(I)->Name(), CheatDB->Game(I));
        St.Sort();
        SetItems(ui.GameComboBox, St);
    }
    GamesListUpdating = false;

    if (ui.GameComboBox->count() > 0) SetItemIndex(ui.GameComboBox, 0);
    ui.ActionAddButton->setEnabled(ui.GameComboBox->count() > 0);
    GameComboBoxChange(this);
}

void TCheatDBEditForm::SelectGame(TCheatGameRecord* G)
{
    int I;
    for (I = 0; I < ui.GameComboBox->count(); I++) if (static_cast<TCheatGameRecord*>(ItemObject(ui.GameComboBox, I)) == G) {
        SetItemIndex(ui.GameComboBox, I);
        GameComboBoxChange(this);
        break;
    }
}

void TCheatDBEditForm::GameComboBoxChange(QObject* Sender)
{
    int I;
    TCheatGameRecord* G;

    if (GamesListUpdating) return;

    if (LastGame >= 0) {
        SetItemIndex(ui.ActionComboBox, -1);
        ActionComboBoxChange(Sender);
    }

    if (ui.GameComboBox->currentIndex() >= 0) LastGame = CheatDB->IndexOf(static_cast<TCheatGameRecord*>(ItemObject(ui.GameComboBox, ui.GameComboBox->currentIndex()))); else LastGame = -1;
    ui.GameEditButton->setEnabled(ui.GameComboBox->currentIndex() >= 0);
    ui.GameDeleteButton->setEnabled(ui.GameComboBox->currentIndex() >= 0);

    ActionListUpdating = true;
    {
        TStringList St;
        if (LastGame >= 0) {
            G = CheatDB->Game(LastGame);
            for (I = 0; I < G->Count(); I++) St.AddObject(G->Action(I)->Name(), G->Action(I));
        }
        St.Sort();
        SetItems(ui.ActionComboBox, St);
    }
    ActionListUpdating = false;

    if (ui.ActionComboBox->count() > 0) SetItemIndex(ui.ActionComboBox, 0);
    ui.ActionStepAddButton->setEnabled(ui.ActionComboBox->count() > 0);
    ActionComboBoxChange(Sender);
}

void TCheatDBEditForm::SelectAction(TCheatAction* A)
{
    int I;
    for (I = 0; I < ui.ActionComboBox->count(); I++) if (static_cast<TCheatAction*>(ItemObject(ui.ActionComboBox, I)) == A) {
        SetItemIndex(ui.ActionComboBox, I);
        ActionComboBoxChange(this);
        break;
    }
}

void TCheatDBEditForm::ActionComboBoxChange(QObject* Sender)
{
    int I;
    TCheatAction* A;
    QString S;

    if (ActionListUpdating) return;

    if ((LastGame >= 0) && (LastAction >= 0)) {
        CheatDB->Game(LastGame)->Action(LastAction)->SetFileMask(ui.FileMaskEdit->text());
        SetItemIndex(ui.ActionStepComboBox, -1);
        ActionStepComboBoxChange(Sender);
    }

    if ((LastGame >= 0) && (ui.ActionComboBox->currentIndex() >= 0)) LastAction = CheatDB->Game(LastGame)->IndexOf(static_cast<TCheatAction*>(ItemObject(ui.ActionComboBox, ui.ActionComboBox->currentIndex()))); else LastAction = -1;
    ui.ActionEditButton->setEnabled(ui.ActionComboBox->currentIndex() >= 0);
    ui.ActionDeleteButton->setEnabled(ui.ActionComboBox->currentIndex() >= 0);

    ActionStepsListUpdating = true;
    {
        TStringList St;
        if ((LastGame >= 0) && (LastAction >= 0)) {
            A = CheatDB->Game(LastGame)->Action(LastAction);
            for (I = 0; I < A->Count(); I++) {
                S = "";
                if (auto* Step = dynamic_cast<TCheatActionStepChangeAddress*>(A->ActionStep(I))) {
                    S = LanguageSetup->EditCheatsActionStepAddChangeAddress();
                    S = S + " (" + Step->Addresses() + ")";
                }
                if (auto* Step = dynamic_cast<TCheatActionStepChangeAddressWithDialog*>(A->ActionStep(I))) {
                    S = LanguageSetup->EditCheatsActionStepAddChangeAddressWithDialog();
                    S = S + " (" + Step->Addresses() + ")";
                }
                if (auto* Step = dynamic_cast<TCheatActionStepInternal*>(A->ActionStep(I))) {
                    S = LanguageSetup->EditCheatsActionStepInternal();
                    S = S + " (" + IntToStr(Step->Nr()) + ")";
                }
                St.AddObject(S, A->ActionStep(I));
            }
        }
        SetItems(ui.ActionStepComboBox, St);
    }
    ActionStepsListUpdating = false;

    ui.FileMaskEdit->setEnabled(LastAction >= 0);

    if (LastAction < 0) {
        ui.FileMaskEdit->setText("");
    } else {
        ui.FileMaskEdit->setText(CheatDB->Game(LastGame)->Action(LastAction)->FileMask());
    }

    if (ui.ActionStepComboBox->count() > 0) SetItemIndex(ui.ActionStepComboBox, 0);
    ActionStepComboBoxChange(Sender);
}

void TCheatDBEditForm::ActionStepComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    TCheatActionStep* A;
    bool B;
    int I;

    if (ActionStepsListUpdating) return;

    SetComboHint(ui.ActionStepComboBox);
    ActionStepComboBoxDropDown(ui.ActionStepComboBox);   // OnDropDown (QComboBox has no drop down event, the width is set in advance)

    if ((LastGame >= 0) && (LastAction >= 0) && (LastActionStep >= 0)) {
        A = CheatDB->Game(LastGame)->Action(LastAction)->ActionStep(LastActionStep);
        switch (ui.Notebook->currentIndex()) {
            case 0: B = (dynamic_cast<TCheatActionStepChangeAddress*>(A) != nullptr); break;
            case 1: B = (dynamic_cast<TCheatActionStepChangeAddressWithDialog*>(A) != nullptr); break;
            case 2: B = (dynamic_cast<TCheatActionStepInternal*>(A) != nullptr); break;
            default: B = true;
        }
        if (!B) A = nullptr;

        switch (ui.Notebook->currentIndex()) {
            case 0: {
                /* <ChangeAddress Addresses="..." Bytes="..." NewValue="..."/> */
                if (A == nullptr) A = new TCheatActionStepChangeAddress(nullptr);
                TCheatActionStepChangeAddress* C = static_cast<TCheatActionStepChangeAddress*>(A);
                C->SetAddresses(ui.ChangeAddressAddressEdit->text());
                C->SetNewValue(ui.ChangeAddressNewValueEdit->text());
                switch (ui.ChangeAddressBytesComboBox->currentIndex()) {
                    case 0: C->SetBytes(1); break;
                    case 1: C->SetBytes(2); break;
                    case 2: C->SetBytes(4); break;
                }
                break;
            }
            case 1: {
                /* <ChangeAddressWithDialog Addresses="..." Bytes="..." DefaultValue="..."/DefaultValueAddress="..." Prompt="..." [MinValue="..."] [MaxValue="..."]/> */
                if (A == nullptr) A = new TCheatActionStepChangeAddressWithDialog(nullptr);
                TCheatActionStepChangeAddressWithDialog* C = static_cast<TCheatActionStepChangeAddressWithDialog*>(A);
                C->SetAddresses(ui.ChangeAddressWithDialogAddressEdit->text());
                if (ui.ChangeAddressWithDialogDefaultFixedRadioButton->isChecked()) {
                    C->SetDefaultValueAddress("");
                    C->SetDefaultValue(ui.ChangeAddressWithDialogDefaultFixedEdit->text());
                } else {
                    C->SetDefaultValue("");
                    C->SetDefaultValueAddress(ui.ChangeAddressWithDialogDefaultAddressEdit->text());
                }
                C->SetDialogPrompt(ui.ChangeAddressWithDialogPromptEdit->text());
                C->SetMinValue(Trim(ui.ChangeAddressWithDialogMinValueEdit->text()));
                C->SetMaxValue(Trim(ui.ChangeAddressWithDialogMaxValueEdit->text()));
                switch (ui.ChangeAddressWithDialogBytesComboBox->currentIndex()) {
                    case 0: C->SetBytes(1); break;
                    case 1: C->SetBytes(2); break;
                    case 2: C->SetBytes(4); break;
                }
                break;
            }
            case 2: {
                /* <Internal Nr="..."/> */
                if (A == nullptr) A = new TCheatActionStepInternal(nullptr);
                static_cast<TCheatActionStepInternal*>(A)->SetNr(ui.InternalEdit->value());
                break;
            }
            default: A = nullptr;
        }
        if (!B) {
            for (I = 0; I < ui.ActionStepComboBox->count(); I++) if (ItemObject(ui.ActionStepComboBox, I) == CheatDB->Game(LastGame)->Action(LastAction)->ActionStep(LastActionStep)) {
                ui.ActionStepComboBox->setItemData(I, QVariant::fromValue<void*>(A)); break;
            }
            CheatDB->Game(LastGame)->Action(LastAction)->SetNewActionStep(A, LastActionStep);
        }
    }

    if ((LastGame >= 0) && (LastAction >= 0) && (ui.ActionStepComboBox->currentIndex() >= 0)) LastActionStep = CheatDB->Game(LastGame)->Action(LastAction)->IndexOf(static_cast<TCheatActionStep*>(ItemObject(ui.ActionStepComboBox, ui.ActionStepComboBox->currentIndex()))); else LastActionStep = -1;
    ui.ActionStepDeleteButton->setEnabled(ui.ActionStepComboBox->currentIndex() >= 0);

    ui.Notebook->setVisible(LastActionStep >= 0);
    if (LastActionStep >= 0) {
        A = CheatDB->Game(LastGame)->Action(LastAction)->ActionStep(LastActionStep);

        if (auto* C = dynamic_cast<TCheatActionStepChangeAddress*>(A)) {
            /* <ChangeAddress Addresses="..." Bytes="..." NewValue="..."/> */
            ui.Notebook->setCurrentIndex(0);
            ui.ChangeAddressAddressEdit->setText(C->Addresses());
            ui.ChangeAddressNewValueEdit->setText(C->NewValue());
            ui.ChangeAddressBytesComboBox->setCurrentIndex(2);
            switch (C->Bytes()) {
                case 1: ui.ChangeAddressBytesComboBox->setCurrentIndex(0); break;
                case 2: ui.ChangeAddressBytesComboBox->setCurrentIndex(1); break;
                case 3: ui.ChangeAddressBytesComboBox->setCurrentIndex(2); break;   // sic (original code)
            }
        }

        if (auto* C = dynamic_cast<TCheatActionStepChangeAddressWithDialog*>(A)) {
            /* <ChangeAddressWithDialog Addresses="..." Bytes="..." DefaultValue="..."/DefaultValueAddress="..." Prompt="..." [MinValue="..."] [MaxValue="..."]/> */
            ui.Notebook->setCurrentIndex(1);
            ui.ChangeAddressWithDialogAddressEdit->setText(C->Addresses());
            {
                // VCL does not fire OnChange of the edits for programmatic changes in this order of events
                const QSignalBlocker B1(ui.ChangeAddressWithDialogDefaultAddressEdit);
                const QSignalBlocker B2(ui.ChangeAddressWithDialogDefaultFixedEdit);
                if (Trim(C->DefaultValue()) != "") {
                    ui.ChangeAddressWithDialogDefaultAddressEdit->setText("");
                    ui.ChangeAddressWithDialogDefaultFixedRadioButton->setChecked(true);
                    ui.ChangeAddressWithDialogDefaultFixedEdit->setText(C->DefaultValue());
                } else {
                    ui.ChangeAddressWithDialogDefaultFixedEdit->setText("");
                    ui.ChangeAddressWithDialogDefaultAddressRadioButton->setChecked(true);
                    ui.ChangeAddressWithDialogDefaultAddressEdit->setText(C->DefaultValueAddress());
                }
            }
            ui.ChangeAddressWithDialogPromptEdit->setText(C->DialogPrompt());
            ui.ChangeAddressWithDialogMinValueEdit->setText(C->MinValue());
            ui.ChangeAddressWithDialogMaxValueEdit->setText(C->MaxValue());
            switch (C->Bytes()) {
                case 1: ui.ChangeAddressWithDialogBytesComboBox->setCurrentIndex(0); break;
                case 2: ui.ChangeAddressWithDialogBytesComboBox->setCurrentIndex(1); break;
                case 3: ui.ChangeAddressWithDialogBytesComboBox->setCurrentIndex(2); break;   // sic (original code)
            }
        }

        if (auto* C = dynamic_cast<TCheatActionStepInternal*>(A)) {
            /* <Internal Nr="..."/> */
            ui.Notebook->setCurrentIndex(2);
            ui.InternalEdit->setValue(C->Nr());
        }
    }
}

void TCheatDBEditForm::ActionStepComboBoxDropDown(QObject* Sender)
{
    Q_UNUSED(Sender);
    SetComboDropDownDropDownWidth(ui.ActionStepComboBox);
}

void TCheatDBEditForm::ChangeAddressWithDialogDefaultEditChange(QObject* Sender)
{
    if (Sender == ui.ChangeAddressWithDialogDefaultAddressEdit) ui.ChangeAddressWithDialogDefaultAddressRadioButton->setChecked(true);
    if (Sender == ui.ChangeAddressWithDialogDefaultFixedEdit) ui.ChangeAddressWithDialogDefaultFixedRadioButton->setChecked(true);
}

void TCheatDBEditForm::ButtonWork(QObject* Sender)
{
    QString S;
    TCheatGameRecord* G;
    TCheatAction* A;
    TCheatActionStep* A2;
    QPoint P;

    switch (Sender->property("tag").toInt()) {
        case 0: /* Add game */
            if (!InputQuery(LanguageSetup->EditCheatsGameAdd(), LanguageSetup->EditCheatsGameAddPrompt(), S, this)) return;
            G = new TCheatGameRecord(nullptr); G->SetName(S); CheatDB->Add(G);
            LoadGamesList(); SelectGame(G);
            break;
        case 1: /* Edit game */
            if (ui.GameComboBox->currentIndex() < 0) return;
            G = static_cast<TCheatGameRecord*>(ItemObject(ui.GameComboBox, ui.GameComboBox->currentIndex()));
            S = G->Name(); if (!InputQuery(LanguageSetup->EditCheatsGameEdit(), LanguageSetup->EditCheatsGameEditPrompt(), S, this)) return;
            G->SetName(S); LoadGamesList(); SelectGame(G);
            break;
        case 2: /* Delete game */
            if (ui.GameComboBox->currentIndex() < 0) return;
            G = static_cast<TCheatGameRecord*>(ItemObject(ui.GameComboBox, ui.GameComboBox->currentIndex()));
            if (MessageDlg(Format(LanguageSetup->EditCheatsGameDeletePrompt(), {G->Name()}), mtConfirmation, mbYes | mbNo, 0, this) != mrYes) return;
            CheatDB->Delete(G); LastGame = -1; LoadGamesList();
            break;
        case 3: /* Add action */
            if (ui.GameComboBox->currentIndex() < 0) return;
            G = static_cast<TCheatGameRecord*>(ItemObject(ui.GameComboBox, ui.GameComboBox->currentIndex()));
            if (!InputQuery(LanguageSetup->EditCheatsActionAdd(), LanguageSetup->EditCheatsActionAddPrompt(), S, this)) return;
            A = new TCheatAction(nullptr); A->SetName(S); G->Add(A);
            LoadGamesList(); SelectGame(G); SelectAction(A);
            break;
        case 4: /* Edit action */
            if (ui.GameComboBox->currentIndex() < 0) return;
            G = static_cast<TCheatGameRecord*>(ItemObject(ui.GameComboBox, ui.GameComboBox->currentIndex()));
            if (ui.ActionComboBox->currentIndex() < 0) return;
            A = static_cast<TCheatAction*>(ItemObject(ui.ActionComboBox, ui.ActionComboBox->currentIndex()));
            S = A->Name(); if (!InputQuery(LanguageSetup->EditCheatsActionEdit(), LanguageSetup->EditCheatsActionEditPrompt(), S, this)) return;
            A->SetName(S); LoadGamesList(); SelectGame(G); SelectAction(A);
            break;
        case 5: /* Delete action */
            if (ui.GameComboBox->currentIndex() < 0) return;
            G = static_cast<TCheatGameRecord*>(ItemObject(ui.GameComboBox, ui.GameComboBox->currentIndex()));
            if (ui.ActionComboBox->currentIndex() < 0) return;
            A = static_cast<TCheatAction*>(ItemObject(ui.ActionComboBox, ui.ActionComboBox->currentIndex()));
            if (MessageDlg(Format(LanguageSetup->EditCheatsActionDeletePrompt(), {A->Name(), G->Name()}), mtConfirmation, mbYes | mbNo, 0, this) != mrYes) return;
            G->Delete(A); LastAction = -1; LoadGamesList(); SelectGame(G);
            break;
        case 6: /* Add action step */
            if (ui.ActionComboBox->currentIndex() < 0) return;
            P = mapToGlobal(QPoint(ui.ActionStepAddButton->x(), ui.ActionStepAddButton->y()));
            ui.AddActionStepPopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
            break;
        case 7: /* Delete action step */
            if (ui.GameComboBox->currentIndex() < 0) return;
            G = static_cast<TCheatGameRecord*>(ItemObject(ui.GameComboBox, ui.GameComboBox->currentIndex()));
            if (ui.ActionComboBox->currentIndex() < 0) return;
            A = static_cast<TCheatAction*>(ItemObject(ui.ActionComboBox, ui.ActionComboBox->currentIndex()));
            if (ui.ActionStepComboBox->currentIndex() < 0) return;
            if (MessageDlg(LanguageSetup->EditCheatsActionStepDeletePrompt(), mtConfirmation, mbYes | mbNo, 0, this) != mrYes) return;
            A->Delete(ui.ActionStepComboBox->currentIndex()); LastActionStep = -1; LoadGamesList(); SelectGame(G); SelectAction(A);
            break;
        case 8:
            if (ui.GameComboBox->currentIndex() < 0) return;
            G = static_cast<TCheatGameRecord*>(ItemObject(ui.GameComboBox, ui.GameComboBox->currentIndex()));
            if (ui.ActionComboBox->currentIndex() < 0) return;
            A = static_cast<TCheatAction*>(ItemObject(ui.ActionComboBox, ui.ActionComboBox->currentIndex()));
            A2 = new TCheatActionStepChangeAddress(nullptr); A->Add(A2);
            LoadGamesList(); SelectGame(G); SelectAction(A);
            SetItemIndex(ui.ActionStepComboBox, ui.ActionStepComboBox->count() - 1); ActionStepComboBoxChange(Sender);
            break;
        case 9:
            if (ui.GameComboBox->currentIndex() < 0) return;
            G = static_cast<TCheatGameRecord*>(ItemObject(ui.GameComboBox, ui.GameComboBox->currentIndex()));
            if (ui.ActionComboBox->currentIndex() < 0) return;
            A = static_cast<TCheatAction*>(ItemObject(ui.ActionComboBox, ui.ActionComboBox->currentIndex()));
            A2 = new TCheatActionStepChangeAddressWithDialog(nullptr); A->Add(A2);
            LoadGamesList(); SelectGame(G); SelectAction(A);
            SetItemIndex(ui.ActionStepComboBox, ui.ActionStepComboBox->count() - 1); ActionStepComboBoxChange(Sender);
            break;
    }
}

void TCheatDBEditForm::OKButtonClick(QObject* Sender)
{
    SetItemIndex(ui.GameComboBox, -1);
    GameComboBoxChange(Sender);
    SmartSaveCheatsDB(CheatDB, this);
}

void TCheatDBEditForm::UpdateButtonClick(QObject* Sender)
{
    OKButtonClick(Sender);
    SearchForUpdates = true;
    setModalResult(mrOk);
}

void TCheatDBEditForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    HelpCommandContext(ID_ExtrasEditCheatDB);
}

void TCheatDBEditForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

/* global */

bool ShowCheatDBEditDialog(QWidget* AOwner, bool& SearchForUpdates)
{
    bool result;
    SearchForUpdates = false;
    CheatsDBUpdateCheckIfSetup(ApplicationMainForm());
    CheatDBEditForm = new TCheatDBEditForm(AOwner);
    result = (CheatDBEditForm->ShowModal() == mrOk);
    SearchForUpdates = CheatDBEditForm->SearchForUpdates;
    delete CheatDBEditForm;
    CheatDBEditForm = nullptr;
    return result;
}
