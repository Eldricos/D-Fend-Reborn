// C++ port of SmallWaitFormUnit.pas (implementation section).
#include "SmallWaitFormUnit.h"
#include "CommonTools.h"
#include "LanguageSetupUnit.h"

TSmallWaitForm* SmallWaitForm = nullptr;

// Application.MainForm
static QWidget* ApplicationMainForm()
{
    const QWidgetList L = QApplication::topLevelWidgets();
    for (QWidget* W : L) if (qobject_cast<QMainWindow*>(W) != nullptr) return W;
    return nullptr;
}

TSmallWaitForm::TSmallWaitForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    FormCreate(this);   // OnCreate
}

TSmallWaitForm::~TSmallWaitForm() = default;

void TSmallWaitForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // DoubleBuffered:=True; SetVistaFonts(self); Font.Charset:=... - not needed in Qt
}

void LoadAndShowSmallWaitForm(const QString& AInfo)
{
    QString Info = AInfo;
    if (ApplicationMainForm() != nullptr && ApplicationMainForm()->isMinimized()) return;
    SmallWaitForm = new TSmallWaitForm(ApplicationMainForm());
    if (Trim(Info).isEmpty()) Info = LanguageSetup->ProgressFormCaption();
    SmallWaitForm->ui.Label1->setText(Info);
    SmallWaitForm->show();
    SmallWaitForm->raise();
    ProcessMessages();
    SmallWaitForm->repaint();
}

void FreeSmallWaitForm()
{
    if (SmallWaitForm != nullptr) { delete SmallWaitForm; SmallWaitForm = nullptr; }
}
