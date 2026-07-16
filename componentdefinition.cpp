#include "componentdefinition.h"

namespace
{
ComponentDefinition component(const char *id,
                              const char *name,
                              const char *category,
                              const char *description,
                              const char *defaultValue)
{
    return {QString::fromLatin1(id),
            QString::fromLatin1(name),
            QString::fromLatin1(category),
            QString::fromLatin1(description),
            QString::fromLatin1(defaultValue)};
}
}

const QList<ComponentDefinition> &ComponentCatalog::all()
{
    static const QList<ComponentDefinition> definitions{
        component("Ground", "GND", "Power / Sources",
                  "0 V reference node used by the circuit and simulation engine.", "0"),
        component("Voltage", "DC Voltage Source", "Power / Sources",
                  "Ideal direct-current voltage source with configurable voltage.", "5 V"),
        component("Battery", "Battery", "Power / Sources",
                  "Battery-style DC source prepared for an internal-resistance property.", "5 V"),
        component("Clock", "Clock Generator", "Power / Sources",
                  "Digital square-wave source with a configurable frequency.", "1 kHz"),
        component("Current", "Current Source", "Power / Sources",
                  "Ideal current source for later nodal-analysis support.", "1 mA"),

        component("Resistor", "Resistor", "Passive / Analog",
                  "Ohmic two-terminal passive component.", "1 kOhm"),
        component("Capacitor", "Capacitor", "Passive / Analog",
                  "Two-terminal energy-storage component.", "1 uF"),
        component("Inductor", "Inductor", "Passive / Analog",
                  "Two-terminal magnetic energy-storage component.", "1 mH"),
        component("Potentiometer", "Potentiometer", "Passive / Analog",
                  "Adjustable resistance prepared for live interaction.", "10 kOhm"),

        component("Switch", "Switch", "Interactive / Output",
                  "Persistent open/closed interactive switch.", "OFF"),
        component("PushButton", "Push Button", "Interactive / Output",
                  "Momentary digital input button.", "OFF"),
        component("LEDRed", "LED Red", "Interactive / Output",
                  "Red light-emitting diode indicator.", "RED"),
        component("LEDGreen", "LED Green", "Interactive / Output",
                  "Green light-emitting diode indicator.", "GREEN"),
        component("LEDBlue", "LED Blue", "Interactive / Output",
                  "Blue light-emitting diode indicator.", "BLUE"),
        component("SevenSegment", "7-Segment Display", "Interactive / Output",
                  "Seven-segment numeric display with separate segment inputs.", "0"),

        component("AND", "AND Gate", "Digital Logic",
                  "Multi-input digital AND gate.", "delay=10ns"),
        component("OR", "OR Gate", "Digital Logic",
                  "Multi-input digital OR gate.", "delay=10ns"),
        component("NOT", "NOT Gate", "Digital Logic",
                  "Single-input digital inverter.", "delay=5ns"),
        component("NAND", "NAND Gate", "Digital Logic",
                  "Inverted AND gate.", "delay=10ns"),
        component("XOR", "XOR Gate", "Digital Logic",
                  "Exclusive-OR digital gate.", "delay=10ns"),
        component("DFlipFlop", "D Flip-Flop", "Digital Logic",
                  "Rising-edge D flip-flop prepared for D, CLK, Q and /Q pins.", "delay=10ns"),

        component("ADC", "ADC", "Converters / Advanced",
                  "Ideal analog-to-digital converter with configurable resolution.", "bits=4"),
        component("DAC", "DAC", "Converters / Advanced",
                  "Ideal digital-to-analog converter with configurable resolution.", "bits=4"),
        component("MCU", "Microcontroller", "Converters / Advanced",
                  "Educational MCU shell prepared for firmware, registers, RAM and ports.", "firmware=empty"),
        component("EEPROM", "External EEPROM / RAM", "Converters / Advanced",
                  "External byte-addressable memory peripheral.", "256 B"),
        component("LCD16x2", "LCD 16x2", "Converters / Advanced",
                  "Character display prepared for standard LCD commands.", "Hello"),
        component("Keypad4x4", "Keypad 4x4", "Converters / Advanced",
                  "Matrix keypad prepared for row/column scanning.", "key=none"),

        component("VoltageProbe", "Voltage Probe", "Instruments",
                  "Single-node voltage inspection tool.", "V"),
        component("Voltmeter", "Voltmeter", "Instruments",
                  "Two-terminal digital voltage measurement instrument.", "0.000 V"),
        component("Ammeter", "Ammeter", "Instruments",
                  "Series current measurement instrument.", "0.000 A"),
        component("Oscilloscope", "Oscilloscope", "Instruments",
                  "Two-channel waveform display instrument.", "CH1, CH2")};
    return definitions;
}

QList<ComponentDefinition> ComponentCatalog::filtered(const QString &query)
{
    const QString normalized = query.trimmed().toLower();
    if (normalized.isEmpty())
        return all();

    QList<ComponentDefinition> matches;
    for (const ComponentDefinition &definition : all())
    {
        const QString searchable =
            (definition.id + QLatin1Char(' ') + definition.displayName + QLatin1Char(' ') +
             definition.category + QLatin1Char(' ') + definition.description + QLatin1Char(' ') +
             definition.defaultValue)
                .toLower();
        if (searchable.contains(normalized))
            matches.append(definition);
    }
    return matches;
}

const ComponentDefinition *ComponentCatalog::find(const QString &id)
{
    for (const ComponentDefinition &definition : all())
    {
        if (definition.id == id)
            return &definition;
    }
    return nullptr;
}
