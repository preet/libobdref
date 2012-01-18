#include "parser.h"

namespace obdref
{
    Parser::Parser(QString const &filePath, bool &parsedOk)
    {
        // setup pugixml with xml source file
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

                                // special case 1: ISO 15765-4 (11-bit id)
                                // store 11-bits in two bytes
                                if(protocolName == "ISO 15765-4 Standard")
                                {
                                    pugi::xml_node nodeReq = nodeAddress.child("request");
                                    if(nodeReq)
                                    {
                                        QString myHeader(nodeReq.attribute("identifier").value());
                                        if(!myHeader.isEmpty())
                                        {
                                            bool convOk = false;
                                            uint headerVal = stringToUInt(convOk,myHeader);
                                            uint upperByte = headerVal & 3840;      // 0b0000111100000000 = 3840
                                            uint lowerByte = headerVal & 255;       // 0b0000000011111111 = 255

                                            requestMsg.requestHeaderBytes.append(char(upperByte));
                                            requestMsg.requestHeaderBytes.append(char(lowerByte));
                                        }
                                    }

                                    pugi::xml_node nodeResp = nodeAddress.child("response");
                                    if(nodeResp)
                                    {
                                        QString myHeader(nodeResp.attribute("identifier").value());
                                        if(!myHeader.isEmpty())
                                        {
                                            bool convOk = false;
                                            uint headerVal = stringToUInt(convOk,myHeader);
                                            uint upperByte = headerVal & 3840;      // 0b0000111100000000 = 3840
                                            uint lowerByte = headerVal & 255;       // 0b0000000011111111 = 255

                                            requestMsg.expectedHeaderBytes.append(char(upperByte));
                                            requestMsg.expectedHeaderBytes.append(char(lowerByte));
                                        }
                                    }
                                }

                                // special case 2: ISO 15765-4 (29-bit id)
                                // we have to include the special 'format' byte
                                // that differentiates between physical and functional
                                // addressing, so this header is made up of four bytes:
                                // [prio] [format] [target] [source]
                                else if(protocolName == "ISO 15765-4 Extended")
                                {
                                    pugi::xml_node nodeReq = nodeAddress.child("request");
                                    if(nodeReq)
                                    {
                                        QString headerPrio(nodeReq.attribute("prio").value());
                                        QString headerFormat(nodeReq.attribute("format").value());
                                        QString headerTarget(nodeReq.attribute("target").value());
                                        QString headerSource(nodeReq.attribute("source").value());

                                        bool convOk = false;

                                        uint prioByte = stringToUInt(convOk,headerPrio);
                                        requestMsg.requestHeaderBytes.append(char(prioByte));

                                        uint formatByte = stringToUInt(convOk,headerFormat);
                                        requestMsg.requestHeaderBytes.append(char(formatByte));

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        requestMsg.requestHeaderBytes.append(char(targetByte));

                                        uint sourceByte = stringToUInt(convOk,headerSource);
                                        requestMsg.requestHeaderBytes.append(char(sourceByte));
                                    }

                                    pugi::xml_node nodeResp = nodeAddress.child("response");
                                    if(nodeResp)
                                    {
                                        QString headerPrio(nodeResp.attribute("prio").value());
                                        QString headerFormat(nodeResp.attribute("format").value());
                                        QString headerTarget(nodeResp.attribute("target").value());
                                        QString headerSource(nodeResp.attribute("source").value());

                                        bool convOk = false;

                                        uint prioByte = stringToUInt(convOk,headerPrio);
                                        requestMsg.expectedHeaderBytes.append(char(prioByte));

                                        uint formatByte = stringToUInt(convOk,headerFormat);
                                        requestMsg.expectedHeaderBytes.append(char(formatByte));

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        requestMsg.expectedHeaderBytes.append(char(targetByte));

                                        if(!headerSource.isEmpty())
                                        {
                                            uint sourceByte = stringToUInt(convOk,headerSource);
                                            requestMsg.expectedHeaderBytes.append(char(sourceByte));
                                        }
                                    }
                                }

                                else
                                {
                                    // special case 3/note: ISO 14230-4
                                    // the data length needs to be inserted into the
                                    // priority byte (has to be done later)

                                    pugi::xml_node nodeReq = nodeAddress.child("request");
                                    if(nodeReq)
                                    {
                                        QString headerPrio(nodeReq.attribute("prio").value());
                                        QString headerTarget(nodeReq.attribute("target").value());
                                        QString headerSource(nodeReq.attribute("source").value());

                                        bool convOk = false;
                                        uint prioByte = stringToUInt(convOk,headerPrio);
                                        requestMsg.requestHeaderBytes.append(char(prioByte));

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        requestMsg.requestHeaderBytes.append(char(targetByte));

                                        uint sourceByte = stringToUInt(convOk,headerSource);
                                        requestMsg.requestHeaderBytes.append(char(sourceByte));
                                    }

                                    pugi::xml_node nodeResp = nodeAddress.child("response");
                                    if(nodeResp)
                                    {
                                        QString headerPrio(nodeResp.attribute("prio").value());
                                        QString headerTarget(nodeResp.attribute("target").value());
                                        QString headerSource(nodeResp.attribute("source").value());

                                        bool convOk = false;
                                        uint prioByte = stringToUInt(convOk,headerPrio);
                                        requestMsg.expectedHeaderBytes.append(char(prioByte));

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        requestMsg.expectedHeaderBytes.append(char(targetByte));

                                        if(!headerSource.isEmpty())
                                        {
                                            uint sourceByte = stringToUInt(convOk,headerSource);
                                            requestMsg.expectedHeaderBytes.append(char(sourceByte));
                                        }
                                    }
                                }
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
                                bool convOk = false;

                                // data request bytes
                                QString requestBytes(nodeParameter.attribute("request").value());
                                QStringList listRequestBytes = requestBytes.split(" ");
                                for(int i=0; i < listRequestBytes.size(); i++)
                                {
                                    uint dataByte = stringToUInt(convOk,listRequestBytes.at(i));
                                    requestMsg.requestDataBytes.append(char(dataByte));
                                }

                                // expected response prefix
                                QString respPrefix(nodeParameter.attribute("response.prefix").value());
                                QStringList listRespPrefix = respPrefix.split(" ");
                                for(int i=0; i < listRespPrefix.size(); i++)
                                {
                                    uint dataByte = stringToUInt(convOk,listRespPrefix.at(i));
                                    requestMsg.expectedDataPrefix.append(char(dataByte));
                                }

                                // expected num response bytes
                                QString respBytes(nodeParameter.attribute("response.bytes").value());
                                requestMsg.expectedDataBytes = stringToUInt(convOk,respBytes);

                                // save <parse /> children nodes this parameter has
                                pugi::xml_node parseChild = nodeParameter.child("parse");
                                for(parseChild; parseChild; parseChild = parseChild.next_sibling("parse"))
                                {
                                    // SAVE PARSE EXPR                                    
                                    QString pExpr(parseChild.attribute("expr").value());
                                    if(!pExpr.isEmpty())
                                    {
                                        ParseInfo parseInfo;
                                        parseInfo.expr = pExpr;

                                        QString pExprTrue(parseChild.attribute("true").value());
                                        QString pExprFalse(parseChild.attribute("false").value());
                                        if(pExprTrue.isEmpty() && pExprFalse.isEmpty())
                                        {   // assume value is numerical
                                            parseInfo.isNumerical = true;
                                            if(parseChild.attribute("min"))
                                            {
                                                parseInfo.hasMin = true;
                                                parseInfo.valueMin = parseChild.attribute("min").as_double();
                                            }
                                            if(parseChild.attribute("max"))
                                            {
                                                parseInfo.hasMax = true;
                                                parseInfo.valueMax = parseChild.attribute("max").as_double();
                                            }
                                        }
                                        else
                                        {   // assume value is a boolean flag
                                            parseInfo.isFlag = true;
                                            parseInfo.valueIfTrue = pExprTrue;
                                            parseInfo.valueIfFalse = pExprFalse;
                                        }

                                        // save parse info
                                        requestMsg.listParseInfo.append(parseInfo);
                                    }
                                }

                                // call walkConditionTree on <condition> children this parameter has
                                QStringList listConditionExprs;
                                pugi::xml_node condChild = nodeParameter.child("condition");
                                for(condChild; condChild; condChild = condChild.next_sibling("condition"))
                                {
                                    walkConditionTree(condChild, listConditionExprs, requestMsg);
                                }

                                #ifdef OBDREF_DEBUG_ON
                                std::cerr << "OBDREF_DEBUG: " << specName.toStdString() << ", "
                                          << protocolName.toStdString() << ", " << addressName.toStdString()
                                          << ", " << paramName.toStdString() << ":" << std::endl;

                                for(int i=0; i < requestMsg.listParseInfo.size(); i++)
                                {
                                    std::cerr << "OBDREF_DEBUG: \t";
                                    for(int j=0; j < requestMsg.listParseInfo.at(i).listConditions.size(); j++)
                                    {
                                        std::cerr << " { ";
                                        std::cerr << requestMsg.listParseInfo.at(i).listConditions.at(j).toStdString()
                                                  << " } | ";
                                    }
                                    std::cerr << "Eval: { " << requestMsg.listParseInfo.at(i).expr.toStdString() << " }"
                                              << std::endl;
                                }
                                #endif

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

    bool Parser::ParseMessage(const Message &parseMsg, QString &jsonStr)
    {
        QRegExp rx1; QRegExp rx2; int pos=0;
        QString sampleExpr = "((A*256)+B5+F3)/32768";
        QList<double> listVars;

        // we mandate that the only variables allowed are bytes,
        // represented by a single capital letter, ie 'A' and
        // a given byte's individual bits, indicated by a number
        // after the byte's letter, ie. 'A2' would be the second
        // bit in byte A.

        // 1 -  use regexp matching to find all
        //      instances of [A-Z],[A-Z][0-7]
        rx1.setPattern("([A-Z]|[A-Z][0-7])+");
        QStringList listRegExpMatches;
        while ((pos = rx1.indexIn(sampleExpr, pos)) != -1)
        {
             listRegExpMatches << rx1.cap(1);
             pos += rx1.matchedLength();
        }

        for(int i=0; i < listRegExpMatches.size(); i++)
        {
            if(listRegExpMatches.at(i).size() == 1)
            {   // BYTE VARIABLE

                // set variable for muParser
                uint byteNum = uint(listRegExpMatches.at(i).at(0).toAscii())-65;
                listVars.append(double(parseMsg.dataBytesSansPrefix.at(byteNum)));

                try
                {
                    std::string varName = listRegExpMatches[i].toStdString();
                    m_parser.DefineVar(varName,&(listVars.last()));
                }

                catch(mu::Parser::exception_type &e)
                {
                  std::cerr << "Message:  " << e.GetMsg() << "\n";
                  std::cerr << "Formula:  " << e.GetExpr() << "\n";
                  std::cerr << "Token:    " << e.GetToken() << "\n";
                  std::cerr << "Position: " << e.GetPos() << "\n";
                  std::cerr << "Errc:     " << e.GetCode() << "\n";
                }
            }
            else
            {   // BIT VARIABLE

                // set variable for muParser
                uint byteNum = uint(listRegExpMatches.at(i).at(0).toAscii())-65;
                uint byteVal = uint(parseMsg.dataBytesSansPrefix.at(byteNum));
                int bitNum = listRegExpMatches.at(i).at(1).digitValue();
                int bitMask = int(pow(2,bitNum));
                listVars.append(double(byteVal & bitMask));

                try
                {
                    std::string varName = listRegExpMatches[i].toStdString();
                    m_parser.DefineVar(varName,&(listVars.last()));
                }
                catch(mu::Parser::exception_type &e)
                {
                  std::cerr << "Message:  " << e.GetMsg() << "\n";
                  std::cerr << "Formula:  " << e.GetExpr() << "\n";
                  std::cerr << "Token:    " << e.GetToken() << "\n";
                  std::cerr << "Position: " << e.GetPos() << "\n";
                  std::cerr << "Errc:     " << e.GetCode() << "\n";
                }
            }
        }

        sampleExpr.replace(rx1,"POOP");
        std::cerr << sampleExpr.toStdString() << std::endl;


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
            QString pExpr(parseChild.attribute("expr").value());
            if(!pExpr.isEmpty())
            {
                ParseInfo parseInfo;
                parseInfo.expr = pExpr;

                QString pExprTrue(parseChild.attribute("true").value());
                QString pExprFalse(parseChild.attribute("false").value());
                if(pExprTrue.isEmpty() && pExprFalse.isEmpty())
                {   // assume value is numerical
                    parseInfo.isNumerical = true;
                    if(parseChild.attribute("min"))
                    {
                        parseInfo.hasMin = true;
                        parseInfo.valueMin = parseChild.attribute("min").as_double();
                    }
                    if(parseChild.attribute("max"))
                    {
                        parseInfo.hasMax = true;
                        parseInfo.valueMax = parseChild.attribute("max").as_double();
                    }
                }
                else
                {   // assume value is a boolean flag
                    parseInfo.isFlag = true;
                    parseInfo.valueIfTrue = pExprTrue;
                    parseInfo.valueIfFalse = pExprFalse;
                }

                // save parse info (with conditions!)
                parseInfo.listConditions = listConditionExprs;
                requestMsg.listParseInfo.append(parseInfo);
            }

//            #ifdef OBDREF_DEBUG_ON
//            std::cerr << "OBDREF_DEBUG: Conditions: ";
//            for(int i=0; i < listConditionExprs.size(); i++)
//            {   std::cerr << listConditionExprs.at(i).toStdString() << ", ";   }
//            std::cerr << " Parse Expr: " << parseChild.attribute("expr").value();
//            std::cerr << std::endl;
//            #endif
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

    uint Parser::stringToUInt(bool &convOk, const QString &parseStr)
    {
        // expect ONE token representing a number
        // if str contains '0bXXXXX' -> parse as binary
        // if str contains '0xXXXXX' -> parse as hex
        // else parse as dec

        bool conv;
        QString myString(parseStr);

        if(myString.contains("0b"))
        {
            myString.remove("0b");
            uint myVal = myString.toUInt(&conv,2);
            if(conv)
            {
                convOk = conv;
                return myVal;
            }
            else
            {
                convOk = false;
                return 0;
            }
        }

        else if(myString.contains("0x"))
        {
            myString.remove("0x");
            uint myVal = myString.toUInt(&conv,16);
            if(conv)
            {
                convOk = conv;
                return myVal;
            }
            else
            {
                convOk = false;
                return 0;
            }
        }

        else
        {
            uint myVal = myString.toUInt(&conv,10);
            if(conv)
            {
                convOk = conv;
                return myVal;
            }
            else
            {
                convOk = false;
                return 0;
            }
        }
    }
}
