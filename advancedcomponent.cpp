#include "advancedcomponent.h"

#include "convertercomponents.h"
#include "firmwareloader.h"

#include <QFileInfo>

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

void appendUnique(QStringList &messages, const QString &message)
{
    if (!message.isEmpty() && !messages.contains(message))
        messages.append(message);
}

bool isLow(const std::optional<double> &voltage)
{
    return LogicThresholds::fromVoltage(voltage) == LogicLevel::Low;
}

bool isHigh(const std::optional<double> &voltage)
{
    return LogicThresholds::fromVoltage(voltage) == LogicLevel::High;
}

QString hexByte(quint8 value)
{
    return QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

QString printableLine(const QString &line)
{
    QString copy = line;
    while (copy.endsWith(QLatin1Char(' ')))
        copy.chop(1);
    return copy;
}
}

QString MicrocontrollerComponent::typeId() const
{
    return QStringLiteral("MCU");
}

QVector<ComponentPinDefinition> MicrocontrollerComponent::pinDefinitions() const
{
    QVector<ComponentPinDefinition> pins;
    for (int port = 0; port < 3; ++port)
    {
        for (int bit = 0; bit < 8; ++bit)
            pins.append({QStringLiteral("P%1%2").arg(QChar(static_cast<ushort>('A' + port))).arg(bit), PinDirection::Passive});
    }
    pins.append({QStringLiteral("VCC"), PinDirection::Power});
    pins.append({QStringLiteral("GND"), PinDirection::Power});
    return pins;
}

QVector<ComponentProperty> MicrocontrollerComponent::editableProperties() const
{
    return {fileProperty(QStringLiteral("firmwarePath"),
                         QStringLiteral("Firmware (.hex)"),
                         m_firmwarePath),
            numberProperty(QStringLiteral("clockFrequency"),
                           QStringLiteral("Instruction clock"),
                           m_clockFrequency,
                           1.0,
                           10000.0,
                           1.0,
                           QStringLiteral(" Hz")),
            booleanProperty(QStringLiteral("autoRun"), QStringLiteral("Run firmware"), m_autoRun),
            booleanProperty(QStringLiteral("requirePower"),
                            QStringLiteral("Require VCC and GND"),
                            m_requirePower)};
}

bool MicrocontrollerComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("firmwarePath"))
    {
        const QString path = value.toString().trimmed();
        if (path.isEmpty())
        {
            m_firmwarePath.clear();
            m_firmwareStatus = QStringLiteral("No firmware loaded");
            m_core.loadFlash({});
            m_lastEvaluationTime = -1.0;
            m_cycleRemainder = 0.0;
            return true;
        }
        loadFirmware(path);
        return true;
    }
    if (key == QStringLiteral("clockFrequency"))
    {
        m_clockFrequency = std::clamp(value.toDouble(), 1.0, 10000.0);
        return true;
    }
    if (key == QStringLiteral("autoRun"))
    {
        m_autoRun = value.toBool();
        return true;
    }
    if (key == QStringLiteral("requirePower"))
    {
        m_requirePower = value.toBool();
        return true;
    }
    return false;
}

QVariant MicrocontrollerComponent::property(const QString &key) const
{
    if (key == QStringLiteral("firmwarePath"))
        return m_firmwarePath;
    if (key == QStringLiteral("clockFrequency"))
        return m_clockFrequency;
    if (key == QStringLiteral("autoRun"))
        return m_autoRun;
    if (key == QStringLiteral("requirePower"))
        return m_requirePower;
    return {};
}

QString MicrocontrollerComponent::valueText() const
{
    if (m_firmwarePath.isEmpty())
        return QStringLiteral("firmware=empty");
    return QStringLiteral("firmware=%1").arg(QFileInfo(m_firmwarePath).fileName());
}

QString MicrocontrollerComponent::runtimeText() const
{
    if (m_core.flashSize() == 0)
        return m_firmwareStatus;

    const QString state = m_core.halted() ? QStringLiteral("HALTED")
                                         : (m_autoRun ? QStringLiteral("RUN") : QStringLiteral("PAUSED"));
    return QStringLiteral("%1 PC=%2 A=%3 B=%4 PORTA=%5 PORTB=%6 PORTC=%7 | %8")
        .arg(state)
        .arg(m_core.programCounter(), 4, 16, QLatin1Char('0'))
        .arg(hexByte(m_core.accumulator()))
        .arg(hexByte(m_core.generalRegister()))
        .arg(hexByte(m_core.port(0).read()))
        .arg(hexByte(m_core.port(1).read()))
        .arg(hexByte(m_core.port(2).read()))
        .arg(m_core.lastTrace().isEmpty() ? m_firmwareStatus : m_core.lastTrace());
}

ComponentStepResult MicrocontrollerComponent::step(
    const QVector<std::optional<double>> &pinVoltages,
    double timeSeconds)
{
    ComponentStepResult result = emptyResult(pinDefinitions().size());
    QStringList warnings;

    for (int port = 0; port < 3; ++port)
        m_core.setPortInput(port, samplePort(pinVoltages, port, warnings));

    const int vccIndex = 24;
    const int groundIndex = 25;
    bool powered = true;
    if (m_requirePower)
    {
        const std::optional<double> vcc = voltageAt(pinVoltages, vccIndex);
        const std::optional<double> ground = voltageAt(pinVoltages, groundIndex);
        powered = vcc.has_value() && ground.has_value() && (*vcc - *ground) >= 2.0;
        if (!powered)
            appendUnique(warnings, QStringLiteral("MCU VCC and GND are not connected to a valid supply."));
    }

    if (m_core.flashSize() == 0)
        appendUnique(warnings, m_firmwareStatus);

    if (m_lastEvaluationTime < 0.0)
        m_lastEvaluationTime = timeSeconds;

    if (powered && m_autoRun && !m_core.halted() && m_core.flashSize() > 0 &&
        timeSeconds > m_lastEvaluationTime + 1e-12)
    {
        const double elapsed = std::clamp(timeSeconds - m_lastEvaluationTime, 0.0, 0.25);
        const double requestedCycles = elapsed * m_clockFrequency + m_cycleRemainder;
        int cycles = static_cast<int>(std::floor(requestedCycles));
        m_cycleRemainder = requestedCycles - static_cast<double>(cycles);
        cycles = std::clamp(cycles, 0, 200);

        for (int cycle = 0; cycle < cycles && !m_core.halted(); ++cycle)
        {
            const McuInstructionResult instruction = m_core.executeNext();
            if (!instruction.error.isEmpty())
            {
                appendUnique(warnings, instruction.error);
                break;
            }
        }
    }

    if (timeSeconds > m_lastEvaluationTime)
        m_lastEvaluationTime = timeSeconds;

    for (int port = 0; port < 3; ++port)
        drivePort(result, port);
    result.warnings = warnings;
    return result;
}

bool MicrocontrollerComponent::active() const
{
    return m_core.flashSize() > 0 && m_autoRun && !m_core.halted();
}

QVariantMap MicrocontrollerComponent::saveState() const
{
    return {{QStringLiteral("firmwarePath"), m_firmwarePath},
            {QStringLiteral("clockFrequency"), m_clockFrequency},
            {QStringLiteral("autoRun"), m_autoRun},
            {QStringLiteral("requirePower"), m_requirePower},
            {QStringLiteral("lastEvaluationTime"), m_lastEvaluationTime},
            {QStringLiteral("cycleRemainder"), m_cycleRemainder},
            {QStringLiteral("core"), m_core.saveState()}};
}

void MicrocontrollerComponent::loadState(const QVariantMap &state)
{
    m_clockFrequency = state.value(QStringLiteral("clockFrequency"), 20.0).toDouble();
    m_autoRun = state.value(QStringLiteral("autoRun"), true).toBool();
    m_requirePower = state.value(QStringLiteral("requirePower"), true).toBool();
    const QString path = state.value(QStringLiteral("firmwarePath")).toString();
    if (!path.isEmpty())
        loadFirmware(path);
    m_core.loadState(state.value(QStringLiteral("core")).toMap());
    m_lastEvaluationTime = state.value(QStringLiteral("lastEvaluationTime"), -1.0).toDouble();
    m_cycleRemainder = state.value(QStringLiteral("cycleRemainder"), 0.0).toDouble();
}

QString MicrocontrollerComponent::firmwareStatus() const
{
    return m_firmwareStatus;
}

QString MicrocontrollerComponent::firmwarePath() const
{
    return m_firmwarePath;
}

bool MicrocontrollerComponent::loadFirmware(const QString &filePath)
{
    const FirmwareLoadResult loaded = IntelHexLoader::loadFile(filePath);
    m_firmwarePath = filePath;
    m_lastEvaluationTime = -1.0;
    m_cycleRemainder = 0.0;
    if (!loaded.success)
    {
        m_firmwareStatus = QStringLiteral("Firmware error: %1").arg(loaded.error);
        m_core.loadFlash({});
        return false;
    }

    m_core.loadFlash(loaded.flashBytes);
    m_firmwareStatus = loaded.notes.isEmpty() ? QStringLiteral("Firmware loaded") : loaded.notes.last();
    return true;
}

quint8 MicrocontrollerComponent::samplePort(const QVector<std::optional<double>> &pinVoltages,
                                            int portIndex,
                                            QStringList &warnings) const
{
    quint8 sample = 0;
    const int firstPin = portIndex * 8;
    for (int bit = 0; bit < 8; ++bit)
    {
        if (m_core.port(portIndex).drivesBit(bit))
            continue;

        const LogicLevel level = LogicThresholds::fromVoltage(voltageAt(pinVoltages, firstPin + bit));
        if (level == LogicLevel::High)
            sample = static_cast<quint8>(sample | static_cast<quint8>(1u << bit));
        else if (level == LogicLevel::Undefined)
            appendUnique(warnings,
                         QStringLiteral("MCU input P%1%2 is floating or undefined.")
                             .arg(QChar(static_cast<ushort>('A' + portIndex)))
                             .arg(bit));
    }
    return sample;
}

void MicrocontrollerComponent::drivePort(ComponentStepResult &result, int portIndex) const
{
    const McuPort &currentPort = m_core.port(portIndex);
    const int firstPin = portIndex * 8;
    for (int bit = 0; bit < 8; ++bit)
    {
        if (!currentPort.drivesBit(bit))
            continue;
        result.drivenPins[firstPin + bit] = true;
        result.pinVoltages[firstPin + bit] = currentPort.outputBit(bit)
                                                   ? LogicThresholds::highVoltage()
                                                   : LogicThresholds::lowVoltage();
    }
}

ExternalMemoryComponent::ExternalMemoryComponent()
    : m_memory(256, static_cast<char>(0xFF))
{
}

QString ExternalMemoryComponent::typeId() const
{
    return QStringLiteral("EEPROM");
}

QVector<ComponentPinDefinition> ExternalMemoryComponent::pinDefinitions() const
{
    QVector<ComponentPinDefinition> pins;
    for (int bit = 0; bit < 8; ++bit)
        pins.append({QStringLiteral("A%1").arg(bit), PinDirection::Input});
    for (int bit = 0; bit < 8; ++bit)
        pins.append({QStringLiteral("D%1").arg(bit), PinDirection::Passive});
    pins.append({QStringLiteral("/CS"), PinDirection::Input});
    pins.append({QStringLiteral("/RD"), PinDirection::Input});
    pins.append({QStringLiteral("/WR"), PinDirection::Input});
    return pins;
}

QVector<ComponentProperty> ExternalMemoryComponent::editableProperties() const
{
    return {integerProperty(QStringLiteral("sizeBytes"),
                            QStringLiteral("Memory size"),
                            m_memory.size(),
                            16,
                            256,
                            16,
                            QStringLiteral(" B")),
            integerProperty(QStringLiteral("fillValue"),
                            QStringLiteral("Fill value"),
                            m_fillValue,
                            0,
                            255)};
}

bool ExternalMemoryComponent::setProperty(const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("sizeBytes"))
    {
        resizeMemory(std::clamp(value.toInt(), 16, 256), static_cast<quint8>(m_fillValue));
        return true;
    }
    if (key == QStringLiteral("fillValue"))
    {
        m_fillValue = std::clamp(value.toInt(), 0, 255);
        return true;
    }
    return false;
}

QVariant ExternalMemoryComponent::property(const QString &key) const
{
    if (key == QStringLiteral("sizeBytes"))
        return m_memory.size();
    if (key == QStringLiteral("fillValue"))
        return m_fillValue;
    return {};
}

QString ExternalMemoryComponent::valueText() const
{
    return QStringLiteral("%1 B external memory").arg(m_memory.size());
}

QString ExternalMemoryComponent::runtimeText() const
{
    return m_lastOperation;
}

ComponentStepResult ExternalMemoryComponent::step(
    const QVector<std::optional<double>> &pinVoltages,
    double timeSeconds)
{
    Q_UNUSED(timeSeconds);
    ComponentStepResult result = emptyResult(pinDefinitions().size());
    QStringList warnings;

    const LogicLevel chipSelect = LogicThresholds::fromVoltage(voltageAt(pinVoltages, 16));
    if (chipSelect == LogicLevel::Undefined)
    {
        appendUnique(warnings, QStringLiteral("External memory /CS pin is floating or undefined."));
        m_writeLatched = false;
        result.warnings = warnings;
        return result;
    }
    if (chipSelect == LogicLevel::High)
    {
        m_writeLatched = false;
        result.warnings = warnings;
        return result;
    }

    const LogicLevel readEnable = LogicThresholds::fromVoltage(voltageAt(pinVoltages, 17));
    const LogicLevel writeEnable = LogicThresholds::fromVoltage(voltageAt(pinVoltages, 18));
    if (readEnable == LogicLevel::Undefined || writeEnable == LogicLevel::Undefined)
    {
        appendUnique(warnings, QStringLiteral("External memory /RD or /WR pin is floating or undefined."));
        m_writeLatched = false;
        result.warnings = warnings;
        return result;
    }

    const bool selected = true;
    const bool reading = readEnable == LogicLevel::Low;
    const bool writing = writeEnable == LogicLevel::Low;
    if (reading && writing)
        appendUnique(warnings, QStringLiteral("External memory /RD and /WR cannot be active together."));

    const std::optional<int> address = readAddress(pinVoltages, warnings);
    if (address.has_value() && *address >= m_memory.size())
        appendUnique(warnings, QStringLiteral("External memory address is outside the configured range."));

    if (selected && writing && !reading && address.has_value() && *address < m_memory.size())
    {
        if (!m_writeLatched)
        {
            const std::optional<quint8> data = readData(pinVoltages, warnings);
            if (data.has_value())
            {
                m_memory[*address] = static_cast<char>(*data);
                m_lastOperation = QStringLiteral("write 0x%1 -> [0x%2]")
                                      .arg(hexByte(*data))
                                      .arg(*address, 2, 16, QLatin1Char('0'));
                m_writeLatched = true;
            }
        }
    }
    else
    {
        m_writeLatched = false;
    }

    if (selected && reading && !writing && address.has_value() && *address < m_memory.size())
    {
        const quint8 data = static_cast<quint8>(m_memory.at(*address));
        for (int bit = 0; bit < 8; ++bit)
        {
            result.drivenPins[8 + bit] = true;
            result.pinVoltages[8 + bit] = ((data >> bit) & 0x1u) != 0u
                                               ? LogicThresholds::highVoltage()
                                               : LogicThresholds::lowVoltage();
        }
        m_lastOperation = QStringLiteral("read [0x%1] -> 0x%2")
                              .arg(*address, 2, 16, QLatin1Char('0'))
                              .arg(hexByte(data));
    }

    result.warnings = warnings;
    return result;
}

QVariantMap ExternalMemoryComponent::saveState() const
{
    return {{QStringLiteral("memory"), m_memory},
            {QStringLiteral("fillValue"), m_fillValue},
            {QStringLiteral("lastOperation"), m_lastOperation}};
}

void ExternalMemoryComponent::loadState(const QVariantMap &state)
{
    const QByteArray stored = state.value(QStringLiteral("memory")).toByteArray();
    if (!stored.isEmpty())
        m_memory = stored;
    m_fillValue = std::clamp(state.value(QStringLiteral("fillValue"), 255).toInt(), 0, 255);
    m_lastOperation = state.value(QStringLiteral("lastOperation"), QStringLiteral("idle")).toString();
}

std::optional<int> ExternalMemoryComponent::readAddress(
    const QVector<std::optional<double>> &pinVoltages,
    QStringList &warnings) const
{
    int address = 0;
    for (int bit = 0; bit < 8; ++bit)
    {
        const LogicLevel level = LogicThresholds::fromVoltage(voltageAt(pinVoltages, bit));
        if (level == LogicLevel::Undefined)
        {
            appendUnique(warnings,
                         QStringLiteral("External memory address bit A%1 is undefined.").arg(bit));
            return std::nullopt;
        }
        if (level == LogicLevel::High)
            address |= (1 << bit);
    }
    return address;
}

std::optional<quint8> ExternalMemoryComponent::readData(
    const QVector<std::optional<double>> &pinVoltages,
    QStringList &warnings) const
{
    quint8 data = 0;
    for (int bit = 0; bit < 8; ++bit)
    {
        const LogicLevel level = LogicThresholds::fromVoltage(voltageAt(pinVoltages, 8 + bit));
        if (level == LogicLevel::Undefined)
        {
            appendUnique(warnings,
                         QStringLiteral("External memory data bit D%1 is undefined during write.").arg(bit));
            return std::nullopt;
        }
        if (level == LogicLevel::High)
            data = static_cast<quint8>(data | static_cast<quint8>(1u << bit));
    }
    return data;
}

void ExternalMemoryComponent::resizeMemory(int size, quint8 fillValue)
{
    const int oldSize = m_memory.size();
    m_memory.resize(size);
    if (size > oldSize)
        std::fill(m_memory.begin() + oldSize, m_memory.end(), static_cast<char>(fillValue));
}

QString Lcd16x2Component::typeId() const
{
    return QStringLiteral("LCD16x2");
}

QVector<ComponentPinDefinition> Lcd16x2Component::pinDefinitions() const
{
    QVector<ComponentPinDefinition> pins{{QStringLiteral("RS"), PinDirection::Input},
                                         {QStringLiteral("RW"), PinDirection::Input},
                                         {QStringLiteral("E"), PinDirection::Input}};
    for (int bit = 0; bit < 8; ++bit)
        pins.append({QStringLiteral("D%1").arg(bit), PinDirection::Input});
    return pins;
}

QVector<ComponentProperty> Lcd16x2Component::editableProperties() const
{
    return {textProperty(QStringLiteral("initialText"),
                         QStringLiteral("Initial text"),
                         m_initialText),
            booleanProperty(QStringLiteral("displayOn"), QStringLiteral("Display enabled"), m_displayOn)};
}

bool Lcd16x2Component::setProperty(const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("initialText"))
    {
        setInitialText(value.toString());
        return true;
    }
    if (key == QStringLiteral("displayOn"))
    {
        m_displayOn = value.toBool();
        return true;
    }
    return false;
}

QVariant Lcd16x2Component::property(const QString &key) const
{
    if (key == QStringLiteral("initialText"))
        return m_initialText;
    if (key == QStringLiteral("displayOn"))
        return m_displayOn;
    return {};
}

QString Lcd16x2Component::valueText() const
{
    return QStringLiteral("LCD 16x2");
}

QString Lcd16x2Component::runtimeText() const
{
    if (!m_displayOn)
        return QStringLiteral("display off");
    return QStringLiteral("\"%1\" / \"%2\"")
        .arg(printableLine(m_lines.value(0)), printableLine(m_lines.value(1)));
}

ComponentStepResult Lcd16x2Component::step(const QVector<std::optional<double>> &pinVoltages,
                                           double timeSeconds)
{
    Q_UNUSED(timeSeconds);
    ComponentStepResult result = emptyResult(pinDefinitions().size());
    const LogicLevel enable = LogicThresholds::fromVoltage(voltageAt(pinVoltages, 2));
    const LogicLevel registerSelect = LogicThresholds::fromVoltage(voltageAt(pinVoltages, 0));
    const LogicLevel readWrite = LogicThresholds::fromVoltage(voltageAt(pinVoltages, 1));

    if (enable == LogicLevel::Undefined)
    {
        appendUnique(result.warnings, QStringLiteral("LCD enable pin is floating or undefined."));
        m_previousEnable = enable;
        return result;
    }

    const bool risingEdge = m_previousEnable == LogicLevel::Low && enable == LogicLevel::High;
    m_previousEnable = enable;
    if (!risingEdge)
        return result;

    if (registerSelect == LogicLevel::Undefined || readWrite == LogicLevel::Undefined)
    {
        appendUnique(result.warnings, QStringLiteral("LCD RS or RW pin is floating or undefined."));
        return result;
    }
    if (readWrite == LogicLevel::High)
    {
        appendUnique(result.warnings, QStringLiteral("LCD read cycles are not required by this educational model."));
        return result;
    }

    const std::optional<quint8> data = readDataBus(pinVoltages, result.warnings);
    if (!data.has_value())
        return result;

    if (registerSelect == LogicLevel::Low)
        executeCommand(*data);
    else
        writeCharacter(*data);
    return result;
}

bool Lcd16x2Component::active() const
{
    if (!m_displayOn)
        return false;
    return std::any_of(m_lines.cbegin(), m_lines.cend(), [](const QString &line)
                       { return !line.trimmed().isEmpty(); });
}

QStringList Lcd16x2Component::displayLines() const
{
    if (!m_displayOn)
        return {QString(16, QLatin1Char(' ')), QString(16, QLatin1Char(' '))};
    return m_lines;
}

QVariantMap Lcd16x2Component::saveState() const
{
    return {{QStringLiteral("lines"), m_lines},
            {QStringLiteral("initialText"), m_initialText},
            {QStringLiteral("cursorAddress"), m_cursorAddress},
            {QStringLiteral("displayOn"), m_displayOn},
            {QStringLiteral("incrementCursor"), m_incrementCursor},
            {QStringLiteral("previousEnable"), static_cast<int>(m_previousEnable)},
            {QStringLiteral("lastOperation"), m_lastOperation}};
}

void Lcd16x2Component::loadState(const QVariantMap &state)
{
    m_lines = state.value(QStringLiteral("lines")).toStringList();
    m_initialText = state.value(QStringLiteral("initialText"), QStringLiteral("Hello")).toString();
    m_cursorAddress = state.value(QStringLiteral("cursorAddress"), 0).toInt();
    m_displayOn = state.value(QStringLiteral("displayOn"), true).toBool();
    m_incrementCursor = state.value(QStringLiteral("incrementCursor"), true).toBool();
    m_previousEnable = static_cast<LogicLevel>(
        state.value(QStringLiteral("previousEnable"), static_cast<int>(LogicLevel::Undefined)).toInt());
    m_lastOperation = state.value(QStringLiteral("lastOperation"), QStringLiteral("idle")).toString();
    normalizeLines();
}

std::optional<quint8> Lcd16x2Component::readDataBus(
    const QVector<std::optional<double>> &pinVoltages,
    QStringList &warnings) const
{
    quint8 value = 0;
    for (int bit = 0; bit < 8; ++bit)
    {
        const LogicLevel level = LogicThresholds::fromVoltage(voltageAt(pinVoltages, 3 + bit));
        if (level == LogicLevel::Undefined)
        {
            appendUnique(warnings, QStringLiteral("LCD data bit D%1 is floating or undefined.").arg(bit));
            return std::nullopt;
        }
        if (level == LogicLevel::High)
            value = static_cast<quint8>(value | static_cast<quint8>(1u << bit));
    }
    return value;
}

void Lcd16x2Component::executeCommand(quint8 command)
{
    if (command == 0x01)
    {
        m_lines = {QString(16, QLatin1Char(' ')), QString(16, QLatin1Char(' '))};
        m_cursorAddress = 0;
        m_lastOperation = QStringLiteral("clear display");
    }
    else if (command == 0x02)
    {
        m_cursorAddress = 0;
        m_lastOperation = QStringLiteral("cursor home");
    }
    else if ((command & 0x80u) != 0u)
    {
        m_cursorAddress = command & 0x7Fu;
        m_lastOperation = QStringLiteral("set cursor 0x%1")
                              .arg(m_cursorAddress, 2, 16, QLatin1Char('0'));
    }
    else if ((command & 0xF8u) == 0x08u)
    {
        m_displayOn = (command & 0x04u) != 0u;
        m_lastOperation = m_displayOn ? QStringLiteral("display on") : QStringLiteral("display off");
    }
    else if ((command & 0xFCu) == 0x04u)
    {
        m_incrementCursor = (command & 0x02u) != 0u;
        m_lastOperation = m_incrementCursor ? QStringLiteral("entry mode increment")
                                            : QStringLiteral("entry mode decrement");
    }
    else
    {
        m_lastOperation = QStringLiteral("command 0x%1").arg(command, 2, 16, QLatin1Char('0'));
    }
}

void Lcd16x2Component::writeCharacter(quint8 value)
{
    int row = 0;
    int column = m_cursorAddress;
    if (m_cursorAddress >= 0x40)
    {
        row = 1;
        column = m_cursorAddress - 0x40;
    }
    if (row >= 0 && row < 2 && column >= 0 && column < 16)
        m_lines[row][column] = QChar(static_cast<ushort>(value));

    if (m_incrementCursor)
        ++m_cursorAddress;
    else
        --m_cursorAddress;
    m_lastOperation = QStringLiteral("write '%1'").arg(QChar(static_cast<ushort>(value)));
}

void Lcd16x2Component::setInitialText(const QString &text)
{
    m_initialText = text;
    const QStringList requestedLines = text.split(QLatin1Char('\n'));
    m_lines = {requestedLines.value(0).left(16).leftJustified(16, QLatin1Char(' ')),
               requestedLines.value(1).left(16).leftJustified(16, QLatin1Char(' '))};
    m_cursorAddress = 0;
    m_lastOperation = QStringLiteral("initial text applied");
}

void Lcd16x2Component::normalizeLines()
{
    while (m_lines.size() < 2)
        m_lines.append(QString());
    if (m_lines.size() > 2)
        m_lines = m_lines.mid(0, 2);
    for (QString &line : m_lines)
        line = line.left(16).leftJustified(16, QLatin1Char(' '));
}

QString MatrixKeypadComponent::typeId() const
{
    return QStringLiteral("Keypad4x4");
}

QVector<ComponentPinDefinition> MatrixKeypadComponent::pinDefinitions() const
{
    return {{QStringLiteral("R1"), PinDirection::Passive},
            {QStringLiteral("R2"), PinDirection::Passive},
            {QStringLiteral("R3"), PinDirection::Passive},
            {QStringLiteral("R4"), PinDirection::Passive},
            {QStringLiteral("C1"), PinDirection::Passive},
            {QStringLiteral("C2"), PinDirection::Passive},
            {QStringLiteral("C3"), PinDirection::Passive},
            {QStringLiteral("C4"), PinDirection::Passive}};
}

QVector<ComponentProperty> MatrixKeypadComponent::editableProperties() const
{
    return {};
}

bool MatrixKeypadComponent::setProperty(const QString &key, const QVariant &value)
{
    Q_UNUSED(key);
    Q_UNUSED(value);
    return false;
}

QVariant MatrixKeypadComponent::property(const QString &key) const
{
    Q_UNUSED(key);
    return {};
}

QString MatrixKeypadComponent::valueText() const
{
    return QStringLiteral("4x4 matrix keypad");
}

QString MatrixKeypadComponent::runtimeText() const
{
    return active() ? QStringLiteral("pressed=%1").arg(keyLabel(m_pressedRow, m_pressedColumn))
                    : QStringLiteral("no key pressed");
}

ComponentStepResult MatrixKeypadComponent::step(
    const QVector<std::optional<double>> &pinVoltages,
    double timeSeconds)
{
    Q_UNUSED(pinVoltages);
    Q_UNUSED(timeSeconds);
    return emptyResult(pinDefinitions().size());
}

QVector<QPair<int, int>> MatrixKeypadComponent::conductivePinPairs() const
{
    if (!active())
        return {};
    return {qMakePair(m_pressedRow, 4 + m_pressedColumn)};
}

void MatrixKeypadComponent::pressKey(int row, int column)
{
    if (row < 0 || row >= 4 || column < 0 || column >= 4)
        return;
    m_pressedRow = row;
    m_pressedColumn = column;
}

void MatrixKeypadComponent::releaseKey()
{
    m_pressedRow = -1;
    m_pressedColumn = -1;
}

bool MatrixKeypadComponent::active() const
{
    return m_pressedRow >= 0 && m_pressedRow < 4 && m_pressedColumn >= 0 && m_pressedColumn < 4;
}

QVector<bool> MatrixKeypadComponent::keyStates() const
{
    QVector<bool> states(16, false);
    if (active())
        states[m_pressedRow * 4 + m_pressedColumn] = true;
    return states;
}

QVariantMap MatrixKeypadComponent::saveState() const
{
    return {{QStringLiteral("pressedRow"), m_pressedRow},
            {QStringLiteral("pressedColumn"), m_pressedColumn}};
}

void MatrixKeypadComponent::loadState(const QVariantMap &state)
{
    m_pressedRow = state.value(QStringLiteral("pressedRow"), -1).toInt();
    m_pressedColumn = state.value(QStringLiteral("pressedColumn"), -1).toInt();
    if (!active())
        releaseKey();
}

QString MatrixKeypadComponent::keyLabel(int row, int column) const
{
    static const QVector<QString> labels{QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
                                         QStringLiteral("A"), QStringLiteral("4"), QStringLiteral("5"),
                                         QStringLiteral("6"), QStringLiteral("B"), QStringLiteral("7"),
                                         QStringLiteral("8"), QStringLiteral("9"), QStringLiteral("C"),
                                         QStringLiteral("*"), QStringLiteral("0"), QStringLiteral("#"),
                                         QStringLiteral("D")};
    const int index = row * 4 + column;
    return index >= 0 && index < labels.size() ? labels.at(index) : QStringLiteral("?");
}

std::unique_ptr<Component> createAdvancedComponent(const QString &type)
{
    if (std::unique_ptr<Component> converter = createConverterComponent(type))
        return converter;
    if (type == QStringLiteral("MCU"))
        return std::make_unique<MicrocontrollerComponent>();
    if (type == QStringLiteral("EEPROM"))
        return std::make_unique<ExternalMemoryComponent>();
    if (type == QStringLiteral("LCD16x2"))
        return std::make_unique<Lcd16x2Component>();
    if (type == QStringLiteral("Keypad4x4"))
        return std::make_unique<MatrixKeypadComponent>();
    return {};
}
