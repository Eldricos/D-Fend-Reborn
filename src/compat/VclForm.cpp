#include "VclForm.h"

#include <QCloseEvent>
#include <QTimer>

TVclForm::TVclForm(QWidget *Parent) : QDialog(Parent)
{
}

void TVclForm::setModalResult(int MR)
{
    FModalResult = MR;
    if (MR == mrNone || FClosePending) return;
    FClosePending = true;
    QTimer::singleShot(0, this, [this]() {
        FClosePending = false;
        if (FModalResult != mrNone) done(FModalResult);
    });
}

int TVclForm::ShowModal()
{
    FModalResult = mrNone;
    int R = exec();
    if (R == QDialog::Rejected) R = (FModalResult != mrNone) ? FModalResult : mrCancel;
    FModalResult = R;
    return R;
}

void TVclForm::done(int R)
{
    if (!FQueryDone && !closeQuery()) {
        FModalResult = mrNone;
        return;
    }
    FModalResult = (R == QDialog::Rejected) ? mrCancel : R;
    QDialog::done(R);
}

void TVclForm::reject()
{
    // Esc / window close button
    done(mrCancel);
}

void TVclForm::closeEvent(QCloseEvent *E)
{
    if (!closeQuery()) {
        E->ignore();
        return;
    }
    FQueryDone = true;
    QDialog::closeEvent(E);
    FQueryDone = false;
}
