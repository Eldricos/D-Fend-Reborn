// C++ port of MiniRunFormUnit.pas (implementation section).
#include "MiniRunFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "DosBoxUnit.h"
#include "ScummVMUnit.h"
#include "GameDBToolsUnit.h"
#include "WindowsProfileUnit.h"
#include "IconLoaderUnit.h"
#include "MainUnit.h"

TMiniRunForm* MiniRunForm = nullptr;

TMiniRunForm::TMiniRunForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.ListBox, &QListWidget::itemDoubleClicked, this, [this]() { OKButtonClick(ui.ListBox); });
    // OnKeyUp of the form (KeyPreview=True), ListBox and NameEdit.  Every key release in the
    // form calls NameEditKeyUp (the original calls it twice: form preview + control).
    ui.ListBox->installEventFilter(this);
    ui.NameEdit->installEventFilter(this);
    FormCreate(this);   // OnCreate
}

TMiniRunForm::~TMiniRunForm()
{
    FormDestroy(this);   // OnDestroy
}

void TMiniRunForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

bool TMiniRunForm::eventFilter(QObject *O, QEvent *E)
{
    if (E->type() == QEvent::KeyRelease && (O == ui.ListBox || O == ui.NameEdit)) {
        QKeyEvent* KE = static_cast<QKeyEvent*>(E);
        if (!KE->isAutoRepeat()) {
            int Key = KE->key();
            if (Key == Qt::Key_Enter) Key = Qt::Key_Return;
            NameEditKeyUp(O, Key, KE->modifiers() & ~Qt::KeypadModifier);
        }
    }
    return TVclForm::eventFilter(O, E);
}

void TMiniRunForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    List = new TStringList;
    ListUpper = new TStringList;

    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    setWindowTitle(LanguageSetup->TrayPopupRunInfo());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);

    const QRect WorkAreaRect = QGuiApplication::primaryScreen()->availableGeometry();
    move(WorkAreaRect.x() + WorkAreaRect.width() - frameGeometry().width(),
         WorkAreaRect.y() + WorkAreaRect.height() - frameGeometry().height());
}

void TMiniRunForm::FormShow(QObject* Sender)
{
    for (int I = 0; I < GameDB->Count(); I++) List->AddObject(GameDB->Game(I)->Name(), GameDB->Game(I));
    List->Sort();
    for (int I = 0; I < List->Count(); I++) ListUpper->Add(Trim(ExtUpperCase((*List)[I])));

    int W = 0; NameEditKeyUp(Sender, W, Qt::NoModifier);
}

void TMiniRunForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (List != nullptr) { delete List; List = nullptr; }
    if (ListUpper != nullptr) { delete ListUpper; ListUpper = nullptr; }
}

void TMiniRunForm::NameEditKeyUp(QObject* Sender, int& Key, TShiftState Shift)
{
    void *G, *O;
    int GNr;

    const QString S = Trim(ExtUpperCase(ui.NameEdit->text()));

    if (ui.ListBox->currentRow() >= 0) G = ui.ListBox->item(ui.ListBox->currentRow())->data(Qt::UserRole).value<void*>(); else G = nullptr;
    GNr = -1;

    ui.ListBox->setUpdatesEnabled(false);
    {
        const QSignalBlocker Blocker(ui.ListBox);
        ui.ListBox->clear();
        for (int I = 0; I < ListUpper->Count(); I++) if (S.isEmpty() || Pos(S, (*ListUpper)[I]) > 0) {
            O = List->Objects(I);
            if (O == G) GNr = I;
            QListWidgetItem* Item = new QListWidgetItem((*List)[I], ui.ListBox);
            Item->setData(Qt::UserRole, QVariant::fromValue<void*>(O));
        }
    }
    ui.ListBox->setUpdatesEnabled(true);

    // Note: as in the original GNr is the index in the full list, not in the filtered list
    if (GNr >= 0 && GNr < ui.ListBox->count()) {
        ui.ListBox->setCurrentRow(GNr);
        ui.ListBox->scrollToItem(ui.ListBox->item(GNr), QAbstractItemView::PositionAtTop);
    }

    if (Key == Qt::Key_Return && Shift == Qt::NoModifier && ModalResult() == mrNone) OKButtonClick(Sender);
}

void TMiniRunForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    TGame* G;

    setModalResult(mrOk); // {if triggered by listbox dblclick}

    if (ui.ListBox->currentRow() < 0) {
        MessageDlg(LanguageSetup->MessageNoGameSelected(), mtError, mbOK, 0);
        setModalResult(mrNone); return;
    }

    G = static_cast<TGame*>(ui.ListBox->item(ui.ListBox->currentRow())->data(Qt::UserRole).value<void*>());
    if (ScummVMMode(G)) {
        RunScummVMGame(G);
    } else {
        if (WindowsExeMode(G))
            RunWindowsGame(G);
        else
            RunGame(G, DFendRebornMainForm->DeleteOnExit);
    }
}

// { global }

bool ShowMiniRunDialog(QWidget* AOwner, TGameDB* AGameDB)
{
    MiniRunForm = new TMiniRunForm(AOwner);
    MiniRunForm->GameDB = AGameDB;
    const bool result = (MiniRunForm->ShowModal() == mrOk);
    delete MiniRunForm;
    MiniRunForm = nullptr;
    return result;
}
