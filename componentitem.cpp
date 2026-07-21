#include "componentitem.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QLineF>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QSpinBox>
#include <QPushButton>

#include <algorithm>
#include <cmath>

namespace
{
QHash<QString, int> &referenceCounters()
{
    static QHash<QString, int> counters;
    return counters;
}

PinModel makePin(const QString &name, const QPointF &position, PinDirection direction)
{
    return {name, position, direction, 11.0};
}

QColor faded(const QColor &color)
{
    QColor result = color;
    result.setAlpha(55);
    return result;
}

QVariant editorValue(QWidget *editor, ComponentPropertyType type)
{
    if (type == ComponentPropertyType::Number)
        return qobject_cast<QDoubleSpinBox *>(editor)->value();
    if (type == ComponentPropertyType::Integer)
        return qobject_cast<QSpinBox *>(editor)->value();
    if (type == ComponentPropertyType::Boolean)
        return qobject_cast<QCheckBox *>(editor)->isChecked();
    if (type == ComponentPropertyType::Choice)
        return qobject_cast<QComboBox *>(editor)->currentText();
    if (type == ComponentPropertyType::FilePath)
    {
        auto *pathEdit = editor->findChild<QLineEdit *>(QStringLiteral("PathEdit"));
        return pathEdit ? QVariant(pathEdit->text().trimmed()) : QVariant();
    }
    auto *lineEdit = qobject_cast<QLineEdit *>(editor);
    return lineEdit ? QVariant(lineEdit->text().trimmed()) : QVariant();
}
}

ComponentItem::ComponentItem(const QString &modelId,
                             const ComponentDefinition &definition,
                             int gridSpacing,
                             QGraphicsItem *parent)
    : QGraphicsObject(parent), m_modelId(modelId), m_definition(definition),
      m_reference(nextReference(definition.id)), m_component(ComponentFactory::create(definition.id)),
      m_gridSpacing(std::max(5, gridSpacing))
{
    if (auto *generic = dynamic_cast<GenericComponent *>(m_component.get()))
        generic->setProperty(QStringLiteral("value"), definition.defaultValue);
    m_pins = pinsForModel();
    syncValueText();
    setFlags(ItemIsMovable | ItemIsSelectable | ItemIsFocusable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setCursor(Qt::OpenHandCursor);
    setToolTip(definition.displayName + QStringLiteral("\n") + definition.description);
    setZValue(10.0);
}

ComponentItem::~ComponentItem() = default;

int ComponentItem::type() const
{
    return Type;
}

QRectF ComponentItem::boundingRect() const
{
    if (m_definition.id == QStringLiteral("MCU"))
        return QRectF(-108.0, -92.0, 216.0, 184.0);
    if (m_definition.id == QStringLiteral("EEPROM"))
        return QRectF(-98.0, -82.0, 196.0, 164.0);
    if (m_definition.id == QStringLiteral("LCD16x2"))
        return QRectF(-106.0, -76.0, 212.0, 152.0);
    if (m_definition.id == QStringLiteral("Keypad4x4"))
        return QRectF(-88.0, -82.0, 176.0, 164.0);
    if (m_definition.id == QStringLiteral("ADC") || m_definition.id == QStringLiteral("DAC"))
        return QRectF(-94.0, -74.0, 188.0, 148.0);
    if (m_definition.id == QStringLiteral("SevenSegment"))
        return QRectF(-78.0, -66.0, 156.0, 132.0);
    return QRectF(-70.0, -58.0, 140.0, 116.0);
}

void ComponentItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
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
        painter->setPen(
            QPen(highlighted ? QColor(220, 38, 38) : QColor(30, 64, 175), highlighted ? 2.2 : 1.3));
        painter->setBrush(highlighted ? QColor(254, 202, 202) : QColor(255, 255, 255));
        painter->drawEllipse(m_pins[i].localPosition, highlighted ? 5.5 : 4.0, highlighted ? 5.5 : 4.0);
    }

    drawPinLabels(painter);

    QFont referenceFont = painter->font();
    referenceFont.setPointSize(8);
    referenceFont.setBold(true);
    painter->setFont(referenceFont);
    painter->setPen(QColor(15, 23, 42));

    qreal referenceY = 35.0;
    qreal captionWidth = 124.0;
    if (m_definition.id == QStringLiteral("MCU"))
    {
        referenceY = 66.0;
        captionWidth = 178.0;
    }
    else if (m_definition.id == QStringLiteral("EEPROM") ||
             m_definition.id == QStringLiteral("Keypad4x4"))
    {
        referenceY = 56.0;
        captionWidth = 158.0;
    }
    else if (m_definition.id == QStringLiteral("LCD16x2") ||
             m_definition.id == QStringLiteral("ADC") || m_definition.id == QStringLiteral("DAC"))
    {
        referenceY = 48.0;
        captionWidth = 170.0;
    }

    painter->drawText(QRectF(-captionWidth / 2.0, referenceY, captionWidth, 16.0),
                      Qt::AlignCenter,
                      m_reference);

    referenceFont.setPointSize(7);
    referenceFont.setBold(false);
    painter->setFont(referenceFont);
    const QString displayValue = runtimeText();
    const int maximumCharacters = captionWidth > 150.0 ? 46 : 30;
    const QString shortValue = displayValue.size() > maximumCharacters
                                   ? displayValue.left(maximumCharacters - 1) + QChar(0x2026)
                                   : displayValue;
    painter->drawText(QRectF(-captionWidth / 2.0, referenceY + 13.0, captionWidth, 14.0),
                      Qt::AlignCenter,
                      shortValue);
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

QString ComponentItem::runtimeText() const
{
    return m_component ? m_component->runtimeText() : m_value;
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
    dialog.setMinimumWidth(440);
    QFormLayout layout(&dialog);

    auto *typeLabel = new QLabel(m_definition.displayName, &dialog);
    auto *referenceEdit = new QLineEdit(m_reference, &dialog);
    auto *pinLabel = new QLabel(&dialog);
    QStringList pinNames;
    for (const PinModel &pin : m_pins)
        pinNames.append(pin.name);
    pinLabel->setText(tr("%1 pin(s): %2").arg(m_pins.size()).arg(pinNames.join(QStringLiteral(", "))));
    pinLabel->setWordWrap(true);

    layout.addRow(tr("Type"), typeLabel);
    layout.addRow(tr("Reference"), referenceEdit);
    layout.addRow(tr("Pins"), pinLabel);

    QHash<QString, QWidget *> editors;
    QHash<QString, ComponentPropertyType> editorTypes;
    const QVector<ComponentProperty> properties = m_component->editableProperties();
    for (const ComponentProperty &property : properties)
    {
        QWidget *editor = nullptr;
        if (property.type == ComponentPropertyType::Number)
        {
            auto *spin = new QDoubleSpinBox(&dialog);
            spin->setDecimals(12);
            spin->setRange(property.minimum, property.maximum);
            spin->setSingleStep(property.step);
            spin->setValue(property.value.toDouble());
            spin->setSuffix(property.suffix);
            spin->setKeyboardTracking(false);
            editor = spin;
        }
        else if (property.type == ComponentPropertyType::Integer)
        {
            auto *spin = new QSpinBox(&dialog);
            spin->setRange(static_cast<int>(property.minimum), static_cast<int>(property.maximum));
            spin->setSingleStep(static_cast<int>(property.step));
            spin->setValue(property.value.toInt());
            spin->setSuffix(property.suffix);
            spin->setKeyboardTracking(false);
            editor = spin;
        }
        else if (property.type == ComponentPropertyType::Boolean)
        {
            auto *check = new QCheckBox(&dialog);
            check->setChecked(property.value.toBool());
            editor = check;
        }
        else if (property.type == ComponentPropertyType::FilePath)
        {
            auto *container = new QWidget(&dialog);
            auto *row = new QHBoxLayout(container);
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(6);

            auto *pathEdit = new QLineEdit(property.value.toString(), container);
            pathEdit->setObjectName(QStringLiteral("PathEdit"));
            pathEdit->setPlaceholderText(tr("Select an Intel HEX firmware file"));
            auto *browseButton = new QPushButton(tr("Browse..."), container);
            connect(browseButton, &QPushButton::clicked, &dialog, [pathEdit, &dialog]()
                    {
                        const QString initialDirectory = QFileInfo(pathEdit->text()).absolutePath();
                        const QString selected = QFileDialog::getOpenFileName(
                            &dialog,
                            QObject::tr("Choose firmware"),
                            initialDirectory,
                            QObject::tr("Intel HEX files (*.hex *.ihx);;All files (*.*)"));
                        if (!selected.isEmpty())
                            pathEdit->setText(selected);
                    });
            row->addWidget(pathEdit, 1);
            row->addWidget(browseButton);
            editor = container;
        }
        else if (property.type == ComponentPropertyType::Choice)
        {
            auto *combo = new QComboBox(&dialog);
            combo->addItems(property.choices);
            const int index = combo->findText(property.value.toString());
            if (index >= 0)
                combo->setCurrentIndex(index);
            editor = combo;
        }
        else
        {
            editor = new QLineEdit(property.value.toString(), &dialog);
        }
        editors.insert(property.key, editor);
        editorTypes.insert(property.key, property.type);
        layout.addRow(property.label, editor);
    }

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString newReference = referenceEdit->text().trimmed();
    if (!newReference.isEmpty())
        m_reference = newReference;

    for (const ComponentProperty &property : properties)
    {
        QWidget *editor = editors.value(property.key);
        if (editor)
            m_component->setProperty(property.key, editorValue(editor, editorTypes.value(property.key)));
    }

    refreshPins(true);
    syncValueText();
    update();
    emit edited();
    emit stateChanged();
}

Component *ComponentItem::componentModel()
{
    return m_component.get();
}

const Component *ComponentItem::componentModel() const
{
    return m_component.get();
}

bool ComponentItem::setComponentProperty(const QString &key, const QVariant &value)
{
    if (!m_component || !m_component->setProperty(key, value))
        return false;

    refreshPins(false);
    syncValueText();
    update();
    return true;
}

ComponentStepResult ComponentItem::updateSimulation(const QVector<std::optional<double>> &pinVoltages,
                                                    double timeSeconds)
{
    ComponentStepResult result = m_component->step(pinVoltages, timeSeconds);
    for (const QString &warning : result.warnings)
        emit simulationWarning(warning);
    syncValueText();
    update();
    return result;
}

QVariantMap ComponentItem::componentState() const
{
    return m_component->saveState();
}

void ComponentItem::restoreComponentState(const QVariantMap &state)
{
    m_component->loadState(state);
    syncValueText();
    update();
    emit stateChanged();
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

void ComponentItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_pressScenePosition = event->scenePos();
        if (m_definition.id == QStringLiteral("Keypad4x4"))
        {
            const QRectF keysRect(-42.0, -44.0, 84.0, 88.0);
            if (keysRect.contains(event->pos()))
            {
                const int column = std::clamp(
                    static_cast<int>((event->pos().x() - keysRect.left()) / (keysRect.width() / 4.0)),
                    0,
                    3);
                const int row = std::clamp(
                    static_cast<int>((event->pos().y() - keysRect.top()) / (keysRect.height() / 4.0)),
                    0,
                    3);
                m_interactionPressed = true;
                m_component->pressKey(row, column);
                setSelected(true);
                syncValueText();
                update();
                emit stateChanged();
                emit edited();
                event->accept();
                return;
            }
        }
        else if (m_definition.id == QStringLiteral("PushButton"))
        {
            m_interactionPressed = true;
            m_component->pointerPressed();
            syncValueText();
            update();
            emit stateChanged();
            emit edited();
        }
        else if (m_definition.id == QStringLiteral("Switch"))
        {
            m_interactionPressed = true;
        }
    }
    QGraphicsObject::mousePressEvent(event);
}

void ComponentItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_interactionPressed &&
        m_definition.id == QStringLiteral("Keypad4x4"))
    {
        m_component->releaseKey();
        m_interactionPressed = false;
        syncValueText();
        update();
        emit stateChanged();
        emit edited();
        event->accept();
        return;
    }

    QGraphicsObject::mouseReleaseEvent(event);

    if (event->button() != Qt::LeftButton || !m_interactionPressed)
        return;

    if (m_definition.id == QStringLiteral("PushButton"))
    {
        m_component->pointerReleased();
        syncValueText();
        update();
        emit stateChanged();
        emit edited();
    }
    else if (m_definition.id == QStringLiteral("Switch") &&
             QLineF(m_pressScenePosition, event->scenePos()).length() <= 4.0)
    {
        m_component->activate();
        syncValueText();
        update();
        emit stateChanged();
        emit edited();
    }

    m_interactionPressed = false;
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
    QAction *toggleAction = nullptr;
    if (m_definition.id == QStringLiteral("Switch"))
        toggleAction = menu.addAction(m_component->active() ? tr("Open switch") : tr("Close switch"));
    menu.addSeparator();
    QAction *rotateAction = menu.addAction(tr("Rotate 90 degrees"));
    QAction *mirrorHorizontalAction = menu.addAction(tr("Mirror horizontally"));
    QAction *mirrorVerticalAction = menu.addAction(tr("Mirror vertically"));
    menu.addSeparator();
    QAction *deleteAction = menu.addAction(tr("Delete component"));

    QAction *chosen = menu.exec(event->screenPos());
    if (chosen == propertiesAction)
        openProperties();
    else if (toggleAction && chosen == toggleAction)
    {
        m_component->activate();
        syncValueText();
        update();
        emit stateChanged();
        emit edited();
    }
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
    const std::unique_ptr<Component> model = ComponentFactory::create(type);
    const QVector<ComponentPinDefinition> definitions = model->pinDefinitions();
    const QVector<QPointF> positions = pinPositionsForType(type, definitions.size());
    QVector<PinModel> result;
    result.reserve(definitions.size());
    for (int i = 0; i < definitions.size(); ++i)
        result.append(makePin(definitions.at(i).name, positions.value(i), definitions.at(i).direction));
    return result;
}

QVector<PinModel> ComponentItem::pinsForModel() const
{
    if (!m_component)
        return createPins(m_definition.id);

    const QVector<ComponentPinDefinition> definitions = m_component->pinDefinitions();
    const QVector<QPointF> positions = pinPositionsForType(m_definition.id, definitions.size());
    QVector<PinModel> result;
    result.reserve(definitions.size());
    for (int i = 0; i < definitions.size(); ++i)
        result.append(makePin(definitions.at(i).name, positions.value(i), definitions.at(i).direction));
    return result;
}

void ComponentItem::refreshPins(bool notify)
{
    const QVector<PinModel> updatedPins = pinsForModel();
    bool changed = updatedPins.size() != m_pins.size();
    if (!changed)
    {
        for (int i = 0; i < updatedPins.size(); ++i)
        {
            if (updatedPins.at(i).name != m_pins.at(i).name ||
                updatedPins.at(i).direction != m_pins.at(i).direction ||
                updatedPins.at(i).localPosition != m_pins.at(i).localPosition)
            {
                changed = true;
                break;
            }
        }
    }

    if (!changed)
        return;

    prepareGeometryChange();
    m_pins = updatedPins;
    m_highlightedPin = -1;
    update();
    emit geometryChanged();
    if (notify)
        emit pinsChanged();
}

QVector<QPointF> ComponentItem::pinPositionsForType(const QString &type, int count)
{
    if (type == QStringLiteral("Ground") || type == QStringLiteral("VoltageProbe"))
        return {QPointF(0.0, -34.0)};
    if (type == QStringLiteral("Voltage") || type == QStringLiteral("Battery") ||
        type == QStringLiteral("Clock") || type == QStringLiteral("Current"))
        return {QPointF(0.0, -42.0), QPointF(0.0, 42.0)};
    if (type == QStringLiteral("PushButton"))
        return {QPointF(48.0, 0.0)};
    if (type == QStringLiteral("NOT"))
        return {QPointF(-52.0, 0.0), QPointF(52.0, 0.0)};
    if (type == QStringLiteral("AND") || type == QStringLiteral("OR") || type == QStringLiteral("NAND") ||
        type == QStringLiteral("XOR"))
    {
        QVector<QPointF> result;
        const int inputCount = std::max(2, count - 1);
        for (int i = 0; i < inputCount; ++i)
        {
            const qreal y = inputCount == 1
                                ? 0.0
                                : -26.0 + 52.0 * static_cast<qreal>(i) / static_cast<qreal>(inputCount - 1);
            result.append(QPointF(-54.0, y));
        }
        result.append(QPointF(54.0, 0.0));
        return result;
    }
    if (type == QStringLiteral("DFlipFlop"))
        return {QPointF(-58.0, -20.0), QPointF(-58.0, 20.0), QPointF(58.0, -20.0), QPointF(58.0, 20.0)};
    if (type == QStringLiteral("SevenSegment"))
    {
        QVector<QPointF> result;
        const int segmentCount = std::max(0, count - 1);
        for (int i = 0; i < segmentCount; ++i)
        {
            const qreal y = segmentCount == 1
                                ? 0.0
                                : -35.0 + 70.0 * static_cast<qreal>(i) / static_cast<qreal>(segmentCount - 1);
            result.append(QPointF(-60.0, y));
        }
        result.append(QPointF(60.0, 35.0));
        return result;
    }
    if (type == QStringLiteral("ADC"))
    {
        QVector<QPointF> result{QPointF(-70.0, -30.0),
                                QPointF(-70.0, 0.0),
                                QPointF(-70.0, 30.0)};
        const int outputCount = std::max(0, count - 3);
        for (int bit = 0; bit < outputCount; ++bit)
        {
            const qreal y = outputCount == 1
                                ? 0.0
                                : -42.0 + 84.0 * static_cast<qreal>(bit) /
                                              static_cast<qreal>(outputCount - 1);
            result.append(QPointF(70.0, y));
        }
        return result;
    }
    if (type == QStringLiteral("DAC"))
    {
        QVector<QPointF> result;
        const int inputCount = std::max(0, count - 3);
        for (int bit = 0; bit < inputCount; ++bit)
        {
            const qreal y = inputCount == 1
                                ? 0.0
                                : -42.0 + 84.0 * static_cast<qreal>(bit) /
                                              static_cast<qreal>(inputCount - 1);
            result.append(QPointF(-70.0, y));
        }
        result.append(QPointF(-20.0, 50.0));
        result.append(QPointF(-20.0, -50.0));
        result.append(QPointF(70.0, 0.0));
        return result;
    }
    if (type == QStringLiteral("MCU"))
    {
        QVector<QPointF> result;
        for (int bit = 0; bit < 8; ++bit)
            result.append(QPointF(-88.0, -56.0 + bit * 16.0));
        for (int bit = 0; bit < 8; ++bit)
            result.append(QPointF(88.0, -56.0 + bit * 16.0));
        for (int bit = 0; bit < 8; ++bit)
            result.append(QPointF(-56.0 + bit * 16.0, 70.0));
        result.append(QPointF(-24.0, -70.0));
        result.append(QPointF(24.0, -70.0));
        return result;
    }
    if (type == QStringLiteral("EEPROM"))
    {
        QVector<QPointF> result;
        for (int bit = 0; bit < 8; ++bit)
            result.append(QPointF(-76.0, -49.0 + bit * 14.0));
        for (int bit = 0; bit < 8; ++bit)
            result.append(QPointF(76.0, -49.0 + bit * 14.0));
        result.append(QPointF(-36.0, 60.0));
        result.append(QPointF(0.0, 60.0));
        result.append(QPointF(36.0, 60.0));
        return result;
    }
    if (type == QStringLiteral("LCD16x2"))
    {
        QVector<QPointF> result{QPointF(-92.0, -24.0),
                                QPointF(-92.0, 0.0),
                                QPointF(-92.0, 24.0)};
        const int busCount = std::max(0, count - 3);
        for (int bit = 0; bit < busCount; ++bit)
        {
            const qreal x = busCount == 1
                                ? 0.0
                                : -70.0 + 140.0 * static_cast<qreal>(bit) /
                                               static_cast<qreal>(busCount - 1);
            result.append(QPointF(x, 56.0));
        }
        return result;
    }
    if (type == QStringLiteral("Keypad4x4"))
        return {QPointF(-72.0, -36.0),
                QPointF(-72.0, -12.0),
                QPointF(-72.0, 12.0),
                QPointF(-72.0, 36.0),
                QPointF(72.0, -36.0),
                QPointF(72.0, -12.0),
                QPointF(72.0, 12.0),
                QPointF(72.0, 36.0)};
    if (type == QStringLiteral("Oscilloscope"))
        return {QPointF(-62.0, -20.0), QPointF(-62.0, 20.0), QPointF(62.0, 40.0)};
    if (type == QStringLiteral("Potentiometer"))
        return {QPointF(-48.0, 0.0), QPointF(0.0, -42.0), QPointF(48.0, 0.0)};
    if (count == 1)
        return {QPointF(0.0, -34.0)};
    if (count == 2)
        return {QPointF(-48.0, 0.0), QPointF(48.0, 0.0)};

    QVector<QPointF> result;
    const int leftCount = (count + 1) / 2;
    const int rightCount = count - leftCount;
    for (int i = 0; i < leftCount; ++i)
        result.append(QPointF(-58.0, (i - (leftCount - 1) / 2.0) * 20.0));
    for (int i = 0; i < rightCount; ++i)
        result.append(QPointF(58.0, (i - (rightCount - 1) / 2.0) * 20.0));
    return result;
}

QString ComponentItem::referencePrefix(const QString &type)
{
    static const QHash<QString, QString> prefixes{{QStringLiteral("Resistor"), QStringLiteral("R")},
                                                  {QStringLiteral("Capacitor"), QStringLiteral("C")},
                                                  {QStringLiteral("Inductor"), QStringLiteral("L")},
                                                  {QStringLiteral("Potentiometer"), QStringLiteral("RV")},
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
    if (type == QStringLiteral("AND") || type == QStringLiteral("OR") || type == QStringLiteral("NOT") ||
        type == QStringLiteral("NAND") || type == QStringLiteral("XOR"))
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

void ComponentItem::syncValueText()
{
    m_value = m_component ? m_component->valueText() : m_definition.defaultValue;
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

    if (type == QStringLiteral("Potentiometer"))
    {
        painter->drawLine(QPointF(-48.0, 0.0), QPointF(-34.0, 0.0));
        QPainterPath path(QPointF(-34.0, 0.0));
        for (int i = 0; i < 8; ++i)
            path.lineTo(-30.0 + i * 8.0, i % 2 == 0 ? -10.0 : 10.0);
        path.lineTo(34.0, 0.0);
        painter->drawPath(path);
        painter->drawLine(QPointF(34.0, 0.0), QPointF(48.0, 0.0));
        painter->drawLine(QPointF(0.0, -42.0), QPointF(0.0, -17.0));
        painter->drawLine(QPointF(0.0, -17.0), QPointF(-6.0, -24.0));
        painter->drawLine(QPointF(0.0, -17.0), QPointF(6.0, -24.0));
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

    if (type == QStringLiteral("Voltage"))
    {
        drawSource(painter, QStringLiteral("V"));
        return;
    }

    if (type == QStringLiteral("Battery"))
    {
        painter->drawLine(QPointF(0.0, -42.0), QPointF(0.0, -18.0));
        painter->drawLine(QPointF(-24.0, -18.0), QPointF(24.0, -18.0));
        painter->drawLine(QPointF(-14.0, -6.0), QPointF(14.0, -6.0));
        painter->drawLine(QPointF(-24.0, 6.0), QPointF(24.0, 6.0));
        painter->drawLine(QPointF(-14.0, 18.0), QPointF(14.0, 18.0));
        painter->drawLine(QPointF(0.0, 18.0), QPointF(0.0, 42.0));
        return;
    }

    if (type == QStringLiteral("Clock"))
    {
        drawSource(painter, QStringLiteral("CLK"));
        return;
    }

    if (type == QStringLiteral("Current"))
    {
        drawSource(painter, QStringLiteral("I"));
        return;
    }

    if (type == QStringLiteral("Switch"))
    {
        drawSwitch(painter, m_component->active());
        return;
    }

    if (type == QStringLiteral("PushButton"))
    {
        drawPushButton(painter, m_component->active());
        return;
    }

    if (type.startsWith(QStringLiteral("LED")))
    {
        drawLed(painter);
        return;
    }

    if (type == QStringLiteral("SevenSegment"))
    {
        drawSevenSegment(painter);
        return;
    }

    if (type == QStringLiteral("AND") || type == QStringLiteral("OR") || type == QStringLiteral("NOT") ||
        type == QStringLiteral("NAND") || type == QStringLiteral("XOR"))
    {
        drawLogicGate(painter);
        return;
    }

    if (type == QStringLiteral("ADC") || type == QStringLiteral("DAC"))
    {
        drawConverter(painter);
        return;
    }
    if (type == QStringLiteral("MCU"))
    {
        drawMicrocontroller(painter);
        return;
    }
    if (type == QStringLiteral("EEPROM"))
    {
        drawExternalMemory(painter);
        return;
    }
    if (type == QStringLiteral("LCD16x2"))
    {
        drawLcd(painter);
        return;
    }
    if (type == QStringLiteral("Keypad4x4"))
    {
        drawKeypad(painter);
        return;
    }

    drawGenericBody(painter);
}

void ComponentItem::drawSource(QPainter *painter, const QString &label) const
{
    painter->drawLine(QPointF(0.0, -42.0), QPointF(0.0, -24.0));
    painter->drawEllipse(QPointF(0.0, 0.0), 24.0, 24.0);
    painter->drawLine(QPointF(0.0, 24.0), QPointF(0.0, 42.0));
    if (label == QStringLiteral("CLK"))
    {
        QPainterPath wave(QPointF(-14.0, 7.0));
        wave.lineTo(-14.0, -7.0);
        wave.lineTo(0.0, -7.0);
        wave.lineTo(0.0, 7.0);
        wave.lineTo(14.0, 7.0);
        wave.lineTo(14.0, -7.0);
        painter->drawPath(wave);
    }
    else
    {
        painter->drawText(QRectF(-20.0, -10.0, 40.0, 20.0), Qt::AlignCenter, label);
    }
}

void ComponentItem::drawSwitch(QPainter *painter, bool closed) const
{
    painter->drawLine(QPointF(-48.0, 0.0), QPointF(-24.0, 0.0));
    painter->drawLine(QPointF(24.0, 0.0), QPointF(48.0, 0.0));
    painter->drawEllipse(QPointF(-24.0, 0.0), 3.0, 3.0);
    painter->drawEllipse(QPointF(24.0, 0.0), 3.0, 3.0);
    painter->drawLine(QPointF(-21.0, -2.0), closed ? QPointF(21.0, -2.0) : QPointF(19.0, -18.0));
}

void ComponentItem::drawPushButton(QPainter *painter, bool pressed) const
{
    painter->setBrush(pressed ? QColor(191, 219, 254) : QColor(248, 250, 252));
    painter->drawRoundedRect(QRectF(-30.0, -24.0, 60.0, 48.0), 8.0, 8.0);
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(30.0, 0.0), QPointF(48.0, 0.0));
    painter->drawEllipse(QPointF(0.0, pressed ? 4.0 : -2.0), 11.0, 7.0);
    painter->drawLine(QPointF(0.0, -24.0), QPointF(0.0, pressed ? -3.0 : -9.0));
}

void ComponentItem::drawLed(QPainter *painter) const
{
    const QColor color = m_component->indicatorColor();
    painter->drawLine(QPointF(-48.0, 0.0), QPointF(-18.0, 0.0));
    QPolygonF triangle;
    triangle << QPointF(-18.0, -18.0) << QPointF(-18.0, 18.0) << QPointF(14.0, 0.0);
    painter->setBrush(m_component->active() ? color : faded(color));
    painter->drawPolygon(triangle);
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(14.0, -20.0), QPointF(14.0, 20.0));
    painter->drawLine(QPointF(14.0, 0.0), QPointF(48.0, 0.0));
    painter->drawLine(QPointF(18.0, -22.0), QPointF(32.0, -34.0));
    painter->drawLine(QPointF(26.0, -18.0), QPointF(40.0, -30.0));
}

void ComponentItem::drawSevenSegment(QPainter *painter) const
{
    const QRectF body(-42.0, -48.0, 84.0, 88.0);
    painter->setBrush(QColor(15, 23, 42));
    painter->setPen(QPen(QColor(51, 65, 85), 2.0));
    painter->drawRoundedRect(body, 7.0, 7.0);

    const QVector<bool> segments = m_component->segmentStates();
    const QColor onColor = m_component->indicatorColor();
    const QColor offColor = faded(onColor);
    const QVector<QLineF> lines{QLineF(-22.0, -34.0, 22.0, -34.0),
                                QLineF(27.0, -29.0, 27.0, -3.0),
                                QLineF(27.0, 3.0, 27.0, 29.0),
                                QLineF(-22.0, 34.0, 22.0, 34.0),
                                QLineF(-27.0, 3.0, -27.0, 29.0),
                                QLineF(-27.0, -29.0, -27.0, -3.0),
                                QLineF(-22.0, 0.0, 22.0, 0.0)};
    for (int i = 0; i < lines.size(); ++i)
    {
        painter->setPen(QPen(segments.value(i) ? onColor : offColor, 6.0, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(lines[i]);
    }
    painter->setPen(Qt::NoPen);
    painter->setBrush(segments.value(7) ? onColor : offColor);
    painter->drawEllipse(QPointF(34.0, 34.0), 3.5, 3.5);
    painter->setBrush(Qt::NoBrush);
}

void ComponentItem::drawLogicGate(QPainter *painter) const
{
    const QString type = m_definition.id;
    painter->setBrush(QColor(248, 250, 252));

    if (type == QStringLiteral("NOT"))
    {
        QPolygonF triangle;
        triangle << QPointF(-32.0, -28.0) << QPointF(-32.0, 28.0) << QPointF(28.0, 0.0);
        painter->drawPolygon(triangle);
        painter->setBrush(QColor(248, 250, 252));
        painter->drawEllipse(QPointF(34.0, 0.0), 6.0, 6.0);
        painter->drawLine(QPointF(-52.0, 0.0), QPointF(-32.0, 0.0));
        painter->drawLine(QPointF(40.0, 0.0), QPointF(52.0, 0.0));
        return;
    }

    QPainterPath body;
    if (type == QStringLiteral("AND") || type == QStringLiteral("NAND"))
    {
        body.moveTo(-34.0, -30.0);
        body.lineTo(0.0, -30.0);
        body.arcTo(QRectF(-2.0, -30.0, 64.0, 60.0), 90.0, -180.0);
        body.lineTo(-34.0, 30.0);
        body.closeSubpath();
    }
    else
    {
        body.moveTo(-36.0, -30.0);
        body.cubicTo(-12.0, -28.0, 18.0, -28.0, 38.0, 0.0);
        body.cubicTo(18.0, 28.0, -12.0, 28.0, -36.0, 30.0);
        body.cubicTo(-20.0, 12.0, -20.0, -12.0, -36.0, -30.0);
        body.closeSubpath();
        if (type == QStringLiteral("XOR"))
        {
            QPainterPath extra;
            extra.moveTo(-44.0, -30.0);
            extra.cubicTo(-28.0, -12.0, -28.0, 12.0, -44.0, 30.0);
            painter->drawPath(extra);
        }
    }
    painter->drawPath(body);
    const int outputIndex = m_pins.size() - 1;
    for (int i = 0; i < outputIndex; ++i)
        painter->drawLine(m_pins.at(i).localPosition, QPointF(-32.0, m_pins.at(i).localPosition.y()));

    if (type == QStringLiteral("NAND"))
    {
        painter->drawEllipse(QPointF(43.0, 0.0), 6.0, 6.0);
        painter->drawLine(QPointF(49.0, 0.0), QPointF(54.0, 0.0));
    }
    else
    {
        painter->drawLine(QPointF(38.0, 0.0), QPointF(54.0, 0.0));
    }
}

void ComponentItem::drawConverter(QPainter *painter) const
{
    const QRectF body(-52.0, -48.0, 104.0, 96.0);
    painter->save();
    painter->setBrush(QColor(248, 250, 252));
    painter->setPen(QPen(QColor(30, 64, 175), 1.8));
    painter->drawRoundedRect(body, 7.0, 7.0);

    QFont titleFont = painter->font();
    titleFont.setBold(true);
    titleFont.setPointSize(11);
    painter->setFont(titleFont);
    painter->setPen(QColor(15, 23, 42));
    painter->drawText(QRectF(-45.0, -21.0, 90.0, 22.0),
                      Qt::AlignCenter,
                      m_definition.id);

    QFont detailFont = titleFont;
    detailFont.setBold(false);
    detailFont.setPointSize(7);
    painter->setFont(detailFont);
    painter->setPen(QColor(71, 85, 105));
    painter->drawText(QRectF(-45.0, 3.0, 90.0, 24.0),
                      Qt::AlignCenter | Qt::TextWordWrap,
                      m_component ? m_component->valueText() : m_value);

    painter->setPen(QPen(QColor(30, 64, 175), 1.3));
    for (const PinModel &pin : m_pins)
    {
        const QPointF point = pin.localPosition;
        QPointF inner = point;
        if (point.x() < body.left())
            inner.setX(body.left());
        else if (point.x() > body.right())
            inner.setX(body.right());
        else if (point.y() < body.top())
            inner.setY(body.top());
        else if (point.y() > body.bottom())
            inner.setY(body.bottom());
        painter->drawLine(inner, point);
    }
    painter->restore();
}

void ComponentItem::drawMicrocontroller(QPainter *painter) const
{
    const QRectF body(-64.0, -58.0, 128.0, 116.0);
    painter->save();
    painter->setPen(QPen(QColor(30, 64, 175), 1.8));
    painter->setBrush(QColor(241, 245, 249));
    painter->drawRoundedRect(body, 6.0, 6.0);

    QFont titleFont = painter->font();
    titleFont.setBold(true);
    titleFont.setPointSize(10);
    painter->setFont(titleFont);
    painter->setPen(QColor(15, 23, 42));
    painter->drawText(QRectF(-54.0, -20.0, 108.0, 22.0),
                      Qt::AlignCenter,
                      QStringLiteral("Educational MCU"));

    QFont detailFont = titleFont;
    detailFont.setBold(false);
    detailFont.setPointSize(7);
    painter->setFont(detailFont);
    painter->setPen(QColor(71, 85, 105));
    painter->drawText(QRectF(-54.0, 5.0, 108.0, 34.0),
                      Qt::AlignCenter | Qt::TextWordWrap,
                      m_component ? m_component->valueText() : m_value);

    painter->setPen(QPen(QColor(30, 64, 175), 1.2));
    for (const PinModel &pin : m_pins)
    {
        const QPointF point = pin.localPosition;
        QPointF inner = point;
        if (point.x() < body.left())
            inner.setX(body.left());
        else if (point.x() > body.right())
            inner.setX(body.right());
        else if (point.y() < body.top())
            inner.setY(body.top());
        else
            inner.setY(body.bottom());
        painter->drawLine(inner, point);
    }
    painter->restore();
}

void ComponentItem::drawExternalMemory(QPainter *painter) const
{
    const QRectF body(-54.0, -52.0, 108.0, 104.0);
    painter->save();
    painter->setBrush(QColor(248, 250, 252));
    painter->setPen(QPen(QColor(30, 64, 175), 1.8));
    painter->drawRoundedRect(body, 6.0, 6.0);

    QFont titleFont = painter->font();
    titleFont.setBold(true);
    titleFont.setPointSize(10);
    painter->setFont(titleFont);
    painter->setPen(QColor(15, 23, 42));
    painter->drawText(QRectF(-47.0, -18.0, 94.0, 20.0), Qt::AlignCenter,
                      QStringLiteral("RAM / EEPROM"));

    QFont detailFont = titleFont;
    detailFont.setBold(false);
    detailFont.setPointSize(7);
    painter->setFont(detailFont);
    painter->setPen(QColor(71, 85, 105));
    painter->drawText(QRectF(-47.0, 5.0, 94.0, 30.0),
                      Qt::AlignCenter | Qt::TextWordWrap,
                      m_component ? m_component->valueText() : m_value);

    painter->setPen(QPen(QColor(30, 64, 175), 1.2));
    for (const PinModel &pin : m_pins)
    {
        const QPointF point = pin.localPosition;
        QPointF inner = point;
        if (point.x() < body.left())
            inner.setX(body.left());
        else if (point.x() > body.right())
            inner.setX(body.right());
        else
            inner.setY(body.bottom());
        painter->drawLine(inner, point);
    }
    painter->restore();
}

void ComponentItem::drawLcd(QPainter *painter) const
{
    const QRectF body(-80.0, -44.0, 160.0, 82.0);
    const QRectF screen(-68.0, -31.0, 136.0, 50.0);
    painter->save();
    painter->setPen(QPen(QColor(30, 64, 175), 1.8));
    painter->setBrush(QColor(226, 232, 240));
    painter->drawRoundedRect(body, 6.0, 6.0);

    painter->setPen(QPen(QColor(20, 83, 45), 1.2));
    painter->setBrush(m_component && m_component->active() ? QColor(187, 247, 208)
                                                            : QColor(203, 213, 225));
    painter->drawRoundedRect(screen, 3.0, 3.0);

    QStringList lines = m_component ? m_component->displayLines() : QStringList{};
    while (lines.size() < 2)
        lines.append(QString());
    QFont lcdFont(QStringLiteral("Consolas"));
    lcdFont.setStyleHint(QFont::Monospace);
    lcdFont.setPointSize(8);
    painter->setFont(lcdFont);
    painter->setPen(QColor(20, 83, 45));
    painter->drawText(QRectF(-62.0, -25.0, 124.0, 18.0),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      lines.value(0).left(16));
    painter->drawText(QRectF(-62.0, -5.0, 124.0, 18.0),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      lines.value(1).left(16));

    painter->setPen(QPen(QColor(30, 64, 175), 1.2));
    for (const PinModel &pin : m_pins)
    {
        const QPointF point = pin.localPosition;
        QPointF inner = point;
        if (point.x() < body.left())
            inner.setX(body.left());
        else
            inner.setY(body.bottom());
        painter->drawLine(inner, point);
    }
    painter->restore();
}

void ComponentItem::drawKeypad(QPainter *painter) const
{
    const QRectF body(-50.0, -50.0, 100.0, 100.0);
    const QRectF keysRect(-42.0, -44.0, 84.0, 88.0);
    static const QStringList labels{QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
                                    QStringLiteral("A"), QStringLiteral("4"), QStringLiteral("5"),
                                    QStringLiteral("6"), QStringLiteral("B"), QStringLiteral("7"),
                                    QStringLiteral("8"), QStringLiteral("9"), QStringLiteral("C"),
                                    QStringLiteral("*"), QStringLiteral("0"), QStringLiteral("#"),
                                    QStringLiteral("D")};

    painter->save();
    painter->setPen(QPen(QColor(30, 64, 175), 1.8));
    painter->setBrush(QColor(241, 245, 249));
    painter->drawRoundedRect(body, 6.0, 6.0);

    const QVector<bool> states = m_component ? m_component->keyStates() : QVector<bool>{};
    QFont buttonFont = painter->font();
    buttonFont.setPointSize(8);
    buttonFont.setBold(true);
    painter->setFont(buttonFont);
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            const int index = row * 4 + column;
            QRectF key(keysRect.left() + column * keysRect.width() / 4.0 + 2.0,
                       keysRect.top() + row * keysRect.height() / 4.0 + 2.0,
                       keysRect.width() / 4.0 - 4.0,
                       keysRect.height() / 4.0 - 4.0);
            painter->setPen(QPen(QColor(71, 85, 105), 1.0));
            painter->setBrush(states.value(index) ? QColor(191, 219, 254) : QColor(255, 255, 255));
            painter->drawRoundedRect(key, 3.0, 3.0);
            painter->setPen(QColor(15, 23, 42));
            painter->drawText(key, Qt::AlignCenter, labels.value(index));
        }
    }

    painter->setPen(QPen(QColor(30, 64, 175), 1.2));
    for (const PinModel &pin : m_pins)
    {
        QPointF inner = pin.localPosition;
        inner.setX(pin.localPosition.x() < 0.0 ? body.left() : body.right());
        painter->drawLine(inner, pin.localPosition);
    }
    painter->restore();
}

void ComponentItem::drawGenericBody(QPainter *painter) const
{
    const QString type = m_definition.id;
    QRectF body(-38.0, -30.0, 76.0, 60.0);

    painter->setBrush(QColor(248, 250, 252));
    painter->drawRoundedRect(body, 7.0, 7.0);
    painter->setBrush(Qt::NoBrush);

    QString label = m_definition.displayName;
    if (type == QStringLiteral("DFlipFlop"))
        label = QStringLiteral("D Flip-Flop\nQ on rising CLK");
    painter->drawText(body.adjusted(5.0, 5.0, -5.0, -5.0), Qt::AlignCenter | Qt::TextWordWrap, label);

    for (const PinModel &pin : m_pins)
    {
        const QPointF point = pin.localPosition;
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

    for (const PinModel &pin : m_pins)
    {
        const QPointF point = pin.localPosition;
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
        painter->drawText(textRect, alignment, pin.name);
    }
}
