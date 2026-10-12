// C++ port of HistoryFormUnit.pas (implementation section).
#include "HistoryFormUnit.h"
#include "GameDBToolsUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"
#include "HistoryUnit.h"
#include "Process.h"

THistoryForm* HistoryForm = nullptr;

THistoryForm::THistoryForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.ClearButton, &QPushButton::clicked, this, [this]() { ClearButtonClick(ui.ClearButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.ListView, &TListView::columnClick, this, [this](int Column) { ListViewColumnClick(ui.ListView, ui.ListView->Column(Column)); });
    connect(ui.ListView2, &TListView::columnClick, this, [this](int Column) { ListViewColumnClick(ui.ListView2, ui.ListView2->Column(Column)); });
    ui.ListView->OnCompare = [this](TListItem* Item1, TListItem* Item2) { int Compare = 0; ListViewCompare(ui.ListView, Item1, Item2, 0, Compare); return Compare; };
    ui.ListView2->OnCompare = [this](TListItem* Item1, TListItem* Item2) { int Compare = 0; ListViewCompare(ui.ListView2, Item1, Item2, 0, Compare); return Compare; };
    // OnKeyDown with KeyPreview=True
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key; FormKeyDown(this, K, Mods); return K == 0;
    });
}

THistoryForm::~THistoryForm() = default;

void THistoryForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

void THistoryForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=...; NoFlicker(PageControl/ListView/ListView2) - not needed in Qt

    setWindowTitle(LanguageSetup->HistoryForm());

    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.TabSheet1), LanguageSetup->HistoryForm());
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.TabSheet2), LanguageSetup->StatisticsCaption());

    ui.CloseButton->setText(LanguageSetup->Close());
    ui.ClearButton->setText(LanguageSetup->Clear());
    ui.HelpButton->setText(LanguageSetup->Help());

    UserIconLoader->DialogImage(DI_Close, ui.CloseButton);
    UserIconLoader->DialogImage(DI_Clear, ui.ClearButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    History->LoadHistory(ui.ListView);
    History->LoadStatistics(ui.ListView2);

    List1CompareMode = 0; List2CompareMode = 0;
}

void THistoryForm::ClearButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    History->Clear();
    close();
}

void THistoryForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ExtrasViewHistory);
}

void THistoryForm::ListViewColumnClick(QObject* Sender, TListColumn* Column)
{
    if (Column == nullptr) return;
    if (Sender == ui.ListView) {
        if (qAbs(List1CompareMode) == Column->Index() + 1) List1CompareMode = -List1CompareMode; else List1CompareMode = Column->Index() + 1;
        ui.ListView->AlphaSort();
    }

    if (Sender == ui.ListView2) {
        if (qAbs(List2CompareMode) == Column->Index() + 1) List2CompareMode = -List2CompareMode; else List2CompareMode = Column->Index() + 1;
        ui.ListView2->AlphaSort();
    }
}

void THistoryForm::ListViewCompare(QObject* Sender, TListItem* Item1, TListItem* Item2, int Data, int& Compare)
{
    Q_UNUSED(Data);
    int I;
    int J1, J2;
    TDateTime D1, D2;
    auto Nr = [](TListItem* Item) { return static_cast<int>(reinterpret_cast<intptr_t>(Item->Data())); };

    if (Sender == ui.ListView) I = List1CompareMode; else I = List2CompareMode;
    Compare = 0;
    switch (qAbs(I)) {
        case 0: return;
        case 1:
            // {Name}
            if (CompareStr(Item1->Caption(), Item2->Caption()) > 0) Compare = 1 * I;
            if (CompareStr(Item1->Caption(), Item2->Caption()) < 0) Compare = -1 * I;
            break;
        case 2:
            if (Sender == ui.ListView) {
                // {Start date&time}
                D1 = History->StartDateTime(Nr(Item1));
                D2 = History->StartDateTime(Nr(Item2));
                if (D1 > D2) Compare = 1 * I;
                if (D1 < D2) Compare = -1 * I;
            } else {
                // {Number of starts}
                J1 = History->StartCount(Nr(Item1));
                J2 = History->StartCount(Nr(Item2));
                if (J1 > J2) Compare = 1 * I;
                if (J1 < J2) Compare = -1 * I;
            }
            break;
        case 3:
            if (Sender == ui.ListView) {
                // {Number of starts}
                J1 = History->StartCount(Nr(Item1));
                J2 = History->StartCount(Nr(Item2));
                if (J1 > J2) Compare = 1 * I;
                if (J1 < J2) Compare = -1 * I;
            } else {
                // {First start}
                D1 = History->StartDateTimeFirst(Nr(Item1));
                D2 = History->StartDateTimeFirst(Nr(Item2));
                if (D1 > D2) Compare = 1 * I;
                if (D1 < D2) Compare = -1 * I;
            }
            break;
        case 4:
            if (Sender != ui.ListView) {
                // {Last start}
                D1 = History->StartDateTimeLast(Nr(Item1));
                D2 = History->StartDateTimeLast(Nr(Item2));
                if (D1 > D2) Compare = 1 * I;
                if (D1 < D2) Compare = -1 * I;
            }
            break;
    }
}

void THistoryForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if (Key == Qt::Key_F1 && Shift == Qt::NoModifier) HelpButtonClick(Sender);
}

// { global }

void ShowHistoryDialog(QWidget* AOwner)
{
    HistoryForm = new THistoryForm(AOwner);
    HistoryForm->ShowModal();
    delete HistoryForm;
    HistoryForm = nullptr;
}
