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
            OBDREFDEBUG << "OBDREF: Error: XML [" << filePath << "] errors!\n";

            OBDREFDEBUG << "OBDREF: Error: "
                        << QString::fromStdString(xmlParseResult.description()) << "\n";

            OBDREFDEBUG << "OBDREF: Error: Offset Char: "
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

                                            reqHeaderBytes << ubyte(upperByte);
                                            reqHeaderBytes << ubyte(lowerByte);
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
                                        ByteList &expHeaderBytes =
                                                msgFrame.listMessageData[0].expHeaderBytes;

                                        ByteList &expHeaderMask =
                                                msgFrame.listMessageData[0].expHeaderMask;

                                        QString hIdentifier(nodeResp.attribute("identifier").value());
                                        if(!hIdentifier.isEmpty())
                                        {
                                            bool convOk = false;
                                            uint headerVal = stringToUInt(convOk,hIdentifier);
                                            uint upperByte = (headerVal & 0xF00) >> 8;
                                            uint lowerByte = headerVal & 0xFF;

                                            expHeaderBytes << ubyte(upperByte) << ubyte(lowerByte);
                                            expHeaderMask  << ubyte(0xFF) << ubyte(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes << ubyte(0x00) << ubyte(0x00);
                                            expHeaderMask  << ubyte(0x00) << ubyte(0x00);
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
                                        reqHeaderBytes << char(prioByte);

                                        uint formatByte = stringToUInt(convOk,headerFormat);
                                        reqHeaderBytes << char(formatByte);

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        reqHeaderBytes << char(targetByte);

                                        uint sourceByte = stringToUInt(convOk,headerSource);
                                        reqHeaderBytes << char(sourceByte);
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
                                            expHeaderBytes << char(prioByte);
                                            expHeaderMask << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes << char(0x00);
                                            expHeaderMask << char(0x00);
                                        }
                                        // format byte
                                        if(!headerFormat.isEmpty())   {
                                            uint formatByte = stringToUInt(convOk,headerFormat);
                                            expHeaderBytes << char(formatByte);
                                            expHeaderMask << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes << char(0x00);
                                            expHeaderMask << char(0x00);
                                        }
                                        // target byte
                                        if(!headerTarget.isEmpty())   {
                                            uint targetByte = stringToUInt(convOk,headerTarget);
                                            expHeaderBytes << char(targetByte);
                                            expHeaderMask << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes << char(0x00);
                                            expHeaderMask << char(0x00);
                                        }
                                        // source byte
                                        if(!headerSource.isEmpty())   {
                                            uint sourceByte = stringToUInt(convOk,headerSource);
                                            expHeaderBytes << (sourceByte);
                                            expHeaderMask << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes << char(0x00);
                                            expHeaderMask << char(0x00);
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
                                        reqHeaderBytes << char(formatByte);

                                        // target byte
                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        reqHeaderBytes << char(targetByte);

                                        // source byte
                                        uint sourceByte = stringToUInt(convOk,headerSource);
                                        reqHeaderBytes << char(sourceByte);

                                        // length byte
                                        if(!headerLength.isEmpty())   {
                                            uint lengthByte = stringToUInt(convOk,headerLength);
                                            reqHeaderBytes << char(lengthByte);
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
                                            expHeaderBytes << char(formatByte);
                                            expHeaderMask << char(0xC0);
                                            // note: the mask for the formatByte is set to
                                            // 0b11000000 (0xC0) to ignore the length bits
                                        }
                                        else   {
                                            expHeaderBytes << char(0x00);
                                            expHeaderMask << char(0x00);
                                        }
                                        // target byte
                                        if(!headerTarget.isEmpty())   {
                                            uint targetByte = stringToUInt(convOk,headerTarget);
                                            expHeaderBytes << char(targetByte);
                                            expHeaderMask << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes << char(0x00);
                                            expHeaderMask << char(0x00);
                                        }
                                        // source byte
                                        if(!headerSource.isEmpty())   {
                                            uint sourceByte = stringToUInt(convOk,headerSource);
                                            expHeaderBytes << char(sourceByte);
                                            expHeaderMask << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes << char(0x00);
                                            expHeaderMask << char(0x00);
                                        }

                                        // length byte is ignored for expResponse
                                        expHeaderBytes << char(0x00);
                                        expHeaderMask << char(0x00);
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
                                        reqHeaderBytes << char(prioByte);

                                        uint targetByte = stringToUInt(convOk,headerTarget);
                                        reqHeaderBytes << char(targetByte);

                                        uint sourceByte = stringToUInt(convOk,headerSource);
                                        reqHeaderBytes << char(sourceByte);
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
                                            expHeaderBytes << char(prioByte);
                                            expHeaderMask << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes << char(0x00);
                                            expHeaderMask << char(0x00);
                                        }
                                        // target byte
                                        if(!headerTarget.isEmpty())   {
                                            uint targetByte = stringToUInt(convOk,headerTarget);
                                            expHeaderBytes << char(targetByte);
                                            expHeaderMask << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes << char(0x00);
                                            expHeaderMask << char(0x00);
                                        }
                                        // source byte
                                        if(!headerSource.isEmpty())   {
                                            uint sourceByte = stringToUInt(convOk,headerSource);
                                            expHeaderBytes << char(sourceByte);
                                            expHeaderMask << char(0xFF);
                                        }
                                        else   {
                                            expHeaderBytes << char(0x00);
                                            expHeaderMask << char(0x00);
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

                                            ByteList reqDataBytes;
                                            msgData->listReqDataBytes << reqDataBytes;
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
                                            msgData->expDataPrefix << char(dataByte);
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
                                                msgData->listReqDataBytes[0] << char(dataByte);
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

                                // copy over header data from first message in list
                                // to other entries; only relevant for multi-frame
                                MessageData const &firstMsg = msgFrame.listMessageData[0];
                                for(size_t i=1; i < msgFrame.listMessageData.size(); i++)   {
                                    MessageData &nextMsg = msgFrame.listMessageData[i];
                                    nextMsg.reqHeaderBytes = firstMsg.reqHeaderBytes;
                                    nextMsg.expHeaderBytes = firstMsg.expHeaderBytes;
                                    nextMsg.expHeaderMask = firstMsg.expHeaderMask;
                                }

                                // ISO 15765-4:
                                if(protocolName.contains("ISO 15765-4"))
                                {
                                    for(size_t i=0; i < msgFrame.listMessageData.size(); i++)
                                    {
                                        QList<ByteList> &listReqDataBytes =
                                                msgFrame.listMessageData[i].listReqDataBytes;

                                        uint payloadSize = listReqDataBytes[0].size();

                                        if(msgFrame.iso15765_splitReqIntoFrames &&
                                                (payloadSize > 7))
                                        {
                                            ByteList emptyByteList;

                                            // first frame
                                            ubyte idxFrame = 0;
                                            ubyte idxBeg = 5;   // FF hold 6 data bytes

                                            // truncate FF and copy to subsequent frame
                                            listReqDataBytes << emptyByteList;
                                            for(size_t j=idxBeg; j < listReqDataBytes[idxFrame].size(); j++)   {
                                                ubyte dataByte = listReqDataBytes[idxFrame].takeAt(idxBeg);
                                                listReqDataBytes[idxFrame+1] << dataByte;
                                            }

                                            idxFrame++;
                                            idxBeg = 6;         // CF hold 7 data bytes

                                            // consecutive frames
                                            while(1)   {
                                                // check if we're done
                                                if(listReqDataBytes[idxFrame].size() <= 7)   {
                                                    break;
                                                }

                                                // truncate CF and copy to subsequent frame
                                                listReqDataBytes << emptyByteList;
                                                for(size_t j=idxBeg; j < listReqDataBytes[idxFrame].size(); j++)   {
                                                    ubyte dataByte = listReqDataBytes[idxFrame].takeAt(idxBeg);
                                                    listReqDataBytes[idxFrame+1] << dataByte;
                                                }
                                                idxFrame++;
                                            }
                                        }

                                        if(msgFrame.iso15765_addPciByte)
                                        {
                                            // [single frame]
                                            // higher 4 bits set to 0 to indicate single frame
                                            // lower 4 bits gives the number of bytes in payload
                                            if(listReqDataBytes.size() == 1)   {
                                                ubyte pciByte = listReqDataBytes[0].size();
                                                listReqDataBytes[0].prepend(pciByte);
                                            }
                                            // [multi frame]
                                            else   {
                                                // first frame
                                                // higher 4 bits set to 0001
                                                // lower 12 bits give number of bytes in payload
                                                ubyte lowerByte = (payloadSize & 0x0FF);
                                                ubyte upperByte = (payloadSize & 0xF00) >> 8;
                                                upperByte += 16;    // += 0b1000

                                                listReqDataBytes[0].prepend(lowerByte);
                                                listReqDataBytes[0].prepend(upperByte);

                                                // consecutive frames
                                                for(size_t j=1; j < listReqDataBytes.size(); j++)   {
                                                    ubyte pciByte = 0x20 + (j % 0x10);      // cycle 0x20-0x2F
                                                                                            // starting w 0x21
                                                    listReqDataBytes[j].prepend(pciByte);
                                                }
                                            }
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
                                        ubyte hSize = msgData.reqHeaderBytes.size();
                                        ubyte dSize = msgData.listReqDataBytes[0].size();

                                        if(dSize > 255)   {
                                            OBDREFDEBUG << "OBDREF: Error: ISO 14230-4,"
                                                        << " Invalid Data Length";
                                            return false;
                                        }

                                        // if the header is two or four bytes
                                        // add data length to the last byte
                                        if(hSize % 2 == 0)   {
                                            msgData.reqHeaderBytes[hSize-1] = dSize;
                                        }

                                        // else if the header is one or three bytes
                                        else   {
                                            if(dSize > 63)   {
                                                // or append a length byte if its > 63 bytes
                                                msgData.reqHeaderBytes << dSize;
                                            }
                                            else    {
                                                // add data length to six least significant
                                                // bits of the first (format) byte
                                                ubyte formatByte = msgData.reqHeaderBytes[0];
                                                msgData.reqHeaderBytes[0] = formatByte | dSize;
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

    bool Parser::ParseMessageFrame(MessageFrame &msgFrame,
                                   QList<obdref::Data> &listData)
    {
        if(msgFrame.parseScript.isEmpty())
        {
            OBDREFDEBUG << "OBDREF: Error: No parse info "
                        << "found in message frame\n";
            return false;
        }

        // clean up response data based on protocol type
        bool formatOk = false;  

        if(msgFrame.protocol.contains("ISO 15765-4"))   {
            formatOk = cleanRawData_ISO_15765_4(msgFrame);
        }
        else if(msgFrame.protocol == "ISO 14230-4")   {
            formatOk = cleanRawData_ISO_14230_4(msgFrame);
        }
        else   {
            formatOk = cleanRawData_Legacy(msgFrame);
        }

        if(!formatOk)   {
            OBDREFDEBUG << "OBDREF: Error: Could not clean"
                        << "raw data using spec'd format\n";
            return false;
        }

        bool parseOk = false;
        parseOk = parseResponse(msgFrame,listData);

        if(!parseOk)   {
            OBDREFDEBUG << "OBDREF: Error: Could not parse message";
            return false;
        }

        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    QByteArray Parser::ConvValToHexByte(ubyte myVal)
    {   return m_mapValToHexByte.value(myVal);   }

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

    bool Parser::cleanRawData_Legacy(MessageFrame &msgFrame)
    {
        bool hasData = false;
        for(size_t i=0; i < msgFrame.listMessageData.size(); i++)
        {
            QList<ByteList> &listRawDataFrames =
                    msgFrame.listMessageData[i].listRawDataFrames;

            ByteList &expHeaderBytes =
                    msgFrame.listMessageData[i].expHeaderBytes;

            ByteList &expHeaderMask =
                    msgFrame.listMessageData[i].expHeaderMask;

            ByteList const &expDataPrefix =
                    msgFrame.listMessageData[i].expDataPrefix;

            for(size_t j=0; j < listRawDataFrames.size(); j++)
            {
                ByteList &rawFrame = listRawDataFrames[j];
                size_t numHeaderBytes = 3;

                // split raw data into header/data bytes
                ByteList headerBytes,dataBytes;
                for(size_t k=0; k < rawFrame.size(); k++)   {
                    if(k < numHeaderBytes)   {   headerBytes << rawFrame.at(k);   }
                    else                     {   dataBytes << rawFrame.at(k);   }
                }

                // check for expHeaderBytes if required
                if(expHeaderBytes.size() > 0)
                {
                    bool headerBytesMatch = true;
                    for(size_t k=0; k < numHeaderBytes; k++)
                    {
                        ubyte byteTest = headerBytes.at(k) &
                                         expHeaderMask.at(k);

                        ubyte byteMask = expHeaderBytes.at(k) &
                                         expHeaderMask.at(k);

                        if(byteMask != byteTest)   {
                            headerBytesMatch = false;
                            break;
                        }
                    }

                    if(!headerBytesMatch)
                    {
                        OBDREFDEBUG << "OBDREF: Warn: SAE J1850/ISO 9141-2, "
                                       "header bytes mismatch";
                        continue;
                    }
                }

                // check for expected data prefix bytes
                bool dataPrefixMatch = true;
                for(size_t k=0; k < expDataPrefix.size(); k++)
                {
                    if(expDataPrefix.at(k) != dataBytes.at(k))   {
                        dataPrefixMatch = false;
                        break;
                    }
                }

                if(!dataPrefixMatch)
                {
                    OBDREFDEBUG << "OBDREF: Warn: SAE J1850/ISO 9141-2, "
                                   "data prefix mismatch";
                    continue;
                }

                // remove data prefix bytes
                for(size_t k=0; k < expDataPrefix.size(); k++)   {
                    dataBytes.removeAt(0);
                }

                // save data
                msgFrame.listMessageData[i].listHeaders << headerBytes;
                msgFrame.listMessageData[i].listCleanData << dataBytes;
            }

            // merge nonunique listHeaders and cat
            // corresponding listCleanData entries
            if(listRawDataFrames.size() > 1)
            {
                groupDataByHeader(msgFrame.listMessageData[i].listHeaders,
                                  msgFrame.listMessageData[i].listCleanData);
            }

            if(msgFrame.listMessageData[i].listHeaders.size() > 0)
            {   hasData = true;   }
        }
        if(!hasData)
        {
            OBDREFDEBUG << "OBDREF: Error: SAE J1850/ISO 9141-2, "
                           "empty message data after cleaning";
            return false;
        }
        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::cleanRawData_ISO_14230_4(MessageFrame &msgFrame)
    {
        bool hasData = false;
        for(size_t i=0; i < msgFrame.listMessageData.size(); i++)
        {
            QList<ByteList> &listRawDataFrames =
                    msgFrame.listMessageData[i].listRawDataFrames;

            ByteList &msgDataExpHeaderBytes =
                    msgFrame.listMessageData[i].expHeaderBytes;

            ByteList &msgDataExpHeaderMask =
                    msgFrame.listMessageData[i].expHeaderMask;

            ByteList const &msgDataExpPrefix =
                    msgFrame.listMessageData[i].expDataPrefix;

            for(size_t j=0; j < listRawDataFrames.size(); j++)
            {
                ByteList &rawFrame = listRawDataFrames[j];
                if(rawFrame.size() > 259)   {
                    // message length can't exceed
                    // 4 header bytes + 255 data bytes
                    OBDREFDEBUG << "OBDREF: Error: ISO 14230-4 "
                                   "Message exceeds 259 bytes";
                    continue;
                }

                // first we need to determine the number of bytes
                // in the header since there are four header types:
                // A [format]
                // B [format] [target] [source]
                // C [format] [length]
                // D [format] [target] [source] [length]

                // we know that the header format is C or D if the six
                // least significant bits of the format byte are 0
                uint numHeaderBytes = 0;
                ubyte hFormatMask = 0x3F;   // 0b00111111
                ubyte dataLength = rawFrame.at(0) & hFormatMask;

                if(dataLength > 0)
                {   // frame is A or B
                    numHeaderBytes = rawFrame.size() - dataLength;
                }
                else
                {   // frame is C or D
                    // ambiguous so guess and check each type
                    dataLength = rawFrame.at(3);
                    if(dataLength + 4 == rawFrame.size())   {
                        numHeaderBytes = 4;     // frame D
                    }
                    else   {
                        dataLength = rawFrame.at(1);
                        numHeaderBytes = 2;     // frame C
                    }
                }

                // split raw data into header/data bytes
                ByteList headerBytes,dataBytes;
                for(size_t k=0; k < rawFrame.size(); k++)   {
                    if(k < numHeaderBytes)   {   headerBytes << rawFrame.at(k);   }
                    else                     {   dataBytes << rawFrame.at(k);   }
                }

                // filter with expHeaderBytes if required
                if(msgDataExpHeaderBytes.size() > 0)
                {
                    // create expHeaderBytes and expHeaderMask to match
                    // the correct numHeaderBytes (when building the
                    // message, we pad expHeader[] to assume the largest
                    // num of header bytes to preserve byte order)
                    ByteList expHeaderBytes,expHeaderMask;
                    expHeaderBytes << msgDataExpHeaderBytes.at(0);
                    expHeaderMask  << msgDataExpHeaderMask.at(0);

                    switch(numHeaderBytes)   {
                        case 1:   {   // A
                            break;
                        }
                        case 2:   {   // C
                            expHeaderBytes << msgDataExpHeaderBytes.at(3);
                            expHeaderMask  << msgDataExpHeaderMask.at(3);
                            break;
                        }
                        case 3:   {   // B
                            expHeaderBytes << msgDataExpHeaderBytes.at(1)
                                           << msgDataExpHeaderBytes.at(2);
                            expHeaderMask  << msgDataExpHeaderMask.at(1)
                                           << msgDataExpHeaderMask.at(2);
                            break;
                        }
                        case 4:   {   // D
                            expHeaderBytes << msgDataExpHeaderBytes.at(1)
                                           << msgDataExpHeaderBytes.at(2)
                                           << msgDataExpHeaderBytes.at(3);
                            expHeaderMask  << msgDataExpHeaderMask.at(1)
                                           << msgDataExpHeaderMask.at(2)
                                           << msgDataExpHeaderMask.at(3);
                            break;
                        }
                    }

                    // check for expected header bytes
                    bool headerBytesMatch = true;
                    for(size_t k=0; k < numHeaderBytes; k++)
                    {
                        ubyte byteTest = headerBytes.at(k) &
                                         expHeaderMask.at(k);

                        ubyte byteMask = expHeaderBytes.at(k) &
                                         expHeaderMask.at(k);

                        if(byteMask != byteTest)   {
                            headerBytesMatch = false;
                            break;
                        }
                    }

                    if(!headerBytesMatch)
                    {
                        OBDREFDEBUG << "OBDREF: Warn: ISO 14230-4, "
                                       "header bytes mismatch";
                        continue;
                    }
                }

                // check for expected data prefix bytes
                bool dataPrefixMatch = true;
                for(size_t k=0; k < msgDataExpPrefix.size(); k++)
                {
                    if(msgDataExpPrefix.at(k) != dataBytes.at(k))   {
                        dataPrefixMatch = false;
                        break;
                    }
                }

                if(!dataPrefixMatch)
                {
                    OBDREFDEBUG << "OBDREF: Warn: ISO 14230-4, "
                                   "data prefix mismatch";
                    continue;
                }

                // remove data prefix bytes
                for(size_t k=0; k < msgDataExpPrefix.size(); k++)   {
                    dataBytes.removeAt(0);
                }

                // save data
                msgFrame.listMessageData[i].listHeaders << headerBytes;
                msgFrame.listMessageData[i].listCleanData << dataBytes;
            }

            // merge nonunique listHeaders and cat
            // corresponding listCleanData entries

            if(listRawDataFrames.size() > 1)   {

                QList<ByteList> &listHeaders =
                    msgFrame.listMessageData[i].listHeaders;

                QList<ByteList> &listCleanData =
                    msgFrame.listMessageData[i].listCleanData;

                groupDataByHeader(listHeaders,listCleanData);
            }

            if(msgFrame.listMessageData[i].listHeaders.size() > 0)
            {   hasData = true;   }
        }
        if(!hasData)
        {
            OBDREFDEBUG << "OBDREF: Error: ISO 14230-4, "
                           "empty message data after cleaning";
            return false;
        }
        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::cleanRawData_ISO_15765_4(MessageFrame &msgFrame)
    {
        bool hasData = false;
        for(size_t i=0; i < msgFrame.listMessageData.size(); i++)
        {
            QList<ByteList> &listRawDataFrames =
                    msgFrame.listMessageData[i].listRawDataFrames;

            ByteList const &expHeaderBytes =
                    msgFrame.listMessageData[i].expHeaderBytes;

            ByteList const &expHeaderMask =
                    msgFrame.listMessageData[i].expHeaderMask;

            ByteList const &expDataPrefix =
                    msgFrame.listMessageData[i].expDataPrefix;

            ubyte numHeaderBytes;
            if(msgFrame.protocol.contains("Standard"))
            {   numHeaderBytes = 2;   }
            else
            {   numHeaderBytes = 4;   }

            for(size_t j=0; j < listRawDataFrames.size(); j++)
            {
                ByteList &rawFrame = listRawDataFrames[j];

                // split raw data into header/data bytes
                ByteList headerBytes,dataBytes;
                for(size_t k=0; k < rawFrame.size(); k++)   {
                    if(k < numHeaderBytes)   {   headerBytes << rawFrame.at(k);   }
                    else                     {   dataBytes << rawFrame.at(k);   }
                }

                // check for expHeaderBytes if required
                if(expHeaderBytes.size() > 0)
                {
                    bool headerBytesMatch = true;
                    for(size_t k=0; k < numHeaderBytes; k++)
                    {
                        ubyte byteTest = headerBytes.at(k) &
                                         expHeaderMask.at(k);

                        ubyte byteMask = expHeaderBytes.at(k) &
                                         expHeaderMask.at(k);

                        if(byteMask != byteTest)   {
                            headerBytesMatch = false;
                            break;
                        }
                    }

                    if(!headerBytesMatch)
                    {
                        OBDREFDEBUG << "OBDREF: Warn: ISO 15765-4, "
                                       "header bytes mismatch";
                        continue;
                    }
                }

                // check for expected data prefix bytes
                // note: have to take pci byte(s) into account
                bool dataPrefixMatch = true;
                ubyte pciOffset = ((dataBytes[0] >> 4) == 1) ? 2 : 1;
                for(size_t k=0; k < expDataPrefix.size(); k++)
                {
                    if(expDataPrefix.at(k) != dataBytes.at(k+pciOffset))   {
                        dataPrefixMatch = false;
                        break;
                    }
                }

                if(!dataPrefixMatch)
                {
                    OBDREFDEBUG << "OBDREF: Warn: ISO 15765-4, "
                                   "data prefix mismatch";
                    continue;
                }

                // save data
                msgFrame.listMessageData[i].listHeaders << headerBytes;
                msgFrame.listMessageData[i].listCleanData << dataBytes;
            }

            if(listRawDataFrames.size() == 1)   {

                // with only one frame, ensure we have
                // a single frame pci byte

                ByteList &dataBytes =
                    msgFrame.listMessageData[i].listCleanData[0];

                ubyte pciByte = dataBytes.takeAt(0);
                if((pciByte >> 4) != 0)   {
                    OBDREFDEBUG << "OBDREF: Error: ISO 15765, "
                                   "invalid PCI byte for single frame";
                    return false;
                }

                // remove data prefix bytes
                for(size_t j=0; j < expDataPrefix.size(); j++)
                {   dataBytes.removeFirst();   }
            }
            else
            {
                QList<ByteList> &listHeaderFields =
                    msgFrame.listMessageData[i].listHeaders;

                QList<ByteList> &listDataFields =
                    msgFrame.listMessageData[i].listCleanData;

                // [single frame]
                QList<ByteList> listSFHeaders;
                QList<ByteList> listSFDataBytes;
                for(size_t j=0; j < listHeaderFields.size(); j++)   {
                    // check pci byte is sf
                    ubyte pciByte = listDataFields[j][0];
                    if((pciByte >> 4) == 0)   {

                        // remove pci byte
                        listDataFields[j].removeFirst();

                        // remove data prefix bytes
                        for(size_t k=0; k < expDataPrefix.size(); k++)
                        {   listDataFields[j].removeFirst();   }

                        // save copy and remove original
                        listSFHeaders << listHeaderFields[j];
                        listSFDataBytes << listDataFields[j];
                        listHeaderFields.removeAt(j);
                        listDataFields.removeAt(j);
                        j--;
                    }
                }
                groupDataByHeader(listSFHeaders,listSFDataBytes);


                // [multi frame]
                // map all messages to unique headers
                QList<ByteList>         listUnqHeaderFields;
                QList<QList<ByteList> >  listListDataFields;

                for(size_t j=0; j < listHeaderFields.size(); j++)
                {
                    QList<ByteList> listMappedDataFields;

                    listUnqHeaderFields << listHeaderFields.takeAt(j);
                    listMappedDataFields << listDataFields.takeAt(j);
                    j--;

                    for(size_t k=0; k < listHeaderFields.size(); k++)   {
                        if(listUnqHeaderFields.last() == listHeaderFields[k])   {
                            listMappedDataFields << listDataFields.takeAt(k);
                            listHeaderFields.removeAt(k);
                            k--;
                        }
                    }
                    listListDataFields << listMappedDataFields;
                }

                // sort databytes mapped by header according
                // to ISO 15765-4 pci byte order for multi-frame
                QList<ByteList> listMFHeaders;
                QList<ByteList> listMFDataBytes;
                for(size_t j=0; j < listUnqHeaderFields.size(); j++)
                {
                    QList<ByteList> &listDataFields = listListDataFields[j];
                    for(size_t k=0; k < listDataFields.size(); k++)
                    {
                        ubyte pciMask = (k == 0) ? 0xF0 : 0xFF;
                        ubyte pciByte = (k == 0) ? 0x10 : 0x20;
                        pciByte += (k % 0x10);      // cycle 0-F

                        // reorganize list of data fields using pciByte
                        // ISO 15765-4 MF pci byte order is:
                        // [first frame]        0x1D 0xDD where DDD == data length
                        // [consecutive frame]  0x21,0x22... 0x2F,0x20, (repeat)

                        for(size_t m=k; m < listDataFields.size(); m++)
                        {
                            if((listDataFields[m][0] & pciMask) == pciByte)   {
                                if(m != k)   {
                                    // move idx 'm' to idx 'k'
                                    listDataFields.move(m,k);
                                    // note: can optimize: directly copy
                                    // data to catDataBytes instead of
                                    // reorganizing this list,etc
                                }
                                break;
                            }
                        }

                        // remove pciByte from dataBytes
                        listDataFields[k].removeFirst();
                        if(k == 0)   {
                            // if [first frame], remove the
                            // data length byte as well
                            listDataFields[k].removeFirst();
                        }

                        // remove data prefix bytes
                        for(size_t m=0; m < expDataPrefix.size(); m++)
                        {   listDataFields[k].removeFirst();   }
                    }

                    ByteList catDataBytes;
                    for(size_t k=0; k < listDataFields.size(); k++)
                    {   catDataBytes << listDataFields[k];   }

                    // save
                    listMFHeaders << listUnqHeaderFields[j];
                    listMFDataBytes << catDataBytes;
                }

                // copy over reformated data to msgFrame
                listHeaderFields.clear();
                listDataFields.clear();

                listHeaderFields << listMFHeaders;
                listDataFields << listMFDataBytes;

                listHeaderFields << listSFHeaders;
                listDataFields << listSFDataBytes;
            }

            if(msgFrame.listMessageData[i].listHeaders.size() > 0)
            {   hasData = true;   }
        }

        if(!hasData)
        {
            OBDREFDEBUG << "OBDREF: Error: ISO 15765-4, "
                           "empty message data after cleaning";
            return false;
        }
        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    void Parser::groupDataByHeader(QList<ByteList> &listHeaders,
                                   QList<ByteList> &listDataBytes)
    {
        // merge nonunique listHeaders and cat
        // corresponding listDataBytes entries

        for(size_t i=0; i < listHeaders.size(); i++)
        {
            for(size_t j=i+1; j < listHeaders.size(); j++)
            {
                if(listHeaders[i] == listHeaders[j])
                {   // merge headerBytes and cat dataBytes
                    listDataBytes[i] << listDataBytes[j];
                    listHeaders.removeAt(j);
                    listDataBytes.removeAt(j);
                    j--;
                }
            }
        }
    }

    // ========================================================================== //
    // ========================================================================== //

    void Parser::printCleanedData(MessageFrame &msgFrame)
    {
        OBDREFDEBUG << "OBDREF: Info: Cleaned Data:";
        for(size_t i=0; i < msgFrame.listMessageData.size(); i++)   {
            OBDREFDEBUG << "OBDREF: Info: Message " << i;

            QList<ByteList> const &listHeaders =
                msgFrame.listMessageData[i].listHeaders;

            QList<ByteList> const &listDataBytes =
                msgFrame.listMessageData[i].listCleanData;

            for(size_t j=0; j < listHeaders.size(); j++)   {
                OBDREFDEBUG <<listHeaders[j]
                            << "|" << listDataBytes[j];
            }
        }
    }

    // ========================================================================== //
    // ========================================================================== //

    void Parser::printRawData(const QList<ByteList> &listRawData)
    {
        for(int k=0; k < listRawData.size(); k++)
        {
            OBDREFDEBUG << "OBDREF: Raw Data Frame " << k << ": ";
            for(int l=0; l < listRawData[k].size(); l++)
            {   OBDREFDEBUG << m_mapValToHexByte.value(listRawData[k][l]) << " ";   }
            OBDREFDEBUG << "\n";
        }
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::parseResponse(const MessageFrame &msgFrame,
                               QList<Data> &listData,
                               ParseMode parseMode)
    {
        // todo: support multi-part message frames
        MessageData const &msgData = msgFrame.listMessageData[0];

        if(parseMode == PER_RESP_DATA)
        {   // PER_RESP_DATA:

            // * default parse mode -- the parse script is run
            //   once for each entry in MessageData.listCleanData

            // * the data is accessed using "BYTE(N)" in js where N
            //   is the Nth data byte in a listCleanData entry

            for(size_t i=0; i < msgData.listHeaders.size(); i++)
            {
                ByteList const &headerBytes = msgData.listHeaders[i];
                ByteList const &dataBytes = msgData.listCleanData[i];

                obdref::Data myData;

                // fill out meta data
                for(size_t j=0; j < headerBytes.size(); j++)   {
                    QString bStr = QString::number(int(headerBytes[j]),16);
                    myData.srcAddress.append(bStr);
                }

                myData.paramName    = msgFrame.name;
                myData.srcName      = msgFrame.address;
                myData.srcAddress   = myData.srcAddress.toUpper();


                // create v8 local scope handle
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

                // copy over databytes to js context
                v8::Local<v8::Array> dataByteArray = v8::Array::New(dataBytes.size());
                for(int j=0; j < dataBytes.size(); j++)
                {   dataByteArray->Set(j,v8::Integer::New(int(dataBytes.at(j))));   }

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
                listData.append(myData);
            }

            return true;
        }
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

    void Parser::stringToBaseDec(QString &parseExpr)
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
            OBDREFDEBUG << "OBDREF: Could not open file: "
                        << filePath << "\n";
            return false;
        }

        fileDataAsStr = QString::fromStdString(contentAsString);
        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    /*
    bool Parser::parseSinglePartResponse(const MessageFrame &msgFrame,
                                         QList<Data> &listDataResults)
    {
        // DEPRECATED
        // [single part message]

        // * typical response type
        // * a single part message is identified by having
        //   only one request message in the MessageFrame
        // * we interpret multiple device responses as
        //   individual responses to that single request
        //   from different sources

        for(int i=0; i < msgFrame.listMessageData.at(0).listCleanData.size(); i++)
        {
            ByteList const & headerBytes =
                    msgFrame.listMessageData.at(0).listHeaders.at(i);

            ByteList const & dataBytes =
                    msgFrame.listMessageData.at(0).listCleanData.at(i);

            obdref::Data myData;

            // fill out response meta data
            for(size_t j=0; j < headerBytes.size(); j++)   {
                myData.srcAddress.append(QString::number(int(headerBytes[j]),16));
            }
            myData.paramName = msgFrame.name;
            myData.srcName = msgFrame.address;
            myData.srcAddress = myData.srcAddress.toUpper();

            // create scope for local handles
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

            // copy over databytes to js context
            v8::Local<v8::Array> dataByteArray = v8::Array::New(dataBytes.size());
            for(int j=0; j < dataBytes.size(); j++)
            {   dataByteArray->Set(j,v8::Integer::New(int(dataBytes.at(j))));   }

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
        // DEPRECATED
        // [multi part request]
        // * a multi part message is identified by having
        //   multiple request messages in the MessageFrame
        // * we interpret multiple device responses as
        //   forming a single response from a single source

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

        for(int j=0; j < headerBytes.size(); j++)
        {   myData.srcAddress.append(QString::number(int(headerBytes.at(j)),16));   }

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
            v8::Local<v8::Array> dataByteArray = v8::Array::New(dataBytes.size());
            for(int j=0; j < dataBytes.size(); j++)
            {   dataByteArray->Set(j,v8::Integer::New(int(dataBytes.at(j))));   }

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
    */
}
