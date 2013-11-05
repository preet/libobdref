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

#ifndef DATATYPES_H
#define DATATYPES_H

#include <QStringList>

namespace obdref
{

typedef quint8 ubyte;
typedef QList<ubyte> ByteList;

struct LiteralData
{
    LiteralData() : value(false) {}

    bool value;
    QString valueIfFalse;
    QString valueIfTrue;
    QString property;
};

struct NumericalData
{
    NumericalData() : value(0),min(0),max(0) {}

    double value;
    double min;
    double max;
    QString units;
    QString property;
};

struct Data
{
    QString paramName;
    QString srcName;
    QList<LiteralData> listLiteralData;
    QList<NumericalData> listNumericalData;
};

// MessageData
// * generic container for vehicle message data
// * the message data may represent data tied to
//   a (single) request or just be any data retreived
//   from the vehicle some other way (by monitoring
//   the bus, etc)
struct MessageData
{
    // Request Data
    // * used to put together a vehicle request
    //   message (sending a request is optional)
    ByteList        reqHeaderBytes;     // request header bytes
    QList<ByteList> listReqDataBytes;   // request data bytes (may be split into frames)
    quint32         reqDataDelayMs;     // delay before sending this request

    // Expected Data
    // * we expect the following to be seen in any
    //   message data received from the vehicle
    ByteList        expHeaderBytes;     // expected header bytes in the response
    ByteList        expHeaderMask;      // expected header bytes mask
    ByteList        expDataPrefix;      // expected data prefix
    int             expDataByteCount;   // expected data byte count (after prefix); a value
                                        // less than 0 means the expected length is unknown
    // Raw Data
    // * each entry in the list contains bytes received for
    //   a single data frame in the format [header] [data]
    // * the frames may have originated from different source
    //   addresses and do not need to be organized as such
    QList<ByteList> listRawFrames;

    // Cleaned Data
    // * unlike raw data, cleaned data has no 'frames', but
    //   is organized as one set of data bytes corresponding
    //   to its header:
    //   [header1] [d0 d1 d2 ...]
    //   [header1] [d0 d1 d2 ...]
    //   [header2] [d0 d1 d2 ...]
    QList<ByteList> listHeaders;
    QList<ByteList> listData;


    MessageData() :
        reqDataDelayMs(0),
        expDataByteCount(-1)
    {}
};

enum ParseMode
{
    // PARSE_SEPARATELY (default)
    // * The parse function is called once separately
    //   for each entry in MessageData.listCleanData
    PARSE_SEPARATELY,

    // PARSE_COMBINED
    // * The parse function is called only once for
    //   all of the data in all of the MessageFrames
    // * Individual MessageFrames are accessed with REQ(N)
    // * Individual entries in MessageData.listCleanData
    //   are accessed with DATA(N)
    PARSE_COMBINED
};

enum Protocol
{
    // Protocols that are less than 0xA00 are legacy
    // strict OBDII format, so they can be cleaned by
    // the cleanRawData_Legacy method

    PROTOCOL_SAE_J1850      = 0x001,
    PROTOCOL_ISO_9141_2     = 0x002,
    PROTOCOL_ISO_14230      = 0xA01,
    PROTOCOL_ISO_15765      = 0xA02,    // doesn't currently support
                                        // extended or mixed addressing
};

// ParameterFrame
// * struct that stores all the data for a single
//   parameter from the definitions file
class ParameterFrame
{
public:
    // Lookup Data
    // * info required to lookup a parameter
    //   from the definitions file
    QString             spec;
    QString             protocol;
    QString             address;
    QString             name;

    // ISO 15765 Settings
    // * flag to calculate and add the PCI byte
    //   when generating MessageData.reqDataBytes
    bool                iso15765_addPciByte;

    // * flag to split request data for this message
    //   into single frames (true by default)
    bool                iso15765_splitReqIntoFrames;

    // Options
    // * options that are set indirectly
    //   through the xml definitions file
    ParseMode           parseMode;
    Protocol            parseProtocol;
    bool                iso14230_addLengthByte;
    bool                iso15765_extendedId;
    bool                iso15765_extendedAddr;

    // Message Data
    // * list of message data for this parameter
    // * each MessageData struct is tied to at most
    //   one request -- multiple MessageData structs
    //   indicate that multiple requests are used to
    //   construct the parameter
    QList<MessageData>  listMessageData;

    // Parse
    // * contains the javascript function name that will
    //   be called in the definitions file to parse
    //   the MessageData into useful values
    int                functionKeyIdx;


    ParameterFrame() :
        iso15765_addPciByte(true),
        iso15765_splitReqIntoFrames(true),
        iso14230_addLengthByte(false),
        iso15765_extendedId(false),
        iso15765_extendedAddr(false),
        functionKeyIdx(-1)
    {}
};


}
#endif // DATATYPES_H
