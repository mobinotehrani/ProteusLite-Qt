#pragma once

#include <QString>
#include <QVector>
#include <QWidget>
#include <optional>

class QLabel;
class QDoubleSpinBox;

class OscilloscopePlot final : public QWidget
{
  public:
    struct Sample
    {
        double timeSeconds{0.0};
        std::optional<double> channel1;
        std::optional<double> channel2;
    };

    explicit OscilloscopePlot(QWidget *parent = nullptr);

    void setSamples(const QVector<Sample> &samples);
    void setTimePerDivision(double seconds);
    void setChannelScale(int channel, double voltsPerDivision);

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QVector<Sample> m_samples;
    double m_timePerDivision{0.1};
    double m_channel1Scale{1.0};
    double m_channel2Scale{1.0};
};

class OscilloscopeWidget final : public QWidget
{
  public:
    explicit OscilloscopeWidget(QWidget *parent = nullptr);

    void appendSample(double timeSeconds,
                      std::optional<double> channel1,
                      std::optional<double> channel2);
    void clearSamples();
    void setSourceText(const QString &text);
    void setStateText(const QString &text);

  private:
    void trimSamples();
    void refreshReadout(std::optional<double> channel1,
                        std::optional<double> channel2);
    static QString voltageText(const std::optional<double> &value);

    OscilloscopePlot *m_plot{};
    QLabel *m_sourceLabel{};
    QLabel *m_stateLabel{};
    QLabel *m_channel1Label{};
    QLabel *m_channel2Label{};
    QDoubleSpinBox *m_timeScale{};
    QDoubleSpinBox *m_channel1Scale{};
    QDoubleSpinBox *m_channel2Scale{};
    QVector<OscilloscopePlot::Sample> m_samples;
};
