// C++ port of WaitFormUnit.pas (implementation section).
#include "WaitFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"

TWaitForm* WaitForm = nullptr;

// Application.MainForm
static QWidget* ApplicationMainForm()
{
    const QWidgetList L = QApplication::topLevelWidgets();
    for (QWidget* W : L) if (qobject_cast<QMainWindow*>(W) != nullptr) return W;
    return nullptr;
}

TWaitForm::TWaitForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    FormCreate(this);   // OnCreate
}

TWaitForm::~TWaitForm() = default;

// { TWaitForm }

TWaitForm* TWaitForm::CreateWaitForm(QWidget* AOwner, const QString& ACaption, int AMaxPos)
{
    TWaitForm* F;
    if (AOwner == nullptr) {
        if (ApplicationMainForm() != nullptr) F = new TWaitForm(ApplicationMainForm()); else F = new TWaitForm(nullptr);
    } else {
        F = new TWaitForm(AOwner);
    }

    F->FCaption = ACaption;
    F->setWindowTitle(ACaption);

    F->ui.ProgressBar->setMinimum(0);
    F->ui.ProgressBar->setValue(0);
    F->ui.ProgressBar->setMaximum(AMaxPos);

    // DoubleBuffered:=True; ProgressBar.DoubleBuffered:=True; - not needed in Qt
    return F;
}

void TWaitForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt
}

void TWaitForm::Step(int Pos)
{
    ui.ProgressBar->setValue(Pos);
    // Delphi would raise EDivByZero for Max=0
    const int Max = ui.ProgressBar->maximum();
    setWindowTitle(FCaption + " [" + IntToStr(Max != 0 ? 100 * Pos / Max : 0) + "%]");
    ui.ProgressBar->repaint();
    repaint();
    ProcessMessages();
}

void TWaitForm::StepIt()
{
    Step(ui.ProgressBar->value() + 1);
}

// { global }

TWaitForm* CreateWaitForm(QWidget* AOwner, const QString& ACaption, int AMaxPos)
{
    TWaitForm* result = TWaitForm::CreateWaitForm(AOwner, ACaption, AMaxPos);
    result->show();
    result->update();
    result->repaint();
    ProcessMessages();
    return result;
}

void InitProgressWindow(QWidget* AOwner, int Steps)
{
    if (WaitForm != nullptr) { delete WaitForm; WaitForm = nullptr; }
    WaitForm = CreateWaitForm(AOwner, LanguageSetup->ProgressFormCaption(), Steps);
    WaitForm->raise();
    ProcessMessages();
}

void StepProgressWindow()
{
    if (WaitForm != nullptr) WaitForm->StepIt();
}

void DoneProgressWindow()
{
    if (WaitForm != nullptr) { delete WaitForm; WaitForm = nullptr; }
}
