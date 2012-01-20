#include "parser.h"

namespace obdref
{
    // ========================================================================== //
    // ========================================================================== //

    Parser::Parser(QString const &filePath, bool &parsedOk)
    {
        // convenience values
        m_listDecValOfBitPos[0] = 1;
        m_listDecValOfBitPos[1] = 2;
        m_listDecValOfBitPos[2] = 4;
        m_listDecValOfBitPos[3] = 8;
        m_listDecValOfBitPos[4] = 16;
        m_listDecValOfBitPos[5] = 32;
        m_listDecValOfBitPos[6] = 64;
        m_listDecValOfBitPos[7] = 128;

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

        // setup parser
        m_parser.DefineInfixOprt("!",muLogicalNot);
        m_parser.DefineInfixOprt("~",muBitwiseNot);
        m_parser.DefineOprt("&",muBitwiseAnd,3);
        m_parser.DefineOprt("|",muBitwiseOr,3);

        for(int i=0; i < MAX_EXPR_VARS; i++)
        {  m_listExprVars[i] = 0;   }

        // TODO i dont know if adding ".", "[", "]" as
        // variable characters is appropriate -- need to double check
        m_parser.DefineNameChars("0123456789_.[]"
                                 "abcdefghijklmnopqrstuvwxyz"
                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::BuildMessageFrame(const QString &specName, const QString &protocolName,
                                   const QString &addressName, const QString &paramName,
                                   MessageFrame &msgFrame)
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

                                            msgFrame.reqHeaderBytes.append(char(upperByte));
                                            msgFrame.reqHeaderBytes.append(char(lowerByte));
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

                                            msgFrame.expHeaderBytes.append(char(upperByte));
                                            msgFrame.expHeaderBytes.append(char(lowerByte));
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
                                        msgFrame.reqHeaderBytes.append(char(prioByte));

                                        uint formatByte = stringToUInt(convOk,headerFormat);
                                        msgFrame.reqHeaderBytes.append(char(formatByte));

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        msgFrame.reqHeaderBytes.append(char(targetByte));

                                        uint sourceByte = stringToUInt(convOk,headerSource);
                                        msgFrame.reqHeaderBytes.append(char(sourceByte));
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
                                        msgFrame.expHeaderBytes.append(char(prioByte));

                                        uint formatByte = stringToUInt(convOk,headerFormat);
                                        msgFrame.expHeaderBytes.append(char(formatByte));

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        msgFrame.expHeaderBytes.append(char(targetByte));

                                        if(!headerSource.isEmpty())
                                        {
                                            uint sourceByte = stringToUInt(convOk,headerSource);
                                            msgFrame.expHeaderBytes.append(char(sourceByte));
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
                                        msgFrame.reqHeaderBytes.append(char(prioByte));

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        msgFrame.reqHeaderBytes.append(char(targetByte));

                                        uint sourceByte = stringToUInt(convOk,headerSource);
                                        msgFrame.reqHeaderBytes.append(char(sourceByte));
                                    }

                                    pugi::xml_node nodeResp = nodeAddress.child("response");
                                    if(nodeResp)
                                    {
                                        QString headerPrio(nodeResp.attribute("prio").value());
                                        QString headerTarget(nodeResp.attribute("target").value());
                                        QString headerSource(nodeResp.attribute("source").value());

                                        bool convOk = false;
                                        uint prioByte = stringToUInt(convOk,headerPrio);
                                        msgFrame.expHeaderBytes.append(char(prioByte));

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        msgFrame.expHeaderBytes.append(char(targetByte));

                                        if(!headerSource.isEmpty())
                                        {
                                            uint sourceByte = stringToUInt(convOk,headerSource);
                                            msgFrame.expHeaderBytes.append(char(sourceByte));
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
                                // TODO ISO14230 special case
                                bool convOk = false;
                                int n=0; int t=0;

                                // we can have multiple request/response data stored
                                // within a single parameter, marked as follows:
                                // requestN="" responseN.prefix="" responseN.bytes="",
                                // where 'N' is some integer from 1-inf

                                // singular request/response (the more common case)
                                // should be indicated by leaving out numbers:
                                // request="" response.prefix="" response.bytes=""

                                // note we use 'responseN.prefix' to determine how
                                // many messages we care about for this frame, because
                                // responseN.prefix is *required* per message, whereas
                                // requestN* is not (ie monitored messages)

                                while(1)
                                {
                                    QString respPrefixName, respBytesName, reqName, reqDelayName;
                                    if(n == 0)
                                    {
                                        respPrefixName = QString("response.prefix");
                                        respBytesName = QString("response.bytes");
                                        reqDelayName = QString("request.delay");
                                        reqName = QString("request");
                                    }
                                    else
                                    {
                                        respPrefixName = QString("response")+QString::number(n,10)+QString(".prefix");
                                        respBytesName = QString("response")+QString::number(n,10)+QString(".bytes");
                                        reqDelayName = QString("request")+QString::number(n,10)+QString(".delay");
                                        reqName = QString("request")+QString::number(n,10);
                                    }

                                    std::string respPrefixAttrStr = respPrefixName.toStdString();
                                    std::string respBytesAttrStr = respBytesName.toStdString();
                                    std::string reqDelayAttrStr = reqDelayName.toStdString();
                                    std::string reqAttrStr = reqName.toStdString();

                                    if(nodeParameter.attribute(respPrefixAttrStr.c_str()))      // [responseN.prefix]
                                    {
                                        MessageData myMsgData;

                                        QString respPrefix(nodeParameter.attribute(respPrefixAttrStr.c_str()).value());
                                        QStringList listRespPrefix = respPrefix.split(" ");
                                        for(int i=0; i < listRespPrefix.size(); i++)
                                        {
                                            uint dataByte = stringToUInt(convOk,listRespPrefix.at(i));
                                            myMsgData.expDataPrefix.append(char(dataByte));
                                        }

                                        if(nodeParameter.attribute(respBytesAttrStr.c_str()))   // [responseN.bytes]
                                        {
                                            QString respBytes(nodeParameter.attribute(respBytesAttrStr.c_str()).value());
                                            myMsgData.expDataBytes = respBytes;
                                        }

                                        if(nodeParameter.attribute(reqDelayAttrStr.c_str()))    // [requestN.delay]
                                        {
                                            QString reqDelay(nodeParameter.attribute(reqDelayAttrStr.c_str()).value());
                                            myMsgData.reqDataDelayMs = stringToUInt(convOk,reqDelay);
                                        }

                                        if(nodeParameter.attribute(reqAttrStr.c_str()))         // [requestN]
                                        {
                                            QString reqData(nodeParameter.attribute(reqAttrStr.c_str()).value());
                                            QStringList listReqData = reqData.split(" ");
                                            for(int i=0; i < listReqData.size(); i++)
                                            {
                                                uint dataByte = stringToUInt(convOk,listReqData.at(i));
                                                myMsgData.reqDataBytes.append(char(dataByte));
                                            }
                                        }

                                        msgFrame.listMessageData.append(myMsgData);
                                    }
                                    else
                                    {   t++;  if(t > 1) { break; }   }

                                    n++;
                                }

                                // save all <parse /> children nodes this parameter has
                                pugi::xml_node parseChild = nodeParameter.child("parse");
                                for(parseChild; parseChild; parseChild = parseChild.next_sibling("parse"))
                                {
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
                                            {   parseInfo.numericalData.min = parseChild.attribute("min").as_double();   }

                                            if(parseChild.attribute("max"))
                                            {   parseInfo.numericalData.max = parseChild.attribute("max").as_double();   }
                                        }
                                        else
                                        {   // assume value is literal
                                            parseInfo.isLiteral = true;
                                            parseInfo.literalData.valueIfFalse = pExprFalse;
                                            parseInfo.literalData.valueIfTrue = pExprTrue;
                                        }

                                        msgFrame.listParseInfo.append(parseInfo);
                                    }
                                }

                                // call walkConditionTree on <condition> children this parameter has
                                QStringList listConditionExprs;
                                pugi::xml_node condChild = nodeParameter.child("condition");
                                for(condChild; condChild; condChild = condChild.next_sibling("condition"))
                                {   walkConditionTree(condChild, listConditionExprs, msgFrame);   }

                                #ifdef OBDREF_DEBUG_ON
                                std::cerr << "OBDREF_DEBUG: " << specName.toStdString() << ", "
                                          << protocolName.toStdString() << ", " << addressName.toStdString()
                                          << ", " << paramName.toStdString() << ":" << std::endl;

                                for(int i=0; i < msgFrame.listParseInfo.size(); i++)
                                {
                                    std::cerr << "OBDREF_DEBUG: \t";
                                    for(int j=0; j < msgFrame.listParseInfo[i].listConditions.size(); j++)
                                    {   std::cerr << " { " << msgFrame.listParseInfo[i].listConditions[j].toStdString() << " } | ";   }
                                    std::cerr << "Eval: { " << msgFrame.listParseInfo[i].expr.toStdString() << " }" << std::endl;
                                }
                                #endif

                                // save spec, protocol, address and param info
                                msgFrame.spec = specName;
                                msgFrame.protocol = protocolName;
                                msgFrame.address = addressName;
                                msgFrame.name = paramName;

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

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::ParseMessageFrame(const MessageFrame &msgFrame, Data &paramData)
    {
        if(msgFrame.listParseInfo.size() == 0)
        {
            #ifdef OBDREF_DEBUG_ON
            std::cerr << "OBDREF_DEBUG: Error: No parsing information "
                      << "found in message frame." << std::endl;
            #endif
            return false;
        }

        if(msgFrame.listMessageData.size() < 1)
        {
            #ifdef OBDREF_DEBUG_ON
            std::cerr << "OBDREF_DEBUG: Error: No messages found "
                      << "found in message frame." << std::endl;
            #endif
            return false;
        }

        for(int i=0; i < msgFrame.listParseInfo.size(); i++)
        {
            double myResult = 0;
            bool parsedOk = false;
            bool allConditionsValid = true;
            for(int j=0; j < msgFrame.listParseInfo[i].listConditions.size(); j++)
            {
                parsedOk = parseMessage(msgFrame,msgFrame.listParseInfo[i].listConditions.at(j),myResult);
                if(!parsedOk)
                {   // error in condition expression
                    #ifdef OBDREF_DEBUG_ON
                    std::cerr << "OBDREF_DEBUG: Error: Could not interpret "
                              << "condition expression." << std::endl;
                    #endif
                    return false;
                }
                if(myResult == 0)
                {   // condition failed, ignore this parse expr
                    allConditionsValid = false;
                    break;
                }
            }

            if(allConditionsValid)
            {
                parsedOk = parseMessage(msgFrame, msgFrame.listParseInfo.at(i).expr, myResult);
                if(!parsedOk)
                {   // error in parse expression
                    #ifdef OBDREF_DEBUG_ON
                    std::cerr << "OBDREF_DEBUG: Error: Could not calculate "
                              << "parameter expression." << std::endl;
                    #endif
                    return false;
                }

                // save result
                if(msgFrame.listParseInfo.at(i).isNumerical)
                {
                    // TODO should we throw out value if not within min/max?
                    paramData.listNumericalData.append(msgFrame.listParseInfo.at(i).numericalData);
                    paramData.listNumericalData.last().value = myResult;
                    #ifdef OBDREF_DEBUG_ON
                    std::cerr << "OBDREF_DEBUG: Numerical Data: " << myResult << " "
                              << paramData.listNumericalData.last().units.toStdString()
                              << std::endl;
                    #endif
                }
                else if(msgFrame.listParseInfo.at(i).isLiteral)
                {
                    // TODO throw out value if not 0 or 1
                    paramData.listLiteralData.append(msgFrame.listParseInfo.at(i).literalData);
                    paramData.listLiteralData.last().value = myResult;
                    #ifdef OBDREF_DEBUG_ON
                    if(paramData.listLiteralData.last().value)
                    {
                        std::cerr << "OBDREF_DEBUG: Literal Data: " << myResult << " "
                                  << paramData.listLiteralData.last().valueIfTrue.toStdString()
                                  << std::endl;
                    }
                    else
                    {
                        std::cerr << "OBDREF_DEBUG: Literal Data: " << myResult << " "
                                  << paramData.listLiteralData.last().valueIfFalse.toStdString()
                                  << std::endl;
                    }
                    #endif
                }
            }
        }
        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    void Parser::walkConditionTree(pugi::xml_node &nodeCondition,
                                   QStringList &listConditionExprs,
                                   MessageFrame &msgFrame)
    {
        // add this condition expr to list
        QString conditionExpr(nodeCondition.attribute("expr").value());
        listConditionExprs.push_back(conditionExpr);

        // save <parse /> children nodes this condition has
        pugi::xml_node parseChild = nodeCondition.child("parse");
        for(parseChild; parseChild; parseChild = parseChild.next_sibling("parse"))
        {
            QString pExpr(parseChild.attribute("expr").value());
            if(!pExpr.isEmpty())
            {
                ParseInfo parseInfo;
                parseInfo.expr = pExpr;

                QString pExprTrue(parseChild.attribute("true").value());
                QString pExprFalse(parseChild.attribute("false").value());
                if(pExprTrue.isEmpty() && pExprFalse.isEmpty())
                {   // assume value is numerical
                    if(parseChild.attribute("min"))
                    {   parseInfo.numericalData.min = parseChild.attribute("min").as_double();   }

                    if(parseChild.attribute("max"))
                    {   parseInfo.numericalData.max = parseChild.attribute("max").as_double();   }
                }
                else
                {   // assume value is literal
                    parseInfo.literalData.valueIfFalse = pExprFalse;
                    parseInfo.literalData.valueIfTrue = pExprTrue;
                }

                // save parse info (with conditions!)
                parseInfo.listConditions = listConditionExprs;
                msgFrame.listParseInfo.append(parseInfo);
            }
        }

        // call walkConditionTree recursively on
        // <condition> children this condition has
        pugi::xml_node condChild = nodeCondition.child("condition");
        for(condChild; condChild; condChild = condChild.next_sibling("condition"))
        {   walkConditionTree(condChild, listConditionExprs, msgFrame);    }

        // remove last condition expr added (since this is a DFS)
        listConditionExprs.removeLast();
    }

    // ========================================================================== //
    // ========================================================================== //

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
            {   convOk = conv;   return myVal;   }
            else
            {   convOk = false;   return 0;   }
        }
        else if(myString.contains("0x"))
        {
            myString.remove("0x");
            uint myVal = myString.toUInt(&conv,16);
            if(conv)
            {   convOk = conv;   return myVal;   }
            else
            {   convOk = false;   return 0;   }
        }
        else
        {
            uint myVal = myString.toUInt(&conv,10);
            if(conv)
            {   convOk = conv;   return myVal;   }
            else
            {   convOk = false;   return 0;   }
        }
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::parseMessage(const MessageFrame &msgFrame,
                              const QString &parseExpr,
                              double &myResult)
    {
        // there are two distinct ways of representing data
        // bytes within an expression:

        // 1 -  byte letters with a 'respN' prefix, meant for
        //      differentiating between different response messages

        // 2 -  byte letters without a prefix, meant for most
        //      parameters, which have a single response message

        // we allow the expression to be style (1) even if there
        // is only one message, in case the user wants to adhere
        // to an explicit data byte identifier

        // however an expression with style (2) in a message frame
        // with multiple messages is *not* allowed since it
        // creates ambiguity

        bool convOk = false;

        if(parseExpr.contains("resp"))
        {
            QRegExp rx; int pos=0; double fVal = 0;
            rx.setPattern("(resp[1-99]\\.[A-Z]|resp[1-99]\\.[A-Z]\\[[0-7]\\])+");

            QStringList listRegExpMatches;
            while ((pos = rx.indexIn(parseExpr, pos)) != -1)
            {
                listRegExpMatches << rx.cap(1);
                pos += rx.matchedLength();
            }

            // map the data required to calc expression
            for(int i=0; i < listRegExpMatches.size(); i++)
            {
                uint msgNumDigits = (msgFrame.listMessageData.size() > 9) ? 2 : 1;
                QString msgNumChars(listRegExpMatches.at(i).mid(4,msgNumDigits));
                uint msgNum = stringToUInt(convOk,msgNumChars) - 1;

                if(msgFrame.listMessageData.size() <= msgNum)
                {
                    #ifdef OBDREF_DEBUG_ON
                    std::cerr << "OBDREF_DEBUG: Error: Expression message "
                              << "number prefix out of range: "
                              << parseExpr.toStdString() << std::endl;
                    #endif
                    return false;
                }

                int byteCharPos = listRegExpMatches.at(i).indexOf(".") + 1;
                uint byteNum = uint(listRegExpMatches.at(i).at(byteCharPos).toAscii())-65;

                if(!(byteNum < msgFrame.listMessageData[msgNum].dataBytes.size()))
                {
                    #ifdef OBDREF_DEBUG_ON
                    std::cerr << "OBDREF_DEBUG: Error: Data for " << msgFrame.name.toStdString()
                              << " has insufficient bytes for parsing: "
                              << parseExpr.toStdString() << std::endl;
                    #endif
                    return false;
                }

                uint byteVal = uint(msgFrame.listMessageData[msgNum].dataBytes.at(byteNum));

                if(listRegExpMatches.at(i).contains("["))
                {   // BIT VARIABLE

                    int bitCharPos = listRegExpMatches.at(i).indexOf("[") + 1;
                    int bitNum = listRegExpMatches.at(i).at(bitCharPos).digitValue();
                    int bitMask = m_listDecValOfBitPos[bitNum];

                    m_listExprVars[i] = double(byteVal & bitMask);
                    m_parser.DefineVar(listRegExpMatches.at(i).toStdString(),
                                       &m_listExprVars[i]);
                }
                else
                {   // BYTE VARIABLE

                    m_listExprVars[i] = double(byteVal);
                    m_parser.DefineVar(listRegExpMatches.at(i).toStdString(),
                                       &m_listExprVars[i]);
                }
            }
        }
        else
        {
            // single message variables follow the convention:
            // bytes: [A-Z], bits: A[0-7]

            if(msgFrame.listMessageData.size() > 1)
            {
                #ifdef OBDREF_DEBUG_ON
                std::cerr << "OBDREF_DEBUG: Error: Multiple messages in frame "
                          << "but expr missing message number prefixes: "
                          << parseExpr.toStdString() << std::endl;
                #endif
                return false;
            }

            QRegExp rx; int pos=0; double fVal = 0;
            rx.setPattern("([A-Z]|[A-Z]\\[[0-7]\\])+");

            QStringList listRegExpMatches;
            while ((pos = rx.indexIn(parseExpr, pos)) != -1)
            {
                listRegExpMatches << rx.cap(1);
                pos += rx.matchedLength();
            }

            // map the data required to calc expression
            for(int i=0; i < listRegExpMatches.size(); i++)
            {
                uint byteNum = uint(listRegExpMatches.at(i).at(0).toAscii())-65;
                if(!(byteNum < msgFrame.listMessageData[0].dataBytes.size()))
                {
                    #ifdef OBDREF_DEBUG_ON
                    std::cerr << "OBDREF_DEBUG: Error: Data for " << msgFrame.name.toStdString()
                              << " has insufficient bytes for parsing: "
                              << parseExpr.toStdString() << std::endl;
                    #endif
                    return false;
                }

                uint byteVal = uint(msgFrame.listMessageData[0].dataBytes.at(byteNum));

                if(listRegExpMatches.at(i).contains("["))
                {   // BIT VARIABLE

                    int bitCharPos = listRegExpMatches.at(i).indexOf("[") + 1;
                    int bitNum = listRegExpMatches.at(i).at(bitCharPos).digitValue();
                    int bitMask = m_listDecValOfBitPos[bitNum];

                    m_listExprVars[i] = double(byteVal & bitMask);
                    m_parser.DefineVar(listRegExpMatches.at(i).toStdString(),
                                       &m_listExprVars[i]);
                }
                else
                {   // BYTE VARIABLE

                    m_listExprVars[i] = double(byteVal);
                    m_parser.DefineVar(listRegExpMatches.at(i).toStdString(),
                                       &m_listExprVars[i]);
                }
            }
        }

        // eval with muParser
        try
        {
            m_parser.SetExpr(parseExpr.toStdString());
            myResult = m_parser.Eval();
        }
        catch(mu::Parser::exception_type &e)
        {
            #ifdef OBDREF_DEBUG_ON
            std::cerr << "OBDREF_DEBUG: muParser: Message: " << e.GetMsg() << "\n";
            std::cerr << "OBDREF_DEBUG: muParser: Formula: " << e.GetExpr() << "\n";
            std::cerr << "OBDREF_DEBUG: muParser: Token: " << e.GetToken() << "\n";
            std::cerr << "OBDREF_DEBUG: muParser: Position: " << e.GetPos() << "\n";
            std::cerr << "OBDREF_DEBUG: muParser: ErrC: " << e.GetCode() << "\n";
            #endif
        }

        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    mu::value_type Parser::muLogicalNot(mu::value_type fVal)
    {
        std::cerr << "Called LogicalNot" << std::endl;

        if(fVal == 0)
        {   return 1;   }

        else if(fVal == 1)
        {   return 0;   }

        else
        {   throw mu::ParserError("LogicalNot: Argument is not 0 or 1");   }
    }

    mu::value_type Parser::muBitwiseNot(mu::value_type fVal)
    {
        std::cerr << "Called BitwiseNot" << std::endl;

        int iVal = (int)fVal;
        iVal = ~iVal;
        return mu::value_type(fVal);
    }

    mu::value_type Parser::muBitwiseAnd(mu::value_type fVal1, mu::value_type fVal2)
    {
        std::cerr << "Called BitwiseAnd" << std::endl;

        int iVal1 = (int)fVal1;
        int iVal2 = (int)fVal2;
        return double(iVal1 & iVal2);
    }

    mu::value_type Parser::muBitwiseOr(mu::value_type fVal1, mu::value_type fVal2)
    {
        std::cerr << "Called BitwiseOr" << std::endl;

        int iVal1 = (int)fVal1;
        int iVal2 = (int)fVal2;
        return double(iVal1 | iVal2);
    }

    // ========================================================================== //
    // ========================================================================== //
}
