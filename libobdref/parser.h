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

#ifndef PARSER_H
#define PARSER_H

// stdlib includes
#include <fstream>

// Qt includes
#include <QDebug>

// pugixml includes
#include "pugixml/pugixml.hpp"

// v8 includes
#include <v8.h>

// obdref data types
#include "message.h"

namespace obdref
{

enum ParseMode
{
    PER_RESP_DATA,
    PER_MESSAGE
};

class Parser
{

public:
    Parser(QString const &filePath, bool &parsedOk);
    ~Parser();

    // BuildMessageFrame
    // * uses the definitions file to build
    //   up request message data for the spec,
    //   protocol and param defined in msgFrame
    bool BuildMessageFrame(MessageFrame &msgFrame);

    // ParseMessageFrame
    // * parses vehicle response data defined
    //   in msgFrame[i].listRawDataFrames and
    //   saves it in listDataResults
    bool ParseMessageFrame(MessageFrame &msgFrame,
                           QList<Data> &listDataResults);

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
    // cleanRawData_[Legacy,ISO_14230_4,ISO_15765_4]
    // * cleans up rawDataFrames by checking for
    //   expected message bytes and groups/merges
    //   the frames as appropriate to save the
    //   data in listHeaders and listCleanData
    bool cleanRawData_Legacy(MessageFrame &msgFrame);
    bool cleanRawData_ISO_14230_4(MessageFrame &msgFrame);
    bool cleanRawData_ISO_15765_4(MessageFrame &msgFrame);

    // groupDataByHeader
    // * helper function to convert a list of data
    //   frames into a separate list of unique headers
    //   and respective concatenated data bytes
    void groupDataByHeader(QList<ByteList> &listHeaders,
                           QList<ByteList> &listDataBytes);

    // printCleanedData
    // * debug function to print message data after
    //   its been processed by cleanRawData_[]
    void printCleanedData(MessageFrame &msgFrame);

    // printRawData
    // * debug function to print a list of raw data frames
    void printRawData(const QList<ByteList> &listRawData);

    // parseResponse
    // * passes data processed by cleanRawData[] to
    //   the v8 javascript engine and uses the script
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
    bool parseResponse(MessageFrame const &msgFrame,
                       QList<Data> &listData,
                       ParseMode parseMode=PER_RESP_DATA);

    // saveNumAndLitData
    // * helper function that saves the numerical
    //   and literal data interpreted with the
    //   vehicle response from the v8 context
    void saveNumAndLitData(Data &myData);

    // getErrorStream
    QTextStream & getErrorStream();

    // stringToBaseDec
    // * converts a string representing a binary or
    //   hex number ("0x_") and ("0b_") into its
    //   corresponding decimal numeric string
    void stringToBaseDec(QString &parseExpr);

    // stringToUInt
    // * converts a string represengint a binary,
    //   hex, or decimal number into a uint
    uint stringToUInt(bool &convOk, QString const &parseStr);

    // convTextFileToQStr
    // * converts a text file into a QString
    bool convTextFileToQStr(QString const &filePath,
                            QString & fileDataAsStr);

    /*
    // DEPRECATED
    bool parseSinglePartResponse(MessageFrame const &msgFrame,
                                 QList<Data> &listDataResults);
    bool parseMultiPartResponse(MessageFrame const &msgFrame,
                                QList<Data> &listDataResults);
    */

    QMap<ubyte,QByteArray> m_mapValToHexByte;
    QMap<QByteArray,ubyte> m_mapHexByteToVal;

    // v8 vars
    v8::Persistent<v8::Context> m_v8_context;
    v8::Persistent<v8::Object> m_v8_listDataBytes;
    v8::Persistent<v8::Object> m_v8_listLitData;
    v8::Persistent<v8::Object> m_v8_listNumData;

    // pugixml vars
    QString m_xmlFilePath;
    pugi::xml_document m_xmlDoc;

    // errors
    QTextStream m_lkErrors;
    QString m_lkErrorString;
};
}

//#define OBDREFDEBUG getErrorStream()
#define OBDREFDEBUG qDebug()
#endif // PARSER_H
