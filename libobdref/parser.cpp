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
        QString scriptFilePath = H_INSTALLPATH;
        scriptFilePath += QString("/globals.js");

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

        // get refs to global vars
        v8::Local<v8::Value> val_listDataBytes;
        val_listDataBytes = m_v8_context->Global()->Get(v8::String::New("global_list_databytes"));
        m_v8_listDataBytes = v8::Persistent<v8::Object>::New(val_listDataBytes->ToObject());

        v8::Local<v8::Value> val_listLitData;
        val_listLitData = m_v8_context->Global()->Get(v8::String::New("global_list_lit_data"));
        m_v8_listLitData = v8::Persistent<v8::Object>::New(val_listLitData->ToObject());

        v8::Local<v8::Value> val_listNumData;
        val_listNumData = m_v8_context->Global()->Get(v8::String::New("global_list_num_data"));
        m_v8_listNumData = v8::Persistent<v8::Object>::New(val_listNumData->ToObject());
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
        bool foundSpec          = false;
        bool foundProtocol      = false;
        bool foundAddress       = false;
        bool foundParams        = false;
        bool foundParameter     = false;

        QString specName        = msgFrame.spec;
        QString protocolName    = msgFrame.protocol;
        QString addressName     = msgFrame.address;
        QString paramName       = msgFrame.name;

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

                                // [build request message header]

                                MessageData msgData;
                                msgFrame.listMessageData.push_back(msgData);

                                // special case 1: ISO 15765-4 (11-bit id)
                                // store 11-bit header in two bytes
                                if(protocolName == "ISO 15765-4 Standard")
                                {   // request header bytes
                                    pugi::xml_node nodeReq = nodeAddress.child("request");
                                    if(nodeReq)
                                    {
                                        QString hIdentifier(nodeReq.attribute("identifier").value());
                                        if(!hIdentifier.isEmpty())
                                        {
                                            bool convOk = false;
                                            uint headerVal = stringToUInt(convOk,hIdentifier);
                                            uint upperByte = (headerVal & 0xF00) >> 8;
                                            uint lowerByte = headerVal & 0xFF;

                                            ByteList &reqHeaderBytes =
                                                    msgFrame.listMessageData[0].reqHeaderBytes;

                                            reqHeaderBytes.data << char(upperByte);
                                            reqHeaderBytes.data << char(lowerByte);
                                        }
                                        else
                                        {
                                            OBDREFDEBUG << "OBDREF: Error: ISO 15765-4 Std,"
                                                        << "Request Identifier Empty";
                                            return false;
                                        }
                                    }
                                    else
                                    {
                                        OBDREFDEBUG << "OBDREF: Error: ISO 15765-4 Std,"
                                                    << "No Request Header";
                                        return false;
                                    }
                                    // response header bytes
                                    pugi::xml_node nodeResp = nodeAddress.child("response");
                                    if(nodeResp)
                                    {
                                        QString hIdentifier(nodeResp.attribute("identifier").value());
                                        if(!hIdentifier.isEmpty())
                                        {
                                            bool convOk = false;
                                            uint headerVal = stringToUInt(convOk,hIdentifier);
                                            uint upperByte = (headerVal & 0xF00) >> 8;
                                            uint lowerByte = headerVal & 0xFF;

                                            ByteList &expHeaderBytes =
                                                    msgFrame.listMessageData[0].expHeaderBytes;

                                            ByteList &expHeaderMask =
                                                    msgFrame.listMessageData[0].expHeaderMask;

                                            expHeaderBytes.data << char(upperByte);
                                            expHeaderBytes.data << char(lowerByte);
                                            expHeaderMask.data << char(0xFF) << char(0xFF);
                                        }
                                    }
                                }
                                // special case 2: ISO 15765-4 (29-bit id)
                                // we have to include the special 'format' byte
                                // so this header is made up of four bytes:
                                // [prio] [format] [target] [source]
                                else if(protocolName == "ISO 15765-4 Extended")
                                {   // request header bytes
                                    pugi::xml_node nodeReq = nodeAddress.child("request");
                                    if(nodeReq)
                                    {
                                        QString headerPrio(nodeReq.attribute("prio").value());
                                        QString headerFormat(nodeReq.attribute("format").value());
                                        QString headerTarget(nodeReq.attribute("target").value());
                                        QString headerSource(nodeReq.attribute("source").value());

                                        // all four bytes must be defined
                                        if(headerPrio.isEmpty() || headerFormat.isEmpty() ||
                                           headerTarget.isEmpty() || headerSource.isEmpty())   {
                                            OBDREFDEBUG << "OBDREF: Error: ISO 15765-4 Ext,"
                                                        << "Incomplete Request Header";
                                            return false;
                                        }

                                        bool convOk = false;

                                        ByteList &reqHeaderBytes =
                                                msgFrame.listMessageData[0].reqHeaderBytes;

                                        uint prioByte = stringToUInt(convOk,headerPrio);
                                        reqHeaderBytes.data << char(prioByte);

                                        uint formatByte = stringToUInt(convOk,headerFormat);
                                        reqHeaderBytes.data << char(formatByte);

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        reqHeaderBytes.data << char(targetByte);

                                        uint sourceByte = stringToUInt(convOk,headerSource);
                                        reqHeaderBytes.data << char(sourceByte);
                                    }
                                    else
                                    {
                                        OBDREFDEBUG << "OBDREF: Error: ISO 15765-4 Ext,"
                                                    << "No Request Header";
                                        return false;
                                    }
                                    // response header bytes
                                    pugi::xml_node nodeResp = nodeAddress.child("response");
                                    if(nodeResp)
                                    {
                                        // note: regardless of which bytes are defined
                                        // in the response tag, fill out a full header
                                        // in expHeaderBytes and expHeaderMask so the
                                        // expResponse byte ordering is correct

                                        QString headerPrio(nodeResp.attribute("prio").value());
                                        QString headerFormat(nodeResp.attribute("format").value());
                                        QString headerTarget(nodeResp.attribute("target").value());
                                        QString headerSource(nodeResp.attribute("source").value());

                                        bool convOk = false;

                                        ByteList &expHeaderBytes =
                                                msgFrame.listMessageData[0].expHeaderBytes;

                                        ByteList &expHeaderMask =
                                                msgFrame.listMessageData[0].expHeaderMask;

                                        // priority byte
                                        if(!headerPrio.isEmpty())   {
                                            uint prioByte = stringToUInt(convOk,headerPrio);
                                            expHeaderBytes.data << char(prioByte);
                                            expHeaderMask.data << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes.data << char(0x00);
                                            expHeaderMask.data << char(0x00);
                                        }
                                        // format byte
                                        if(!headerFormat.isEmpty())   {
                                            uint formatByte = stringToUInt(convOk,headerFormat);
                                            expHeaderBytes.data << char(formatByte);
                                            expHeaderMask.data << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes.data << char(0x00);
                                            expHeaderMask.data << char(0x00);
                                        }
                                        // target byte
                                        if(!headerTarget.isEmpty())   {
                                            uint targetByte = stringToUInt(convOk,headerTarget);
                                            expHeaderBytes.data << char(targetByte);
                                            expHeaderMask.data << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes.data << char(0x00);
                                            expHeaderMask.data << char(0x00);
                                        }
                                        // source byte
                                        if(!headerSource.isEmpty())   {
                                            uint sourceByte = stringToUInt(convOk,headerSource);
                                            expHeaderBytes.data << (sourceByte);
                                            expHeaderMask.data << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes.data << char(0x00);
                                            expHeaderMask.data << char(0x00);
                                        }
                                    }
                                }
                                // special case 3: ISO 14230-4
                                // this protocol has a variable header:
                                // A [format]
                                // B [format] [target] [source]
                                // C [format] [length]
                                // D [format] [target] [source] [length]
                                // * with A and B, the data length is encoded
                                //   in the six least significant bits of
                                //   the format byte: 0b??LLLLLL
                                // * with C and D, the data length is
                                //   specified in its own byte and the
                                //   six least significant bits in the
                                //   format byte are zero'd: 0b??000000
                                else if(protocolName == "ISO 14230-4")
                                {   // request header bytes
                                    pugi::xml_node nodeReq = nodeAddress.child("request");
                                    if(nodeReq)
                                    {
                                        QString headerFormat(nodeReq.attribute("format").value());
                                        QString headerTarget(nodeReq.attribute("target").value());
                                        QString headerSource(nodeReq.attribute("source").value());
                                        QString headerLength(nodeReq.attribute("length").value());

                                        // check to ensure given header bytes are valid
                                        bool validHeader = false;
                                        if(!headerFormat.isEmpty())   {
                                            // A or C
                                            if(headerTarget.isEmpty() &&
                                               headerSource.isEmpty())   {
                                                validHeader = true;
                                            }
                                            // B or D
                                            else if(!headerTarget.isEmpty() &&
                                                    !headerSource.isEmpty())   {
                                                validHeader = true;
                                            }
                                        }

                                        if(!validHeader)   {
                                            OBDREFDEBUG << "OBDREF: Error: ISO 14230-4,"
                                                        << "Incomplete Request Header";
                                            return false;
                                        }

                                        ByteList &reqHeaderBytes =
                                                msgFrame.listMessageData[0].reqHeaderBytes;

                                        bool convOk;

                                        // format byte (mandatory)
                                        uint formatByte = stringToUInt(convOk,headerFormat);
                                        reqHeaderBytes.data << char(formatByte);

                                        // target byte
                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        reqHeaderBytes.data << char(targetByte);

                                        // source byte
                                        uint sourceByte = stringToUInt(convOk,headerSource);
                                        reqHeaderBytes.data << char(sourceByte);

                                        // length byte
                                        if(!headerLength.isEmpty())   {
                                            uint lengthByte = stringToUInt(convOk,headerLength);
                                            reqHeaderBytes.data << char(lengthByte);
                                        }
                                    }
                                    else
                                    {
                                        OBDREFDEBUG << "OBDREF: Error: ISO 14230-4,"
                                                    << "No Request Header";
                                        return false;
                                    }
                                    // response header bytes
                                    pugi::xml_node nodeResp = nodeAddress.child("response");
                                    if(nodeResp)
                                    {
                                        // note: regardless of which bytes are defined
                                        // in the response tag, fill out a full header
                                        // in expHeaderBytes and expHeaderMask so the
                                        // expResponse byte ordering is correct

                                        QString headerFormat(nodeResp.attribute("format").value());
                                        QString headerTarget(nodeResp.attribute("target").value());
                                        QString headerSource(nodeResp.attribute("source").value());

                                        bool convOk = false;

                                        ByteList &expHeaderBytes =
                                                msgFrame.listMessageData[0].expHeaderBytes;

                                        ByteList &expHeaderMask =
                                                msgFrame.listMessageData[0].expHeaderMask;

                                        // format byte
                                        if(!headerFormat.isEmpty())   {
                                            uint formatByte = stringToUInt(convOk,headerFormat);
                                            expHeaderBytes.data << char(formatByte);
                                            expHeaderMask.data << char(0xC0);
                                            // note: the mask for the formatByte is set to
                                            // 0b11000000 (0xC0) to ignore the length bits
                                        }
                                        else   {
                                            expHeaderBytes.data << char(0x00);
                                            expHeaderMask.data << char(0x00);
                                        }
                                        // target byte
                                        if(!headerTarget.isEmpty())   {
                                            uint targetByte = stringToUInt(convOk,headerTarget);
                                            expHeaderBytes.data << char(targetByte);
                                            expHeaderMask.data << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes.data << char(0x00);
                                            expHeaderMask.data << char(0x00);
                                        }
                                        // source byte
                                        if(!headerSource.isEmpty())   {
                                            uint sourceByte = stringToUInt(convOk,headerSource);
                                            expHeaderBytes.data << char(sourceByte);
                                            expHeaderMask.data << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes.data << char(0x00);
                                            expHeaderMask.data << char(0x00);
                                        }

                                        // length byte is ignored for expResponse
                                        expHeaderBytes.data << char(0x00);
                                        expHeaderMask.data << char(0x00);
                                    }
                                }

                                // default case:
                                // ISO 9141-2, SAE J1850 VPW/PWM all
                                // share the same obd message structure
                                else
                                {   // request header bytes
                                    pugi::xml_node nodeReq = nodeAddress.child("request");
                                    if(nodeReq)
                                    {
                                        QString headerPrio(nodeReq.attribute("prio").value());
                                        QString headerTarget(nodeReq.attribute("target").value());
                                        QString headerSource(nodeReq.attribute("source").value());

                                        // all three bytes must be defined
                                        if(headerPrio.isEmpty() || headerTarget.isEmpty() ||
                                           headerSource.isEmpty())   {
                                            OBDREFDEBUG << "OBDREF: Error: ISO 9141-2/SAE J1850,"
                                                        << "Incomplete Request Header";
                                            return false;
                                        }

                                        ByteList &reqHeaderBytes =
                                                msgFrame.listMessageData[0].reqHeaderBytes;

                                        bool convOk = false;

                                        uint prioByte = stringToUInt(convOk,headerPrio);
                                        reqHeaderBytes.data << char(prioByte);

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        reqHeaderBytes.data << char(targetByte);

                                        uint sourceByte = stringToUInt(convOk,headerSource);
                                        reqHeaderBytes.data << char(sourceByte);
                                    }
                                    else
                                    {
                                        OBDREFDEBUG << "OBDREF: Error: ISO 9141-2/SAE J1850,"
                                                    << "No Request Header";
                                        return false;
                                    }
                                    // response header bytes
                                    pugi::xml_node nodeResp = nodeAddress.child("response");
                                    if(nodeResp)
                                    {
                                        // note: regardless of which bytes are defined
                                        // in the response tag, fill out a full header
                                        // in expHeaderBytes and expHeaderMask so the
                                        // expResponse byte ordering is correct

                                        QString headerPrio(nodeResp.attribute("prio").value());
                                        QString headerTarget(nodeResp.attribute("target").value());
                                        QString headerSource(nodeResp.attribute("source").value());

                                        bool convOk = false;

                                        ByteList &expHeaderBytes =
                                                msgFrame.listMessageData[0].expHeaderBytes;

                                        ByteList &expHeaderMask =
                                                msgFrame.listMessageData[0].expHeaderMask;

                                        // priority byte
                                        if(!headerPrio.isEmpty())   {
                                            uint prioByte = stringToUInt(convOk,headerPrio);
                                            expHeaderBytes.data << char(prioByte);
                                            expHeaderMask.data << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes.data << char(0x00);
                                            expHeaderMask.data << char(0x00);
                                        }
                                        // target byte
                                        if(!headerTarget.isEmpty())   {
                                            uint targetByte = stringToUInt(convOk,headerTarget);
                                            expHeaderBytes.data << char(targetByte);
                                            expHeaderMask.data << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes.data << char(0x00);
                                            expHeaderMask.data << char(0x00);
                                        }
                                        // source byte
                                        if(!headerSource.isEmpty())   {
                                            uint sourceByte = stringToUInt(convOk,headerSource);
                                            expHeaderBytes.data << char(sourceByte);
                                            expHeaderMask.data << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes.data << char(0x00);
                                            expHeaderMask.data << char(0x00);
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

                                // [build request message data]

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
                                    QString respPrefixName,respBytesName,
                                            reqDelayName,reqName;

                                    if(n == 0)
                                    {
                                        respPrefixName  = QString("response.prefix");
                                        respBytesName   = QString("response.bytes");
                                        reqDelayName    = QString("request.delay");
                                        reqName         = QString("request");
                                    }
                                    else
                                    {
                                        respPrefixName  = QString("response")+QString::number(n,10)+QString(".prefix");
                                        respBytesName   = QString("response")+QString::number(n,10)+QString(".bytes");
                                        reqDelayName    = QString("request")+QString::number(n,10)+QString(".delay");
                                        reqName         = QString("request")+QString::number(n,10);
                                    }

                                    std::string respPrefixAttrStr   = respPrefixName.toStdString();
                                    std::string respBytesAttrStr    = respBytesName.toStdString();
                                    std::string reqDelayAttrStr     = reqDelayName.toStdString();
                                    std::string reqAttrStr          = reqName.toStdString();

                                    // [responseN.prefix]
                                    if(nodeParameter.attribute(respPrefixAttrStr.c_str()))
                                    {
                                        MessageData * msgData;
                                        if(n == 0 || n == 1)   {
                                            // we already have a single MessageData
                                            // element in msgFrame's listMessageData
                                            // from adding a set of header bytes
                                            msgData = &(msgFrame.listMessageData[0]);
                                        }
                                        else   {
                                            // add to listMessageData as necessary
                                            MessageData mData;
                                            msgFrame.listMessageData << mData;

                                            size_t listSize = msgFrame.listMessageData.size();
                                            msgData = &(msgFrame.listMessageData[listSize-1]);
                                        }

                                        QString respPrefix(nodeParameter.attribute(respPrefixAttrStr.c_str()).value());
                                        QStringList listRespPrefix = respPrefix.split(" ");
                                        for(int i=0; i < listRespPrefix.size(); i++)   {
                                            uint dataByte = stringToUInt(convOk,listRespPrefix.at(i));
                                            msgData->expDataPrefix.data << char(dataByte);
                                        }

                                        // [responseN.bytes]
                                        if(nodeParameter.attribute(respBytesAttrStr.c_str()))   {
                                            QString respBytes(nodeParameter.attribute(respBytesAttrStr.c_str()).value());
                                            msgData->expDataBytes = respBytes;
                                        }
                                        // [requestN.delay]
                                        if(nodeParameter.attribute(reqDelayAttrStr.c_str()))   {
                                            QString reqDelay(nodeParameter.attribute(reqDelayAttrStr.c_str()).value());
                                            msgData->reqDataDelayMs = stringToUInt(convOk,reqDelay);
                                        }
                                        // [requestN]
                                        if(nodeParameter.attribute(reqAttrStr.c_str()))   {
                                            QString reqData(nodeParameter.attribute(reqAttrStr.c_str()).value());
                                            QStringList listReqData = reqData.split(" ");
                                            for(int i=0; i < listReqData.size(); i++)   {
                                                uint dataByte = stringToUInt(convOk,listReqData.at(i));
                                                msgData->reqDataBytes.data << char(dataByte);
                                            }
                                        }
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

                                // ISO 15765-4:
                                // we need to prepend the data bytes with a PCI
                                // byte specifying frame order and data length
                                msgFrame.multiFrameReq = false;
                                if(protocolName.contains("ISO 15765-4"))
                                {
                                    if(msgFrame.multiFrameReq)
                                    {   // todo
                                        // set pci byte up for multiframe request

                                    }
                                    else
                                    {   // single frame request
                                        // higher 4 bits set to 0 to indicate single frame
                                        // lower 4 bits gives the number of bytes in payload
                                        for(size_t i=0; i < msgFrame.listMessageData.size(); i++)
                                        {
                                            ByteList &reqDataBytes =
                                                    msgFrame.listMessageData[i].reqDataBytes;

                                            ubyte pciByte = reqDataBytes.data.size();
                                            reqDataBytes.data.prepend(pciByte);
                                        }
                                    }
                                }

                                // ISO 14230-4:
                                // we need to add data length info to the header
                                if(protocolName.contains("ISO 14230"))
                                {
                                    // data length value can be from from 1-255,
                                    // includes all databytes, excludes checksum
                                    for(size_t i=0; i < msgFrame.listMessageData.size(); i++)
                                    {
                                        MessageData &msgData = msgFrame.listMessageData[i];
                                        ubyte hSize = msgData.reqHeaderBytes.data.size();
                                        ubyte dSize = msgData.reqDataBytes.data.size();

                                        if(dSize > 255)   {
                                            OBDREFDEBUG << "OBDREF: Error: ISO 14230-4,"
                                                        << " Invalid Data Length";
                                            return false;
                                        }

                                        // if the header is two or four bytes
                                        // add data length to the last byte
                                        if(hSize % 2 == 0)   {
                                            msgData.reqHeaderBytes.data[hSize-1] = dSize;
                                        }

                                        // else if the header is one or three bytes
                                        else   {
                                            if(dSize > 63)   {
                                                // add data length to six least significant
                                                // bits of the first (format) byte
                                                ubyte formatByte = msgData.reqHeaderBytes.data[0];
                                                formatByte = formatByte | dSize;
                                            }
                                            else    {
                                                // or append a length byte if its > 63 bytes
                                                msgData.reqHeaderBytes.data << dSize;
                                            }
                                        }
                                    }
                                }

                                // save parse code
                                QString parseCode;
                                pugi::xml_node nodeScript = nodeParameter.child("script");
                                if(nodeScript.attribute("protocols"))
                                {
                                    QString scriptProtocols(nodeScript.attribute("protocols").value());
                                    if(scriptProtocols.contains(protocolName))
                                    {   parseCode = QString(nodeParameter.child_value("script"));   }
                                }
                                else
                                {   parseCode = QString(nodeParameter.child_value("script"));   }

                                if(parseCode.isEmpty())
                                {
                                    OBDREFDEBUG << "OBDREF: Error: No parse info found for "
                                                << "message: " << paramName << "\n";
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

        if(msgFrame.protocol.contains("ISO 15765-4"))
        {   formatOk = cleanRawData_ISO_15765_4(msgFrame);   }
        else
        {   // TODO
        }     

        if(!formatOk)
        {
            OBDREFDEBUG << "OBDREF: Error: Could not clean "
                        << "raw data using spec'd format\n";
            return false;
        }

        // determine if we have a single or multi-part response
        if(msgFrame.listMessageData.size() > 1)
        {   parseMultiPartResponse(msgFrame,listData);   }
        else
        {   parseSinglePartResponse(msgFrame,listData);   }

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

    bool Parser::cleanRawData_Default(MessageFrame &msgFrame)
    {
//        for(size_t i=0; i < msgFrame.listMessageData.size(); i++)
//        {
//            QList<ByteList> listUniqueHeaders;
//            QList<QList<ByteList> > listMappedDataFrames;
//            QList<ByteList> const &listRawDataFrames =
//                    msgFrame.listMessageData.at(i).listRawDataFrames;

//            size_t numHeaderBytes;
//        }
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::cleanRawData_ISO_15765_4(MessageFrame &msgFrame)
    {

        /*
        for(int i=0; i < msgFrame.listMessageData.size(); i++)
        {
            QList<ByteList> const &listRawDataFrames =
                    msgFrame.listMessageData.at(i).listRawDataFrames;

            QList<ByteList> listUniqueHeaders;
            QList<QList<ByteList> > listMappedDataFrames;

            int numHeaderBytes;
            if(msgFrame.protocol.contains("Standard"))
            {   numHeaderBytes = 2;   }
            else
            {   numHeaderBytes = 4;   }

            for(int j=0; j < listRawDataFrames.size(); j++)
            {
                // separate into header/data bytes
                ByteList headerBytes,dataBytes;

                for(int k=0; k < listRawDataFrames.at(j).data.size(); k++)
                {
                    if(k < numHeaderBytes)   {   headerBytes.data << listRawDataFrames.at(j).data.at(k);   }
                    else                     {   dataBytes.data << listRawDataFrames.at(j).data.at(k);   }
                }

                // check if expected header bytes exist / match
                bool headerBytesMatch = true;
                if(msgFrame.expHeaderBytes.data.size() > 0)
                {
                    for(int k=0; k < headerBytes.data.size(); k++)
                    {   // expHeaderMask byte of 0xFF means we expect a match
                        if(msgFrame.expHeaderMask.data.at(k) == 0xFF &&
                                (msgFrame.expHeaderBytes.data.at(k) != headerBytes.data.at(k)))
                        {   headerBytesMatch = false;   }
                    }
                }

                if(!headerBytesMatch)
                {   continue;   }

                // save dataBytes based on frame type (pci byte)
                if((dataBytes.data.at(0) & 0xF0) == 0)              // single frame
                {
                    if(listUniqueHeaders.contains(headerBytes))
                    {
                        OBDREFDEBUG << "OBDREF: Error: Found multiple frame "
                                    << "response types from single address:\n";
                        dumpRawDataToDebugInfo(listRawDataFrames);
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
                            dumpRawDataToDebugInfo(listRawDataFrames);
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
                                    dumpRawDataToDebugInfo(listRawDataFrames);
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
                            dumpRawDataToDebugInfo(listRawDataFrames);
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
                OBDREFDEBUG << "OBDREF: Error: No valid data frames found: \n";
                dumpRawDataToDebugInfo(listRawDataFrames);
                return false;
            }

            // sort and merge data under each header
            QList<ByteList> listCatDataBytes;                       // concatenated dataBytes
            for(int j=0; j < listUniqueHeaders.size(); j++)
            {
                if(listMappedDataFrames.at(j).size() == 1)          // single frame
                {
                    if((listMappedDataFrames.at(j).at(0).data.at(0) & 0xF0) != 0x00)
                    {
                        OBDREFDEBUG << "OBDREF: Error: Single frame response,"
                                    << "but incorrect PCI byte: \n";
                        dumpRawDataToDebugInfo(listRawDataFrames);
                        return false;
                    }
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

            //debug
//            for(int j=0; j < msgFrame.listMessageData[i].listHeaders.size(); j++)
//            {
//                qDebug() << msgFrame.listMessageData[i].listHeaders.at(j).data << "|"
//                         << msgFrame.listMessageData[i].listCleanData.at(j).data;
//            }

            if(listCatDataBytes.size() > 0)
            {
                if(msgFrame.listMessageData.at(i).listCleanData.size() == 0)
                {
                    OBDREFDEBUG << "OBDREF: Error: No frames with correct prefix: \n";
                    dumpRawDataToDebugInfo(listRawDataFrames);
                    return false;
                }
            }
            else
            {   return false;   }
        }

        return true;

        */
    }

    // ========================================================================== //
    // ========================================================================== //

    void Parser::dumpRawDataToDebugInfo(const QList<ByteList> &listRawDataFrames)
    {
        for(int k=0; k < listRawDataFrames.size(); k++)
        {
            OBDREFDEBUG << "OBDREF: Raw Data Frame " << k << ": ";
            for(int l=0; l < listRawDataFrames.at(k).data.size(); l++)
            {   OBDREFDEBUG << m_mapValToHexByte.value(listRawDataFrames.at(k).data.at(l)) << " ";   }
            OBDREFDEBUG << "\n";
        }
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::parseSinglePartResponse(const MessageFrame &msgFrame,
                                         QList<Data> &listDataResults)
    {
        // a single part response may have responses from different addresses
        // but is interpreted as one response message per parameter per address
        for(int i=0; i < msgFrame.listMessageData.at(0).listCleanData.size(); i++)
        {
            obdref::Data myData;
            ByteList const & headerBytes = msgFrame.listMessageData.at(0).listHeaders.at(i);
            ByteList const & dataBytes = msgFrame.listMessageData.at(0).listCleanData.at(i);

            v8::HandleScope handleScope;

            // fill out response meta data
            for(int j=0; j < headerBytes.data.size(); j++)
            {   myData.srcAddress.append(QString::number(int(headerBytes.data.at(j)),16));   }

            myData.paramName = msgFrame.name;
            myData.srcName = msgFrame.address;
            myData.srcAddress = myData.srcAddress.toUpper();

            // clear existing databytes in js context
            v8::Local<v8::Value> val_clearDataBytes;
            val_clearDataBytes = m_v8_listDataBytes->Get(v8::String::New("clearDataBytes"));

            v8::Local<v8::Function> method_clearDataBytes;
            method_clearDataBytes = v8::Local<v8::Function>::Cast(val_clearDataBytes);
            method_clearDataBytes->Call(m_v8_listDataBytes,0,NULL);

            // clear existing lit and num data lists
            v8::Local<v8::Value> val_clearLitData;
            val_clearLitData = m_v8_listLitData->Get(v8::String::New("clearData"));

            v8::Local<v8::Function> method_clearLitData;
            method_clearLitData = v8::Local<v8::Function>::Cast(val_clearLitData);
            method_clearLitData->Call(m_v8_listLitData,0,NULL);

            v8::Local<v8::Value> val_clearNumData;
            val_clearNumData = m_v8_listNumData->Get(v8::String::New("clearData"));

            v8::Local<v8::Function> method_clearNumData;
            method_clearNumData = v8::Local<v8::Function>::Cast(val_clearNumData);
            method_clearNumData->Call(m_v8_listNumData,0,NULL);

            // copy over databytes to js context
            v8::Local<v8::Array> dataByteArray = v8::Array::New(dataBytes.data.size());
            for(int j=0; j < dataBytes.data.size(); j++)
            {   dataByteArray->Set(j,v8::Integer::New(int(dataBytes.data.at(j))));   }

            v8::Local<v8::Value> val_appendDataBytes;
            val_appendDataBytes = m_v8_listDataBytes->Get(v8::String::New("appendDataBytes"));

            v8::Local<v8::Value> args_appendDataBytes[] = { dataByteArray };
            v8::Local<v8::Function> method_appendDataBytes;
            method_appendDataBytes = v8::Local<v8::Function>::Cast(val_appendDataBytes);
            method_appendDataBytes->Call(m_v8_listDataBytes,1,args_appendDataBytes);

            // evaluate the data
            v8::Local<v8::String> scriptString;
            v8::Local<v8::Script> scriptSource;
            scriptString = v8::String::New(msgFrame.parseScript.toLocal8Bit().data());
            scriptSource = v8::Script::Compile(scriptString);
            scriptSource->Run();

            // save evaluation results
            this->saveNumAndLitData(myData);
            listDataResults.append(myData);
        }

        return true;
    }

    bool Parser::parseMultiPartResponse(const MessageFrame &msgFrame,
                                        QList<Data> &listDataResults)
    {
        // a multi part response is limited to receiving responses
        // from only one address
        for(int i=0; i < msgFrame.listMessageData.size(); i++)
        {
            if(msgFrame.listMessageData.at(i).listCleanData.size() > 1)
            {
                OBDREFDEBUG << "OBDREF: Error: Frame has multi-part message but "
                            << "received responses from multiple addresses "
                            << "(only one address allowed for multi-part "
                            << "messages)";
                return false;
            }
        }

        // check to make sure all parts of the response come from
        // the same address
        for(int i=1; i < msgFrame.listMessageData.size(); i++)
        {
            if(!(msgFrame.listMessageData.at(i).listHeaders.at(0) ==
                    msgFrame.listMessageData.at(i-1).listHeaders.at(0)))
            {
                OBDREFDEBUG << "OBDREF: Error: Expected all parts of "
                            << "multi-part message to originate from "
                            << "same address";
                return false;
            }
        }

        obdref::Data myData;
        ByteList const & headerBytes = msgFrame.listMessageData.at(0).listHeaders.at(0);

        for(int j=0; j < headerBytes.data.size(); j++)
        {   myData.srcAddress.append(QString::number(int(headerBytes.data.at(j)),16));   }

        myData.paramName = msgFrame.name;
        myData.srcAddress = myData.srcAddress.toUpper();

        v8::HandleScope handleScope;

        // clear existing databytes in js context
        v8::Local<v8::Value> val_clearDataBytes;
        val_clearDataBytes = m_v8_listDataBytes->Get(v8::String::New("clearDataBytes"));

        v8::Local<v8::Function> method_clearDataBytes;
        method_clearDataBytes = v8::Local<v8::Function>::Cast(val_clearDataBytes);
        method_clearDataBytes->Call(m_v8_listDataBytes,0,NULL);

        // clear existing lit and num data lists
        v8::Local<v8::Value> val_clearLitData;
        val_clearLitData = m_v8_listLitData->Get(v8::String::New("clearData"));

        v8::Local<v8::Function> method_clearLitData;
        method_clearLitData = v8::Local<v8::Function>::Cast(val_clearLitData);
        method_clearLitData->Call(m_v8_listLitData,0,NULL);

        v8::Local<v8::Value> val_clearNumData;
        val_clearNumData = m_v8_listNumData->Get(v8::String::New("clearData"));

        v8::Local<v8::Function> method_clearNumData;
        method_clearNumData = v8::Local<v8::Function>::Cast(val_clearNumData);
        method_clearNumData->Call(m_v8_listNumData,0,NULL);

        for(int i=0; i < msgFrame.listMessageData.size(); i++)
        {
            ByteList const & dataBytes = msgFrame.listMessageData.at(i).listCleanData.at(0);

            // copy over databytes to js context
            v8::Local<v8::Array> dataByteArray = v8::Array::New(dataBytes.data.size());
            for(int j=0; j < dataBytes.data.size(); j++)
            {   dataByteArray->Set(j,v8::Integer::New(int(dataBytes.data.at(j))));   }

            v8::Local<v8::Value> val_appendDataBytes;
            val_appendDataBytes = m_v8_listDataBytes->Get(v8::String::New("appendDataBytes"));

            v8::Local<v8::Value> args_appendDataBytes[] = { dataByteArray };
            v8::Local<v8::Function> method_appendDataBytes;
            method_appendDataBytes = v8::Local<v8::Function>::Cast(val_appendDataBytes);
            method_appendDataBytes->Call(m_v8_listDataBytes,1,args_appendDataBytes);
        }

        // evaluate the data
        v8::Local<v8::String> scriptString;
        v8::Local<v8::Script> scriptSource;
        scriptString = v8::String::New(msgFrame.parseScript.toLocal8Bit().data());
        scriptSource = v8::Script::Compile(scriptString);
        scriptSource->Run();

        // save evaluation results
        this->saveNumAndLitData(myData);
        listDataResults.append(myData);

        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    void Parser::saveNumAndLitData(Data &myData)
    {
        // saves numerical and literal data from js context

        // NOTE: it is expected that execution is already in
        //       a handle scope

        // get numerical values, min, max, units, desc
        v8::Local<v8::Value> val_listNumData = m_v8_listNumData->Get(v8::String::New("listData"));
        v8::Local<v8::Array> array_numData = v8::Local<v8::Array>::Cast(val_listNumData);
        for(int j=0; j < array_numData->Length(); j++)
        {
            v8::Local<v8::Value> val_numData = array_numData->Get(j);
            v8::Local<v8::Object> obj_numData = val_numData->ToObject();

            v8::String::Utf8Value numUnitsStr(obj_numData->Get(v8::String::New("units"))->ToString());
            v8::String::Utf8Value numPropertyStr(obj_numData->Get(v8::String::New("property"))->ToString());

            obdref::NumericalData myNumData;
            myNumData.value = obj_numData->Get(v8::String::New("value"))->NumberValue();
            myNumData.min = obj_numData->Get(v8::String::New("min"))->NumberValue();
            myNumData.max = obj_numData->Get(v8::String::New("max"))->NumberValue();
            myNumData.units = QString(*numUnitsStr);
            myNumData.property = QString(*numPropertyStr);
            myData.listNumericalData.append(myNumData);
        }

        // get literal values, valueIfFalse, valueIfTrue, property, desc
        v8::Local<v8::Value> val_listLitData = m_v8_listLitData->Get(v8::String::New("listData"));
        v8::Local<v8::Array> array_litData = v8::Local<v8::Array>::Cast(val_listLitData);
        for(int j=0; j < array_litData->Length(); j++)
        {
            v8::Local<v8::Value> val_litData = array_litData->Get(j);
            v8::Local<v8::Object> obj_litData = val_litData->ToObject();

            v8::String::Utf8Value litValIfFalseStr(obj_litData->Get(v8::String::New("valueIfFalse"))->ToString());
            v8::String::Utf8Value litValIfTrueStr(obj_litData->Get(v8::String::New("valueIfTrue"))->ToString());
            v8::String::Utf8Value litPropertyStr(obj_litData->Get(v8::String::New("property"))->ToString());

            obdref::LiteralData myLitData;
            myLitData.value = obj_litData->Get(v8::String::New("value"))->BooleanValue();
            myLitData.valueIfFalse = QString(*litValIfFalseStr);
            myLitData.valueIfTrue = QString(*litValIfTrueStr);
            myLitData.property = QString(*litPropertyStr);
            myData.listLiteralData.append(myLitData);
        }
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
