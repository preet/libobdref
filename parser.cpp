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

    bool Parser::BuildMessage(const QString &specName,
                              const QString &protocolName,
                              const QString &addressName,
                              const QString &paramName,
                              Message &requestMsg)
    {
        bool foundSpec = false;
        bool foundProtocol = false;
        bool foundAddress = false;
        bool foundParams = false;
        bool foundParameter = false;

        pugi::xml_node nodeSpec = m_xmlDoc.child("spec");
        for(nodeSpec; nodeSpec; nodeSpec = nodeSpec.next_sibling("spec"))
        {
            QString fileSpecName(nodeSpec.attribute("name").value());
            if(fileSpecName == specName)
            {
                foundSpec = true;
                pugi::xml_node nodeProtocol = nodeSpec.child("protocol");
                for(nodeProtocol; nodeProtocol; nodeProtocol = nodeProtocol.next_sibling("protocol"))
                {
                    QString fileProtocolName(nodeProtocol.attribute("name").value());
                    if(fileProtocolName == protocolName)
                    {
                        foundProtocol = true;
                        pugi::xml_node nodeAddress = nodeProtocol.child("address");
                        for(nodeAddress; nodeAddress; nodeAddress = nodeAddress.next_sibling("address"))
                        {
                            QString fileAddressName(nodeAddress.attribute("name").value());
                            if(fileAddressName == addressName)
                            {
                                foundAddress = true;

                                // BUILD HEADER HERE

                                // special case 1: ISO 14230-4
                                // we have to embed the number of data bytes being
                                // sent out in the prio byte of the header -- this
                                // must be done later on

                                // special case 2: ISO 15765-4 (11-bit id)
                                // we're given one and a half bytes to represent
                                // 11-bit headers (ie. 0x7DF), how should this be stored?

                                // special case 3: ISO 15765-4 (29-bit id)
                                // we have to include the special 'format' byte
                                // that differentiates between physical and functional
                                // addressing, so this header is made up of four bytes:
                                // [prio] [format] [target] [source]
                            }
                        }
                    }
                }

                if(!foundProtocol)
                {   break;   }

                pugi::xml_node nodeParams = nodeSpec.child("parameters");
                for(nodeParams; nodeParams; nodeParams = nodeParams.next_sibling("parameters"))
                {
                    QString fileParamsName(nodeParams.attribute("address").value());
                    if(fileParamsName == addressName)
                    {
                        foundParams = true;
                        pugi::xml_node nodeParameter = nodeParams.child("parameter");
                        for(nodeParameter; nodeParameter; nodeParameter = nodeParameter.next_sibling("parameter"))
                        {
                            QString fileParameterName(nodeParameter.attribute("name").value());
                            if(fileParameterName == paramName)
                            {
                                foundParameter = true;

                                // BUILD DATA BYTES HERE

                                QStringList listConditionExprs;

                                // save <parse /> children nodes this parameter has
                                pugi::xml_node parseChild = nodeParameter.child("parse");
                                for(parseChild; parseChild; parseChild = parseChild.next_sibling("parse"))
                                {
                                    // SAVE PARSE EXPR
                                }

                                // call walkConditionTree on <condition> children this parameter has
                                pugi::xml_node condChild = nodeParameter.child("condition");
                                for(condChild; condChild; condChild = condChild.next_sibling("condition"))
                                {
                                    walkConditionTree(condChild, listConditionExprs, requestMsg);
                                }

                                return true;
                            }
                        }
                    }
                }
            }
        }

        #ifdef OBDREF_DEBUG_ON
        if(!foundSpec)
        {
            std::cerr << "OBDREF_DEBUG: Error: Could not find spec "
                      << specName.toStdString() << std::endl;
        }
        else if(!foundProtocol)
        {
            std::cerr << "OBDREF_DEBUG: Error: Could not find protocol "
                      << protocolName.toStdString() << std::endl;
        }
        else if(!foundAddress)
        {
            std::cerr << "OBDREF_DEBUG: Error: Could not find address "
                      << addressName.toStdString() <<std::endl;
        }
        else if(!foundParams)
        {
            std::cerr << "OBDREF_DEBUG: Error: Could not find parameters group "
                      << addressName.toStdString() << std::endl;
        }
        else if(!foundParameter)
        {
            std::cerr << "OBDREF_DEBUG: Error: Could not find parameter "
                      << paramName.toStdString() << std::endl;
        }
        #endif

        return false;
    }

    void Parser::walkConditionTree(pugi::xml_node &nodeCondition,
                                   QStringList &listConditionExprs,
                                   Message &requestMsg)
    {
        // add this condition expr to list
        QString conditionExpr(nodeCondition.attribute("expr").value());
        listConditionExprs.push_back(conditionExpr);        

        // save <parse /> children nodes this condition has
        pugi::xml_node parseChild = nodeCondition.child("parse");
        for(parseChild; parseChild; parseChild = parseChild.next_sibling("parse"))
        {
            // SAVE PARSE EXPR WITH CONDITION LIST

            #ifdef OBDREF_DEBUG_ON
            std::cerr << "OBDREF_DEBUG: Conditions: ";
            for(int i=0; i < listConditionExprs.size(); i++)
            {   std::cerr << listConditionExprs.at(i).toStdString() << ", ";   }
            std::cerr << " Parse Expr: " << parseChild.attribute("expr").value();
            std::cerr << std::endl;
            #endif
        }

        // call walkConditionTree recursively on <condition>
        // children this condition has
        pugi::xml_node condChild = nodeCondition.child("condition");
        for(condChild; condChild; condChild = condChild.next_sibling("condition"))
        {
            walkConditionTree(condChild, listConditionExprs, requestMsg);
        }

        // remove last condition expr added (since this is a DFS)
        listConditionExprs.removeLast();
    }
}
