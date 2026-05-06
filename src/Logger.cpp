#include "Logger.h"
#include <QDebug>
#include <QDateTime>

Logger* Logger::m_instance = nullptr;

Logger::Logger(QObject* parent)
    : QObject(parent)
{
}

Logger::~Logger()
{
}

Logger* Logger::instance()
{
    if (!m_instance) {
        m_instance = new Logger();
    }
    return m_instance;
}

void Logger::destroyInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
}

void Logger::log(const QString& message, LogLevel level)
{
    QString levelStr;
    switch (level) {
        case Info: levelStr = "[INFO]"; break;
        case Warning: levelStr = "[WARN]"; break;
        case Error: levelStr = "[ERROR]"; break;
    }

    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString logMessage = QString("%1 %2 %3").arg(timeStr, levelStr, message);
    
    qDebug() << logMessage;
}
