#include "parser.h"

using namespace std;

int main()
{
    bool parsedOk;
    QString filePath = "/home/preet/Dev/obdref/obd2.xml";
    obdref::Parser myParser(filePath,parsedOk);

    obdref::Message someMsg;
    someMsg.dataBytesSansPrefix.append(char(0x16));     // A
    someMsg.dataBytesSansPrefix.append(char(0x17));     // B
    someMsg.dataBytesSansPrefix.append(char(0x12));     // C
    someMsg.dataBytesSansPrefix.append(char(0x18));     // D
    someMsg.dataBytesSansPrefix.append(char(0x55));     // E
    someMsg.dataBytesSansPrefix.append(char(0x11));     // F
    someMsg.dataBytesSansPrefix.append(char(0x22));     // G
    someMsg.dataBytesSansPrefix.append(char(0x13));     // H

//    myParser.BuildMessage("SAEJ1979","ISO 14230-4",
//                          "Default","Fuel Type",someMsg);

    QString someString;
    myParser.ParseMessage(someMsg,someString);

    return 0;
}

