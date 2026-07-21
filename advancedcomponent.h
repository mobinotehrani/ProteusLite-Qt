#pragma once

#include "basiccomponent.h"
#include "microcontrollercore.h"

#include <QByteArray>

#include <memory>

class MicrocontrollerComponent final : public Component
{
  public:
    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    QString runtimeText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages,
                             double timeSeconds) override;
    bool active() const override;
    QVariantMap saveState() const override;
    void loadState(const QVariantMap &state) override;

    QString firmwareStatus() const;
    QString firmwarePath() const;

  private:
    bool loadFirmware(const QString &filePath);
    quint8 samplePort(const QVector<std::optional<double>> &pinVoltages,
                      int portIndex,
                      QStringList &warnings) const;
    void drivePort(ComponentStepResult &result, int portIndex) const;

    QString m_firmwarePath;
    QString m_firmwareStatus{QStringLiteral("No firmware loaded")};
    double m_clockFrequency{20.0};
    bool m_autoRun{true};
    bool m_requirePower{true};
    double m_lastEvaluationTime{-1.0};
    double m_cycleRemainder{0.0};
    EducationalMcuCore m_core;
};

class ExternalMemoryComponent final : public Component
{
  public:
    ExternalMemoryComponent();

    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    QString runtimeText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages,
                             double timeSeconds) override;
    QVariantMap saveState() const override;
    void loadState(const QVariantMap &state) override;

  private:
    std::optional<int> readAddress(const QVector<std::optional<double>> &pinVoltages,
                                   QStringList &warnings) const;
    std::optional<quint8> readData(const QVector<std::optional<double>> &pinVoltages,
                                   QStringList &warnings) const;
    void resizeMemory(int size, quint8 fillValue);

    QByteArray m_memory;
    int m_fillValue{255};
    bool m_writeLatched{false};
    QString m_lastOperation{QStringLiteral("idle")};
};

class Lcd16x2Component final : public Component
{
  public:
    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    QString runtimeText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages,
                             double timeSeconds) override;
    bool active() const override;
    QStringList displayLines() const override;
    QVariantMap saveState() const override;
    void loadState(const QVariantMap &state) override;

  private:
    std::optional<quint8> readDataBus(const QVector<std::optional<double>> &pinVoltages,
                                      QStringList &warnings) const;
    void executeCommand(quint8 command);
    void writeCharacter(quint8 value);
    void setInitialText(const QString &text);
    void normalizeLines();

    QStringList m_lines{QString(16, QLatin1Char(' ')), QString(16, QLatin1Char(' '))};
    QString m_initialText{QStringLiteral("Hello")};
    int m_cursorAddress{0};
    bool m_displayOn{true};
    bool m_incrementCursor{true};
    LogicLevel m_previousEnable{LogicLevel::Undefined};
    QString m_lastOperation{QStringLiteral("idle")};
};

class MatrixKeypadComponent final : public Component
{
  public:
    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    QString runtimeText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages,
                             double timeSeconds) override;
    QVector<QPair<int, int>> conductivePinPairs() const override;
    void pressKey(int row, int column) override;
    void releaseKey() override;
    bool active() const override;
    QVector<bool> keyStates() const override;
    QVariantMap saveState() const override;
    void loadState(const QVariantMap &state) override;

    QString keyLabel(int row, int column) const;

  private:
    int m_pressedRow{-1};
    int m_pressedColumn{-1};
};

std::unique_ptr<Component> createAdvancedComponent(const QString &type);
