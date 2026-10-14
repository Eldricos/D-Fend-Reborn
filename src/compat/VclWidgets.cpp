#include "VclWidgets.h"
#include "VclLayout.h"
#include "Graphics.h"

#include <QRadioButton>
#include <QButtonGroup>
#include <QMouseEvent>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QToolButton>
#include <QHBoxLayout>
#include <QRegularExpressionValidator>
#include <QComboBox>
#include <QPainter>

// ---------------------------------------------------------------------------
// TRadioGroupWidget
// ---------------------------------------------------------------------------

TRadioGroupWidget::TRadioGroupWidget(QWidget *parent) : QGroupBox(parent)
{
    FGroup = new QButtonGroup(this);
    FGroup->setExclusive(true);
    connect(FGroup, &QButtonGroup::idClicked, this, [this](int) { emit clicked(); });
    connect(FGroup, &QButtonGroup::idToggled, this, [this](int id, bool on) { if (on) emit itemIndexChanged(id); });
}

void TRadioGroupWidget::setItems(const QStringList &Items)
{
    int Old = itemIndex();
    FItems = Items;
    rebuild();
    if (FPendingIndex >= 0) { setItemIndex(FPendingIndex); FPendingIndex = -1; }
    else if (Old >= 0 && Old < FItems.size()) setItemIndex(Old);
}

void TRadioGroupWidget::setColumns(int C)
{
    FColumns = qMax(1, C);
    arrange();
}

int TRadioGroupWidget::itemIndex() const
{
    return FGroup->checkedId();
}

void TRadioGroupWidget::setItemIndex(int I)
{
    if (I < 0 || I >= FButtons.size()) {
        if (FButtons.isEmpty() && I >= 0) { FPendingIndex = I; return; }
        // VCL: ItemIndex:=-1 unchecks all buttons
        FGroup->setExclusive(false);
        for (QRadioButton *B : FButtons) B->setChecked(false);
        FGroup->setExclusive(true);
        return;
    }
    FButtons[I]->setChecked(true);
}

void TRadioGroupWidget::addItem(const QString &S)
{
    int Old = itemIndex();
    FItems << S;
    rebuild();
    if (Old >= 0) setItemIndex(Old);
}

void TRadioGroupWidget::clear()
{
    FItems.clear();
    rebuild();
}

void TRadioGroupWidget::setItemText(int I, const QString &S)
{
    if (I < 0 || I >= FItems.size()) return;
    FItems[I] = S;
    FButtons[I]->setText(S);
}

QRadioButton *TRadioGroupWidget::button(int I) const
{
    return (I >= 0 && I < FButtons.size()) ? FButtons[I] : nullptr;
}

void TRadioGroupWidget::rebuild()
{
    for (QRadioButton *B : FButtons) { FGroup->removeButton(B); delete B; }
    FButtons.clear();
    for (int i = 0; i < FItems.size(); i++) {
        auto *B = new QRadioButton(FItems[i], this);
        B->setObjectName(QString("%1_Item%2").arg(objectName()).arg(i));
        FGroup->addButton(B, i);
        FButtons << B;
        B->show();
    }
    arrange();
}

void TRadioGroupWidget::arrange()
{
    // VCL TRadioGroup.ArrangeButtons
    if (FButtons.isEmpty()) return;
    QFontMetrics FM(font());
    int Top = FM.height() + 2;
    int Rows = (FButtons.size() + FColumns - 1) / FColumns;
    int W = width() - 10;
    int H = height() - Top - 4;
    int ColW = W / FColumns;
    int RowH = Rows > 0 ? H / Rows : H;
    int BH = qMin(RowH, FM.height() + 6);
    for (int i = 0; i < FButtons.size(); i++) {
        int Col = i / Rows;
        int Row = i % Rows;
        int Y = Top + Row * RowH + (RowH - BH) / 2;
        FButtons[i]->setGeometry(8 + Col * ColW, Y, ColW - 4, BH);
    }
}

void TRadioGroupWidget::resizeEvent(QResizeEvent *e)
{
    QGroupBox::resizeEvent(e);
    arrange();
}

void TRadioGroupWidget::changeEvent(QEvent *e)
{
    QGroupBox::changeEvent(e);
    if (e->type() == QEvent::FontChange) arrange();
}

// ---------------------------------------------------------------------------
// TSplitterWidget
// ---------------------------------------------------------------------------

TSplitterWidget::TSplitterWidget(QWidget *parent) : QFrame(parent)
{
    setFrameShape(QFrame::NoFrame);
}

QString TSplitterWidget::align() const
{
    QString A = property("vclAlign").toString();
    return A.isEmpty() ? QString("alLeft") : A;
}

void TSplitterWidget::showEvent(QShowEvent *e)
{
    QFrame::showEvent(e);
    QString A = align();
    setCursor((A == "alLeft" || A == "alRight") ? Qt::SplitHCursor : Qt::SplitVCursor);
}

QWidget *TSplitterWidget::findControl() const
{
    QWidget *P = parentWidget();
    if (!P) return nullptr;
    QString A = align();
    QRect G = geometry();
    QWidget *Best = nullptr;
    int BestDist = INT_MAX;
    for (QObject *O : P->children()) {
        auto *W = qobject_cast<QWidget *>(O);
        if (!W || W == this || W->isHidden()) continue;
        if (W->property("vclAlign").toString() != A) continue;
        QRect R = W->geometry();
        int D = INT_MAX;
        if (A == "alLeft") D = qAbs(R.right() + 1 - G.left());
        else if (A == "alRight") D = qAbs(R.left() - (G.right() + 1));
        else if (A == "alTop") D = qAbs(R.bottom() + 1 - G.top());
        else if (A == "alBottom") D = qAbs(R.top() - (G.bottom() + 1));
        if (D < BestDist) { BestDist = D; Best = W; }
    }
    return BestDist <= 2 ? Best : nullptr;
}

void TSplitterWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    FControl = findControl();
    FStart = e->globalPosition().toPoint();
    if (FControl) FStartSize = FControl->size();
}

void TSplitterWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (!FControl || !(e->buttons() & Qt::LeftButton)) return;
    QPoint D = e->globalPosition().toPoint() - FStart;
    QString A = align();
    QWidget *P = parentWidget();
    int Max;
    QSize S = FStartSize;
    if (A == "alLeft") { Max = P->width() - FMinSize - width(); S.setWidth(qBound(FMinSize, FStartSize.width() + D.x(), qMax(FMinSize, Max))); }
    else if (A == "alRight") { Max = P->width() - FMinSize - width(); S.setWidth(qBound(FMinSize, FStartSize.width() - D.x(), qMax(FMinSize, Max))); }
    else if (A == "alTop") { Max = P->height() - FMinSize - height(); S.setHeight(qBound(FMinSize, FStartSize.height() + D.y(), qMax(FMinSize, Max))); }
    else if (A == "alBottom") { Max = P->height() - FMinSize - height(); S.setHeight(qBound(FMinSize, FStartSize.height() - D.y(), qMax(FMinSize, Max))); }
    VclLayout::setAlignedExtent(FControl, (A == "alLeft" || A == "alRight") ? S.width() : S.height());
}

void TSplitterWidget::mouseReleaseEvent(QMouseEvent *e)
{
    Q_UNUSED(e);
    if (FControl) emit moved();
    FControl = nullptr;
}

// ---------------------------------------------------------------------------
// TColorBoxWidget
// ---------------------------------------------------------------------------

namespace {
// Colours of TColorBox (style cbStandardColors, cbExtendedColors,
// cbSystemColors) in the order of the VCL
const int ColorBoxColors[] = {
    clBlack, clMaroon, clGreen, clOlive, clNavy, clPurple, clTeal, clGray,
    clSilver, clRed, clLime, clYellow, clBlue, clFuchsia, clAqua, clWhite,
    clMoneyGreen, clSkyBlue, clCream, clMedGray,
    clActiveBorder, clActiveCaption, clAppWorkSpace, clBackground, clBtnFace, clBtnHighlight,
    clBtnShadow, clBtnText, clCaptionText, clGrayText, clHighlight, clHighlightText,
    clHotLight, clInactiveBorder, clInactiveCaption, clInactiveCaptionText, clInfoBk, clInfoText,
    clMenu, clMenuText, clScrollBar, cl3DDkShadow, cl3DLight, clWindow,
    clWindowFrame, clWindowText,
};

// TCustomColorBox.ColorCallBack with cbPrettyNames: the colour identifier
// without "cl" and with a space before each capital letter ("clMoneyGreen"
// -> "Money Green", "cl3DDkShadow" -> "3 D Dk Shadow"). The names are the
// same in all Delphi language versions.
QString PrettyColorName(const QString& Ident)
{
    const int Start = Ident.startsWith(QLatin1String("cl")) ? 2 : 0;
    QString Name;
    for (int I = Start; I < Ident.size(); I++) {
        const QChar C = Ident[I];
        if (C >= QLatin1Char('A') && C <= QLatin1Char('Z') && !Name.isEmpty()) Name += QLatin1Char(' ');
        Name += C;
    }
    return Name;
}
const int RoleTColor = Qt::UserRole;
} // namespace

TColorBoxWidget::TColorBoxWidget(QWidget *parent) : QComboBox(parent)
{
    setEditable(false);
    for (const int C : ColorBoxColors) addColor(C);
}

QString TColorBoxWidget::colorName(int C) const
{
    const QString Ident = ColorToString(C);   // "clBlack", "clWindow" or "$00RRGGBB" (custom colour)
    if (FPrettyNames && Ident.startsWith(QLatin1String("cl"))) return PrettyColorName(Ident);
    return Ident;
}

void TColorBoxWidget::addColor(int C)
{
    QPixmap P(16, 12);
    P.fill(TColorToQColor(C));
    QPainter Pa(&P);
    Pa.setPen(Qt::black);
    Pa.drawRect(0, 0, 15, 11);
    Pa.end();
    addItem(QIcon(P), colorName(C), C);
}

void TColorBoxWidget::updateNames()
{
    for (int I = 0; I < count(); I++) setItemText(I, colorName(itemData(I, RoleTColor).toInt()));
}

void TColorBoxWidget::setPrettyNames(bool Pretty)
{
    if (FPrettyNames == Pretty) return;
    FPrettyNames = Pretty;
    updateNames();
}

QColor TColorBoxWidget::selected() const
{
    return TColorToQColor(selectedTColor());
}

void TColorBoxWidget::setSelected(const QColor &C)
{
    setSelectedTColor(QColorToTColor(C));
}

int TColorBoxWidget::selectedTColor() const
{
    if (currentIndex() < 0) return clBlack;
    return itemData(currentIndex(), RoleTColor).toInt();
}

void TColorBoxWidget::setSelectedTColor(int C)
{
    for (int I = 0; I < count(); I++) if (itemData(I, RoleTColor).toInt() == C) { setCurrentIndex(I); return; }
    // a colour that is not in the list (TColorBox adds it as a custom colour)
    addColor(C);
    setCurrentIndex(count() - 1);
}

// ---------------------------------------------------------------------------
// TValueListEditorWidget
// ---------------------------------------------------------------------------

namespace {
const int RolePickList = Qt::UserRole + 1;
const int RolePickListReadOnly = Qt::UserRole + 2;
const int RoleEllipsis = Qt::UserRole + 3;
const int RoleReadOnly = Qt::UserRole + 4;
const int RoleEditMask = Qt::UserRole + 5;

// Delphi edit mask -> regular expression. The masks used by the program
// ('90', '900', '9000', '90000') stand for "a number with up to N digits":
// Delphi masks are positional and unfilled positions end up as trimmed
// blanks, so shorter values are accepted (e.g. the default cycles step 10 in
// a '90000' field). Each mask position therefore is optional; the value must
// not be empty. 0/9 = digit, L/l = letter, A/a = letter or digit, C/c = any
// character, # = digit or sign; other characters are literals. Only the mask
// part before the first ';' is used.
QRegularExpression EditMaskToRegex(const QString &Mask)
{
    QString M = Mask.section(';', 0, 0), R;
    bool Literal = false;
    for (QChar C : M) {
        if (Literal) { R += QRegularExpression::escape(QString(C)) + "?"; Literal = false; continue; }
        switch (C.unicode()) {
            case '\\': Literal = true; break;
            case '0': case '9': R += "[0-9]?"; break;
            case 'L': case 'l': R += "[A-Za-z]?"; break;
            case 'A': case 'a': R += "[A-Za-z0-9]?"; break;
            case 'C': case 'c': R += ".?"; break;
            case '#': R += "[0-9+\\- ]?"; break;
            case '>': case '<': case '!': break;   // case conversion / alignment
            default: R += QRegularExpression::escape(QString(C)) + "?"; break;
        }
    }
    return QRegularExpression("^(?=.)" + R + "$");
}
}

class ValueListDelegate : public QStyledItemDelegate
{
public:
    explicit ValueListDelegate(TValueListEditorWidget *Owner) : QStyledItemDelegate(Owner), FOwner(Owner) {}
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStringList Pick = index.data(RolePickList).toStringList();
        const bool ReadOnly = index.data(RoleReadOnly).toBool();
        const QString Mask = index.data(RoleEditMask).toString();
        if (index.column() == 1 && !Pick.isEmpty()) {
            auto *C = new QComboBox(parent);
            C->addItems(Pick);
            C->setEditable(!ReadOnly && !index.data(RolePickListReadOnly).toBool());
            if (C->isEditable() && !Mask.isEmpty()) C->setValidator(new QRegularExpressionValidator(EditMaskToRegex(Mask), C));
            return C;
        }
        if (index.column() == 1 && index.data(RoleEllipsis).toBool()) {
            auto *W = new QWidget(parent);
            auto *L = new QHBoxLayout(W);
            L->setContentsMargins(0, 0, 0, 0);
            L->setSpacing(0);
            auto *E = new QLineEdit(W);
            E->setObjectName("edit");
            E->setReadOnly(ReadOnly);
            if (!Mask.isEmpty()) E->setValidator(new QRegularExpressionValidator(EditMaskToRegex(Mask), E));
            auto *B = new QToolButton(W);
            B->setText("...");
            L->addWidget(E);
            L->addWidget(B);
            W->setFocusProxy(E);
            int Row = index.row();
            TValueListEditorWidget *O = FOwner;
            QObject::connect(B, &QToolButton::clicked, O, [O, Row]() { emit O->editButtonClicked(Row); });
            return W;
        }
        QWidget *Ed = QStyledItemDelegate::createEditor(parent, option, index);
        if (index.column() == 1 && !Mask.isEmpty())
            if (auto *E = qobject_cast<QLineEdit *>(Ed)) E->setValidator(new QRegularExpressionValidator(EditMaskToRegex(Mask), E));
        return Ed;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *C = qobject_cast<QComboBox *>(editor)) { C->setCurrentText(index.data().toString()); return; }
        if (auto *E = editor->findChild<QLineEdit *>("edit")) { E->setText(index.data().toString()); return; }
        QStyledItemDelegate::setEditorData(editor, index);
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        // values not matching the edit mask are rejected (the old value stays)
        const QString Mask = index.data(RoleEditMask).toString();
        auto Accept = [&Mask](const QString &S) { return Mask.isEmpty() || EditMaskToRegex(Mask).match(S).hasMatch(); };
        if (auto *C = qobject_cast<QComboBox *>(editor)) { if (Accept(C->currentText())) model->setData(index, C->currentText()); return; }
        if (auto *E = editor->findChild<QLineEdit *>("edit")) { if (E->isReadOnly() || Accept(E->text())) model->setData(index, E->text()); return; }
        if (auto *E = qobject_cast<QLineEdit *>(editor)) { if (Accept(E->text())) model->setData(index, E->text()); return; }
        QStyledItemDelegate::setModelData(editor, model, index);
    }
private:
    TValueListEditorWidget *FOwner;
};

TValueListEditorWidget::TValueListEditorWidget(QWidget *parent) : QTableWidget(parent)
{
    setColumnCount(2);
    setHorizontalHeaderLabels(FTitles);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(18);
    horizontalHeader()->setStretchLastSection(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::AllEditTriggers);
    setItemDelegate(new ValueListDelegate(this));
}

QStringList TValueListEditorWidget::titleCaptions() const { return FTitles; }

void TValueListEditorWidget::setTitleCaptions(const QStringList &L)
{
    FTitles = L;
    while (FTitles.size() < 2) FTitles << QString();
    setHorizontalHeaderLabels(FTitles);
}

QStringList TValueListEditorWidget::strings() const
{
    QStringList R;
    for (int i = 0; i < rowCount(); i++) R << key(i) + "=" + value(i);
    return R;
}

void TValueListEditorWidget::setStrings(const QStringList &L)
{
    setRowCount(0);
    for (const QString &S : L) {
        int P = S.indexOf('=');
        insertRow(P >= 0 ? S.left(P) : S, P >= 0 ? S.mid(P + 1) : QString());
    }
}

void TValueListEditorWidget::setKeyEdit(bool B)
{
    FKeyEdit = B;
    for (int i = 0; i < rowCount(); i++) setKeyReadOnly(i, !B);
}

QString TValueListEditorWidget::key(int Row) const
{
    QTableWidgetItem *I = item(Row, 0);
    return I ? I->text() : QString();
}

QString TValueListEditorWidget::value(int Row) const
{
    QTableWidgetItem *I = item(Row, 1);
    return I ? I->text() : QString();
}

void TValueListEditorWidget::setKey(int Row, const QString &S)
{
    if (!item(Row, 0)) setItem(Row, 0, new QTableWidgetItem());
    item(Row, 0)->setText(S);
}

void TValueListEditorWidget::setValue(int Row, const QString &S)
{
    if (!item(Row, 1)) setItem(Row, 1, new QTableWidgetItem());
    item(Row, 1)->setText(S);
}

QString TValueListEditorWidget::values(const QString &Key) const
{
    for (int i = 0; i < rowCount(); i++)
        if (QString::compare(key(i), Key, Qt::CaseInsensitive) == 0) return value(i);
    return QString();
}

void TValueListEditorWidget::setValues(const QString &Key, const QString &Value)
{
    for (int i = 0; i < rowCount(); i++)
        if (QString::compare(key(i), Key, Qt::CaseInsensitive) == 0) { setValue(i, Value); return; }
    insertRow(Key, Value);
}

int TValueListEditorWidget::insertRow(const QString &Key, const QString &Value, bool Append)
{
    int Row = Append ? rowCount() : qMax(0, currentRow());
    QTableWidget::insertRow(Row);
    setKey(Row, Key);
    setValue(Row, Value);
    setKeyReadOnly(Row, !FKeyEdit);
    return Row;
}

void TValueListEditorWidget::deleteRow(int Row)
{
    removeRow(Row);
}

void TValueListEditorWidget::setKeyReadOnly(int Row, bool ReadOnly)
{
    if (!item(Row, 0)) setItem(Row, 0, new QTableWidgetItem());
    QTableWidgetItem *I = item(Row, 0);
    I->setFlags(ReadOnly ? (I->flags() & ~Qt::ItemIsEditable) : (I->flags() | Qt::ItemIsEditable));
    if (ReadOnly) I->setBackground(palette().button());
    else I->setBackground(QBrush());
}

QTableWidgetItem *TValueListEditorWidget::valueItem(int Row)
{
    if (!item(Row, 1)) setItem(Row, 1, new QTableWidgetItem());
    return item(Row, 1);
}

// A read only value cell can only be edited (= the editor opened) if it has a
// pick list or an ellipsis button.
void TValueListEditorWidget::updateValueFlags(int Row)
{
    QTableWidgetItem *I = valueItem(Row);
    const bool Editor = !I->data(RoleReadOnly).toBool() || !I->data(RolePickList).toStringList().isEmpty() || I->data(RoleEllipsis).toBool();
    I->setFlags(Editor ? (I->flags() | Qt::ItemIsEditable) : (I->flags() & ~Qt::ItemIsEditable));
}

void TValueListEditorWidget::setPickList(int Row, const QStringList &Items, bool ReadOnlyList)
{
    if (Row < 0 || Row >= rowCount()) return;
    valueItem(Row)->setData(RolePickList, Items);
    valueItem(Row)->setData(RolePickListReadOnly, ReadOnlyList);
    updateValueFlags(Row);
}

QStringList TValueListEditorWidget::pickList(int Row) const
{
    QTableWidgetItem *I = item(Row, 1);
    return I ? I->data(RolePickList).toStringList() : QStringList();
}

void TValueListEditorWidget::setEllipsis(int Row, bool On)
{
    if (Row < 0 || Row >= rowCount()) return;
    valueItem(Row)->setData(RoleEllipsis, On);
    updateValueFlags(Row);
}

void TValueListEditorWidget::setReadOnly(int Row, bool ReadOnly)
{
    if (Row < 0 || Row >= rowCount()) return;
    valueItem(Row)->setData(RoleReadOnly, ReadOnly);
    updateValueFlags(Row);
}

bool TValueListEditorWidget::isReadOnly(int Row) const
{
    QTableWidgetItem *I = item(Row, 1);
    return I && I->data(RoleReadOnly).toBool();
}

void TValueListEditorWidget::setEditMask(int Row, const QString &Mask)
{
    if (Row < 0 || Row >= rowCount()) return;
    valueItem(Row)->setData(RoleEditMask, Mask);
}

// ---------------------------------------------------------------------------
// TImageWidget
// ---------------------------------------------------------------------------

TImageWidget::TImageWidget(QWidget *parent) : QLabel(parent)
{
}

void TImageWidget::setPicture(const QPixmap &P)
{
    FPixmap = P;
    updatePixmap();
}

void TImageWidget::clearPicture()
{
    FPixmap = QPixmap();
    QLabel::clear();
}

void TImageWidget::updatePixmap()
{
    if (FPixmap.isNull()) {
        if (!pixmap().isNull() && FPixmap.isNull()) FPixmap = pixmap();
        if (FPixmap.isNull()) return;
    }
    if (FProportional) {
        setScaledContents(false);
        QSize S = FPixmap.size();
        if (S.width() > width() || S.height() > height() || hasScaledContents())
            S.scale(size(), Qt::KeepAspectRatio);
        QLabel::setPixmap(FPixmap.scaled(S, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        QLabel::setPixmap(FPixmap);
    }
}

void TImageWidget::resizeEvent(QResizeEvent *e)
{
    QLabel::resizeEvent(e);
    if (FProportional) updatePixmap();
}

void TImageWidget::mouseReleaseEvent(QMouseEvent *e)
{
    QLabel::mouseReleaseEvent(e);
    if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint())) emit clicked();
}

void TImageWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
    QLabel::mouseDoubleClickEvent(e);
    emit doubleClicked();
}
