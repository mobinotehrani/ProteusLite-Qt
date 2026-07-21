#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

struct FirmwareLoadResult
{
    bool success{false};
    QByteArray flashBytes;
    QString error;
    QStringList notes;
    quint32 firstAddress{0};
    quint32 lastAddress{0};
};

class IntelHexLoader final
{
  public:
    static FirmwareLoadResult loadFile(const QString &filePath);
    static FirmwareLoadResult parse(const QByteArray &contents);
};
