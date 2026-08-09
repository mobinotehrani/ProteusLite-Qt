#include "oscilloscopewidget.h"

#include <QDoubleSpinBox>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace
{
QRectF plotArea(const QRectF &rect)
{
    return rect.adjusted(52.0, 18.0, -18.0, -34.0);
}

void drawTrace(QPainter &painter,
               const QRectF &area,
               const QVector<OscilloscopePlot::Sample> &samples,
               int channel,
               double startTime,
               double endTime,
               double voltsPerDivision,
               const QColor &color)
{
    if (samples.isEmpty() || endTime <= startTime || voltsPerDivision <= 0.0)
        return;

    const double divisionHeight = area.height() / 8.0;
    const double zeroY = area.center().y();
    QPainterPath path;
    bool hasPoint = false;

    for (const auto &sample : samples)
    {
        if (sample.timeSeconds < startTime || sample.timeSeconds > endTime)
            continue;

        const std::optional<double> value = channel == 1 ? sample.channel1 : sample.channel2;
        if (!value.has_value() || !std::isfinite(*value))
        {
            hasPoint = false;
            continue;
        }

        const double normalizedTime = (sample.timeSeconds - startTime) / (endTime - startTime);
        const double x = area.left() + normalizedTime * area.width();
        const double y = zeroY - (*value / voltsPerDivision) * divisionHeight;
        const QPointF point(x, std::clamp(y, area.top() - 2.0, area.bottom() + 2.0));
        if (!hasPoint)
        {
            path.moveTo(point);
            hasPoint = true;
        }
        else
        {
            path.lineTo(point);
        }
    }

    painter.save();
    painter.setClipRect(area);
    painter.setPen(QPen(color, 1.8));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.restore();
}
}

OscilloscopePlot::OscilloscopePlot(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(620, 300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(false);
}

void OscilloscopePlot::setSamples(const QVector<Sample> &samples)
{
    m_samples = samples;
    update();
}

void OscilloscopePlot::setTimePerDivision(double seconds)
{
    m_timePerDivision = std::clamp(seconds, 0.001, 10.0);
    update();
}

void OscilloscopePlot::setChannelScale(int channel, double voltsPerDivision)
{
    const double safeValue = std::clamp(voltsPerDivision, 0.01, 100.0);
    if (channel == 1)
        m_channel1Scale = safeValue;
    else if (channel == 2)
        m_channel2Scale = safeValue;
    update();
}

void OscilloscopePlot::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(7, 13, 24));

    const QRectF area = plotArea(rect());
    painter.setPen(QPen(QColor(51, 65, 85), 1.0));
    painter.setBrush(QColor(9, 18, 32));
    painter.drawRoundedRect(area, 6.0, 6.0);

    painter.setPen(QPen(QColor(30, 41, 59), 1.0));
    for (int column = 0; column <= 10; ++column)
    {
        const double x = area.left() + area.width() * static_cast<double>(column) / 10.0;
        painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
    }
    for (int row = 0; row <= 8; ++row)
    {
        const double y = area.top() + area.height() * static_cast<double>(row) / 8.0;
        painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }

    painter.setPen(QPen(QColor(71, 85, 105), 1.2));
    painter.drawLine(QPointF(area.left(), area.center().y()),
                     QPointF(area.right(), area.center().y()));

    const double latestTime = m_samples.isEmpty() ? 0.0 : m_samples.constLast().timeSeconds;
    const double visibleSeconds = m_timePerDivision * 10.0;
    const double windowStart = latestTime > visibleSeconds ? latestTime - visibleSeconds : 0.0;
    const double windowEnd = latestTime > visibleSeconds ? latestTime : visibleSeconds;

    drawTrace(painter,
              area,
              m_samples,
              1,
              windowStart,
              std::max(windowStart + 0.000001, windowEnd),
              m_channel1Scale,
              QColor(34, 197, 94));
    drawTrace(painter,
              area,
              m_samples,
              2,
              windowStart,
              std::max(windowStart + 0.000001, windowEnd),
              m_channel2Scale,
              QColor(56, 189, 248));

    painter.setPen(QColor(148, 163, 184));
    painter.drawText(QRectF(area.left(), area.bottom() + 8.0, area.width(), 22.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("%1 s").arg(windowStart, 0, 'f', 3));
    painter.drawText(QRectF(area.left(), area.bottom() + 8.0, area.width(), 22.0),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QStringLiteral("%1 s").arg(windowEnd, 0, 'f', 3));

    if (m_samples.isEmpty())
    {
        painter.setPen(QColor(148, 163, 184));
        painter.drawText(area, Qt::AlignCenter, tr("Waiting for simulation data"));
    }
}

OscilloscopeWidget::OscilloscopeWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("OscilloscopePanel"));
    setStyleSheet(QStringLiteral(
        "QWidget#OscilloscopePanel{background:#0b1220;}"
        "QLabel{color:#cbd5e1;}"
        "QLabel#ScopeTitle{color:#f8fafc;font-weight:700;font-size:11pt;}"
        "QLabel#ChannelOne{color:#22c55e;font-weight:700;}"
        "QLabel#ChannelTwo{color:#38bdf8;font-weight:700;}"
        "QDoubleSpinBox{background:#111827;color:#e2e8f0;border:1px solid #334155;"
        "border-radius:6px;padding:4px 7px;min-width:86px;}"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(8);

    auto *header = new QHBoxLayout;
    auto *title = new QLabel(tr("Two-channel oscilloscope"), this);
    title->setObjectName(QStringLiteral("ScopeTitle"));
    m_sourceLabel = new QLabel(tr("No oscilloscope component selected"), this);
    m_stateLabel = new QLabel(tr("STOPPED"), this);
    header->addWidget(title);
    header->addSpacing(12);
    header->addWidget(m_sourceLabel, 1);
    header->addWidget(m_stateLabel);
    layout->addLayout(header);

    auto *controls = new QGridLayout;
    controls->setHorizontalSpacing(8);
    controls->setVerticalSpacing(5);

    m_timeScale = new QDoubleSpinBox(this);
    m_timeScale->setDecimals(1);
    m_timeScale->setRange(1.0, 10000.0);
    m_timeScale->setValue(100.0);
    m_timeScale->setSuffix(tr(" ms/div"));

    m_channel1Scale = new QDoubleSpinBox(this);
    m_channel1Scale->setDecimals(2);
    m_channel1Scale->setRange(0.01, 100.0);
    m_channel1Scale->setValue(1.0);
    m_channel1Scale->setSuffix(tr(" V/div"));

    m_channel2Scale = new QDoubleSpinBox(this);
    m_channel2Scale->setDecimals(2);
    m_channel2Scale->setRange(0.01, 100.0);
    m_channel2Scale->setValue(1.0);
    m_channel2Scale->setSuffix(tr(" V/div"));

    m_channel1Label = new QLabel(tr("CH1: --"), this);
    m_channel1Label->setObjectName(QStringLiteral("ChannelOne"));
    m_channel2Label = new QLabel(tr("CH2: --"), this);
    m_channel2Label->setObjectName(QStringLiteral("ChannelTwo"));

    controls->addWidget(new QLabel(tr("Time scale"), this), 0, 0);
    controls->addWidget(m_timeScale, 0, 1);
    controls->addWidget(new QLabel(tr("CH1 scale"), this), 0, 2);
    controls->addWidget(m_channel1Scale, 0, 3);
    controls->addWidget(new QLabel(tr("CH2 scale"), this), 0, 4);
    controls->addWidget(m_channel2Scale, 0, 5);
    controls->addWidget(m_channel1Label, 0, 6);
    controls->addWidget(m_channel2Label, 0, 7);
    controls->setColumnStretch(8, 1);
    layout->addLayout(controls);

    m_plot = new OscilloscopePlot(this);
    layout->addWidget(m_plot, 1);

    connect(m_timeScale,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            [this](double value)
            {
                m_plot->setTimePerDivision(value / 1000.0);
                trimSamples();
            });
    connect(m_channel1Scale,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            [this](double value) { m_plot->setChannelScale(1, value); });
    connect(m_channel2Scale,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            [this](double value) { m_plot->setChannelScale(2, value); });
}

void OscilloscopeWidget::appendSample(double timeSeconds,
                                      std::optional<double> channel1,
                                      std::optional<double> channel2)
{
    m_samples.append({timeSeconds, channel1, channel2});
    trimSamples();
    m_plot->setSamples(m_samples);
    refreshReadout(channel1, channel2);
}

void OscilloscopeWidget::clearSamples()
{
    m_samples.clear();
    m_plot->setSamples(m_samples);
    refreshReadout(std::nullopt, std::nullopt);
}

void OscilloscopeWidget::setSourceText(const QString &text)
{
    m_sourceLabel->setText(text);
}

void OscilloscopeWidget::setStateText(const QString &text)
{
    m_stateLabel->setText(text);
}

void OscilloscopeWidget::trimSamples()
{
    if (m_samples.isEmpty())
        return;

    const double visibleSeconds = (m_timeScale ? m_timeScale->value() / 1000.0 : 0.1) * 10.0;
    const double keepAfter = std::max(0.0, m_samples.constLast().timeSeconds - visibleSeconds * 1.5);
    int removeCount = 0;
    while (removeCount < m_samples.size() && m_samples.at(removeCount).timeSeconds < keepAfter)
        ++removeCount;
    if (removeCount > 0)
        m_samples.remove(0, removeCount);

    const int hardLimit = 6000;
    if (m_samples.size() > hardLimit)
        m_samples.remove(0, m_samples.size() - hardLimit);
}

void OscilloscopeWidget::refreshReadout(std::optional<double> channel1,
                                        std::optional<double> channel2)
{
    m_channel1Label->setText(tr("CH1: %1").arg(voltageText(channel1)));
    m_channel2Label->setText(tr("CH2: %1").arg(voltageText(channel2)));
}

QString OscilloscopeWidget::voltageText(const std::optional<double> &value)
{
    if (!value.has_value() || !std::isfinite(*value))
        return QStringLiteral("--");
    return QStringLiteral("%1 V").arg(*value, 0, 'f', 3);
}
