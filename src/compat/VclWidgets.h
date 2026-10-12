// Qt widgets emulating VCL controls that have no direct Qt equivalent.
// They are referenced as custom widgets by the generated .ui files.
#pragma once

#include <QGroupBox>
#include <QFrame>
#include <QComboBox>
#include <QTableWidget>
#include <QLabel>
#include <QStringList>
#include <QList>

class QRadioButton;
class QButtonGroup;

// TRadioGroup: group box containing a column layout of radio buttons.
class TRadioGroupWidget : public QGroupBox
{
    Q_OBJECT
    Q_PROPERTY(QStringList items READ items WRITE setItems)
    Q_PROPERTY(int columns READ columns WRITE setColumns)
    Q_PROPERTY(int itemIndex READ itemIndex WRITE setItemIndex)
public:
    explicit TRadioGroupWidget(QWidget *parent = nullptr);
    QStringList items() const { return FItems; }
    void setItems(const QStringList &Items);
    int columns() const { return FColumns; }
    void setColumns(int C);
    int itemIndex() const;
    void setItemIndex(int I);
    int count() const { return FItems.size(); }
    void addItem(const QString &S);
    void clear();
    void setItemText(int I, const QString &S);
    QRadioButton *button(int I) const;
    // Delphi style aliases
    int ItemIndex() const { return itemIndex(); }
    void SetItemIndex(int I) { setItemIndex(I); }
signals:
    void clicked();               // OnClick
    void itemIndexChanged(int);   // fired on every change
protected:
    void resizeEvent(QResizeEvent *e) override;
    void changeEvent(QEvent *e) override;
private:
    QStringList FItems;
    int FColumns = 1;
    QList<QRadioButton *> FButtons;
    QButtonGroup *FGroup;
    int FPendingIndex = -1;
    void rebuild();
    void arrange();
};

// TSplitter: resizes the adjacent aligned sibling (see VclLayout).
class TSplitterWidget : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(int minSize READ minSize WRITE setMinSize)
public:
    explicit TSplitterWidget(QWidget *parent = nullptr);
    int minSize() const { return FMinSize; }
    void setMinSize(int M) { FMinSize = M; }
signals:
    void moved();   // OnMoved
protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void showEvent(QShowEvent *e) override;
private:
    int FMinSize = 30;
    QWidget *FControl = nullptr;
    QPoint FStart;
    QSize FStartSize;
    QString align() const;
    QWidget *findControl() const;
};

// TColorBox: combo box listing the standard VCL colors.
class TColorBoxWidget : public QComboBox
{
    Q_OBJECT
public:
    explicit TColorBoxWidget(QWidget *parent = nullptr);
    QColor selected() const;
    void setSelected(const QColor &C);
    // Delphi TColor ($00BBGGRR) access
    int selectedTColor() const;
    void setSelectedTColor(int C);
private:
    void addColor(const QString &Name, const QColor &C);
};

// TValueListEditor: two column key/value grid.
class TValueListEditorWidget : public QTableWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList titleCaptions READ titleCaptions WRITE setTitleCaptions)
    Q_PROPERTY(QStringList strings READ strings WRITE setStrings)
    Q_PROPERTY(bool keyEdit READ keyEdit WRITE setKeyEdit)
public:
    explicit TValueListEditorWidget(QWidget *parent = nullptr);
    QStringList titleCaptions() const;
    void setTitleCaptions(const QStringList &L);
    QStringList strings() const;           // "key=value" lines
    void setStrings(const QStringList &L);
    bool keyEdit() const { return FKeyEdit; }
    void setKeyEdit(bool B);
    int rowCountValues() const { return rowCount(); }
    QString key(int Row) const;
    QString value(int Row) const;
    void setKey(int Row, const QString &S);
    void setValue(int Row, const QString &S);
    QString values(const QString &Key) const;
    void setValues(const QString &Key, const QString &Value);
    int insertRow(const QString &Key, const QString &Value, bool Append = true);
    void deleteRow(int Row);
    void setKeyReadOnly(int Row, bool ReadOnly);
    // ItemProps[].EditStyle=esEllipsis / esPickList
    void setPickList(int Row, const QStringList &Items, bool ReadOnlyList = false);
    void setEllipsis(int Row, bool On);
signals:
    void editButtonClicked(int Row);   // OnEditButtonClick
private:
    bool FKeyEdit = false;
    QStringList FTitles{"Key", "Value"};
    friend class ValueListDelegate;
};

// TImage: QLabel with Proportional/Stretch support.
class TImageWidget : public QLabel
{
    Q_OBJECT
    Q_PROPERTY(bool proportional READ proportional WRITE setProportional)
public:
    explicit TImageWidget(QWidget *parent = nullptr);
    bool proportional() const { return FProportional; }
    void setProportional(bool B) { FProportional = B; updatePixmap(); }
    void setPicture(const QPixmap &P);   // Picture.Assign / LoadFromFile
    QPixmap picture() const { return FPixmap; }
    void clearPicture();
signals:
    void clicked();
    void doubleClicked();
protected:
    void resizeEvent(QResizeEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
private:
    QPixmap FPixmap;
    bool FProportional = false;
    void updatePixmap();
};
