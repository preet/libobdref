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

// ================================================================ //
// ================================================================ //

function LiteralDataObj()
{
    this.value = false;
    this.valueIfFalse = "";
    this.valueIfTrue = "";
    this.property = "";
}

function NumericalDataObj()
{
    this.value = 0;
    this.min = 0;
    this.max = 0;
    this.units = "";
    this.property = "";
}

function ListDataObj()
{
    this.listData = [];

    this.appendData = function(newData)
    {   this.listData.push(newData);   }

    this.clearData = function()
    {   this.listData.length = 0;   }
}

var global_list_num_data = new ListDataObj();

var global_list_lit_data = new ListDataObj();

function saveNumericalData(numDataObj)
{   global_list_num_data.appendData(numDataObj);   }

function saveLiteralData(litDataObj)
{   global_list_lit_data.appendData(litDataObj);   }

function __private_get_num_data()
{
   return global_list_num_data.listData;
}

function __private_get_lit_data()
{
   return global_list_lit_data.listData;
}

// ================================================================ //
// ================================================================ //

function DataBytesObj()
{
    this.bytes = [];

    this.BYTE = function(bytePos)   {
        return this.bytes[bytePos];
    }

    this.BIT = function(bytePos,bitPos)   {
        var byteVal = this.bytes[bytePos];
        if((byteVal & (1 << bitPos)) > 0)   {
            return 1;
        }
        return 0;
    }

    this.LENGTH = function()
    {   return this.bytes.length;   }
}

function MessageDataObj()
{
    this.listHeaderBytes = [];
    this.listDataBytes = [];

    this.setListHeaderBytes = function(list_headerbytes)   {
        for(var i=0; i < list_headerbytes.length; i++)   {
            var headerBytes = new DataBytesObj();
            headerBytes.bytes = list_headerbytes[i];
            this.listHeaderBytes.push(headerBytes);
        }
    }

    this.setListDataBytes = function(list_databytes)   {
        for(var i=0; i < list_databytes.length; i++)   {
            var dataBytes = new DataBytesObj();
            dataBytes.bytes = list_databytes[i];
            this.listDataBytes.push(dataBytes);
        }
    }
    
    this.HEADER = function(headerIdx)   {
       return this.listHeaderBytes[headerIdx];
    }
    
    this.DATA = function(dataIdx)   {
       return this.listDataBytes[dataIdx];
    }
}

function ParameterObj()
{
    this.listMessageData = [];

    this.appendMessageData = function(msg)   {
        this.listMessageData.push(msg);
    }

    this.clearAll = function()   {
        this.listMessageData.length = 0;
    }
}

var global_param = new ParameterObj();

function REQ(idx)   {
    return global_param.listMessageData[idx];
}

function NUM_REQ()   {
   return global_param.listMessageData.length;
}

function NUM_RESP(idx)   {
   return REQ(idx).listDataBytes.length;
}

function HEADER(idx)   {
    return REQ(0).listHeaderBytes[idx];
}

function DATA(idx)   {
    return REQ(0).listDataBytes[idx];
}

function BYTE(bytePos)   {
    return DATA(0).BYTE(bytePos);
}

function BIT(bytePos,bitPos)   {
    return DATA(0).BIT(bytePos,bitPos);
}

function LENGTH()   {
    return DATA(0).LENGTH();
}

// ================================================================ //
// ================================================================ //

// where list_databytes is a 2d array that looks like:
// list_databytes[0]: 0xAA 0xBB 0xCC ...
// list_databytes[1]: 0xDD 0xEE 0xFF ...
// list_databytes[2]: 0x00 0x11 0x22 ... etc
function __private__add_list_databytes(list_databytes)
{
    var msg = new MessageDataObj();
    msg.setListDataBytes(list_databytes);
    global_param.appendMessageData(msg);
}

function __private__add_msg_data(list_headerbytes,list_databytes)
{
    var msg = new MessageDataObj();
    msg.setListHeaderBytes(list_headerbytes);
    msg.setListDataBytes(list_databytes);
    global_param.appendMessageData(msg);
}

function __private__clear_all_data()
{
   global_list_num_data.clearData();
   global_list_lit_data.clearData();
   global_param.clearAll();
}

// ================================================================ //
// ================================================================ //
