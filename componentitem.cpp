#include "componentitem.h"

#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QLineF>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QStyleOptionGraphicsItem>

#include <algorithm>
#include <cmath>

namespace
{
QHash<QString, int> &referenceCounters()
{
    static QHash<QString, int> counters;
    return counters;
}

PinModel pin(const char *name, qreal x, qreal y, PinDirection direction = PinDirection::Passive)
{
    return PinModel{QString::fromLatin1(name), QPointF(x, y), direction, 11.0};
}

QVector<PinModel> horizontalPair(const char *first = "A", const char *second = "B")
{
    return {pin(first, -48.0, 0.0), pin(second, 48.0, 0.0)};
}
}

ComponentItem::ComponentItem(const QString &modelId,
                             const ComponentDefinition &definition,
                             int gridSpacing,
                             QGraphicsItem *parent)
    : QGraphicsObject(parent),
      m_modelId(modelId),
      m_definition(definition),
      m_reference(nextReference(definition.id)),
      m_value(definition.defaultValue),
      m_pins(createPins(definition.id)),
      m_gridSpacing(std::max(5, gridSpacing))
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemIsFocusable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setCursor(Qt::OpenHandCursor);
    setToolTip(definition.displayName + QStringLiteral("\n") + definition.description);
    setZValue(10.0);
}

int ComponentItem::type() const
{
    return Type;
}

QRectF ComponentItem::boundingRect() const
{
    if (m_definition.id == QStringLiteral("MCU"))
        return QRectF(-86.0, -70.0, 172.0, 140.0);
    if (m_definition.id == QStringLiteral("LCD16x2"))
        return QRectF(-94.0, -62.0, 188.0, 124.0);
    if (m_definition.id == QStringLiteral("Keypad4x4"))
        return QRectF(-78.0, -68.0, 156.0, 136.0);
    if (m_definition.id == QStringLiteral("ADC") || m_definition.id == QStringLiteral("DAC"))
        return QRectF(-80.0, -62.0, 160.0, 124.0);
    return QRectF(-70.0, -58.0, 140.0, 116.0);
}

void ComponentItem::paint(QPainter *painter,
                          const QStyleOptionGraphicsItem *option,
                          QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);

    if (isSelected())
    {
        painter->setPen(QPen(QColor(37, 99, 235), 1.5, Qt::DashLine));
        painter->setBrush(QColor(59, 130, 246, 24));
        painter->drawRoundedRect(boundingRect().adjusted(3.0, 3.0, -3.0, -3.0), 7.0, 7.0);
    }

    drawSymbol(painter);

    for (int i = 0; i < m_pins.size(); ++i)
    {
        const bool highlighted = i == m_highlightedPin;
        painter->setPen(QPen(highlighted ? QColor(220, 38, 38) : QColor(30, 64, 175),
                             highlighted ? 2.2 : 1.3));
        painter->setBrush(highlighted ? QColor(254, 202, 202) : QColor(255, 255, 255));
        painter->drawEllipse(m_pins[i].localPosition, highlighted ? 5.5 : 4.0,
                             highlighted ? 5.5 : 4.0);
    }

    drawPinLabels(painter);

    QFont referenceFont = painter->font();
    referenceFont.setPointSize(8);
    referenceFont.setBold(true);
    painter->setFont(referenceFont);
    painter->setPen(QColor(15, 23, 42));
    painter->drawText(QRectF(-62.0, 35.0, 124.0, 16.0), Qt::AlignCenter, m_reference);

    referenceFont.setPointSize(7);
    referenceFont.setBold(false);
    painter->setFont(referenceFont);
    const QString shortValue = m_value.size() > 22 ? m_value.left(21) + QChar(0x2026) : m_value;
    painter->drawText(QRectF(-64.0, 48.0, 128.0, 14.0), Qt::AlignCenter, shortValue);
}

QString ComponentItem::modelId() const
{
    return m_modelId;
}

QString ComponentItem::componentType() const
{
    return m_definition.id;
}

QString ComponentItem::reference() const
{
    return m_reference;
}

QString ComponentItem::value() const
{
    return m_value;
}

QVector<PinModel> ComponentItem::pins() const
{
    return m_pins;
}

QPointF ComponentItem::scenePinPosition(int pinIndex) const
{
    if (pinIndex < 0 || pinIndex >= m_pins.size())
        return scenePos();
    return mapToScene(m_pins[pinIndex].localPosition);
}

int ComponentItem::pinAtScenePosition(const QPointF &scenePosition, qreal tolerance) const
{
    int closestIndex = -1;
    qreal closestDistance = tolerance;
    for (int i = 0; i < m_pins.size(); ++i)
    {
        const QLineF distanceLine(scenePinPosition(i), scenePosition);
        if (distanceLine.length() <= closestDistance)
        {
            closestDistance = distanceLine.length();
            closestIndex = i;
        }
    }
    return closestIndex;
}

void ComponentItem::setHighlightedPin(int pinIndex)
{
    if (m_highlightedPin == pinIndex)
        return;
    m_highlightedPin = pinIndex;
    update();
}

void ComponentItem::rotateClockwise()
{
    m_rotationSteps = (m_rotationSteps + 1) % 4;
    applyVisualTransform();
}

void ComponentItem::mirrorHorizontal()
{
    m_mirrorHorizontal = !m_mirrorHorizontal;
    applyVisualTransform();
}

void ComponentItem::mirrorVertical()
{
    m_mirrorVertical = !m_mirrorVertical;
    applyVisualTransform();
}

void ComponentItem::openProperties()
{
    QDialog dialog;
    dialog.setWindowTitle(tr("Properties - %1").arg(m_definition.displayName));
    dialog.setMinimumWidth(390);

    QFormLayout layout(&dialog);
    auto *referenceEdit = new QLineEdit(m_reference, &dialog);
    auto *valueEdit = new QLineEdit(m_value, &dialog);
    auto *typeLabel = new QLabel(m_definition.displayName, &dialog);
    auto *pinLabel = new QLabel(tr("%1 pin(s): %2")
                                    .arg(m_pins.size())
                                    .arg([this]
                                         {
                                             QStringList names;
                                             for (const PinModel &model : m_pins)
                                                 names.append(model.name);
                                             return names.join(QStringLiteral(", "));
                                         }()),
                                &dialog);
    pinLabel->setWordWrap(true);

    layout.addRow(tr("Type"), typeLabel);
    layout.addRow(tr("Reference"), referenceEdit);
    layout.addRow(tr("Value / Parameters"), valueEdit);
    layout.addRow(tr("Pins"), pinLabel);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString newReference = referenceEdit->text().trimmed();
    if (!newReference.isEmpty())
        m_reference = newReference;
    m_value = valueEdit->text().trimmed();
    update();
    emit edited();
}

QVector<PinModel> ComponentItem::pinsForType(const QString &type)
{
    return createPins(type);
}

void ComponentItem::resetReferenceCounters()
{
    referenceCounters().clear();
}

QVariant ComponentItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && scene())
    {
        const QPointF proposed = value.toPointF();
        const qreal spacing = static_cast<qreal>(m_gridSpacing);
        return QPointF(std::round(proposed.x() / spacing) * spacing,
                       std::round(proposed.y() / spacing) * spacing);
    }

    const QVariant result = QGraphicsObject::itemChange(change, value);
    if (change == ItemPositionHasChanged || change == ItemTransformHasChanged)
        emit geometryChanged();
    return result;
}

void ComponentItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    int closest = -1;
    qreal closestDistance = 12.0;
    for (int i = 0; i < m_pins.size(); ++i)
    {
        const QLineF line(event->pos(), m_pins[i].localPosition);
        if (line.length() <= closestDistance)
        {
            closest = i;
            closestDistance = line.length();
        }
    }
    setHighlightedPin(closest);
    QGraphicsObject::hoverMoveEvent(event);
}

void ComponentItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setHighlightedPin(-1);
    QGraphicsObject::hoverLeaveEvent(event);
}

void ComponentItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    openProperties();
    event->accept();
}

void ComponentItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;
    QAction *propertiesAction = menu.addAction(tr("Properties..."));
    menu.addSeparator();
    QAction *rotateAction = menu.addAction(tr("Rotate 90 degrees"));
    QAction *mirrorHorizontalAction = menu.addAction(tr("Mirror horizontally"));
    QAction *mirrorVerticalAction = menu.addAction(tr("Mirror vertically"));
    menu.addSeparator();
    QAction *deleteAction = menu.addAction(tr("Delete component"));

    QAction *chosen = menu.exec(event->screenPos());
    if (chosen == propertiesAction)
        openProperties();
    else if (chosen == rotateAction)
        rotateClockwise();
    else if (chosen == mirrorHorizontalAction)
        mirrorHorizontal();
    else if (chosen == mirrorVerticalAction)
        mirrorVertical();
    else if (chosen == deleteAction)
        emit deleteRequested(this);

    event->accept();
}

QVector<PinModel> ComponentItem::createPins(const QString &type)
{
    if (type == QStringLiteral("Ground"))
        return {pin("GND", 0.0, -34.0, PinDirection::Power)};
    if (type == QStringLiteral("VoltageProbe"))
        return {pin("V", 0.0, -34.0, PinDirection::Input)};
    if (type == QStringLiteral("Voltage") || type == QStringLiteral("Battery") ||
        type == QStringLiteral("Clock") || type == QStringLiteral("Current"))
        return {pin("OUT", 0.0, -42.0, PinDirection::Output),
                pin("RET", 0.0, 42.0, PinDirection::Power)};
    if (type == QStringLiteral("NOT"))
        return {pin("IN", -52.0, 0.0, PinDirection::Input),
                pin("OUT", 52.0, 0.0, PinDirection::Output)};
    if (type == QStringLiteral("AND") || type == QStringLiteral("OR") ||
        type == QStringLiteral("NAND") || type == QStringLiteral("XOR"))
        return {pin("A", -54.0, -18.0, PinDirection::Input),
                pin("B", -54.0, 18.0, PinDirection::Input),
                pin("Y", 54.0, 0.0, PinDirection::Output)};
    if (type == QStringLiteral("DFlipFlop"))
        return {pin("D", -58.0, -20.0, PinDirection::Input),
                pin("CLK", -58.0, 20.0, PinDirection::Input),
                pin("Q", 58.0, -20.0, PinDirection::Output),
                pin("/Q", 58.0, 20.0, PinDirection::Output)};
    if (type == QStringLiteral("SevenSegment"))
        return {pin("a", -56.0, -30.0, PinDirection::Input),
                pin("b", -56.0, -20.0, PinDirection::Input),
                pin("c", -56.0, -10.0, PinDirection::Input),
                pin("d", -56.0, 0.0, PinDirection::Input),
                pin("e", -56.0, 10.0, PinDirection::Input),
                pin("f", -56.0, 20.0, PinDirection::Input),
                pin("g", -56.0, 30.0, PinDirection::Input),
                pin("COM", 56.0, 30.0, PinDirection::Power)};
    if (type == QStringLiteral("ADC"))
        return {pin("VIN", -64.0, -28.0, PinDirection::Input),
                pin("VREF-", -64.0, 0.0, PinDirection::Power),
                pin("VREF+", -64.0, 28.0, PinDirection::Power),
                pin("D0", 64.0, -30.0, PinDirection::Output),
                pin("D1", 64.0, -10.0, PinDirection::Output),
                pin("D2", 64.0, 10.0, PinDirection::Output),
                pin("D3", 64.0, 30.0, PinDirection::Output)};
    if (type == QStringLiteral("DAC"))
        return {pin("D0", -64.0, -30.0, PinDirection::Input),
                pin("D1", -64.0, -10.0, PinDirection::Input),
                pin("D2", -64.0, 10.0, PinDirection::Input),
                pin("D3", -64.0, 30.0, PinDirection::Input),
                pin("VOUT", 64.0, 0.0, PinDirection::Output),
                pin("VREF+", 64.0, -28.0, PinDirection::Power),
                pin("VREF-", 64.0, 28.0, PinDirection::Power)};
    if (type == QStringLiteral("MCU"))
    {
        QVector<PinModel> result;
        for (int i = 0; i < 6; ++i)
            result.append(PinModel{QStringLiteral("PA%1").arg(i),
                                   QPointF(-74.0, -50.0 + i * 20.0),
                                   PinDirection::Passive, 11.0});
        for (int i = 0; i < 6; ++i)
            result.append(PinModel{QStringLiteral("PB%1").arg(i),
                                   QPointF(74.0, -50.0 + i * 20.0),
                                   PinDirection::Passive, 11.0});
        return result;
    }
    if (type == QStringLiteral("EEPROM"))
        return {pin("A0", -62.0, -28.0), pin("A1", -62.0, 0.0), pin("A2", -62.0, 28.0),
                pin("D0", 62.0, -28.0), pin("D1", 62.0, 0.0), pin("D2", 62.0, 28.0)};
    if (type == QStringLiteral("LCD16x2"))
    {
        QVector<PinModel> result;
        for (int i = 0; i < 8; ++i)
            result.append(PinModel{QStringLiteral("D%1").arg(i), QPointF(-70.0 + i * 20.0, 46.0),
                                   PinDirection::Input, 11.0});
        return result;
    }
    if (type == QStringLiteral("Keypad4x4"))
        return {pin("R1", -62.0, -30.0), pin("R2", -62.0, -10.0),
                pin("R3", -62.0, 10.0), pin("R4", -62.0, 30.0),
                pin("C1", 62.0, -30.0), pin("C2", 62.0, -10.0),
                pin("C3", 62.0, 10.0), pin("C4", 62.0, 30.0)};
    if (type == QStringLiteral("Oscilloscope"))
        return {pin("CH1", -62.0, -20.0, PinDirection::Input),
                pin("CH2", -62.0, 20.0, PinDirection::Input),
                pin("GND", 62.0, 40.0, PinDirection::Power)};
    if (type.startsWith(QStringLiteral("LED")))
        return horizontalPair("A", "K");
    if (type == QStringLiteral("Voltmeter"))
        return horizontalPair("V+", "V-");
    if (type == QStringLiteral("Ammeter"))
        return horizontalPair("I+", "I-");
    return horizontalPair();
}

QString ComponentItem::referencePrefix(const QString &type)
{
    static const QHash<QString, QString> prefixes{
        {QStringLiteral("Resistor"), QStringLiteral("R")},
        {QStringLiteral("Capacitor"), QStringLiteral("C")},
        {QStringLiteral("Inductor"), QStringLiteral("L")},
        {QStringLiteral("Voltage"), QStringLiteral("V")},
        {QStringLiteral("Battery"), QStringLiteral("BAT")},
        {QStringLiteral("Clock"), QStringLiteral("CLK")},
        {QStringLiteral("Current"), QStringLiteral("I")},
        {QStringLiteral("Ground"), QStringLiteral("GND")},
        {QStringLiteral("Switch"), QStringLiteral("SW")},
        {QStringLiteral("PushButton"), QStringLiteral("PB")},
        {QStringLiteral("DFlipFlop"), QStringLiteral("U")},
        {QStringLiteral("ADC"), QStringLiteral("ADC")},
        {QStringLiteral("DAC"), QStringLiteral("DAC")},
        {QStringLiteral("MCU"), QStringLiteral("MCU")},
        {QStringLiteral("EEPROM"), QStringLiteral("MEM")},
        {QStringLiteral("LCD16x2"), QStringLiteral("LCD")},
        {QStringLiteral("Keypad4x4"), QStringLiteral("KEY")},
        {QStringLiteral("VoltageProbe"), QStringLiteral("PR")},
        {QStringLiteral("Voltmeter"), QStringLiteral("VM")},
        {QStringLiteral("Ammeter"), QStringLiteral("AM")},
        {QStringLiteral("Oscilloscope"), QStringLiteral("OSC")}};

    if (type.startsWith(QStringLiteral("LED")) || type == QStringLiteral("SevenSegment"))
        return QStringLiteral("D");
    if (type == QStringLiteral("AND") || type == QStringLiteral("OR") ||
        type == QStringLiteral("NOT") || type == QStringLiteral("NAND") ||
        type == QStringLiteral("XOR"))
        return QStringLiteral("U");
    return prefixes.value(type, QStringLiteral("X"));
}

QString ComponentItem::nextReference(const QString &type)
{
    const QString prefix = referencePrefix(type);
    const int number = ++referenceCounters()[prefix];
    return prefix + QString::number(number);
}

void ComponentItem::applyVisualTransform()
{
    QTransform transform;
    transform.scale(m_mirrorHorizontal ? -1.0 : 1.0, m_mirrorVertical ? -1.0 : 1.0);
    transform.rotate(90.0 * m_rotationSteps);
    setTransform(transform);
    update();
    emit geometryChanged();
    emit edited();
}

void ComponentItem::drawSymbol(QPainter *painter) const
{
    const QString type = m_definition.id;
    const QPen mainPen(QColor(30, 88, 220), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(mainPen);
    painter->setBrush(Qt::NoBrush);

    if (type == QStringLiteral("Resistor"))
    {
        painter->drawLine(QPointF(-48.0, 0.0), QPointF(-34.0, 0.0));
        QPainterPath path(QPointF(-34.0, 0.0));
        for (int i = 0; i < 8; ++i)
            path.lineTo(-30.0 + i * 8.0, i % 2 == 0 ? -10.0 : 10.0);
        path.lineTo(34.0, 0.0);
        painter->drawPath(path);
        painter->drawLine(QPointF(34.0, 0.0), QPointF(48.0, 0.0));
        return;
    }
    if (type == QStringLiteral("Capacitor"))
    {
        painter->drawLine(QPointF(-48.0, 0.0), QPointF(-8.0, 0.0));
        painter->drawLine(QPointF(-8.0, -22.0), QPointF(-8.0, 22.0));
        painter->drawLine(QPointF(8.0, -22.0), QPointF(8.0, 22.0));
        painter->drawLine(QPointF(8.0, 0.0), QPointF(48.0, 0.0));
        return;
    }
    if (type == QStringLiteral("Inductor"))
    {
        painter->drawLine(QPointF(-48.0, 0.0), QPointF(-32.0, 0.0));
        for (int i = 0; i < 4; ++i)
            painter->drawArc(QRectF(-32.0 + i * 16.0, -10.0, 16.0, 20.0), 0, 180 * 16);
        painter->drawLine(QPointF(32.0, 0.0), QPointF(48.0, 0.0));
        return;
    }
    if (type == QStringLiteral("Ground"))
    {
        painter->drawLine(QPointF(0.0, -34.0), QPointF(0.0, -8.0));
        painter->drawLine(QPointF(-22.0, -8.0), QPointF(22.0, -8.0));
        painter->drawLine(QPointF(-15.0, 0.0), QPointF(15.0, 0.0));
        painter->drawLine(QPointF(-8.0, 8.0), QPointF(8.0, 8.0));
        return;
    }
    if (type == QStringLiteral("Voltage") || type == QStringLiteral("Battery") ||
        type == QStringLiteral("Clock") || type == QStringLiteral("Current"))
    {
        painter->drawLine(QPointF(0.0, -42.0), QPointF(0.0, -24.0));
        painter->drawEllipse(QPointF(0.0, 0.0), 24.0, 24.0);
        painter->drawLine(QPointF(0.0, 24.0), QPointF(0.0, 42.0));
        painter->drawText(QRectF(-20.0, -10.0, 40.0, 20.0), Qt::AlignCenter,
                          type == QStringLiteral("Clock") ? QStringLiteral("~")
                                                           : type.left(1));
        return;
    }
    if (type == QStringLiteral("Switch") || type == QStringLiteral("PushButton"))
    {
        painter->drawLine(QPointF(-48.0, 0.0), QPointF(-24.0, 0.0));
        painter->drawLine(QPointF(24.0, 0.0), QPointF(48.0, 0.0));
        painter->drawEllipse(QPointF(-24.0, 0.0), 3.0, 3.0);
        painter->drawEllipse(QPointF(24.0, 0.0), 3.0, 3.0);
        painter->drawLine(QPointF(-22.0, -2.0), QPointF(20.0, -18.0));
        return;
    }
    if (type.startsWith(QStringLiteral("LED")))
    {
        painter->drawLine(QPointF(-48.0, 0.0), QPointF(-18.0, 0.0));
        QPolygonF triangle;
        triangle << QPointF(-18.0, -18.0) << QPointF(-18.0, 18.0) << QPointF(14.0, 0.0);
        painter->drawPolygon(triangle);
        painter->drawLine(QPointF(14.0, -20.0), QPointF(14.0, 20.0));
        painter->drawLine(QPointF(14.0, 0.0), QPointF(48.0, 0.0));
        painter->drawLine(QPointF(18.0, -22.0), QPointF(32.0, -34.0));
        painter->drawLine(QPointF(26.0, -18.0), QPointF(40.0, -30.0));
        return;
    }

    QRectF body(-38.0, -30.0, 76.0, 60.0);
    if (type == QStringLiteral("MCU"))
        body = QRectF(-58.0, -54.0, 116.0, 108.0);
    else if (type == QStringLiteral("LCD16x2"))
        body = QRectF(-80.0, -42.0, 160.0, 78.0);
    else if (type == QStringLiteral("Keypad4x4"))
        body = QRectF(-46.0, -52.0, 92.0, 104.0);
    else if (type == QStringLiteral("ADC") || type == QStringLiteral("DAC"))
        body = QRectF(-50.0, -44.0, 100.0, 88.0);

    painter->setBrush(QColor(248, 250, 252));
    painter->drawRoundedRect(body, 7.0, 7.0);
    painter->setBrush(Qt::NoBrush);

    QString label = m_definition.displayName;
    if (type == QStringLiteral("LCD16x2"))
        label = QStringLiteral("LCD 16x2\n") + m_value.left(16);
    painter->drawText(body.adjusted(5.0, 5.0, -5.0, -5.0), Qt::AlignCenter | Qt::TextWordWrap,
                      label);

    for (const PinModel &model : m_pins)
    {
        const QPointF point = model.localPosition;
        QPointF inner = point;
        if (std::abs(point.x()) > std::abs(point.y()))
            inner.setX(point.x() > 0.0 ? body.right() : body.left());
        else
            inner.setY(point.y() > 0.0 ? body.bottom() : body.top());
        painter->drawLine(inner, point);
    }
}

void ComponentItem::drawPinLabels(QPainter *painter) const
{
    QFont font = painter->font();
    font.setPointSize(6);
    font.setBold(false);
    painter->setFont(font);
    painter->setPen(QColor(71, 85, 105));

    for (const PinModel &model : m_pins)
    {
        const QPointF point = model.localPosition;
        QRectF textRect;
        Qt::Alignment alignment = Qt::AlignCenter;
        if (point.x() < -20.0)
        {
            textRect = QRectF(point.x() + 8.0, point.y() - 7.0, 42.0, 14.0);
            alignment = Qt::AlignLeft | Qt::AlignVCenter;
        }
        else if (point.x() > 20.0)
        {
            textRect = QRectF(point.x() - 50.0, point.y() - 7.0, 42.0, 14.0);
            alignment = Qt::AlignRight | Qt::AlignVCenter;
        }
        else if (point.y() < 0.0)
        {
            textRect = QRectF(point.x() - 24.0, point.y() + 7.0, 48.0, 14.0);
        }
        else
        {
            textRect = QRectF(point.x() - 24.0, point.y() - 21.0, 48.0, 14.0);
        }
        painter->drawText(textRect, alignment, model.name);
    }
}
