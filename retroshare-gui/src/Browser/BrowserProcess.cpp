/* Ricochet - https://ricochet.im/
 * Copyright (C) 2014, John Brooks <john.brooks@dereferenced.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 *    * Neither the names of the copyright owners nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "BrowserProcess_p.h"
#include "../TorControl/CryptoKey.h"
#include "../TorControl/SecureRNG.h"
#include <QDir>
#include <QDebug>
#include <QCoreApplication>

using namespace Browser;

BrowserProcess::BrowserProcess(QObject *parent)
    : QObject(parent), d(new BrowserProcessPrivate(this))
{
}

BrowserProcess::~BrowserProcess()
{
    if (state() > NotStarted)
        stop();
}

BrowserProcessPrivate::BrowserProcessPrivate(BrowserProcess *q)
    : QObject(q), q(q), state(BrowserProcess::NotStarted)
{
    connect(&process, &QProcess::started, this, &BrowserProcessPrivate::processStarted);
    connect(&process, (void (QProcess::*)(int, QProcess::ExitStatus))&QProcess::finished,
            this, &BrowserProcessPrivate::processFinished);
    connect(&process, (void (QProcess::*)(QProcess::ProcessError))&QProcess::error,
            this, &BrowserProcessPrivate::processError);
    connect(&process, &QProcess::readyRead, this, &BrowserProcessPrivate::processReadable);

    controlPortTimer.setInterval(500);
    connect(&controlPortTimer, &QTimer::timeout, this, &BrowserProcessPrivate::tryReadPriflePrefs);
}

QString BrowserProcess::executable() const
{
    return d->executable;
}

void BrowserProcess::setExecutable(const QString &path)
{
    d->executable = path;
}

QString BrowserProcess::dataDir() const
{
    return d->dataDir;
}

void BrowserProcess::setDataDir(const QString &path)
{
    d->dataDir = path;
}

QString BrowserProcess::defaultBrowserProfile() const
{
    return d->defaultBrowserProfile;
}

void BrowserProcess::setDefaultBrowserProfile(const QString &path)
{
    d->defaultBrowserProfile = path;
}

QStringList BrowserProcess::extraSettings() const
{
    return d->extraSettings;
}

void BrowserProcess::setExtraSettings(const QStringList &settings)
{
    d->extraSettings = settings;
}

BrowserProcess::State BrowserProcess::state() const
{
    return d->state;
}

QString BrowserProcess::errorMessage() const
{
    return d->errorMessage;
}

void BrowserProcess::start()
{
    if (state() > NotStarted)
        return;

    d->errorMessage.clear();

    if (d->executable.isEmpty() || d->dataDir.isEmpty()) {
        d->errorMessage = QStringLiteral("Firefox Browser executable and data directory not specified");
        d->state = Failed;
        emit errorMessageChanged(d->errorMessage);
        emit stateChanged(d->state);
        return;
    }

    if (!d->ensureFilesExist()) {
        d->state = Failed;
        emit errorMessageChanged(d->errorMessage);
        emit stateChanged(d->state);
        return;
    }

//    QByteArray password = controlPassword();
//    QByteArray hashedPassword = torControlHashedPassword(password);
//    if (password.isEmpty() || hashedPassword.isEmpty()) {
//        d->errorMessage = QStringLiteral("Random password generation failed");
//        d->state = Failed;
//        emit errorMessageChanged(d->errorMessage);
//        emit stateChanged(d->state);
//    }

    QStringList args;
    if (!d->defaultBrowserProfile.isEmpty())
        args << QStringLiteral("--profile") << d->defaultBrowserProfile;
    //args << QStringLiteral("-f") << d->torrcPath();
    args << QStringLiteral("DataDirectory") << d->dataDir;
    //args << QStringLiteral("HashedControlPassword") << QString::fromLatin1(hashedPassword);
    //args << QStringLiteral("ControlPort") << QStringLiteral("auto");
    //args << QStringLiteral("ControlPortWriteToFile") << d->controlPortFilePath();
    //args << QStringLiteral("__OwningControllerProcess") << QString::number(qApp->applicationPid());
    args << d->extraSettings;

    d->state = Starting;
    emit stateChanged(d->state);

//    if (QFile::exists(d->controlPortFilePath()))
//        QFile::remove(d->controlPortFilePath());
//    d->controlPort = 0;
//    d->controlHost.clear();

    d->process.setProcessChannelMode(QProcess::MergedChannels);
    d->process.start(d->executable, args, QIODevice::ReadOnly);
}

void BrowserProcess::stop()
{
    if (state() < Starting)
        return;

    d->controlPortTimer.stop();

    if (d->process.state() == QProcess::Starting)
        d->process.waitForStarted(2000);

    d->state = NotStarted;

    // Windows can't terminate the process well, but Tor will clean itself up
#ifndef Q_OS_WIN
    if (d->process.state() == QProcess::Running) {
        d->process.terminate();
        if (!d->process.waitForFinished(5000)) {
            qWarning() << "Tor process" << d->process.pid() << "did not respond to terminate, killing...";
            d->process.kill();
            if (!d->process.waitForFinished(2000)) {
                qCritical() << "Tor process" << d->process.pid() << "did not respond to kill!";
            }
        }
    }
#endif

    emit stateChanged(d->state);
}

QHostAddress BrowserProcess::proxyHost()
{
    QString addr=QString::fromStdString(d->proxyHost);
    QHostAddress address(addr);
    return address;
}

quint16 BrowserProcess::proxyPort()
{
    quint16 ret= d->proxyPort;
    return ret;
}

bool BrowserProcessPrivate::ensureFilesExist()
{
    QFile pref(prefJSPath());
    if (!pref.exists()) {
        QDir dir(dataDir);
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            errorMessage = QStringLiteral("Cannot create Tor data directory: %1").arg(dataDir);
            return false;
        }

        if (!pref.open(QIODevice::ReadWrite)) {
            errorMessage = QStringLiteral("Cannot create Browser Profile configuration file: %1").arg(prefJSPath());
            return false;
        }
    }

    return true;
}

QString BrowserProcessPrivate::prefJSPath() const
{
    return QDir::toNativeSeparators(dataDir) + QDir::separator() + QStringLiteral("prefs.js");
}


void BrowserProcessPrivate::processStarted()
{
    state = BrowserProcess::Connecting;
    emit q->stateChanged(state);

    controlPortAttempts = 0;
    controlPortTimer.start();
}

void BrowserProcessPrivate::processFinished()
{
    if (state < BrowserProcess::Starting)
        return;

    controlPortTimer.stop();
    errorMessage = process.errorString();
    if (errorMessage.isEmpty())
        errorMessage = QStringLiteral("Process exited unexpectedly (code %1)").arg(process.exitCode());
    state = BrowserProcess::Failed;
    emit q->errorMessageChanged(errorMessage);
    emit q->stateChanged(state);
}

void BrowserProcessPrivate::processError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart || error == QProcess::Crashed)
        processFinished();
}

void BrowserProcessPrivate::processReadable()
{
    while (process.bytesAvailable() > 0) {
        QByteArray line = process.readLine(2048).trimmed();
        if (!line.isEmpty())
            emit q->logMessage(QString::fromLatin1(line));
    }
}

bool BrowserProcessPrivate::tryNetworkProxyInfo(){


    uint32_t status ;
    return rsPeers->getProxyServer(RS_HIDDEN_TYPE_TOR, proxyHost, proxyPort, status);

}
void BrowserProcessPrivate::tryReadPriflePrefs()
{
    uint16_t localProxyPort;
    std::string localProxyHost;

    bool isUpdate = false;
    QStringList listlines;

    QFile file(prefJSPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text) && tryNetworkProxyInfo()) {

        while (!file.atEnd()) {
            QString sline = file.readLine();
            //QString sline(line);

            if(sline.contains("network.proxy.socks_port"))
            {

                QRegExp exp("\\((.*)\\)");
                if(exp.indexIn(sline) >= 0)
                {
                    //qDebug() << exp.cap(0);   // "(acb+d)"
                    //qDebug() << exp.cap(1);   // "abc+d"
                    QString network = exp.cap(1);

                    QStringList result = network.split(",");
                    QString currentPort = result[1].trimmed();

                    localProxyPort = currentPort.toUShort();
                    localProxyHost = "localhost";

                    //compare the network proxy info vs localProfile Proxy Info
                    qDebug()<<"TorProxy Info Port: "<<proxyPort<< " TorHost: ";
                    if(localProxyPort == proxyPort)
                        isUpdate=true;
                    else{
                        sline.replace(sline.indexOf(currentPort),currentPort.size(),proxyPort);
                    }


                }

            }
            listlines.push_back(sline);

        }
        file.close();
        if(isUpdate){
            state = BrowserProcess::Ready;
            emit q->stateChanged(state);
        }else{
            file.open(QIODevice::ReadWrite );
            QTextStream stream(&file);
            for(auto it = listlines.begin(); it !=listlines.end(); it++ ){
                QString aline = *it;
                stream<<aline;
            }
            file.close();
            state = BrowserProcess::Ready;
            emit q->stateChanged(state);
        }


    }
    else{
        state = BrowserProcess::Failed;
        emit q->errorMessageChanged(errorMessage);
        emit q->stateChanged(state);
    }
}


