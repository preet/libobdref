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

//        // setup additional parser function
//        m_parser.DefineInfixOprt("!",muLogicalNot);
//        m_parser.DefineInfixOprt("~",muBitwiseNot);
//        m_parser.DefineOprt("&",muBitwiseAnd,3);
//        m_parser.DefineOprt("|",muBitwiseOr,3);

//        // setup parser variables
//        for(uint i=0; i < 26; i++)
//        {
//            // define variables [A-Z] in muParser
//            QString byteStr(QChar::fromAscii(char(65+i)));
//            m_parser.DefineVar(byteStr.toStdString(),
//                               &m_listBytesAZ[i]);

//            for(uint j=0; j < 8; j++)
//            {
//                QString bitStr = byteStr + (QString::number(j,10));
//                m_parser.DefineVar(bitStr.toStdString(),
//                                   &m_listBytesAZBits[i].bits[j]);
//            }
//        }
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
                                        qDebug() << "Response" << n << "Prefix: " << listRespPrefix;
                                        for(int i=0; i < listRespPrefix.size(); i++)
                                        {
                                            uint dataByte = stringToUInt(convOk,listRespPrefix.at(i));
                                            myMsgData.expDataPrefix.append(char(dataByte));
                                        }

                                        if(nodeParameter.attribute(respBytesAttrStr.c_str()))   // [responseN.bytes]
                                        {
                                            QString respBytes(nodeParameter.attribute(respBytesAttrStr.c_str()).value());
                                            qDebug() << "Response" << n << "Bytes: " <<  respBytes;
                                            myMsgData.expDataBytes = respBytes;
                                        }

                                        if(nodeParameter.attribute(reqDelayAttrStr.c_str()))    // [requestN.delay]
                                        {
                                            QString reqDelay(nodeParameter.attribute(reqDelayAttrStr.c_str()).value());
                                            qDebug() << "Request" << n << "Delay: " <<  reqDelay;
                                            myMsgData.reqDataDelayMs = stringToUInt(convOk,reqDelay);
                                        }

                                        if(nodeParameter.attribute(reqAttrStr.c_str()))         // [requestN]
                                        {
                                            QString reqData(nodeParameter.attribute(reqAttrStr.c_str()).value());
                                            QStringList listReqData = reqData.split(" ");
                                            qDebug() << "Request" << n << "Data: " <<  listReqData;
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

    bool Parser::ParseMessageFrame(const MessageFrame &parseMsg, Data &paramData)
    {

    }

    bool Parser::ParseMessage(const Message &parseMsg, QString &jsonStr)
    {
//        // if this message has no parse information, return the raw
//        // databytes in the json string
//        if(parseMsg.listParseInfo.size() == 0)
//        {
//        }


//        QRegExp rx1; int pos=0; double fVal;
//        QString sampleExpr = jsonStr;

//        // use regexp matching to find all instances of [A-Z],[A-Z][0-7]
//        rx1.setPattern("([A-Z]|[A-Z][0-7])+");
//        QStringList listRegExpMatches;
//        while ((pos = rx1.indexIn(sampleExpr, pos)) != -1)
//        {
//             listRegExpMatches << rx1.cap(1);
//             pos += rx1.matchedLength();
//        }

//        // set variables for muParser
//        for(int i=0; i < listRegExpMatches.size(); i++)
//        {
//            if(listRegExpMatches.at(i).size() == 1)
//            {   // BYTE VARIABLE
//                uint byteNum = uint(listRegExpMatches.at(i).at(0).toAscii())-65;
//                uint byteVal = uint(parseMsg.dataBytesSansPrefix.at(byteNum));
//                m_listBytesAZ[byteNum] = double(byteVal);
//            }
//            else
//            {   // BIT VARIABLE
//                uint byteNum = uint(listRegExpMatches.at(i).at(0).toAscii())-65;
//                uint byteVal = uint(parseMsg.dataBytesSansPrefix.at(byteNum));
//                int bitNum = listRegExpMatches.at(i).at(1).digitValue();
//                int bitMask = m_listDecValOfBitPos[bitNum];
//                m_listBytesAZBits[byteNum].bits[bitNum] = double(byteVal & bitMask);
//            }
//        }

//        // eval with muParser
//        try
//        {
//            m_parser.SetExpr(sampleExpr.toStdString());
//            fVal = m_parser.Eval();
//        }
//        catch(mu::Parser::exception_type &e)
//        {
//            std::cerr << "OBDREF_DEBUG: muParser: Message:  " << e.GetMsg() << "\n";
//            std::cerr << "OBDREF_DEBUG: muParser: Formula:  " << e.GetExpr() << "\n";
//            std::cerr << "OBDREF_DEBUG: muParser: Token:    " << e.GetToken() << "\n";
//            std::cerr << "OBDREF_DEBUG: muParser: Position: " << e.GetPos() << "\n";
//            std::cerr << "OBDREF_DEBUG: muParser: ErrC:     " << e.GetCode() << "\n";
//        }

//        qDebug() << "VALUE: " << fVal;
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

//    mu::value_type Parser::muLogicalNot(mu::value_type fVal)
//    {
//        std::cerr << "Called LogicalNot" << std::endl;

//        if(fVal == 0)
//        {   return 1;   }

//        else if(fVal == 1)
//        {   return 0;   }

//        else
//        {   throw mu::ParserError("LogicalNot: Argument is not 0 or 1");   }
//    }

//    mu::value_type Parser::muBitwiseNot(mu::value_type fVal)
//    {
//        std::cerr << "Called BitwiseNot" << std::endl;

//        int iVal = (int)fVal;
//        iVal = ~iVal;
//        return mu::value_type(fVal);
//    }

//    mu::value_type Parser::muBitwiseAnd(mu::value_type fVal1, mu::value_type fVal2)
//    {
//        std::cerr << "Called BitwiseAnd" << std::endl;

//        int iVal1 = (int)fVal1;
//        int iVal2 = (int)fVal2;
//        return double(iVal1 & iVal2);
//    }

//    mu::value_type Parser::muBitwiseOr(mu::value_type fVal1, mu::value_type fVal2)
//    {
//        std::cerr << "Called BitwiseOr" << std::endl;

//        int iVal1 = (int)fVal1;
//        int iVal2 = (int)fVal2;
//        return double(iVal1 | iVal2);
//    }

    // ========================================================================== //
    // ========================================================================== //
}
