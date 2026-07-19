#pragma once

#include "circuitmodel.h"

#include <QColor>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

#include <memory>
#include <optional>

enum class ComponentPropertyType
{
    Number,
    Integer,
    Boolean,
    Text,
    Choice
};

struct ComponentProperty
{
    QString key;
    QString label;
    ComponentPropertyType type{ComponentPropertyType::Text};
    QVariant value;
    double minimum{0.0};
    double maximum{0.0};
    double step{1.0};
    QString suffix;
    QStringList choices;
};

enum class LogicLevel
{
    Low,
    High,
    Undefined
};

class LogicThresholds final
{
  public:
    static double lowVoltage();
    static double highVoltage();
    static double lowMaximum();
    static double highMinimum();
    static LogicLevel fromVoltage(const std::optional<double> &voltage);
    static std::optional<double> toVoltage(LogicLevel level);
    static QString text(LogicLevel level);
};

struct ComponentPinDefinition
{
    QString name;
    PinDirection direction{PinDirection::Passive};
};

struct ComponentStepResult
{
    QVector<std::optional<double>> pinVoltages;
    QVector<bool> drivenPins;
    QStringList warnings;
};

struct ComponentElectricalInput
{
    std::optional<double> voltageAcross;
    std::optional<double> branchCurrent;
    double deltaSeconds{0.0};
};

struct ComponentElectricalResult
{
    std::optional<double> voltageAcross;
    std::optional<double> branchCurrent;
    QStringList warnings;
};

class Component
{
  public:
    virtual ~Component() = default;

    virtual QString typeId() const = 0;
    virtual QVector<ComponentPinDefinition> pinDefinitions() const = 0;
    virtual QVector<ComponentProperty> editableProperties() const = 0;
    virtual bool setProperty(const QString &key, const QVariant &value) = 0;
    virtual QVariant property(const QString &key) const = 0;
    virtual QString valueText() const = 0;
    virtual QString runtimeText() const;
    virtual ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages,
                                     double timeSeconds) = 0;
    virtual ComponentElectricalResult evaluateElectrical(const ComponentElectricalInput &input);
    virtual QVector<QPair<int, int>> conductivePinPairs() const;

    virtual void activate();
    virtual void pointerPressed();
    virtual void pointerReleased();
    virtual bool active() const;
    virtual QColor indicatorColor() const;
    virtual QVector<bool> segmentStates() const;
    virtual QVariantMap saveState() const;
    virtual void loadState(const QVariantMap &state);

  protected:
    static ComponentProperty numberProperty(const QString &key,
                                            const QString &label,
                                            double value,
                                            double minimum,
                                            double maximum,
                                            double step,
                                            const QString &suffix = {});
    static ComponentProperty integerProperty(const QString &key,
                                             const QString &label,
                                             int value,
                                             int minimum,
                                             int maximum,
                                             int step = 1,
                                             const QString &suffix = {});
    static ComponentProperty booleanProperty(const QString &key, const QString &label, bool value);
    static ComponentProperty textProperty(const QString &key, const QString &label, const QString &value);
    static ComponentProperty choiceProperty(const QString &key,
                                            const QString &label,
                                            const QString &value,
                                            const QStringList &choices);
    static ComponentStepResult emptyResult(int pinCount);
};

class GroundComponent final : public Component
{
  public:
    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages, double timeSeconds) override;
    ComponentElectricalResult evaluateElectrical(const ComponentElectricalInput &input) override;
};

class VoltageSourceComponent final : public Component
{
  public:
    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages, double timeSeconds) override;
    ComponentElectricalResult evaluateElectrical(const ComponentElectricalInput &input) override;

  private:
    double m_voltage{5.0};
};

class BatteryComponent final : public Component
{
  public:
    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages, double timeSeconds) override;
    ComponentElectricalResult evaluateElectrical(const ComponentElectricalInput &input) override;

  private:
    double m_voltage{5.0};
    double m_internalResistance{0.5};
};

class ClockComponent final : public Component
{
  public:
    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    QString runtimeText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages, double timeSeconds) override;
    bool active() const override;

  private:
    double m_frequency{1000.0};
    double m_dutyCycle{50.0};
    double m_highVoltage{5.0};
    double m_phaseDegrees{0.0};
    bool m_lastHigh{false};
};

class ResistorComponent final : public Component
{
  public:
    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    QString runtimeText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages, double timeSeconds) override;
    ComponentElectricalResult evaluateElectrical(const ComponentElectricalInput &input) override;

  private:
    double m_resistance{1000.0};
    std::optional<double> m_lastVoltage;
    std::optional<double> m_lastCurrent;
};

class CapacitorComponent final : public Component
{
  public:
    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    QString runtimeText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages, double timeSeconds) override;
    ComponentElectricalResult evaluateElectrical(const ComponentElectricalInput &input) override;
    QVariantMap saveState() const override;
    void loadState(const QVariantMap &state) override;

  private:
    double m_capacitance{0.000001};
    double m_initialVoltage{0.0};
    double m_storedVoltage{0.0};
    std::optional<double> m_lastCurrent;
};

class InductorComponent final : public Component
{
  public:
    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    QString runtimeText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages, double timeSeconds) override;
    ComponentElectricalResult evaluateElectrical(const ComponentElectricalInput &input) override;
    QVariantMap saveState() const override;
    void loadState(const QVariantMap &state) override;

  private:
    double m_inductance{0.001};
    double m_initialCurrent{0.0};
    double m_current{0.0};
    std::optional<double> m_lastVoltage;
};

class SwitchComponent final : public Component
{
  public:
    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages, double timeSeconds) override;
    ComponentElectricalResult evaluateElectrical(const ComponentElectricalInput &input) override;
    QVector<QPair<int, int>> conductivePinPairs() const override;
    void activate() override;
    bool active() const override;
    QVariantMap saveState() const override;
    void loadState(const QVariantMap &state) override;

  private:
    bool m_closed{false};
};

class PushButtonComponent final : public Component
{
  public:
    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages, double timeSeconds) override;
    ComponentElectricalResult evaluateElectrical(const ComponentElectricalInput &input) override;
    void pointerPressed() override;
    void pointerReleased() override;
    bool active() const override;

  private:
    double m_highVoltage{5.0};
    bool m_pressed{false};
};

class LedComponent final : public Component
{
  public:
    explicit LedComponent(const QString &type);

    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    QString runtimeText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages, double timeSeconds) override;
    ComponentElectricalResult evaluateElectrical(const ComponentElectricalInput &input) override;
    bool active() const override;
    QColor indicatorColor() const override;

  private:
    QString m_type;
    double m_forwardVoltage{2.0};
    double m_maxCurrentMilliamp{20.0};
    double m_currentAmp{0.0};
    bool m_lit{false};
};

class SevenSegmentComponent final : public Component
{
  public:
    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    QString runtimeText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages, double timeSeconds) override;
    bool active() const override;
    QColor indicatorColor() const override;
    QVector<bool> segmentStates() const override;

  private:
    QString m_commonType{QStringLiteral("Common cathode")};
    QColor m_color{220, 38, 38};
    bool m_hasDecimalPoint{true};
    QVector<bool> m_segments{false, false, false, false, false, false, false, false};
    bool m_hasUndefinedInput{false};
};

class LogicGateComponent final : public Component
{
  public:
    explicit LogicGateComponent(const QString &type);

    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    QString runtimeText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages, double timeSeconds) override;
    bool active() const override;
    QVariantMap saveState() const override;
    void loadState(const QVariantMap &state) override;

  private:
    LogicLevel evaluate(const QVector<LogicLevel> &inputs) const;
    void schedule(LogicLevel target, double timeSeconds);

    QString m_type;
    int m_inputCount{2};
    double m_propagationDelayNanoseconds{10.0};
    LogicLevel m_output{LogicLevel::Low};
    LogicLevel m_pendingOutput{LogicLevel::Low};
    double m_applyAtSeconds{0.0};
    bool m_hasPendingOutput{false};
};

class DFlipFlopComponent final : public Component
{
  public:
    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    QString runtimeText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages, double timeSeconds) override;
    bool active() const override;
    QVariantMap saveState() const override;
    void loadState(const QVariantMap &state) override;

  private:
    void schedule(LogicLevel target, double timeSeconds);

    double m_propagationDelayNanoseconds{10.0};
    LogicLevel m_q{LogicLevel::Low};
    LogicLevel m_previousClock{LogicLevel::Undefined};
    LogicLevel m_pendingQ{LogicLevel::Low};
    double m_applyAtSeconds{0.0};
    bool m_hasPendingQ{false};
};

class GenericComponent final : public Component
{
  public:
    explicit GenericComponent(const QString &type);

    QString typeId() const override;
    QVector<ComponentPinDefinition> pinDefinitions() const override;
    QVector<ComponentProperty> editableProperties() const override;
    bool setProperty(const QString &key, const QVariant &value) override;
    QVariant property(const QString &key) const override;
    QString valueText() const override;
    ComponentStepResult step(const QVector<std::optional<double>> &pinVoltages, double timeSeconds) override;

  private:
    QString m_type;
    QString m_value;
};

class ComponentFactory final
{
  public:
    static std::unique_ptr<Component> create(const QString &type);
};
