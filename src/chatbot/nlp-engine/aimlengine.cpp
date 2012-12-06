/*
 * Copyright (C) 2012 Andres Pagliano, Gabriel Miretti, Gonzalo Buteler,
 * Nestor Bustamante, Pablo Perez de Angelis
 *
 * This file is part of LVK Chatbot.
 *
 * LVK Chatbot is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LVK Chatbot is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LVK Chatbot.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "nlp-engine/aimlengine.h"
#include "nlp-engine/rule.h"
#include "nlp-engine/nullsanitizer.h"
#include "nlp-engine/nulllemmatizer.h"
#include "nlp-engine/nlpproperties.h"
#include "common/settings.h"
#include "common/settingskeys.h"
#include "common/logger.h"

#include <QStringList>
#include <QFile>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QtDebug>
#include <cassert>

#define ANY_USER    ""

//--------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------

namespace
{

// getCategoryId provide a simple way to map two numbers in one unique number
// getRuleId and getInputNumber retrieve the original values
// Asumming sizeof(long) == 8, that's true for most modern archs
// 2^12 = 4K input rules should be far enough and we have 2^52 different rules ids

const int  INPUT_NUMBER_BITS = 12; 
const long INPUT_NUMBER_MASK = 0xfffl;

//--------------------------------------------------------------------------------------------------

inline long getCategoryId(Lvk::Nlp::RuleId ruleId, int inputNumber)
{
    return ((long)ruleId << INPUT_NUMBER_BITS) | (long)inputNumber;
}

//--------------------------------------------------------------------------------------------------

inline Lvk::Nlp::RuleId getRuleId(long categoryId)
{
    return categoryId >> INPUT_NUMBER_BITS;
}

//--------------------------------------------------------------------------------------------------

inline int getInputNumber(long categoryId)
{
    return categoryId & INPUT_NUMBER_MASK;
}

//--------------------------------------------------------------------------------------------------

// convert IDs returned by AIMLParser to Nlp::Engine::MatchList
inline void convert(Lvk::Nlp::Engine::MatchList &matches, const QList<long> &categoriesId)
{
    matches.clear();

    if (categoriesId.size() > 0) {
        long catId = categoriesId.last();
        matches.append(QPair<Lvk::Nlp::RuleId, int>(getRuleId(catId), getInputNumber(catId)));
    }
}

//--------------------------------------------------------------------------------------------------

// "std::make_ptr"-like function to construct QSharedPointers
template<class T>
inline QSharedPointer<T> makeSharedPtr(T *p)
{
    return QSharedPointer<T>(p);
}

} // namespace

//--------------------------------------------------------------------------------------------------
// AimlEngine
//--------------------------------------------------------------------------------------------------

Lvk::Nlp::AimlEngine::AimlEngine()
    : m_preSanitizer(new Nlp::NullSanitizer()),
      m_postSanitizer(new Nlp::NullSanitizer()),
      m_lemmatizer(new Nlp::NullLemmatizer()),
      m_logFile(new QFile()),
      m_mutex(new QMutex(QMutex::Recursive)),
      m_dirty(false),
      m_setTopics(false)
{
    initLog();
}

//--------------------------------------------------------------------------------------------------

Lvk::Nlp::AimlEngine::AimlEngine(Sanitizer *sanitizer)
    : m_preSanitizer(sanitizer),
      m_postSanitizer(new Nlp::NullSanitizer()),
      m_lemmatizer(new Nlp::NullLemmatizer()),
      m_logFile(new QFile()),
      m_mutex(new QMutex(QMutex::Recursive)),
      m_dirty(false),
      m_setTopics(false)
{
    initLog();
}

//--------------------------------------------------------------------------------------------------

Lvk::Nlp::AimlEngine::AimlEngine(Sanitizer *preSanitizer, Lemmatizer *lemmatizer,
                                 Sanitizer *postSanitizer)
    : m_preSanitizer(preSanitizer),
      m_postSanitizer(postSanitizer),
      m_lemmatizer(lemmatizer),
      m_logFile(new QFile()),
      m_mutex(new QMutex(QMutex::Recursive)),
      m_dirty(false),
      m_setTopics(false)
{
    initLog();
}

//--------------------------------------------------------------------------------------------------

Lvk::Nlp::AimlEngine::~AimlEngine()
{
    delete m_mutex;
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::initLog()
{
    Cmn::Settings settings;
    QString logsPath = settings.value(SETTING_LOGS_PATH).toString();
    QString filename = logsPath + QDir::separator() + "aiml_parser.log";

    Cmn::Logger::rotateLog(filename);

    m_logFile->setFileName(filename);

    if (!m_logFile->open(QFile::Append)) {
        qCritical() << "AimlEngine: Cannot open log file" << filename;
    }
}

//--------------------------------------------------------------------------------------------------

Lvk::Nlp::RuleList Lvk::Nlp::AimlEngine::rules() const
{
    QMutexLocker locker(m_mutex);

    return m_rules;
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::setRules(const Lvk::Nlp::RuleList &rules)
{
    qDebug() << "AimlEngine: Setting new AIML rules...";

    QMutexLocker locker(m_mutex);

    m_rules = rules;

    m_dirty = true;
}

//--------------------------------------------------------------------------------------------------

QString Lvk::Nlp::AimlEngine::getResponse(const QString &input, MatchList &matches)
{
    return getResponse(input, ANY_USER, matches);
}

//--------------------------------------------------------------------------------------------------

QString Lvk::Nlp::AimlEngine::getResponse(const QString &input, const QString &target,
                                          MatchList &matches)
{
    matches.clear();

    MatchList allMatches;
    QStringList responses = getAllResponses(input, target, allMatches);

    if (!allMatches.empty()) {
        matches.append(allMatches.first());

        assert(!responses.empty());
        return responses.first();
    } else {
        return "";
    }
}

//--------------------------------------------------------------------------------------------------

QStringList Lvk::Nlp::AimlEngine::getAllResponses(const QString &input, MatchList &matches)
{
    return _getAllResponses(input, ANY_USER, matches);
}

//--------------------------------------------------------------------------------------------------

QStringList Lvk::Nlp::AimlEngine::getAllResponses(const QString &input, const QString &target,
                                                  MatchList &matches)
{
    return _getAllResponses(input, target, matches);
}

//--------------------------------------------------------------------------------------------------

inline QStringList Lvk::Nlp::AimlEngine::_getAllResponses(const QString &input,
                                                          const QString &target,
                                                          MatchList &matches)
{
    QMutexLocker locker(m_mutex);

    if (m_dirty) {
        qDebug("AimlEngine: Dirty flag set. Refreshing AIML rules...");
        refreshAiml();
        m_dirty = false;
    }

    qDebug() << "AimlEngine: Getting response for input" << input
             << "and target" << target << "...";

    QString normInput = input;
    normalize(normInput);
    normInput.remove('&'); // Ignore &

    QStringList responses = getAllResponsesWithParser(normInput, target, matches, target);

    // No response found with the given target, fallback to rules with any user
    if (responses.isEmpty() && target != ANY_USER) {
        responses = getAllResponsesWithParser(normInput, target, matches, ANY_USER);
    }

    qDebug() << "AimlEngine: Responses found: " << responses;

    return responses;
}

//--------------------------------------------------------------------------------------------------

QStringList Lvk::Nlp::AimlEngine::getAllResponsesWithParser(const QString &normInput,
                                                            const QString &target,
                                                            Engine::MatchList &matches,
                                                            const QString &parserName)
{
    QStringList responses;
    QString response;
    QList<long> categoriesId;

    ParsersMap::iterator it = m_parsers.find(parserName);

    if (it != m_parsers.end()) {
        // We keep the topic for each target, otherwise the Chatbot will confuse topics
        // if it's talking with two or more people at the same time
        (*it)->setTopic(m_topics[target]);
        response = (*it)->getResponse(normInput, categoriesId);
        m_topics[target] = (*it)->topic();

        // An empty response is considered not valid
        if (response.isEmpty()) {
            categoriesId.clear();
        }

        if (response != "Internal Error!" && categoriesId.size() > 0) { // FIXME harcoded string
            responses.append(response);
            convert(matches, categoriesId);
        }
    }

    return responses;
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::refreshAiml()
{
    QString aiml;

    m_parsers.clear();

    // Initialize AIML parser for rules without targets

    buildAiml(aiml, ANY_USER);
    m_parsers[ANY_USER] = makeSharedPtr(new AIMLParser(aiml, m_logFile.get()));

    // Initialize AIML parser for each different target

    foreach (const Nlp::Rule &rule, m_rules) {
        foreach (const QString &target, rule.target()) {
            if (!m_parsers.contains(target)) {
                buildAiml(aiml, target);
                m_parsers[target] = makeSharedPtr(new AIMLParser(aiml, m_logFile.get()));
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::buildAiml(QString &aiml, const QString &target)
{
    aiml = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>";
    aiml += "<aiml>";

    // Build AIML for rules that match the given target

    for (int i = 0; i < m_rules.size(); ++i) {
        const QStringList &targetList = m_rules[i].target();
        if ((target == ANY_USER && targetList.isEmpty()) || targetList.contains(target)) {
            buildAiml(aiml, m_rules[i]);
        }
    }

    aiml += "</aiml>";
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::buildAiml(QString &aiml, const Rule &rule)
{
    const QStringList &inputs = rule.input();

    for (int i = 0; i < inputs.size(); ++i) {
        // id is not part of AIML standar. It's an LVK extension to know which rule has matched
        QString catId = QString::number(getCategoryId(rule.id(), i));

        QString input = inputs[i].trimmed();
        normalize(input);
        escape(input);

        QString randOuput;
        buildAimlRandOutput(randOuput, rule.output());
        escape(randOuput);

        QString topic = rule.topic();
        topic.remove('"');
        escape(topic);

        // build category AIML string
        QString cat = QString("<category>"
                              "<id>%1</id>"
                              "<pattern>%2</pattern>"
                              "<template>"
                              "<think><set name=\"topic\">%3</set></think>"
                              "%4"
                              "</template>"
                              "</category>")
                              .arg(catId, input, topic, randOuput);;

        if (m_setTopics) {
            // Add category with topic
            aiml += "<topic name=\"" + topic + "\">" + cat + "</topic>";
            // Add category also with default topic as fallback mechanism
            aiml += "<topic name=\"\">" + cat + "</topic>";
        } else {
            // No topics
            aiml += cat;
        }
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::buildAimlRandOutput(QString &aiml, const QStringList &output) const
{
    aiml.clear();

    if (output.size() == 1) {
        aiml += output[0];
    } else if (output.size() > 1) {
        aiml += "<random>";
        for (int j = 0; j < output.size(); ++j) {
            aiml += "<li>" + output[j] + "</li>";
        }
        aiml += "</random>";
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::normalize(QStringList &inputList)
{
    for (int i = 0; i < inputList.size(); ++i) {
        normalize(inputList[i]);
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::normalize(QString &input)
{
    qDebug() << " - Normalizing input" << input;

    input = m_preSanitizer->sanitize(input);
    input = m_lemmatizer->lemmatize(input);
    input = m_postSanitizer->sanitize(input);
}


//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::escape(QString &str)
{
    str.replace('&', "&amp;");
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::setPreSanitizer(Lvk::Nlp::Sanitizer *sanitizer)
{
    QMutexLocker locker(m_mutex);

    m_preSanitizer.reset(sanitizer ? sanitizer : new NullSanitizer());
    m_dirty = true;
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::setLemmatizer(Lvk::Nlp::Lemmatizer *lemmatizer)
{
    QMutexLocker locker(m_mutex);

    m_lemmatizer.reset(lemmatizer ? lemmatizer : new NullLemmatizer());
    m_dirty = true;
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::setPostSanitizer(Lvk::Nlp::Sanitizer *sanitizer)
{
    QMutexLocker locker(m_mutex);

    m_postSanitizer.reset(sanitizer ? sanitizer : new NullSanitizer);
    m_dirty = true;
}

//--------------------------------------------------------------------------------------------------

bool Lvk::Nlp::AimlEngine::hasVariable(const QString &/*input*/)
{
    return false;
}

//--------------------------------------------------------------------------------------------------

bool Lvk::Nlp::AimlEngine::hasKeywordOp(const QString &/*input*/)
{
    return false;
}

//--------------------------------------------------------------------------------------------------

bool Lvk::Nlp::AimlEngine::hasRegexOp(const QString &/*input*/)
{
    return false;
}

//--------------------------------------------------------------------------------------------------

bool Lvk::Nlp::AimlEngine::hasConditional(const QString &/*output*/)
{
    return false;
}

//--------------------------------------------------------------------------------------------------

QVariant Lvk::Nlp::AimlEngine::property(const QString &name)
{
    if (name == NLP_PROP_PREFER_CUR_TOPIC) {
        return QVariant(m_setTopics);
    } else {
        return QVariant();
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::setProperty(const QString &name, const QVariant &value)
{
    if (name == NLP_PROP_PREFER_CUR_TOPIC) {
        QMutexLocker locker(m_mutex);

        if (value.toBool() == true && !m_setTopics) {
            qDebug() << "AimlEngine: Enabled topics";
            m_setTopics = true;
            m_dirty = true;
        }
        if (value.toBool() == false && m_setTopics) {
            qDebug() << "AimlEngine: Disabled topics";
            m_setTopics = false;
            m_dirty = true;
        }
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::Nlp::AimlEngine::clear()
{
    QMutexLocker locker(m_mutex);

    m_dirty = true;
    m_rules.clear();
    m_parsers.clear();
    m_topics.clear();
}

