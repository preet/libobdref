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

int bad_args()
{
    qDebug() << "Pass the definitions file, spec, protocol,"
                "and address in as arguments:";
    qDebug() << "./test_spec /path/to/obd2.xml spec=\"SAEJ1979\" "
                "protocol=\"ISO 14230\" address=\"Default\"";

    return -1;
}

int test_failed()
{
    qDebug() << "////////////////////////////////////////////////";
    qDebug() << g_test_desc << "failed!";
}

int main(int argc, char* argv[])
{
    // we expect a single argument that specifies
    // the path to the test definitions file
    QString path_definitions,spec,protocol,address;
    if(argc == 5)   {
        path_definitions=QString(argv[1]);

        spec=QString(argv[2]);
        spec = spec.mid(spec.indexOf("=")+1);

        protocol=QString(argv[3]);
        protocol = protocol.mid(protocol.indexOf("=")+1);

        address=QString(argv[4]);
        address = address.mid(address.indexOf("=")+1);
    }
    else  {
        return bad_args();
    }

    // create parser
    bool ok=false;
    obdref::Parser parser(path_definitions,ok);
    if(!ok) { return -1; }

    // get parameter list
    QStringList listParams =
        parser.GetParameterNames(spec,protocol,address);

    if(listParams.isEmpty())   {
        qDebug() << "Error: no params found! "
                    "did you make a typo?";
        return -1;
    }

    g_debug_output = false;
    g_test_desc = spec+":"+protocol+":"+address;
    bool randomizeHeader = true;
    for(int i=0; i < listParams.size(); i++)
    {
        // build parameter
        obdref::ParameterFrame param;
        param.spec = spec;
        param.protocol = protocol;
        param.address = address;
        param.name = listParams[i];

        if(!parser.BuildParameterFrame(param))   {
            qDebug() << "Error: could not build frame "
                        "for param:" << listParams[i];
            return test_failed();
        }

        if(param.parseProtocol < 0xA00)   {
            sim_vehicle_message_legacy(param,1,randomizeHeader);
            sim_vehicle_message_legacy(param,3,randomizeHeader);
        }
        else if(param.parseProtocol == obdref::PROTOCOL_ISO_14230)   {
            sim_vehicle_message_iso14230(param,1,randomizeHeader);
            sim_vehicle_message_iso14230(param,3,randomizeHeader);
        }
        else if(param.parseProtocol == obdref::PROTOCOL_ISO_15765)   {
            sim_vehicle_message_iso15765(param,1,randomizeHeader);
            sim_vehicle_message_iso15765(param,3,randomizeHeader);
        }
        else   {
            qDebug() << "Error: unknown protocol" << listParams[i];
            return test_failed();
        }

        QList<obdref::Data> listData;
        if(parser.ParseParameterFrame(param,listData))  {
            if(g_debug_output)  {
                print_message_frame(param);
                print_parsed_data(listData);
            }
        }
        else   {
            qDebug() << "Error: could not parse parameter" << listParams[i];
            return test_failed();
        }
    }

    qDebug() << "////////////////////////////////////////////////";
    qDebug() << g_test_desc << "passed!";
    return 0;
}
