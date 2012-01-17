#ifndef MESSAGE_H
#define MESSAGE_H

#include <QStringList>

namespace obdref
{

class ParseInfo
{
public:
    QString expr;

    bool isNumerical;

    bool hasMin;
    double valueMin;

    bool hasMax;
    double valueMax;

    bool isFlag;
    QString valueIfTrue;
    QString valueIfFalse;

    QStringList listConditions;
};

class Message
{
public:
    QString spec;
    QString protocol;
    QString address;
    QString name;

    // for request messages
    QByteArray requestHeaderBytes;
    QByteArray requestDataBytes;

    // for response or monitored messages
    QByteArray expectedHeaderBytes;
    QByteArray expectedDataPrefix;
    uint expectedDataBytes;

    QList<ParseInfo> listParseInfo;
};

}
#endif // MESSAGE_H
