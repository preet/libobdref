### libobdref Definitions File
libobdref uses a definitions file that is a mix of XML and JavaScript to build and parse vehicle messages. This allows a great deal of flexibility since vehicle parameters can be added and modified on the fly without recomplication.
***
#### Introduction
Modern cars have highly complex networks that relay hundreds of messages between different components in the vehicle. A base set of messages that all cars must have available (at least in the US) is specified by the SAE. 

Vehicle manufacturers almost always have their own **specifications** for additional messages as well. There are also standard **protocols** used to communicate this information which vary between car make and model. Each vehicle has several nodes in its communications network that talk to each other. These nodes are either a single device, or a grouping of them. A single node is usually identified by its **address**. 

Single messages usually (but not always) indicate where they are from (through a **source address**), and where they are going (through a **target address**). The message may also contain **priority** information, indicating its relative importance. Some messages are **request** messages, in that they're only there to ask an address for some specific information. The resulting messages are **response** messages. There are also 'passive' messages which are broadcast throughout the vehicle network, and can be monitored. 

Messages convey information through their data bytes. Some of the data is **numeric**, such as Engine RPM or Coolant Temperature. Other information is **literal**, for example the status of the Fuel System (Open Loop, Closed Loop etc) or a DTC. The information conveyed in data bytes must be **parsed** to convert it from a series of bytes to a meaningful value.

The aforementioned information was translated into an XML document definition to make it portable and easy to modify:
***
#### XML Structure
**Specifications**  
The top level tag is the **spec** tag. It has _name_ and _desc_ attributes, with the _name_ attribute used to uniquely identify each specification. The most common specification is the SAE's OBDII standard, which all vehicles in North America made after 1996 comply to.

    <spec name="specName" desc="Specification Description">

**Protocols**  
Each specification contains one or more **protocol** tags, to indicate compatible protocols. Protocol tags are identified by their _name_ attribute.

        <protocol name="protoName" desc="Protocol Description">

**Addresses**  
Each protocol further has addresses used to talk to specific devices in the car. In the OBDII specification, each protocol has a 'default' address. Other addresses are vehicle specific. All of these addresses are defined with the **address** tag and its _name_ attribute.

            <address name="someAddress">

**Request and Response**  
Every address has a mandatory **request** tag and an optional corresponding **response** tag. The request and response tags specify the header information required in a typical OBD message. Including a response tag will only interpret response messages with the specified header data. The specific attributes required for each tag vary by protocol. See the definitions file for examples.

                <request prio="0x00" target="0x01" source="0x02" />

                <response prio="0x00" target="0x02" source="0x01" />

That completes the minimum amount of information required to create a protocol. Each specification requires at least one valid protocol.

            </address>
        </protocol>
**Parameters**  
After specifying at least one protocol, we need to specify some parameters with the **parameters** tag. Parameters are grouped by address, and the address is specified with the _address_ attribute.

        <parameters address="someAddress">

Each parameters grouping contains several **parameter** tags. The parameter tags have _name_, _response_(optional), _response.prefix_(optional) and _response.bytes_(optional) attributes:  
- _request_: Data we need to send to request our specific message  
- _response.prefix_: Used to identify the response (ie the response message data should start with 0xEB 0xCD)  
- _response.bytes_: Expected data bytes (after the prefix) in the response
    
            <parameter name="numericalParam" request="0xAB 0xCD" response.prefix="0xEB 0xCD" response.bytes="2">

In special cases, a single parameter may require multiple requests or receive multiple responses from a vehicle that needs to be combined for a single set of data. To account for this, multiple requests can be specified from within the same parameter:

            <parameter name="multiRequest"
                request0="0x22 0x04" response0.prefix="0x62 0x04" response0.bytes="2"
                request1="0x22 0x05" response1.prefix="0x62 0x05" response1.bytes="2"
                request2="0x22 0x06" response2.prefix="0x62 0x06" response2.bytes="2"
                parse="combined">
                
The special 'parse' attribute tells libobdref how you want to parse a parameter that has multiple sets of data. With the default parse mode (when no parse attribute is specified), libdobdref parses each response individually. In the above case, that would mean the corresponding parse function for this parameter (parse functions are described below) would be called **3 times**. If the parse attribute has a value of "combined" however, all of the responses would be made available together and interpreted **once**. 

**Scripts**  
When a parameter message response is received, obdref runs the JavaScript contained in the parameter's **script** tags. Note that the script is further enclosed by CDATA identifiers so the XML parser doesn't try to parse the actual script as well.

                  <script [protocols="..."]>
                     <![CDATA[
                     /* Javascript goes here! */
                     ]]>
                  </script>
                  
The script tag has an optional 'protocols' attribute to specify different scripts for different protocols in the same parameter. In rare cases, the same parameter returns data that needs to be parsed in a different way based on the protocol (an example is retrieving diagnostic trouble codes for SAEJ1979 -- the contents of the vehicle response will differ slightly when the protocol is ISO 15765)

Finally, closing up all open tags marks the end of the specification:

              </parameter>                              
          </parameters>
      </spec>
***
#### Parsing with JavaScript  
**Numerical and Literal Data**  
Javascript is used to parse the data received from the response. The data must be interpreted into one or more **literal** or **numerical** data objects. Each object type has properties that can be filled out. The data objects can be constructed as follows:

    var litData = new LiteralDataObj();
    litData.value = true;                      // boolean, true or false
    litData.valueIfFalse = ""                  // string describing value if false
    litData.valueIfTrue = ""                   // string describing value if true
    litData.property = ""                      // string describing 'sub-property' of parameter

    var numData = new NumericalDataObj();
    numData.value = 0;                         // number value
    numData.min = 0;                           // number minimum value expected
    numData.max = 0;                           // number maximum value expected
    numData.units = "";                        // string describing units of value
    numData.property = "";                     // string describing 'sub-property' of parameter

To save a literal or numerical data object, the **saveNumericalData** and **saveLiteralData** functions are used. To save the above example objects, you would call:

    saveLiteralData(litData);
    saveNumericalData(numData);

**Accessing data with Javascript**  
By default, every individual response received for a parameter is parsed separately using the parse function. This means one set of data bytes is made available, which can be accessed with the following helper functions:

    BYTE(bytePos)                              // get byte (0-N)
    BIT(bytePos,bitPos)                        // get bit (0-7) of byte (0-N)
    LENGTH()                                   // get the data length

Here are a couple of examples for constructing and saving numerical and literal data:

    var engSpd = new NumericalDataObj();
    engSpd.units = "rpm";
    engSpd.min = 0;
    engSpd.max = 16384;
    engSpd.value = (BYTE(0)*256 + BYTE(1))/4;
    saveNumericalData(engSpd);

    var milStatus = new LiteralDataObj();
    milStatus.value = BIT(0,5);
    milStatus.property = "Malfunction Indicator Light Status";
    milStatus.valueIfTrue = "ON";
    milStatus.valueIfFalse = "OFF";
    saveLiteralData(milStatus);

When the parameter XML tag has the parse="combined" attribute, all responses are made available to the parse function together. They are accessed as follows:

    REQ(x).HEADER(y)  // get set 'y' of header bytes for request 'x'
    REQ(x).DATA(y)    // get the corresponding set (y) of data bytes for request 'x'
    NUM_REQ()         // get the number of requests for this param
    NUM_RESP(r)       // get the number of responses for request 'r'
    
    // The BYTE(), BIT() and LENGTH() functions operate on 
    // HEADER() and DATA() objects exactly as described above
    
Here is an example that saves all header and data bytes for a multi part request parameter with combined parsing as literal data.

    for(var i=0; i < NUM_REQ(); i++)   {
        for(var j=0; j < NUM_RESP(i); j++)   {
            var headerBytes="";
            for(var k=0; k < REQ(i).HEADER(j).LENGTH(); k++)   {
               headerBytes += REQ(i).HEADER(j).BYTE(k).toString(16);
               headerBytes += " ";
            }
         
            var dataBytes="";
            for(var k=0; k < REQ(i).DATA(j).LENGTH(); k++)   {
               dataBytes += REQ(i).DATA(j).BYTE(k).toString(16);
               dataBytes += " ";
            }
         
            // save
            var i_s = i.toString(10);
            var j_s = j.toString(10);
         
            var jsData = new LiteralDataObj();
            jsData.property = "req:"+i_s+" resp:"+j_s;
            jsData.valueIfTrue = headerBytes.toUpperCase() + "  " + dataBytes.toUpperCase();
            jsData.value = true;
            saveLiteralData(jsData);
        }
    }
***
For more information, see the 'globals.js' file in the source -- all of the JavaScript functions and objects used in libobdref are defined there.  

For more examples, see the included definitions files.
