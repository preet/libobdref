### Using libobdref 
Note: a simple example of how to use libobdref can be found in _examples/quickstart.cpp_
***
#### OBD Message Format  
Before you can integrate obdref in your application, you need to understand the OBD data obdref expects to work with. libobdref only understands a given OBD message as being made up of **header byte** and **data bytes**

The number of header bytes and data bytes in each message is protocol dependent. Closer to the hardware level, an OBD message may contain much more information, such as the checksum, but obdref expects this additional data to be inserted/removed as necessary by your application. 

This means that when you get a parameter from obdref to send a request through a hardware device, you only get header bytes and data bytes. All other data required in the message must be input by your application. The same thing applies in reverse; when a message is received through a hardware device and obdref is asked to parse it, you should only pass on header bytes and data bytes.

For example, requesting Engine RPM on the ISO9141-2 protocol might require a message like the following:

    0x68 0x6A 0xF1 0x01 0x0C 0x00 0x00 0x00 0x00 0x00 0xD0

But when generating a request with libobdref, you'll only get

    0x68 0x6A 0xF1 0x01 0x0C
    
The padding filler bytes and checksum need to be added manually. Conversely, a response for the above request from the vehicle might look like:

    0x48 0x6B 0x10 0x41 0x0c 0x11 0xAA 0x00 0x00 0x00 0xCB

The checksum (0xCB) should be removed before passing this frame to libobdref.

There's a slight difference with ISO 15765 messages, where libobdref will generate PCI bytes for you if you ask, and where you shoud leave PCI bytes intact so that long messages can be pieced together (though technically PCI bytes are part of the data bytes in an ISO 15765 frame).

***

#### The Parser
Everything related to obdref lives in the obdref namespace. Almost everything is done through one main object in obdref -- the Parser. Creating an instance of the Parser requires passing a file path to an valid definitions file:

    // definitions file path
    QString filePath = "/home/canurabus/definitionsFile.xml";
    
    // create Parser instance
    bool opOk = false;
    obdref::Parser parser(filePath,opOk);
    
    // check if everything went ok
    if(!ok) { qDebug() << "something went wrong!"; return -1; }
    
***

#### Building a Parameter
libobdref uses an object called a ParameterFrame to store *everything* related to a given parameter -- how to look it up from a definitions file and all the request and response data. To build a Parameter Frame, you need to first create an instance and then fill in data for the Specification, Protocol, Address and Parameter Name you want to request or parse.

    obdref::ParameterFrame pf;
    pf.spec = "SAEJ1979";
    pf.protocol = "ISO 15765 Standard Id";
    pf.address = "Default";
    pf.name = "Vehicle Speed";

Once this is done, call the **BuildParameterFrame()** method on the Parser to fill in some important data from the definitions file. The method returns a boolean indicating success.

    ok = parser.BuildParameterFrame(pf);
    if(!ok) { qDebug() << "couldn't build parameter!"; return -1; }
    
***

#### Sending and receiving data
Once the ParameterFrame has been built, it will have the header and data bytes your application should use to construct any messages it needs to send to the vehicle to request the parameter. It's also possible that the parameter doesn't need to be explicitly requested (maybe the data is by monitoring the bus, etc) -- in this case the request header and data bytes won't be generated and can be ignored.

Most of the useful data in a ParameterFrame object is stored in a list of **MessageData** structs. Each MessageData instance represents at most a single request for the parameter (remember that a single parameter can have 0 or more requests).

Here are examples on how to access request and response data for a parameter:

    // for a single request and single framed response, or for
    // a parameter without a request there's only one MessageData:
    ParameterFrame.listMessageData[0].reqHeaderBytes;   // request header
    ParameterFrame.listMessageData[0].listReqDataBytes; // request data
    ParameterFrame.listMessageData[0].listRawFrames;    // response data
    
    // for request N when multiple requests are present
    ParameterFrame.listMessageData[N].reqHeaderBytes;   // request header
    ParameterFrame.listMessageData[N].listReqDataBytes; // request data
    ParameterFrame.listMessageData[N].listRawFrames;    // response data
    
Note that for a single MessageData, there's a list of request data (listReqDataBytes) -- this is when a request needs to be split across multiple frames.

Once you've built and sent the request, the vehicle will hopefully send a valid response. This response should be cleaned as describe above (in "OBD Message Format") and then saved in the corresponding MessageData:

    obdref::ByteList frame;
    frame << 0x07 << 0xE8 << 0x03 << 0x41 << 0x05 << 0xB3 << 0x00 << 0x00 << 0x00 << 0x00;
    parameterFrame.listMessageData[0].listRawFrames << frame;
    
Response data needs to be saved in the same MessageData that generated its request if there are multiple requests for the parameter:

    // -- message 1 --
    MessageData &msg1 = parameterFrame.listMessageData[0];
    obdref::ByteList request1,response1;
    
    // build and send request 1
    request1 << msg1.reqHeaderBytes << msg1.listReqDataBytes[0];
    vehicle_interface_write(request1);
    
    // receive and save response 1
    response1 = vehicle_interface_read();
    msg1.listRawFrames << response1;
    
    
    // -- message 2 --
    MessageData &msg2 = parameterFrame.listMessageData[1];
    obdref::ByteList request2;
    
    // build and send request 2
    request2 << msg2.reqHeaderBytes << msg2.listReqDataBytes[2];
    vehicle_interface_write(request2);
    
    // receive and save response 2
    response2 = vehicle_interface_read();
    msg2.listRawFrames << response2;

***
#### Parse Received Data
Now libobdref can be used to interpret the received messages into two types of data: Numerical Data and Literal Data. The former provides quantities like Engine RPM, Coolant Temperature, Fuel Pressure etc, whereas the latter provides status indicators, DTCs and so on.

A given message can have multiple sets of numerical and literal data. The obdref::Data object stores a list of numerical and literal data for a single address. Since multiple addresses can reply to a single request, we use a list of obdref::Data to store data when parsing a message:

    QList<obdref::Data> listData;
    ok = ParseParameterFrame(parameterFrame,listData);

If (ok == true), you should now have a set of numerical and literal data from the parameter to use in your application.
