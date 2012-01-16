#include "parser.h"

namespace obdref
{
    Parser::Parser(QString const &pathToXmlData)
    {
        pugi::xml_document xmlDoc;
        pugi::xml_parse_result xmlParseResult;
        const char* xmlSource = pathToXmlData.toLocal8Bit().data();
        xmlParseResult = xmlDoc.load_file(xmlSource);

    #ifdef OBDREF_DEBUG_ON
        if(xmlParseResult)
        {   std::cerr << "XML [" << xmlSource << "] parsed without errors." << std::endl;   }
        else
        {
            std::cerr << "XML [" << xmlSource << "] errors!" << std::endl;
            std::cerr << "Error Desc: " << xmlParseResult.description() << std::endl;
            std::cerr << "Error Offset: " << xmlParseResult.offset
                      << "(error at [..." << (xmlSource+xmlParseResult.offset)
                      << "]" << std::endl << std::endl;
        }
    #endif
    }
}
