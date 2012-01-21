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

#include "parser.h"

#include <QString>
#include <QDebug>

int main()
{
    // read in xml file
    bool opOk = false;
    QString filePath = "/home/preet/Dev/obdref/definitions/obd2.xml";
    obdref::Parser myParser(filePath,opOk);

    if(!opOk)
    {   qDebug() << "Reading in XML Failed! Exiting...";

        QStringList listErrors = myParser.GetLastKnownErrors();
        for(int i=0; i < listErrors.size(); i++)
        {   qDebug() << listErrors.at(i);   }

        return -1;
    }


    // build message frame
    obdref::MessageFrame myMsg;
    opOk = myParser.BuildMessageFrame("SAEJ1979","ISO 15765-4 Standard",
                                      "Default","SSM Engine Speed",myMsg);
    if(!opOk)
    {   qDebug() << "BuildMessageFrame Failed! Exiting...";

        QStringList listErrors = myParser.GetLastKnownErrors();
        for(int i=0; i < listErrors.size(); i++)
        {   qDebug() << listErrors.at(i);   }

        return -1;
    }

    // pretend we got data from a device
    myMsg.listMessageData[0].dataBytes.append(0b11111111);
    myMsg.listMessageData[0].dataBytes.append(0);
    myMsg.listMessageData[0].dataBytes.append(21);
    myMsg.listMessageData[0].dataBytes.append(75);
    myMsg.listMessageData[0].dataBytes.append(75);
    myMsg.listMessageData[0].dataBytes.append(75);
    myMsg.listMessageData[1].dataBytes.append(0b11111111);
    myMsg.listMessageData[1].dataBytes.append(75);
    myMsg.listMessageData[1].dataBytes.append(75);
    myMsg.listMessageData[1].dataBytes.append(75);
    myMsg.listMessageData[1].dataBytes.append(75);
    myMsg.listMessageData[1].dataBytes.append(75);

    // parse message frame
    obdref::Data myData;
    opOk = myParser.ParseMessageFrame(myMsg, myData);

    if(!opOk)
    {   qDebug() << "ParseMessageFrame Failed! Exiting...";

        QStringList listErrors = myParser.GetLastKnownErrors();
        for(int i=0; i < listErrors.size(); i++)
        {   qDebug() << listErrors.at(i);   }

        return -1;
    }

    // print out data
    if(myData.listLiteralData.size() > 0)
    {
        qDebug() << "[Literal Data]";
        qDebug() << myData.desc;
        for(int i=0; i < myData.listLiteralData.size(); i++)
        {
            if(myData.listLiteralData[i].value)
            {   qDebug() << myData.listLiteralData[i].valueIfTrue;   }
            else
            {   qDebug() << myData.listLiteralData[i].valueIfFalse;   }
        }
    }

    if(myData.listNumericalData.size() > 0)
    {
        qDebug() << "\n";
        qDebug() << "[Numerical Data]";
        qDebug() << myData.desc;
        for(int i=0; i < myData.listNumericalData.size(); i++)
        {
            qDebug() << myData.listNumericalData[i].value
                     << myData.listNumericalData[i].units;
        }
    }


    return 0;
}
