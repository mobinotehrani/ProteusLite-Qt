#pragma once

#include "componentdefinition.h"

#include <QWidget>

class QPaintEvent;
class QPainter;

class SchematicPreviewWidget final : public QWidget
{
public:
    explicit SchematicPreviewWidget(QWidget *parent = nullptr);

    void setComponent(const ComponentDefinition *definition);
    const ComponentDefinition *component() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawSymbol(QPainter &painter, const QString &id) const;
    void drawResistor(QPainter &painter) const;
    void drawCapacitor(QPainter &painter) const;
    void drawInductor(QPainter &painter) const;
    void drawGround(QPainter &painter) const;
    void drawSource(QPainter &painter, const QString &id) const;
    void drawSwitch(QPainter &painter, bool pushButton) const;
    void drawLed(QPainter &painter, const QString &id) const;
    void drawSevenSegment(QPainter &painter) const;
    void drawLogicGate(QPainter &painter, const QString &id) const;
    void drawBlock(QPainter &painter, const QString &title, int inputPins, int outputPins) const;
    void drawInstrument(QPainter &painter, const QString &id) const;

    const ComponentDefinition *m_definition{};
};
