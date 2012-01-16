#include "parser.h"

using namespace std;

int main()
{
    bool parsedOk;
    QString filePath = "/home/preet/Dev/obdref/obd2.xml";
    obdref::Parser myParser(filePath,parsedOk);

    obdref::Message someMsg;
    myParser.BuildRequest("SAEJ1979","ISO 15765-4 (11-bit id)",
                          "Default","Engine RPM",someMsg);

    return 0;
}

