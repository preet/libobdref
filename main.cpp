#include "parser.h"

using namespace std;

int main()
{
    bool parsedOk;
    QString filePath = "/home/preet/Dev/obdref/obd2.xml";
    obdref::Parser myParser(filePath,parsedOk);

    obdref::Message someMsg;
    someMsg.dataBytesSansPrefix.append(char(25));     // A
    someMsg.dataBytesSansPrefix.append(char(11));     // B
    someMsg.dataBytesSansPrefix.append(char(75));     // C
    someMsg.dataBytesSansPrefix.append(char(34));     // D
    someMsg.dataBytesSansPrefix.append(char(58));     // E
    someMsg.dataBytesSansPrefix.append(char(23));     // F
    someMsg.dataBytesSansPrefix.append(char(43));     // G
    someMsg.dataBytesSansPrefix.append(char(55));     // H

//    myParser.BuildMessage("SAEJ1979","ISO 14230-4",
//                          "Default","Fuel Type",someMsg);

    QString someString;
    myParser.ParseMessage(someMsg,someString);

    return 0;
}

