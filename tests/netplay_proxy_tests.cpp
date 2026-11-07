#include "fightcade/netplayproxy.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QThread>
#include <QUdpSocket>
#include <QDebug>

#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
int failures = 0;
#define CHECK(cond) do { if (!(cond)) { qCritical() << "FAIL" << __LINE__ << #cond; ++failures; } } while (0)

bool bindUdp(int &fd, quint16 port)
{
    fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) return false;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (::bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        ::close(fd);
        fd = -1;
        return false;
    }
    timeval timeout{0, 100000};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    return true;
}

struct UdpPeer {
    int fd = -1;
    sockaddr_in address{};
    int recv(char *buffer, int length) const
    {
        socklen_t size = sizeof(address);
        return static_cast<int>(::recvfrom(fd, buffer, length, 0,
                                           reinterpret_cast<sockaddr *>(const_cast<sockaddr_in *>(&address)), &size));
    }
    void sendTo(const QHostAddress &host, quint16 port, const QByteArray &payload) const
    {
        sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_addr.s_addr = htonl(host.toIPv4Address());
        target.sin_port = htons(port);
        ::sendto(fd, payload.constData(), payload.size(), 0,
                 reinterpret_cast<const sockaddr *>(&target), sizeof(target));
    }
};

class FakePeer
{
public:
    FakePeer(quint16 port) : m_port(port) { bindUdp(m_fd, port); }
    ~FakePeer() { stop(); }
    void start()
    {
        m_thread = std::thread([this] {
            char buffer[2048];
            while (m_running) {
                sockaddr_in from{};
                socklen_t fromSize = sizeof(from);
                const int count = static_cast<int>(::recvfrom(m_fd, buffer, sizeof(buffer), 0,
                                                               reinterpret_cast<sockaddr *>(&from), &fromSize));
                if (count <= 0) continue;
                const QByteArray payload(buffer, count);
                if (payload.startsWith("0.")) {
                    m_peerEndpoint = from;
                    const QByteArray reply = QByteArray("0.777 ") + payload.split(' ').value(0) + " ok";
                    sendRaw(from, reply);
                } else if (payload == "RELAY") {
                    sendRaw(from, QByteArray("REPLY"));
                    ++m_relayCount;
                }
            }
        });
    }
    void stop()
    {
        m_running = false;
        if (m_thread.joinable()) m_thread.join();
        if (m_fd >= 0) { ::close(m_fd); m_fd = -1; }
    }
    std::atomic<int> m_relayCount{0};
private:
    void sendRaw(const sockaddr_in &to, const QByteArray &payload)
    {
        ::sendto(m_fd, payload.constData(), payload.size(), 0,
                 reinterpret_cast<const sockaddr *>(&to), sizeof(to));
    }
    int m_fd = -1;
    quint16 m_port;
    sockaddr_in m_peerEndpoint{};
    std::atomic<bool> m_running{true};
    std::thread m_thread;
};
class FakeMaster
{
public:
    FakeMaster(quint16 port, quint16 peerPort) : m_port(port), m_peerPort(peerPort) { bindUdp(m_fd, port); }
    ~FakeMaster() { stop(); }
    void start()
    {
        m_thread = std::thread([this] {
            char buffer[2048];
            while (m_running) {
                sockaddr_in from{};
                socklen_t fromSize = sizeof(from);
                const int count = static_cast<int>(::recvfrom(m_fd, buffer, sizeof(buffer), 0,
                                                               reinterpret_cast<sockaddr *>(&from), &fromSize));
                if (count <= 0) continue;
                const QByteArray payload(buffer, count);
                if (payload.startsWith(m_quark.toUtf8() + '/')) {
                    m_client = from;
                    const QByteArray reply = "ok " + m_quark.toUtf8();
                    sendRaw(from, reply);
                } else if (payload == "ok") {
                    QByteArray address;
                    address.append(char(127)); address.append(char(0)); address.append(char(0)); address.append(char(1));
                    address.append(char(m_peerPort & 0xff));
                    address.append(char((m_peerPort >> 8) & 0xff));
                    sendRaw(from, address);
                } else if (payload.startsWith("useports/")) {
                    m_usedPorts = true;
                }
            }
        });
    }
    void stop()
    {
        m_running = false;
        if (m_thread.joinable()) m_thread.join();
        if (m_fd >= 0) { ::close(m_fd); m_fd = -1; }
    }
    std::atomic<bool> m_usedPorts{false};
    QString m_quark = QStringLiteral("1234567890-42.0");
private:
    void sendRaw(const sockaddr_in &to, const QByteArray &payload)
    {
        ::sendto(m_fd, payload.constData(), payload.size(), 0,
                 reinterpret_cast<const sockaddr *>(&to), sizeof(to));
    }
    int m_fd = -1;
    quint16 m_port;
    quint16 m_peerPort;
    sockaddr_in m_client{};
    std::atomic<bool> m_running{true};
    std::thread m_thread;
};
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    constexpr quint16 masterPort = 17012;
    constexpr quint16 peerPort = 17013;
    constexpr quint16 proxyPort = 17010;
    constexpr quint16 proxyBindPort = 17011;

    qputenv("DECKADE_PROXY_MASTER_HOST", "127.0.0.1");
    qputenv("DECKADE_PROXY_LOCAL_PORT", QByteArray::number(proxyPort));
    qputenv("DECKADE_PROXY_MASTER_BIND_PORT", QByteArray::number(proxyBindPort));

    FakeMaster master(masterPort, peerPort);
    FakePeer peer(peerPort);
    master.start();
    peer.start();

    QVariantMap match{
        {"gameId", "sfiii3nr1"},
        {"quarkId", "1234567890-42"},
        {"playerId", 0},
        {"port", masterPort},
        {"delay", 0},
        {"ranked", 0}
    };
    NetplayProxy proxy(match);
    const bool prepared = proxy.prepare();
    qInfo().noquote() << "proxy prepared=" << prepared << "log=" << proxy.logPath();
    CHECK(prepared);
    CHECK(proxy.ready());
    const QVariantMap env = proxy.environment();
    CHECK(env.contains("MACADE_GGPO_PROXY_HOST"));
    CHECK(env.value("MACADE_GGPO_PROXY_PORT").toString() == QString::number(proxyPort));
    CHECK(env.value("MACADE_GGPO_TCP_REGISTER_PORT").toString() == QString::number(proxyPort));

    QUdpSocket emulator;
    CHECK(emulator.bind(QHostAddress::LocalHost, 0));
    const quint16 emulatorPort = emulator.localPort();
    emulator.writeDatagram(QByteArray("RELAY"), QHostAddress::LocalHost, proxyPort);
    QByteArray received;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 2000 && received.isEmpty()) {
        QCoreApplication::processEvents();
        if (emulator.hasPendingDatagrams()) {
            received.resize(emulator.pendingDatagramSize());
            emulator.readDatagram(received.data(), received.size());
        }
        QThread::msleep(10);
    }
    CHECK(received == "REPLY");
    CHECK(peer.m_relayCount.load() == 1);
    CHECK(!master.m_usedPorts.load());

    proxy.stop();
    master.stop();
    peer.stop();
    if (failures) { qCritical() << failures << "proxy test(s) failed"; return 1; }
    qInfo() << "Fightcade proxy lifecycle test passed";
    return 0;
}
