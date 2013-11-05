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

#include "obdrefdebug.h"
#include <QDebug>

namespace obdref
{

// ================================================================ //
// ================================================================ //

DebugBroadcast * g_debug_broadcast = new DebugBroadcast;
static QMutex * g_debug_mutex = new QMutex;

// ================================================================ //
// ================================================================ //

DebugBroadcast::DebugBroadcast(QObject * parent) :
    QObject(parent)
{
    g_debug_broadcast = this;
    m_timer.start();
}

DebugBroadcast::~DebugBroadcast()
{}

qint64 DebugBroadcast::msElapsed()
{   return m_timer.elapsed();   }

// ================================================================ //
// ================================================================ //

Debug::Debug()
{
    m_stream = new QTextStream(&m_byteArray,QIODevice::ReadWrite);

    // for replacing 'bad' ASCII chars
    for(int i=0; i < 256; i++)   {
        // ie 0x
        QByteArray hexcode = QByteArray::number(i,16);
        if(hexcode.length() < 2)   {
            hexcode.prepend("0");
        }
        hexcode = hexcode.toUpper();
        hexcode.prepend("<0x");
        hexcode.append(">");
        m_listHexCodes.push_back(hexcode);
    }
}

Debug::~Debug()
{
    // broadcast
    g_debug_mutex->lock();

    qint64 ms = g_debug_broadcast->msElapsed();
    qint64 numsec = ms/1000;
    qint64 numms = ms%1000;

    QString s_numsec = QString::number(numsec,10);
    while(s_numsec.size() < 3) { s_numsec.prepend("0"); }

    QString s_numms = QString::number(numms,10);
    while(s_numms.size() < 3) { s_numms.prepend("0"); }

    QString timestamp = s_numsec+"."+s_numms+":";

    m_stream->flush();
    g_debug_broadcast->debug(timestamp,m_byteArray);

    g_debug_mutex->unlock();

    // clean up
    delete m_stream;
}

// ================================================================ //
// ================================================================ //

template<typename T>
DebugBlackHole & DebugBlackHole::operator << (T const &spaceShip)
{
    return *this;
}

// ================================================================ //
// ================================================================ //

}
