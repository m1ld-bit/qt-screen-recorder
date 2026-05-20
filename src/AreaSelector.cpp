#include "AreaSelector.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QCursor>
#include <QDebug>

AreaSelector::AreaSelector(QWidget* parent)
    : QWidget(parent)
    , m_state(SelectionState::Idle)
    , m_cancelled(false)
    , m_targetScreen(nullptr)
{
    initUI();
}

AreaSelector::~AreaSelector()
{
}

void AreaSelector::initUI()
{
    setWindowFlags(Qt::FramelessWindowHint | 
                   Qt::Tool | 
                   Qt::WindowStaysOnTopHint | 
                   Qt::BypassWindowManagerHint);
    
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    
    setCursor(Qt::CrossCursor);
}

void AreaSelector::showForScreen(QScreen* screen)
{
    if (!screen) {
        screen = QApplication::primaryScreen();
    }
    
    m_targetScreen = screen;
    m_screenGeometry = calculateVirtualGeometry(screen);
    m_state = SelectionState::Idle;
    m_cancelled = false;
    m_selectedRect = QRect();
    
    setGeometry(m_screenGeometry);
    show();
    activateWindow();
    raise();
    setFocus();
    
    grabKeyboard();
    grabMouse();
}

QRect AreaSelector::getSelectedRect() const
{
    return m_selectedRect;
}

bool AreaSelector::wasCancelled() const
{
    return m_cancelled;
}

void AreaSelector::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    drawMask(painter);
    
    if (m_state == SelectionState::Selecting || m_state == SelectionState::Selected) {
        drawSelection(painter);
        drawSizeLabel(painter);
    }
}

void AreaSelector::drawMask(QPainter& painter)
{
    painter.fillRect(rect(), m_maskColor);
}

void AreaSelector::drawSelection(QPainter& painter)
{
    QRect selectionRect = QRect(m_startPoint, m_endPoint).normalized();
    
    QPen pen(m_borderColor, m_borderWidth, Qt::DashLine);
    pen.setDashPattern(QVector<qreal>({4, 2}));
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(selectionRect);
    
    QColor semiTransparent = m_borderColor;
    semiTransparent.setAlpha(30);
    painter.setBrush(semiTransparent);
    painter.setPen(Qt::NoPen);
    painter.drawRect(selectionRect);
    
    painter.setPen(QPen(m_borderColor, 1));
    int handleSize = 8;
    int halfHandle = handleSize / 2;
    
    QRect handles[] = {
        QRect(selectionRect.left() - halfHandle, selectionRect.top() - halfHandle, handleSize, handleSize),
        QRect(selectionRect.right() - halfHandle, selectionRect.top() - halfHandle, handleSize, handleSize),
        QRect(selectionRect.left() - halfHandle, selectionRect.bottom() - halfHandle, handleSize, handleSize),
        QRect(selectionRect.right() - halfHandle, selectionRect.bottom() - halfHandle, handleSize, handleSize),
        
        QRect(selectionRect.center().x() - halfHandle, selectionRect.top() - halfHandle, handleSize, handleSize),
        QRect(selectionRect.center().x() - halfHandle, selectionRect.bottom() - halfHandle, handleSize, handleSize),
        QRect(selectionRect.left() - halfHandle, selectionRect.center().y() - halfHandle, handleSize, handleSize),
        QRect(selectionRect.right() - halfHandle, selectionRect.center().y() - halfHandle, handleSize, handleSize)
    };
    
    painter.setBrush(Qt::white);
    for (const auto& handle : handles) {
        painter.drawRect(handle);
    }
}

void AreaSelector::drawSizeLabel(QPainter& painter)
{
    QRect selectionRect = QRect(m_startPoint, m_endPoint).normalized();
    
    QString sizeText = QString("%1 × %2")
                      .arg(selectionRect.width())
                      .arg(selectionRect.height());
    
    QFont font = painter.font();
    font.setPointSize(10);
    font.setBold(true);
    painter.setFont(font);
    
    QFontMetrics fm(font);
    QSize textSize = fm.size(0, sizeText);
    
    QPoint labelPos;
    labelPos.setX(selectionRect.right() + m_labelMargin);
    labelPos.setY(selectionRect.bottom() + m_labelMargin);
    
    if (labelPos.x() + textSize.width() + m_labelPadding * 2 > width()) {
        labelPos.setX(selectionRect.left() - textSize.width() - m_labelPadding * 2 - m_labelMargin);
    }
    
    if (labelPos.y() + textSize.height() + m_labelPadding * 2 > height()) {
        labelPos.setY(selectionRect.top() - textSize.height() - m_labelPadding * 2 - m_labelMargin);
    }
    
    QRect labelRect(labelPos.x(), labelPos.y(), 
                    textSize.width() + m_labelPadding * 2, 
                    textSize.height() + m_labelPadding * 2);
    
    painter.setBrush(m_labelBgColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(labelRect, 3, 3);
    
    painter.setPen(m_labelTextColor);
    painter.drawText(labelRect, Qt::AlignCenter, sizeText);
}

void AreaSelector::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_state == SelectionState::Idle) {
        m_startPoint = event->pos();
        m_endPoint = event->pos();
        m_state = SelectionState::Selecting;
        update();
    }
}

void AreaSelector::mouseMoveEvent(QMouseEvent* event)
{
    if (m_state == SelectionState::Selecting) {
        m_endPoint = event->pos();
        update();
    }
}

void AreaSelector::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_state == SelectionState::Selecting) {
        m_endPoint = event->pos();
        m_selectedRect = QRect(m_startPoint, m_endPoint).normalized();
        
        if (m_selectedRect.width() < 10 || m_selectedRect.height() < 10) {
            m_state = SelectionState::Idle;
            m_selectedRect = QRect();
        } else {
            m_state = SelectionState::Selected;
        }
        
        update();
    }
}

void AreaSelector::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_state == SelectionState::Selected && !m_selectedRect.isNull()) {
            QRect globalRect = m_selectedRect;
            globalRect.translate(m_screenGeometry.topLeft());
            
            emit areaSelected(globalRect);
            hide();
            releaseMouse();
            releaseKeyboard();
        }
    } else if (event->key() == Qt::Key_Escape) {
        m_cancelled = true;
        emit selectionCancelled();
        hide();
        releaseMouse();
        releaseKeyboard();
    }
}

QRect AreaSelector::calculateVirtualGeometry(QScreen* screen) const
{
    if (!screen) {
        return QRect();
    }
    
    QRect geometry = screen->geometry();
    
    QList<QScreen*> allScreens = QApplication::screens();
    int minX = geometry.x();
    int minY = geometry.y();
    int maxX = geometry.x() + geometry.width();
    int maxY = geometry.y() + geometry.height();
    
    for (QScreen* s : allScreens) {
        QRect g = s->geometry();
        minX = qMin(minX, g.x());
        minY = qMin(minY, g.y());
        maxX = qMax(maxX, g.x() + g.width());
        maxY = qMax(maxY, g.y() + g.height());
    }
    
    return QRect(minX, minY, maxX - minX, maxY - minY);
}

QPoint AreaSelector::mapFromGlobalToWidget(const QPoint& globalPos) const
{
    return globalPos - m_screenGeometry.topLeft();
}

QPoint AreaSelector::mapFromWidgetToGlobal(const QPoint& widgetPos) const
{
    return widgetPos + m_screenGeometry.topLeft();
}
