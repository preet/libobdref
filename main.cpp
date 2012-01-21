#include "parser.h"

int main()
{
    // read in xml file
    bool opOk = false;
    QString filePath = "/home/preet/Dev/obdref/obd2.xml";
    obdref::Parser myParser(filePath,opOk);

    if(!opOk)
    {   qDebug() << "Reading in XML Failed! Exiting...";
        qDebug() << myParser.GetLastKnownErrors();
        return -1;   }


    // build message frame
    obdref::MessageFrame myMsg;
    opOk = myParser.BuildMessageFrame("SAEJ1979","ISO 15765-4 Standard",
                                      "Default","MAF Air Flow Rate",myMsg);
    if(!opOk)
    {   qDebug() << "BuildMessageFrame Failed! Exiting...";
        qDebug() << myParser.GetLastKnownErrors();
        return -1;   }

    // pretend we got data from a device
   myMsg.listMessageData[0].dataBytes.append(0b11111111);
   myMsg.listMessageData[0].dataBytes.append(13);
   myMsg.listMessageData[0].dataBytes.append(21);
   myMsg.listMessageData[0].dataBytes.append(75);

    // parse message frame
    obdref::Data myData;
    opOk = myParser.ParseMessageFrame(myMsg, myData);

    if(!opOk)
    {   qDebug() << "ParseMessageFrame Failed! Exiting...";
        qDebug() << myParser.GetLastKnownErrors();
        return -1;   }

    qDebug() << myData.desc;
    qDebug() << myData.listNumericalData[0].value
             << myData.listNumericalData[0].units;

    return 0;
}
