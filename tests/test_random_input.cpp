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
    if(filePath.isEmpty())
    {
        filePath = "/home/preet/Dev/obdref/definitions/obd2.xml";
    }

    // read in xml definitions file and globals js
    obdref::Parser myParser(filePath,opOk);
    if(!opOk)
    {   qDebug() << "Reading in XML and JS Failed! Exiting...";

        QStringList listErrors = myParser.GetLastKnownErrors();
        for(int i=0; i < listErrors.size(); i++)
        {   qDebug() << listErrors.at(i);   }

        return -1;
    }
    qDebug() << "OBDREF: Successfully read in XML Defs and JS globals!";


    // get a list of default parameters
    QStringList myParamList;
    myParamList = myParser.GetParameterNames("SAEJ1979",
                                             "ISO 15765-4 Standard",
                                             "Default");

    for(int i=0; i < myParamList.size(); i++)
    {
        // build a message frame for the current param
        obdref::MessageFrame myMsg;
        myMsg.spec = "SAEJ1979";
        myMsg.protocol = "ISO 15765-4 Standard";
        myMsg.address = "Default";
        myMsg.name = myParamList.at(i);

        opOk = myParser.BuildMessageFrame(myMsg);
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
            obdref::ByteList randomData;
            srand(j+52);
            int prefixSize;

//            // response 1
            randomData.data << 0x07 << 0xE9 << 0x05;
            randomData.data.append(myMsg.listMessageData[j].expDataPrefix.data);
            prefixSize = myMsg.listMessageData[j].expDataPrefix.data.size();
            for(int k=0; k < 7-prefixSize; k++)
            {
                obdref::ubyte myDataByte = obdref::ubyte(rand() % 256);
                randomData.data << myDataByte;
            }
            myMsg.listMessageData[j].listRawDataFrames.append(randomData);
            qDebug() << randomData.data;
            randomData.data.clear();


//            // response 2
//            srand(j+32);
//            randomData.data << 0x07 << 0xEB << 0x26;
//            randomData.data.append(myMsg.listMessageData[j].expDataPrefix.data);
//            prefixSize = myMsg.listMessageData[j].expDataPrefix.data.size();
//            for(int k=0; k < 7-prefixSize; k++)
//            {
//                obdref::ubyte myDataByte = obdref::ubyte(rand() % 256);
//                randomData.data << myDataByte;
//            }
//            myMsg.listMessageData[j].listRawDataFrames.append(randomData);
//            randomData.data.clear();

//            // response 3
//            srand(j+1);
//            randomData.data << 0x07 << 0xA9 << 0x03;
//            randomData.data.append(myMsg.listMessageData[j].expDataPrefix.data);
//            prefixSize = myMsg.listMessageData[j].expDataPrefix.data.size();
//            for(int k=0; k < 7-prefixSize; k++)
//            {
//                obdref::ubyte myDataByte = obdref::ubyte(rand() % 256);
//                randomData.data << myDataByte;
//            }
//            myMsg.listMessageData[j].listRawDataFrames.append(randomData);
//            randomData.data.clear();

            // response 4
//            srand(j+32);
//            randomData.data << 0x07 << 0x69 << 0x07;
//            randomData.data.append(myMsg.listMessageData[j].expDataPrefix.data);
//            prefixSize = myMsg.listMessageData[j].expDataPrefix.data.size();
//            for(int k=0; k < 7-prefixSize; k++)
//            {
//                obdref::ubyte myDataByte = obdref::ubyte(rand() % 256);
//                randomData.data << myDataByte;
//            }
//            myMsg.listMessageData[j].listRawDataFrames.append(randomData);
//            randomData.data.clear();
        }

        // parse message frame
        QList<obdref::Data> listData;
        opOk = myParser.ParseMessageFrame(myMsg, listData);

        if(!opOk || listData.size() == 0)
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
        qDebug() << myParamList.at(i) << "|" << listData.size() << "responses";

        for(int i=0; i < listData.size(); i++)
        {
            qDebug() << "\n[From Address]" << listData.at(i).srcAddress;

            if(listData.at(i).listLiteralData.size() > 0)
            {   qDebug() << "[Literal Data]";   }

            for(int j=0; j < listData.at(i).listLiteralData.size(); j++)
            {
                if(listData.at(i).listLiteralData.at(j).value)
                {
                    qDebug() << listData.at(i).listLiteralData.at(j).property <<
                    "  " << listData.at(i).listLiteralData.at(j).valueIfTrue;
                }
                else
                {
                    qDebug() << listData.at(i).listLiteralData.at(j).property <<
                    "  " << listData.at(i).listLiteralData.at(j).valueIfFalse;
                }
            }

            if(listData.at(i).listNumericalData.size() > 0)
            {   qDebug() << "[Numerical Data]";   }

            for(int j=0; j < listData.at(i).listNumericalData.size(); j++)
            {
                qDebug() << listData.at(i).listNumericalData.at(j).value
                         << listData.at(i).listNumericalData.at(j).units;
            }
        }
        qDebug() << "================================================";
    }

    return 0;
}
