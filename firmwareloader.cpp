#include "firmwareloader.h"

#include <QFile>
#include <QHash>
#include <QList>
#include <QVector>

#include <algorithm>
#include <limits>

namespace
{
bool parseHexByte(const QByteArray &line, int offset, quint8 &value)
{
    if (offset < 0 || offset + 2 > line.size())
        return false;

    for (int index = offset; index < offset + 2; ++index)
    {
        const char character = line.at(index);
        const bool digit = character >= '0' && character <= '9';
        const bool lowerHex = character >= 'a' && character <= 'f';
        const bool upperHex = character >= 'A' && character <= 'F';
        if (!digit && !lowerHex && !upperHex)
            return false;
    }

    bool ok = false;
    const int parsed = line.mid(offset, 2).toInt(&ok, 16);
    if (!ok || parsed < 0 || parsed > 255)
        return false;

    value = static_cast<quint8>(parsed);
    return true;
}

QString lineError(int lineNumber, const QString &message)
{
    return QStringLiteral("Line %1: %2").arg(lineNumber).arg(message);
}
}

FirmwareLoadResult IntelHexLoader::loadFile(const QString &filePath)
{
    FirmwareLoadResult result;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        result.error = QStringLiteral("Unable to open firmware file: %1").arg(file.errorString());
        return result;
    }

    result = parse(file.readAll());
    if (result.success)
        result.notes.prepend(QStringLiteral("Loaded %1").arg(filePath));
    return result;
}

FirmwareLoadResult IntelHexLoader::parse(const QByteArray &contents)
{
    FirmwareLoadResult result;
    const QList<QByteArray> rawLines = contents.split('\n');
    QHash<quint32, quint8> bytesByAddress;
    quint32 baseAddress = 0;
    bool eofSeen = false;
    quint32 firstAddress = std::numeric_limits<quint32>::max();
    quint32 lastAddress = 0;

    for (int index = 0; index < rawLines.size(); ++index)
    {
        QByteArray line = rawLines.at(index).trimmed();
        const int lineNumber = index + 1;
        if (line.isEmpty())
            continue;
        if (eofSeen)
        {
            result.error = lineError(lineNumber, QStringLiteral("A record appears after the EOF record."));
            return result;
        }
        if (!line.startsWith(':'))
        {
            result.error = lineError(lineNumber, QStringLiteral("Intel HEX records must start with ':'."));
            return result;
        }

        line.remove(0, 1);
        if (line.size() < 10 || (line.size() % 2) != 0)
        {
            result.error = lineError(lineNumber, QStringLiteral("The record length is invalid."));
            return result;
        }

        quint8 byteCount = 0;
        quint8 addressHigh = 0;
        quint8 addressLow = 0;
        quint8 recordType = 0;
        if (!parseHexByte(line, 0, byteCount) || !parseHexByte(line, 2, addressHigh) ||
            !parseHexByte(line, 4, addressLow) || !parseHexByte(line, 6, recordType))
        {
            result.error = lineError(lineNumber, QStringLiteral("The record header contains invalid hex digits."));
            return result;
        }

        const int expectedCharacters = 2 * (5 + static_cast<int>(byteCount));
        if (line.size() != expectedCharacters)
        {
            result.error = lineError(lineNumber,
                                     QStringLiteral("Byte count does not match the record length."));
            return result;
        }

        QVector<quint8> recordBytes;
        recordBytes.reserve(5 + byteCount);
        for (int offset = 0; offset < line.size(); offset += 2)
        {
            quint8 parsed = 0;
            if (!parseHexByte(line, offset, parsed))
            {
                result.error = lineError(lineNumber, QStringLiteral("The record contains invalid hex digits."));
                return result;
            }
            recordBytes.append(parsed);
        }

        quint32 checksum = 0;
        for (quint8 value : recordBytes)
            checksum += value;
        if ((checksum & 0xFFu) != 0u)
        {
            result.error = lineError(lineNumber, QStringLiteral("Checksum validation failed."));
            return result;
        }

        const quint16 address = static_cast<quint16>((addressHigh << 8) | addressLow);
        if (recordType == 0x00)
        {
            for (int dataIndex = 0; dataIndex < byteCount; ++dataIndex)
            {
                const quint32 absoluteAddress = baseAddress + address + static_cast<quint32>(dataIndex);
                if (absoluteAddress > 0xFFFFu)
                {
                    result.error = lineError(
                        lineNumber,
                        QStringLiteral("Firmware address exceeds the 64 KiB educational MCU flash range."));
                    return result;
                }
                const quint8 value = recordBytes.at(4 + dataIndex);
                if (bytesByAddress.contains(absoluteAddress) &&
                    bytesByAddress.value(absoluteAddress) != value)
                {
                    result.error = lineError(
                        lineNumber,
                        QStringLiteral("Two records contain different data for address 0x%1.")
                            .arg(absoluteAddress, 4, 16, QLatin1Char('0')));
                    return result;
                }
                bytesByAddress.insert(absoluteAddress, value);
                firstAddress = std::min(firstAddress, absoluteAddress);
                lastAddress = std::max(lastAddress, absoluteAddress);
            }
        }
        else if (recordType == 0x01)
        {
            if (byteCount != 0 || address != 0)
            {
                result.error = lineError(
                    lineNumber,
                    QStringLiteral("EOF records must use address zero and contain no data."));
                return result;
            }
            eofSeen = true;
        }
        else if (recordType == 0x02)
        {
            if (address != 0)
            {
                result.error = lineError(
                    lineNumber,
                    QStringLiteral("Extended segment records must use address zero."));
                return result;
            }
            if (byteCount != 2)
            {
                result.error = lineError(lineNumber,
                                         QStringLiteral("Extended segment records must contain two bytes."));
                return result;
            }
            baseAddress = static_cast<quint32>((recordBytes.at(4) << 8) | recordBytes.at(5)) << 4;
        }
        else if (recordType == 0x04)
        {
            if (address != 0)
            {
                result.error = lineError(
                    lineNumber,
                    QStringLiteral("Extended linear records must use address zero."));
                return result;
            }
            if (byteCount != 2)
            {
                result.error = lineError(lineNumber,
                                         QStringLiteral("Extended linear records must contain two bytes."));
                return result;
            }
            baseAddress = static_cast<quint32>((recordBytes.at(4) << 8) | recordBytes.at(5)) << 16;
        }
        else if (recordType == 0x03 || recordType == 0x05)
        {
            if (byteCount != 4 || address != 0)
            {
                result.error = lineError(
                    lineNumber,
                    QStringLiteral("Start-address records must use address zero and contain four bytes."));
                return result;
            }
            result.notes.append(lineError(lineNumber,
                                          QStringLiteral("Start-address record accepted but ignored.")));
        }
        else
        {
            result.error = lineError(lineNumber,
                                     QStringLiteral("Unsupported Intel HEX record type 0x%1.")
                                         .arg(recordType, 2, 16, QLatin1Char('0')));
            return result;
        }
    }

    if (!eofSeen)
    {
        result.error = QStringLiteral("The firmware file does not contain an Intel HEX EOF record.");
        return result;
    }
    if (bytesByAddress.isEmpty())
    {
        result.error = QStringLiteral("The firmware file does not contain any data records.");
        return result;
    }

    result.firstAddress = firstAddress;
    result.lastAddress = lastAddress;
    result.flashBytes.fill(static_cast<char>(0xFF), static_cast<int>(lastAddress + 1));
    for (auto entry = bytesByAddress.cbegin(); entry != bytesByAddress.cend(); ++entry)
        result.flashBytes[static_cast<int>(entry.key())] = static_cast<char>(entry.value());

    result.success = true;
    result.notes.append(QStringLiteral("Validated %1 byte(s), address range 0x%2-0x%3.")
                            .arg(bytesByAddress.size())
                            .arg(firstAddress, 4, 16, QLatin1Char('0'))
                            .arg(lastAddress, 4, 16, QLatin1Char('0')));
    return result;
}
