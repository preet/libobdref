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

class ByteList
{
public:
    QList<ubyte> data;

    bool operator < (ByteList const &a) const
    {
        int valA=0;  int valB=0;

        for(int i=0; i < a.data.size(); i++)
        {   valA += a.data.at(i) * int(pow(256, a.data.size()-(i+1)));   }

        for(int i=0; i < data.size(); i++)
        {   valB += data.at(i) * int(pow(256, data.size()-(i+1)));   }

        return (valA < valB);
    }

    bool operator == (ByteList const &a) const
    {
        if(a.data.size() == data.size())
        {
            for(int i=0; i < a.data.size(); i++)
            {
                if(a.data.at(i) != data.at(i))
                {   return false;   }
            }
            return true;
        }
        return false;
    }
};


class LiteralData
{
public:
    LiteralData() : value(false) {}

    bool value;
    QString valueIfFalse;
    QString valueIfTrue;
    QString property;
    QString desc;
};

class NumericalData
{
public:
    NumericalData() : value(0),min(0),max(0) {}

    double value;
    double min;
    double max;
    QString units;
    QString desc;
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

class ParseInfo
{
public:
    ParseInfo() : isLiteral(false),isNumerical(false) {}

    QString expr;
    bool isLiteral;
    bool isNumerical;
    LiteralData literalData;
    NumericalData numericalData;
    QStringList listConditions;
};

class MessageData
{
public:
    MessageData() : reqDataDelayMs(0) {}

    ByteList            reqDataBytes;
    ByteList            expDataPrefix;
    QList<ByteList>     listRawDataFrames;      // [list of raw data frames]

                                                    // 'raw' meaning including all received
                                                    // bytes (header, prefix, etc)

                                                    // each entry in the list represents a
                                                    // separate data frame, which may originate
                                                    // from different source addresses

    QList<ByteList>     listHeaders;

    QList<ByteList>     listCleanData;          // [list of cleaned data]

                                                    // after multi-frame/multiple node responses
                                                    // have been processed, they are stored in
                                                    // this list (which does not contain header
                                                    // or prefix bytes) for parsing

                                                    // note that unlike listRawDataFrames, this
                                                    // list has no 'frames' concept -- the
                                                    // separate entries indicate responses from
                                                    // different addresses

    QString     expDataBytes;                       // explain 'N', '2N' stuff here
    uint        reqDataDelayMs;
};

class MessageFrame
{
public:
    QString spec;
    QString protocol;
    QString address;
    QString name;
    uint baudRate;

    ByteList  reqHeaderBytes;
    ByteList  expHeaderBytes;

    QList<MessageData>  listMessageData;            // [list of message data for this chain]

                                                    // we use a list to account for special
                                                    // parameters that spread their information
                                                    // over multiple messages -- the majority
                                                    // of parameters will only have one set
                                                    // of MessageData

    QString             parseScript;                // [script (js) to parse message bytes into data]
};


}
#endif // MESSAGE_H
