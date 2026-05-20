#ifndef AREASELECTOR_H
#define AREASELECTOR_H

#include <QWidget>
#include <QRect>
#include <QScreen>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QFont>

class AreaSelector : public QWidget
{
    Q_OBJECT

public:
    explicit AreaSelector(QWidget* parent = nullptr);
    ~AreaSelector() override;

    void showForScreen(QScreen* screen);
    QRect getSelectedRect() const;
    bool wasCancelled() const;

signals:
    void areaSelected(const QRect& rect);
    void selectionCancelled();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class SelectionState {
        Idle,
        Selecting,
        Selected
    };

    void initUI();
    void drawMask(QPainter& painter);
    void drawSelection(QPainter& painter);
    void drawSizeLabel(QPainter& painter);
    QRect calculateVirtualGeometry(QScreen* screen) const;
    QPoint mapFromGlobalToWidget(const QPoint& globalPos) const;
    QPoint mapFromWidgetToGlobal(const QPoint& widgetPos) const;

    SelectionState m_state = SelectionState::Idle;
    QPoint m_startPoint;
    QPoint m_endPoint;
    QRect m_selectedRect;
    
    QColor m_maskColor = QColor(0, 0, 0, 120);
    QColor m_borderColor = QColor(0, 120, 215);
    QColor m_labelBgColor = QColor(0, 120, 215, 200);
    QColor m_labelTextColor = Qt::white;
    
    int m_borderWidth = 2;
    int m_labelPadding = 8;
    int m_labelMargin = 10;
    
    bool m_cancelled = false;
    QScreen* m_targetScreen = nullptr;
    QRect m_screenGeometry;
};

#endif // AREASELECTOR_H
