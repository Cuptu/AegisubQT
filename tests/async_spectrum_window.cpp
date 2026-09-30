#include "AsyncSpectrumWindow.h"
#include <QCoreApplication>
#include <QDataStream>
#include <QElapsedTimer>
#include <QFile>
#include <QSemaphore>
#include <QRunnable>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QtTest/QTest>
#include <atomic>
#include <cmath>
#include <cstdio>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static bool writeTone(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    file.write("RIFF",4); stream << quint32(36+32000);
    file.write("WAVEfmt ",8); stream << quint32(16) << quint16(1) << quint16(1)
        << quint32(16000) << quint32(32000) << quint16(2) << quint16(16);
    file.write("data",4); stream << quint32(32000);
    for (int i=0;i<16000;++i) stream << qint16(std::sin(i*2*3.141592653589793*640/16000)*16000);
    return stream.status() == QDataStream::Ok;
}
template<class Predicate> static bool waitFor(Predicate predicate) {
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed()<15000) QTest::qWait(1);
    return predicate();
}
struct Gate {
    QSemaphore entered, release;
    QThreadPool &pool;
    bool unlocked=false;
    explicit Gate(QThreadPool &pool):pool(pool) {
        pool.start(QRunnable::create([this]{entered.release();release.acquire();})); entered.acquire();
    }
    void unlock() {if(!unlocked){unlocked=true;release.release();}}
    ~Gate(){unlock();pool.waitForDone();}
};
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    QThreadPool pool; pool.setMaxThreadCount(1);
    QTemporaryDir temp;
    CHECK(temp.isValid() && writeTone(temp.filePath("tone.wav")));
    AudioPcmProvider tone, noise, blank;
    CHECK(tone.loadWav(temp.filePath("tone.wav")));
    CHECK(noise.loadVirtualAudio(AudioPcmProvider::VirtualKind::Noise,9000,16000));
    CHECK(blank.loadVirtualAudio(AudioPcmProvider::VirtualKind::Blank,9000,16000));
    std::atomic<int> ready=0;
    auto cache=std::make_shared<AsyncSpectrumWindow>([&]{++ready;},&pool);
    cache->request({tone,1,9,8,30,20});
    CHECK(waitFor([&]{return bool(cache->result());}));
    const auto first=cache->result();
    CHECK(first->workerThread != reinterpret_cast<quintptr>(QThread::currentThreadId()));
    CHECK(first->recomputed && first->source==1 && first->count==63 && first->image.size()==QSize(63,512));
    CHECK(qRed(first->image.pixel(30,41)) > qRed(first->image.pixel(30,10))+40); // Independent 640Hz dominant bin.
    AegisubStftCore reference;
    CHECK(reference.processAudio(tone) && reference.ensureWindow(tone,30,20));
    CHECK(first->image==reference.stftTexture());
    cache->request({tone,1,9,8,31,20});
    CHECK(cache->result()); // Scrolling within a cached window does not flash blank while recentering.
    CHECK(waitFor([&]{const auto r=cache->result();return r && r->revision!=first->revision;}));
    CHECK(!cache->result()->recomputed && cache->result()->image==first->image); // Persistent cache hit across separate jobs.
    CHECK(cache->result()->imageRevision == first->imageRevision); // No redundant GPU upload on a cache hit.
    CHECK(pool.waitForDone(15000));
    {
        Gate gate(pool);
        const auto previous=ready.load();
        QElapsedTimer submission; submission.start();
        for(int i=0;i<200;++i) cache->request({noise,2,9,8,10000+i*1000,3000});
        CHECK(!cache->result() && ready.load()==previous); // No synchronous computation; all requests are still queued.
        printf("coalesced 200 requests in %lld us\n", submission.nsecsElapsed()/1000);
        gate.unlock();
        CHECK(waitFor([&]{return bool(cache->result());}));
        const auto last=cache->result();
        CHECK(last->source==2 && last->start<=209000-1500 && last->start+last->count*last->step>=209000+1500);
        CHECK(last->count<=AegisubStftCore::MAX_TEXTURE_FRAMES && ready.load()==previous+1);
        CHECK(first->image==reference.stftTexture()); // Published images stay immutable after another FFT.
    }
    // Observe a live cache miss, then replace its source and viewport before it can publish.
    const auto beforeInflight = ready.load();
    cache->request({noise,3,10,8,350000,4096});
    CHECK(waitFor([&]{return cache->activeRevision()!=0;}));
    CHECK(!cache->result());
    int ticks=0;
    QTimer heartbeat; heartbeat.setInterval(1);
    QObject::connect(&heartbeat,&QTimer::timeout,[&]{++ticks;}); heartbeat.start();
    cache->request({blank,4,9,8,1000,100});
    CHECK(waitFor([&]{const auto r=cache->result();return r && r->source==4;}));
    heartbeat.stop();
    const auto final=cache->result();
    CHECK(ticks>0 && final->recomputed);
    for(int y=0;y<final->image.height();++y) for(int x=0;x<final->image.width();++x)
        CHECK(final->image.pixel(x,y)==qRgba(0,0,0,255));
    printf("GUI heartbeat ticks while worker drains obsolete FFT: %d; latest blank FFT %lld us\n",ticks,final->computeNanoseconds/1000);
    CHECK(pool.waitForDone(15000));
    CHECK(ready.load() == beforeInflight + 1); // Only the replacement source published; no transient stale result.
    {
        Gate gate(pool);
        const auto previous=ready.load();
        cache->request({noise,5,9,8,300000,4096});
        cache->invalidate();
        gate.unlock();
        CHECK(pool.waitForDone(15000) && !cache->result() && ready.load()==previous);
    }
    {
        Gate gate(pool);
        auto closing=std::make_shared<AsyncSpectrumWindow>([&]{++ready;},&pool);
        closing->request({noise,6,9,8,300000,4096});
        const auto previous=ready.load();
        closing->stop(); closing.reset();
        gate.unlock();
        CHECK(pool.waitForDone(15000) && ready.load()==previous);
    }
    cache->stop();
    puts("PASS real PCM/tone spectrum, worker thread, persistent cache, immutable images, coalesced scrub, stale in-flight source rejection, GUI heartbeat, invalidation and queued destruction");
    return 0;
}
