#include "netplayprotocol.h"

#include <QtEndian>

namespace FightcadeNetplay {

quint32 readBig32(const QByteArray &data, int offset)
{
    if (offset < 0 || offset + 4 > data.size()) return 0;
    return qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + offset));
}

qint32 readBigSigned32(const QByteArray &data, int offset)
{
    return static_cast<qint32>(readBig32(data, offset));
}

quint16 readLittle16(const QByteArray &data, int offset)
{
    if (offset < 0 || offset + 2 > data.size()) return 0;
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(data.constData() + offset));
}

quint32 readLittle32(const QByteArray &data, int offset)
{
    if (offset < 0 || offset + 4 > data.size()) return 0;
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + offset));
}

QByteArray appendBig32(QByteArray data, quint32 value)
{
    uchar bytes[4];
    qToBigEndian(value, bytes);
    data.append(reinterpret_cast<const char *>(bytes), 4);
    return data;
}

QByteArray appendLittle16(QByteArray data, quint16 value)
{
    uchar bytes[2];
    qToLittleEndian(value, bytes);
    data.append(reinterpret_cast<const char *>(bytes), 2);
    return data;
}

QByteArray appendLittle32(QByteArray data, quint32 value)
{
    uchar bytes[4];
    qToLittleEndian(value, bytes);
    data.append(reinterpret_cast<const char *>(bytes), 4);
    return data;
}

QByteArray appendLengthString(QByteArray data, const QByteArray &text)
{
    data = appendBig32(std::move(data), static_cast<quint32>(text.size()));
    data.append(text);
    return data;
}

QByteArray CommandFrame::encoded() const
{
    QByteArray data = appendBig32({}, static_cast<quint32>(payload.size() + 8));
    data = appendBig32(std::move(data), sequence);
    data = appendBig32(std::move(data), command);
    data.append(payload);
    return data;
}

CommandFrame CommandFrame::initialNegotiation(quint32 sequence)
{
    CommandFrame frame;
    frame.sequence = sequence;
    frame.command = 0;
    frame.payload = appendBig32({}, 0);
    frame.payload = appendBig32(std::move(frame.payload), 29);
    frame.payload = appendBig32(std::move(frame.payload), 1);
    return frame;
}

void TcpFrameDecoder::append(const QByteArray &data)
{
    m_buffer.append(data);
}

std::optional<CommandFrame> TcpFrameDecoder::next(QString *error)
{
    if (m_buffer.size() < 4) return std::nullopt;
    const quint32 length = readBig32(m_buffer, 0);
    if (length < 8) {
        if (error) *error = QStringLiteral("invalid length %1").arg(length);
        m_buffer.clear();
        return std::nullopt;
    }
    const int total = static_cast<int>(length) + 4;
    if (m_buffer.size() < total) return std::nullopt;
    CommandFrame frame;
    frame.sequence = readBig32(m_buffer, 4);
    frame.command = readBig32(m_buffer, 8);
    frame.payload = m_buffer.mid(12, total - 12);
    m_buffer.remove(0, total);
    return frame;
}

void ServerRecordDecoder::append(const QByteArray &data)
{
    m_buffer.append(data);
}

std::optional<ServerRecordFrame> ServerRecordDecoder::next(QString *error)
{
    if (m_buffer.size() < 4) return std::nullopt;
    const quint32 length = readBig32(m_buffer, 0);
    if (length < 4) {
        if (error) *error = QStringLiteral("invalid length %1").arg(length);
        m_buffer.clear();
        return std::nullopt;
    }
    const int total = static_cast<int>(length) + 4;
    if (m_buffer.size() < total) return std::nullopt;
    ServerRecordFrame frame;
    frame.code = readBigSigned32(m_buffer, 4);
    frame.payload = m_buffer.mid(8, total - 8);
    m_buffer.remove(0, total);
    return frame;
}

namespace {
QString readGgpoString(const QByteArray &data, int &offset)
{
    if (offset + 4 > data.size()) return {};
    const quint32 length = readBig32(data, offset);
    offset += 4;
    if (offset + static_cast<int>(length) > data.size()) return {};
    const QString text = QString::fromUtf8(data.mid(offset, static_cast<int>(length)).constData(),
                                           static_cast<int>(length));
    offset += static_cast<int>(length);
    return text;
}
}

std::optional<ServerRecord> parseServerRecord(const ServerRecordFrame &frame, QString *error)
{
    ServerRecord record;
    record.rawPayload = frame.payload;
    if (frame.code >= 0) {
        record.kind = ServerRecordKind::Positive;
        record.positive.sequence = frame.code;
        record.positive.payload = frame.payload;
        return record;
    }
    switch (frame.code) {
    case -7: {
        int offset = 0;
        record.endpoint.host = readGgpoString(frame.payload, offset);
        if (record.endpoint.host.isEmpty() || offset + 8 > frame.payload.size()) break;
        record.endpoint.value1 = readBig32(frame.payload, offset);
        record.endpoint.value2 = readBig32(frame.payload, offset + 4);
        record.kind = ServerRecordKind::EndpointNotice;
        return record;
    }
    case -8: {
        int offset = 0;
        record.command.first = readGgpoString(frame.payload, offset);
        record.command.second = readGgpoString(frame.payload, offset);
        record.command.third = readGgpoString(frame.payload, offset);
        if (record.command.first.isEmpty() || record.command.second.isEmpty()) break;
        record.kind = ServerRecordKind::CommandMessage;
        return record;
    }
    case -10:
        if (frame.payload.size() >= 4) {
            record.kind = ServerRecordKind::Integer;
            record.integer.value = readBigSigned32(frame.payload, 0);
            return record;
        }
        break;
    case -12:
        if (frame.payload.size() >= 4) {
            record.kind = ServerRecordKind::GameBuffer;
            record.gameBuffer.value = readBig32(frame.payload, 0);
            record.gameBuffer.blob = frame.payload.mid(4);
            return record;
        }
        break;
    case -13:
        if (frame.payload.size() >= 8) {
            record.streamed.frameSize = readBig32(frame.payload, 0);
            record.streamed.frameCount = readBig32(frame.payload, 4);
            int offset = 8;
            const int frameBytes = static_cast<int>(record.streamed.frameSize);
            if (frameBytes > 0 && record.streamed.frameCount < 100000) {
                bool complete = true;
                for (quint32 i = 0; i < record.streamed.frameCount; ++i) {
                    if (offset + frameBytes > frame.payload.size()) { complete = false; break; }
                    record.streamed.frames.append(frame.payload.mid(offset, frameBytes));
                    offset += frameBytes;
                }
                if (complete) {
                    record.streamed.trailing = frame.payload.mid(offset);
                    record.kind = ServerRecordKind::StreamedFrames;
                    return record;
                }
            }
        }
        break;
    default:
        break;
    }
    if (error) *error = QStringLiteral("malformed server record %1").arg(frame.code);
    record.kind = ServerRecordKind::Unknown;
    return record;
}

QByteArray HolePunchMessage::payload() const
{
    QString text = localToken + QStringLiteral(" ") + (remoteToken.isEmpty() ? QStringLiteral("_") : remoteToken);
    if (!remoteToken.isEmpty()) text += QStringLiteral(" ok");
    return text.toUtf8();
}

std::optional<HolePunchMessage> HolePunchMessage::parse(const QByteArray &data)
{
    const QList<QByteArray> parts = data.split(' ');
    if (parts.size() != 2 && parts.size() != 3) return std::nullopt;
    if (!parts[0].startsWith("0.") || (parts[1] != "_" && !parts[1].startsWith("0."))) return std::nullopt;
    if (parts.size() == 3 && parts[2] != "ok") return std::nullopt;
    HolePunchMessage message;
    message.localToken = QString::fromLatin1(parts[0]);
    message.remoteToken = QString::fromLatin1(parts[0]);
    message.remoteKnowsLocalToken = parts.size() == 3 && parts[1].startsWith("0.");
    if (parts.size() == 3) message.acknowledgedToken = QString::fromLatin1(parts[1]);
    return message;
}

bool HolePunchMessage::isWireMessage(const QByteArray &data)
{
    return parse(data).has_value();
}

int PeerInputPacket::expectedBitPayloadBytes() const
{
    return (compressedBitCount + 7) / 8;
}

bool PeerInputPacket::valid() const
{
    return inputBits.size() == expectedBitPayloadBytes();
}

QByteArray PeerInputPacket::encoded() const
{
    QByteArray data;
    data.append(char(3));
    data = appendLittle32(std::move(data), startFrame);
    data = appendLittle32(std::move(data), static_cast<quint32>(ackFrame));
    data = appendLittle16(std::move(data), compressedBitCount);
    data.append(char(inputSizeBytes));
    data.append(inputBits);
    return data;
}

PeerPacket parsePeerPacket(const QByteArray &data)
{
    PeerPacket packet;
    packet.raw = data;
    packet.payloadLength = data.size();
    if (const auto hole = HolePunchMessage::parse(data)) {
        packet.kind = PeerPacketKind::HolePunch;
        packet.holePunch = *hole;
        return packet;
    }
    if (data.isEmpty()) return packet;
    const quint8 type = static_cast<quint8>(data.at(0));
    packet.binaryType = type;
    switch (type) {
    case 1:
        if (data.size() == 5) {
            packet.kind = PeerPacketKind::SyncRequest;
            packet.nonce = readLittle32(data, 1);
            return packet;
        }
        break;
    case 2:
        if (data.size() == 5) {
            packet.kind = PeerPacketKind::SyncReply;
            packet.nonce = readLittle32(data, 1);
            return packet;
        }
        break;
    case 3:
        if (data.size() >= 12) {
            packet.input.startFrame = readLittle32(data, 1);
            packet.input.ackFrame = static_cast<qint32>(readLittle32(data, 5));
            packet.input.compressedBitCount = readLittle16(data, 9);
            packet.input.inputSizeBytes = static_cast<quint8>(data.at(11));
            packet.input.inputBits = data.mid(12);
            if (packet.input.valid()) {
                packet.kind = PeerPacketKind::Input;
                return packet;
            }
        }
        break;
    case 4:
        if (data.size() == 6) {
            packet.kind = PeerPacketKind::QualityReport;
            packet.frameAdvantage = static_cast<qint8>(data.at(1));
            packet.nonce = readLittle32(data, 2);
            return packet;
        }
        break;
    case 5:
        if (data.size() == 5) {
            packet.kind = PeerPacketKind::QualityReply;
            packet.nonce = readLittle32(data, 1);
            return packet;
        }
        break;
    default:
        break;
    }
    packet.kind = PeerPacketKind::Binary;
    return packet;
}

PeerSummary summarizePeerPackets(const QVector<PeerPacket> &packets)
{
    PeerSummary summary;
    for (const PeerPacket &packet : packets) {
        switch (packet.kind) {
        case PeerPacketKind::HolePunch: ++summary.holePunchCount; break;
        case PeerPacketKind::SyncRequest:
        case PeerPacketKind::SyncReply:
        case PeerPacketKind::Input:
        case PeerPacketKind::QualityReport:
        case PeerPacketKind::QualityReply:
        case PeerPacketKind::Binary:
            ++summary.binaryTypeCounts[packet.binaryType];
            ++summary.payloadLengthCounts[packet.payloadLength];
            break;
        }
    }
    return summary;
}

QString ServedPlan::registrationPayload() const
{
    return QStringLiteral("%1/%2").arg(quark).arg(emulatorProxyPort);
}

QString ServedPlan::expectedOkPayload() const
{
    return QStringLiteral("ok %1").arg(quark);
}

QString ServedPlan::acknowledgePayload() const
{
    return QStringLiteral("ok");
}

QString ServedPlan::usePortsPayload() const
{
    return QStringLiteral("useports/%1").arg(quark);
}

QByteArray startupConnectPayload(const ServedPlan &plan)
{
    QByteArray data;
    data = appendLengthString(std::move(data), plan.quark.toUtf8());
    data = appendBig32(std::move(data), plan.emulatorProxyPort);
    return data;
}

QString parseOkPayload(const QByteArray &payload)
{
    const QString text = QString::fromLatin1(payload);
    return text.startsWith(QStringLiteral("ok ")) ? text.mid(3) : QString();
}

} // namespace FightcadeNetplay
