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
                                      "Default","O2S8_WR_lambda(1): Equivalence Ratio, Voltage",myMsg);
    if(!opOk)
    {   std::cerr << "BuildMessageFrame Failed! Exiting..." << std::endl; return -1;   }


    // pretend we got data from a device
   myMsg.listMessageData[0].dataBytes.append(char(92));    // byte
   myMsg.listMessageData[0].dataBytes.append(char(13));    // byte
   myMsg.listMessageData[0].dataBytes.append(char(21));    // byte

   // myMsg.listMessageData[1].dataBytes.append(char(83));    // byte

    // parse message frame
    obdref::Data myData;
    myParser.ParseMessageFrame(myMsg, myData);

    if(!opOk)
    {   std::cerr << "ParseMessageFrame Failed! Exiting..." << std::endl; return -1;   }

    return 0;
}
