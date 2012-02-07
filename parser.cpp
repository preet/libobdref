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

    Parser::Parser(QString const &filePath, bool &initOk)
    {
        // error logging
        m_lkErrors.setString(&m_lkErrorString, QIODevice::ReadWrite);

        // lookup maps for hex str conversion
        for(int i=0; i < 256; i++)
        {
            QByteArray hexByteStr = QByteArray::number(i,16);
            m_mapValToHexByte.insert(ubyte(i),hexByteStr);
            m_mapHexByteToVal.insert(hexByteStr,ubyte(i));
        }

        // setup pugixml with xml source file
        m_xmlFilePath = filePath;

        pugi::xml_parse_result xmlParseResult;
        xmlParseResult = m_xmlDoc.load_file(filePath.toStdString().c_str());

        if(xmlParseResult)
        {   initOk = true;   }
        else
        {
            OBDREFDEBUG << "XML [" << filePath << "] errors!\n";

            OBDREFDEBUG << "OBDREF: Error Desc: "
                        << QString::fromStdString(xmlParseResult.description()) << "\n";

            OBDREFDEBUG << "OBDREF: Error Offset Char: "
                        << qint64(xmlParseResult.offset) << "\n";

            initOk = false;
            return;
        }

        // setup v8

        // create persistent context and enter it
        v8::HandleScope handleScope;
        m_v8_context = v8::Context::New();
        m_v8_context->Enter();

        // read in and compile globals js
        QString scriptTxt;
        QString scriptFilePath = "/home/preet/Dev/obdref/globals.js";
        if(convTextFileToQStr(scriptFilePath,scriptTxt))
        {   initOk = true;   }
        else
        {
            OBDREFDEBUG << "OBDREF: Error: Could not find globals.js\n";
            initOk = false;
            return;
        }

        // compile and run
        v8::Local<v8::String> scriptStr = v8::String::New(scriptTxt.toLocal8Bit().data());
        v8::Local<v8::Script> scriptSrc = v8::Script::Compile(scriptStr);
        scriptSrc->Run();
    }

    Parser::~Parser()
    {
        // exit and remove context handle
        m_v8_context->Exit();
        m_v8_context.Dispose();
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::BuildMessageFrame(MessageFrame &msgFrame)
    {
        bool foundSpec = false;
        bool foundProtocol = false;
        bool foundAddress = false;
        bool foundParams = false;
        bool foundParameter = false;

        // save spec, protocol, address and param info
        QString specName = msgFrame.spec;
        QString protocolName = msgFrame.protocol;
        QString addressName = msgFrame.address;
        QString paramName = msgFrame.name;

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
                                            uint upperByte = headerVal & 0xF00;      // 0b0000111100000000 = 3840
                                            uint lowerByte = headerVal & 0xFF;       // 0b0000000011111111 = 255

                                            msgFrame.reqHeaderBytes.data.append(char(upperByte));
                                            msgFrame.reqHeaderBytes.data.append(char(lowerByte));
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
                                            uint upperByte = headerVal & 0xF00;      // 0b0000111100000000 = 3840
                                            uint lowerByte = headerVal & 0xFF;       // 0b0000000011111111 = 255

                                            msgFrame.expHeaderBytes.data.append(char(upperByte));
                                            msgFrame.expHeaderBytes.data.append(char(lowerByte));
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
                                        msgFrame.reqHeaderBytes.data.append(char(prioByte));

                                        uint formatByte = stringToUInt(convOk,headerFormat);
                                        msgFrame.reqHeaderBytes.data.append(char(formatByte));

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        msgFrame.reqHeaderBytes.data.append(char(targetByte));

                                        uint sourceByte = stringToUInt(convOk,headerSource);
                                        msgFrame.reqHeaderBytes.data.append(char(sourceByte));
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
                                        msgFrame.expHeaderBytes.data.append(char(prioByte));

                                        uint formatByte = stringToUInt(convOk,headerFormat);
                                        msgFrame.expHeaderBytes.data.append(char(formatByte));

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        msgFrame.expHeaderBytes.data.append(char(targetByte));

                                        if(!headerSource.isEmpty())
                                        {
                                            uint sourceByte = stringToUInt(convOk,headerSource);
                                            msgFrame.expHeaderBytes.data.append(char(sourceByte));
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
                                        msgFrame.reqHeaderBytes.data.append(char(prioByte));

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        msgFrame.reqHeaderBytes.data.append(char(targetByte));

                                        uint sourceByte = stringToUInt(convOk,headerSource);
                                        msgFrame.reqHeaderBytes.data.append(char(sourceByte));
                                    }

                                    pugi::xml_node nodeResp = nodeAddress.child("response");
                                    if(nodeResp)
                                    {
                                        QString headerPrio(nodeResp.attribute("prio").value());
                                        QString headerTarget(nodeResp.attribute("target").value());
                                        QString headerSource(nodeResp.attribute("source").value());

                                        bool convOk = false;
                                        uint prioByte = stringToUInt(convOk,headerPrio);
                                        msgFrame.expHeaderBytes.data.append(char(prioByte));

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        msgFrame.expHeaderBytes.data.append(char(targetByte));

                                        if(!headerSource.isEmpty())
                                        {
                                            uint sourceByte = stringToUInt(convOk,headerSource);
                                            msgFrame.expHeaderBytes.data.append(char(sourceByte));
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
                                bool convOk = false; int n=0; int t=0;

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
                                            myMsgData.expDataPrefix.data.append(char(dataByte));
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
                                                myMsgData.reqDataBytes.data.append(char(dataByte));
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

                                // save parse code
                                QString parseCode(nodeParameter.child_value("script"));
                                if(parseCode.isEmpty())
                                {
                                    OBDREFDEBUG << "OBDREF: Error: No parse info found for "
                                                << "message: " << paramName;
                                    return false;
                                }
                                msgFrame.parseScript = parseCode;

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

    bool Parser::ParseMessageFrame(MessageFrame &msgFrame, QList<obdref::Data> &listData)
    {
        if(msgFrame.parseScript.isEmpty())
        {
            OBDREFDEBUG << "OBDREF: Error: No parse info "
                        << "found in message frame\n";
            return false;
        }

        // clean up response data based on protocol type
        bool formatOk = false;
        if(msgFrame.protocol == "ISO 15765-4 Standard")
        {   formatOk = cleanRawData_ISO_15765_4_ST(msgFrame);   }

        else if(msgFrame.protocol == "ISO 15765-4 Extended")
        {   formatOk = cleanRawData_ISO_15765_4_EX(msgFrame);   }

        else
        {   // TODO
        }

        if(!formatOk)
        {   return false;   }

        // determine if we have a single or multi-part response
        if(msgFrame.listMessageData.size() > 1)
        {
            parseMultiPartResponse(msgFrame,listData);
        }
        else
        {
            parseSinglePartResponse(msgFrame,listData);
        }

        return true;
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

    // ========================================================================== //
    // ========================================================================== //

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

    bool Parser::cleanRawData_ISO_15765_4_ST(MessageFrame &msgFrame)
    {
        for(int i=0; i < msgFrame.listMessageData.size(); i++)
        {
            QList<ByteList> const &listRawDataFrames =
                    msgFrame.listMessageData.at(i).listRawDataFrames;

            QList<ByteList> listUniqueHeaders;
            QList<QList<ByteList> > listMappedDataFrames;

            for(int j=0; j < listRawDataFrames.size(); j++)
            {
                // separate into header/data bytes
                ByteList headerBytes,dataBytes;
                for(int k=0; k < listRawDataFrames.at(j).data.size(); k++)
                {
                    if(k < 2)   {   headerBytes.data << listRawDataFrames.at(j).data.at(k);   }
                    else        {   dataBytes.data << listRawDataFrames.at(j).data.at(k);   }
                }

                // check if expected header bytes exist / match
                if((msgFrame.expHeaderBytes.data.size() > 0) && !(msgFrame.expHeaderBytes == headerBytes))
                {   continue;   }

                // save dataBytes based on frame type (pci byte)
                if((dataBytes.data.at(0) & 0xF0) == 0)              // single frame
                {
                    if(listUniqueHeaders.contains(headerBytes))
                    {
                        OBDREFDEBUG << "OBDREF: Error: Found multiple frame "
                                    << "response types from single address:\n";
                        for(int k=0; k < listRawDataFrames.size(); k++)
                        {
                            OBDREFDEBUG << "OBDREF: Raw Data Frame " << k << ": ";
                            for(int l=0; l < listRawDataFrames.at(k).data.size(); l++)
                            {   OBDREFDEBUG << listRawDataFrames.at(k).data.at(l) << ",";   }
                            OBDREFDEBUG << "\n";
                        }
                        return false;
                    }
                    else
                    {
                        QList<ByteList> listDataBytes;
                        listMappedDataFrames << (listDataBytes << dataBytes);
                        listUniqueHeaders << headerBytes;
                    }
                }
                else if((dataBytes.data.at(0) & 0xF0) == 0x10)
                {                                                   // multi frame [first]
                    int headerIdx = listUniqueHeaders.indexOf(headerBytes);
                    if(headerIdx > -1)
                    {
                        if(((listMappedDataFrames.at(headerIdx).at(0).data.at(0) & 0xF0) == 0))
                        {
                            OBDREFDEBUG << "OBDREF: Error: Found multiple frame "
                                        << "response types from single address:\n";
                            for(int k=0; k < listRawDataFrames.size(); k++)
                            {
                                OBDREFDEBUG << "OBDREF: Raw Data Frame " << k << ": ";
                                for(int l=0; l < listRawDataFrames.at(k).data.size(); l++)
                                {   OBDREFDEBUG << m_mapValToHexByte.value(listRawDataFrames.at(k).data.at(l)) << ",";   }
                                OBDREFDEBUG << "\n";
                            }
                            return false;
                        }
                        else
                        {
                            for(int k=0; k < listMappedDataFrames.at(headerIdx).size(); k++)
                            {
                                if((listMappedDataFrames.at(headerIdx).at(k).data.at(0) & 0xF0) == 0x10)
                                {
                                    OBDREFDEBUG << "OBDREF: Error: Found multiple frame "
                                                << "response types from single address:\n";
                                    for(int l=0; l < listRawDataFrames.size(); l++)
                                    {
                                        OBDREFDEBUG << "OBDREF: Raw Data Frame " << l << ": ";
                                        for(int m=0; m < listRawDataFrames.at(l).data.size(); m++)
                                        {   OBDREFDEBUG << m_mapValToHexByte.value(listRawDataFrames.at(l).data.at(m)) << ",";   }
                                        OBDREFDEBUG << "\n";
                                    }
                                    return false;
                                }
                            }
                            listMappedDataFrames[headerIdx] << dataBytes;
                        }
                    }
                    else
                    {
                        QList<ByteList> listDataBytes;
                        listMappedDataFrames << (listDataBytes << dataBytes);
                        listUniqueHeaders << headerBytes;
                    }
                }
                else if((dataBytes.data.at(0) & 0xF0) == 0x20)
                {                                                   // multi frame [consecutive]
                    int headerIdx = listUniqueHeaders.indexOf(headerBytes);
                    if(headerIdx > -1)
                    {
                        if((listMappedDataFrames.at(headerIdx).at(0).data.at(0) & 0xF0) == 0)
                        {
                            OBDREFDEBUG << "OBDREF: Error: Found mixed single/multi-frame "
                                        << "response from single address:\n";
                            for(int k=0; k < listRawDataFrames.size(); k++)
                            {
                                OBDREFDEBUG << "OBDREF: Raw Data Frame " << k << ": ";
                                for(int l=0; l < listRawDataFrames.at(k).data.size(); l++)
                                {   OBDREFDEBUG << m_mapValToHexByte.value(listRawDataFrames.at(k).data.at(l)) << ",";   }
                                OBDREFDEBUG << "\n";
                            }
                            return false;
                        }
                        else
                        {   listMappedDataFrames[headerIdx] << dataBytes;   }
                    }
                    else
                    {
                        QList<ByteList> listDataBytes;
                        listMappedDataFrames << (listDataBytes << dataBytes);
                        listUniqueHeaders << headerBytes;
                    }
                }
            }

            if(listUniqueHeaders.size() == 0)
            {
                OBDREFDEBUG << "OBDREF: No valid data frames found: \n";
                for(int k=0; k < listRawDataFrames.size(); k++)
                {
                    OBDREFDEBUG << "OBDREF: Raw Data Frame " << k << ": ";
                    for(int l=0; l < listRawDataFrames.at(k).data.size(); l++)
                    {   OBDREFDEBUG << m_mapValToHexByte.value(listRawDataFrames.at(k).data.at(l)) << ",";   }
                    OBDREFDEBUG << "\n";
                }
                return false;
            }

            // sort and merge data under each header
            QList<ByteList> listCatDataBytes;                       // concatenated dataBytes
            for(int j=0; j < listUniqueHeaders.size(); j++)
            {
                if(listMappedDataFrames.at(j).size() == 1)          // single frame
                {
                    listMappedDataFrames[j][0].data.removeAt(0);
                    listCatDataBytes << listMappedDataFrames.at(j).at(0);
                    listMappedDataFrames[j].removeAt(0);
                }
                else
                {                                                   // multi frame
                    ByteList orderedDataBytes; int frameNum = 0;
                    while(listMappedDataFrames.at(j).size() > 0)
                    {
                        if(frameNum == 0)       // first frame
                        {
                            for(int k=0; k < listMappedDataFrames.at(j).size(); k++)
                            {
                                if((listMappedDataFrames.at(j).at(k).data.at(0) & 0xF0) == 0x10)
                                {
                                    listMappedDataFrames[j][k].data.removeAt(0);
                                    listMappedDataFrames[j][k].data.removeAt(0);
                                    orderedDataBytes.data.append(listMappedDataFrames.at(j).at(k).data);
                                    listMappedDataFrames[j].removeAt(k);
                                    break;
                                }
                            }
                        }
                        else                    // consecutive frames
                        {
                            ubyte cfFirstByte = 0x20 + (frameNum & 0x0F);
                            for(int k=0; k < listMappedDataFrames.at(j).size(); k++)
                            {
                                if(listMappedDataFrames.at(j).at(k).data.at(0) == cfFirstByte)
                                {
                                    listMappedDataFrames[j][k].data.removeAt(0);
                                    orderedDataBytes.data.append(listMappedDataFrames.at(j).at(k).data);
                                    listMappedDataFrames[j].removeAt(k);
                                    break;
                                }
                            }
                        }
                        frameNum++;
                    }
                    // save ordered dataBytes
                    listCatDataBytes << orderedDataBytes;
                }
            }

            // TODO check no missing consecutive frames

            // check dataBytes against expected prefix
            for(int j=0; j < listUniqueHeaders.size(); j++)
            {
                bool prefixOk = true;
                for(int k=0; k < msgFrame.listMessageData.at(i).expDataPrefix.data.size(); k++)
                {
                    if(listCatDataBytes.at(j).data.at(k) !=
                            msgFrame.listMessageData.at(i).expDataPrefix.data.at(k))
                    {   prefixOk = false;   break;   }
                }

                if(prefixOk)
                {
                    for(int k=0; k < msgFrame.listMessageData.at(i).expDataPrefix.data.size(); k++)
                    {   listCatDataBytes[j].data.removeAt(0);   }    // remove prefix bytes

                    msgFrame.listMessageData[i].listCleanData << listCatDataBytes.at(j);
                    msgFrame.listMessageData[i].listHeaders << listUniqueHeaders.at(j);
                }
            }

//            //debug
//            for(int j=0; j < msgFrame.listMessageData[i].listHeaders.size(); j++)
//            {
//                qDebug() << msgFrame.listMessageData[i].listHeaders.at(j).data << "|"
//                         << msgFrame.listMessageData[i].listCleanData.at(j).data;
//            }
        }

        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::cleanRawData_ISO_15765_4_EX(MessageFrame &msgFrame)
    {}

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::parseSinglePartResponse(const MessageFrame &msgFrame,
                                         QList<Data> &listDataResults)
    {
        for(int i=0; i < msgFrame.listMessageData.at(0).listCleanData.size(); i++)
        {
            ByteList const & headerBytes = msgFrame.listMessageData.at(0).listHeaders.at(i);
            ByteList const & dataBytes = msgFrame.listMessageData.at(0).listCleanData.at(i);


        }
    }

    bool Parser::parseMultiPartResponse(const MessageFrame &msgFrame,
                                        QList<Data> &listDataResults)
    {}

    // ========================================================================== //
    // ========================================================================== //

    uint Parser::stringToUInt(bool &convOk, const QString &parseStr)
    {
        // expect ONE token representing a number
        // if str contains '0bXXXXX' -> parse as binary
        // if str contains '0xXXXXX' -> parse as hex
        // else parse as dec

        // TODO: use maps! takes less time than conversion!

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

    // ========================================================================== //
    // ========================================================================== //

    QTextStream & Parser::getErrorStream()
    {
        return m_lkErrors;
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::convTextFileToQStr(const QString &filePath,
                                    QString & fileDataAsStr)
    {
        // TODO: use QFile and QTextStream instead of stdlib?

        std::ifstream file;
        file.open(filePath.toLocal8Bit().data());

        std::string contentAsString;
        if(file.is_open())
        {
            std::string temp;
            while(!file.eof())
            {
                std::getline(file,temp);
                contentAsString.append(temp).append("\n");
            }
            file.close();
        }
        else
        {
            OBDREFDEBUG << "OBDREF: Could not open file: " << filePath << "\n";
            return false;
        }

        fileDataAsStr = QString::fromStdString(contentAsString);
        return true;
    }
}
