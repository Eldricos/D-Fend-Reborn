#include "MissingGamesFormUnit.h"

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include "CommonTools.h"
#include "Dialogs.h"
#include "GameDBUnit.h"
#include "GameLocationUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"

namespace {

class TMissingGamesForm : public QDialog
{
public:
    TMissingGamesForm(QWidget* AOwner, TGameDB* AGameDB) : QDialog(AOwner), GameDB(AGameDB)
    {
        setObjectName(QStringLiteral("MissingGamesForm"));
        setWindowTitle(LanguageSetup->MissingGamesCaption());
        setProperty("HelpContext", ID_ExtrasMissingGames);
        auto* Main = new QVBoxLayout(this);
        auto* Info = new QLabel(LanguageSetup->MissingGamesInfo(), this);
        Info->setWordWrap(true);
        Main->addWidget(Info);
        List = new QTreeWidget(this);
        List->setObjectName(QStringLiteral("MissingGamesList"));
        List->setColumnCount(2);
        List->setHeaderLabels({LanguageSetup->MissingGamesProfile(), LanguageSetup->MissingGamesMissing()});
        List->setRootIsDecorated(false);
        List->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        Main->addWidget(List, 1);

        auto* Buttons = new QHBoxLayout;
        auto* SearchButton = new QPushButton(LanguageSetup->MissingGamesSearch(), this);
        auto* DeleteButton = new QPushButton(LanguageSetup->MissingGamesDelete(), this);
        auto* HelpButton = new QPushButton(LanguageSetup->Help(), this);
        auto* CloseButton = new QPushButton(LanguageSetup->Close(), this);
        CloseButton->setDefault(true);
        Buttons->addWidget(SearchButton);
        Buttons->addWidget(DeleteButton);
        Buttons->addStretch(1);
        Buttons->addWidget(HelpButton);
        Buttons->addWidget(CloseButton);
        Main->addLayout(Buttons);
        connect(SearchButton, &QPushButton::clicked, this, [this]() { SearchClick(); });
        connect(DeleteButton, &QPushButton::clicked, this, [this]() { DeleteClick(); });
        connect(HelpButton, &QPushButton::clicked, this, []() { ApplicationHelpCommand(HELP_CONTEXT, ID_ExtrasMissingGames); });
        connect(CloseButton, &QPushButton::clicked, this, &QDialog::accept);
        resize(760, 420);
        LoadList();
    }

    int Count() const { return List->topLevelItemCount(); }
    bool Changed = false;

private:
    TGameDB* GameDB;
    QTreeWidget* List;

    void LoadList()
    {
        List->clear();
        for (int I = 0; I < GameDB->Count(); I++) {
            TGame* G = GameDB->Game(I);
            const QStringList Missing = MissingGamePaths(G);
            if (Missing.isEmpty()) continue;
            auto* Item = new QTreeWidgetItem(List, {G->Name(), QDir::toNativeSeparators(Missing[0])});
            Item->setData(0, Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(G)));
            Item->setFlags(Item->flags() | Qt::ItemIsUserCheckable);
            Item->setCheckState(0, Qt::Checked);
            Item->setToolTip(1, QDir::toNativeSeparators(Missing.join(QLatin1Char('\n'))));
        }
        List->sortItems(0, Qt::AscendingOrder);
    }

    QList<TGame*> CheckedGames() const
    {
        QList<TGame*> R;
        for (int I = 0; I < List->topLevelItemCount(); I++) {
            QTreeWidgetItem* Item = List->topLevelItem(I);
            if (Item->checkState(0) == Qt::Checked) R << reinterpret_cast<TGame*>(Item->data(0, Qt::UserRole).value<quintptr>());
        }
        return R;
    }

    void SearchClick()
    {
        const QList<TGame*> Games = CheckedGames();
        if (Games.isEmpty()) return;
        QString S = PrgSetup->BaseDir();
        if (!SelectDirectory(THandle(0), LanguageSetup->MissingGamesSearchCaption(), S)) return;
        QStringList Found, NotFound;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        for (TGame* G : Games) {
            if (RelocateGame(G, S)) Found << G->Name(); else NotFound << G->Name();
        }
        QApplication::restoreOverrideCursor();
        if (!Found.isEmpty()) Changed = true;
        TStringList Text;
        if (!Found.isEmpty()) { Text.Add(LanguageSetup->MissingGamesFound()); for (const QString& N : Found) Text.Add("  " + N); }
        if (!NotFound.isEmpty()) {
            if (Text.Count() > 0) Text.Add("");
            Text.Add(LanguageSetup->MissingGamesNotFound());
            for (const QString& N : NotFound) Text.Add("  " + N);
        }
        MessageDlg(Text.ToQStringList().join(QLatin1Char('\n')), mtInformation, mbOK, 0, this);
        LoadList();
    }

    void DeleteClick()
    {
        const QList<TGame*> Games = CheckedGames();
        if (Games.isEmpty()) return;
        if (MessageDlg(Format(LanguageSetup->MissingGamesDeleteConfirm(), {IntToStr(Games.size())}), mtConfirmation, mbYes | mbNo, 0, this) != mrYes) return;
        for (TGame* G : Games) GameDB->Delete(G);
        Changed = true;
        LoadList();
    }
};

}  // namespace

bool ShowMissingGamesDialog(QWidget* AOwner, TGameDB* GameDB)
{
    TMissingGamesForm F(AOwner, GameDB);
    if (F.Count() == 0) {
        MessageDlg(LanguageSetup->MissingGamesNone(), mtInformation, mbOK, 0, AOwner);
        return false;
    }
    F.exec();
    return F.Changed;
}
