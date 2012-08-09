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
#include "message.hpp"

namespace obdref
{

class Parser
{

public:
    Parser(QString const &filePath, bool &parsedOk);
    ~Parser();

    bool BuildMessageFrame(MessageFrame &msgFrame);

    bool ParseMessageFrame(MessageFrame &msgFrame,
                           QList<Data> &listDataResults);

    QStringList GetParameterNames(QString const &specName,
                                  QString const &protocolName,
                                  QString const &addressName);

    QStringList GetLastKnownErrors();

private:
    bool cleanRawData_Default(MessageFrame &msgFrame);
    bool cleanRawData_ISO_14230_4(MessageFrame &msgFrame);
    bool cleanRawData_ISO_15765_4(MessageFrame &msgFrame);

    void dumpRawDataToDebugInfo(QList<ByteList> const &listRawDataFrames);

    bool parseSinglePartResponse(MessageFrame const &msgFrame,
                                 QList<Data> &listDataResults);

    bool parseMultiPartResponse(MessageFrame const &msgFrame,
                                QList<Data> &listDataResults);

    void saveNumAndLitData(Data &myData);

    void convToDecEquivExpression(QString &parseExpr);

    uint stringToUInt(bool &convOk, QString const &parseStr);

    QTextStream & getErrorStream();

    bool convTextFileToQStr(QString const &filePath,
                            QString & fileDataAsStr);

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
