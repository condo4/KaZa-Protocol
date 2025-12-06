#include "kazaprotocol.h"
#include <QFile>
#include <QTcpSocket>
#include <QDataStream>

// #define DEBUG_FRAME

KaZaProtocol::KaZaProtocol(QTcpSocket *socket, QObject *parent)
    : QObject{parent}
    , m_socket(socket)
    , m_peerProtocolMajor(0)
    , m_peerProtocolMinor(0)
{
    QObject::connect(m_socket, &QTcpSocket::readyRead, this, &KaZaProtocol::_dataReady);
    QObject::connect(m_socket, &QTcpSocket::disconnected, this, &KaZaProtocol::disconnectFromHost);
}

quint16 KaZaProtocol::peerPort() const
{
    return m_socket->peerPort();
}

void KaZaProtocol::_dataReady()
{
    m_pending.append(m_socket->readAll());

    bool dataAvailable = true;
    while(dataAvailable)
    {
        uint8_t packid;
        QByteArray data;
        int len = _readFrame(packid, data);
        if(len < 1)
            break;

        switch(packid)
        {
        case FRAME_VERSION: {
            QDataStream frameStream(&data, QIODevice::ReadOnly);
            frameStream.setByteOrder(QDataStream::BigEndian);

            uint8_t peerMajor;
            uint8_t peerMinor;
            QString username;
            QString devicename;
            int channel;
            frameStream >> peerMajor;
            frameStream >> peerMinor;
            frameStream >> username;
            frameStream >> devicename;
            frameStream >> channel;

            if(peerMajor == PROTOCOL_VERSION_MAJOR && peerMinor <= PROTOCOL_VERSION_MINOR)
            {
                // Protocol supported
                sendCommand("CONNECTED");
                emit versionNegotiated(username, devicename, channel);
            }
            else
            {
                // Protocol not supported
                QString reason = "Protocol version incompatible. Client: " +
                                 QString::number(peerMajor) + "." + QString::number(peerMinor) +
                                 " Server " + QString::number(PROTOCOL_VERSION_MAJOR) + "." + QString::number(PROTOCOL_VERSION_MINOR);
                sendCommand("ERROR: " + reason);
                emit versionIncompatible(reason);
            }
            break;
        }

        case FRAME_SYSTEM:
#ifdef DEBUG_FRAME
            qDebug() << "RX CMD " << QString::fromUtf8(data);
#endif
            emit frameCommand(QString::fromUtf8(data));
            break;
        case FRAME_OBJVALUE: {
            QDataStream frameStream(&data, QIODevice::ReadOnly);
            frameStream.setByteOrder(QDataStream::BigEndian);
            quint16 id;
            frameStream >> id;

            QByteArray dataRaw = data.mid(2);
            QDataStream dataStream(&dataRaw, QIODevice::ReadOnly);
            dataStream.setVersion(QDataStream::Qt_6_0);
            QVariant value;
            bool confirm;
            dataStream >> value;
            dataStream >> confirm;
#ifdef DEBUG_FRAME
            qDebug().noquote() << "frameOject(" << id << ", " << value << ")";
#endif
            emit frameOject(id, value, confirm);
            break;
        }
        case FRAME_FILE:
            emit frameFile(data.mid(0, 3), data.mid(4));
            break;
        case FRAME_DBQUERY: {
            QDataStream dataStream(&data, QIODevice::ReadOnly);
            uint32_t id;
            QString query;
            dataStream >> id;
            dataStream >> query;
            emit frameDbQuery(id, query);
            break;
        }
        case FRAME_DBRESULT: {
            QDataStream dataStream(&data, QIODevice::ReadOnly);
            uint32_t id;
            QStringList columns;
            QList<QList<QVariant>> result;
            dataStream >> id;
            dataStream >> columns;
            dataStream >> result;
            emit frameDbQueryResult(id, columns, result);
            break;
        }
        case FRAME_SOCKET_CONNECT: {
            QDataStream dataStream(&data, QIODevice::ReadOnly);
            uint16_t id;
            QString hostname;
            uint16_t port;
            dataStream >> id;
            dataStream >> hostname;
            dataStream >> port;
            emit frameSocketConnect(id, hostname, port);
            break;
        }
        case FRAME_SOCKET_DATA: {
            QDataStream dataStream(&data, QIODevice::ReadOnly);
            uint16_t id;
            QByteArray datas;
            dataStream >> id;
            dataStream >> datas;
            emit frameSocketData(id, datas);
            break;
        }
        case FRAME_SOCKET_STATE: {
            QDataStream dataStream(&data, QIODevice::ReadOnly);
            uint16_t id;
            uint16_t state;
            dataStream >> id;
            dataStream >> state;
            emit frameSocketState(id, state);
            break;
        }
        case FRAME_OBJLIST: {
            // Decompress the data
            QByteArray uncompressed = qUncompress(data);
            QDataStream dataStream(&uncompressed, QIODevice::ReadOnly);
            dataStream.setVersion(QDataStream::Qt_6_0);

            QMap<QString, QPair<QVariant, QString>> objects;
            dataStream >> objects;

            emit frameObjectsList(objects);
            break;
        }
        }
        m_pending.remove(0, len+5);
    }
}


int KaZaProtocol::_readFrame(uint8_t &id, QByteArray &source) {
    if(m_pending.length() < 5)
        return -1;
    id = m_pending[0];
    uint32_t len =
        ((m_pending[1] << 24) & 0xFF000000) |
        ((m_pending[2] << 16) & 0x00FF0000) |
        ((m_pending[3] <<  8) & 0x0000FF00) |
        ((m_pending[4] <<  0) & 0x000000FF) ;
    if(m_pending.length() < (5 + len))
    {
#ifdef DEBUG_FRAME
        qDebug() << "PENDING WAIT:" << m_pending.length() << " / " << (5 + len);
        qDebug() << "CURRENT: " << frameToStr(m_pending.mid(0, 10));
#endif
        return -1;
    }
    source = m_pending.mid(5, len);
    return len;
}

void KaZaProtocol::_sendFrame(uint8_t id, const QByteArray &source) {
    QByteArray frame;
    QDataStream stream(&frame, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << id;
    stream << (quint32)source.length();
    frame.append(source);

    m_socket->write(frame);
}

void KaZaProtocol::sendVersion(QString username, QString devicename, int channel)
{
    QByteArray frame;
    QDataStream frameStream(&frame, QIODevice::WriteOnly);
    frameStream.setByteOrder(QDataStream::BigEndian);
    uint8_t major = PROTOCOL_VERSION_MAJOR;
    uint8_t minor = PROTOCOL_VERSION_MINOR;
    frameStream << major;
    frameStream << minor;
    frameStream << username;
    frameStream << devicename;
    frameStream << channel;

#ifdef DEBUG_FRAME
    qDebug() << "Sending VERSION request:" << major << "." << minor << " " << username << " " << devicename << " " << channel;
#endif

    _sendFrame(FRAME_VERSION, frame);
}

void KaZaProtocol::sendCommand(QString cmd)
{
    _sendFrame(FRAME_SYSTEM, cmd.toUtf8());
}

void KaZaProtocol::sendFile(const QString &fileid, const QString &filepath)
{
    QFile f(filepath);
    if(fileid.length() != 3)
    {
        qWarning() << "fileid should be 3 characters for SendFile";
        return;
    }
    if(f.open(QFile::ReadOnly))
    {
        QByteArray frame;
        frame.append(fileid.toUtf8());
        frame.append(":");
        frame.append(f.readAll());
        _sendFrame(FRAME_FILE, frame);
    }
}

void KaZaProtocol::sendObject(quint16 id, const QVariant &value, bool confirm)
{
    QByteArray frame;
    QDataStream frameStream(&frame, QIODevice::WriteOnly);
    frameStream.setByteOrder(QDataStream::BigEndian);
    frameStream << id;

    QByteArray data;
    QDataStream dataStream(&data, QIODevice::WriteOnly);
    dataStream.setVersion(QDataStream::Qt_6_0);
    dataStream << value;
    dataStream << confirm;

    frame.append(data);
    _sendFrame(FRAME_OBJVALUE, frame);
}

void KaZaProtocol::sendDbQuery(uint32_t id, const QString &query)
{
#ifdef DEBUG_FRAME
    qDebug() << "QUERY: " << id << " = " << query;
#endif

    QByteArray data;
    QDataStream dataStream(&data, QIODevice::WriteOnly);
    dataStream << id;
    dataStream << query;
    _sendFrame(FRAME_DBQUERY, data);
}

void KaZaProtocol::sendDbQueryResult(uint32_t id, const QStringList &columns, const QList<QList<QVariant>> &result)
{
    QByteArray dataret;
    QDataStream stream(&dataret, QIODevice::ReadWrite);
    stream << id;
    stream << columns;
    stream << result;
    _sendFrame(FRAME_DBRESULT, dataret);
}

void KaZaProtocol::sendSocketConnect(uint16_t id, const QString hostname, uint16_t port)
{
    QByteArray dataret;
    QDataStream stream(&dataret, QIODevice::ReadWrite);
    stream << id;
    stream << hostname;
    stream << port;
    _sendFrame(FRAME_SOCKET_CONNECT, dataret);
}

void KaZaProtocol::sendSocketData(uint16_t id, QByteArray data)
{
    QByteArray dataret;
    QDataStream stream(&dataret, QIODevice::ReadWrite);
    stream << id;
    stream << data;
    _sendFrame(FRAME_SOCKET_DATA, dataret);
}

void KaZaProtocol::sendSocketState(uint16_t id, uint16_t state)
{
    QByteArray dataret;
    QDataStream stream(&dataret, QIODevice::ReadWrite);
    stream << id;
    stream << state;
    _sendFrame(FRAME_SOCKET_STATE, dataret);
}

void KaZaProtocol::sendFrameObjectsList(const QMap<QString, QPair<QVariant, QString>> &objects)
{
    // Serialize objects to QByteArray
    QByteArray uncompressed;
    QDataStream stream(&uncompressed, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << objects;

    // Compress the serialized data
    QByteArray compressed = qCompress(uncompressed);

    // Send the compressed frame
    _sendFrame(FRAME_OBJLIST, compressed);
}
