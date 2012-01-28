/*
    Copyright (c) 2012 Preet Desai (preet.desai@gmail.com)

    Permission is hereby granted, free of charge, to any person obtaining
    a copy of this software and associated documentation files (the
    "Software"), to deal in the Software without restriction, including
    without limitation the rights to use, copy, modify, merge, publish,
    distribute, sublicense, and/or sell copies of the Software, and to
    permit persons to whom the Software is furnished to do so, subject to
    the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
    LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
    OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

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

    bool BuildMessageFrame(MessageFrame &msgFrame);

    bool ParseMessageFrame(MessageFrame &msgFrame,
                           QList<Data> &listDataResults);

    QStringList GetParameterNames(QString const &specName,
                                  QString const &protocolName,
                                  QString const &addressName);

    QStringList GetLastKnownErrors();



private:
    void walkConditionTree(pugi::xml_node &nodeParameter,
                           QStringList &listConditions,
                           MessageFrame &msgFrame);

    bool processRawData_ISO_15765_4_ST(MessageFrame &msgFrame);

    bool parseMessage(MessageFrame const &msgFrame,
                      QString const &parseExpr,
                      QList<double> &myResults);

    void convToDecEquivExpression(QString &parseExpr);

    uint stringToUInt(bool &convOk, QString const &parseStr);

    QTextStream & getErrorStream();
    ubyte GetHexByteStrValue(QByteArray const &hexByte);
    QByteArray GetValueHexByteStr(ubyte const &value);

    static mu::value_type muLogicalNot(mu::value_type);
    static mu::value_type muBitwiseNot(mu::value_type);
    static mu::value_type muBitwiseOr(mu::value_type,mu::value_type);
    static mu::value_type muBitwiseAnd(mu::value_type,mu::value_type);

    // pugixml vars
    QString m_xmlFilePath;
    pugi::xml_document m_xmlDoc;

    // muParser vars
    mu::Parser m_parser;
    uint m_listDecValOfBitPos[8];
    QMap<ubyte,QByteArray> m_mapValToHexByte;   // TODO, conv to QHash
    QMap<QByteArray,ubyte> m_mapHexByteToVal;
    double m_listExprVars[MAX_EXPR_VARS];

    // errors
    QTextStream m_lkErrors;
    QString m_lkErrorString;
};
}

#define OBDREFDEBUG getErrorStream()
#endif // PARSER_H
