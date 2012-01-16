#ifndef PARSER_H
#define PARSER_H

#include <iostream>
#include <QString>
#include "pugixml/pugixml.hpp"
#include "message.h"

namespace obdref
{
class Parser
{
public:
    Parser(QString const &filePath, bool &parsedOk);

    bool BuildRequest(QString const &specName,
                      QString const &protocolName,
                      QString const &addressName,
                      QString const &paramName,
                      Message &requestMsg);

    bool ParseMessage(Message const &parseMsg,
                      QString &jsonStr);

private:
    void walkParameterParseTree(pugi::xml_node &nodeParameter);

    QString m_xmlFilePath;
    pugi::xml_document m_xmlDoc;
};
}
#endif // PARSER_H
