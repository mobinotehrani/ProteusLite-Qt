#include "basiccomponent.h"

#include <algorithm>
#include <cmath>

namespace
{
QString compactNumber(double value, int precision = 6)
{
    return QString::number(value, 'g', precision);
}

QString measurementText(const std::optional<double> &voltage, const std::optional<double> &current)
{
    if (!voltage.has_value() || !current.has_value())
        return QStringLiteral("not measured");
    return QStringLiteral("V=%1 V, I=%2 A").arg(compactNumber(*voltage), compactNumber(*current));
}

QVector<ComponentPinDefinition> twoPassivePins()
{
    return {{QStringLiteral("A"), PinDirection::Passive}, {QStringLiteral("B"), PinDirection::Passive}};
}

std::optional<double> pinVoltage(const QVector<std::optional<double>> &voltages, int index)
{
    if (index < 0 || index >= voltages.size())
        return std::nullopt;
    return voltages.at(index);
}

LogicLevel invertLevel(LogicLevel level)
{
    if (level == LogicLevel::Low)
        return LogicLevel::High;
    if (level == LogicLevel::High)
        return LogicLevel::Low;
    return LogicLevel::Undefined;
}

void appendFloatingWarning(ComponentStepResult &result)
{
    const QString warning = QStringLiteral("Floating input detected.");
    if (!result.warnings.contains(warning))
        result.warnings.append(warning);
}

ComponentStepResult referencedSourceResult(double voltageDifference,
                                           const QVector<std::optional<double>> &pinVoltages)
{
    ComponentStepResult result;
    result.pinVoltages.resize(2);
    result.drivenPins.fill(false, 2);
    const std::optional<double> output = pinVoltage(pinVoltages, 0);
    const std::optional<double> reference = pinVoltage(pinVoltages, 1);

    if (reference.has_value())
    {
        result.pinVoltages[0] = *reference + voltageDifference;
        result.drivenPins[0] = true;
    }
    else if (output.has_value())
    {
        result.pinVoltages[1] = *output - voltageDifference;
        result.drivenPins[1] = true;
    }

    return result;
}
}

double LogicThresholds::lowVoltage()
{
    return 0.0;
}

double LogicThresholds::highVoltage()
{
    return 5.0;
}

double LogicThresholds::lowMaximum()
{
    return 0.8;
}

double LogicThresholds::highMinimum()
{
    return 2.0;
}

LogicLevel LogicThresholds::fromVoltage(const std::optional<double> &voltage)
{
    if (!voltage.has_value() || !std::isfinite(*voltage))
        return LogicLevel::Undefined;
    if (*voltage <= lowMaximum())
        return LogicLevel::Low;
    if (*voltage >= highMinimum())
        return LogicLevel::High;
    return LogicLevel::Undefined;
}

std::optional<double> LogicThresholds::toVoltage(LogicLevel level)
{
    if (level == LogicLevel::Low)
        return lowVoltage();
    if (level == LogicLevel::High)
        return highVoltage();
    return std::nullopt;
}

QString LogicThresholds::text(LogicLevel level)
{
    if (level == LogicLevel::Low)
        return QStringLiteral("LOW");
    if (level == LogicLevel::High)
        return QStringLiteral("HIGH");
    return QStringLiteral("UNDEFINED");
}

QString Component::runtimeText() const
{
    return valueText();
}

ComponentElectricalResult Component::evaluateElectrical(const ComponentElectricalInput &input)
{
    Q_UNUSED(input);
    return {};
}

QVector<QPair<int, int>> Component::conductivePinPairs() const
{
    return {};
}

void Component::activate()
{
}

void Component::pointerPressed()
{
}

void Component::pointerReleased()
{
}

bool Component::active() const
{
    return false;
}

QColor Component::indicatorColor() const
{
    return QColor(148, 163, 184);
}

QVector<bool> Component::segmentStates() const
{
    return {};
}

QVariantMap Component::saveState() const
{
    return {};
}

void Component::loadState(const QVariantMap &state)
{
    Q_UNUSED(state);
}

ComponentProperty Component::numberProperty(const QString &key,
                                            const QString &label,
                                            double value,
                                            double minimum,
                                            double maximum,
                                            double step,
                                            const QString &suffix)
{
    return {key, label, ComponentPropertyType::Number, value, minimum, maximum, step, suffix, {}};
}

ComponentProperty Component::integerProperty(const QString &key,
                                             const QString &label,
                                             int value,
                                             int minimum,
                                             int maximum,
                                             int step,
                                             const QString &suffix)
{
    return {key,
            label,
            ComponentPropertyType::Integer,
            value,
            static_cast<double>(minimum),
            static_cast<double>(maximum),
            static_cast<double>(step),
            suffix,
            {}};
}

ComponentProperty Component::booleanProperty(const QString &key, const QString &label, bool value)
{
    return {key, label, ComponentPropertyType::Boolean, value, 0.0, 1.0, 1.0, {}, {}};
}

ComponentProperty Component::textProperty(const QString &key, const QString &label, const QString &value)
{
    return {key, label, ComponentPropertyType::Text, value, 0.0, 0.0, 1.0, {}, {}};
}

ComponentProperty Component::choiceProperty(const QString &key,
                                            const QString &label,
                                            const QString &value,
                                            const QStringList &choices)
{
    return {key, label, ComponentPropertyType::Choice, value, 0.0, 0.0, 1.0, {}, choices};
}

ComponentStepResult Component::emptyResult(int pinCount)
{
    ComponentStepResult result;
    result.pinVoltages.resize(std::max(0, pinCount));
    result.drivenPins.fill(false, std::max(0, pinCount));
    return result;
}

QString GroundComponent::typeId() const
{
    return QStringLiteral("Ground");
}

QVector<ComponentPinDefinition> GroundComponent::pinDefinitions() const
{
    return {{QStringLiteral("GND"), PinDirection::Power}};
}

QVector<ComponentProperty> GroundComponent::editableProperties() const
{
    return {};
}

bool GroundComponent::setProperty(const QString &key, const QVariant &value)
{
    Q_UNUSED(key);
    Q_UNUSED(value);
    return false;
}

QVariant GroundComponent::property(const QString &key) const
{
    Q_UNUSED(key);
    return {};
}

QString GroundComponent::valueText() const
{
    return QStringLiteral("0 V");
}

ComponentStepResult GroundComponent::step(const QVector<std::optional<double>> &pinVoltages,
                                          double timeSeconds)
{
    Q_UNUSED(pinVoltages);
    Q_UNUSED(timeSeconds);
    ComponentStepResult result = emptyResult(1);
    result.pinVoltages[0] = 0.0;
    result.drivenPins[0] = true;
    return result;
}

ComponentElectricalResult GroundComponent::evaluateElectrical(const ComponentElectricalInput &input)
{
    Q_UNUSED(input);
    ComponentElectricalResult result;
    result.voltageAcross = 0.0;
    return result;
}

QString VoltageSourceComponent::typeId() const
{
    return QStringLiteral("Voltage");
}

QVector<ComponentPinDefinition> VoltageSourceComponent::pinDefinitions() const
{
    return {{QStringLiteral("OUT"), PinDirection::Output}, {QStringLiteral("RET"), PinDirection::Power}};
}

QVector<ComponentProperty> VoltageSourceComponent::editableProperties() const
{
    return {numberProperty(QStringLiteral("voltage"),
                           QStringLiteral("DC voltage"),
                           m_voltage,
                           -1000000.0,
                           1000000.0,
                           0.1,
                           QStringLiteral(" V"))};
}

bool VoltageSourceComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key != QStringLiteral("voltage"))
        return false;
    m_voltage = std::clamp(value.toDouble(), -1000000.0, 1000000.0);
    return true;
}

QVariant VoltageSourceComponent::property(const QString &key) const
{
    return key == QStringLiteral("voltage") ? QVariant(m_voltage) : QVariant();
}

QString VoltageSourceComponent::valueText() const
{
    return compactNumber(m_voltage) + QStringLiteral(" V DC");
}

ComponentStepResult VoltageSourceComponent::step(const QVector<std::optional<double>> &pinVoltages,
                                                 double timeSeconds)
{
    Q_UNUSED(timeSeconds);
    return referencedSourceResult(m_voltage, pinVoltages);
}

ComponentElectricalResult VoltageSourceComponent::evaluateElectrical(const ComponentElectricalInput &input)
{
    Q_UNUSED(input);
    ComponentElectricalResult result;
    result.voltageAcross = m_voltage;
    return result;
}

QString BatteryComponent::typeId() const
{
    return QStringLiteral("Battery");
}

QVector<ComponentPinDefinition> BatteryComponent::pinDefinitions() const
{
    return {{QStringLiteral("OUT"), PinDirection::Output}, {QStringLiteral("RET"), PinDirection::Power}};
}

QVector<ComponentProperty> BatteryComponent::editableProperties() const
{
    return {numberProperty(QStringLiteral("voltage"),
                           QStringLiteral("Nominal voltage"),
                           m_voltage,
                           0.0,
                           1000000.0,
                           0.1,
                           QStringLiteral(" V")),
            numberProperty(QStringLiteral("internalResistance"),
                           QStringLiteral("Internal resistance"),
                           m_internalResistance,
                           0.0,
                           1000000000.0,
                           0.01,
                           QStringLiteral(" ohm"))};
}

bool BatteryComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("voltage"))
    {
        m_voltage = std::clamp(value.toDouble(), 0.0, 1000000.0);
        return true;
    }
    if (key == QStringLiteral("internalResistance"))
    {
        m_internalResistance = std::clamp(value.toDouble(), 0.0, 1000000000.0);
        return true;
    }
    return false;
}

QVariant BatteryComponent::property(const QString &key) const
{
    if (key == QStringLiteral("voltage"))
        return m_voltage;
    if (key == QStringLiteral("internalResistance"))
        return m_internalResistance;
    return {};
}

QString BatteryComponent::valueText() const
{
    return QStringLiteral("%1 V, r=%2 ohm")
        .arg(compactNumber(m_voltage), compactNumber(m_internalResistance));
}

ComponentStepResult BatteryComponent::step(const QVector<std::optional<double>> &pinVoltages,
                                           double timeSeconds)
{
    Q_UNUSED(timeSeconds);
    return referencedSourceResult(m_voltage, pinVoltages);
}

ComponentElectricalResult BatteryComponent::evaluateElectrical(const ComponentElectricalInput &input)
{
    ComponentElectricalResult result;
    if (input.branchCurrent.has_value())
    {
        result.branchCurrent = input.branchCurrent;
        result.voltageAcross = m_voltage - *input.branchCurrent * m_internalResistance;
    }
    else if (input.voltageAcross.has_value())
    {
        result.voltageAcross = input.voltageAcross;
        if (m_internalResistance > 0.0)
            result.branchCurrent = (m_voltage - *input.voltageAcross) / m_internalResistance;
        else if (std::abs(*input.voltageAcross - m_voltage) > 0.000001)
            result.warnings.append(QStringLiteral("Ideal battery voltage constraint is inconsistent."));
    }
    else
    {
        result.voltageAcross = m_voltage;
    }
    return result;
}

QString ClockComponent::typeId() const
{
    return QStringLiteral("Clock");
}

QVector<ComponentPinDefinition> ClockComponent::pinDefinitions() const
{
    return {{QStringLiteral("OUT"), PinDirection::Output}, {QStringLiteral("RET"), PinDirection::Power}};
}

QVector<ComponentProperty> ClockComponent::editableProperties() const
{
    return {numberProperty(QStringLiteral("frequency"),
                           QStringLiteral("Frequency"),
                           m_frequency,
                           0.000001,
                           1000000000.0,
                           1.0,
                           QStringLiteral(" Hz")),
            numberProperty(QStringLiteral("dutyCycle"),
                           QStringLiteral("Duty cycle"),
                           m_dutyCycle,
                           0.1,
                           99.9,
                           1.0,
                           QStringLiteral(" %")),
            numberProperty(QStringLiteral("highVoltage"),
                           QStringLiteral("HIGH voltage"),
                           m_highVoltage,
                           0.1,
                           1000000.0,
                           0.1,
                           QStringLiteral(" V")),
            numberProperty(QStringLiteral("phase"),
                           QStringLiteral("Phase"),
                           m_phaseDegrees,
                           0.0,
                           359.999,
                           1.0,
                           QStringLiteral(" deg"))};
}

bool ClockComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("frequency"))
    {
        m_frequency = std::clamp(value.toDouble(), 0.000001, 1000000000.0);
        return true;
    }
    if (key == QStringLiteral("dutyCycle"))
    {
        m_dutyCycle = std::clamp(value.toDouble(), 0.1, 99.9);
        return true;
    }
    if (key == QStringLiteral("highVoltage"))
    {
        m_highVoltage = std::clamp(value.toDouble(), 0.1, 1000000.0);
        return true;
    }
    if (key == QStringLiteral("phase"))
    {
        m_phaseDegrees = std::fmod(std::max(0.0, value.toDouble()), 360.0);
        return true;
    }
    return false;
}

QVariant ClockComponent::property(const QString &key) const
{
    if (key == QStringLiteral("frequency"))
        return m_frequency;
    if (key == QStringLiteral("dutyCycle"))
        return m_dutyCycle;
    if (key == QStringLiteral("highVoltage"))
        return m_highVoltage;
    if (key == QStringLiteral("phase"))
        return m_phaseDegrees;
    return {};
}

QString ClockComponent::valueText() const
{
    return QStringLiteral("%1 Hz, %2%").arg(compactNumber(m_frequency), compactNumber(m_dutyCycle));
}

QString ClockComponent::runtimeText() const
{
    return QStringLiteral("%1, %2").arg(valueText(),
                                        m_lastHigh ? QStringLiteral("HIGH") : QStringLiteral("LOW"));
}

ComponentStepResult ClockComponent::step(const QVector<std::optional<double>> &pinVoltages,
                                         double timeSeconds)
{
    const double phaseCycles = m_phaseDegrees / 360.0;
    const double cycle = timeSeconds * m_frequency + phaseCycles;
    double position = cycle - std::floor(cycle);
    if (position < 0.0)
        position += 1.0;
    m_lastHigh = position < m_dutyCycle / 100.0;
    const double level = m_lastHigh ? m_highVoltage : LogicThresholds::lowVoltage();
    return referencedSourceResult(level, pinVoltages);
}

bool ClockComponent::active() const
{
    return m_lastHigh;
}

QString ResistorComponent::typeId() const
{
    return QStringLiteral("Resistor");
}

QVector<ComponentPinDefinition> ResistorComponent::pinDefinitions() const
{
    return twoPassivePins();
}

QVector<ComponentProperty> ResistorComponent::editableProperties() const
{
    return {numberProperty(QStringLiteral("resistance"),
                           QStringLiteral("Resistance"),
                           m_resistance,
                           0.000001,
                           1000000000000.0,
                           1.0,
                           QStringLiteral(" ohm"))};
}

bool ResistorComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key != QStringLiteral("resistance"))
        return false;
    m_resistance = std::clamp(value.toDouble(), 0.000001, 1000000000000.0);
    return true;
}

QVariant ResistorComponent::property(const QString &key) const
{
    return key == QStringLiteral("resistance") ? QVariant(m_resistance) : QVariant();
}

QString ResistorComponent::valueText() const
{
    return compactNumber(m_resistance) + QStringLiteral(" ohm");
}

QString ResistorComponent::runtimeText() const
{
    return valueText() + QStringLiteral(", ") + measurementText(m_lastVoltage, m_lastCurrent);
}

ComponentStepResult ResistorComponent::step(const QVector<std::optional<double>> &pinVoltages,
                                            double timeSeconds)
{
    Q_UNUSED(pinVoltages);
    Q_UNUSED(timeSeconds);
    return emptyResult(2);
}

ComponentElectricalResult ResistorComponent::evaluateElectrical(const ComponentElectricalInput &input)
{
    ComponentElectricalResult result;
    if (input.voltageAcross.has_value())
    {
        result.voltageAcross = input.voltageAcross;
        result.branchCurrent = *input.voltageAcross / m_resistance;
    }
    else if (input.branchCurrent.has_value())
    {
        result.branchCurrent = input.branchCurrent;
        result.voltageAcross = *input.branchCurrent * m_resistance;
    }
    m_lastVoltage = result.voltageAcross;
    m_lastCurrent = result.branchCurrent;
    return result;
}

QString CapacitorComponent::typeId() const
{
    return QStringLiteral("Capacitor");
}

QVector<ComponentPinDefinition> CapacitorComponent::pinDefinitions() const
{
    return twoPassivePins();
}

QVector<ComponentProperty> CapacitorComponent::editableProperties() const
{
    return {numberProperty(QStringLiteral("capacitance"),
                           QStringLiteral("Capacitance"),
                           m_capacitance,
                           0.000000000001,
                           1000000.0,
                           0.000001,
                           QStringLiteral(" F")),
            numberProperty(QStringLiteral("initialVoltage"),
                           QStringLiteral("Initial voltage"),
                           m_initialVoltage,
                           -1000000.0,
                           1000000.0,
                           0.1,
                           QStringLiteral(" V"))};
}

bool CapacitorComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("capacitance"))
    {
        m_capacitance = std::clamp(value.toDouble(), 0.000000000001, 1000000.0);
        return true;
    }
    if (key == QStringLiteral("initialVoltage"))
    {
        m_initialVoltage = std::clamp(value.toDouble(), -1000000.0, 1000000.0);
        m_storedVoltage = m_initialVoltage;
        return true;
    }
    return false;
}

QVariant CapacitorComponent::property(const QString &key) const
{
    if (key == QStringLiteral("capacitance"))
        return m_capacitance;
    if (key == QStringLiteral("initialVoltage"))
        return m_initialVoltage;
    return {};
}

QString CapacitorComponent::valueText() const
{
    return compactNumber(m_capacitance) + QStringLiteral(" F");
}

QString CapacitorComponent::runtimeText() const
{
    const QString current = m_lastCurrent.has_value() ? compactNumber(*m_lastCurrent) + QStringLiteral(" A")
                                                      : QStringLiteral("not measured");
    return QStringLiteral("%1, V=%2 V, I=%3").arg(valueText(), compactNumber(m_storedVoltage), current);
}

ComponentStepResult CapacitorComponent::step(const QVector<std::optional<double>> &pinVoltages,
                                             double timeSeconds)
{
    Q_UNUSED(pinVoltages);
    Q_UNUSED(timeSeconds);
    return emptyResult(2);
}

ComponentElectricalResult CapacitorComponent::evaluateElectrical(const ComponentElectricalInput &input)
{
    ComponentElectricalResult result;
    if (input.deltaSeconds <= 0.0)
    {
        result.voltageAcross = m_storedVoltage;
        result.warnings.append(QStringLiteral("Capacitor evaluation needs a positive time step."));
        return result;
    }

    if (input.voltageAcross.has_value())
    {
        result.voltageAcross = input.voltageAcross;
        result.branchCurrent = m_capacitance * (*input.voltageAcross - m_storedVoltage) / input.deltaSeconds;
        m_storedVoltage = *input.voltageAcross;
    }
    else if (input.branchCurrent.has_value())
    {
        result.branchCurrent = input.branchCurrent;
        m_storedVoltage += *input.branchCurrent * input.deltaSeconds / m_capacitance;
        result.voltageAcross = m_storedVoltage;
    }
    else
    {
        result.voltageAcross = m_storedVoltage;
    }

    m_lastCurrent = result.branchCurrent;
    return result;
}

QVariantMap CapacitorComponent::saveState() const
{
    return {{QStringLiteral("storedVoltage"), m_storedVoltage}};
}

void CapacitorComponent::loadState(const QVariantMap &state)
{
    m_storedVoltage = state.value(QStringLiteral("storedVoltage"), m_initialVoltage).toDouble();
}

QString InductorComponent::typeId() const
{
    return QStringLiteral("Inductor");
}

QVector<ComponentPinDefinition> InductorComponent::pinDefinitions() const
{
    return twoPassivePins();
}

QVector<ComponentProperty> InductorComponent::editableProperties() const
{
    return {numberProperty(QStringLiteral("inductance"),
                           QStringLiteral("Inductance"),
                           m_inductance,
                           0.000000000001,
                           1000000.0,
                           0.001,
                           QStringLiteral(" H")),
            numberProperty(QStringLiteral("initialCurrent"),
                           QStringLiteral("Initial current"),
                           m_initialCurrent,
                           -1000000.0,
                           1000000.0,
                           0.001,
                           QStringLiteral(" A"))};
}

bool InductorComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("inductance"))
    {
        m_inductance = std::clamp(value.toDouble(), 0.000000000001, 1000000.0);
        return true;
    }
    if (key == QStringLiteral("initialCurrent"))
    {
        m_initialCurrent = std::clamp(value.toDouble(), -1000000.0, 1000000.0);
        m_current = m_initialCurrent;
        return true;
    }
    return false;
}

QVariant InductorComponent::property(const QString &key) const
{
    if (key == QStringLiteral("inductance"))
        return m_inductance;
    if (key == QStringLiteral("initialCurrent"))
        return m_initialCurrent;
    return {};
}

QString InductorComponent::valueText() const
{
    return compactNumber(m_inductance) + QStringLiteral(" H");
}

QString InductorComponent::runtimeText() const
{
    const QString voltage = m_lastVoltage.has_value() ? compactNumber(*m_lastVoltage) + QStringLiteral(" V")
                                                      : QStringLiteral("not measured");
    return QStringLiteral("%1, V=%2, I=%3 A").arg(valueText(), voltage, compactNumber(m_current));
}

ComponentStepResult InductorComponent::step(const QVector<std::optional<double>> &pinVoltages,
                                            double timeSeconds)
{
    Q_UNUSED(pinVoltages);
    Q_UNUSED(timeSeconds);
    return emptyResult(2);
}

ComponentElectricalResult InductorComponent::evaluateElectrical(const ComponentElectricalInput &input)
{
    ComponentElectricalResult result;
    if (input.deltaSeconds <= 0.0)
    {
        result.branchCurrent = m_current;
        result.warnings.append(QStringLiteral("Inductor evaluation needs a positive time step."));
        return result;
    }

    if (input.voltageAcross.has_value())
    {
        result.voltageAcross = input.voltageAcross;
        m_current += *input.voltageAcross * input.deltaSeconds / m_inductance;
        result.branchCurrent = m_current;
    }
    else if (input.branchCurrent.has_value())
    {
        result.branchCurrent = input.branchCurrent;
        result.voltageAcross = m_inductance * (*input.branchCurrent - m_current) / input.deltaSeconds;
        m_current = *input.branchCurrent;
    }
    else
    {
        result.branchCurrent = m_current;
    }

    m_lastVoltage = result.voltageAcross;
    return result;
}

QVariantMap InductorComponent::saveState() const
{
    return {{QStringLiteral("current"), m_current}};
}

void InductorComponent::loadState(const QVariantMap &state)
{
    m_current = state.value(QStringLiteral("current"), m_initialCurrent).toDouble();
}

QString SwitchComponent::typeId() const
{
    return QStringLiteral("Switch");
}

QVector<ComponentPinDefinition> SwitchComponent::pinDefinitions() const
{
    return twoPassivePins();
}

QVector<ComponentProperty> SwitchComponent::editableProperties() const
{
    return {booleanProperty(QStringLiteral("closed"), QStringLiteral("Closed"), m_closed)};
}

bool SwitchComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key != QStringLiteral("closed"))
        return false;
    m_closed = value.toBool();
    return true;
}

QVariant SwitchComponent::property(const QString &key) const
{
    return key == QStringLiteral("closed") ? QVariant(m_closed) : QVariant();
}

QString SwitchComponent::valueText() const
{
    return m_closed ? QStringLiteral("Closed") : QStringLiteral("Open");
}

ComponentStepResult SwitchComponent::step(const QVector<std::optional<double>> &pinVoltages,
                                          double timeSeconds)
{
    Q_UNUSED(pinVoltages);
    Q_UNUSED(timeSeconds);
    return emptyResult(2);
}

ComponentElectricalResult SwitchComponent::evaluateElectrical(const ComponentElectricalInput &input)
{
    ComponentElectricalResult result;
    if (!m_closed)
    {
        result.voltageAcross = input.voltageAcross;
        result.branchCurrent = 0.0;
        return result;
    }

    constexpr double closedResistance = 0.000001;
    if (input.voltageAcross.has_value())
    {
        result.voltageAcross = input.voltageAcross;
        result.branchCurrent = *input.voltageAcross / closedResistance;
    }
    else if (input.branchCurrent.has_value())
    {
        result.branchCurrent = input.branchCurrent;
        result.voltageAcross = *input.branchCurrent * closedResistance;
    }
    return result;
}

QVector<QPair<int, int>> SwitchComponent::conductivePinPairs() const
{
    return m_closed ? QVector<QPair<int, int>>{{0, 1}} : QVector<QPair<int, int>>{};
}

void SwitchComponent::activate()
{
    m_closed = !m_closed;
}

bool SwitchComponent::active() const
{
    return m_closed;
}

QVariantMap SwitchComponent::saveState() const
{
    return {{QStringLiteral("closed"), m_closed}};
}

void SwitchComponent::loadState(const QVariantMap &state)
{
    m_closed = state.value(QStringLiteral("closed"), false).toBool();
}

QString PushButtonComponent::typeId() const
{
    return QStringLiteral("PushButton");
}

QVector<ComponentPinDefinition> PushButtonComponent::pinDefinitions() const
{
    return {{QStringLiteral("OUT"), PinDirection::Output}};
}

QVector<ComponentProperty> PushButtonComponent::editableProperties() const
{
    return {numberProperty(QStringLiteral("highVoltage"),
                           QStringLiteral("HIGH voltage"),
                           m_highVoltage,
                           0.1,
                           1000000.0,
                           0.1,
                           QStringLiteral(" V"))};
}

bool PushButtonComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key != QStringLiteral("highVoltage"))
        return false;
    m_highVoltage = std::clamp(value.toDouble(), 0.1, 1000000.0);
    return true;
}

QVariant PushButtonComponent::property(const QString &key) const
{
    return key == QStringLiteral("highVoltage") ? QVariant(m_highVoltage) : QVariant();
}

QString PushButtonComponent::valueText() const
{
    return m_pressed ? QStringLiteral("HIGH") : QStringLiteral("LOW");
}

ComponentStepResult PushButtonComponent::step(const QVector<std::optional<double>> &pinVoltages,
                                              double timeSeconds)
{
    Q_UNUSED(pinVoltages);
    Q_UNUSED(timeSeconds);
    ComponentStepResult result = emptyResult(1);
    result.pinVoltages[0] = m_pressed ? m_highVoltage : LogicThresholds::lowVoltage();
    result.drivenPins[0] = true;
    return result;
}

ComponentElectricalResult PushButtonComponent::evaluateElectrical(const ComponentElectricalInput &input)
{
    Q_UNUSED(input);
    ComponentElectricalResult result;
    result.voltageAcross = m_pressed ? m_highVoltage : LogicThresholds::lowVoltage();
    return result;
}

void PushButtonComponent::pointerPressed()
{
    m_pressed = true;
}

void PushButtonComponent::pointerReleased()
{
    m_pressed = false;
}

bool PushButtonComponent::active() const
{
    return m_pressed;
}

LedComponent::LedComponent(const QString &type) : m_type(type)
{
    if (m_type == QStringLiteral("LEDRed"))
        m_forwardVoltage = 1.8;
    else if (m_type == QStringLiteral("LEDGreen"))
        m_forwardVoltage = 2.1;
    else
        m_forwardVoltage = 3.0;
}

QString LedComponent::typeId() const
{
    return m_type;
}

QVector<ComponentPinDefinition> LedComponent::pinDefinitions() const
{
    return {{QStringLiteral("A"), PinDirection::Input}, {QStringLiteral("K"), PinDirection::Input}};
}

QVector<ComponentProperty> LedComponent::editableProperties() const
{
    return {numberProperty(QStringLiteral("forwardVoltage"),
                           QStringLiteral("Forward voltage"),
                           m_forwardVoltage,
                           0.1,
                           20.0,
                           0.1,
                           QStringLiteral(" V")),
            numberProperty(QStringLiteral("maxCurrent"),
                           QStringLiteral("Maximum current"),
                           m_maxCurrentMilliamp,
                           0.1,
                           1000.0,
                           1.0,
                           QStringLiteral(" mA"))};
}

bool LedComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("forwardVoltage"))
    {
        m_forwardVoltage = std::clamp(value.toDouble(), 0.1, 20.0);
        return true;
    }
    if (key == QStringLiteral("maxCurrent"))
    {
        m_maxCurrentMilliamp = std::clamp(value.toDouble(), 0.1, 1000.0);
        return true;
    }
    return false;
}

QVariant LedComponent::property(const QString &key) const
{
    if (key == QStringLiteral("forwardVoltage"))
        return m_forwardVoltage;
    if (key == QStringLiteral("maxCurrent"))
        return m_maxCurrentMilliamp;
    return {};
}

QString LedComponent::valueText() const
{
    QString color = QStringLiteral("Red");
    if (m_type == QStringLiteral("LEDGreen"))
        color = QStringLiteral("Green");
    else if (m_type == QStringLiteral("LEDBlue"))
        color = QStringLiteral("Blue");
    return QStringLiteral("%1, Vf=%2 V").arg(color, compactNumber(m_forwardVoltage));
}

QString LedComponent::runtimeText() const
{
    return QStringLiteral("%1, %2, I=%3 A")
        .arg(valueText(), m_lit ? QStringLiteral("ON") : QStringLiteral("OFF"), compactNumber(m_currentAmp));
}

ComponentStepResult LedComponent::step(const QVector<std::optional<double>> &pinVoltages, double timeSeconds)
{
    Q_UNUSED(timeSeconds);
    const std::optional<double> anode = pinVoltage(pinVoltages, 0);
    const std::optional<double> cathode = pinVoltage(pinVoltages, 1);
    if (!anode.has_value() || !cathode.has_value())
    {
        m_lit = false;
        m_currentAmp = 0.0;
        return emptyResult(2);
    }

    constexpr double dynamicResistance = 10.0;
    const double forward = *anode - *cathode;
    m_currentAmp = forward > m_forwardVoltage ? (forward - m_forwardVoltage) / dynamicResistance : 0.0;
    m_lit = m_currentAmp > 0.0;

    ComponentStepResult result = emptyResult(2);
    if (m_currentAmp * 1000.0 > m_maxCurrentMilliamp)
        result.warnings.append(QStringLiteral("LED current exceeds the configured maximum."));
    return result;
}

ComponentElectricalResult LedComponent::evaluateElectrical(const ComponentElectricalInput &input)
{
    ComponentElectricalResult result;
    constexpr double dynamicResistance = 10.0;
    if (input.voltageAcross.has_value())
    {
        result.voltageAcross = input.voltageAcross;
        m_currentAmp = *input.voltageAcross > m_forwardVoltage
                           ? (*input.voltageAcross - m_forwardVoltage) / dynamicResistance
                           : 0.0;
        result.branchCurrent = m_currentAmp;
    }
    else if (input.branchCurrent.has_value())
    {
        m_currentAmp = std::max(0.0, *input.branchCurrent);
        result.branchCurrent = input.branchCurrent;
        result.voltageAcross = m_currentAmp > 0.0 ? m_forwardVoltage + m_currentAmp * dynamicResistance : 0.0;
    }
    m_lit = m_currentAmp > 0.0;
    if (m_currentAmp * 1000.0 > m_maxCurrentMilliamp)
        result.warnings.append(QStringLiteral("LED current exceeds the configured maximum."));
    return result;
}

bool LedComponent::active() const
{
    return m_lit;
}

QColor LedComponent::indicatorColor() const
{
    if (m_type == QStringLiteral("LEDGreen"))
        return QColor(22, 163, 74);
    if (m_type == QStringLiteral("LEDBlue"))
        return QColor(37, 99, 235);
    return QColor(220, 38, 38);
}

QString SevenSegmentComponent::typeId() const
{
    return QStringLiteral("SevenSegment");
}

QVector<ComponentPinDefinition> SevenSegmentComponent::pinDefinitions() const
{
    QVector<ComponentPinDefinition> pins{{QStringLiteral("a"), PinDirection::Input},
                                         {QStringLiteral("b"), PinDirection::Input},
                                         {QStringLiteral("c"), PinDirection::Input},
                                         {QStringLiteral("d"), PinDirection::Input},
                                         {QStringLiteral("e"), PinDirection::Input},
                                         {QStringLiteral("f"), PinDirection::Input},
                                         {QStringLiteral("g"), PinDirection::Input}};
    if (m_hasDecimalPoint)
        pins.append({QStringLiteral("dp"), PinDirection::Input});
    pins.append({QStringLiteral("COM"), PinDirection::Power});
    return pins;
}

QVector<ComponentProperty> SevenSegmentComponent::editableProperties() const
{
    QString color = QStringLiteral("Red");
    if (m_color == QColor(22, 163, 74))
        color = QStringLiteral("Green");
    else if (m_color == QColor(37, 99, 235))
        color = QStringLiteral("Blue");

    return {choiceProperty(QStringLiteral("commonType"),
                           QStringLiteral("Connection"),
                           m_commonType,
                           {QStringLiteral("Common cathode"), QStringLiteral("Common anode")}),
            choiceProperty(QStringLiteral("color"),
                           QStringLiteral("Color"),
                           color,
                           {QStringLiteral("Red"), QStringLiteral("Green"), QStringLiteral("Blue")}),
            booleanProperty(
                QStringLiteral("decimalPoint"), QStringLiteral("Decimal point pin"), m_hasDecimalPoint)};
}

bool SevenSegmentComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("commonType"))
    {
        const QString candidate = value.toString();
        if (candidate != QStringLiteral("Common cathode") && candidate != QStringLiteral("Common anode"))
            return false;
        m_commonType = candidate;
        return true;
    }
    if (key == QStringLiteral("color"))
    {
        const QString color = value.toString();
        if (color == QStringLiteral("Green"))
            m_color = QColor(22, 163, 74);
        else if (color == QStringLiteral("Blue"))
            m_color = QColor(37, 99, 235);
        else if (color == QStringLiteral("Red"))
            m_color = QColor(220, 38, 38);
        else
            return false;
        return true;
    }
    if (key == QStringLiteral("decimalPoint"))
    {
        m_hasDecimalPoint = value.toBool();
        if (!m_hasDecimalPoint)
            m_segments[7] = false;
        return true;
    }
    return false;
}

QVariant SevenSegmentComponent::property(const QString &key) const
{
    if (key == QStringLiteral("commonType"))
        return m_commonType;
    if (key == QStringLiteral("color"))
    {
        if (m_color == QColor(22, 163, 74))
            return QStringLiteral("Green");
        if (m_color == QColor(37, 99, 235))
            return QStringLiteral("Blue");
        return QStringLiteral("Red");
    }
    if (key == QStringLiteral("decimalPoint"))
        return m_hasDecimalPoint;
    return {};
}

QString SevenSegmentComponent::valueText() const
{
    return m_commonType;
}

QString SevenSegmentComponent::runtimeText() const
{
    QStringList activeSegments;
    const QStringList names{QStringLiteral("a"),
                            QStringLiteral("b"),
                            QStringLiteral("c"),
                            QStringLiteral("d"),
                            QStringLiteral("e"),
                            QStringLiteral("f"),
                            QStringLiteral("g"),
                            QStringLiteral("dp")};
    for (int i = 0; i < m_segments.size(); ++i)
    {
        if (m_segments.at(i))
            activeSegments.append(names.value(i));
    }
    const QString state = m_hasUndefinedInput        ? QStringLiteral("UNDEFINED")
                          : activeSegments.isEmpty() ? QStringLiteral("all off")
                                                     : activeSegments.join(QLatin1Char(','));
    return QStringLiteral("%1, %2").arg(valueText(), state);
}

ComponentStepResult SevenSegmentComponent::step(const QVector<std::optional<double>> &pinVoltages,
                                                double timeSeconds)
{
    Q_UNUSED(timeSeconds);
    const int segmentCount = m_hasDecimalPoint ? 8 : 7;
    ComponentStepResult result = emptyResult(segmentCount + 1);
    m_hasUndefinedInput = false;
    const LogicLevel common = LogicThresholds::fromVoltage(pinVoltage(pinVoltages, segmentCount));
    if (common == LogicLevel::Undefined)
        m_hasUndefinedInput = true;

    for (int i = 0; i < segmentCount; ++i)
    {
        const LogicLevel level = LogicThresholds::fromVoltage(pinVoltage(pinVoltages, i));
        if (level == LogicLevel::Undefined)
            m_hasUndefinedInput = true;

        if (m_commonType == QStringLiteral("Common cathode"))
            m_segments[i] = common == LogicLevel::Low && level == LogicLevel::High;
        else
            m_segments[i] = common == LogicLevel::High && level == LogicLevel::Low;
    }

    if (!m_hasDecimalPoint)
        m_segments[7] = false;
    if (m_hasUndefinedInput)
        appendFloatingWarning(result);
    return result;
}

bool SevenSegmentComponent::active() const
{
    return std::any_of(m_segments.cbegin(), m_segments.cend(), [](bool state) { return state; });
}

QColor SevenSegmentComponent::indicatorColor() const
{
    return m_color;
}

QVector<bool> SevenSegmentComponent::segmentStates() const
{
    return m_segments;
}

LogicGateComponent::LogicGateComponent(const QString &type) : m_type(type)
{
    if (m_type == QStringLiteral("NOT"))
    {
        m_inputCount = 1;
        m_propagationDelayNanoseconds = 5.0;
    }
}

QString LogicGateComponent::typeId() const
{
    return m_type;
}

QVector<ComponentPinDefinition> LogicGateComponent::pinDefinitions() const
{
    QVector<ComponentPinDefinition> pins;
    if (m_type == QStringLiteral("NOT"))
    {
        pins.append({QStringLiteral("IN"), PinDirection::Input});
        pins.append({QStringLiteral("OUT"), PinDirection::Output});
        return pins;
    }

    for (int i = 0; i < m_inputCount; ++i)
        pins.append({QStringLiteral("I%1").arg(i + 1), PinDirection::Input});
    pins.append({QStringLiteral("Y"), PinDirection::Output});
    return pins;
}

QVector<ComponentProperty> LogicGateComponent::editableProperties() const
{
    QVector<ComponentProperty> properties;
    if (m_type != QStringLiteral("NOT"))
    {
        properties.append(
            integerProperty(QStringLiteral("inputCount"), QStringLiteral("Input count"), m_inputCount, 2, 8));
    }
    properties.append(numberProperty(QStringLiteral("propagationDelay"),
                                     QStringLiteral("Propagation delay"),
                                     m_propagationDelayNanoseconds,
                                     0.0,
                                     1000000000.0,
                                     1.0,
                                     QStringLiteral(" ns")));
    return properties;
}

bool LogicGateComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("inputCount") && m_type != QStringLiteral("NOT"))
    {
        m_inputCount = std::clamp(value.toInt(), 2, 8);
        return true;
    }
    if (key == QStringLiteral("propagationDelay"))
    {
        m_propagationDelayNanoseconds = std::clamp(value.toDouble(), 0.0, 1000000000.0);
        return true;
    }
    return false;
}

QVariant LogicGateComponent::property(const QString &key) const
{
    if (key == QStringLiteral("inputCount"))
        return m_inputCount;
    if (key == QStringLiteral("propagationDelay"))
        return m_propagationDelayNanoseconds;
    return {};
}

QString LogicGateComponent::valueText() const
{
    if (m_type == QStringLiteral("NOT"))
        return QStringLiteral("delay=%1 ns").arg(compactNumber(m_propagationDelayNanoseconds));
    return QStringLiteral("inputs=%1, delay=%2 ns")
        .arg(m_inputCount)
        .arg(compactNumber(m_propagationDelayNanoseconds));
}

QString LogicGateComponent::runtimeText() const
{
    return QStringLiteral("%1, OUT=%2").arg(valueText(), LogicThresholds::text(m_output));
}

LogicLevel LogicGateComponent::evaluate(const QVector<LogicLevel> &inputs) const
{
    if (inputs.isEmpty() || std::any_of(inputs.cbegin(),
                                        inputs.cend(),
                                        [](LogicLevel value) { return value == LogicLevel::Undefined; }))
        return LogicLevel::Undefined;

    if (m_type == QStringLiteral("NOT"))
        return invertLevel(inputs.first());

    const int highCount = static_cast<int>(std::count(inputs.cbegin(), inputs.cend(), LogicLevel::High));
    bool output = false;
    if (m_type == QStringLiteral("AND"))
        output = highCount == inputs.size();
    else if (m_type == QStringLiteral("OR"))
        output = highCount > 0;
    else if (m_type == QStringLiteral("NAND"))
        output = highCount != inputs.size();
    else if (m_type == QStringLiteral("XOR"))
        output = highCount % 2 == 1;
    else
        return LogicLevel::Undefined;

    return output ? LogicLevel::High : LogicLevel::Low;
}

void LogicGateComponent::schedule(LogicLevel target, double timeSeconds)
{
    if (target == m_output)
    {
        m_hasPendingOutput = false;
        return;
    }
    if (m_hasPendingOutput && target == m_pendingOutput)
        return;

    m_pendingOutput = target;
    m_applyAtSeconds = timeSeconds + m_propagationDelayNanoseconds * 0.000000001;
    m_hasPendingOutput = true;
}

ComponentStepResult LogicGateComponent::step(const QVector<std::optional<double>> &pinVoltages,
                                             double timeSeconds)
{
    const int inputCount = m_type == QStringLiteral("NOT") ? 1 : m_inputCount;
    ComponentStepResult result = emptyResult(inputCount + 1);
    QVector<LogicLevel> inputs;
    inputs.reserve(inputCount);

    for (int i = 0; i < inputCount; ++i)
    {
        const LogicLevel level = LogicThresholds::fromVoltage(pinVoltage(pinVoltages, i));
        inputs.append(level);
        if (level == LogicLevel::Undefined)
            appendFloatingWarning(result);
    }

    schedule(evaluate(inputs), timeSeconds);
    if (m_hasPendingOutput && timeSeconds >= m_applyAtSeconds)
    {
        m_output = m_pendingOutput;
        m_hasPendingOutput = false;
    }

    result.pinVoltages[inputCount] = LogicThresholds::toVoltage(m_output);
    result.drivenPins[inputCount] = true;
    return result;
}

bool LogicGateComponent::active() const
{
    return m_output == LogicLevel::High;
}

QVariantMap LogicGateComponent::saveState() const
{
    return {{QStringLiteral("output"), static_cast<int>(m_output)},
            {QStringLiteral("pendingOutput"), static_cast<int>(m_pendingOutput)},
            {QStringLiteral("applyAtSeconds"), m_applyAtSeconds},
            {QStringLiteral("hasPendingOutput"), m_hasPendingOutput}};
}

void LogicGateComponent::loadState(const QVariantMap &state)
{
    m_output = static_cast<LogicLevel>(state.value(QStringLiteral("output"), 0).toInt());
    m_pendingOutput = static_cast<LogicLevel>(state.value(QStringLiteral("pendingOutput"), 0).toInt());
    m_applyAtSeconds = state.value(QStringLiteral("applyAtSeconds"), 0.0).toDouble();
    m_hasPendingOutput = state.value(QStringLiteral("hasPendingOutput"), false).toBool();
}

QString DFlipFlopComponent::typeId() const
{
    return QStringLiteral("DFlipFlop");
}

QVector<ComponentPinDefinition> DFlipFlopComponent::pinDefinitions() const
{
    return {{QStringLiteral("D"), PinDirection::Input},
            {QStringLiteral("CLK"), PinDirection::Input},
            {QStringLiteral("Q"), PinDirection::Output},
            {QStringLiteral("/Q"), PinDirection::Output}};
}

QVector<ComponentProperty> DFlipFlopComponent::editableProperties() const
{
    return {numberProperty(QStringLiteral("propagationDelay"),
                           QStringLiteral("Propagation delay"),
                           m_propagationDelayNanoseconds,
                           0.0,
                           1000000000.0,
                           1.0,
                           QStringLiteral(" ns"))};
}

bool DFlipFlopComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key != QStringLiteral("propagationDelay"))
        return false;
    m_propagationDelayNanoseconds = std::clamp(value.toDouble(), 0.0, 1000000000.0);
    return true;
}

QVariant DFlipFlopComponent::property(const QString &key) const
{
    return key == QStringLiteral("propagationDelay") ? QVariant(m_propagationDelayNanoseconds) : QVariant();
}

QString DFlipFlopComponent::valueText() const
{
    return QStringLiteral("delay=%1 ns").arg(compactNumber(m_propagationDelayNanoseconds));
}

QString DFlipFlopComponent::runtimeText() const
{
    return QStringLiteral("%1, Q=%2").arg(valueText(), LogicThresholds::text(m_q));
}

void DFlipFlopComponent::schedule(LogicLevel target, double timeSeconds)
{
    if (target == m_q)
    {
        m_hasPendingQ = false;
        return;
    }
    if (m_hasPendingQ && target == m_pendingQ)
        return;

    m_pendingQ = target;
    m_applyAtSeconds = timeSeconds + m_propagationDelayNanoseconds * 0.000000001;
    m_hasPendingQ = true;
}

ComponentStepResult DFlipFlopComponent::step(const QVector<std::optional<double>> &pinVoltages,
                                             double timeSeconds)
{
    ComponentStepResult result = emptyResult(4);
    const LogicLevel data = LogicThresholds::fromVoltage(pinVoltage(pinVoltages, 0));
    const LogicLevel clock = LogicThresholds::fromVoltage(pinVoltage(pinVoltages, 1));

    if (data == LogicLevel::Undefined || clock == LogicLevel::Undefined)
        appendFloatingWarning(result);

    if (data == LogicLevel::Undefined || clock == LogicLevel::Undefined)
    {
        schedule(LogicLevel::Undefined, timeSeconds);
    }
    else
    {
        const bool risingEdge = m_previousClock == LogicLevel::Low && clock == LogicLevel::High;
        if (risingEdge)
            schedule(data, timeSeconds);
    }

    if (clock != LogicLevel::Undefined)
        m_previousClock = clock;

    if (m_hasPendingQ && timeSeconds >= m_applyAtSeconds)
    {
        m_q = m_pendingQ;
        m_hasPendingQ = false;
    }

    result.pinVoltages[2] = LogicThresholds::toVoltage(m_q);
    result.pinVoltages[3] = LogicThresholds::toVoltage(invertLevel(m_q));
    result.drivenPins[2] = true;
    result.drivenPins[3] = true;
    return result;
}

bool DFlipFlopComponent::active() const
{
    return m_q == LogicLevel::High;
}

QVariantMap DFlipFlopComponent::saveState() const
{
    return {{QStringLiteral("q"), static_cast<int>(m_q)},
            {QStringLiteral("previousClock"), static_cast<int>(m_previousClock)},
            {QStringLiteral("pendingQ"), static_cast<int>(m_pendingQ)},
            {QStringLiteral("applyAtSeconds"), m_applyAtSeconds},
            {QStringLiteral("hasPendingQ"), m_hasPendingQ}};
}

void DFlipFlopComponent::loadState(const QVariantMap &state)
{
    m_q = static_cast<LogicLevel>(state.value(QStringLiteral("q"), 0).toInt());
    m_previousClock = static_cast<LogicLevel>(
        state.value(QStringLiteral("previousClock"), static_cast<int>(LogicLevel::Undefined)).toInt());
    m_pendingQ = static_cast<LogicLevel>(state.value(QStringLiteral("pendingQ"), 0).toInt());
    m_applyAtSeconds = state.value(QStringLiteral("applyAtSeconds"), 0.0).toDouble();
    m_hasPendingQ = state.value(QStringLiteral("hasPendingQ"), false).toBool();
}

GenericComponent::GenericComponent(const QString &type) : m_type(type), m_value(QStringLiteral("ready"))
{
}

QString GenericComponent::typeId() const
{
    return m_type;
}

QVector<ComponentPinDefinition> GenericComponent::pinDefinitions() const
{
    if (m_type == QStringLiteral("VoltageProbe"))
        return {{QStringLiteral("V"), PinDirection::Input}};
    if (m_type == QStringLiteral("Current"))
        return {{QStringLiteral("OUT"), PinDirection::Output}, {QStringLiteral("RET"), PinDirection::Power}};
    if (m_type == QStringLiteral("Potentiometer"))
        return {{QStringLiteral("A"), PinDirection::Passive},
                {QStringLiteral("W"), PinDirection::Passive},
                {QStringLiteral("B"), PinDirection::Passive}};
    if (m_type == QStringLiteral("Voltmeter"))
        return {{QStringLiteral("V+"), PinDirection::Input}, {QStringLiteral("V-"), PinDirection::Input}};
    if (m_type == QStringLiteral("Ammeter"))
        return {{QStringLiteral("I+"), PinDirection::Input}, {QStringLiteral("I-"), PinDirection::Input}};
    if (m_type == QStringLiteral("Oscilloscope"))
        return {{QStringLiteral("CH1"), PinDirection::Input},
                {QStringLiteral("CH2"), PinDirection::Input},
                {QStringLiteral("GND"), PinDirection::Power}};
    if (m_type == QStringLiteral("ADC"))
        return {{QStringLiteral("VIN"), PinDirection::Input},
                {QStringLiteral("VREF-"), PinDirection::Power},
                {QStringLiteral("VREF+"), PinDirection::Power},
                {QStringLiteral("D0"), PinDirection::Output},
                {QStringLiteral("D1"), PinDirection::Output},
                {QStringLiteral("D2"), PinDirection::Output},
                {QStringLiteral("D3"), PinDirection::Output}};
    if (m_type == QStringLiteral("DAC"))
        return {{QStringLiteral("D0"), PinDirection::Input},
                {QStringLiteral("D1"), PinDirection::Input},
                {QStringLiteral("D2"), PinDirection::Input},
                {QStringLiteral("D3"), PinDirection::Input},
                {QStringLiteral("VOUT"), PinDirection::Output},
                {QStringLiteral("VREF+"), PinDirection::Power},
                {QStringLiteral("VREF-"), PinDirection::Power}};
    if (m_type == QStringLiteral("MCU"))
    {
        QVector<ComponentPinDefinition> result;
        for (int i = 0; i < 6; ++i)
            result.append({QStringLiteral("PA%1").arg(i), PinDirection::Passive});
        for (int i = 0; i < 6; ++i)
            result.append({QStringLiteral("PB%1").arg(i), PinDirection::Passive});
        return result;
    }
    if (m_type == QStringLiteral("EEPROM"))
        return {{QStringLiteral("A0"), PinDirection::Passive},
                {QStringLiteral("A1"), PinDirection::Passive},
                {QStringLiteral("A2"), PinDirection::Passive},
                {QStringLiteral("D0"), PinDirection::Passive},
                {QStringLiteral("D1"), PinDirection::Passive},
                {QStringLiteral("D2"), PinDirection::Passive}};
    if (m_type == QStringLiteral("LCD16x2"))
    {
        QVector<ComponentPinDefinition> result;
        for (int i = 0; i < 8; ++i)
            result.append({QStringLiteral("D%1").arg(i), PinDirection::Input});
        return result;
    }
    if (m_type == QStringLiteral("Keypad4x4"))
        return {{QStringLiteral("R1"), PinDirection::Passive},
                {QStringLiteral("R2"), PinDirection::Passive},
                {QStringLiteral("R3"), PinDirection::Passive},
                {QStringLiteral("R4"), PinDirection::Passive},
                {QStringLiteral("C1"), PinDirection::Passive},
                {QStringLiteral("C2"), PinDirection::Passive},
                {QStringLiteral("C3"), PinDirection::Passive},
                {QStringLiteral("C4"), PinDirection::Passive}};
    return twoPassivePins();
}

QVector<ComponentProperty> GenericComponent::editableProperties() const
{
    return {textProperty(QStringLiteral("value"), QStringLiteral("Value / parameters"), m_value)};
}

bool GenericComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key != QStringLiteral("value"))
        return false;
    m_value = value.toString().trimmed();
    return true;
}

QVariant GenericComponent::property(const QString &key) const
{
    return key == QStringLiteral("value") ? QVariant(m_value) : QVariant();
}

QString GenericComponent::valueText() const
{
    return m_value;
}

ComponentStepResult GenericComponent::step(const QVector<std::optional<double>> &pinVoltages,
                                           double timeSeconds)
{
    Q_UNUSED(pinVoltages);
    Q_UNUSED(timeSeconds);
    return emptyResult(pinDefinitions().size());
}

std::unique_ptr<Component> ComponentFactory::create(const QString &type)
{
    if (type == QStringLiteral("Ground"))
        return std::make_unique<GroundComponent>();
    if (type == QStringLiteral("Voltage"))
        return std::make_unique<VoltageSourceComponent>();
    if (type == QStringLiteral("Battery"))
        return std::make_unique<BatteryComponent>();
    if (type == QStringLiteral("Clock"))
        return std::make_unique<ClockComponent>();
    if (type == QStringLiteral("Resistor"))
        return std::make_unique<ResistorComponent>();
    if (type == QStringLiteral("Capacitor"))
        return std::make_unique<CapacitorComponent>();
    if (type == QStringLiteral("Inductor"))
        return std::make_unique<InductorComponent>();
    if (type == QStringLiteral("Switch"))
        return std::make_unique<SwitchComponent>();
    if (type == QStringLiteral("PushButton"))
        return std::make_unique<PushButtonComponent>();
    if (type == QStringLiteral("LEDRed") || type == QStringLiteral("LEDGreen") ||
        type == QStringLiteral("LEDBlue"))
        return std::make_unique<LedComponent>(type);
    if (type == QStringLiteral("SevenSegment"))
        return std::make_unique<SevenSegmentComponent>();
    if (type == QStringLiteral("AND") || type == QStringLiteral("OR") || type == QStringLiteral("NOT") ||
        type == QStringLiteral("NAND") || type == QStringLiteral("XOR"))
        return std::make_unique<LogicGateComponent>(type);
    if (type == QStringLiteral("DFlipFlop"))
        return std::make_unique<DFlipFlopComponent>();
    return std::make_unique<GenericComponent>(type);
}
