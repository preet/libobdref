
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

// global list of databytes for when a parameter
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

