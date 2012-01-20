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
                                      "Default","SSM Engine Speed",myMsg);
    if(!opOk)
    {   std::cerr << "BuildMessageFrame Failed! Exiting..." << std::endl; return -1;   }


    // pretend we got data from a device
    myMsg.listMessageData[0].dataBytes.append(char(32));    // byte resp1.A
    myMsg.listMessageData[1].dataBytes.append(char(13));    // byte resp2.A


    // parse message frame
    obdref::Data myData;
    myParser.ParseMessageFrame(myMsg, myData);

    if(!opOk)
    {   std::cerr << "ParseMessageFrame Failed! Exiting..." << std::endl; return -1;   }

    return 0;
}
