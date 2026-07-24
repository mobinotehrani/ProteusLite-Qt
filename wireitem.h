#pragma once

#include <QGraphicsPathItem>
#include <QString>
#include <QVector>
#include <optional>

class WireItem final : public QGraphicsPathItem
{
  public:
    enum
    {
        Type = QGraphicsItem::UserType + 502
    };

    enum class SignalState
    {
        Idle,
        Low,
        High,
        Analog,
        Undefined,
        Conflict
    };

    WireItem(const QString &modelId,
             const QString &startEndpoint,
             const QString &endEndpoint,
             QGraphicsItem *parent = nullptr);

    int type() const override;
    QString modelId() const;
    QString startEndpoint() const;
    QString endEndpoint() const;
    QVector<QPointF> points() const;
    SignalState signalState() const;
    std::optional<double> signalVoltage() const;

    void setPoints(const QVector<QPointF> &points);
    void setSignalState(SignalState state,
                        std::optional<double> voltage = std::nullopt);
    QPainterPath shape() const override;

  private:
    void updateAppearance();

    QString m_modelId;
    QString m_startEndpoint;
    QString m_endEndpoint;
    QVector<QPointF> m_points;
    SignalState m_signalState{SignalState::Idle};
    std::optional<double> m_signalVoltage;
};
