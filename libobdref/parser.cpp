/*
   This source is part of libobdref

   Copyright (C) 2012,2013 Preet Desai (preet.desai@gmail.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "parser.h"
#include "globals_js.h"

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
            hexByteStr = hexByteStr.toUpper();

            // zero pad to two digits for a byte
            if(hexByteStr.size() < 2)
            {   hexByteStr.prepend("0");   }

            m_mapUByteToHexStr.insert(ubyte(i),hexByteStr);
            m_mapHexStrToUByte.insert(hexByteStr,ubyte(i));
        }

        // setup pugixml with xml source file
        m_xmlFilePath = filePath;
        QString xmlFileContents;
        if(!convTextFileToQStr(m_xmlFilePath,xmlFileContents))   {
            initOk = false;
            return;
        }

        pugi::xml_parse_result xmlParseResult;
        xmlParseResult = m_xmlDoc.load(xmlFileContents.toLocal8Bit().data());

        if(xmlParseResult)
        {   initOk = true;   }
        else
        {
            OBDREFDEBUG << "Error: XML [" << filePath << "] errors!\n";

            OBDREFDEBUG << "Error: "
                        << QString::fromStdString(xmlParseResult.description()) << "\n";

            OBDREFDEBUG << "Error: Offset Char: "
                        << qint64(xmlParseResult.offset) << "\n";

            initOk = false;
            return;
        }

        // setup js context
        if(!jsInit())   {
            OBDREFDEBUG << "Error: failed to setup JS engine";
            initOk = false;
            return;
        }
    }



    Parser::~Parser()
    {

    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::BuildParameterFrame(ParameterFrame &paramFrame)
    {
        bool xnSpecFound       = false;
        bool xnProtocolFound   = false;
        bool xnAddressFound    = false;
        bool xnParamsFound     = false;
        bool xnParameterFound  = false;

        pugi::xml_node xnSpec = m_xmlDoc.child("spec");
        for(; xnSpec!=NULL; xnSpec=xnSpec.next_sibling("spec"))
        {
            QString const spec(xnSpec.attribute("name").value());
            if(spec == paramFrame.spec)
            {   // found spec
                xnSpecFound = true;
                pugi::xml_node xnProtocol = xnSpec.child("protocol");
                for(; xnProtocol!=NULL; xnProtocol=xnProtocol.next_sibling("protocol"))
                {
                    QString const protocol(xnProtocol.attribute("name").value());
                    if(protocol == paramFrame.protocol)
                    {   // found protocol
                        xnProtocolFound = true;

                        QStringList listOptNames; QList<bool> listOptValues;
                        pugi::xml_node xnOption = xnProtocol.child("option");
                        for(; xnOption!=NULL; xnOption=xnOption.next_sibling("option"))
                        {
                            QString const optName(xnOption.attribute("name").value());
                            QString const optValue(xnOption.attribute("value").value());
                            if(!optName.isEmpty())   {
                                listOptNames.push_back(optName);
                                listOptValues.push_back(false);
                                if(optValue == "true")   {
                                    listOptValues.last() = true;
                                }
                            }
                        }

                        // set actual protocol used to clean up raw message data
                        int optIdx;
                        if(protocol.contains("SAE J1850"))   {
                            paramFrame.parseProtocol = PROTOCOL_SAE_J1850;
                        }
                        else if(protocol == "ISO 9141-2")   {
                            paramFrame.parseProtocol = PROTOCOL_ISO_9141_2;
                        }
                        else if(protocol == "ISO 14230")   {
                            paramFrame.parseProtocol = PROTOCOL_ISO_14230;

                            // check for options
                            optIdx = listOptNames.indexOf("Length Byte");
                            if(optIdx > -1) {
                                paramFrame.iso14230_addLengthByte = listOptValues[optIdx];
                            }
                        }
                        else if(protocol.contains("ISO 15765"))   {
                            paramFrame.parseProtocol = PROTOCOL_ISO_15765;

                            if(protocol.contains("Extended Id"))   {
                                paramFrame.iso15765_extendedId = true;
                            }

                            // check for options
                            optIdx = listOptNames.indexOf("Extended Address");
                            if(optIdx > -1)   {
                                paramFrame.iso15765_extendedAddr = listOptValues[optIdx];
                            }
                        }
                        else   {
                            OBDREFDEBUG << "ERROR: unsupported protocol: "
                                        << protocol;
                            return false;
                        }

                        // use address information to build the request header
                        pugi::xml_node xnAddress = xnProtocol.child("address");
                        for(; xnAddress!=NULL; xnAddress=xnAddress.next_sibling("address"))
                        {
                            QString const address(xnAddress.attribute("name").value());
                            if(address == paramFrame.address)
                            {   // found address
                                xnAddressFound = true;

                                // [build headers]
                                bool headerOk=false;
                                if(paramFrame.parseProtocol < 0xA00)   {
                                    headerOk = buildHeader_Legacy(paramFrame,xnAddress);
                                }
                                else if(paramFrame.parseProtocol == PROTOCOL_ISO_14230)   {
                                    headerOk = buildHeader_ISO_14230(paramFrame,xnAddress);
                                }
                                else if(paramFrame.parseProtocol == PROTOCOL_ISO_15765)   {
                                    headerOk = buildHeader_ISO_15765(paramFrame,xnAddress);
                                }

                                if(!headerOk)   {
                                    return false;
                                }
                            }
                        }
                    }
                }
                if(!xnAddressFound)   {
                    break;
                }

                pugi::xml_node xnParams = xnSpec.child("parameters");
                for(; xnParams!=NULL; xnParams=xnParams.next_sibling("parameters"))
                {
                    QString const paramsAddr(xnParams.attribute("address").value());
                    if(paramsAddr == paramFrame.address)
                    {   // found parameters group
                        xnParamsFound = true;

                        // use parameter information to build request data
                        pugi::xml_node xnParameter = xnParams.child("parameter");
                        for(; xnParameter!=NULL; xnParameter=xnParameter.next_sibling("parameter"))
                        {
                            QString const name(xnParameter.attribute("name").value());
                            if(name == paramFrame.name)
                            {   // found parameter
                                xnParameterFound = true;

                                // [build request data]
                                if(!buildData(paramFrame,xnParameter))   {
                                    OBDREFDEBUG << "Error: failed to build request data";
                                    return false;
                                }

                                // [save parse script]
                                // set parse mode
                                QString parseMode(xnParameter.attribute("parse").value());
                                if(parseMode == "combined")   {
                                    paramFrame.parseMode = PARSE_COMBINED;
                                }
                                else   {
                                    paramFrame.parseMode = PARSE_SEPARATELY;
                                }

                                // save reference to parse function
                                pugi::xml_node xnScript = xnParameter.child("script");
                                QString protocols(xnScript.attribute("protocols").value());
                                if(!protocols.isEmpty())   {
                                    bool foundProtocol=false;
                                    for(; xnScript!=NULL; xnScript = xnScript.next_sibling("script"))   {
                                        // get the script for the specified protocol
                                        protocols = QString(xnScript.attribute("protocols").value());
                                        if(protocols.contains(paramFrame.protocol))   {
                                            foundProtocol = true;
                                            break;
                                        }
                                    }
                                    if(!foundProtocol)   {
                                        OBDREFDEBUG << "Error: protocol specified not "
                                                       "found in parse script";
                                        return false;
                                    }
                                }

                                QString jsFunctionKey = paramFrame.spec+":"+paramFrame.address+":"+
                                                        paramFrame.name+":"+protocols;

                                paramFrame.functionKeyIdx = m_js_listFunctionKey.indexOf(jsFunctionKey);
                                if(paramFrame.functionKeyIdx == -1)   {
                                    OBDREFDEBUG << "No parse function found for "
                                                << "message: " << paramFrame.name << "\n";
                                    return false;
                                }
                                // done
                                return true;
                            }
                        }
                    }
                }
            }
        }

        if(!xnSpecFound)   {
            OBDREFDEBUG << "Error: could not find spec " << paramFrame.spec;
        }
        else if(!xnProtocolFound)   {
            OBDREFDEBUG << "Error: could not find protocol " << paramFrame.protocol;
        }
        else if(!xnAddressFound)    {
            OBDREFDEBUG << "Error: could not find address " << paramFrame.address;
        }
        else if(!xnParamsFound)   {
            OBDREFDEBUG << "Error: could not find param group";
        }
        else if(!xnParameterFound)   {
            OBDREFDEBUG << "Error: could not find parameter " << paramFrame.name;
        }

        return false;
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::ParseParameterFrame(ParameterFrame &msgFrame,
                                   QList<obdref::Data> &listData)
    {
        if(msgFrame.functionKeyIdx == -1)   {
            OBDREFDEBUG << "OBDREF: Error: Invalid parse"
                        << "function index in message frame\n";
            return false;
        }

        bool formatOk=true;

        // clean message data based on protocol type
        Protocol parseProtocol = msgFrame.parseProtocol;

        if(parseProtocol < 0xA00)
        {   // SAE_J1850, ISO_9141-2, ISO_14230-4
            for(int i=0; i < msgFrame.listMessageData.size(); i++)   {
                if(!cleanFrames_Legacy(msgFrame.listMessageData[i]))   {
                    formatOk=false;
                    break;
                }
            }
        }
        else if(parseProtocol == PROTOCOL_ISO_15765)   {
            int headerLength = (msgFrame.iso15765_extendedId) ? 4 : 2;
            for(int i=0; i < msgFrame.listMessageData.size(); i++)   {
                if(!cleanFrames_ISO_15765(msgFrame.listMessageData[i],
                                          headerLength))   {
                    formatOk=false;
                    break;
                }
            }
        }
        else if(parseProtocol == PROTOCOL_ISO_14230)   {
            for(int i=0; i < msgFrame.listMessageData.size(); i++)   {
                if(!cleanFrames_ISO_14230(msgFrame.listMessageData[i]))   {
                    formatOk=false;
                    break;
                }
            }
        }
        else   {
            OBDREFDEBUG << "ERROR: protocol not yet supported";
            return false;
        }


        if(!formatOk)   {
            OBDREFDEBUG << "OBDREF: Error: Could not clean"
                        << "raw data using spec'd format\n";
            return false;
        }

        // parse
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

    QStringList Parser::GetParameterNames(const QString &specName,
                                          const QString &protocolName,
                                          const QString &addressName)
    {
        bool foundAddress = false;
        QStringList myParamList;
        pugi::xml_node nodeSpec = m_xmlDoc.child("spec");

        for(; nodeSpec!=NULL; nodeSpec = nodeSpec.next_sibling("spec"))
        {
            QString fileSpecName(nodeSpec.attribute("name").value());
            if(fileSpecName == specName)
            {
                pugi::xml_node nodeProtocol = nodeSpec.child("protocol");
                for(; nodeProtocol!=NULL; nodeProtocol = nodeProtocol.next_sibling("protocol"))
                {
                    QString fileProtocolName(nodeProtocol.attribute("name").value());
                    if(fileProtocolName == protocolName)
                    {
                        pugi::xml_node nodeAddress = nodeProtocol.child("address");
                        for(; nodeAddress!=NULL; nodeAddress = nodeAddress.next_sibling("address"))
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
                for(; nodeParams!=NULL; nodeParams = nodeProtocol.next_sibling("parameters"))
                {
                    QString fileParamsName(nodeParams.attribute("address").value());
                    if(fileParamsName == addressName)
                    {
                        pugi::xml_node nodeParameter = nodeParams.child("parameter");
                        for(; nodeParameter!=NULL; nodeParameter = nodeParameter.next_sibling("parameter"))
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

    bool Parser::jsInit()
    {
        // create js heap and default context
        m_js_ctx = duk_create_heap_default();
        if(!m_js_ctx)   {
            OBDREFDEBUG << "ERROR: Could not create JS context";
            return false;
        }

        // push the global object onto the context's stack
        duk_push_global_object(m_js_ctx);
        m_js_idx_global_object = duk_normalize_index(m_js_ctx,-1);

        // register properties to the global object
        duk_eval_string(m_js_ctx,globals_js);
        duk_pop(m_js_ctx);

        // add important properties to the stack and
        // and save their location
        duk_get_prop_string(m_js_ctx,m_js_idx_global_object,
                            "__private_get_lit_data");

        duk_get_prop_string(m_js_ctx,m_js_idx_global_object,
                            "__private_get_num_data");

        duk_get_prop_string(m_js_ctx,m_js_idx_global_object,
                            "__private__clear_all_data");

        duk_get_prop_string(m_js_ctx,m_js_idx_global_object,
                            "__private__add_list_databytes");

        duk_get_prop_string(m_js_ctx,m_js_idx_global_object,
                            "__private__add_msg_data");

        m_js_idx_f_add_msg_data     = duk_normalize_index(m_js_ctx,-1);
        m_js_idx_f_add_databytes    = duk_normalize_index(m_js_ctx,-2);
        m_js_idx_f_clear_data       = duk_normalize_index(m_js_ctx,-3);
        m_js_idx_f_get_num_data     = duk_normalize_index(m_js_ctx,-4);
        m_js_idx_f_get_lit_data     = duk_normalize_index(m_js_ctx,-5);

        // evaluate and save all parse functions
        pugi::xml_node xnSpec = m_xmlDoc.child("spec");
        for(; xnSpec!=NULL; xnSpec=xnSpec.next_sibling("spec"))
        {   // for each spec
            QString spec(xnSpec.attribute("name").value());
            pugi::xml_node xnParams = xnSpec.child("parameters");
            for(; xnParams!=NULL; xnParams=xnParams.next_sibling("parameters"))
            {   // for each set of parameters
                QString address(xnParams.attribute("address").value());
                pugi::xml_node xnParam = xnParams.child("parameter");
                for(; xnParam!=NULL; xnParam=xnParam.next_sibling("parameter"))
                {   // for each parameter
                    QString param(xnParam.attribute("name").value());
                    pugi::xml_node xnScript = xnParam.child("script");
                    for(; xnScript!=NULL; xnScript=xnScript.next_sibling("script"))
                    {   // for each script
                        QString protocols(xnScript.attribute("protocols").value());
                        QString script(xnScript.child_value());

                        // add parse function name and scope
                        QString idx=QString::number(m_js_listFunctionKey.size(),10);
                        QString fname = "f"+idx;
                        script.prepend("function "+fname+"() {");
                        script.append("}");

                        // register parse function to global js object
                        duk_eval_string(m_js_ctx,script.toLocal8Bit().data());
                        duk_pop(m_js_ctx);

                        // add parse function to top of stack
                        duk_get_prop_string(m_js_ctx,m_js_idx_global_object,
                                            fname.toLocal8Bit().data());

                        // save unique key string and stack index for function
                        QString jsFunctionKey = spec+":"+address+":"+param+":"+protocols;
                        m_js_listFunctionKey.push_back(jsFunctionKey);
                        m_js_listFunctionIdx.push_back(duk_normalize_index(m_js_ctx,-1));
                    }
                }
            }
        }
        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::buildHeader_Legacy(ParameterFrame &paramFrame,
                                    pugi::xml_node xnAddress)
    {
        // store the request header data in
        // a MessageData struct
        MessageData msg;

        // request header bytes
        pugi::xml_node xnReq = xnAddress.child("request");
        if(xnReq)   {
            QString prio(xnReq.attribute("prio").value());
            QString target(xnReq.attribute("target").value());
            QString source(xnReq.attribute("source").value());

            // all three bytes must be defined
            if(prio.isEmpty() || target.isEmpty() || source.isEmpty())   {
                OBDREFDEBUG << "Error: ISO 9141-2/SAE J1850,"
                            << "Incomplete Request Header";
                return false;
            }

            bool okPrio,okTarget,okSource;
            msg.reqHeaderBytes << stringToUInt(okPrio,prio);
            msg.reqHeaderBytes << stringToUInt(okTarget,target);
            msg.reqHeaderBytes << stringToUInt(okSource,source);

            if(!(okPrio&&okTarget&&okSource))   {
                OBDREFDEBUG << "Error: ISO 9141-2/SAE J1850,"
                               "bad request header";
                return false;
            }
        }
        else   {
            // for messages without requests,
            // we use an empty header
            OBDREFDEBUG << "Warn: ISO 9141-2/SAE J1850,"
                        << "No Request Header";
        }

        // preemptively fill out response header
        // bytes (unspec'd bytes will be ignored
        // by expHeaderMask)
        msg.expHeaderBytes << 0x00 << 0x00 << 0x00;
        msg.expHeaderMask  << 0x00 << 0x00 << 0x00;

        // response header bytes
        pugi::xml_node xnResp = xnAddress.child("response");
        if(xnResp)   {
            QString prio(xnResp.attribute("prio").value());
            QString target(xnResp.attribute("target").value());
            QString source(xnResp.attribute("source").value());

            bool okPrio = true;
            bool okTarget = true;
            bool okSource = true;

            if(!prio.isEmpty())   {
                msg.expHeaderBytes[0] = stringToUInt(okPrio,prio);
                msg.expHeaderMask[0] = 0xFF;
            }
            if(!target.isEmpty())   {
                msg.expHeaderBytes[1] = stringToUInt(okTarget,target);
                msg.expHeaderMask[1] = 0xFF;
            }
            if(!source.isEmpty())   {
                msg.expHeaderBytes[2] = stringToUInt(okSource,source);
                msg.expHeaderMask[2] = 0xFF;
            }

            if(!(okPrio&&okTarget&&okSource))   {
                OBDREFDEBUG << "Error: ISO 9141-2/SAE J1850,"
                               "bad response header";
                return false;
            }
        }

        // save the MessageData
        paramFrame.listMessageData.push_back(msg);
        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::buildHeader_ISO_14230(ParameterFrame &paramFrame,
                                       pugi::xml_node xnAddress)
    {
        // ISO 14230
        // This protocol has a variable header:
        // A [format]
        // B [format] [target] [source] // ISO 14230-4, OBDII spec
        // C [format] [length]
        // D [format] [target] [source] [length]

        // ref: http://www.dgtech.com/product/dpa/manual/dpa_family_1_261.pdf

        // The format byte determines which header is used.
        // Assume the 8 bits of the format byte look like:
        // A1 A0 L5 L4 L3 L2 L1 L0

        // * If [A1 A0] == [0 0], [target] and [source]
        //   addresses are not included

        // * If [A1 A0] == [1 0], physical addressing is used

        // * If [A1 A0] == [0 1], functional addressing is used

        // * If [L0 to L5] are all 0, data length is specified
        //   in a separate length byte. Otherwise, the length
        //   is specified by [L0 to L5]

        // store the request header data in
        // a MessageData struct
        MessageData msg;

        // request header bytes
        pugi::xml_node xnReq = xnAddress.child("request");
        if(xnReq)   {
            QString format(xnReq.attribute("format").value());
            if(format.isEmpty())   {
                OBDREFDEBUG << "Error: ISO 14230, request "
                            << "header is missing format byte";
                return false;
            }

            bool convOk=false;
            ubyte formatByte = stringToUInt(convOk,format);
            msg.reqHeaderBytes << formatByte;

            if(!convOk)   {
                OBDREFDEBUG << "Error: ISO 14230, invalid "
                               "request header format byte";
                return false;
            }

            // check which type of header
            if((formatByte >> 6) != 0)   {
                // [source] and [target] must be present
                QString target(xnReq.attribute("target").value());
                QString source(xnReq.attribute("source").value());

                bool okTarget,okSource;
                msg.reqHeaderBytes << stringToUInt(okTarget,target);
                msg.reqHeaderBytes << stringToUInt(okSource,source);

                if(!(okTarget&&okSource))   {
                    OBDREFDEBUG << "Error: ISO 14230, invalid "
                                   "request source/target bytes";
                    return false;
                }
            }
        }
        else   {
            OBDREFDEBUG << "Warn: ISO 14230,"
                        << "no request header";
        }

        // preemptively fill out response header
        // bytes (unspec'd bytes will be ignored
        // by expHeaderMask)
        msg.expHeaderBytes << 0x00 << 0x00 << 0x00;
        msg.expHeaderMask  << 0x00 << 0x00 << 0x00;

        // response header bytes
        pugi::xml_node xnResp = xnAddress.child("response");
        if(xnResp)   {
            QString format(xnResp.attribute("format").value());
            QString target(xnResp.attribute("target").value());
            QString source(xnResp.attribute("source").value());

            bool okFormat = true;
            bool okTarget = true;
            bool okSource = true;

            if(!format.isEmpty())   {
                msg.expHeaderBytes[0] = stringToUInt(okFormat,format);
                msg.expHeaderMask[0] = 0xC0;
                // note: the mask for the formatByte is set to
                // 0b11000000 (0xC0) to ignore the length bits
            }
            if(!target.isEmpty())   {
                msg.expHeaderBytes[1] = stringToUInt(okTarget,target);
                msg.expHeaderMask[1] = 0xFF;
            }
            if(!source.isEmpty())   {
                msg.expHeaderBytes[2] = stringToUInt(okSource,source);
                msg.expHeaderMask[2] = 0xFF;
            }

            if(!(okFormat&&okTarget&&okSource))   {
                OBDREFDEBUG << "Error: ISO 14230,"
                               "invalid response header";
                return false;
            }
        }

        // save the MessageData
        paramFrame.listMessageData.push_back(msg);
        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::buildHeader_ISO_15765(ParameterFrame &paramFrame,
                                       pugi::xml_node xnAddress)
    {
        // store the request header data in
        // a MessageData struct
        MessageData msg;

        // ISO 15765-4 (11-bit standard id)
        // store 11-bit header in two bytes
        if(!paramFrame.iso15765_extendedId)   {
            // request header bytes
            pugi::xml_node xnReq = xnAddress.child("request");
            if(xnReq)   {
                QString identifier(xnReq.attribute("identifier").value());
                if(identifier.isEmpty())   {
                    OBDREFDEBUG << "Error: ISO 15765-4 std,"
                                << "Incomplete Request Header";
                    return false;
                }
                bool convOk = false;
                quint32 headerVal = stringToUInt(convOk,identifier);
                if(!convOk)   {
                    OBDREFDEBUG << "Error: ISO 15765 std, "
                                   "bad response header";
                    return false;
                }
                ubyte upperByte = ((headerVal & 0xF00) >> 8);
                ubyte lowerByte = (headerVal & 0xFF);
                msg.reqHeaderBytes << upperByte << lowerByte;
            }
            else   {
                OBDREFDEBUG << "Warn: ISO 15765 std, "
                               "no request header";
            }

            // preemptively fill out response header
            // bytes (unspec'd bytes will be ignored
            // by expHeaderMask)
            msg.expHeaderBytes << 0x00 << 0x00;
            msg.expHeaderMask  << 0x00 << 0x00;

            // response header bytes
            pugi::xml_node xnResp = xnAddress.child("response");
            if(xnResp)   {
                QString identifier(xnResp.attribute("identifier").value());
                if(identifier.isEmpty())   {
                    OBDREFDEBUG << "Error: ISO 15765-4 std,"
                                << "incomplete response header";
                    return false;
                }
                bool convOk = false;
                quint32 headerVal = stringToUInt(convOk,identifier);
                if(!convOk)   {
                    OBDREFDEBUG << "Error: ISO 15765 std, "
                                   "bad request header";
                    return false;
                }
                ubyte upperByte = ((headerVal & 0xF00) >> 8);
                ubyte lowerByte = (headerVal & 0xFF);
                msg.expHeaderBytes[0] = upperByte;
                msg.expHeaderBytes[1] = lowerByte;
            }
        }
        // ISO 15765-4 (29-bit extended id)
        // store 29-bit header in two bytes
        else   {
            // request header bytes
            pugi::xml_node xnReq = xnAddress.child("request");
            if(xnReq)   {
                QString prio(xnReq.attribute("prio").value());
                QString format(xnReq.attribute("format").value());
                QString target(xnReq.attribute("target").value());
                QString source(xnReq.attribute("source").value());

                // all four bytes must be defined
                if(prio.isEmpty() || format.isEmpty() ||
                   target.isEmpty() || source.isEmpty())   {
                    OBDREFDEBUG << "Error: ISO 15765-4 Ext,"
                                << "incomplete request header";
                    return false;
                }

                bool okPrio,okFormat,okTarget,okSource;
                msg.reqHeaderBytes << stringToUInt(okPrio,prio);
                msg.reqHeaderBytes << stringToUInt(okFormat,format);
                msg.reqHeaderBytes << stringToUInt(okTarget,target);
                msg.reqHeaderBytes << stringToUInt(okSource,source);

                if(!(okPrio&&okFormat&&okTarget&&okSource))   {
                    OBDREFDEBUG << "Error: ISO 15765-4 Ext,"
                                << "invalid request header";
                    return false;
                }

            }
            else   {
                OBDREFDEBUG << "Warn: ISO 15765 ext, "
                               "no request header";
            }

            // preemptively fill out response header
            // bytes (unspec'd bytes will be ignored
            // by expHeaderMask)
            msg.expHeaderBytes << 0x00 << 0x00 << 0x00 << 0x00;
            msg.expHeaderMask  << 0x00 << 0x00 << 0x00 << 0x00;

            // response header bytes
            pugi::xml_node xnResp = xnAddress.child("response");
            if(xnResp)   {
                QString prio(xnReq.attribute("prio").value());
                QString format(xnReq.attribute("format").value());
                QString target(xnReq.attribute("target").value());
                QString source(xnReq.attribute("source").value());

                bool okPrio   = true;
                bool okFormat = true;
                bool okTarget = true;
                bool okSource = true;

                if(!prio.isEmpty())   {
                    msg.expHeaderBytes[0] = stringToUInt(okPrio,prio);
                    msg.expHeaderMask[0] = 0xFF;
                }
                if(!format.isEmpty())   {
                    msg.expHeaderBytes[1] = stringToUInt(okFormat,format);
                    msg.expHeaderMask[1] = 0xFF;
                }
                if(!target.isEmpty())   {
                    msg.expHeaderBytes[2] = stringToUInt(okTarget,target);
                    msg.expHeaderMask[2] = 0xFF;
                }
                if(!target.isEmpty())   {
                    msg.expHeaderBytes[3] = stringToUInt(okSource,source);
                    msg.expHeaderMask[3] = 0xFF;
                }

                if(!(okPrio&&okFormat&&okTarget&&okSource))   {
                    OBDREFDEBUG << "Error: ISO 15765-4 ext, "
                                   "invalid response header";
                    return false;
                }
            }
        }

        // save the MessageData
        paramFrame.listMessageData.push_back(msg);
        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::buildData(ParameterFrame &paramFrame,
                           pugi::xml_node xnParameter)
    {
        // If the parameter is only being used to parse
        // passively, it should not have any request or
        // response attributes
        QString request,request0;
        bool hasMultipleReqs=false;
        {
            request = QString(xnParameter.attribute("request").value());
            request0 = QString(xnParameter.attribute("request0").value());
            if(request.isEmpty() && request0.isEmpty())   {
                // assume that no requests will be made
                // for this parameter so we are done
                return true;
            }
            else if((!request.isEmpty()) && (!request0.isEmpty()))   {
                OBDREFDEBUG << "Error: mixed single and multiple requests";
                return false;
            }
            else if(request.isEmpty())   {
                hasMultipleReqs=true;
            }
        }

        // We expect singular request data to be indicated by:
        // request="" [response.prefix=""] [response.mask=""] ...

        // And multiple request data to be indicated by:
        // request0="" [response0.prefix=""] [response0.mask=""] ...
        // request1="" [response1.prefix=""] [response1.mask=""] ...
        // requestN="" [responseN.prefix=""] [responseN.mask=""] ...

        // The request attribute is mandatory, the response
        // attributes are optional

        // For multiple requests, numbering starts at 0

        bool convOk=false;
        int n=0;

        while(true)
        {
            if(n > 0)   {
                // each separate request is stored in
                // individual MessageData objects
                MessageData msg;
                paramFrame.listMessageData.push_back(msg);
            }
            MessageData &msg = paramFrame.listMessageData.last();

            QString request_delay,response_prefix,response_bytes;

            if(!hasMultipleReqs)  {
                request         = QString(xnParameter.attribute("request").value());
                request_delay   = QString(xnParameter.attribute("request.delay").value());
                response_prefix = QString(xnParameter.attribute("response.prefix").value());
                response_bytes  = QString(xnParameter.attribute("response.bytes").value());
            }
            else   {
                QString request_n           = "request"+QString::number(n);
                QString request_delay_n     = request_n+".delay";
                QString response_n          = "response"+QString::number(n);
                QString response_prefix_n   = response_n+".prefix";
                QString response_bytes_n    = response_n+".bytes";

                request         = QString(xnParameter.
                    attribute(request_n.toLocal8Bit().data()).value());

                request_delay   = QString(xnParameter.
                    attribute(request_delay_n.toLocal8Bit().data()).value());

                response_prefix = QString(xnParameter.
                    attribute(response_prefix_n.toLocal8Bit().data()).value());

                response_bytes  = QString(xnParameter.
                    attribute(response_bytes_n.toLocal8Bit().data()).value());

                if(request.isEmpty())   {
                    paramFrame.listMessageData.removeLast();
                    break;
                }
                n++;
            }

            // request

            // add an initial set of data bytes
            ByteList byteList;
            msg.listReqDataBytes << byteList;

            QStringList sl_request = request.split(" ",QString::SkipEmptyParts);
            for(int i=0; i < sl_request.size(); i++)   {
                msg.listReqDataBytes[0] << stringToUInt(convOk,sl_request[i]);
            }
            if(msg.listReqDataBytes[0].isEmpty())   {
                OBDREFDEBUG << "Error: invalid request data bytes";
                return false;
            }

            // request delay
            if(!request_delay.isEmpty())   {
                msg.reqDataDelayMs = stringToUInt(convOk,request_delay);
            }

            // response prefix
            if(!response_prefix.isEmpty())   {
                QStringList sl_response_prefix = response_prefix.split(" ",QString::SkipEmptyParts);
                for(int i=0; i < sl_response_prefix.size(); i++)   {
                    msg.expDataPrefix << stringToUInt(convOk,sl_response_prefix[i]);
                }
            }

            // response bytes
            if(!response_bytes.isEmpty())   {
                msg.expDataByteCount = stringToUInt(convOk,response_bytes);
            }

            if(!hasMultipleReqs)   {
                break;
            }
        }

        // copy over header data from first message in list
        // to other entries; only relevant for multi-response
        MessageData const &firstMsg = paramFrame.listMessageData[0];
        for(int i=1; i < paramFrame.listMessageData.size(); i++)   {
            MessageData &nextMsg = paramFrame.listMessageData[i];
            nextMsg.reqHeaderBytes = firstMsg.reqHeaderBytes;
            nextMsg.expHeaderBytes = firstMsg.expHeaderBytes;
            nextMsg.expHeaderMask  = firstMsg.expHeaderMask;
        }

        // ISO 15765 may need additional formatting
        if(paramFrame.parseProtocol == PROTOCOL_ISO_15765)
        {
            for(int i=0; i < paramFrame.listMessageData.size(); i++)   {
                MessageData &msg = paramFrame.listMessageData[i];
                QList<ByteList> &listReqDataBytes = msg.listReqDataBytes;
                int dataLength = listReqDataBytes[0].size();

                if(paramFrame.iso15765_splitReqIntoFrames && dataLength > 7)   {
                    // split the request data into frames
                    ubyte idxFrame = 0;
                    ByteList emptyByteList;

                    // (first frame)
                    // truncate the current frame after six
                    // bytes and copy to the next frame
                    listReqDataBytes << emptyByteList;
                    while(listReqDataBytes[idxFrame].size() > 6)   {
                        ubyte dataByte = listReqDataBytes[idxFrame].takeAt(6);
                        listReqDataBytes[idxFrame+1] << dataByte;
                    }
                    idxFrame++;

                    // (consecutive frames)
                    // truncate the current frame after seven
                    // bytes and copy to the next frame
                    while(listReqDataBytes[idxFrame].size() > 7)   {
                        listReqDataBytes << emptyByteList;
                        while(listReqDataBytes[idxFrame].size() > 7)   {
                            ubyte dataByte = listReqDataBytes[idxFrame].takeAt(7);
                            listReqDataBytes[idxFrame+1] << dataByte;
                        }
                        idxFrame++;
                    }
                }
                if(paramFrame.iso15765_addPciByte)   {
                    // (single frame)
                    // * higher 4 bits set to 0 for a single frame
                    // * lower 4 bits gives the number of data bytes
                    if(listReqDataBytes.size() == 1)   {
                        ubyte pciByte = listReqDataBytes[0].size();
                        listReqDataBytes[0].prepend(pciByte);
                    }
                    // (multi frame)
                    else   {
                        // (first frame)
                        // higher 4 bits set to 0001
                        // lower 12 bits give number of data bytes
                        ubyte lowerByte = (dataLength & 0x0FF);
                        ubyte upperByte = (dataLength & 0xF00) >> 8;
                        upperByte += 16;    // += 0b1000

                        listReqDataBytes[0].prepend(lowerByte);
                        listReqDataBytes[0].prepend(upperByte);

                        // (consecutive frames)
                        // * cycle 0x20-0x2F for each CF starting with 0x21
                        for(int j=1; j < listReqDataBytes.size(); j++)   {
                            ubyte pciByte = 0x20 + (j % 0x10);
                            listReqDataBytes[j].prepend(pciByte);
                        }
                    }
                }
            }
        }

        // ISO 14230 needs additional formatting to
        // add data length info to the header
        if(paramFrame.parseProtocol == PROTOCOL_ISO_14230)
        {
            for(int i=0; i < paramFrame.listMessageData.size(); i++)   {
                MessageData &msg = paramFrame.listMessageData[i];
                QList<ByteList> &listReqDataBytes = msg.listReqDataBytes;
                int dataLength = listReqDataBytes[0].size();

                if(dataLength > 255)   {
                    OBDREFDEBUG << "Error: ISO 14230, invalid"
                                   "data length ( > 255)";
                    return false;
                }

                if(paramFrame.iso14230_addLengthByte)   {
                    // add separate length byte
                    msg.reqHeaderBytes << ubyte(dataLength);
                }
                else   {
                    // encode length in format byte
                    if(dataLength > 63)   {
                        OBDREFDEBUG << "Error: ISO 14230, invalid"
                                       "data length ( > 63)";
                        return false;
                    }
                    msg.reqHeaderBytes[0] |= ubyte(dataLength);
                }
            }
        }
        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::parseResponse(ParameterFrame const &msgFrame,
                               QList<Data> &listData)
    {
        if(msgFrame.functionKeyIdx < 0)   {
            OBDREFDEBUG << "Error: parseResponse: invalid function idx";
            return false;
        }
        int js_f_idx = msgFrame.functionKeyIdx;

        if(msgFrame.parseMode == PARSE_SEPARATELY)
        {
            // * default parse mode -- the parse script is run
            //   once for each entry in MessageData.listData

            // * the data is accessed using "BYTE(N)" in js where N
            //   is the Nth data byte in a listData entry

            for(int i=0; i < msgFrame.listMessageData.size(); i++)
            {
                MessageData const &msg = msgFrame.listMessageData[i];
                for(int j=0; j < msg.listHeaders.size(); j++)
                {
                    ByteList const &headerBytes = msg.listHeaders[j];
                    ByteList const &dataBytes = msg.listData[j];

                    obdref::Data parsedData;

                    // fill out parameter data
                    parsedData.paramName    = msgFrame.name;
                    parsedData.srcName      = msgFrame.address;

                    // clear existing data in js context
                    duk_dup(m_js_ctx,m_js_idx_f_clear_data);
                    duk_call(m_js_ctx,0);
                    duk_pop(m_js_ctx);

                    // copy over databytes to js context
                    duk_dup(m_js_ctx,m_js_idx_f_add_databytes);
                    int list_arr_idx = duk_push_array(m_js_ctx);    // listDataBytes
                    int data_arr_idx = duk_push_array(m_js_ctx);    // dataBytes
                    for(int k=0; k < dataBytes.size(); k++)   {
                        duk_push_number(m_js_ctx,dataBytes[k]);
                        duk_put_prop_index(m_js_ctx,data_arr_idx,k);
                    }
                    duk_put_prop_index(m_js_ctx,list_arr_idx,0);
                    duk_call(m_js_ctx,1);
                    duk_pop(m_js_ctx);

                    // parse the data
                    duk_dup(m_js_ctx,m_js_listFunctionIdx[js_f_idx]);
                    duk_call(m_js_ctx,0);
                    duk_pop(m_js_ctx);

                    // save results
                    this->saveNumAndLitData(parsedData);

                    // save data source address info in LiteralData
                    LiteralData srcAddress;
                    srcAddress.property = "Source Address";
                    for(int k=0; k < headerBytes.size(); k++)   {
                        QString bStr = ConvUByteToHexStr(headerBytes[k]) + " ";
                        srcAddress.valueIfTrue.append(bStr);
                    }
                    srcAddress.valueIfTrue = srcAddress.valueIfTrue.toUpper();
                    srcAddress.value = true;
                    parsedData.listLiteralData.push_back(srcAddress);

                    listData.push_back(parsedData);
                }
            }
            return true;
        }
        else if(msgFrame.parseMode == PARSE_COMBINED)
        {
            // * the parse script is run once for every
            //   ParameterFrame.listMessageData

            // * all entries in an individual MessageData
            //   are passed to the parse function together

            // * both the header bytes and data bytes are
            //   passed to the js context

            // * data within a MessageData can be accessed
            //   using "REQ(N).DATA(N).BYTE(N)", where:
            //   - REQ(N) represents the MessageData to access
            //   - DATA(N) represents a single entry in the list
            //     of data bytes available for that MessageData
            //   - BYTE(N) is a single byte in that list of
            //     data bytes

            // clear existing data in js context
            duk_dup(m_js_ctx,m_js_idx_f_clear_data);
            duk_call(m_js_ctx,0);
            duk_pop(m_js_ctx);

            obdref::Data parsedData;
            parsedData.paramName    = msgFrame.name;
            parsedData.srcName      = msgFrame.address;

            for(int i=0; i < msgFrame.listMessageData.size(); i++)
            {
                MessageData const &msg = msgFrame.listMessageData[i];
                duk_dup(m_js_ctx,m_js_idx_f_add_msg_data);
                int list_arr_idx;

                // build header bytes js array
                list_arr_idx = duk_push_array(m_js_ctx);    // listHeaderBytes

                for(int j=0; j < msg.listHeaders.size(); j++)   {
                    ByteList const &headerBytes = msg.listHeaders[j];
                    int header_arr_idx = duk_push_array(m_js_ctx);    // headerBytes
                    for(int k=0; k < headerBytes.size(); k++)   {
                        duk_push_number(m_js_ctx,headerBytes[k]);
                        duk_put_prop_index(m_js_ctx,header_arr_idx,k);
                    }
                    duk_put_prop_index(m_js_ctx,list_arr_idx,j);
                }

                // build data bytes js array
                list_arr_idx = duk_push_array(m_js_ctx);    // listDataBytes

                for(int j=0; j < msg.listData.size(); j++)   {
                    ByteList const &dataBytes = msg.listData[j];
                    int data_arr_idx = duk_push_array(m_js_ctx);    // dataBytes
                    for(int k=0; k < dataBytes.size(); k++)   {
                        duk_push_number(m_js_ctx,dataBytes[k]);
                        duk_put_prop_index(m_js_ctx,data_arr_idx,k);
                    }
                    duk_put_prop_index(m_js_ctx,list_arr_idx,j);
                }
                duk_call(m_js_ctx,2);
                duk_pop(m_js_ctx);
            }
            // parse the data
            duk_dup(m_js_ctx,m_js_listFunctionIdx[js_f_idx]);
            duk_call(m_js_ctx,0);
            duk_pop(m_js_ctx);

            // save results
            this->saveNumAndLitData(parsedData);
            listData.push_back(parsedData);
            return true;
        }
        return false;
    }

    void Parser::saveNumAndLitData(Data &data)
    {
        // save numerical data
        duk_dup(m_js_ctx,m_js_idx_f_get_num_data);
        duk_call(m_js_ctx,0);                       // <..., listNumData>
        duk_get_prop_string(m_js_ctx,-1,"length");  // <..., listNumData, listNumData.length>
        quint32 listNumLength = quint32(duk_get_number(m_js_ctx,-1));
        duk_pop(m_js_ctx);                          // <..., listNumData>

        for(quint32 i=0; i < listNumLength; i++)   {
            NumericalData numData;
            duk_get_prop_index(m_js_ctx,-1,i);      // <..., listNumData, listNumData[i]>

            duk_get_prop_string(m_js_ctx,-1,"units");
            numData.units = QString(duk_get_string(m_js_ctx,-1));
            duk_pop(m_js_ctx);

            duk_get_prop_string(m_js_ctx,-1,"min");
            numData.min = duk_get_number(m_js_ctx,-1);
            duk_pop(m_js_ctx);

            duk_get_prop_string(m_js_ctx,-1,"max");
            numData.max = duk_get_number(m_js_ctx,-1);
            duk_pop(m_js_ctx);

            duk_get_prop_string(m_js_ctx,-1,"value");
            numData.value = duk_get_number(m_js_ctx,-1);
            duk_pop(m_js_ctx);

            duk_get_prop_string(m_js_ctx,-1,"property");
            numData.property = QString(duk_get_string(m_js_ctx,-1));
            duk_pop(m_js_ctx);

            duk_pop(m_js_ctx);                      // <..., listNumData>
            data.listNumericalData.push_back(numData);
        }
        duk_pop(m_js_ctx);

        // save literal data
        duk_dup(m_js_ctx,m_js_idx_f_get_lit_data);
        duk_call(m_js_ctx,0);                       // <..., listLitData>
        duk_get_prop_string(m_js_ctx,-1,"length");  // <..., listLitData.length>
        quint32 listLitLength = quint32(duk_get_number(m_js_ctx,-1));
        duk_pop(m_js_ctx);                          // <..., listLitData>

        for(quint32 i=0; i < listLitLength; i++)   {
            LiteralData litData;
            duk_get_prop_index(m_js_ctx,-1,i);      // <..., listLitData, listLitData[i]>

            duk_get_prop_string(m_js_ctx,-1,"value");
            int litDataVal = duk_get_boolean(m_js_ctx,-1);
            litData.value = (litDataVal == 1) ? true : false;
            duk_pop(m_js_ctx);

            duk_get_prop_string(m_js_ctx,-1,"valueIfFalse");
            litData.valueIfFalse = QString(duk_get_string(m_js_ctx,-1));
            duk_pop(m_js_ctx);

            duk_get_prop_string(m_js_ctx,-1,"valueIfTrue");
            litData.valueIfTrue = QString(duk_get_string(m_js_ctx,-1));
            duk_pop(m_js_ctx);

            duk_get_prop_string(m_js_ctx,-1,"property");
            litData.property = QString(duk_get_string(m_js_ctx,-1));
            duk_pop(m_js_ctx);

            duk_pop(m_js_ctx);                      // <..., listLitData>
            data.listLiteralData.push_back(litData);
        }
        duk_pop(m_js_ctx);
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::cleanFrames_Legacy(MessageData &msg)
    {
        int const headerLength=3;
        for(int j=0; j < msg.listRawFrames.size(); j++)
        {
            ByteList const &rawFrame = msg.listRawFrames[j];

            // Split each raw frame into a header and its
            // corresponding data bytes
            // [h0 h1 h2] [d0 d1 d2 d3 d4 d5 d6 ...]
            ByteList headerBytes;
            for(int k=0; k < headerLength; k++)   {
                headerBytes << rawFrame[k];
            }
            ByteList dataBytes;
            for(int k=headerLength; k < rawFrame.size(); k++)   {
                dataBytes << rawFrame[k];
            }

            // check header
            if(!checkBytesAgainstMask(msg.expHeaderBytes,
                                      msg.expHeaderMask,
                                      headerBytes))   {
                OBDREFDEBUG << "Warn: SAE J1850/ISO 9141-2/ISO 14230-4, "
                               "header bytes mismatch";
                continue;
            }

            // check data prefix
            bool dataPrefixOk = true;
            for(int k=0; k < msg.expDataPrefix.size(); k++)   {
                ubyte dataByte = dataBytes.takeAt(0);
                if(dataByte != msg.expDataPrefix[k])   {
                    dataPrefixOk = false;
                    break;
                }
            }
            if(!dataPrefixOk)   {
                OBDREFDEBUG << "Warn: SAE J1850/ISO 9141-2/ISO 14230-4, "
                               "data prefix mismatch";
                continue;
            }

            // save
            msg.listHeaders << headerBytes;
            msg.listData << dataBytes;
        }

        if(msg.listHeaders.empty())   {
            OBDREFDEBUG << "Error: SAE J1850/ISO 9141-2/ISO 14230-4, "
                           "empty message data";
            return false;
        }

        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::cleanFrames_ISO_14230(MessageData &msg)
    {
        for(int j=0; j < msg.listRawFrames.size(); j++)
        {
            ByteList const &rawFrame = msg.listRawFrames[j];

            // determine header type:
            // A [format]
            // B [format] [target] [source]
            // C [format] [length]
            // D [format] [target] [source] [length]

            // let the format byte be:
            // A1 A0 L5 L4 L3 L2 L1 L0

            // if [A1 A0] == [0 0], target and source
            // bytes are not present (A and C)
            bool noAddressing = ((rawFrame[0] >> 6) == 0);

            // if [L0 to L5] are all 0, data length is specified
            // in a separate length byte (C and D)
            // 0x3F = 0b00111111
            bool hasLengthByte = ((rawFrame[0] & 0x3F) == 0);

            ubyte headerLength=4;
            if(noAddressing)   { headerLength -= 2; }
            if(!hasLengthByte) { headerLength -= 1; }

            ubyte dataLength = (hasLengthByte) ?
                rawFrame[headerLength-1] : (rawFrame[0] & 0x3F);

            // split each raw frame into a header and its
            // corresponding data bytes
            ByteList headerBytes;
            for(int k=0; k < headerLength; k++)   {
                headerBytes << rawFrame[k];
            }

            ByteList dataBytes;
            for(int k=headerLength; k < (headerLength+dataLength); k++)   {
                dataBytes << rawFrame[k];
            }

            // filter with expHeaderBytes
            // for ISO 14230, by default expHeaderBytes
            // looks like: [format] [target] [source]

            // we add and remove bytes to match up with
            // the actual header before comparing
            ByteList expHeaderBytes,expHeaderMask;

            // all headers have a format byte
            expHeaderBytes << msg.expHeaderBytes[0];
            expHeaderMask << msg.expHeaderMask[0];

            switch(headerLength)   {
                case 2:   { // [format] [length]
                    expHeaderBytes << 0x00;
                    expHeaderMask << 0x00;
                    break;
                }
                case 3:   { // [format] [target] [source]
                    expHeaderBytes << msg.expHeaderBytes[1] << msg.expHeaderBytes[2];
                    expHeaderMask << msg.expHeaderMask[1] << msg.expHeaderMask[2];
                    break;
                }
                case 4:   { // [format] [target] [source] [length]
                    expHeaderBytes << msg.expHeaderBytes[1] << msg.expHeaderBytes[2] << 0x00;
                    expHeaderMask << msg.expHeaderMask[1] << msg.expHeaderMask[2] << 0x00;
                    break;
                }
                default:   {
                    break;
                }
            }

            // check for expected header bytes
            if(!checkBytesAgainstMask(expHeaderBytes,expHeaderMask,headerBytes))   {
                OBDREFDEBUG << "Warn: ISO 14230, header bytes mismatch";
                continue;
            }

            // check/remove data prefix
            if(!checkAndRemoveDataPrefix(msg.expDataPrefix,dataBytes))   {
                OBDREFDEBUG << "Warn: ISO 14230, data prefix mismatch";
                continue;
            }

            // save
            msg.listHeaders << headerBytes;
            msg.listData << dataBytes;
        }

        if(msg.listHeaders.empty())   {
            OBDREFDEBUG << "Error: ISO 14230, empty message data";
            return false;
        }

        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::cleanFrames_ISO_15765(MessageData &msg, int const headerLength)
    {
        for(int j=0; j < msg.listRawFrames.size(); j++)
        {
            ByteList const &rawFrame = msg.listRawFrames[j];

            // split raw frame into a header and its
            // corresponding data bytes
            ByteList headerBytes;
            for(int k=0; k < headerLength; k++)   {
                headerBytes << rawFrame[k];
            }
            ByteList dataBytes;
            for(int k=headerLength; k < rawFrame.size(); k++)   {
                dataBytes << rawFrame[k];
            }

            // check header
            if(!checkBytesAgainstMask(msg.expHeaderBytes,
                                      msg.expHeaderMask,
                                      headerBytes))   {
                OBDREFDEBUG << "Warn: ISO 15765-4, "
                               "header bytes mismatch";
                continue;
            }

            // save
            msg.listHeaders << headerBytes;
            msg.listData << dataBytes;
        }

        // go through the frames and merge multi-frame messages

        // keep track of CFs that have already been merged
        QList<bool> listMergedFrames;
        for(int j=0; j < msg.listHeaders.size(); j++)   {
            listMergedFrames << false;
        }

        for(int j=0; j < msg.listHeaders.size(); j++)   {
            // ignore already merged frames
            if(listMergedFrames[j])   {
                continue;
            }

            ubyte jPciByte = msg.listData[j][0];

            // [first frame] pci byte: 1N
            if((jPciByte >> 4) == 1)   {
                ByteList & headerFF = msg.listHeaders[j];
                ubyte nextPciByte = 0x21;

                // keep track of the total number of data
                // bytes we expect to see
                int dataLength = ((jPciByte & 0x0F) << 8) +
                                 msg.listData[j][1];

                int dataBytesSeen = msg.listData[j].size()-2;

                for(int k=0; k < msg.listHeaders.size(); k++)   {
                    // ignore already merged frames
                    if(listMergedFrames[k])   {
                        continue;
                    }

                    // if the header and target pci byte match
                    if((msg.listData[k][0] == nextPciByte) &&
                        msg.listHeaders[k] == headerFF)   {
                        // this is the next CF frame

                        // remove the pci byte and merge this frame
                        // to the first frame's data bytes
                        msg.listData[k].removeAt(0);
                        msg.listData[j] << msg.listData[k];

                        // mark as merged
                        listMergedFrames[k] = true;

                        // we can stop once we've merged the
                        // expected number of data bytes
                        dataBytesSeen += msg.listData[k].size();
                        if(dataBytesSeen >= dataLength)   {
                            break;
                        }

                        // set next target pci byte
                        nextPciByte+=0x01;
                        if(nextPciByte == 0x30)   {
                            nextPciByte = 0x20;
                        }

                        // reset k
                        k=0;
                    }
                }
                // once we get here, all the CF for the FF
                // at msg.listData[j] should be merged
            }
        }

        // clean up CFs and pci bytes
        for(int j=msg.listHeaders.size()-1; j >= 0; j--)   {
            if(listMergedFrames[j])   {
                msg.listHeaders.removeAt(j);
                msg.listData.removeAt(j);
                listMergedFrames.removeAt(j);
                continue;
            }
            ubyte pciByte = msg.listData[j][0];
            if((pciByte >> 4) == 0)   {         // SF
                msg.listData[j].removeAt(0);
            }
            else if((pciByte >> 4) == 1)   {    // FF
                msg.listData[j].removeAt(0);
                msg.listData[j].removeAt(0);
            }

            // check data prefix
            bool dataPrefixOk = true;
            for(int k=0; k < msg.expDataPrefix.size(); k++)   {
                ubyte dataByte = msg.listData[j].takeAt(0);
                if(dataByte != msg.expDataPrefix[k])   {
                    dataPrefixOk = false;
                    break;
                }
            }
            if(!dataPrefixOk)   {
                OBDREFDEBUG << "Warn: ISO 15765-4, data prefix mismatch";
                msg.listHeaders.removeAt(j);
                msg.listData.removeAt(j);
                listMergedFrames.removeAt(j);
                continue;
            }
        }

        if(msg.listHeaders.empty())   {
            OBDREFDEBUG << "Error: ISO 15765-4, empty message data";
            return false;
        }

        return true;
    }

    // ========================================================================== //
    // ========================================================================== //

    bool Parser::checkBytesAgainstMask(ByteList const &expBytes,
                                       ByteList const &expMask,
                                       ByteList const &bytes)
    {
        if(bytes.size() < expBytes.size())   {
            return false;   // should never get here
        }

        bool bytesOk = true;
        for(int i=0; i < bytes.size(); i++)   {
            ubyte maskByte = expMask[i];
            if((maskByte & bytes[i]) !=
               (maskByte & expBytes[i]))
            {
                bytesOk = false;
                break;
            }
        }
        return bytesOk;
    }

    bool Parser::checkAndRemoveDataPrefix(ByteList const &expDataPrefix,
                                          ByteList &dataBytes)
    {
        bool dataBytesOk = true;
        for(int i=0; i < expDataPrefix.size(); i++)   {
            if(expDataPrefix[i] != dataBytes.takeAt(0))  {
                dataBytesOk=false;
                break;
            }
        }
        return dataBytesOk;
    }

    // ========================================================================== //
    // ========================================================================== //

    quint32 Parser::stringToUInt(bool &convOk, QString const &parseStr)
    {
        // expect ONE token representing a number
        // if str contains '0bXXXXX' -> parse as binary
        // if str contains '0xXXXXX' -> parse as hex
        // else parse as dec

        int base=10;
        QString numstr(parseStr);
        if(numstr[0] == '0')   {
            if(numstr[1] == 'b')   {
                numstr = numstr.mid(2);
                base = 2;
            }
            else if(numstr[1] == 'x')   {
                numstr = numstr.mid(2);
                base = 16;
            }
        }

        return numstr.toUInt(&convOk,base);
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
        QFile file(filePath);
        if(file.open(QIODevice::ReadOnly))   {
            QTextStream textStream(&file);
            fileDataAsStr = textStream.readAll();
            return true;
        }

        OBDREFDEBUG << "OBDREF: Could not open file: "
                    << filePath << "\n";
        return false;
    }

    // ========================================================================== //
    // ========================================================================== //

    void Parser::printByteList(ByteList const &byteList)
    {
        QString bts;
        for(int i=0; i < byteList.size(); i++)   {
            QString byteStr = QString::number(byteList[i],16);
            while(byteStr.length() < 2)   {
                byteStr.prepend("0");
            }
            byteStr.append(" ");
            byteStr = byteStr.toUpper();
            bts += byteStr;
        }
        OBDREFDEBUG << bts;
    }
    // ========================================================================== //
    // ========================================================================== //
}
