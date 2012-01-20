#ifndef PARSER_H
#define PARSER_H

#include <iostream>
#include <QString>
#include <QDebug>
#include "pugixml/pugixml.hpp"
#include "muparser/muParser.h"
#include "message.h"

#define MAX_EXPR_VARS 64

namespace obdref
{

struct dblArray_s8
{   double bit[8];   };

struct dblArray_s26
{   double byte[26];   };

class Parser
{
public:
    Parser(QString const &filePath, bool &parsedOk);

    bool BuildMessageFrame(QString const &specName,
                           QString const &protocolName,
                           QString const &addressName,
                           QString const &paramName,
                           MessageFrame &msgFrame);

    bool ParseMessageFrame(MessageFrame const &parseMsg,
                           Data &paramData);

    bool ParseMessage(Message const &parseMsg,
                      QString &jsonStr);

private:
    void walkConditionTree(pugi::xml_node &nodeParameter,
                           QStringList &listConditions,
                           MessageFrame &msgFrame);

    uint stringToUInt(bool &convOk, QString const &parseStr);

    static mu::value_type muLogicalNot(mu::value_type);
    static mu::value_type muBitwiseNot(mu::value_type);
    static mu::value_type muBitwiseOr(mu::value_type,mu::value_type);
    static mu::value_type muBitwiseAnd(mu::value_type,mu::value_type);

    // pugixml vars
    QString m_xmlFilePath;
    pugi::xml_document m_xmlDoc;

    // muParser vars
    mu::Parser m_parser;
    int m_listDecValOfBitPos[8];
    double m_listExprVars[64];

};
}
#endif // PARSER_H
