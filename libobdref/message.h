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

#ifndef MESSAGE_H
#define MESSAGE_H

#include <math.h>
#include <QStringList>

namespace obdref
{

typedef unsigned char ubyte;
typedef QList<ubyte> ByteList;

class LiteralData
{
public:
    LiteralData() : value(false) {}

    bool value;
    QString valueIfFalse;
    QString valueIfTrue;
    QString property;
};

class NumericalData
{
public:
    NumericalData() : value(0),min(0),max(0) {}

    double value;
    double min;
    double max;
    QString units;
    QString property;
};

class Data
{
public:
    QString paramName;
    QString srcAddress;
    QString srcName;
    QList<LiteralData> listLiteralData;
    QList<NumericalData> listNumericalData;
};

class MessageData
{
public:
    MessageData() : reqDataDelayMs(0) {}

    // obd request message info
    ByteList            reqHeaderBytes;
    QList<ByteList>     listReqDataBytes;
    uint                reqDataDelayMs;

    // obd response message info
    ByteList            expHeaderBytes;
    ByteList            expHeaderMask;
    ByteList            expDataPrefix;
    QString             expDataBytes;

    // raw data frames -- each frame contains
    // received bytes for a single frame in
    // the format: "header bytes, data bytes"

    // each entry in the list represents a
    // separate data frame, which may originate
    // from different source addresses

    // raw data frames
    // * each frame contains bytes received
    //   for a single frame in the format
    //   [header bytes, data bytes]
    // * each entry in the list represents a
    //   separate frame, which may originate
    //   from different sources
    QList<ByteList>     listRawDataFrames;

    // cleaned device replies
    // * after the raw data frames have been
    //   processed, they are stored here
    // * note that unlike listRawDataFrames,
    //   this list has no concept of 'frames':
    //   separate entries indicate responses
    //   from different addresses
    QList<ByteList>     listHeaders;
    QList<ByteList>     listCleanData;
};

class MessageFrame
{
public:
    MessageFrame() :
        iso15765_addPciByte(true),
        iso15765_splitReqIntoFrames(true)
    {}

    QString spec;
    QString protocol;
    QString address;
    QString name;
    uint baudRate;

    // (ISO 15765)
    // flag to calculate and add pci
    // bytes to reqDataBytes, true
    // by default
    bool iso15765_addPciByte;

    // (ISO 15764)
    // flag to split request data
    // into single frame messages,
    // true by default
    bool iso15765_splitReqIntoFrames;

    // list of message data
    // * includes request/response bytes
    //   for a single message
    // * a list is used to account for some
    //   parameters that spread their info
    //   over multiple messages -- most will
    //   only have one entry in this list
    QList<MessageData>  listMessageData;

    // javascript to parse message bytes into
    // useful numerical or literal data
    QString             parseScript;
};


}
#endif // MESSAGE_H
