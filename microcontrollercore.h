#pragma once

#include <QByteArray>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <QVector>

class ProgramCounter final
{
  public:
    quint16 value() const;
    void setValue(quint16 value);
    void advance(quint16 amount = 1);
    void reset();

  private:
    quint16 m_value{0};
};

class RegisterBank final
{
  public:
    quint8 accumulator() const;
    quint8 general() const;
    void setAccumulator(quint8 value);
    void setGeneral(quint8 value);
    void reset();

  private:
    quint8 m_accumulator{0};
    quint8 m_general{0};
};

class InternalRam final
{
  public:
    explicit InternalRam(int size = 256);

    int size() const;
    quint8 read(quint16 address) const;
    void write(quint16 address, quint8 value);
    void clear();
    QByteArray bytes() const;
    void restore(const QByteArray &bytes);

  private:
    QByteArray m_bytes;
};

class McuPort final
{
  public:
    quint8 directionMask() const;
    quint8 outputLatch() const;
    quint8 inputSample() const;
    quint8 read() const;

    void setDirectionMask(quint8 mask);
    void write(quint8 value);
    void setInputSample(quint8 value);
    void setBit(int bit);
    void clearBit(int bit);
    bool drivesBit(int bit) const;
    bool outputBit(int bit) const;
    void reset();

  private:
    quint8 m_directionMask{0};
    quint8 m_outputLatch{0};
    quint8 m_inputSample{0};
};

struct McuInstructionResult
{
    bool executed{false};
    bool halted{false};
    QString trace;
    QString error;
};

class EducationalMcuCore final
{
  public:
    enum Target : quint8
    {
        Accumulator = 0,
        General = 1,
        PortA = 2,
        PortB = 3,
        PortC = 4,
        DirectionA = 5,
        DirectionB = 6,
        DirectionC = 7
    };

    void loadFlash(const QByteArray &bytes);
    void reset();
    McuInstructionResult executeNext();

    void setPortInput(int portIndex, quint8 value);
    const McuPort &port(int portIndex) const;
    McuPort &port(int portIndex);

    quint16 programCounter() const;
    quint8 accumulator() const;
    quint8 generalRegister() const;
    bool halted() const;
    int flashSize() const;
    quint64 instructionCount() const;
    QString lastTrace() const;

    QVariantMap saveState() const;
    void loadState(const QVariantMap &state);

  private:
    bool fetchByte(quint8 &value);
    bool fetchWord(quint16 &value);
    bool validTarget(quint8 target) const;
    quint8 readTarget(quint8 target) const;
    void writeTarget(quint8 target, quint8 value);
    QString targetName(quint8 target) const;

    QByteArray m_flash;
    ProgramCounter m_programCounter;
    RegisterBank m_registers;
    InternalRam m_ram;
    QVector<McuPort> m_ports = QVector<McuPort>(3);
    bool m_halted{true};
    quint64 m_instructionCount{0};
    QString m_lastTrace;
};
