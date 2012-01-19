#include "parser.h"

int main()
{
    // read in xml file
    bool opOk = false;
    QString filePath = "/home/preet/Dev/obdref/obd2.xml";
    obdref::Parser myParser(filePath,opOk);

    if(!opOk)
    {   std::cerr << "Reading in XML Failed! Exiting..." << std::endl; return -1;   }


    // build message frame
    obdref::MessageFrame myMsg;
    opOk = myParser.BuildMessageFrame("SAEJ1979","ISO 15765-4 Standard",
                                      "Default","Freeze Frame DTC",myMsg);
    if(!opOk)
    {   std::cerr << "BuildMessageFrame Failed! Exiting..." << std::endl; return -1;   }


    // parse message frame
    obdref::Data myData;
    myParser.ParseMessageFrame(myMsg, myData);

    if(!opOk)
    {   std::cerr << "ParseMessageFrame Failed! Exiting..." << std::endl; return -1;   }










//    myParser.BuildMessage("SAEJ1979","ISO 14230-4",
//                          "Default","Fuel Type",someMsg);


//    obdref::Message someMsg;
//    someMsg.dataBytesSansPrefix.append(char(01));     // A
//    someMsg.dataBytesSansPrefix.append(char(25));     // B
//    someMsg.dataBytesSansPrefix.append(char(75));     // C
//    someMsg.dataBytesSansPrefix.append(char(34));     // D
//    someMsg.dataBytesSansPrefix.append(char(58));     // E
//    someMsg.dataBytesSansPrefix.append(char(23));     // F
//    someMsg.dataBytesSansPrefix.append(char(43));     // G
//    someMsg.dataBytesSansPrefix.append(char(55));     // H

//    QString someString;

//    someString = "A & B";
//    myParser.ParseMessage(someMsg,someString);

//    someString = "C & D";
//    myParser.ParseMessage(someMsg,someString);

//    someString = "A | B";
//    myParser.ParseMessage(someMsg,someString);

//    someString = "~C";
//    myParser.ParseMessage(someMsg,someString);

    return 0;
}
