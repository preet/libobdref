#ifndef PARSER_H
#define PARSER_H

#include <iostream>
#include <QString>
#include <QDebug>
#include "pugixml/pugixml.hpp"
#include "muparser/muParser.h"
#include "message.h"

#define dValueOfBitPos0 1;
#define dValueOfBitPos1 2;
#define dValueOfBitPos2 4;
#define dValueOfBitPos3 8;
#define dValueOfBitPos4 16;
#define dValueOfBitPos5 32;
#define dValueOfBitPos6 64;
#define dValueOfBitPos7 128;

namespace obdref
{

struct dblArray_s8
{   double bits[8];   };

class Parser
{
public:
    Parser(QString const &filePath, bool &parsedOk);

    bool BuildMessage(QString const &specName,
                      QString const &protocolName,
                      QString const &addressName,
                      QString const &paramName,
                      Message &requestMsg);

    bool ParseMessage(Message const &parseMsg,
                      QString &jsonStr);

private:
    void walkConditionTree(pugi::xml_node &nodeParameter,
                           QStringList &listConditions,
                           Message &requestMsg);

    uint stringToUInt(bool &convOk, QString const &parseStr);

    // pugixml vars
    QString m_xmlFilePath;
    pugi::xml_document m_xmlDoc;

    // muParser vars
    mu::Parser m_parser;
    int m_listDecValOfBitPos[8];
    double m_listBytesAZ[26];
    dblArray_s8 m_listBytesAZBits[26];

};
}
#endif // PARSER_H
