#include "parser.h"

using namespace std;

int main()
{
    bool parsedOk;
    QString filePath = "/home/preet/Dev/obdref/obd2.xml";
    obdref::Parser myParser(filePath,parsedOk);

    obdref::Message someMsg;
    myParser.BuildMessage("SAEJ1979","ISO 14230-4",
                          "Default","Vehicle Speed",someMsg);

    return 0;
}

