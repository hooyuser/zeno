#if defined(ZENO_MULTIPROCESS) && defined(ZENO_IPC_USE_TCP)
#include "ztcpserver.h"
#include <zeno/extra/GlobalState.h>
#include <zeno/utils/log.h>
#include <QMessageBox>
#include <zeno/zeno.h>
#include "launch/viewdecode.h"
#include "util/log.h"
#include "zenoapplication.h"
#include <zenomodel/include/graphsmanagment.h>
#include "settings/zsettings.h"


ZTcpServer::ZTcpServer(QObject *parent)
    : QObject(parent)
    , m_tcpServer(nullptr)
    , m_tcpSocket(nullptr)
    , m_port(0)
{
}

void ZTcpServer::init(const QHostAddress& address)
{
    m_tcpServer = new QTcpServer(this);
    bool bSucceed = false;
    int maxTry = 10, i = 0;

    while (i < maxTry)
    {
        int minPort = 49152;
        int maxPort = 65535;
        m_port = rand() % (maxPort - minPort + 1) + minPort;
        if (m_tcpServer->listen(QHostAddress::LocalHost, m_port))
        {
            zeno::log_info("tcp server listend, port is {}", m_port);
            bSucceed = true;
            break;
        }
        i++;
    }
    if (!bSucceed)
    {
        zeno::log_info("tcp server listend");
    }
    connect(m_tcpServer, SIGNAL(newConnection()), this, SLOT(onNewConnection()));
}

void ZTcpServer::startProc(const std::string& progJson)
{
    ZASSERT_EXIT(m_tcpServer);
    if (m_proc && m_proc->isOpen())
    {
        zeno::log_info("background process already running");
        return;
    }

    zeno::log_info("launching program...");
    zeno::log_debug("program JSON: {}", progJson);

    m_proc = std::make_unique<QProcess>();
    m_proc->setInputChannelMode(QProcess::InputChannelMode::ManagedInputChannel);
    m_proc->setReadChannel(QProcess::ProcessChannel::StandardOutput);
    m_proc->setProcessChannelMode(QProcess::ProcessChannelMode::ForwardedErrorChannel);
    int sessionid = zeno::getSession().globalState->sessionid;
    m_proc->start(QCoreApplication::applicationFilePath(),
                  QStringList({"-runner", QString::number(sessionid), "-port", QString::number(m_port)}));
    if (!m_proc->waitForStarted(-1)) {
        zeno::log_warn("process failed to get started, giving up");
        return;
    }

    m_proc->write(progJson.data(), progJson.size());
    m_proc->closeWriteChannel();

    connect(m_proc.get(), SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(onProcFinished(int, QProcess::ExitStatus)));
    connect(m_proc.get(), SIGNAL(readyRead()), this, SLOT(onProcPipeReady()));
}

void ZTcpServer::killProc()
{
    if (m_proc) {
        m_proc->terminate();
        m_proc = nullptr;
    }
}

void ZTcpServer::onNewConnection()
{
    ZASSERT_EXIT(m_tcpServer);
    m_tcpSocket = m_tcpServer->nextPendingConnection();
    if (!m_tcpSocket)
    {
        zeno::log_error("tcp connection recv failed");
    }
    else
    {
        zeno::log_debug("tcp connection succeed");

    }
    connect(m_tcpSocket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(m_tcpSocket, SIGNAL(disconnected()), this, SLOT(onDisconnect()));

    QSettings settings(zsCompanyName, zsEditor);
    auto cachedir = settings.value("zencachedir").toString();
    auto cachenum = settings.value("zencachenum").toString();
    int cnum = 0;
    if (!cachenum.isEmpty()) {  // set from zhouhang-style panel, by QSettings
        bool ok = false;
        int t = cachenum.toInt(&ok);
        if (ok) cnum = t;
        else zeno::log_warn("failed to parse ZENCACHENUM: {}", cachenum.toStdString());
    }
    auto cdir = cachedir.toStdString();
    viewDecodeSetFrameCache(cdir.c_str(), cnum);
    viewDecodeClear();
}

void ZTcpServer::onReadyRead()
{
    QByteArray arr = m_tcpSocket->readAll();
    qint64 redSize = arr.size();
    zeno::log_debug("qtcpsocket got {} bytes (ping test has 19)", redSize);
    if (redSize > 0) {
        viewDecodeAppend(arr.data(), redSize);
    }
}

void ZTcpServer::onProcPipeReady()
{
    if (!m_proc) {
        return;
    }
    QByteArray arr = m_proc->readAll();
    QList<QByteArray> lst = arr.split('\n');
    for (QByteArray line : lst)
    {
        if (!line.isEmpty())
        {
            std::cout << line.data() << std::endl;
            ZWidgetErrStream::appendFormatMsg(line.toStdString());
        }
    }
}

void ZTcpServer::onDisconnect()
{
    viewDecodeFinish();
}

void ZTcpServer::onProcFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::NormalExit)
    {
        if (m_proc)
            m_proc->terminate();
        m_proc = nullptr;
        zeno::log_info("runner process normally exited with {}", exitCode);
    }
    else if (exitStatus == QProcess::CrashExit)
    {
        if (m_proc)
            m_proc->terminate();
        m_proc= nullptr;
        zeno::log_error("runner process crashed with code {}", exitCode);
    }
    viewDecodeFinish();
}

#endif
