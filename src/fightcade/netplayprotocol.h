#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QVector>

#include <optional>

namespace FightcadeNetplay {

quint32 readBig32(const QByteArray &data, int offset);
qint32 readBigSigned32(const QByteArray &data, int offset);
quint16 readLittle16(const QByteArray &data, int offset);
quint32 readLittle32(const QByteArray &data, int offset);
QByteArray appendBig32(QByteArray data, quint32 value);
QByteArray appendLittle16(QByteArray data, quint16 value);
QByteArray appendLittle32(QByteArray data, quint32 value);
QByteArray appendLengthString(QByteArray data, const QByteArray &text);

struct CommandFrame {
    quint32 sequence = 0;
    quint32 command = 0;
    QByteArray payload;
    QByteArray encoded() const;
    static CommandFrame initialNegotiation(quint32 sequence = 1);
};

class TcpFrameDecoder
{
public:
    void append(const QByteArray &data);
    // Returns nullopt on incomplete data. On malformed input, error is set and
    // the decoder is reset.
    std::optional<CommandFrame> next(QString *error = nullptr);
private:
    QByteArray m_buffer;
};

struct ServerRecordFrame {
    qint32 code = 0;
    QByteArray payload;
};

class ServerRecordDecoder
{
public:
    void append(const QByteArray &data);
    std::optional<ServerRecordFrame> next(QString *error = nullptr);
private:
    QByteArray m_buffer;
};

struct EndpointNotice {
    QString host;
    quint32 value1 = 0;
    quint32 value2 = 0;
};

struct CommandMessage {
    QString first;
    QString second;
    QString third;
};

struct IntegerSignal { qint32 value = 0; };
struct GameBuffer { quint32 value = 0; QByteArray blob; };
struct StreamedFrames { quint32 frameSize = 0; quint32 frameCount = 0;
                        QVector<QByteArray> frames; QByteArray trailing; };
struct PositiveResponse { qint32 sequence = 0; QByteArray payload; };

enum class ServerRecordKind { Positive, EndpointNotice, CommandMessage, Integer,
                              GameBuffer, StreamedFrames, Unknown };

struct ServerRecord {
    ServerRecordKind kind = ServerRecordKind::Unknown;
    PositiveResponse positive;
    EndpointNotice endpoint;
    CommandMessage command;
    IntegerSignal integer;
    GameBuffer gameBuffer;
    StreamedFrames streamed;
    QByteArray rawPayload;
};

std::optional<ServerRecord> parseServerRecord(const ServerRecordFrame &frame, QString *error = nullptr);

struct HolePunchMessage {
    QString localToken;
    QString remoteToken;
    bool remoteKnowsLocalToken = false;
    QString acknowledgedToken;
    QByteArray payload() const;
    static std::optional<HolePunchMessage> parse(const QByteArray &data);
    static bool isWireMessage(const QByteArray &data);
};

struct PeerInputPacket {
    quint32 startFrame = 0;
    qint32 ackFrame = 0;
    quint16 compressedBitCount = 0;
    quint8 inputSizeBytes = 0;
    QByteArray inputBits;
    int expectedBitPayloadBytes() const;
    bool valid() const;
    QByteArray encoded() const;
};

enum class PeerPacketKind { HolePunch, SyncRequest, SyncReply, Input, QualityReport,
                            QualityReply, Binary };
struct PeerPacket {
    PeerPacketKind kind = PeerPacketKind::Binary;
    HolePunchMessage holePunch;
    quint32 nonce = 0;
    PeerInputPacket input;
    qint8 frameAdvantage = 0;
    quint8 binaryType = 0;
    int payloadLength = 0;
    QByteArray raw;
};

PeerPacket parsePeerPacket(const QByteArray &data);

struct PeerSummary {
    QMap<quint8, int> binaryTypeCounts;
    QMap<int, int> payloadLengthCounts;
    int holePunchCount = 0;
};

PeerSummary summarizePeerPackets(const QVector<PeerPacket> &packets);

struct ServedPlan {
    QString quark;
    QString masterHost = QStringLiteral("ggpo.fightcade.com");
    quint16 masterPort = 0;
    quint16 localBindPort = 6006;
    quint16 emulatorProxyPort = 7001;
    quint16 fixedFallbackPort = 6004;
    QString registrationPayload() const;
    QString expectedOkPayload() const;
    QString acknowledgePayload() const;
    QString usePortsPayload() const;
};

QByteArray startupConnectPayload(const ServedPlan &plan);
QString parseOkPayload(const QByteArray &payload);

} // namespace FightcadeNetplay
