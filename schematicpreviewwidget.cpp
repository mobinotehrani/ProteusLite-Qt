#include "schematicpreviewwidget.h"

#include <QColor>
#include <QFont>
#include <QPen>
#include <QPolygonF>
#include <QRectF>
#include <QLineF>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSizePolicy>

#include <algorithm>
#include <cmath>

SchematicPreviewWidget::SchematicPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(125);
    setMaximumHeight(145);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void SchematicPreviewWidget::setComponent(const ComponentDefinition *definition)
{
    m_definition = definition;
    update();
}

const ComponentDefinition *SchematicPreviewWidget::component() const
{
    return m_definition;
}

void SchematicPreviewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(248, 250, 252));
    painter.setPen(QPen(QColor(203, 213, 225), 1.0));
    painter.setBrush(QColor(255, 255, 255));
    painter.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 12, 12);

    if (!m_definition)
    {
        painter.setPen(QColor(100, 116, 139));
        painter.drawText(rect().adjusted(20, 20, -20, -20), Qt::AlignCenter | Qt::TextWordWrap,
                         tr("Select a component to view its schematic symbol."));
        return;
    }

    painter.save();
    const double symbolAreaHeight = std::max(55.0, height() - 48.0);
    painter.translate(width() / 2.0, symbolAreaHeight / 2.0 + 4.0);
    const double widthScale = width() / 300.0;
    const double heightScale = symbolAreaHeight / 150.0;
    const double scaleFactor = std::min({widthScale, heightScale, 1.0});
    painter.scale(scaleFactor, scaleFactor);
    drawSymbol(painter, m_definition->id);
    painter.restore();

    painter.setPen(QColor(15, 23, 42));
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPointSize(10);
    painter.setFont(titleFont);
    painter.drawText(QRectF(12, height() - 54, width() - 24, 22), Qt::AlignCenter,
                     m_definition->displayName);

    QFont captionFont = titleFont;
    captionFont.setBold(false);
    captionFont.setPointSize(8);
    painter.setFont(captionFont);
    painter.setPen(QColor(100, 116, 139));
    painter.drawText(QRectF(12, height() - 32, width() - 24, 18), Qt::AlignCenter,
                     m_definition->category);
}

void SchematicPreviewWidget::drawSymbol(QPainter &painter, const QString &id) const
{
    painter.setPen(QPen(QColor(30, 88, 220), 2.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    if (id == QStringLiteral("Resistor") || id == QStringLiteral("Potentiometer"))
        drawResistor(painter);
    else if (id == QStringLiteral("Capacitor"))
        drawCapacitor(painter);
    else if (id == QStringLiteral("Inductor"))
        drawInductor(painter);
    else if (id == QStringLiteral("Ground"))
        drawGround(painter);
    else if (id == QStringLiteral("Voltage") || id == QStringLiteral("Battery") ||
             id == QStringLiteral("Clock") || id == QStringLiteral("Current"))
        drawSource(painter, id);
    else if (id == QStringLiteral("Switch") || id == QStringLiteral("PushButton"))
        drawSwitch(painter, id == QStringLiteral("PushButton"));
    else if (id.startsWith(QStringLiteral("LED")))
        drawLed(painter, id);
    else if (id == QStringLiteral("SevenSegment"))
        drawSevenSegment(painter);
    else if (id == QStringLiteral("AND") || id == QStringLiteral("OR") ||
             id == QStringLiteral("NOT") || id == QStringLiteral("NAND") ||
             id == QStringLiteral("XOR"))
        drawLogicGate(painter, id);
    else if (id == QStringLiteral("DFlipFlop"))
        drawBlock(painter, QStringLiteral("D FF"), 2, 2);
    else if (id == QStringLiteral("ADC") || id == QStringLiteral("DAC"))
        drawBlock(painter, id, id == QStringLiteral("ADC") ? 3 : 6,
                  id == QStringLiteral("ADC") ? 4 : 1);
    else if (id == QStringLiteral("MCU"))
        drawBlock(painter, QStringLiteral("MCU"), 4, 4);
    else if (id == QStringLiteral("EEPROM"))
        drawBlock(painter, QStringLiteral("MEM"), 4, 4);
    else if (id == QStringLiteral("LCD16x2"))
        drawBlock(painter, QStringLiteral("LCD 16x2"), 4, 2);
    else if (id == QStringLiteral("Keypad4x4"))
        drawBlock(painter, QStringLiteral("KEYPAD"), 4, 4);
    else
        drawInstrument(painter, id);
}

void SchematicPreviewWidget::drawResistor(QPainter &painter) const
{
    painter.drawLine(QPointF(-72, 0), QPointF(-44, 0));
    QPolygonF zigzag;
    zigzag << QPointF(-44, 0) << QPointF(-32, -15) << QPointF(-20, 15)
           << QPointF(-8, -15) << QPointF(4, 15) << QPointF(16, -15)
           << QPointF(28, 15) << QPointF(40, 0);
    painter.drawPolyline(zigzag);
    painter.drawLine(QPointF(40, 0), QPointF(72, 0));
}

void SchematicPreviewWidget::drawCapacitor(QPainter &painter) const
{
    painter.drawLine(QPointF(-72, 0), QPointF(-18, 0));
    painter.drawLine(QPointF(-18, -30), QPointF(-18, 30));
    painter.drawLine(QPointF(18, -30), QPointF(18, 30));
    painter.drawLine(QPointF(18, 0), QPointF(72, 0));
}

void SchematicPreviewWidget::drawInductor(QPainter &painter) const
{
    painter.drawLine(QPointF(-72, 0), QPointF(-48, 0));
    for (int i = 0; i < 4; ++i)
        painter.drawArc(QRectF(-48 + i * 24, -16, 24, 32), 0, 180 * 16);
    painter.drawLine(QPointF(48, 0), QPointF(72, 0));
}

void SchematicPreviewWidget::drawGround(QPainter &painter) const
{
    painter.drawLine(QPointF(0, -50), QPointF(0, -8));
    painter.drawLine(QPointF(-34, -8), QPointF(34, -8));
    painter.drawLine(QPointF(-22, 8), QPointF(22, 8));
    painter.drawLine(QPointF(-10, 24), QPointF(10, 24));
}

void SchematicPreviewWidget::drawSource(QPainter &painter, const QString &id) const
{
    painter.drawLine(QPointF(0, -70), QPointF(0, -34));
    painter.drawLine(QPointF(0, 34), QPointF(0, 70));
    painter.drawEllipse(QPointF(0, 0), 34, 34);

    if (id == QStringLiteral("Current"))
    {
        painter.drawLine(QPointF(0, 18), QPointF(0, -15));
        QPolygonF arrow;
        arrow << QPointF(-7, -7) << QPointF(7, -7) << QPointF(0, -18);
        painter.setBrush(QColor(30, 88, 220));
        painter.drawPolygon(arrow);
        painter.setBrush(Qt::NoBrush);
    }
    else if (id == QStringLiteral("Clock"))
    {
        QPainterPath wave;
        wave.moveTo(-23, 15);
        wave.lineTo(-23, -14);
        wave.lineTo(-5, -14);
        wave.lineTo(-5, 15);
        wave.lineTo(14, 15);
        wave.lineTo(14, -14);
        wave.lineTo(24, -14);
        painter.drawPath(wave);
    }
    else
    {
        painter.drawLine(QPointF(-13, -12), QPointF(13, -12));
        painter.drawLine(QPointF(0, -25), QPointF(0, 1));
        painter.drawLine(QPointF(-13, 15), QPointF(13, 15));
        if (id == QStringLiteral("Battery"))
            painter.drawText(QRectF(-25, -8, 50, 20), Qt::AlignCenter, QStringLiteral("BAT"));
    }
}

void SchematicPreviewWidget::drawSwitch(QPainter &painter, bool pushButton) const
{
    painter.drawLine(QPointF(-72, 0), QPointF(-22, 0));
    painter.drawLine(QPointF(22, 0), QPointF(72, 0));
    painter.drawEllipse(QPointF(-22, 0), 4, 4);
    painter.drawEllipse(QPointF(22, 0), 4, 4);
    painter.drawLine(QPointF(-18, -2), QPointF(20, -28));
    if (pushButton)
    {
        painter.drawLine(QPointF(0, -50), QPointF(0, -28));
        painter.drawLine(QPointF(-16, -50), QPointF(16, -50));
    }
}

void SchematicPreviewWidget::drawLed(QPainter &painter, const QString &id) const
{
    painter.drawLine(QPointF(-72, 0), QPointF(-28, 0));
    painter.drawLine(QPointF(28, 0), QPointF(72, 0));
    QPolygonF triangle;
    triangle << QPointF(-28, -28) << QPointF(-28, 28) << QPointF(25, 0);

    QColor color = QColor(220, 38, 38);
    if (id == QStringLiteral("LEDGreen"))
        color = QColor(22, 163, 74);
    else if (id == QStringLiteral("LEDBlue"))
        color = QColor(37, 99, 235);

    painter.setBrush(color.lighter(175));
    painter.drawPolygon(triangle);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(25, -28), QPointF(25, 28));
    painter.drawLine(QPointF(35, -24), QPointF(52, -42));
    painter.drawLine(QPointF(42, -15), QPointF(59, -33));
}

void SchematicPreviewWidget::drawSevenSegment(QPainter &painter) const
{
    painter.setBrush(QColor(30, 41, 59));
    painter.drawRoundedRect(QRectF(-46, -60, 92, 120), 10, 10);
    painter.setPen(QPen(QColor(248, 113, 113), 7, Qt::SolidLine, Qt::RoundCap));
    const QList<QLineF> segments{
        QLineF(-24, -43, 24, -43), QLineF(30, -36, 30, -5),
        QLineF(30, 5, 30, 36), QLineF(-24, 43, 24, 43),
        QLineF(-30, 5, -30, 36), QLineF(-30, -36, -30, -5),
        QLineF(-24, 0, 24, 0)};
    for (const QLineF &segment : segments)
        painter.drawLine(segment);
}

void SchematicPreviewWidget::drawLogicGate(QPainter &painter, const QString &id) const
{
    painter.drawLine(QPointF(-75, -22), QPointF(-35, -22));
    if (id != QStringLiteral("NOT"))
        painter.drawLine(QPointF(-75, 22), QPointF(-35, 22));
    painter.drawLine(QPointF(38, 0), QPointF(75, 0));

    if (id == QStringLiteral("NOT"))
    {
        QPolygonF triangle;
        triangle << QPointF(-35, -36) << QPointF(-35, 36) << QPointF(32, 0);
        painter.drawPolygon(triangle);
        painter.drawEllipse(QPointF(38, 0), 6, 6);
        return;
    }

    if (id == QStringLiteral("AND") || id == QStringLiteral("NAND"))
    {
        QPainterPath path;
        path.moveTo(-35, -42);
        path.lineTo(-35, 42);
        path.lineTo(0, 42);
        path.arcTo(QRectF(-42, -42, 84, 84), -90, 180);
        path.closeSubpath();
        painter.drawPath(path);
        if (id == QStringLiteral("NAND"))
            painter.drawEllipse(QPointF(47, 0), 6, 6);
        return;
    }

    QPainterPath path;
    path.moveTo(-35, -42);
    path.quadTo(-5, -34, 35, 0);
    path.quadTo(-5, 34, -35, 42);
    path.quadTo(-15, 0, -35, -42);
    painter.drawPath(path);
    if (id == QStringLiteral("XOR"))
    {
        QPainterPath extra;
        extra.moveTo(-45, -42);
        extra.quadTo(-25, 0, -45, 42);
        painter.drawPath(extra);
    }
}

void SchematicPreviewWidget::drawBlock(QPainter &painter,
                                       const QString &title,
                                       int inputPins,
                                       int outputPins) const
{
    const QRectF body(-48, -52, 96, 104);
    painter.setBrush(QColor(239, 246, 255));
    painter.drawRoundedRect(body, 8, 8);
    painter.setBrush(Qt::NoBrush);

    const int maxPins = std::max(inputPins, outputPins);
    for (int i = 0; i < inputPins; ++i)
    {
        const double y = -40.0 + (maxPins <= 1 ? 40.0 : i * 80.0 / (maxPins - 1));
        painter.drawLine(QPointF(-75, y), QPointF(-48, y));
    }
    for (int i = 0; i < outputPins; ++i)
    {
        const double y = -40.0 + (maxPins <= 1 ? 40.0 : i * 80.0 / (maxPins - 1));
        painter.drawLine(QPointF(48, y), QPointF(75, y));
    }

    painter.setPen(QColor(30, 64, 175));
    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(11);
    painter.setFont(font);
    painter.drawText(body, Qt::AlignCenter, title);
}

void SchematicPreviewWidget::drawInstrument(QPainter &painter, const QString &id) const
{
    painter.drawLine(QPointF(-72, 0), QPointF(-34, 0));
    painter.drawLine(QPointF(34, 0), QPointF(72, 0));
    painter.setBrush(QColor(248, 250, 252));
    painter.drawEllipse(QPointF(0, 0), 34, 34);
    painter.setBrush(Qt::NoBrush);

    QString label = QStringLiteral("V");
    if (id == QStringLiteral("Ammeter"))
        label = QStringLiteral("A");
    else if (id == QStringLiteral("Oscilloscope"))
        label = QStringLiteral("OSC");
    else if (id == QStringLiteral("VoltageProbe"))
        label = QStringLiteral("PR");

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(12);
    painter.setFont(font);
    painter.drawText(QRectF(-32, -18, 64, 36), Qt::AlignCenter, label);
}
