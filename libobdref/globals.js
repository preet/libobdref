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

// ================================================================ //
// WARNING: Don't edit this file unless you know what're doing! 
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

function DataBytesObj()
{
    this.bytes = [];

    this.BYTE = function(bytePos)
         { return ((bytePos > -1 && bytePos < this.bytes.length) ? this.bytes[bytePos] : 0); }

    this.BIT = function(bytePos,bitPos)
         {
             if(bytePos > -1 && bytePos < this.bytes.length && bitPos > -1 && bitPos < 8)
             {
                 var byteVal = this.bytes[bytePos];
                 if((byteVal & (1 << bitPos)) > 0)
                 {   return 1;   }
                 else
                 {   return 0;   }
             }
         }
         
    this.LENGTH = function()
         {   return this.bytes.length;   }

    this.appendByte = function(byteVal)
         {
             if(byteVal >= 0)
             {   this.bytes.push(byteVal);   }
         }

    this.clearData = function()
         {   this.bytes.length = 0;   }
}

function ListDataBytesObj()
{
   this.listDataBytes = [];
   
   this.appendDataBytes = function(dataBytes)
      {
         var dataBytesObj = new DataBytesObj();
         dataBytesObj.bytes = dataBytes;
         this.listDataBytes.push(dataBytesObj);
      }
      
   this.clearDataBytes = function()
      {   this.listDataBytes.length = 0;   }
}

// global list of databytes for when a given parameter
// needs to be reconstructed from multiple messages
var global_list_databytes = new ListDataBytesObj();

function DATA(respNum)
{
   if(respNum > -1 && respNum < global_list_databytes.listDataBytes.length)
   {   return global_list_databytes.listDataBytes[respNum];   }
   else
   {   return 0;   }
}

function BYTE(bytePos)
{   return DATA(0).BYTE(bytePos);   }

function BIT(bytePos,bitPos)
{   return DATA(0).BIT(bytePos,bitPos);   }

function LENGTH()
{   return DATA(0).LENGTH();   }
