/*
   This source is part of libobdref

   Copyright (C) 2012,2013 Preet Desai (preet.desai@gmail.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

// To add libobdref to your project, include parser.h
#include "obdref/parser.h"

void elm327_write(QByteArray reqHeader,QByteArray reqData) {}
QByteArray elm327_read() { QByteArray ba("486B10410C2ABC"); return ba; }
void print_parsed_data(QList<obdref::Data> const &listData);

int main(int argc, char * argv[])
{
    // Expect a single argument that specifies
    // the path to the test definitions file
    bool opOk = false;
    QString filePath(argv[1]);
    if(filePath.isEmpty())   {
       qDebug() << "Pass the obd definitions file in as an argument:";
       qDebug() << "./quickstart /path/to/obd2.xml";
       return -1;
    }

    // Create a Parser object with a path to the
    // obd2.xml definitions file
    bool ok = false;
    obdref::Parser parser("/home/preet/Dev/projects/libobdref/definitions/obd2.xml",ok);
    if(!ok) { qDebug() << "error creating parser"; return -1; }

    // Create a Parameter frame and fill out the spec,
    // protocol, address and name by referring to the
    // definitions file.
    obdref::ParameterFrame pf;
    pf.spec = "SAEJ1979";
    pf.protocol = "ISO 9141-2";
    pf.address = "Default";
    pf.name = "Engine RPM";

    // Tell obdref to fill in important information by
    // calling BuildParameterFrame -- if required, this
    // will generate the message you need to send to the
    // vehicle to request the parameter.
    ok = parser.BuildParameterFrame(pf);
    if(!ok) { qDebug() << "error building parameter"; return -1; }

    // Send the message created by obdref to your vehicle.
    // This step will vary based on the interface you are
    // using. Here is an example for an ELM327 compatible
    // scan tool.

    // Each entry in listMessageData is typically tied
    // to a vehicle request. Here we know "Engine RPM"
    // only has a single request.
    obdref::MessageData &msg = pf.listMessageData[0];

    // ELM327 Adapters expect data in ASCII characters,
    // so we have to convert the request header and data
    // before sending a message out
    QByteArray elmRequestHeader,elmRequestData;

    // Convert the header for the ELM
    for(int i=0; i < msg.reqHeaderBytes.size(); i++)   {
        elmRequestHeader.append(parser.ConvUByteToHexStr(msg.reqHeaderBytes[i]));
    }

    // Convert the data for the ELM
    // The request data is provided in a list because
    // it may have been split into frames for certain
    // protocols. In this case, we know there is only
    // one frame in the request.
    for(int i=0; i < msg.listReqDataBytes[0].size(); i++)   {
        elmRequestData.append(parser.ConvUByteToHexStr(msg.listReqDataBytes[0][i]));
    }

    // Now we have something that looks like:
    // elmRequestHeader: "686AF1"
    // elmRequestData: "010C"

    // Use the ELM adapter to send this request
    // to the vehicle
    elm327_write(elmRequestHeader,elmRequestData);

    // Get the response

    // Note:
    // libobdref expects the reponse to be input as a
    // series of frames with header and data bytes:
    // [h0 h1 h2 ... d0 d1 d2...]

    // Additional fields (like the check sum, or the
    // DLC for ISO 15765) should be discarded

    QByteArray resp = elm327_read();

    // We need to convert the response from the ELM
    // adapter and pass it back to libobdref
    QList<obdref::ubyte> listBytes;
    for(int i=0; i < resp.size(); i+=2)   {
        listBytes << parser.ConvHexStrToUByte(resp.mid(i,2));
    }

    // Save the frame in its corresponding message
    msg.listRawFrames << listBytes;

    // Create a data list to store the results and
    // parse the vehicle response
    QList<obdref::Data> listData;
    ok = parser.ParseParameterFrame(pf,listData);
    if(!ok) { qDebug() << "failed to parse response"; return -1; }

    // All responses are parsed into two kinds of data:
    // Numerical, and Literal. There can be any number of
    // each in each obdref::Data struct.
    print_parsed_data(listData);

    // Done
    return 0;
}

// helper print function
void print_parsed_data(QList<obdref::Data> const &listData)
{
    qDebug() << "================================================";
    for(int i=0; i < listData.size(); i++)   {
        // print literal data
        if(!listData[i].listLiteralData.isEmpty())   {
            qDebug() << "[Literal Data]";
            for(int j=0; j < listData[i].listLiteralData.size(); j++)   {
                obdref::LiteralData litData = listData[i].listLiteralData[j];
                if(litData.value)   {
                    qDebug() << litData.property << "  " << litData.valueIfTrue;
                }
                else   {
                    qDebug() << litData.property << "  " << litData.valueIfFalse;
                }
            }
        }
        // print numerical data
        if(!listData[i].listNumericalData.isEmpty())   {
            qDebug() << "[Numerical Data]";
            for(int j=0; j < listData[i].listNumericalData.size(); j++)   {
                obdref::NumericalData numData = listData[i].listNumericalData[j];
                if(!numData.property.isEmpty())   {
                    qDebug() << numData.property;
                }
                qDebug() << numData.value << numData.units;
            }
        }
    }
    qDebug() << "================================================";
}
