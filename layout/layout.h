#ifndef LAYOUT_H
#define LAYOUT_H

#include "../graph/graph.h"
#include "../graph/grapharray.h"

#include <QList>
#include <QMap>
#include <QVector3D>
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QAtomicInt>

class Layout : public QObject
{
    Q_OBJECT
private:
    QAtomicInt cancelAtomic;
    void setCancel(bool cancel)
    {
        int expectedValue = static_cast<int>(!cancel);
        int newValue = static_cast<int>(!!cancel);

        cancelAtomic.testAndSetRelaxed(expectedValue, newValue);
    }

    virtual void executeReal() = 0;

protected:
    const ReadOnlyGraph* _graph;
    NodeArray<QVector3D>* positions;
    int _iterations;

    bool shouldCancel()
    {
        return cancelAtomic.testAndSetRelaxed(1, 1);
    }

public:
    Layout(const ReadOnlyGraph& graph, NodeArray<QVector3D>& positions, int defaultNumIterations = 1) :
        cancelAtomic(0),
        _graph(&graph),
        positions(&positions),
        _iterations(defaultNumIterations)
    {}

    const ReadOnlyGraph& graph() { return *_graph; }

    void execute()
    {
        setCancel(false);
        executeReal();
    }

    void cancel()
    {
        setCancel(true);
    }

    // Indicates that the algorithm is doing no useful work
    virtual bool shouldPause() { return false; }

    bool iterative() { return _iterations != 1; }

    const static int Unbounded = -1;
    int iterations() { return _iterations; }
    void setIterations(int _iterations) { this->_iterations = _iterations; }

signals:
    void progress(int percentage);
    void complete();
};

class GraphModel;

class LayoutFactory
{
protected:
    GraphModel* _graphModel;

public:
    LayoutFactory(GraphModel* _graphModel) :
        _graphModel(_graphModel)
    {}
    virtual ~LayoutFactory() {}

    const GraphModel& graphModel() const { return *_graphModel; }

    virtual Layout* create(ComponentId componentId) const = 0;
};

class LayoutThread : public QThread
{
    Q_OBJECT
private:
    const LayoutFactory* layoutFactory;
    QMap<ComponentId, Layout*> layouts;
    QMap<Layout*, int> iterationsRemaining;
    QMutex mutex;
    bool _pause;
    bool _isPaused;
    bool _stop;
    QWaitCondition waitForPause;
    QWaitCondition waitForResume;

public:
    LayoutThread(const LayoutFactory* layoutFactory) :
        layoutFactory(layoutFactory),
        _pause(false), _isPaused(false), _stop(false)
    {}

    virtual ~LayoutThread()
    {
        stop();
        wait();
        delete layoutFactory;
    }

    void add(ComponentId componentId)
    {
        QMutexLocker locker(&mutex);

        if(!layouts.contains(componentId))
        {
            Layout* layout = layoutFactory->create(componentId);

            // Take ownership of the algorithm
            layout->moveToThread(this);
            layouts.insert(componentId, layout);
            iterationsRemaining[layout] = layout->iterations();

            start();
        }
    }

    void remove(ComponentId componentId)
    {
        bool resumeAfterRemoval = false;

        if(isPaused())
        {
            pauseAndWait();
            resumeAfterRemoval = true;
        }

        if(layouts.contains(componentId))
        {
            Layout* layout = layouts[componentId];
            layouts.remove(componentId);
            delete layout;
        }

        if(resumeAfterRemoval)
            resume();
    }

    void pause()
    {
        QMutexLocker locker(&mutex);
        _pause = true;

        for(Layout* layout : layouts.values())
            layout->cancel();
    }

    void pauseAndWait()
    {
        QMutexLocker locker(&mutex);
        _pause = true;

        for(Layout* layout : layouts.values())
            layout ->cancel();

        waitForPause.wait(&mutex);
    }

    bool isPaused()
    {
        QMutexLocker locker(&mutex);
        return _isPaused;
    }

    void resume()
    {
        {
            QMutexLocker locker(&mutex);
            _pause = false;
            _isPaused = false;
        }

        waitForResume.wakeAll();
    }

    void stop()
    {
        {
            QMutexLocker locker(&mutex);
            _stop = true;
            _pause = false;

            for(Layout* layout : layouts.values())
                layout->cancel();
        }

        waitForResume.wakeAll();
    }

private:
    bool workRemaining()
    {
        for(Layout* layout : layouts.values())
        {
            if(iterationsRemaining[layout] != 0)
                return true;
        }

        return false;
    }

    bool allLayoutAlgorithmsShouldPause()
    {
        for(Layout* layout : layouts.values())
        {
            if(!layout->shouldPause())
                return false;
        }

        return true;
    }

    void run() Q_DECL_OVERRIDE
    {
        while(workRemaining())
        {
            for(Layout* layout : layouts.values())
            {
                if(layout->shouldPause())
                    continue;

                if(iterationsRemaining[layout] != 0)
                    layout->execute();

                if(iterationsRemaining[layout] != Layout::Unbounded)
                    iterationsRemaining[layout]--;
            }

            {
                QMutexLocker locker(&mutex);

                if(_pause || allLayoutAlgorithmsShouldPause())
                {
                    _isPaused = true;
                    waitForPause.wakeAll();
                    waitForResume.wait(&mutex);
                }

                if(_stop)
                    break;
            }
        }

        for(Layout* layout : layouts.values())
            delete layout;
    }
};

#endif // LAYOUT_H
