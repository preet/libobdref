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

#ifndef PARSER_H
#define PARSER_H

// stdlib includes
#include <fstream>

// Qt includes
#include <QDebug>
#include <QFile>

// pugixml
#include "pugixml/pugixml.hpp"

// duktape
#include "duktape/duktape.h"

// obdref
#include "datatypes.h"
#include "obdrefdebug.h"

namespace obdref
{

class Parser
{

public:
    Parser(QString const &filePath, bool &parsedOk);
    ~Parser();

    // BuildParameterFrame
    // * uses the definitions file to build
    //   up request message data for the spec,
    //   protocol and param defined in msgFrame
    bool BuildParameterFrame(ParameterFrame &paramFrame);



    // ParseParameterFrame
    // * parses vehicle response data defined
    //   in msgFrame[i].listRawDataFrames and
    //   saves it in listDataResults
    bool ParseParameterFrame(ParameterFrame &msgFrame,
                           QList<Data> &listDataResults);

    // ConvValToHexByte
    // * converts a ubyte value to its equivalent
    //   hex byte characters ie 255 -> "FF"
    // * the result is padded with zeros such that
    //   it is two characters in length
    inline QByteArray ConvUByteToHexStr(ubyte byte)
    {
        return m_mapUByteToHexStr.value(byte);
    }

    // ConvHexByteToVal
    // * converts hex byte characters that represent
    //   a byte to a numerical value ("FF"->255)
    inline ubyte ConvHexStrToUByte(QByteArray str)
    {
        str = str.toUpper();  // letters must be uppercase
        return m_mapHexStrToUByte.value(str,0);
    }

    // GetParameterNames
    // * returns a list of parameter names from
    //   the definitions file based on input args
    QStringList GetParameterNames(QString const &specName,
                                  QString const &protocolName,
                                  QString const &addressName);

    // GetLastKnownErrors
    // * returns a list of errors
    QStringList GetLastKnownErrors();

private:
    // jsInit
    // * creates js heap and context
    // * registers all required vars and functions
    //   to the js context's global object
    bool jsInit();

    //
    bool buildHeader_Legacy(ParameterFrame & paramFrame,
                            pugi::xml_node xnAddress);

    bool buildHeader_ISO_14230(ParameterFrame & paramFrame,
                               pugi::xml_node xnAddress);

    bool buildHeader_ISO_15765(ParameterFrame & paramFrame,
                               pugi::xml_node xnAddress);

    bool buildData(ParameterFrame & paramFrame,
                   pugi::xml_node xnParameter);

    // parseResponse
    // * passes data processed by cleanRawData[] to
    //   the javascript engine and uses the script
    //   defined in the definitions file to convert
    //   response data into meaningful values, which
    //   is saved in listData
    // * parseMode == PER_RESP_DATA
    //   the script is run once for each cleaned
    //   response in MessageData
    // * parseMode == PER_MESSAGE
    //   the script is run one for the entire
    //   MessageData, however many responses
    //   there are [not implemented yet]
    bool parseResponse(ParameterFrame const &msgFrame,
                       QList<Data> &listData);

    // saveNumAndLitData
    // * helper function that saves the numerical
    //   and literal data interpreted with the
    //   vehicle response from the js context
    void saveNumAndLitData(Data &myData);

    // cleanFrames_[...]
    // * cleans up rawDataFrames by checking for
    //   expected message bytes and groups/merges
    //   the frames as appropriate to save the
    //   data in listHeaders and listCleanData

    // cleanFrames_Legacy
    // * includes: SAEJ1850 VPW/PWM,ISO 9141-2,ISO 14230-4
    //   (not suitable for any other ISO 14230)
    bool cleanFrames_Legacy(MessageData &msg);

    // cleanFrames_ISO14230
    bool cleanFrames_ISO_14230(MessageData &msg);

    // cleanFrames_ISO_15765
    // * includes: ISO 15765, 11-bit and 29-bit headers,
    //   and ISO 15765-2/ISO-TP (multiframe messages)
    bool cleanFrames_ISO_15765(MessageData &msg,
                               int const headerLength);

    // checkHeaderBytes
    // * checks bytes against expected bytes with a mask
    // * returns false if the masked values do not match
    bool checkBytesAgainstMask(ByteList const &expBytes,
                               ByteList const &expMask,
                               ByteList const &bytes);

    // checkAndRemoveDataPrefix
    // * checks data bytes against the expected prefix
    // * removes prefix from data bytes
    // * returns false if prefix doesn't match
    bool checkAndRemoveDataPrefix(ByteList const &expDataPrefix,
                                  ByteList &dataBytes);

    // getErrorStream
    QTextStream & getErrorStream();

    // stringToUInt
    // * converts a string representing a binary,
    //   hex, or decimal number into a uint
    quint32 stringToUInt(bool &convOk, QString const &parseStr);

    // convTextFileToQStr
    // * converts a text file into a QString
    bool convTextFileToQStr(QString const &filePath,
                            QString & fileDataAsStr);

    // printByteList
    // * prints a bytelist out using hex bytes
    void printByteList(ByteList const &bytelist);



    QHash<ubyte,QByteArray> m_mapUByteToHexStr;
    QHash<QByteArray,ubyte> m_mapHexStrToUByte;

    // pugixml vars
    QString m_xmlFilePath;
    pugi::xml_document m_xmlDoc;

    // duktape
    duk_context * m_js_ctx;
    quint32 m_js_idx_global_object;
    quint32 m_js_idx_f_add_databytes;
    quint32 m_js_idx_f_add_msg_data;
    quint32 m_js_idx_f_clear_data;
    quint32 m_js_idx_f_get_lit_data;
    quint32 m_js_idx_f_get_num_data;

    // duktape javascript parse function registry
    QList<QString> m_js_listFunctionKey;
    QList<quint32> m_js_listFunctionIdx;

    // errors
    QTextStream m_lkErrors;
    QString m_lkErrorString;
};
}

#endif // PARSER_H
