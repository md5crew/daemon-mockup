#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>

#define SOCKET_FILE "2safe.sock"

void computeHash(QString path);

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    auto server = new QLocalServer();
    QObject::connect(server, &QLocalServer::newConnection, [=](){
        auto socket = server->nextPendingConnection();
        while (socket->bytesAvailable() < 1) {
            socket->waitForReadyRead();
        }

        QObject::connect(socket, &QLocalSocket::disconnected, &QLocalSocket::deleteLater);
        if(!socket->isValid() || socket->bytesAvailable() < 1) {
            return;
        }

        qDebug() << "Handling incoming connection";
        QTextStream in(socket);
        QString data(in.readAll());
        qDebug() << "Data read:" << data;

        /* JSON PARSE */
        QJsonParseError json_error;
        QJsonDocument message = QJsonDocument::fromJson(data.toLatin1(), &json_error);
        if(json_error.error) {
            qWarning() << "JSON error:" << json_error.errorString();
            return;
        }

        /* LOGIC */
        QString verb = message.object().value("verb").toString();
        QString path = message.object().value("path").toString();
        qDebug() << "Command:" << verb;
        qDebug() << "Arg0:" << path;
        if(verb == "hash") {
            computeHash(path);
        } else {
            qWarning() << "Unknown command:" << verb;
        }
        /* ----- */
    });

    /* the file we trying to bind to */
    QFile sock_file(QDir::tempPath() + QDir::separator() + SOCKET_FILE);

    if(!server->listen(SOCKET_FILE)) {
        /* remove old socket file */
        if(sock_file.exists() && sock_file.remove()) {
            /* retry bind */
            if(!server->listen(SOCKET_FILE)) {
                qWarning() << "Unable to bind socket to" << SOCKET_FILE << ", will quit";
                delete server;
                return -1;
            } else {
                qDebug() << "Local socket created at" << server->fullServerName();
            }
        } else {
            /* somthing went wrong */
            qWarning() << "Unable to bind socket on" << SOCKET_FILE
                       << ", try to remove it:" << QFileInfo(sock_file).canonicalFilePath();
            delete server;
            return -1;
        }
    } else {
        qDebug() << "Local socket created at" << server->fullServerName();
    }

    a.exec();
    /* exit */
    server->close();
    if(sock_file.exists()) {
        sock_file.remove();
    }
    delete server;
    return 0;
}

void computeHash(QString path) {
    QFile file(path);
    QFile outfile(path + ".md5.txt");
    if(!file.exists()) {
        qWarning() << "No such file exist" << path;
        return;
    }
    if(!file.open(QFile::ReadOnly)) {
        qWarning() << "Unable to read file" << path;
        return;
    }
    if(!outfile.open(QFile::WriteOnly)) {
        qWarning() << "Unable to write hash file" << QFileInfo(outfile).canonicalFilePath();
        return;
    }

    qDebug() << "Computing MD5 hash of file" << path;
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&file);
    QString hexHashStr(hash.result().toHex());
    qDebug() << "MD5:" << hexHashStr << "\n";
    file.close();
    QTextStream out(&outfile);
    out << hexHashStr;
    out.flush();
    outfile.close();
}
