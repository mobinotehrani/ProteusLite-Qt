#include "convertercomponents.h"

#include <algorithm>
#include <cmath>

namespace
{
std::optional<double> voltageAt(const QVector<std::optional<double>> &values, int index)
{
    if (index < 0 || index >= values.size())
        return std::nullopt;
    return values.at(index);
}

QString numberText(double value, int precision = 6)
{
    return QString::number(value, 'g', precision);
}

void appendUnique(QStringList &messages, const QString &message)
{
    if (!message.isEmpty() && !messages.contains(message))
        messages.append(message);
}
}

QString AdcComponent::typeId() const
{
    return QStringLiteral("ADC");
}

QVector<ComponentPinDefinition> AdcComponent::pinDefinitions() const
{
    QVector<ComponentPinDefinition> pins{{QStringLiteral("VIN"), PinDirection::Input},
                                         {QStringLiteral("VREF-"), PinDirection::Power},
                                         {QStringLiteral("VREF+"), PinDirection::Power}};
    for (int bit = 0; bit < m_resolutionBits; ++bit)
        pins.append({QStringLiteral("D%1").arg(bit), PinDirection::Output});
    return pins;
}

QVector<ComponentProperty> AdcComponent::editableProperties() const
{
    return {integerProperty(QStringLiteral("resolutionBits"),
                            QStringLiteral("Resolution"),
                            m_resolutionBits,
                            2,
                            12,
                            1,
                            QStringLiteral(" bit")),
            numberProperty(QStringLiteral("conversionDelayMs"),
                           QStringLiteral("Conversion delay"),
                           m_conversionDelayMilliseconds,
                           0.0,
                           1000.0,
                           0.1,
                           QStringLiteral(" ms")),
            numberProperty(QStringLiteral("logicHighVoltage"),
                           QStringLiteral("Digital HIGH voltage"),
                           m_logicHighVoltage,
                           1.0,
                           24.0,
                           0.1,
                           QStringLiteral(" V"))};
}

bool AdcComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("resolutionBits"))
    {
        m_resolutionBits = std::clamp(value.toInt(), 2, 12);
        m_outputCode = std::clamp(m_outputCode, 0, maximumCode());
        m_pendingCode = std::clamp(m_pendingCode, 0, maximumCode());
        return true;
    }
    if (key == QStringLiteral("conversionDelayMs"))
    {
        m_conversionDelayMilliseconds = std::clamp(value.toDouble(), 0.0, 1000.0);
        return true;
    }
    if (key == QStringLiteral("logicHighVoltage"))
    {
        m_logicHighVoltage = std::clamp(value.toDouble(), 1.0, 24.0);
        return true;
    }
    return false;
}

QVariant AdcComponent::property(const QString &key) const
{
    if (key == QStringLiteral("resolutionBits"))
        return m_resolutionBits;
    if (key == QStringLiteral("conversionDelayMs"))
        return m_conversionDelayMilliseconds;
    if (key == QStringLiteral("logicHighVoltage"))
        return m_logicHighVoltage;
    return {};
}

QString AdcComponent::valueText() const
{
    return QStringLiteral("%1-bit ADC").arg(m_resolutionBits);
}

QString AdcComponent::runtimeText() const
{
    if (!m_hasValidOutput)
    {
        if (m_conversionPending)
            return QStringLiteral("converting, output not ready");
        return QStringLiteral("waiting for VIN and references");
    }

    QString text = QStringLiteral("code=%1/%2").arg(m_outputCode).arg(maximumCode());
    if (m_lastInputVoltage.has_value())
        text += QStringLiteral(", Vin=%1 V").arg(numberText(*m_lastInputVoltage));
    if (m_conversionPending)
        text += QStringLiteral(" (conversion pending)");
    return text;
}

ComponentStepResult AdcComponent::step(const QVector<std::optional<double>> &pinVoltages,
                                       double timeSeconds)
{
    ComponentStepResult result = emptyResult(pinDefinitions().size());
    const std::optional<double> input = voltageAt(pinVoltages, 0);
    const std::optional<double> referenceLow = voltageAt(pinVoltages, 1);
    const std::optional<double> referenceHigh = voltageAt(pinVoltages, 2);

    if (!input.has_value())
        appendUnique(result.warnings, QStringLiteral("ADC analog input is floating."));
    if (!referenceLow.has_value() || !referenceHigh.has_value())
        appendUnique(result.warnings, QStringLiteral("ADC reference pins are not fully connected."));

    if (input.has_value() && referenceLow.has_value() && referenceHigh.has_value())
    {
        m_lastInputVoltage = *input;
        if (*referenceHigh <= *referenceLow)
        {
            appendUnique(result.warnings,
                         QStringLiteral("ADC requires VREF+ to be greater than VREF-."));
        }
        else
        {
            const int targetCode = codeForVoltage(*input, *referenceLow, *referenceHigh);
            if (m_conversionPending)
            {
                if (targetCode != m_pendingCode)
                    scheduleCode(targetCode, timeSeconds);
            }
            else if (!m_hasValidOutput || targetCode != m_outputCode)
            {
                scheduleCode(targetCode, timeSeconds);
            }
        }
    }

    if (m_conversionPending && timeSeconds + 1e-12 >= m_applyAtSeconds)
    {
        m_outputCode = m_pendingCode;
        m_hasValidOutput = true;
        m_conversionPending = false;
    }

    if (m_hasValidOutput)
    {
        for (int bit = 0; bit < m_resolutionBits; ++bit)
        {
            const int pinIndex = 3 + bit;
            const bool high = ((m_outputCode >> bit) & 0x1) != 0;
            result.pinVoltages[pinIndex] = high ? m_logicHighVoltage : LogicThresholds::lowVoltage();
            result.drivenPins[pinIndex] = true;
        }
    }

    return result;
}

QVariantMap AdcComponent::saveState() const
{
    return {{QStringLiteral("outputCode"), m_outputCode},
            {QStringLiteral("pendingCode"), m_pendingCode},
            {QStringLiteral("applyAtSeconds"), m_applyAtSeconds},
            {QStringLiteral("hasValidOutput"), m_hasValidOutput},
            {QStringLiteral("conversionPending"), m_conversionPending},
            {QStringLiteral("lastInputVoltage"),
             m_lastInputVoltage.has_value() ? QVariant(*m_lastInputVoltage) : QVariant()}};
}

void AdcComponent::loadState(const QVariantMap &state)
{
    m_outputCode = std::clamp(state.value(QStringLiteral("outputCode"), 0).toInt(), 0, maximumCode());
    m_pendingCode = std::clamp(state.value(QStringLiteral("pendingCode"), 0).toInt(), 0, maximumCode());
    m_applyAtSeconds = state.value(QStringLiteral("applyAtSeconds"), 0.0).toDouble();
    m_hasValidOutput = state.value(QStringLiteral("hasValidOutput"), false).toBool();
    m_conversionPending = state.value(QStringLiteral("conversionPending"), false).toBool();
    if (state.value(QStringLiteral("lastInputVoltage")).isValid())
        m_lastInputVoltage = state.value(QStringLiteral("lastInputVoltage")).toDouble();
    else
        m_lastInputVoltage.reset();
}

int AdcComponent::maximumCode() const
{
    return (1 << m_resolutionBits) - 1;
}

int AdcComponent::codeForVoltage(double inputVoltage,
                                 double referenceLow,
                                 double referenceHigh) const
{
    const double normalized = std::clamp((inputVoltage - referenceLow) /
                                             (referenceHigh - referenceLow),
                                         0.0,
                                         1.0);
    return static_cast<int>(std::lround(normalized * static_cast<double>(maximumCode())));
}

void AdcComponent::scheduleCode(int code, double timeSeconds)
{
    m_pendingCode = std::clamp(code, 0, maximumCode());
    m_applyAtSeconds = timeSeconds + m_conversionDelayMilliseconds / 1000.0;
    m_conversionPending = true;
}

QString DacComponent::typeId() const
{
    return QStringLiteral("DAC");
}

QVector<ComponentPinDefinition> DacComponent::pinDefinitions() const
{
    QVector<ComponentPinDefinition> pins;
    for (int bit = 0; bit < m_resolutionBits; ++bit)
        pins.append({QStringLiteral("D%1").arg(bit), PinDirection::Input});
    pins.append({QStringLiteral("VREF-"), PinDirection::Power});
    pins.append({QStringLiteral("VREF+"), PinDirection::Power});
    pins.append({QStringLiteral("VOUT"), PinDirection::Output});
    return pins;
}

QVector<ComponentProperty> DacComponent::editableProperties() const
{
    return {integerProperty(QStringLiteral("resolutionBits"),
                            QStringLiteral("Resolution"),
                            m_resolutionBits,
                            2,
                            12,
                            1,
                            QStringLiteral(" bit")),
            numberProperty(QStringLiteral("conversionDelayMs"),
                           QStringLiteral("Conversion delay"),
                           m_conversionDelayMilliseconds,
                           0.0,
                           1000.0,
                           0.1,
                           QStringLiteral(" ms"))};
}

bool DacComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("resolutionBits"))
    {
        m_resolutionBits = std::clamp(value.toInt(), 2, 12);
        m_inputCode = std::clamp(m_inputCode, 0, maximumCode());
        m_pendingCode = std::clamp(m_pendingCode, 0, maximumCode());
        return true;
    }
    if (key == QStringLiteral("conversionDelayMs"))
    {
        m_conversionDelayMilliseconds = std::clamp(value.toDouble(), 0.0, 1000.0);
        return true;
    }
    return false;
}

QVariant DacComponent::property(const QString &key) const
{
    if (key == QStringLiteral("resolutionBits"))
        return m_resolutionBits;
    if (key == QStringLiteral("conversionDelayMs"))
        return m_conversionDelayMilliseconds;
    return {};
}

QString DacComponent::valueText() const
{
    return QStringLiteral("%1-bit DAC").arg(m_resolutionBits);
}

QString DacComponent::runtimeText() const
{
    if (!m_hasValidOutput)
    {
        if (m_conversionPending)
            return QStringLiteral("converting, output not ready");
        return QStringLiteral("waiting for digital bus and references");
    }

    QString text = QStringLiteral("code=%1/%2, Vout=%3 V")
                       .arg(m_inputCode)
                       .arg(maximumCode())
                       .arg(numberText(m_outputVoltage));
    if (m_conversionPending)
        text += QStringLiteral(" (conversion pending)");
    return text;
}

ComponentStepResult DacComponent::step(const QVector<std::optional<double>> &pinVoltages,
                                       double timeSeconds)
{
    ComponentStepResult result = emptyResult(pinDefinitions().size());
    QStringList warnings;
    const std::optional<int> code = inputCode(pinVoltages, warnings);
    const int referenceLowIndex = m_resolutionBits;
    const int referenceHighIndex = m_resolutionBits + 1;
    const int outputIndex = m_resolutionBits + 2;
    const std::optional<double> referenceLow = voltageAt(pinVoltages, referenceLowIndex);
    const std::optional<double> referenceHigh = voltageAt(pinVoltages, referenceHighIndex);

    if (!referenceLow.has_value() || !referenceHigh.has_value())
        appendUnique(warnings, QStringLiteral("DAC reference pins are not fully connected."));

    if (code.has_value() && referenceLow.has_value() && referenceHigh.has_value())
    {
        if (*referenceHigh <= *referenceLow)
        {
            appendUnique(warnings,
                         QStringLiteral("DAC requires VREF+ to be greater than VREF-."));
        }
        else
        {
            const double normalized = static_cast<double>(*code) / static_cast<double>(maximumCode());
            const double targetVoltage = *referenceLow + normalized * (*referenceHigh - *referenceLow);
            if (m_conversionPending)
            {
                if (*code != m_pendingCode ||
                    std::abs(targetVoltage - m_pendingVoltage) > 1e-9)
                {
                    scheduleVoltage(*code, targetVoltage, timeSeconds);
                }
            }
            else if (!m_hasValidOutput || *code != m_inputCode ||
                     std::abs(targetVoltage - m_outputVoltage) > 1e-9)
            {
                scheduleVoltage(*code, targetVoltage, timeSeconds);
            }
        }
    }

    if (m_conversionPending && timeSeconds + 1e-12 >= m_applyAtSeconds)
    {
        m_inputCode = m_pendingCode;
        m_outputVoltage = m_pendingVoltage;
        m_hasValidOutput = true;
        m_conversionPending = false;
    }

    if (m_hasValidOutput)
    {
        result.pinVoltages[outputIndex] = m_outputVoltage;
        result.drivenPins[outputIndex] = true;
    }
    result.warnings = warnings;
    return result;
}

bool DacComponent::active() const
{
    return m_hasValidOutput && m_outputVoltage > 0.001;
}

QVariantMap DacComponent::saveState() const
{
    return {{QStringLiteral("inputCode"), m_inputCode},
            {QStringLiteral("pendingCode"), m_pendingCode},
            {QStringLiteral("outputVoltage"), m_outputVoltage},
            {QStringLiteral("pendingVoltage"), m_pendingVoltage},
            {QStringLiteral("applyAtSeconds"), m_applyAtSeconds},
            {QStringLiteral("hasValidOutput"), m_hasValidOutput},
            {QStringLiteral("conversionPending"), m_conversionPending}};
}

void DacComponent::loadState(const QVariantMap &state)
{
    m_inputCode = std::clamp(state.value(QStringLiteral("inputCode"), 0).toInt(), 0, maximumCode());
    m_pendingCode = std::clamp(state.value(QStringLiteral("pendingCode"), 0).toInt(), 0, maximumCode());
    m_outputVoltage = state.value(QStringLiteral("outputVoltage"), 0.0).toDouble();
    m_pendingVoltage = state.value(QStringLiteral("pendingVoltage"), 0.0).toDouble();
    m_applyAtSeconds = state.value(QStringLiteral("applyAtSeconds"), 0.0).toDouble();
    m_hasValidOutput = state.value(QStringLiteral("hasValidOutput"), false).toBool();
    m_conversionPending = state.value(QStringLiteral("conversionPending"), false).toBool();
}

int DacComponent::maximumCode() const
{
    return (1 << m_resolutionBits) - 1;
}

std::optional<int> DacComponent::inputCode(const QVector<std::optional<double>> &pinVoltages,
                                           QStringList &warnings) const
{
    int code = 0;
    for (int bit = 0; bit < m_resolutionBits; ++bit)
    {
        const LogicLevel level = LogicThresholds::fromVoltage(voltageAt(pinVoltages, bit));
        if (level == LogicLevel::Undefined)
        {
            appendUnique(warnings,
                         QStringLiteral("DAC digital input D%1 is floating or undefined.").arg(bit));
            return std::nullopt;
        }
        if (level == LogicLevel::High)
            code |= (1 << bit);
    }
    return code;
}

void DacComponent::scheduleVoltage(int code, double voltage, double timeSeconds)
{
    m_pendingCode = std::clamp(code, 0, maximumCode());
    m_pendingVoltage = voltage;
    m_applyAtSeconds = timeSeconds + m_conversionDelayMilliseconds / 1000.0;
    m_conversionPending = true;
}

std::unique_ptr<Component> createConverterComponent(const QString &type)
{
    if (type == QStringLiteral("ADC"))
        return std::make_unique<AdcComponent>();
    if (type == QStringLiteral("DAC"))
        return std::make_unique<DacComponent>();
    return {};
}
