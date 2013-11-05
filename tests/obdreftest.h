/*
   This source is part of osmsrender

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

#ifndef OBDREF_TEST_H
#define OBDREF_TEST_H

#include "parser.h"

// global test vars
extern QString g_test_desc;
extern bool g_debug_output;

// simulate vehicle messages
void sim_vehicle_message_legacy(obdref::ParameterFrame &param,
                                int const frames,
                                bool const randomHeader=false);


void sim_vehicle_message_iso14230(obdref::ParameterFrame &param,
                                  int const frames,
                                  bool const randomizeHeader=false,
                                  int const iso14230_headerLength=3);

void sim_vehicle_message_iso15765(obdref::ParameterFrame &param,
                                  int const frames,
                                  bool const randomizeHeader);

// print obdref data structs
void print_message_frame(obdref::ParameterFrame const &msgFrame);
void print_message_data(obdref::MessageData const &msgData);
void print_parsed_data(QList<obdref::Data> &listData);

#endif // OBDREF_TEST_H
