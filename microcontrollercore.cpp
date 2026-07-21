#include "microcontrollercore.h"

#include <algorithm>

quint16 ProgramCounter::value() const
{
    return m_value;
}

void ProgramCounter::setValue(quint16 value)
{
    m_value = value;
}

void ProgramCounter::advance(quint16 amount)
{
    m_value = static_cast<quint16>(m_value + amount);
}

void ProgramCounter::reset()
{
    m_value = 0;
}

quint8 RegisterBank::accumulator() const
{
    return m_accumulator;
}

quint8 RegisterBank::general() const
{
    return m_general;
}

void RegisterBank::setAccumulator(quint8 value)
{
    m_accumulator = value;
}

void RegisterBank::setGeneral(quint8 value)
{
    m_general = value;
}

void RegisterBank::reset()
{
    m_accumulator = 0;
    m_general = 0;
}

InternalRam::InternalRam(int size)
{
    m_bytes.fill('\0', std::max(1, size));
}

int InternalRam::size() const
{
    return m_bytes.size();
}

quint8 InternalRam::read(quint16 address) const
{
    if (m_bytes.isEmpty())
        return 0;
    return static_cast<quint8>(m_bytes.at(static_cast<int>(address) % m_bytes.size()));
}

void InternalRam::write(quint16 address, quint8 value)
{
    if (m_bytes.isEmpty())
        return;
    m_bytes[static_cast<int>(address) % m_bytes.size()] = static_cast<char>(value);
}

void InternalRam::clear()
{
    m_bytes.fill('\0');
}

QByteArray InternalRam::bytes() const
{
    return m_bytes;
}

void InternalRam::restore(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        clear();
    else
        m_bytes = bytes;
}

quint8 McuPort::directionMask() const
{
    return m_directionMask;
}

quint8 McuPort::outputLatch() const
{
    return m_outputLatch;
}

quint8 McuPort::inputSample() const
{
    return m_inputSample;
}

quint8 McuPort::read() const
{
    return static_cast<quint8>((m_outputLatch & m_directionMask) |
                               (m_inputSample & static_cast<quint8>(~m_directionMask)));
}

void McuPort::setDirectionMask(quint8 mask)
{
    m_directionMask = mask;
}

void McuPort::write(quint8 value)
{
    m_outputLatch = value;
}

void McuPort::setInputSample(quint8 value)
{
    m_inputSample = value;
}

void McuPort::setBit(int bit)
{
    if (bit >= 0 && bit < 8)
        m_outputLatch = static_cast<quint8>(m_outputLatch | static_cast<quint8>(1u << bit));
}

void McuPort::clearBit(int bit)
{
    if (bit >= 0 && bit < 8)
        m_outputLatch = static_cast<quint8>(m_outputLatch & static_cast<quint8>(~(1u << bit)));
}

bool McuPort::drivesBit(int bit) const
{
    return bit >= 0 && bit < 8 && ((m_directionMask >> bit) & 0x1u) != 0u;
}

bool McuPort::outputBit(int bit) const
{
    return bit >= 0 && bit < 8 && ((m_outputLatch >> bit) & 0x1u) != 0u;
}

void McuPort::reset()
{
    m_directionMask = 0;
    m_outputLatch = 0;
    m_inputSample = 0;
}

void EducationalMcuCore::loadFlash(const QByteArray &bytes)
{
    m_flash = bytes;
    reset();
    m_halted = m_flash.isEmpty();
}

void EducationalMcuCore::reset()
{
    m_programCounter.reset();
    m_registers.reset();
    m_ram.clear();
    for (McuPort &currentPort : m_ports)
        currentPort.reset();
    m_halted = m_flash.isEmpty();
    m_instructionCount = 0;
    m_lastTrace.clear();
}

McuInstructionResult EducationalMcuCore::executeNext()
{
    McuInstructionResult result;
    if (m_halted)
    {
        result.halted = true;
        return result;
    }

    const quint16 instructionAddress = m_programCounter.value();
    quint8 opcode = 0;
    if (!fetchByte(opcode))
    {
        m_halted = true;
        result.halted = true;
        result.error = QStringLiteral("Program counter moved outside loaded flash.");
        return result;
    }

    result.executed = true;
    ++m_instructionCount;

    if (opcode == 0x00)
    {
        result.trace = QStringLiteral("NOP");
    }
    else if (opcode == 0x10)
    {
        quint8 target = 0;
        quint8 immediate = 0;
        if (!fetchByte(target) || !fetchByte(immediate) || !validTarget(target))
            result.error = QStringLiteral("Malformed MOV immediate instruction.");
        else
        {
            writeTarget(target, immediate);
            result.trace = QStringLiteral("MOV %1, 0x%2")
                               .arg(targetName(target))
                               .arg(immediate, 2, 16, QLatin1Char('0'));
        }
    }
    else if (opcode == 0x11)
    {
        quint8 destination = 0;
        quint8 source = 0;
        if (!fetchByte(destination) || !fetchByte(source) || !validTarget(destination) ||
            !validTarget(source))
            result.error = QStringLiteral("Malformed MOV register instruction.");
        else
        {
            writeTarget(destination, readTarget(source));
            result.trace = QStringLiteral("MOV %1, %2").arg(targetName(destination), targetName(source));
        }
    }
    else if (opcode == 0x20)
    {
        quint8 target = 0;
        quint8 immediate = 0;
        if (!fetchByte(target) || !fetchByte(immediate) || !validTarget(target))
            result.error = QStringLiteral("Malformed ADD immediate instruction.");
        else
        {
            const quint8 sum = static_cast<quint8>(readTarget(target) + immediate);
            writeTarget(target, sum);
            result.trace = QStringLiteral("ADD %1, 0x%2")
                               .arg(targetName(target))
                               .arg(immediate, 2, 16, QLatin1Char('0'));
        }
    }
    else if (opcode == 0x21)
    {
        quint8 destination = 0;
        quint8 source = 0;
        if (!fetchByte(destination) || !fetchByte(source) || !validTarget(destination) ||
            !validTarget(source))
            result.error = QStringLiteral("Malformed ADD register instruction.");
        else
        {
            const quint8 sum = static_cast<quint8>(readTarget(destination) + readTarget(source));
            writeTarget(destination, sum);
            result.trace = QStringLiteral("ADD %1, %2").arg(targetName(destination), targetName(source));
        }
    }
    else if (opcode == 0x30)
    {
        quint16 address = 0;
        if (!fetchWord(address))
            result.error = QStringLiteral("Malformed JMP instruction.");
        else if (static_cast<int>(address) >= m_flash.size())
            result.error = QStringLiteral("JMP target is outside loaded flash.");
        else
        {
            m_programCounter.setValue(address);
            result.trace = QStringLiteral("JMP 0x%1").arg(address, 4, 16, QLatin1Char('0'));
        }
    }
    else if (opcode == 0x40 || opcode == 0x41)
    {
        quint8 portIndex = 0;
        quint8 bit = 0;
        if (!fetchByte(portIndex) || !fetchByte(bit) || portIndex >= m_ports.size() || bit >= 8)
            result.error = QStringLiteral("Malformed SETB/CLR instruction.");
        else
        {
            if (opcode == 0x40)
                m_ports[portIndex].setBit(bit);
            else
                m_ports[portIndex].clearBit(bit);
            result.trace = QStringLiteral("%1 P%2%3")
                               .arg(opcode == 0x40 ? QStringLiteral("SETB") : QStringLiteral("CLR"))
                               .arg(QChar(static_cast<ushort>('A' + portIndex)))
                               .arg(bit);
        }
    }
    else if (opcode == 0x50)
    {
        quint8 address = 0;
        if (!fetchByte(address))
            result.error = QStringLiteral("Malformed STORE instruction.");
        else
        {
            m_ram.write(address, m_registers.accumulator());
            result.trace = QStringLiteral("STORE [0x%1], A").arg(address, 2, 16, QLatin1Char('0'));
        }
    }
    else if (opcode == 0x51)
    {
        quint8 address = 0;
        if (!fetchByte(address))
            result.error = QStringLiteral("Malformed LOAD instruction.");
        else
        {
            m_registers.setAccumulator(m_ram.read(address));
            result.trace = QStringLiteral("LOAD A, [0x%1]").arg(address, 2, 16, QLatin1Char('0'));
        }
    }
    else if (opcode == 0xFF)
    {
        m_halted = true;
        result.halted = true;
        result.trace = QStringLiteral("HALT");
    }
    else
    {
        result.error = QStringLiteral("Unknown opcode 0x%1 at 0x%2.")
                           .arg(opcode, 2, 16, QLatin1Char('0'))
                           .arg(instructionAddress, 4, 16, QLatin1Char('0'));
    }

    if (!result.error.isEmpty())
    {
        m_halted = true;
        result.halted = true;
    }
    m_lastTrace = result.trace.isEmpty() ? result.error : result.trace;
    return result;
}

void EducationalMcuCore::setPortInput(int portIndex, quint8 value)
{
    if (portIndex >= 0 && portIndex < m_ports.size())
        m_ports[portIndex].setInputSample(value);
}

const McuPort &EducationalMcuCore::port(int portIndex) const
{
    static const McuPort emptyPort;
    if (portIndex < 0 || portIndex >= m_ports.size())
        return emptyPort;
    return m_ports.at(portIndex);
}

McuPort &EducationalMcuCore::port(int portIndex)
{
    return m_ports[std::clamp(portIndex, 0, static_cast<int>(m_ports.size()) - 1)];
}

quint16 EducationalMcuCore::programCounter() const
{
    return m_programCounter.value();
}

quint8 EducationalMcuCore::accumulator() const
{
    return m_registers.accumulator();
}

quint8 EducationalMcuCore::generalRegister() const
{
    return m_registers.general();
}

bool EducationalMcuCore::halted() const
{
    return m_halted;
}

int EducationalMcuCore::flashSize() const
{
    return m_flash.size();
}

quint64 EducationalMcuCore::instructionCount() const
{
    return m_instructionCount;
}

QString EducationalMcuCore::lastTrace() const
{
    return m_lastTrace;
}

QVariantMap EducationalMcuCore::saveState() const
{
    QVariantList ports;
    for (const McuPort &currentPort : m_ports)
    {
        ports.append(QVariantMap{{QStringLiteral("direction"), currentPort.directionMask()},
                                 {QStringLiteral("output"), currentPort.outputLatch()},
                                 {QStringLiteral("input"), currentPort.inputSample()}});
    }

    return {{QStringLiteral("pc"), m_programCounter.value()},
            {QStringLiteral("accumulator"), m_registers.accumulator()},
            {QStringLiteral("general"), m_registers.general()},
            {QStringLiteral("ram"), m_ram.bytes()},
            {QStringLiteral("ports"), ports},
            {QStringLiteral("halted"), m_halted},
            {QStringLiteral("instructionCount"), QVariant::fromValue<qulonglong>(m_instructionCount)},
            {QStringLiteral("lastTrace"), m_lastTrace}};
}

void EducationalMcuCore::loadState(const QVariantMap &state)
{
    m_programCounter.setValue(static_cast<quint16>(state.value(QStringLiteral("pc"), 0).toUInt()));
    m_registers.setAccumulator(static_cast<quint8>(state.value(QStringLiteral("accumulator"), 0).toUInt()));
    m_registers.setGeneral(static_cast<quint8>(state.value(QStringLiteral("general"), 0).toUInt()));
    m_ram.restore(state.value(QStringLiteral("ram")).toByteArray());

    const QVariantList ports = state.value(QStringLiteral("ports")).toList();
    for (int index = 0; index < std::min(static_cast<int>(ports.size()),
                                                 static_cast<int>(m_ports.size()));
         ++index)
    {
        const QVariantMap savedPort = ports.at(index).toMap();
        m_ports[index].setDirectionMask(
            static_cast<quint8>(savedPort.value(QStringLiteral("direction"), 0).toUInt()));
        m_ports[index].write(static_cast<quint8>(savedPort.value(QStringLiteral("output"), 0).toUInt()));
        m_ports[index].setInputSample(
            static_cast<quint8>(savedPort.value(QStringLiteral("input"), 0).toUInt()));
    }

    m_halted = state.value(QStringLiteral("halted"), m_flash.isEmpty()).toBool();
    m_instructionCount = state.value(QStringLiteral("instructionCount"), 0).toULongLong();
    m_lastTrace = state.value(QStringLiteral("lastTrace")).toString();
}

bool EducationalMcuCore::fetchByte(quint8 &value)
{
    const quint16 address = m_programCounter.value();
    if (static_cast<int>(address) >= m_flash.size())
        return false;
    value = static_cast<quint8>(m_flash.at(address));
    m_programCounter.advance();
    return true;
}

bool EducationalMcuCore::fetchWord(quint16 &value)
{
    quint8 low = 0;
    quint8 high = 0;
    if (!fetchByte(low) || !fetchByte(high))
        return false;
    value = static_cast<quint16>(low | (static_cast<quint16>(high) << 8));
    return true;
}

bool EducationalMcuCore::validTarget(quint8 target) const
{
    return target <= DirectionC;
}

quint8 EducationalMcuCore::readTarget(quint8 target) const
{
    switch (target)
    {
    case Accumulator:
        return m_registers.accumulator();
    case General:
        return m_registers.general();
    case PortA:
    case PortB:
    case PortC:
        return m_ports.at(target - PortA).read();
    case DirectionA:
    case DirectionB:
    case DirectionC:
        return m_ports.at(target - DirectionA).directionMask();
    default:
        return 0;
    }
}

void EducationalMcuCore::writeTarget(quint8 target, quint8 value)
{
    switch (target)
    {
    case Accumulator:
        m_registers.setAccumulator(value);
        break;
    case General:
        m_registers.setGeneral(value);
        break;
    case PortA:
    case PortB:
    case PortC:
        m_ports[target - PortA].write(value);
        break;
    case DirectionA:
    case DirectionB:
    case DirectionC:
        m_ports[target - DirectionA].setDirectionMask(value);
        break;
    default:
        break;
    }
}

QString EducationalMcuCore::targetName(quint8 target) const
{
    static const QVector<QString> names{QStringLiteral("A"),
                                        QStringLiteral("B"),
                                        QStringLiteral("PORTA"),
                                        QStringLiteral("PORTB"),
                                        QStringLiteral("PORTC"),
                                        QStringLiteral("DDRA"),
                                        QStringLiteral("DDRB"),
                                        QStringLiteral("DDRC")};
    return target < names.size() ? names.at(target) : QStringLiteral("?");
}
