#include "parser.h"

namespace obdref
{
    Parser::Parser(QString const &filePath, bool &parsedOk)
    {
        m_xmlFilePath = filePath;

        pugi::xml_parse_result xmlParseResult;
        xmlParseResult = m_xmlDoc.load_file(filePath.toStdString().c_str());

        if(xmlParseResult)
        {   parsedOk = true;   }
        else
        {   parsedOk = false;   }

        #ifdef OBDREF_DEBUG_ON
        if(xmlParseResult)
        {   std::cerr << "OBDREF_DEBUG: XML [" << filePath.toStdString() << "] parsed without errors." << std::endl;   }
        else
        {
            std::cerr << "OBDREF_DEBUG: XML [" << filePath.toStdString() << "] errors!" << std::endl;
            std::cerr << "OBDREF_DEBUG: Error Desc: " << xmlParseResult.description() << std::endl;
            std::cerr << "OBDREF_DEBUG: Error Offset Char: " << xmlParseResult.offset << std::endl;
        }
        #endif
    }

    bool Parser::BuildRequest(const QString &specName,
                              const QString &protocolName,
                              const QString &addressName,
                              const QString &paramName,
                              Message &requestMsg)
    {
        bool foundSpec = false;
        pugi::xml_node nodeSpec = m_xmlDoc.child("spec");
        for(nodeSpec; nodeSpec; nodeSpec = nodeSpec.next_sibling("spec"))
        {
            QString fileSpecName(nodeSpec.attribute("name").value());
            if(fileSpecName == specName)
            {
                foundSpec = true;
                bool foundProtocol = false;
                pugi::xml_node nodeProtocol = nodeSpec.child("protocol");
                for(nodeProtocol; nodeProtocol; nodeProtocol = nodeProtocol.next_sibling("protocol"))
                {
                    QString fileProtocolName(nodeProtocol.attribute("name").value());
                    if(fileProtocolName == protocolName)
                    {
                        foundProtocol = true;
                        bool foundAddress = false;
                        pugi::xml_node nodeAddress = nodeProtocol.child("address");
                        for(nodeAddress; nodeAddress; nodeAddress = nodeAddress.next_sibling("address"))
                        {
                            QString fileAddressName(nodeAddress.attribute("name").value());
                            if(fileAddressName == addressName)
                            {
                                foundAddress = true;

                                // BUILD HEADER HERE
                            }
                        }
                    }
                }


                bool foundParams = false;
                pugi::xml_node nodeParams = nodeSpec.child("parameters");
                for(nodeParams; nodeParams; nodeParams = nodeParams.next_sibling("parameters"))
                {
                    QString fileParamsName(nodeParams.attribute("address").value());
                    if(fileParamsName == addressName)
                    {
                        foundParams = true;
                        bool foundParameter = false;
                        pugi::xml_node nodeParameter = nodeParams.child("parameter");
                        for(nodeParameter; nodeParameter; nodeParameter = nodeParameter.next_sibling("parameter"))
                        {
                            QString fileParameterName(nodeParameter.attribute("name").value());
                            if(fileParameterName == paramName)
                            {
                                foundParameter = true;

                                // BUILD DATA BYTES HERE
                            }
                        }
                    }
                }
            }
        }
    }

    void Parser::walkConditionTree(pugi::xml_node &nodeCondition, QStringList &listConditionExprs, Message &myMsg)
    {
        // add this condition expr to list
        QString conditionExpr(nodeCondition.attribute("expr").value());
        listConditionExprs.push_back(conditionExpr);

        // save <parse /> children nodes this condition has
        pugi::xml_node parseChild = nodeCondition.child("parse");
        for(parseChild; parseChild; parseChild = parseChild.next_sibling("parse"))
        {
            // SAVE PARSE EXPR WITH CONDITION LIST
        }

        // call walkConditionTree recursively on <condition>
        // children this condition has
        pugi::xml_node condChild = nodeCondition.child("condition");
        for(condChild; condChild; condChild = condChild.next_sibling("condition"))
        {
            walkConditionTree(nodeCondition, listConditionExprs, myMsg);
        }

        // remove last condition expr added (since this is a DFS)
        listConditionExprs.removeLast();
    }
}
