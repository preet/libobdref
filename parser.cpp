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

#include "parser.h"

namespace obdref
{
    // ========================================================================== //
    // ========================================================================== //

    Parser::Parser(QString const &filePath, bool &parsedOk)
    {
        // error logging
        m_lkErrors.setString(&m_lkErrorString, QIODevice::ReadWrite);

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
        {
            OBDREFDEBUG << "XML [" << filePath << "] errors!\n";

            OBDREFDEBUG << "OBDREF: Error Desc: "
                        << QString::fromStdString(xmlParseResult.description()) << "\n";

            OBDREFDEBUG << "OBDREF: Error Offset Char: "
                        << qint64(xmlParseResult.offset) << "\n";

            parsedOk = false;
        }

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

    QStringList Parser::GetParameterNames(const QString &specName,
                                          const QString &protocolName,
                                          const QString &addressName)
    {
        bool foundSpec = false;
        bool foundProtocol = false;
        bool foundAddress = false;
        bool foundParams = false;
        QStringList myParamList;

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
                            }
                        }
                    }
                }

                if(!foundAddress)
                {   break;   }

                pugi::xml_node nodeParams = nodeSpec.child("parameters");
                for(nodeParams; nodeParams; nodeParams = nodeProtocol.next_sibling("parameters"))
                {
                    QString fileParamsName(nodeParams.attribute("address").value());
                    if(fileParamsName == addressName)
                    {
                        foundParams = true;
                        pugi::xml_node nodeParameter = nodeParams.child("parameter");
                        for(nodeParameter; nodeParameter; nodeParameter = nodeParameter.next_sibling("parameter"))
                        {
                            QString fileParameterName(nodeParameter.attribute("name").value());
                            myParamList << fileParameterName;
                        }
                    }
                }
            }
        }

        return myParamList;
    }

    bool Parser::BuildMessageFrame(const QString &specName, const QString &protocolName,
                                   const QString &addressName, const QString &paramName,
                                   MessageFrame &msgFrame)
    {
        bool foundSpec = false;
        bool foundProtocol = false;
        bool foundAddress = false;
        bool foundParams = false;
        bool foundParameter = false;

        // save spec, protocol, address and param info
        msgFrame.spec = specName;
        msgFrame.protocol = protocolName;
        msgFrame.address = addressName;
        msgFrame.name = paramName;

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

                if(!foundAddress)
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

                                if(msgFrame.listMessageData.size() == 0)
                                {
                                    OBDREFDEBUG << "OBDREF: Error: Could not build any "
                                                << "messages from given xml data (does the "
                                                << "file have a typo?) \n";
                                    return false;
                                }

                                // save all <parse /> children nodes this parameter has
                                pugi::xml_node parseChild = nodeParameter.child("parse");
                                for(parseChild; parseChild; parseChild = parseChild.next_sibling("parse"))
                                {
                                    QString pExpr(parseChild.attribute("expr").value());
                                    if(!pExpr.isEmpty())
                                    {
                                        // convert hex and binary notation
                                        // to decimal for muParser
                                        convToDecEquivExpression(pExpr);

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

                                            if(parseChild.attribute("units"))
                                            {   parseInfo.numericalData.units = QString(parseChild.attribute("units").value());   }

                                            if(parseChild.attribute("desc"))
                                            {   parseInfo.numericalData.desc = QString(parseChild.attribute("desc").value());   }
                                        }
                                        else
                                        {   // assume value is literal
                                            parseInfo.isLiteral = true;
                                            parseInfo.literalData.valueIfFalse = pExprFalse;
                                            parseInfo.literalData.valueIfTrue = pExprTrue;

                                            if(parseChild.attribute("property"))
                                            {   parseInfo.literalData.property = QString(parseChild.attribute("property").value());   }
                                            else
                                            {   parseInfo.literalData.property = paramName;   }

                                            if(parseChild.attribute("desc"))
                                            {   parseInfo.literalData.desc = QString(parseChild.attribute("desc").value());   }
                                        }
                                        msgFrame.listParseInfo.append(parseInfo);
                                    }
                                }

                                // call walkConditionTree on <condition> children this parameter has
                                QStringList listConditionExprs;
                                pugi::xml_node condChild = nodeParameter.child("condition");
                                for(condChild; condChild; condChild = condChild.next_sibling("condition"))
                                {   walkConditionTree(condChild, listConditionExprs, msgFrame);   }

                                return true;
                            }
                        }
                    }
                }
            }
        }

        if(!foundSpec)
        {
            OBDREFDEBUG << "OBDREF: Error: Could not find spec "
                        << specName << "\n";
        }
        else if(!foundProtocol)
        {
            OBDREFDEBUG << "OBDREF: Error: Could not find protocol "
                      << protocolName << "\n";
        }
        else if(!foundAddress)
        {
            OBDREFDEBUG << "OBDREF: Error: Could not find address "
                      << addressName << "\n";
        }
        else if(!foundParams)
        {
            OBDREFDEBUG << "OBDREF: Error: Could not find group "
                      << addressName << "\n";
        }
        else if(!foundParameter)
        {
            OBDREFDEBUG << "OBDREF: Error: Could not find parameter "
                      << paramName << "\n";
        }

        return false;
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::ParseMessageFrame(const MessageFrame &msgFrame, Data &paramData)
    {
        if(msgFrame.listParseInfo.size() == 0)
        {
            OBDREFDEBUG << "OBDREF: Error: No parsing info "
                        << "found in message frame\n";
            return false;
        }

        if(msgFrame.listMessageData.size() < 1)
        {
            OBDREFDEBUG << "OBDREF: Error: No messages "
                        << "found in message frame\n";
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
                    OBDREFDEBUG << "OBDREF: Error: Could not interpret "
                                << "condition expression: "
                                << msgFrame.listParseInfo[i].listConditions.at(j) << "\n";
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
                    OBDREFDEBUG << "OBDREF: Error: Could not solve "
                                << "parameter expression"
                                << msgFrame.listParseInfo.at(i).expr << "\n";
                    return false;
                }

                // save result
                paramData.desc = msgFrame.name;
                if(msgFrame.listParseInfo.at(i).isNumerical)
                {
                    // TODO should we throw out value if not within min/max?
                    paramData.listNumericalData.append(msgFrame.listParseInfo.at(i).numericalData);
                    paramData.listNumericalData.last().value = myResult;
                }
                else if(msgFrame.listParseInfo.at(i).isLiteral)
                {
                    // throw out value if true/false value strings are empty
                    bool trueAndTrueStrEmpty = bool(myResult) && msgFrame.listParseInfo[i].literalData.valueIfTrue.isEmpty();
                    bool falseAndFalseStrEmpty = (bool(!myResult)) && msgFrame.listParseInfo[i].literalData.valueIfFalse.isEmpty();

                    if(trueAndTrueStrEmpty || falseAndFalseStrEmpty)
                    {
                        continue;
                    }
                    else
                    {
                        paramData.listLiteralData.append(msgFrame.listParseInfo.at(i).literalData);
                        paramData.listLiteralData.last().value = bool(myResult);
                    }
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
        convToDecEquivExpression(conditionExpr);
        listConditionExprs.push_back(conditionExpr);

        // save <parse /> children nodes this condition has
        pugi::xml_node parseChild = nodeCondition.child("parse");
        for(parseChild; parseChild; parseChild = parseChild.next_sibling("parse"))
        {
            QString pExpr(parseChild.attribute("expr").value());
            if(!pExpr.isEmpty())
            {
                // convert hex and binary notation
                // to decimal for muParser
                convToDecEquivExpression(pExpr);

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

                    if(parseChild.attribute("units"))
                    {   parseInfo.numericalData.units = QString(parseChild.attribute("units").value());   }

                    if(parseChild.attribute("data"))
                    {   parseInfo.numericalData.desc = QString(parseChild.attribute("data").value());   }
                }
                else
                {   // assume value is literal
                    parseInfo.isLiteral = true;
                    parseInfo.literalData.valueIfFalse = pExprFalse;
                    parseInfo.literalData.valueIfTrue = pExprTrue;

                    if(parseChild.attribute("property"))
                    {   parseInfo.literalData.property = QString(parseChild.attribute("property").value());   }
                    else
                    {   parseInfo.literalData.property = msgFrame.name;   }

                    if(parseChild.attribute("desc"))
                    {   parseInfo.literalData.desc = QString(parseChild.attribute("desc").value());   }                    
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

    QStringList Parser::GetLastKnownErrors()
    {
        QStringList listErrors;
        QString lineError;
        while(1)
        {
            lineError = m_lkErrors.readLine();
            if(lineError.isNull())
            {   break;   }

            listErrors << lineError;
        }

        return listErrors;
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

        // 1 -  byte [] with a 'respN' prefix, meant for
        //      differentiating between different response messages

        // 2 -  byte [] without a prefix, meant for most
        //      parameters, which have a single response message

        // we allow the expression to be style (1) even if there
        // is only one message, in case the user wants to adhere
        // to an explicit data message identifier

        // however an expression with style (2) in a message frame
        // with multiple messages is *not* allowed since it
        // creates ambiguity

        bool convOk = false;       

        // case 1
        if(parseExpr.contains("resp"))
        {
            // resp[1-999..][0-999..][0-999..]
            QRegExp rx; int pos=0; double fVal = 0;
            rx.setPattern("(resp[1-9][0-9]*\\[[0-9]+\\](\\[[0-7]\\])?)+");

            QStringList listRegExpMatches;
            while ((pos = rx.indexIn(parseExpr, pos)) != -1)
            {
                listRegExpMatches << rx.cap(1);
                pos += rx.matchedLength();
            }

            // map the data required to calc expression
            for(int i=0; i < listRegExpMatches.size(); i++)
            {
                // get message number
                int posByteStart = listRegExpMatches.at(i).indexOf("[") + 1;
                QString msgNumStr(listRegExpMatches.at(i).mid(4,posByteStart-1-4));
                uint msgNum = stringToUInt(convOk,msgNumStr) - 1;

                if(msgFrame.listMessageData.size() <= msgNum)
                {
                    OBDREFDEBUG << "OBDREF: Error: Expression message "
                                << "number prefix out of range: "
                                << parseExpr << "\n";
                    return false;
                }
                // get byte number
                int posByteEnd = listRegExpMatches.at(i).indexOf("]",posByteStart) - 1;
                int numByteDigits = (posByteEnd-posByteStart) + 1;
                QString byteNumStr = listRegExpMatches.at(i).mid(posByteStart,numByteDigits);
                uint byteNum = stringToUInt(convOk,byteNumStr);

                if(!(byteNum < msgFrame.listMessageData[msgNum].dataBytes.size()))
                {
                    OBDREFDEBUG << "OBDREF: Error: Data for " << msgFrame.name
                                << " has insufficient bytes for parsing: "
                                << parseExpr << "\n";
                    return false;
                }

                uint byteVal = uint(msgFrame.listMessageData[msgNum].dataBytes.at(byteNum));

                // check for bit number
                int posBitStart = listRegExpMatches.at(i).indexOf("[",posByteEnd) + 1;
                if(posBitStart > 0)
                {   // BIT VARIABLE

                    // get bit number
                    QString bitNumStr = listRegExpMatches.at(i).mid(posBitStart,1);
                    uint bitNum = stringToUInt(convOk,bitNumStr);
                    uint bitMask = m_listDecValOfBitPos[bitNum];

                    m_listExprVars[i] = double(byteVal & bitMask) / bitMask;
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
        // case 2
        else
        {
            // single message variables follow the convention:
            // bytes: [0-99], bits: [0-7]
            // example third bit of byte eight: [7][2]

            if(msgFrame.listMessageData.size() > 1)
            {
                OBDREFDEBUG << "OBDREF: Error: Multiple messages in frame "
                            << "but expr missing message number prefixes "
                            << parseExpr << "\n";
                return false;
            }

            QRegExp rx; int pos=0; double fVal = 0;
            rx.setPattern("(\\[[0-9]+\\](\\[[0-7]\\])?)+");

            QStringList listRegExpMatches;
            while ((pos = rx.indexIn(parseExpr, pos)) != -1)
            {
                listRegExpMatches << rx.cap(1);
                pos += rx.matchedLength();
            }

            // map the data required to calc expression
            for(int i=0; i < listRegExpMatches.size(); i++)
            {
                // get byte num
                int posByteStart = listRegExpMatches.at(i).indexOf("[") + 1;
                int posByteEnd = listRegExpMatches.at(i).indexOf("]",posByteStart) - 1;
                int numByteDigits = (posByteEnd-posByteStart) + 1;
                QString byteNumStr = listRegExpMatches.at(i).mid(posByteStart,numByteDigits);
                uint byteNum = stringToUInt(convOk,byteNumStr);

                if(!(byteNum < msgFrame.listMessageData[0].dataBytes.size()))
                {
                    OBDREFDEBUG << "OBDREF: Error: Data for " << msgFrame.name
                                << " has insufficient bytes for parsing: "
                                << parseExpr << "\n";
                    return false;
                }

                uint byteVal = uint(msgFrame.listMessageData[0].dataBytes.at(byteNum));

                // check for bit num
                int posBitStart = listRegExpMatches.at(i).indexOf("[",posByteEnd) + 1;
                if(posBitStart > 0)
                {   // BIT VARIABLE

                    // get bit num
                    QString bitNumStr = listRegExpMatches.at(i).mid(posBitStart,1);
                    uint bitNum = stringToUInt(convOk,bitNumStr);
                    uint bitMask = m_listDecValOfBitPos[bitNum];

                    m_listExprVars[i] = double(byteVal & bitMask) / bitMask;
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
            OBDREFDEBUG << "OBDREF: Error: Could not evaluate expression: \n";
            OBDREFDEBUG << "OBDREF: Error:    Message: " << QString::fromStdString(e.GetMsg()) << "\n";
            OBDREFDEBUG << "OBDREF: Error:    Formula: " << QString::fromStdString(e.GetExpr()) << "\n";
            OBDREFDEBUG << "OBDREF: Error:    Token: " << QString::fromStdString(e.GetToken()) << "\n";
            OBDREFDEBUG << "OBDREF: Error:    Position: " << e.GetPos() << "\n";
            OBDREFDEBUG << "OBDREF: Error:    ErrCode: " << e.GetCode() << "\n";
            return false;
        }

        return true;
    }

    void Parser::convToDecEquivExpression(QString &parseExpr)
    {
        // replaces binary and hex notation numbers
        // with their decimal equivalents

        // look for binary numbers (starting with 0b)
        QRegExp rx; bool convOk = false;

        int pos=0;
        rx.setPattern("(0b[0-1]+)+");
        while ((pos = rx.indexIn(parseExpr, pos)) != -1)
        {
            uint decVal = stringToUInt(convOk,rx.cap(1));
            QString decStr = QString::number(decVal,10);
            parseExpr.replace(pos,rx.cap(1).size(),decStr);
            pos += decStr.length();
        }

        // look for binary numbers (starting with 0x)
        pos = 0;
        rx.setPattern("(0x[0123456789ABCDEF]+)+");
        while ((pos = rx.indexIn(parseExpr, pos)) != -1)
        {
            uint decVal = stringToUInt(convOk,rx.cap(1));
            QString decStr = QString::number(decVal,10);
            parseExpr.replace(pos,rx.cap(1).size(),decStr);
            pos += decStr.length();
        }
    }

    QTextStream & Parser::getErrorStream()
    {
        return m_lkErrors;
    }

    // ========================================================================== //
    // ========================================================================== //

    mu::value_type Parser::muLogicalNot(mu::value_type fVal)
    {
        //std::cerr << "Called LogicalNot" << std::endl;

        if(fVal == 0)
        {   return 1;   }

        else if(fVal == 1)
        {   return 0;   }

        else
        {   throw mu::ParserError("LogicalNot: Argument is not 0 or 1");   }
    }

    mu::value_type Parser::muBitwiseNot(mu::value_type fVal)
    {
        //std::cerr << "Called BitwiseNot" << std::endl;

        ubyte iVal = (ubyte)fVal;
        iVal = ~iVal;
        return mu::value_type(fVal);
    }

    mu::value_type Parser::muBitwiseAnd(mu::value_type fVal1, mu::value_type fVal2)
    {
        //std::cerr << "Called BitwiseAnd" << std::endl;

        ubyte iVal1 = (ubyte)fVal1;
        ubyte iVal2 = (ubyte)fVal2;
        return double(iVal1 & iVal2);
    }

    mu::value_type Parser::muBitwiseOr(mu::value_type fVal1, mu::value_type fVal2)
    {
        //std::cerr << "Called BitwiseOr" << std::endl;

        ubyte iVal1 = (ubyte)fVal1;
        ubyte iVal2 = (ubyte)fVal2;
        return double(iVal1 | iVal2);
    }

    // ========================================================================== //
    // ========================================================================== //
}
