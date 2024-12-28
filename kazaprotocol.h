#ifndef KAZAPROTOCOL_H
#define KAZAPROTOCOL_H

#include <QObject>
#include <QByteArray>
#include <QVariant>

class QTcpSocket;

class KaZaProtocol : public QObject
{
    Q_OBJECT
    QTcpSocket *m_socket;
    QByteArray m_pending;

public:
    explicit KaZaProtocol(QTcpSocket *socket, QObject *parent = nullptr);
    quint16 peerPort() const;

    enum {
        FRAME_SYSTEM,
        FRAME_FILE,
        FRAME_OBJVALUE,
        FRAME_DBQUERY,
        FRAME_DBRESULT
    };

public slots:
    void sendCommand(QString cmd);
    void sendFile(const QString &fileid, const QString &filepath);
    void sendObject(quint16 id, const QVariant &value);
    void sendDbQuery(uint32_t id, const QString &query);
    void sendDbQueryResult(uint32_t id, const QStringList &culumns, const QList<QList<QVariant>> &result);

private slots:
    void _dataReady();
    int _readFrame(uint8_t &id, QByteArray &source);
    void _sendFrame(uint8_t id, const QByteArray &source);

signals:
    void disconnectFromHost();
    void frameCommand(QString cmd);
    void frameFile(const QString &fileid, QByteArray data);
    void frameOject(quint16 id, QVariant value);
    void frameDbQuery(uint32_t id, QString query);
    void frameDbQueryResult(uint32_t id, const QStringList &culumns, const QList<QList<QVariant>> &result);
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