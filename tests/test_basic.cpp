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

#include "obdreftest.h"

bool test_legacy(obdref::Parser & parser,
                 bool const randomizeHeader=false);

bool test_iso14230(obdref::Parser & parser,
                   bool const randomizeHeader=false,
                   int const headerLength=3,
                   bool const checkHeaderFormatByte=true);

bool test_iso15765(obdref::Parser & parser,
                   bool const randomizeHeader=false,
                   bool const extendedId=false);
                   
int main(int argc, char* argv[])
{
    // we expect a single argument that specifies
    // the path to the test definitions file
    bool opOk = false;
    QString filePath(argv[1]);
    if(filePath.isEmpty())   {
       qDebug() << "Pass the test definitions file in as an argument:";
       qDebug() << "./test_basic /path/to/test.xml";
       return -1;
    }

    // read in xml definitions file
    obdref::Parser parser(filePath,opOk);
    if(!opOk) { return -1; }

    g_debug_output = false;

    bool randHeaders=true;

    g_test_desc = "test legacy (sae j1850, iso 9141)";
    if(!test_legacy(parser,randHeaders))   {
        return -1;
    }

    g_test_desc = "test iso 14230 (1 byte header)";
    if(!test_iso14230(parser,randHeaders,1,false))   {
        return -1;
    }

    g_test_desc = "test iso 14230 (2 byte header)";
    if(!test_iso14230(parser,randHeaders,2,false))   {
        return -1;
    }

    g_test_desc = "test iso 14230 (3 byte header)";
    if(!test_iso14230(parser,randHeaders,3,true))   {
        return -1;
    }

    g_test_desc = "test iso 14230 (4 byte header)";
    if(!test_iso14230(parser,randHeaders,4,true))   {
        return -1;
    }

    g_test_desc = "test iso 15765 (standard ids)";
    if(!test_iso15765(parser,randHeaders,false))   {
        return -1;
    }

    g_test_desc = "test iso 15765 (extended ids)";
    if(!test_iso15765(parser,randHeaders,true))   {
        return -1;
    }
    
    return 0;
}

// ========================================================================== //
// ========================================================================== //

bool test_legacy(obdref::Parser & parser,
                 bool const randomizeHeader)
{
    QStringList listParams =
        parser.GetParameterNames("TEST","ISO 9141-2","Default");

    for(int i=0; i < listParams.size(); i++)   {
        obdref::ParameterFrame param;
        param.spec = "TEST";
        param.protocol = "ISO 9141-2";
        param.address = "Default";
        param.name = listParams[i];

        if(!parser.BuildParameterFrame(param))   {
            qDebug() << "Error: could not build frame "
                        "for param:" << listParams[i];
        }
        else   {
            if(     param.name == "T_REQ_NONE_RESP_SF_PARSE_SEP")   {
                sim_vehicle_message_legacy(param,1,randomizeHeader);
            }
            else if(param.name == "T_REQ_NON_RESP_MF_PARSE_COMBINED")   {
                sim_vehicle_message_legacy(param,1,randomizeHeader);
                sim_vehicle_message_legacy(param,1,randomizeHeader);
                sim_vehicle_message_legacy(param,1,randomizeHeader);
            }
            else if(param.name == "T_REQ_SINGLE_RESP_SF_PARSE_SEP")   {
                sim_vehicle_message_legacy(param,1,randomizeHeader);
            }
            else if(param.name == "T_REQ_SINGLE_RESP_MF_PARSE_SEP")   {
                sim_vehicle_message_legacy(param,2,randomizeHeader);
                sim_vehicle_message_legacy(param,2,randomizeHeader);
            }
            else if(param.name == "T_REQ_MULTI_RESP_SF_PARSE_SEP")   {
                sim_vehicle_message_legacy(param,1,randomizeHeader);
            }
            else if(param.name == "T_REQ_MULTI_RESP_SF_PARSE_COMBINED")   {
                sim_vehicle_message_legacy(param,1,randomizeHeader);
            }
            else if(param.name == "T_REQ_MULTI_RESP_MF_PARSE_COMBINED")   {
                sim_vehicle_message_legacy(param,2,randomizeHeader);
                sim_vehicle_message_legacy(param,1,randomizeHeader);
            }
            else   {
                continue;
            }

            QList<obdref::Data> listData;
            if(parser.ParseParameterFrame(param,listData))  {
                if(g_debug_output)  {
                    print_message_frame(param);
                    print_parsed_data(listData);
                }
                continue;
            }
        }
        qDebug() << "////////////////////////////////////////////////";
        qDebug() << g_test_desc << "failed!";
        return false;
    }
    qDebug() << "////////////////////////////////////////////////";
    qDebug() << g_test_desc << "passed!";
    return true;
}

// ========================================================================== //
// ========================================================================== //

bool test_iso14230(obdref::Parser & parser,
                   bool const randomizeHeader,
                   int const headerLength,
                   bool const checkHeaderFormatByte)
{
    QStringList listParams =
        parser.GetParameterNames("TEST","ISO 14230","Default");

    for(int i=0; i < listParams.size(); i++)   {
        obdref::ParameterFrame param;
        param.spec = "TEST";
        param.protocol = "ISO 14230";
        param.address = "Default";
        param.name = listParams[i];

        if(!parser.BuildParameterFrame(param))   {
            qDebug() << "Error: could not build frame "
                        "for param:" << listParams[i];
        }
        else   {
            if(checkHeaderFormatByte==false)   {
                // just makes testing multiple header lengths
                // at once a bit easier
                for(int j=0; j < param.listMessageData.size(); j++)   {
                    param.listMessageData[j].expHeaderMask[0] = 0x00;
                }
            }

            if(     param.name == "T_REQ_NONE_RESP_SF_PARSE_SEP")   {
                sim_vehicle_message_iso14230(param,1,randomizeHeader);
            }
            else if(param.name == "T_REQ_NON_RESP_MF_PARSE_COMBINED")   {
                sim_vehicle_message_iso14230(param,1,randomizeHeader);
                sim_vehicle_message_iso14230(param,1,randomizeHeader);
                sim_vehicle_message_iso14230(param,1,randomizeHeader);
            }
            else if(param.name == "T_REQ_SINGLE_RESP_SF_PARSE_SEP")   {
                sim_vehicle_message_iso14230(param,1,randomizeHeader);
            }
            else if(param.name == "T_REQ_SINGLE_RESP_MF_PARSE_SEP")   {
                sim_vehicle_message_iso14230(param,2,randomizeHeader);
                sim_vehicle_message_iso14230(param,2,randomizeHeader);
            }
            else if(param.name == "T_REQ_MULTI_RESP_SF_PARSE_SEP")   {
                sim_vehicle_message_iso14230(param,1,randomizeHeader);
            }
            else if(param.name == "T_REQ_MULTI_RESP_SF_PARSE_COMBINED")   {
                sim_vehicle_message_iso14230(param,1,randomizeHeader);
            }
            else if(param.name == "T_REQ_MULTI_RESP_MF_PARSE_COMBINED")   {
                sim_vehicle_message_iso14230(param,2,randomizeHeader);
                sim_vehicle_message_iso14230(param,1,randomizeHeader);
            }
            else   {
                continue;
            }

            QList<obdref::Data> listData;
            if(parser.ParseParameterFrame(param,listData))  {
                if(g_debug_output)  {
                    print_message_frame(param);
                    print_parsed_data(listData);
                }
                continue;
            }
        }
        qDebug() << "////////////////////////////////////////////////";
        qDebug() << g_test_desc << "failed!";
        return false;
    }
    qDebug() << "////////////////////////////////////////////////";
    qDebug() << g_test_desc << "passed!";
    return true;
}

// ========================================================================== //
// ========================================================================== //

bool test_iso15765(obdref::Parser & parser,
                   bool const randomizeHeader,
                   bool const extendedId)
{
    QString protocol = (extendedId) ?
        "ISO 15765 Extended Id" : "ISO 15765 Standard Id";

    QStringList listParams =
        parser.GetParameterNames("TEST",protocol,"Default");

    for(int i=0; i < listParams.size(); i++)   {
        obdref::ParameterFrame param;
        param.spec = "TEST";
        param.protocol = protocol;
        param.address = "Default";
        param.name = listParams[i];

        if(!parser.BuildParameterFrame(param))   {
            qDebug() << "Error: could not build frame "
                        "for param:" << listParams[i];
        }
        else   {

            if(     param.name == "T_REQ_NONE_RESP_SF_PARSE_SEP")   {
                sim_vehicle_message_iso15765(param,1,randomizeHeader);
            }
            else if(param.name == "T_REQ_NON_RESP_MF_PARSE_COMBINED")   {
                sim_vehicle_message_iso15765(param,1,randomizeHeader);
                sim_vehicle_message_iso15765(param,1,randomizeHeader);
                sim_vehicle_message_iso15765(param,1,randomizeHeader);
            }
            else if(param.name == "T_REQ_SINGLE_RESP_SF_PARSE_SEP")   {
                sim_vehicle_message_iso15765(param,1,randomizeHeader);
            }
            else if(param.name == "T_REQ_SINGLE_RESP_MF_PARSE_SEP")   {
                sim_vehicle_message_iso15765(param,3,randomizeHeader);
                sim_vehicle_message_iso15765(param,4,randomizeHeader);
            }
            else if(param.name == "T_REQ_MULTI_RESP_SF_PARSE_SEP")   {
                sim_vehicle_message_iso15765(param,1,randomizeHeader);
            }
            else if(param.name == "T_REQ_MULTI_RESP_SF_PARSE_COMBINED")   {
                sim_vehicle_message_iso15765(param,1,randomizeHeader);
            }
            else if(param.name == "T_REQ_MULTI_RESP_MF_PARSE_COMBINED")   {
                sim_vehicle_message_iso15765(param,2,randomizeHeader);
                sim_vehicle_message_iso15765(param,1,randomizeHeader);
            }
            else   {
                continue;
            }

            QList<obdref::Data> listData;
            if(parser.ParseParameterFrame(param,listData))  {
                if(g_debug_output)  {
                    print_message_frame(param);
                    print_parsed_data(listData);
                }
                continue;
            }
        }
        qDebug() << "////////////////////////////////////////////////";
        qDebug() << g_test_desc << "failed!";
        return false;
    }
    qDebug() << "////////////////////////////////////////////////";
    qDebug() << g_test_desc << "passed!";
    return true;
}
