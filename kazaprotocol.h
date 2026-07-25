#ifndef KAZAPROTOCOL_H
#define KAZAPROTOCOL_H

#include <QObject>
#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QVariant>
#include <utility>

class QTcpSocket;

class KaZaProtocol : public QObject
{
    Q_OBJECT
    QTcpSocket *m_socket;
    QByteArray m_pending;
    quint8 m_peerProtocolMajor;
    quint8 m_peerProtocolMinor;

public:
    explicit KaZaProtocol(QTcpSocket *socket, QObject *parent = nullptr);
    quint16 peerPort() const;

    // Protocol version
    static constexpr quint8 PROTOCOL_VERSION_MAJOR = 1;
    static constexpr quint8 PROTOCOL_VERSION_MINOR = 0;

    enum {
        FRAME_SYSTEM,
        FRAME_FILE,
        FRAME_OBJVALUE,
        FRAME_DBQUERY,
        FRAME_DBRESULT,
        FRAME_SOCKET_CONNECT,
        FRAME_SOCKET_DATA,
        FRAME_SOCKET_STATE,
        FRAME_OBJLIST,        // Compressed objects list
        FRAME_HISTORY,        // Generic min/avg/max history request
        FRAME_HISTORYRESULT,  // Generic min/avg/max history result
        FRAME_VERSION = 255   // Version negotiation frame
    };

    // Aggregation step for FRAME_HISTORY requests
    enum HistoryStep : quint8 {
        HistoryStepHour,
        HistoryStepDay,
        HistoryStepWeek,
        HistoryStepMonth,
        HistoryStepYear
    };

public slots:
    // Version negotiation
    void sendVersion(QString username, QString devicename, int channel);

    // Regular protocol frames
    void sendCommand(QString cmd);
    void sendFile(const QString &fileid, const QString &filepath);
    void sendObject(quint16 id, const QVariant &value, bool confirm);
    void sendDbQuery(uint32_t id, const QString &query);
    void sendDbQueryResult(uint32_t id, const QStringList &columns, const QList<QList<QVariant>> &result);
    void sendHistoryRequest(uint32_t id, const QString &domain, const QString &object,
                             const QDateTime &start, const QDateTime &end, quint8 step);
    void sendHistoryResult(uint32_t id, bool ok, const QString &error,
                            const QList<QDateTime> &ts, const QList<double> &min,
                            const QList<double> &avg, const QList<double> &max);
    void sendSocketConnect(uint16_t id, const QString hostname, uint16_t port);
    void sendSocketData(uint16_t id, QByteArray data);
    void sendSocketState(uint16_t id, uint16_t state);
    void sendFrameObjectsList(const QHash<QString, std::pair<QVariant, QString>> &objects);

private slots:
    void _dataReady();
    int _readFrame(uint8_t &id, QByteArray &source);
    void _sendFrame(uint8_t id, const QByteArray &source);

signals:
    void disconnectFromHost();

    // Version negotiation signals
    void versionReceived(quint8 major, quint8 minor, QString &username, QString &devicename, int service);
    void versionNegotiated(QString &username, QString &devicename, int service);
    void versionIncompatible(QString reason);

    // Regular protocol signals
    void frameCommand(QString cmd);
    void frameFile(const QString &fileid, QByteArray data);
    void frameOject(quint16 id, QVariant value, bool confirm);
    void frameDbQuery(uint32_t id, QString query);
    void frameDbQueryResult(uint32_t id, const QStringList &columns, const QList<QList<QVariant>> &result);
    void frameHistory(uint32_t id, QString domain, QString object, QDateTime start, QDateTime end, quint8 step);
    void frameHistoryResult(uint32_t id, bool ok, QString error, QList<QDateTime> ts,
                             QList<double> min, QList<double> avg, QList<double> max);
    void frameSocketConnect(uint16_t id, const QString hostname, uint16_t port);
    void frameSocketData(uint16_t id, QByteArray data);
    void frameSocketState(uint16_t id, uint16_t state);
    void frameObjectsList(const QHash<QString, std::pair<QVariant, QString>> &objects);
};


static inline QString frameToStr(const QByteArray &data)
{
    QString ret = "[";
    for(unsigned char c: data)
    {
        ret += QStringLiteral("%1").arg(c, 2, 16, QLatin1Char('0')) + ":";
    }
    ret.chop(1);
    ret += "]";

    return ret;
}

#endif // KAZAPROTOCOL_H
