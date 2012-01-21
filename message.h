#ifndef MESSAGE_H
#define MESSAGE_H

#include <QStringList>

namespace obdref
{

typedef unsigned char ubyte;

class LiteralData
{
public:
    bool value;
    QString valueIfFalse;
    QString valueIfTrue;
    QString desc;
};

class NumericalData
{
public:
    double value;
    double min;
    double max;
    QString units;
    QString desc;
};

class Data
{
public:
    QString desc;
    QList<LiteralData> listLiteralData;
    QList<NumericalData> listNumericalData;
};

class ParseInfo
{
public:
    QString expr;
    QStringList listConditions;
    bool isLiteral;                     // TODO default to false!
    bool isNumerical;                   // TODO default to false!
    LiteralData literalData;
    NumericalData numericalData;
};

class MessageData
{
public:
    QList<ubyte>    reqDataBytes;
    QList<ubyte>    expDataPrefix;
    QList<ubyte>    dataBytes;                      // expect data bytes SANS prefix

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

    QList<ubyte>  reqHeaderBytes;
    QList<ubyte>  expHeaderBytes;

    QList<MessageData>  listMessageData;            // [list of message data for this chain]
                                                    // we use a list to account for special
                                                    // parameters that spread their information
                                                    // over multiple messages -- the majority
                                                    // of parameters will only have one set
                                                    // of MessageData

    QList<ParseInfo>    listParseInfo;              // [list of parse information]
                                                    // contains all possible parse expressions
                                                    // for this parameter; each parse expr may
                                                    // have multiple conditions as well
};

// Data Description
// List of Literal Data [value, desc]
// List of Numerical Data [with value, units, min, max, desc]

// dunno what to call non numerical property types...
// LexicalData
// ExpressedData
// LiteralData

// example: Request Stored Diagnostic Trouble Codes
// Data Description: "Request Stored Diagnostic Trouble Codes"
// List of Literal Data:
//      value:"P", desc:"First DTC Character is P (Powertrain)"
// List of Numerical Data:
//      value: "0", units:"", min:"0", max:"3", desc:"Second DTC Character"
//      value: "4", units:"", min:"0", max:"F", desc:"Third DTC Character"
//      value: "5", units:"", min:"0", max:"F", desc:"Fourth DTC Character"
//      value: "5", units:"", min:"0", max:"F", desc:"Fifth DTC Character"

// example: Engine Coolant Temperature
// Data Description: "Engine Coolant Temperature"
// List of Literal Data: (empty)
// List of Numerical Data:
//      value: "24", units:"C", min:"-40", max:"215", desc:""

// example: Fuel System Status
// Data Description: Fuel System Status
// List of Literal Data:
//      value: "Fuel System 1: Open Loop: Insufficient Engine Temperature" desc:""
//      value: "Fuel System 2: Open Loop: Due To System Failure" desc:""
// List of Numerical Data: (empty)




}
#endif // MESSAGE_H
