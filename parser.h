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
    Parser(QString const &pathToXmlData);
    ~Parser();
};
}
#endif // PARSER_H
