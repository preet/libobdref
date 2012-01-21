#ifndef PARSER_H
#define PARSER_H

// Qt includes
#include <QDebug>

// pugixml includes
#include "pugixml/pugixml.hpp"

// muParser includes
#include "muparser/muParser.h"

// obdref data types
#include "message.hpp"

#define MAX_EXPR_VARS 64

namespace obdref
{

class Parser
{

public:
    Parser(QString const &filePath, bool &parsedOk);

    bool BuildMessageFrame(QString const &specName,
                           QString const &protocolName,
                           QString const &addressName,
                           QString const &paramName,
                           MessageFrame &msgFrame);

    bool ParseMessageFrame(MessageFrame const &msgFrame,
                           Data &paramData);

    QStringList GetLastKnownErrors();

private:
    void walkConditionTree(pugi::xml_node &nodeParameter,
                           QStringList &listConditions,
                           MessageFrame &msgFrame);

    bool parseMessage(MessageFrame const &msgFrame,
                      QString const &parseExpr,
                      double &myResult);

    void convToDecEquivExpression(QString &parseExpr);

    uint stringToUInt(bool &convOk, QString const &parseStr);

    QTextStream & getErrorStream();

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
    double m_listExprVars[MAX_EXPR_VARS];

    // errors
    QTextStream m_lkErrors;
    QString m_lkErrorString;
};
}

#define OBDREFDEBUG getErrorStream()
#endif // PARSER_H
