#pragma once

#include "basiccomponent.h"

#include <memory>

class AdcComponent final : public Component
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
    QVariantMap saveState() const override;
    void loadState(const QVariantMap &state) override;

  private:
    int maximumCode() const;
    int codeForVoltage(double inputVoltage, double referenceLow, double referenceHigh) const;
    void scheduleCode(int code, double timeSeconds);

    int m_resolutionBits{4};
    double m_conversionDelayMilliseconds{1.0};
    double m_logicHighVoltage{5.0};
    int m_outputCode{0};
    int m_pendingCode{0};
    double m_applyAtSeconds{0.0};
    bool m_hasValidOutput{false};
    bool m_conversionPending{false};
    std::optional<double> m_lastInputVoltage;
};

class DacComponent final : public Component
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

  private:
    int maximumCode() const;
    std::optional<int> inputCode(const QVector<std::optional<double>> &pinVoltages,
                                 QStringList &warnings) const;
    void scheduleVoltage(int code, double voltage, double timeSeconds);

    int m_resolutionBits{4};
    double m_conversionDelayMilliseconds{1.0};
    int m_inputCode{0};
    int m_pendingCode{0};
    double m_outputVoltage{0.0};
    double m_pendingVoltage{0.0};
    double m_applyAtSeconds{0.0};
    bool m_hasValidOutput{false};
    bool m_conversionPending{false};
};

std::unique_ptr<Component> createConverterComponent(const QString &type);
