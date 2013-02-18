#include <QtCore/QString>
#include <QtTest/QtTest>

#include "nlp-engine/cb2engine.h"
#include "nlp-engine/lemmatizerfactory.h"
#include "common/settingskeys.h"
#include "common/settings.h"

#include "ruledef.h"

#define UTF8        QString::fromUtf8

using namespace Lvk;

//--------------------------------------------------------------------------------------------------
// Cb2EnginefullTest
//--------------------------------------------------------------------------------------------------

class Cb2EnginefullTest : public QObject
{
    Q_OBJECT

public:
    Cb2EnginefullTest();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testCase1();
    void testCase1_data();

private:
    Nlp::Cb2Engine *m_engine;
};

//--------------------------------------------------------------------------------------------------

Cb2EnginefullTest::Cb2EnginefullTest()
    : m_engine(0)
{
    Cmn::Settings().setValue(SETTING_APP_LANGUAGE, "es_AR");

    m_engine = new Nlp::Cb2Engine();
    m_engine->setLemmatizer(Nlp::LemmatizerFactory().createLemmatizer());

    setRules1(m_engine);
}

//--------------------------------------------------------------------------------------------------

void Cb2EnginefullTest::initTestCase()
{
}

//--------------------------------------------------------------------------------------------------

void Cb2EnginefullTest::cleanupTestCase()
{
}

//--------------------------------------------------------------------------------------------------

void Cb2EnginefullTest::testCase1()
{
    QFETCH(QString, input);
    QFETCH(QString, expOutput);
    QFETCH(int, ruleId);
    QFETCH(int, inputIdx);

    Nlp::Engine::MatchList matches;
    QString output = m_engine->getResponse(input, matches);

    if (!expOutput.isNull()) {
        QCOMPARE(output, expOutput);
        QVERIFY(matches.size() > 0);
        QCOMPARE(ruleId, static_cast<int>(matches[0].first));
        QCOMPARE(inputIdx, matches[0].second);
    } else {
        QVERIFY(output.isEmpty());
        QVERIFY(matches.isEmpty());
    }
}

//--------------------------------------------------------------------------------------------------

void Cb2EnginefullTest::testCase1_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expOutput");
    QTest::addColumn<int>("ruleId");
    QTest::addColumn<int>("inputIdx");

    QTest::newRow(" 0") << "Hola"                           << RULE_1_OUTPUT_1 << RULE_1_ID << 0;
    QTest::newRow(" 1") << "HOLAAA!!"                       << RULE_1_OUTPUT_1 << RULE_1_ID << 0;
    QTest::newRow(" 2") << "hooola$%&/()^[]{}-_.,=:;'\\!?"  << RULE_1_OUTPUT_1 << RULE_1_ID << 0;
    QTest::newRow(" 3") << "Hola como andas?"               << RULE_1_OUTPUT_1 << RULE_1_ID << 1;
    QTest::newRow(" 3") << "Hola, que hacessss?"            << RULE_1_OUTPUT_1 << RULE_1_ID << 1;
    QTest::newRow(" 4") << "chau"                           << RULE_2_OUTPUT_1 << RULE_2_ID << 0;
    QTest::newRow(" 5") << "CHAUUU CHE..."                  << RULE_2_OUTPUT_1 << RULE_2_ID << 0;
    QTest::newRow(" 6") << "bueno chau"                     << RULE_2_OUTPUT_1 << RULE_2_ID << 1;
    QTest::newRow(" 7") << "hey hola"                       << QString()       << 0         << 1;
    QTest::newRow(" 8") << "Bye"                            << QString()       << 0         << 1;
    QTest::newRow(" 9") << "jugar futbol"                   << RULE_3_OUTPUT_1 << RULE_3_ID << 0;
    QTest::newRow("10") << "yo jugaba futbol"               << RULE_3_OUTPUT_1 << RULE_3_ID << 0;
    QTest::newRow("11") << UTF8("Yo jugu\xc3\xa9 al futbol mucho tiempo")      << RULE_3_OUTPUT_1 << RULE_3_ID << 0;
    QTest::newRow("12") << UTF8("Yo?? jugu\xc3\xa9 al futbol muuucho tiempo!") << RULE_3_OUTPUT_1 << RULE_3_ID << 0;
    QTest::newRow("13") << "te gusta el chocolate?"         << "Si me gusta el chocolate"      << RULE_4_ID << 0;
    QTest::newRow("14") << "te gusta los chocolates?"       << "Si me gusta los chocolates"    << RULE_4_ID << 0;
    QTest::newRow("15") << "te gusta el chocolate mucho?"   << "Si me gusta el chocolate"      << RULE_4_ID << 1;
    QTest::newRow("16") << "te gusta A mas que B?"          << "Entre A y B no se..."          << RULE_5_ID << 0;
    QTest::newRow("17") << "Solamente te gusta A mas que B?"<< "Entre A y B no se..."          << RULE_6_ID << 1;
    QTest::newRow("18") << "w1 v1a v1b w2 v2 w3 hola w4"    << "w5 v2 w6 Hola! w7 v1a v1b w8"  << RULE_7_ID << 0;
    QTest::newRow("19") << ":)"                             << RULE_8_OUTPUT_1 << RULE_8_ID << 0;
    QTest::newRow("20") << ":D"                             << RULE_8_OUTPUT_1 << RULE_8_ID << 1;
    QTest::newRow("21") << ":-)"                            << RULE_8_OUTPUT_1 << RULE_8_ID << 2;
    QTest::newRow("22") << "=)"                             << RULE_8_OUTPUT_1 << RULE_8_ID << 3;
    QTest::newRow("24") << "perro"                          << QString()       << 0         << 0;
    QTest::newRow("25") << "perros"                         << RULE_9_OUTPUT_1 << RULE_9_ID << 0;
    QTest::newRow("26") << "perrito"                        << RULE_9_OUTPUT_1 << RULE_9_ID << 1;
}

//--------------------------------------------------------------------------------------------------

QTEST_APPLESS_MAIN(Cb2EnginefullTest)

#include "cb2enginefulltest.moc"
