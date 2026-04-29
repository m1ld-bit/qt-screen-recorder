#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>

class Logger : public QObject
{
    Q_OBJECT

public:
    enum LogLevel {
        Info,
        Warning,
        Error
    };

    static Logger* instance();
    static void destroyInstance();

    void log(const QString& message, LogLevel level = Info);
    void setLogFile(const QString& filePath);

private:
    explicit Logger(QObject* parent = nullptr);
    ~Logger();

    static Logger* m_instance;
    static QMutex m_mutex;

    QFile m_logFile;
    QTextStream m_textStream;
    QString m_logFilePath;
    bool m_initialized;
};

#define LOG_INFO(msg) Logger::instance()->log(msg, Logger::Info)
#define LOG_WARN(msg) Logger::instance()->log(msg, Logger::Warning)
#define LOG_ERROR(msg) Logger::instance()->log(msg, Logger::Error)

#endif // LOGGER_H

