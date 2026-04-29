#include "Logger.h"
#include <QDateTime>
#include <QDir>

Logger* Logger::m_instance = nullptr;
QMutex Logger::m_mutex;

Logger::Logger(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
{
    QString logDir = QDir::currentPath() + "/logs";
    QDir().mkpath(logDir);
    QString fileName = logDir + "/recorder_" + QDateTime::currentDateTime().toString("yyyyMMdd") + ".log";
    setLogFile(fileName);
}

Logger::~Logger()
{
    if (m_logFile.isOpen()) {
        m_textStream.flush();
        m_logFile.close();
    }
}

Logger* Logger::instance()
{
    if (!m_instance) {
        QMutexLocker locker(&m_mutex);
        if (!m_instance) {
            m_instance = new Logger();
        }
    }
    return m_instance;
}

void Logger::destroyInstance()
{
    QMutexLocker locker(&m_mutex);
    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
}

void Logger::setLogFile(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);
    if (m_logFile.isOpen()) {
        m_textStream.flush();
        m_logFile.close();
    }

    m_logFilePath = filePath;
    m_logFile.setFileName(filePath);

    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_textStream.setDevice(&m_logFile);
        m_textStream.setCodec("UTF-8");
        m_initialized = true;
    }
}

void Logger::log(const QString& message, LogLevel level)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) return;

    QString levelStr;
    switch (level) {
        case Info: levelStr = "[INFO]"; break;
        case Warning: levelStr = "[WARN]"; break;
        case Error: levelStr = "[ERROR]"; break;
    }

    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logMessage = QString("%1 %2 %3").arg(timeStr, levelStr, message);

    m_textStream << logMessage << endl;
    m_textStream.flush();
}

