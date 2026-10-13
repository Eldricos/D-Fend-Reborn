// C++ port of TextEditPopupUnit.pas (implementation section).
#include "TextEditPopupUnit.h"

#include "LanguageSetupUnit.h"
#include "IconLoaderUnit.h"

#include <QClipboard>
#include <QMimeData>

static TImageList *PopupImageList = nullptr;

static const char *PopupMenuName = "RichEditPopupMenu";

static void PopupOpenWork(QMenu *P, QTextEdit *E)
{
    const QList<QAction *> Items = P->actions();
    if (Items.size() < 6) return;
    Items[0]->setEnabled(E->document()->isUndoAvailable());
    const QMimeData *Data = QApplication::clipboard()->mimeData();
    Items[3]->setEnabled(Data != nullptr && Data->hasText());
    Items[4]->setEnabled(E->textCursor().hasSelection());
}

static void PopupItemWork(int MenuIndex, QTextEdit *E)
{
    switch (MenuIndex) {
        case 0: E->undo(); break;
        case 1: E->cut(); break;
        case 2: E->copy(); break;
        case 3: E->paste(); break;
        case 4: E->textCursor().removeSelectedText(); break;
        case 5: E->selectAll(); break;
    }
}

static void AddItem(QMenu *PopupMenu, QTextEdit *Edit, const QString &Title, const QKeySequence &ShortCut, int ImageIndex)
{
    QAction *M = new QAction(Title, PopupMenu);
    M->setShortcut(ShortCut);   // only shown in the menu, the keys are handled by the edit itself
    M->setShortcutVisibleInContextMenu(true);
    if (PopupImageList != nullptr && ImageIndex >= 0 && ImageIndex < PopupImageList->Count()) M->setIcon(PopupImageList->Icon(ImageIndex));
    const int MenuIndex = PopupMenu->actions().size();
    QObject::connect(M, &QAction::triggered, Edit, [MenuIndex, Edit]() { PopupItemWork(MenuIndex, Edit); });
    PopupMenu->addAction(M);
}

void SetRichEditPopup(QTextEdit* Edit)
{
    if (PopupImageList == nullptr) PopupImageList = new TImageList();

    QMenu *PopupMenu = Edit->findChild<QMenu *>(QLatin1String(PopupMenuName), Qt::FindDirectChildrenOnly);
    if (PopupMenu != nullptr) {
        PopupMenu->clear();
    } else {
        PopupMenu = new QMenu(Edit);
        PopupMenu->setObjectName(QLatin1String(PopupMenuName));
        Edit->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(Edit, &QWidget::customContextMenuRequested, PopupMenu, [Edit, PopupMenu](const QPoint &Pos) {
            PopupOpenWork(PopupMenu, Edit);   // OnPopup
            PopupMenu->popup(Edit->viewport()->mapToGlobal(Pos));
        });
    }

    // PopupImageList.Assign(DFendRebornMainForm.GameNotesImageList)
    TImageList *GameNotesImageList = TImageList::FromResource(QStringLiteral("MainUnit"), QStringLiteral("GameNotesImageList"));
    PopupImageList->Assign(*GameNotesImageList);
    delete GameNotesImageList;

    if (UserIconLoader != nullptr) {
        UserIconLoader->DialogImage(DI_Undo, PopupImageList, 0);
        UserIconLoader->DialogImage(DI_CutToClipboard, PopupImageList, 1);
        UserIconLoader->DialogImage(DI_CopyToClipboard, PopupImageList, 2);
        UserIconLoader->DialogImage(DI_PasteFromClipboard, PopupImageList, 3);
    }

    AddItem(PopupMenu, Edit, LanguageSetup->Undo(), QKeySequence(Qt::CTRL | Qt::Key_Z), 0);
    AddItem(PopupMenu, Edit, LanguageSetup->Cut(), QKeySequence(Qt::CTRL | Qt::Key_X), 1);
    AddItem(PopupMenu, Edit, LanguageSetup->Copy(), QKeySequence(Qt::CTRL | Qt::Key_C), 2);
    AddItem(PopupMenu, Edit, LanguageSetup->Paste(), QKeySequence(Qt::CTRL | Qt::Key_V), 3);
    AddItem(PopupMenu, Edit, LanguageSetup->Del(), QKeySequence(Qt::CTRL | Qt::Key_Delete), 4);
    AddItem(PopupMenu, Edit, LanguageSetup->SelectAll(), QKeySequence(Qt::CTRL | Qt::Key_A), 5);
}

void TextEditPopupUnit_Initialization()
{
    PopupImageList = new TImageList();
}

void TextEditPopupUnit_Finalization()
{
    delete PopupImageList;
    PopupImageList = nullptr;
}
