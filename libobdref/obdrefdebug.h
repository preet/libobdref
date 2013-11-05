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

#ifndef OBDREF_DEBUG_H
#define OBDREF_DEBUG_H

#include <QObject>
#include <QMutex>
#include <QIODevice>
#include <QTextStream>
#include <QElapsedTimer>
#include <QList>

namespace obdref
{

class DebugBroadcast : public QObject
{
    Q_OBJECT

public:
    explicit DebugBroadcast(QObject * parent=0);
    ~DebugBroadcast();

    qint64 msElapsed();

signals:
    void debug(QString timestamp,QString msg);

private:
    QElapsedTimer m_timer;
};

class Debug
{
public:
    Debug();
    ~Debug();

    QTextStream * m_stream;

private:
    QByteArray m_byteArray;
    QList<QByteArray> m_listHexCodes;
};

class DebugBlackHole
{
public:
    template<typename T>
    DebugBlackHole& operator << (T const & spaceShip);
};

extern DebugBroadcast * g_debug_broadcast;

}

#ifdef OBDREF_DEBUG_BROADCAST
#define OBDREFDEBUG    (*(obdref::Debug().m_stream))
#endif

#ifdef OBDREF_DEBUG_QDEBUG
#define OBDREFDEBUG    qDebug() << "OBDREF:"
#endif

#ifdef OBDREF_DEBUG_NONE
#define OBDREFDEBUG    obdref::DebugBlackHole()
#endif

#endif // OBDREF_DEBUG_H
