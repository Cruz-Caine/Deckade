#include "fightcade/fightcadeclient.h"

#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    FightcadeClient client;

    QObject::connect(&client, &FightcadeClient::loginFailed,
                     [&](const QString &code, const QString &message) {
                         qInfo().noquote() << "LOGIN_FAILED" << code << message;
                         app.exit(0);
                     });
    QObject::connect(&client, &FightcadeClient::loggedIn,
                     [&](const QVariantMap &user, const QVariantMap &) {
                         qInfo().noquote() << "LOGGED_IN" << user.value("name").toString();
                         app.exit(0);
                     });
    QObject::connect(&client, &FightcadeClient::statusChanged,
                     [&] { qInfo().noquote() << "STATUS" << client.status() << client.statusMessage(); });

    QTimer::singleShot(20000, &app, [&] { qInfo() << "TIMEOUT"; app.exit(2); });
    client.login(QStringLiteral("deckade_probe_no_user"), QStringLiteral("x"));
    return app.exec();
}
