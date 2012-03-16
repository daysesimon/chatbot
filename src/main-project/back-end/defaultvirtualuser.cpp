#include "defaultvirtualuser.h"
#include "nlpengine.h"
#include "random.h"

#include <QFile>
#include <QDateTime>

#define LOG_FILENAME            "chat_conversations.log"
#define DATE_TIME_LOG_FORMAT    "dd-MM-yy hh:mm:ss"

//--------------------------------------------------------------------------------------------------

Lvk::BE::DefaultVirtualUser::DefaultVirtualUser(Nlp::Engine *engine /*= 0*/)
    : m_engine(engine), m_logFile(new QFile(LOG_FILENAME))
{
    m_logFile->open(QFile::WriteOnly | QFile::Append);
}

//--------------------------------------------------------------------------------------------------

Lvk::BE::DefaultVirtualUser::~DefaultVirtualUser()
{
    delete m_logFile;
}

//--------------------------------------------------------------------------------------------------

QString Lvk::BE::DefaultVirtualUser::getResponse(const QString &input, const QString &from)
{
    if (!m_engine) {
        logError("No engine set!");
        return "";
    }

    Nlp::Engine::MatchList matches;
    QString response = m_engine->getResponse(input, matches);

    bool matched = !response.isEmpty() && matches.size() > 0;

    if (!matched) {
        if (m_evasives.size() > 0) {
            response = m_evasives[Common::Random::getInt(0, m_evasives.size() - 1)];
        } else {
            logError("No evasives found!");
            response = "";
        }
    }

    logConversation(input, from, response, matched);

    return response;
}

//--------------------------------------------------------------------------------------------------

QPixmap Lvk::BE::DefaultVirtualUser::getAvatar()
{
    return QPixmap();
}

//--------------------------------------------------------------------------------------------------

void Lvk::BE::DefaultVirtualUser::setNlpEngine(Nlp::Engine *engine)
{
    m_engine = engine;
}

//--------------------------------------------------------------------------------------------------

void Lvk::BE::DefaultVirtualUser::setEvasives(const QStringList &evasives)
{
    m_evasives = evasives;
}

//--------------------------------------------------------------------------------------------------

void Lvk::BE::DefaultVirtualUser::logConversation(const QString &input, const QString &from,
                                                  const QString &response, bool matched)
{
    m_logFile->write(QDateTime::currentDateTime().toString(DATE_TIME_LOG_FORMAT).toUtf8());
    m_logFile->write(" ");
    m_logFile->write(from.toUtf8());
    m_logFile->write(": ");
    m_logFile->write(input.toUtf8());
    m_logFile->write(matched ? " +--> " : " !--> ");
    m_logFile->write(response.toUtf8());
    m_logFile->write("\n");
    m_logFile->flush();
}

//--------------------------------------------------------------------------------------------------

void Lvk::BE::DefaultVirtualUser::logError(const QString &msg)
{
    m_logFile->write(QDateTime::currentDateTime().toString(DATE_TIME_LOG_FORMAT).toUtf8());
    m_logFile->write(" ");
    m_logFile->write("ERROR: ");
    m_logFile->write(msg.toUtf8());
    m_logFile->write("\n");
    m_logFile->flush();
}


