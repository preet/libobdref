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

    std::cerr << "####" << sizeof(int) << std::endl;

    // pretend we got data from a device
   myMsg.listMessageData[0].dataBytes.append(0b11111111);    // byte
   myMsg.listMessageData[0].dataBytes.append(13);    // byte
   myMsg.listMessageData[0].dataBytes.append(21);    // byte
   myMsg.listMessageData[0].dataBytes.append(75);    // byte

    // parse message frame
    obdref::Data myData;
    opOk = myParser.ParseMessageFrame(myMsg, myData);

    if(!opOk)
    {   std::cerr << "ParseMessageFrame Failed! Exiting..." << std::endl; return -1;   }

    return 0;
}
