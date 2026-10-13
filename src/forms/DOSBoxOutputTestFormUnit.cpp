// C++ port of DOSBoxOutputTestFormUnit.pas (implementation section).
#include "DOSBoxOutputTestFormUnit.h"
#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "DosBoxUnit.h"
#include "MainUnit.h"
#include "SmallWaitFormUnit.h"
#include "DOSBoxTempUnit.h"
#include "PrgConsts.h"
#include "IconLoaderUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "Process.h"

TDOSBoxOutputTestForm* DOSBoxOutputTestForm = nullptr;

// TStringGrid.Cells[C,R] on a grid with FixedCols=1 and FixedRows=1: row 0 is the
// horizontal header, column 0 the vertical header.
static void SetCell(QTableWidget* Tab, int C, int R, const QString& S)
{
    if (R == 0 && C == 0) {
        // Top left fixed cell: shown by a label over the corner button
        QLabel* L = Tab->findChild<QLabel*>("vclCornerCell");
        if (L == nullptr) {
            L = new QLabel(Tab);
            L->setObjectName("vclCornerCell");
            L->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            L->setIndent(2);
            L->setAttribute(Qt::WA_TransparentForMouseEvents);
            auto Place = [Tab, L]() {
                L->setGeometry(Tab->frameWidth(), Tab->frameWidth(), Tab->verticalHeader()->width(), Tab->horizontalHeader()->height());
                L->raise();
            };
            QObject::connect(Tab->verticalHeader(), &QHeaderView::geometriesChanged, L, Place);
            QObject::connect(Tab->horizontalHeader(), &QHeaderView::geometriesChanged, L, Place);
            Place();
            L->show();
        }
        L->setText(S);
        return;
    }
    if (R == 0) {
        QTableWidgetItem* H = Tab->horizontalHeaderItem(C - 1);
        if (H == nullptr) { H = new QTableWidgetItem(); Tab->setHorizontalHeaderItem(C - 1, H); }
        H->setText(S);
        return;
    }
    if (C == 0) {
        QTableWidgetItem* H = Tab->verticalHeaderItem(R - 1);
        if (H == nullptr) { H = new QTableWidgetItem(); Tab->setVerticalHeaderItem(R - 1, H); }
        H->setText(S);
        return;
    }
    QTableWidgetItem* Item = Tab->item(R - 1, C - 1);
    if (Item == nullptr) { Item = new QTableWidgetItem(); Tab->setItem(R - 1, C - 1, Item); }
    Item->setText(S);
}

static QString GetCell(QTableWidget* Tab, int C, int R)
{
    QTableWidgetItem* Item;
    if (R == 0 && C == 0) { QLabel* L = Tab->findChild<QLabel*>("vclCornerCell"); return L ? L->text() : QString(); }
    if (R == 0) Item = Tab->horizontalHeaderItem(C - 1);
    else if (C == 0) Item = Tab->verticalHeaderItem(R - 1);
    else Item = Tab->item(R - 1, C - 1);
    return Item ? Item->text() : QString();
}

TDOSBoxOutputTestForm::TDOSBoxOutputTestForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.TestButton, &QPushButton::clicked, this, [this]() { TestButtonClick(ui.TestButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
}

TDOSBoxOutputTestForm::~TDOSBoxOutputTestForm() = default;

void TDOSBoxOutputTestForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

void TDOSBoxOutputTestForm::keyPressEvent(QKeyEvent *E)
{
    int Key = E->key();
    FormKeyDown(this, Key, E->modifiers());
    TVclForm::keyPressEvent(E);
}

void TDOSBoxOutputTestForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    TStringList* St;

    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    setWindowTitle(LanguageSetup->DOSBoxOutputTest());
    ui.InfoLabel->setText(LanguageSetup->DOSBoxOutputTestInfo());

    St = ValueToList(ConfOpt->Render(), ";,");
    ui.Tab->setRowCount(St->Count());   // RowCount:=1+St.Count (1 fixed row = header)
    SetCell(ui.Tab, 0, 0, LanguageSetup->GameRender());
    SetCell(ui.Tab, 1, 0, LanguageSetup->DOSBoxOutputTestWindow());
    SetCell(ui.Tab, 2, 0, LanguageSetup->DOSBoxOutputTestFullscreen());
    for (int I = 0; I < St->Count(); I++) {
        SetCell(ui.Tab, 0, I + 1, (*St)[I]);
        SetCell(ui.Tab, 1, I + 1, LanguageSetup->DOSBoxOutputTestNotChecked());
        SetCell(ui.Tab, 2, I + 1, LanguageSetup->DOSBoxOutputTestNotChecked());
    }
    delete St;
    // ColWidths[0] is the width of the vertical header
    ui.Tab->verticalHeader()->setFixedWidth(125);
    const int ClientWidth = ui.Tab->contentsRect().width();
    ui.Tab->setColumnWidth(0, (ClientWidth - 125 - 5) / 2);
    ui.Tab->setColumnWidth(1, (ClientWidth - 125 - 5) / 2);

    ui.TestButton->setText(LanguageSetup->DOSBoxOutputTestStart());
    ui.CloseButton->setText(LanguageSetup->Close());
    ui.HelpButton->setText(LanguageSetup->Help());

    UserIconLoader->DialogImage(DI_OK, ui.TestButton);
    UserIconLoader->DialogImage(DI_Close, ui.CloseButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
}

bool TDOSBoxOutputTestForm::SingleCheck(const QString& Renderer, bool Fullscreen)
{
    Q_UNUSED(Renderer);   // as in the original: the renderer is not set in the temporary profile
    TTempGame* TempGame;
    THandle Handle;
    TStringList* St;
    bool result;

    DeleteFile(TempDir() + RenderTestTempFile);

    TempGame = new TTempGame;
    TempGame->Game()->SetStartFullscreen(Fullscreen);
    St = new TStringList;
    St->Add("dir");
    St->Add("dir > C:\\" + RenderTestTempFile);
    St->Add("exit");
    TempGame->Game()->SetAutoexec(StringListToString(St));
    delete St;
    TempGame->Game()->SetNrOfMounts(1);
    TempGame->Game()->SetMount0(TempDir() + ";Drive;C;false;");
    TempGame->Game()->SetGameExe("");
    TempGame->Game()->SetCloseDosBoxAfterGameExit(false);
    TempGame->Game()->StoreAllValues();
    Handle = RunCommandAndGetHandle(TempGame->Game(), DFendRebornMainForm->DeleteOnExit, "", false);
    // WaitForSingleObject(Handle,10000); result:=(WaitForSingleObject(Handle,0)=WAIT_OBJECT_0);
    if (Handle != 0) WaitForProcess(Handle, 10000, true);
    result = (Handle != 0) && !IsProcessRunning(Handle);
    if (!result && Handle != 0) TerminateProcessById(Handle);
    if (result) result = FileExists(TempDir() + RenderTestTempFile);
    if (result) result = (GetFileSize(TempDir() + RenderTestTempFile) > 0);
    DeleteFile(TempDir() + RenderTestTempFile);
    // CloseHandle(Handle) - not needed
    delete TempGame;
    return result;
}

void TDOSBoxOutputTestForm::CheckRenderer(const QString& Renderer, bool& Window, bool& Fullscreen)
{
    Window = SingleCheck(Renderer, false);
    Fullscreen = SingleCheck(Renderer, true);
}

void TDOSBoxOutputTestForm::TestButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    bool B1, B2;

    LoadAndShowSmallWaitForm(LanguageSetup->DOSBoxOutputTestRunning());
    try {
        const int RowCount = 1 + ui.Tab->rowCount();
        for (int I = 1; I <= RowCount - 1; I++) {
            CheckRenderer(GetCell(ui.Tab, 0, I), B1, B2);
            if (B1) SetCell(ui.Tab, 1, I, LanguageSetup->DOSBoxOutputTestSuccess()); else SetCell(ui.Tab, 1, I, LanguageSetup->DOSBoxOutputTestFailed());
            if (B2) SetCell(ui.Tab, 2, I, LanguageSetup->DOSBoxOutputTestSuccess()); else SetCell(ui.Tab, 2, I, LanguageSetup->DOSBoxOutputTestFailed());
        }
    } catch (...) {
        FreeSmallWaitForm();
        throw;
    }
    FreeSmallWaitForm();
}

void TDOSBoxOutputTestForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_RunDOSBoxOutputTest);
}

void TDOSBoxOutputTestForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if (Key == Qt::Key_F1 && Shift == Qt::NoModifier) HelpButtonClick(Sender);
}

// {global}

bool ShowDOSBoxOutputTestDialog(QWidget* AOwner, TConfOpt* ConfOpt)
{
    DOSBoxOutputTestForm = new TDOSBoxOutputTestForm(AOwner);
    DOSBoxOutputTestForm->ConfOpt = ConfOpt;
    const bool result = (DOSBoxOutputTestForm->ShowModal() == mrOk);
    delete DOSBoxOutputTestForm;
    DOSBoxOutputTestForm = nullptr;
    return result;
}
