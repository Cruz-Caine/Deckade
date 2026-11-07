#include "fightcade/netplayprotocol.h"

#include <QCoreApplication>
#include <QDebug>

using namespace FightcadeNetplay;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { qCritical() << "FAIL" << __LINE__ << #cond; ++failures; } } while (0)
#define EQ(a,b) do { const auto av=(a); const auto bv=(b); if (!(av==bv)) { qCritical() << "FAIL" << __LINE__ << #a << av << "!=" << bv; ++failures; } } while (0)

static QByteArray hex(const char *text)
{
    QByteArray out;
    for (int i = 0; text[i] && text[i + 1]; i += 2) {
        out.append(char(QByteArray(text + i, 2).toInt(nullptr, 16)));
    }
    return out;
}

static QByteArray commandPayload(const char *a, const char *b, const char *c)
{
    QByteArray d;
    d = appendLengthString(std::move(d), QByteArray(a));
    d = appendLengthString(std::move(d), QByteArray(b));
    d = appendLengthString(std::move(d), QByteArray(c));
    return d;
}

static void commandFrames()
{
    const CommandFrame frame{3, 20, QByteArray::fromHex("aabb")};
    EQ(frame.encoded().toHex(), QByteArray("0000000a0000000300000014aabb"));
    const CommandFrame initial = CommandFrame::initialNegotiation();
    EQ(initial.sequence, 1u);
    EQ(initial.command, 0u);
    EQ(initial.encoded().toHex(), QByteArray("000000140000000100000000000000000000001d00000001"));
}

static void tcpDecoder()
{
    const QByteArray encoded = CommandFrame{9, 8, QByteArray::fromHex("010203")}.encoded();
    TcpFrameDecoder decoder;
    decoder.append(encoded.left(6));
    CHECK(!decoder.next().has_value());
    decoder.append(encoded.mid(6));
    const auto frame = decoder.next();
    CHECK(frame.has_value());
    EQ(frame->sequence, 9u);
    EQ(frame->command, 8u);
    EQ(frame->payload, QByteArray::fromHex("010203"));
    CHECK(!decoder.next().has_value());

    TcpFrameDecoder invalid;
    invalid.append(QByteArray::fromHex("000000070000000100000000"));
    QString error;
    CHECK(!invalid.next(&error).has_value());
    CHECK(error.contains("invalid length"));
}

static void serverRecords()
{
    ServerRecordDecoder decoder;
    decoder.append(QByteArray::fromHex("000000080000000100000000"));
    const auto positive = decoder.next();
    CHECK(positive.has_value());
    EQ(positive->code, 1);
    const auto record = parseServerRecord(*positive);
    CHECK(record.has_value());
    EQ(int(record->kind), int(ServerRecordKind::Positive));
    EQ(record->positive.payload, QByteArray::fromHex("00000000"));

    const QByteArray endpoint = QByteArray::fromHex("00000019fffffff9000000093132372e302e302e3100001b5900000000");
    ServerRecordDecoder fragmentDecoder;
    fragmentDecoder.append(endpoint.left(9));
    CHECK(!fragmentDecoder.next().has_value());
    fragmentDecoder.append(endpoint.mid(9));
    const auto endpointFrame = fragmentDecoder.next();
    CHECK(endpointFrame.has_value());
    const auto endpointRecord = parseServerRecord(*endpointFrame);
    CHECK(endpointRecord.has_value());
    EQ(int(endpointRecord->kind), int(ServerRecordKind::EndpointNotice));
    EQ(endpointRecord->endpoint.host, QStringLiteral("127.0.0.1"));
    EQ(endpointRecord->endpoint.value1, 7001u);
    EQ(endpointRecord->endpoint.value2, 0u);

    ServerRecordDecoder commandDecoder;
    const QByteArray commandBody = commandPayload("match.1", "Command", "2,1,1,0");
    QByteArray commandFrame;
    commandFrame = appendBig32(commandFrame, static_cast<quint32>(commandBody.size() + 4));
    commandFrame = appendBig32(commandFrame, static_cast<quint32>(-8));
    commandFrame.append(commandBody);
    commandDecoder.append(commandFrame);
    const auto command = commandDecoder.next();
    CHECK(command.has_value());
    const auto commandRecord = parseServerRecord(*command);
    CHECK(commandRecord.has_value());
    EQ(commandRecord->command.first, QStringLiteral("match.1"));
    EQ(commandRecord->command.second, QStringLiteral("Command"));
    EQ(commandRecord->command.third, QStringLiteral("2,1,1,0"));

    const auto integer = parseServerRecord(ServerRecordFrame{-10, QByteArray::fromHex("00000001")});
    CHECK(integer.has_value());
    EQ(integer->integer.value, 1);
    const auto blob = parseServerRecord(ServerRecordFrame{-12, QByteArray::fromHex("000000047801aabb")});
    CHECK(blob.has_value());
    EQ(blob->gameBuffer.value, 4u);
    EQ(blob->gameBuffer.blob, QByteArray::fromHex("7801aabb"));
    const auto stream = parseServerRecord(ServerRecordFrame{-13, QByteArray::fromHex("0000000200000002aabbccdd")});
    CHECK(stream.has_value());
    EQ(stream->streamed.frameSize, 2u);
    EQ(stream->streamed.frameCount, 2u);
    EQ(stream->streamed.frames.size(), 2);
    EQ(stream->streamed.trailing, QByteArray());
}

static void peerPackets()
{
    const QVector<PeerPacket> packets = {
        parsePeerPacket(QByteArray("0.123 _")),
        parsePeerPacket(QByteArray::fromHex("017d7d0000")),
        parsePeerPacket(QByteArray::fromHex("0301000000ffffffff0b000ae330")),
        parsePeerPacket(QByteArray::fromHex("040c0c47b400")),
        parsePeerPacket(QByteArray::fromHex("050c47b400")),
    };
    EQ(int(packets[1].kind), int(PeerPacketKind::SyncRequest));
    EQ(packets[1].nonce, 32125u);
    EQ(int(packets[2].kind), int(PeerPacketKind::Input));
    EQ(packets[2].input.startFrame, 1u);
    EQ(packets[2].input.ackFrame, -1);
    EQ(packets[2].input.compressedBitCount, 11);
    EQ(packets[2].input.inputBits, QByteArray::fromHex("e330"));
    EQ(packets[2].input.encoded(), QByteArray::fromHex("0301000000ffffffff0b000ae330"));
    EQ(int(packets[3].kind), int(PeerPacketKind::QualityReport));
    EQ(int(packets[3].frameAdvantage), 12);
    EQ(packets[3].nonce, 11814668u);
    EQ(int(packets[4].kind), int(PeerPacketKind::QualityReply));
    const PeerSummary summary = summarizePeerPackets(packets);
    EQ(summary.holePunchCount, 1);
    EQ(summary.binaryTypeCounts.value(3), 1);
    EQ(summary.binaryTypeCounts.value(4), 1);
    EQ(summary.payloadLengthCounts.value(5), 2);

    const auto truncated = parsePeerPacket(QByteArray::fromHex("03020000000500000011000a4300"));
    EQ(int(truncated.kind), int(PeerPacketKind::Binary));
}

static void holePunchAndPlan()
{
    const HolePunchMessage initial{QStringLiteral("0.123"), {}, false, {}};
    EQ(initial.payload(), QByteArray("0.123 _"));
    const HolePunchMessage acknowledged{QStringLiteral("0.123"), QStringLiteral("0.456"), true, QStringLiteral("0.456")};
    EQ(acknowledged.payload(), QByteArray("0.123 0.456 ok"));
    const auto parsed = HolePunchMessage::parse(QByteArray("0.456 0.123 ok"));
    CHECK(parsed.has_value());
    EQ(parsed->remoteKnowsLocalToken, true);
    EQ(parsed->acknowledgedToken, QStringLiteral("0.123"));

    ServedPlan plan;
    plan.quark = QStringLiteral("1234567890-42.1");
    plan.masterPort = 7000;
    EQ(plan.registrationPayload(), QStringLiteral("1234567890-42.1/7001"));
    EQ(plan.expectedOkPayload(), QStringLiteral("ok 1234567890-42.1"));
    EQ(plan.usePortsPayload(), QStringLiteral("useports/1234567890-42.1"));
    EQ(parseOkPayload(QByteArray("ok 1234567890-42.1")), QStringLiteral("1234567890-42.1"));
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    commandFrames();
    tcpDecoder();
    serverRecords();
    peerPackets();
    holePunchAndPlan();
    if (failures) { qCritical() << failures << "netplay test(s) failed"; return 1; }
    qInfo() << "All Fightcade netplay protocol tests passed.";
    return 0;
}
