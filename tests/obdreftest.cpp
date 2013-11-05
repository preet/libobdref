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

#include <sys/time.h>
#include "parser.h"

// ========================================================================== //
// ========================================================================== //

// global test vars
QString g_test_desc;
bool g_debug_output;

// ========================================================================== //
// ========================================================================== //

obdref::ubyte randomByte()
{   return obdref::ubyte(rand() % 255);  }

QString bytelist_to_string(obdref::ByteList byteList)
{
    QString bts;
    for(int i=0; i < byteList.size(); i++)   {
        QString byteStr = QString::number(byteList[i],16);
        while(byteStr.length() < 2)   {
            byteStr.prepend("0");
        }
        byteStr.append(" ");
        byteStr = byteStr.toUpper();
        bts += byteStr;
    }
    return bts;
}

// ========================================================================== //
// ========================================================================== //

void sim_vehicle_message_legacy(obdref::ParameterFrame &param,
                                int const frames,
                                bool const randomHeader)
{
    if(param.parseProtocol < 0xA00)   {
        // legacy obd message structure
        for(int i=0; i < param.listMessageData.size(); i++)   {
            obdref::MessageData &msg = param.listMessageData[i];
            int expDataByteCount = (msg.expDataByteCount > 0) ?
                        msg.expDataByteCount : 4;

            // create the header
            obdref::ByteList header = msg.expHeaderBytes;
            if(randomHeader)   {
                for(int j=0; j < msg.expHeaderBytes.length(); j++)   {
                    msg.expHeaderBytes[j] = 0x00;
                    msg.expHeaderMask[j] = 0x00;
                    header[j] = randomByte();
                }
            }
            // save frames
            for(int j=0; j < frames; j++)
            {   // for each frame
                obdref::ByteList data = msg.expDataPrefix;
                for(int k=0; k < expDataByteCount; k++)   {
                    data << randomByte();
                }
                obdref::ByteList frame;
                frame << header << data;
                msg.listRawFrames << frame;
            }
        }
    }
}

// ========================================================================== //
// ========================================================================== //

void sim_vehicle_message_iso14230(obdref::ParameterFrame &param,
                                  int const frames,
                                  bool const randomizeHeader,
                                  int const iso14230_headerLength)
{
    if(param.parseProtocol == obdref::PROTOCOL_ISO_14230)
    {
        for(int i=0; i < param.listMessageData.size(); i++)   {
            obdref::MessageData &msg = param.listMessageData[i];

            // data length
            obdref::ubyte expDataByteCount =
                (msg.expDataByteCount > 0) ? msg.expDataByteCount : 4;

            obdref::ubyte dataLength =
                    expDataByteCount+msg.expDataPrefix.size();

            // create the header
            // for iso14230 expHeaderBytes is always [F] [T] [S]
            obdref::ByteList header;

            if(iso14230_headerLength == 1)   { // [F]
                header << (0x3F & dataLength);
            }
            else if(iso14230_headerLength == 2)   { // [F] [L]
                header << (msg.expHeaderBytes[0] & 0x3F) << dataLength;
            }
            else if(iso14230_headerLength == 3)   { // [F] [T] [S]
                header << (0x3F & dataLength)
                       << msg.expHeaderBytes[1]
                       << msg.expHeaderBytes[2];
                // encode that [T] and [S] is present in [F] byte
                header[0] |= 0x80;  // or 0xC0 it doesnt matter for sim
            }
            else if(iso14230_headerLength == 4)   { // [F] [T] [S] [L]
                header << msg.expHeaderBytes[0]
                       << msg.expHeaderBytes[1]
                       << msg.expHeaderBytes[2]
                       << dataLength;
                // encode that [T] and [S] is present in [F] byte
                if((header[0] & 0xC0) == 0)   {
                    header[0] |= 0x80;  // or 0xC0 it doesnt matter for sim
                }
            }

            if(randomizeHeader)   {
                msg.expHeaderBytes[1] = randomByte();
                msg.expHeaderMask[1] = 0x00;

                msg.expHeaderBytes[2] = randomByte();
                msg.expHeaderMask[2] = 0x00;
            }
            // save frames
            for(int j=0; j < frames; j++)
            {   // for each frame
                obdref::ByteList data = msg.expDataPrefix;
                for(int k=0; k < expDataByteCount; k++)   {
                    data << randomByte();
                }
                obdref::ByteList frame;
                frame << header << data;
                msg.listRawFrames << frame;
            }
        }
    }
}

// ========================================================================== //
// ========================================================================== //

void sim_vehicle_message_iso15765(obdref::ParameterFrame &param,
                                  int const frames,
                                  bool const randomizeHeader)
{
    if(param.parseProtocol == obdref::PROTOCOL_ISO_15765)   {
        for(int i=0; i < param.listMessageData.size(); i++)   {
            obdref::MessageData &msg = param.listMessageData[i];
            obdref::ByteList header = msg.expHeaderBytes;
            if(randomizeHeader)   {
                for(int j=0; j < header.size(); j++)   {
                    header[j] = randomByte();
                    msg.expHeaderMask[j] = 0x00;
                }
            }

            if(frames == 1)   {   // single frame
                obdref::ByteList data;
                data << 0x07;   // pci byte
                data << msg.expDataPrefix;
                for(int j=0; j < 8-(msg.expDataPrefix.size()+1); j++)   {
                    data << randomByte();
                }

                obdref::ByteList frame;
                frame << header << data;
                msg.listRawFrames << frame;
            }
            else if(frames > 1)   {
                // multi frame
                int dataLength=(frames*7)-1;
                obdref::ubyte dataLengthLow = dataLength & 0xFF;
                obdref::ubyte dataLengthHigh = (dataLength >> 8) && 0xFF;

                if((dataLengthHigh & 0xF0) != 1)   {
                    dataLengthHigh = dataLengthHigh & 0x0F;
                    dataLengthHigh = dataLengthHigh | 0x10;
                }
                obdref::ByteList catData; // concatenated data
                catData << msg.expDataPrefix;
                for(int j=0; j < dataLength-msg.expDataPrefix.size(); j++)   {
                    catData << randomByte();
                }

                for(int f=0; f < frames; f++)   {
                    if(f==0)   {
                        // first frame
                        obdref::ByteList data;
                        data << dataLengthHigh << dataLengthLow;
                        for(obdref::ubyte i=0; i < 6; i++)   {
                            data << catData.takeAt(0);
                        }
                        obdref::ByteList frame;
                        frame << header << data;
                        msg.listRawFrames << frame;
                    }
                    else   {
                        // consecutive frame
                        obdref::ubyte cfNumber = f%0x10;
                        obdref::ubyte cfPciByte = 0x20 | cfNumber;
                        obdref::ByteList data;
                        data << cfPciByte;
                        for(obdref::ubyte i=0; i < 7; i++)   {
                            data << catData.takeAt(0);
                        }
                        obdref::ByteList frame;
                        frame << header << data;
                        msg.listRawFrames << frame;
                    }
                }
            }
        }
    }
}


// ========================================================================== //
// ========================================================================== //

void print_message_data(obdref::MessageData const &msgData)
{
    qDebug() << "------------------------------------------------";
    qDebug() << "[Request Header Bytes]: " << bytelist_to_string(msgData.reqHeaderBytes);
    qDebug() << "[List Request Data Bytes]";
    for(int j=0; j < msgData.listReqDataBytes.size(); j++)   {
        qDebug() << "  req:" << j << ":" << bytelist_to_string(msgData.listReqDataBytes[j]);
    }
    qDebug() << "[Request Delay]: " << msgData.reqDataDelayMs;
    qDebug() << "[Expected Header Bytes]: " << bytelist_to_string(msgData.expHeaderBytes);
    qDebug() << "[Expected Header Mask]: " << bytelist_to_string(msgData.expHeaderMask);
    qDebug() << "[Expected Data Prefix]: " << bytelist_to_string(msgData.expDataPrefix);
    qDebug() << "[Expected Number of Data Bytes]: " << msgData.expDataByteCount;
    qDebug() << "------------------------------------------------";
    qDebug() << "listRawFrames:";
    for(int i=0; i < msgData.listRawFrames.size(); i++)   {
        qDebug() << i << ":" << bytelist_to_string(msgData.listRawFrames[i]);
    }
    qDebug() << "------------------------------------------------";
    qDebug() << "listCleaned:";
    for(int i=0; i < msgData.listHeaders.size(); i++)   {
        qDebug() << i << ":"
                 << bytelist_to_string(msgData.listHeaders[i])
                 << bytelist_to_string(msgData.listData[i]);
    }
    qDebug() << "------------------------------------------------";
}

// ========================================================================== //
// ========================================================================== //

void print_message_frame(obdref::ParameterFrame const &msgFrame)
{
    qDebug() << "================================================";
    qDebug() << "[Specification]: " << msgFrame.spec;
    qDebug() << "[Protocol]: " << msgFrame.protocol;
    qDebug() << "[Address]: " << msgFrame.address;
    qDebug() << "[Name]: " << msgFrame.name;

    if(msgFrame.protocol.contains("15765"))   {
        qDebug() << "ISO 15765: Add PCI Bytes?"
                 << msgFrame.iso15765_addPciByte;

        qDebug() << "ISO 15765: Split into frames?"
                 << msgFrame.iso15765_splitReqIntoFrames;
    }

    for(int i=0; i < msgFrame.listMessageData.size(); i++)   {
        qDebug() << "-" << i << "-";
        print_message_data(msgFrame.listMessageData[i]);
    }
}

// ========================================================================== //
// ========================================================================== //

void print_parsed_data(QList<obdref::Data> &listData)
{
    for(int i=0; i < listData.size(); i++)
    {
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
            if(!listData.at(i).listNumericalData.at(j).property.isEmpty())
            {   qDebug() << listData.at(i).listNumericalData.at(j).property;   }

            qDebug() << listData.at(i).listNumericalData.at(j).value
                     << listData.at(i).listNumericalData.at(j).units;
        }
    }

    qDebug() << "================================================";
}


