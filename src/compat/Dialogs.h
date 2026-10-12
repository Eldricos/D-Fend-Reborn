// Delphi Dialogs / Controls compatibility: modal results, MessageDlg,
// ShowMessage, InputQuery, SelectDirectory, file dialogs.
#pragma once

#include "Classes.h"

class QWidget;

// TModalResult values (Controls.pas)
enum TModalResult : int {
    mrNone = 0, mrOk = 1, mrCancel = 2, mrAbort = 3, mrRetry = 4, mrIgnore = 5,
    mrYes = 6, mrNo = 7, mrAll = 8, mrNoToAll = 9, mrYesToAll = 10, mrClose = 11
};

enum TMsgDlgType { mtWarning, mtError, mtInformation, mtConfirmation, mtCustom };

// TMsgDlgButtons is a set in Delphi; here a bit mask.
enum TMsgDlgBtn : int {
    mbYes = 0x0001, mbNo = 0x0002, mbOK = 0x0004, mbCancel = 0x0008, mbAbort = 0x0010,
    mbRetry = 0x0020, mbIgnore = 0x0040, mbAll = 0x0080, mbNoToAll = 0x0100,
    mbYesToAll = 0x0200, mbHelp = 0x0400, mbClose = 0x0800
};
using TMsgDlgButtons = int;
constexpr TMsgDlgButtons mbYesNoCancel = mbYes | mbNo | mbCancel;
constexpr TMsgDlgButtons mbYesNo = mbYes | mbNo;
constexpr TMsgDlgButtons mbOKCancel = mbOK | mbCancel;
constexpr TMsgDlgButtons mbAbortRetryIgnore = mbAbort | mbRetry | mbIgnore;

// Captions used by MessageDlg (the original program translated the VCL
// resource strings through ResHookUnit; LanguageSetupUnit fills these).
struct TMsgDlgCaptions {
    QString Warning = "Warning", Error = "Error", Information = "Information", Confirmation = "Confirm";
    QString Yes = "&Yes", No = "&No", OK = "OK", Cancel = "Cancel", Abort = "&Abort", Retry = "&Retry",
            Ignore = "&Ignore", All = "&All", NoToAll = "N&o to All", YesToAll = "Yes to &All",
            Help = "&Help", Close = "&Close";
};
extern TMsgDlgCaptions MsgDlgCaptions;

// Parent window for message boxes (defaults to the active window)
int MessageDlg(const QString &Msg, TMsgDlgType DlgType, TMsgDlgButtons Buttons, int HelpCtx = 0, QWidget *Parent = nullptr);
int MessageDlgCaption(const QString &Caption, const QString &Msg, TMsgDlgType DlgType, TMsgDlgButtons Buttons, QWidget *Parent = nullptr);
void ShowMessage(const QString &Msg, QWidget *Parent = nullptr);
bool InputQuery(const QString &Caption, const QString &Prompt, QString &Value, QWidget *Parent = nullptr);
QString InputBox(const QString &Caption, const QString &Prompt, const QString &Default, QWidget *Parent = nullptr);
bool SelectDirectory(const QString &Caption, const QString &Root, QString &Directory, QWidget *Parent = nullptr);

// Converts a Delphi dialog filter ("Text files|*.txt|All files|*.*") into a
// Qt name filter list ("Text files (*.txt);;All files (*)").
QString DelphiFilterToQt(const QString &Filter);

// TOpenDialog / TSaveDialog replacement with Delphi like properties.
class TOpenDialog
{
public:
    explicit TOpenDialog(QWidget *Parent = nullptr) : FParent(Parent) {}
    QString Title, Filter, DefaultExt, InitialDir, FileName;
    int FilterIndex = 1;
    bool AllowMultiSelect = false;
    bool FileMustExist = true;
    TStringList Files;
    bool Execute();
protected:
    QWidget *FParent;
};

class TSaveDialog : public TOpenDialog
{
public:
    explicit TSaveDialog(QWidget *Parent = nullptr) : TOpenDialog(Parent) { FileMustExist = false; }
    bool OverwritePrompt = true;
    bool Execute();
};
