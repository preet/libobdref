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

#include <cstdlib>
#include "parser.h"

int main(int argc, char* argv[])
{
    // we expect a single argument that's the path
    // to the definitions file to be tested
    bool opOk = false;
    QString filePath(argv[1]);


    // read in xml definitions file
    obdref::Parser myParser(filePath,opOk);
    if(!opOk)
    {   qDebug() << "Reading in XML Failed! Exiting...";

        QStringList listErrors = myParser.GetLastKnownErrors();
        for(int i=0; i < listErrors.size(); i++)
        {   qDebug() << listErrors.at(i);   }

        return -1;
    }
    qDebug() << "OBDREF: Successfully read in XML Defs file";


    // get a list of default parameters
    QStringList myParamList;
    myParamList = myParser.GetParameterNames("SAEJ1979",
                                             "ISO 15765-4 Standard",
                                             "Default");
    for(int i=0; i < myParamList.size(); i++)
    {
        // build a message frame for the current param
        obdref::MessageFrame myMsg;
        opOk = myParser.BuildMessageFrame("SAEJ1979",
                                          "ISO 15765-4 Standard",
                                          "Default",
                                          myParamList.at(i),
                                          myMsg);
        if(!opOk)
        {
            qDebug() << "BuildMessageFrame for" << myParamList.at(i)
                     << "Failed! Exiting...";

            QStringList listErrors = myParser.GetLastKnownErrors();
            for(int i=0; i < listErrors.size(); i++)
            {   qDebug() << listErrors.at(i);   }

            return -1;
        }

        // generate some random data to pretend we
        // have an actual device response
        for(int j=0; j < myMsg.listMessageData.size(); j++)
        {
            srand(j+123);
            for(int k=0; k < 99; k++)
            {
                // generate data bytes (0-255)
                obdref::ubyte myDataByte = obdref::ubyte(rand() % 256);
                myMsg.listMessageData[j].dataBytes.append(myDataByte);
            }
        }

        // parse message frame
        obdref::Data myData;
        opOk = myParser.ParseMessageFrame(myMsg, myData);
        if(!opOk)
        {
            qDebug() << "ParseMessageFrame for" << myParamList.at(i)
                     << "Failed! Exiting...";

            QStringList listErrors = myParser.GetLastKnownErrors();
            for(int i=0; i < listErrors.size(); i++)
            {   qDebug() << listErrors.at(i);   }

            return -1;
        }

        // print out data
        qDebug() << "================================================";
        qDebug() << myParamList.at(i);
        if(myData.listLiteralData.size() > 0)
        {
            qDebug() << "[Literal Data]";
            for(int i=0; i < myData.listLiteralData.size(); i++)
            {
                if(myData.listLiteralData[i].value)
                {
                    qDebug() << myData.listLiteralData[i].property
                             << myData.listLiteralData[i].valueIfTrue;
                }
                else
                {
                    qDebug() << myData.listLiteralData[i].property
                             << myData.listLiteralData[i].valueIfFalse;
                }
            }
        }

        if(myData.listNumericalData.size() > 0)
        {
            qDebug() << "[Numerical Data]";
            for(int i=0; i < myData.listNumericalData.size(); i++)
            {
                qDebug() << myData.listNumericalData[i].value
                         << myData.listNumericalData[i].units;
            }
        }
        qDebug() << "================================================";
    }









    return 0;
}
