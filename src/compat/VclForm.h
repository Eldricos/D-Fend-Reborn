// Base class for all ported VCL forms (TForm descendants).
//
// Provides Delphi's ModalResult semantics:
//   * setModalResult(mrOk) closes a modal form after the current event handler
//     returned; setting mrNone again inside the handler keeps it open.
//   * ShowModal() returns the TModalResult (closing the window = mrCancel).
//   * closeQuery() is called before the form closes (OnCloseQuery).
#pragma once

#include <QDialog>
#include "Dialogs.h"

class TVclForm : public QDialog
{
    Q_OBJECT
public:
    explicit TVclForm(QWidget *Parent = nullptr);

    int ModalResult() const { return FModalResult; }
    void setModalResult(int MR);
    int ShowModal();
    void Close() { close(); }

    // Tag property of the form
    int Tag() const { return FTag; }
    void setTag(int T) { FTag = T; }

protected:
    // OnCloseQuery: return false to keep the form open
    virtual bool closeQuery() { return true; }
    // OnShow equivalent that is called once before the first show
    void done(int R) override;
    void closeEvent(QCloseEvent *E) override;
    void reject() override;

private:
    int FModalResult = mrNone;
    bool FClosePending = false;
    int FTag = 0;
    bool FQueryDone = false;
};
