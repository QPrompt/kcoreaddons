/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kjobtest.h"

#include <QMetaEnum>
#include <QTimer>
#include <QSignalSpy>
#include <QTest>
#include <QVector>

#include <string>

QTEST_MAIN(KJobTest)

KJobTest::KJobTest()
    : loop(this)
{

}

void KJobTest::testEmitResult_data()
{
    QTest::addColumn<int>("errorCode");
    QTest::addColumn<QString>("errorText");

    QTest::newRow("no error") << int(KJob::NoError) << QString();
    QTest::newRow("error no text") << 2 << QString();
    QTest::newRow("error with text") << 6 << "oops! an error? naaah, really?";
}

void KJobTest::testEmitResult()
{
    TestJob *job = new TestJob;

    connect(job, &KJob::result, this, &KJobTest::slotResult);

    QFETCH(int, errorCode);
    QFETCH(QString, errorText);

    job->setError(errorCode);
    job->setErrorText(errorText);

    QSignalSpy destroyed_spy(job, SIGNAL(destroyed(QObject*)));
    job->start();
    QVERIFY(!job->isFinished());
    loop.exec();
    QVERIFY(job->isFinished());

    QCOMPARE(m_lastError, errorCode);
    QCOMPARE(m_lastErrorText, errorText);

    // Verify that the job is not deleted immediately...
    QCOMPARE(destroyed_spy.size(), 0);
    QTimer::singleShot(0, &loop, SLOT(quit()));
    // ... but when we enter the event loop again.
    loop.exec();
    QCOMPARE(destroyed_spy.size(), 1);
}

void KJobTest::testProgressTracking()
{
    TestJob *testJob = new TestJob;
    KJob *job = testJob;

    qRegisterMetaType<KJob *>("KJob*");
    qRegisterMetaType<qulonglong>("qulonglong");

    QSignalSpy processed_spy(job, SIGNAL(processedAmount(KJob*,KJob::Unit,qulonglong)));
    QSignalSpy total_spy(job, SIGNAL(totalAmount(KJob*,KJob::Unit,qulonglong)));
    QSignalSpy percent_spy(job, SIGNAL(percent(KJob*,ulong)));

    /* Process a first item. Corresponding signal should be emitted.
     * Total size didn't change.
     * Since the total size is unknown, no percent signal is emitted.
     */
    testJob->setProcessedSize(1);

    QCOMPARE(processed_spy.size(), 1);
    QCOMPARE(processed_spy.at(0).at(0).value<KJob *>(), static_cast<KJob *>(job));
    QCOMPARE(processed_spy.at(0).at(2).value<qulonglong>(), qulonglong(1));
    QCOMPARE(total_spy.size(), 0);
    QCOMPARE(percent_spy.size(), 0);

    /* Now, we know the total size. It's signaled.
     * The new percentage is signaled too.
     */
    testJob->setTotalSize(10);

    QCOMPARE(processed_spy.size(), 1);
    QCOMPARE(total_spy.size(), 1);
    QCOMPARE(total_spy.at(0).at(0).value<KJob *>(), job);
    QCOMPARE(total_spy.at(0).at(2).value<qulonglong>(), qulonglong(10));
    QCOMPARE(percent_spy.size(), 1);
    QCOMPARE(percent_spy.at(0).at(0).value<KJob *>(), job);
    QCOMPARE(percent_spy.at(0).at(1).value<unsigned long>(), static_cast<unsigned long>(10));

    /* We announce a new percentage by hand.
     * Total, and processed didn't change, so no signal is emitted for them.
     */
    testJob->setPercent(15);

    QCOMPARE(processed_spy.size(), 1);
    QCOMPARE(total_spy.size(), 1);
    QCOMPARE(percent_spy.size(), 2);
    QCOMPARE(percent_spy.at(1).at(0).value<KJob *>(), job);
    QCOMPARE(percent_spy.at(1).at(1).value<unsigned long>(), static_cast<unsigned long>(15));

    /* We make some progress.
     * Processed size and percent are signaled.
     */
    testJob->setProcessedSize(3);

    QCOMPARE(processed_spy.size(), 2);
    QCOMPARE(processed_spy.at(1).at(0).value<KJob *>(), job);
    QCOMPARE(processed_spy.at(1).at(2).value<qulonglong>(), qulonglong(3));
    QCOMPARE(total_spy.size(), 1);
    QCOMPARE(percent_spy.size(), 3);
    QCOMPARE(percent_spy.at(2).at(0).value<KJob *>(), job);
    QCOMPARE(percent_spy.at(2).at(1).value<unsigned long>(), static_cast<unsigned long>(30));

    /* We set a new total size, but equals to the previous one.
     * No signal is emitted.
     */
    testJob->setTotalSize(10);

    QCOMPARE(processed_spy.size(), 2);
    QCOMPARE(total_spy.size(), 1);
    QCOMPARE(percent_spy.size(), 3);

    /* We 'lost' the previous work done.
     * Signals both percentage and new processed size.
     */
    testJob->setProcessedSize(0);

    QCOMPARE(processed_spy.size(), 3);
    QCOMPARE(processed_spy.at(2).at(0).value<KJob *>(), job);
    QCOMPARE(processed_spy.at(2).at(2).value<qulonglong>(), qulonglong(0));
    QCOMPARE(total_spy.size(), 1);
    QCOMPARE(percent_spy.size(), 4);
    QCOMPARE(percent_spy.at(3).at(0).value<KJob *>(), job);
    QCOMPARE(percent_spy.at(3).at(1).value<unsigned long>(), static_cast<unsigned long>(0));

    /* We process more than the total size!?
     * Signals both percentage and new processed size.
     * Percentage is 150%
     *
     * Might sounds weird, but verify that this case is handled gracefully.
     */
    testJob->setProcessedSize(15);

    QCOMPARE(processed_spy.size(), 4);
    QCOMPARE(processed_spy.at(3).at(0).value<KJob *>(), job);
    QCOMPARE(processed_spy.at(3).at(2).value<qulonglong>(), qulonglong(15));
    QCOMPARE(total_spy.size(), 1);
    QCOMPARE(percent_spy.size(), 5);
    QCOMPARE(percent_spy.at(4).at(0).value<KJob *>(), job);
    QCOMPARE(percent_spy.at(4).at(1).value<unsigned long>(), static_cast<unsigned long>(150));

    delete job;
}

void KJobTest::testExec_data()
{
    QTest::addColumn<int>("errorCode");
    QTest::addColumn<QString>("errorText");

    QTest::newRow("no error") << int(KJob::NoError) << QString();
    QTest::newRow("error no text") << 2 << QString();
    QTest::newRow("error with text") << 6 << "oops! an error? naaah, really?";
}

void KJobTest::testExec()
{
    TestJob *job = new TestJob;

    QFETCH(int, errorCode);
    QFETCH(QString, errorText);

    job->setError(errorCode);
    job->setErrorText(errorText);

    int resultEmitted = 0;
    // Prove to Kai Uwe that one can connect a job to a lambdas, despite the "private" signal
    connect(job, &KJob::result, this, [&resultEmitted](KJob *) { ++resultEmitted; });

    QSignalSpy destroyed_spy(job, SIGNAL(destroyed(QObject*)));

    QVERIFY(!job->isFinished());
    bool status = job->exec();
    QVERIFY(job->isFinished());

    QCOMPARE(resultEmitted, 1);
    QCOMPARE(status, (errorCode == KJob::NoError));
    QCOMPARE(job->error(),  errorCode);
    QCOMPARE(job->errorText(),  errorText);

    // Verify that the job is not deleted immediately...
    QCOMPARE(destroyed_spy.size(), 0);
    QTimer::singleShot(0, &loop, SLOT(quit()));
    // ... but when we enter the event loop again.
    loop.exec();
    QCOMPARE(destroyed_spy.size(), 1);
}

void KJobTest::testKill_data()
{
    QTest::addColumn<int>("killVerbosity");
    QTest::addColumn<int>("errorCode");
    QTest::addColumn<QString>("errorText");
    QTest::addColumn<int>("resultEmitCount");
    QTest::addColumn<int>("finishedEmitCount");

    QTest::newRow("killed with result") << int(KJob::EmitResult)
                                        << int(KJob::KilledJobError)
                                        << QString()
                                        << 1
                                        << 1;
    QTest::newRow("killed quietly") << int(KJob::Quietly)
                                    << int(KJob::KilledJobError)
                                    << QString()
                                    << 0
                                    << 1;
}

void KJobTest::testKill()
{
    auto *const job = setupErrorResultFinished();
    QSignalSpy destroyed_spy(job, &QObject::destroyed);

    QFETCH(int, killVerbosity);
    QFETCH(int, errorCode);
    QFETCH(QString, errorText);
    QFETCH(int, resultEmitCount);
    QFETCH(int, finishedEmitCount);

    QVERIFY(!job->isFinished());
    job->kill(static_cast<KJob::KillVerbosity>(killVerbosity));
    QVERIFY(job->isFinished());
    loop.processEvents(QEventLoop::AllEvents, 2000);

    QCOMPARE(m_lastError, errorCode);
    QCOMPARE(m_lastErrorText, errorText);

    QCOMPARE(job->error(),  errorCode);
    QCOMPARE(job->errorText(),  errorText);

    QCOMPARE(m_resultCount, resultEmitCount);
    QCOMPARE(m_finishedCount, finishedEmitCount);

    // Verify that the job is not deleted immediately...
    QCOMPARE(destroyed_spy.size(), 0);
    QTimer::singleShot(0, &loop, SLOT(quit()));
    // ... but when we enter the event loop again.
    loop.exec();
    QCOMPARE(destroyed_spy.size(), 1);
}

void KJobTest::testDestroy()
{
    auto *const job = setupErrorResultFinished();
    QVERIFY(!job->isFinished());
    delete job;
    QCOMPARE(m_lastError, static_cast<int>(KJob::NoError));
    QCOMPARE(m_lastErrorText, QString{});
    QCOMPARE(m_resultCount, 0);
    QCOMPARE(m_finishedCount, 1);
}

void KJobTest::testEmitAtMostOnce_data()
{
    QTest::addColumn<bool>("autoDelete");
    QTest::addColumn<QVector<Action>>("actions");

    const auto actionName = [](Action action) {
        return QMetaEnum::fromType<Action>().valueToKey(static_cast<int>(action));
    };

    for (bool autoDelete : {true, false}) {
        for (Action a : {Action::Start, Action::KillQuietly, Action::KillVerbosely}) {
            for (Action b : {Action::Start, Action::KillQuietly, Action::KillVerbosely}) {
                const auto dataTag = std::string{actionName(a)} + '-' + actionName(b)
                                     + (autoDelete ? "-autoDelete" : "");
                QTest::newRow(dataTag.c_str()) << autoDelete << QVector<Action>{a, b};
            }
        }
    }
}

void KJobTest::testEmitAtMostOnce()
{
    auto *const job = setupErrorResultFinished();
    QSignalSpy destroyed_spy(job, &QObject::destroyed);

    QFETCH(bool, autoDelete);
    job->setAutoDelete(autoDelete);

    QFETCH(QVector<Action>, actions);
    for (auto action : actions) {
        switch (action) {
            case Action::Start:
                job->start(); // in effect calls QTimer::singleShot(0, ... emitResult)
                break;
            case Action::KillQuietly:
                QTimer::singleShot(0, job, [=] { job->kill(KJob::Quietly); });
                break;
            case Action::KillVerbosely:
                QTimer::singleShot(0, job, [=] { job->kill(KJob::EmitResult); });
                break;
        }
    }

    QVERIFY(!job->isFinished());
    loop.processEvents(QEventLoop::AllEvents, 2000);
    QCOMPARE(destroyed_spy.size(), autoDelete);
    if (!autoDelete) {
        QVERIFY(job->isFinished());
    }

    QVERIFY(!actions.empty());
    // The first action alone should determine the job's error and result.
    const auto firstAction = actions.front();

    const int errorCode = firstAction == Action::Start ? KJob::NoError
                                                       : KJob::KilledJobError;
    QCOMPARE(m_lastError, errorCode);
    QCOMPARE(m_lastErrorText, QString{});
    if (!autoDelete) {
        QCOMPARE(job->error(), m_lastError);
        QCOMPARE(job->errorText(), m_lastErrorText);
    }

    QCOMPARE(m_resultCount, firstAction == Action::KillQuietly ? 0 : 1);
    QCOMPARE(m_finishedCount, 1);

    if (!autoDelete) {
        delete job;
    }
}

void KJobTest::testDelegateUsage()
{
    TestJob *job1 = new TestJob;
    TestJob *job2 = new TestJob;
    TestJobUiDelegate *delegate = new TestJobUiDelegate;
    QPointer<TestJobUiDelegate> guard(delegate);

    QVERIFY(job1->uiDelegate() == nullptr);
    job1->setUiDelegate(delegate);
    QVERIFY(job1->uiDelegate() == delegate);

    QVERIFY(job2->uiDelegate() == nullptr);
    job2->setUiDelegate(delegate);
    QVERIFY(job2->uiDelegate() == nullptr);

    delete job1;
    delete job2;
    QVERIFY(guard.isNull()); // deleted by job1
}

void KJobTest::testNestedExec()
{
    m_innerJob = nullptr;
    QTimer::singleShot(100, this, SLOT(slotStartInnerJob()));
    m_outerJob = new WaitJob();
    m_outerJob->exec();
}

void KJobTest::slotStartInnerJob()
{
    QTimer::singleShot(100, this, SLOT(slotFinishOuterJob()));
    m_innerJob = new WaitJob();
    m_innerJob->exec();
}

void KJobTest::slotFinishOuterJob()
{
    QTimer::singleShot(100, this, SLOT(slotFinishInnerJob()));
    m_outerJob->makeItFinish();
}

void KJobTest::slotFinishInnerJob()
{
    m_innerJob->makeItFinish();
}

void KJobTest::slotResult(KJob *job)
{
    const auto testJob = qobject_cast<const TestJob *>(job);
    QVERIFY(testJob);
    QVERIFY(testJob->isFinished());

    if (job->error()) {
        m_lastError = job->error();
        m_lastErrorText = job->errorText();
    } else {
        m_lastError = KJob::NoError;
        m_lastErrorText.clear();
    }

    m_resultCount++;
    loop.quit();
}

void KJobTest::slotFinished(KJob *job)
{
    // qobject_cast and dynamic_cast to TestJob* fail when finished() signal is emitted from
    // ~KJob(). The static_cast allows to call the otherwise protected KJob::isFinished().
    // WARNING: don't use this trick in production code, because static_cast-ing
    // to a wrong type and then dereferencing the pointer is undefined behavior.
    // Normally a KJob and its subclasses should manage their finished state on their own.
    // If you *really* need KJob::isFinished() to be public, request this access
    // modifier change in the KJob class.
    QVERIFY(static_cast<const TestJob *>(job)->isFinished());

    if (job->error()) {
        m_lastError = job->error();
        m_lastErrorText = job->errorText();
    } else {
        m_lastError = KJob::NoError;
        m_lastErrorText.clear();
    }

    m_finishedCount++;
}

TestJob *KJobTest::setupErrorResultFinished()
{
    m_lastError = KJob::UserDefinedError;
    m_lastErrorText.clear();
    m_resultCount = 0;
    m_finishedCount = 0;

    auto *job = new TestJob;
    connect(job, &KJob::result, this, &KJobTest::slotResult);
    connect(job, &KJob::finished, this, &KJobTest::slotFinished);
    return job;
}

TestJob::TestJob() : KJob()
{

}

TestJob::~TestJob()
{

}

void TestJob::start()
{
    QTimer::singleShot(0, this, [this] { emitResult(); });
}

bool TestJob::doKill()
{
    return true;
}

void TestJob::setError(int errorCode)
{
    KJob::setError(errorCode);
}

void TestJob::setErrorText(const QString &errorText)
{
    KJob::setErrorText(errorText);
}

void TestJob::setProcessedSize(qulonglong size)
{
    KJob::setProcessedAmount(KJob::Bytes, size);
}

void TestJob::setTotalSize(qulonglong size)
{
    KJob::setTotalAmount(KJob::Bytes, size);
}

void TestJob::setPercent(unsigned long percentage)
{
    KJob::setPercent(percentage);
}

void WaitJob::start()
{
}

void WaitJob::makeItFinish()
{
    emitResult();
}

void TestJobUiDelegate::connectJob(KJob *job)
{
    QVERIFY(job->uiDelegate() != nullptr);
}

#include "moc_kjobtest.cpp"
