#ifndef FORCEDIRECTEDLAYOUT_H
#define FORCEDIRECTEDLAYOUT_H

#include "layout.h"
#include "graph/componentmanager.h"
#include "shared/utils/circularbuffer.h"

#include <QVector3D>

#include <vector>

struct ForceDirectedDisplacement
{
    QVector3D _repulsive;
    QVector3D _attractive;

    QVector3D _previous;
    QVector3D _next;
    float _previousLength = 0.0f;
    float _nextLength = 0.0f;

    void computeAndDamp();
};

using ForceDirectedDisplacements = NodeArray<ForceDirectedDisplacement>;

class ForceDirectedLayout : public Layout
{
    Q_OBJECT
private:
    const float MINIMUM_STDDEV_THRESHOLD = 0.008f;
    const float FINETUNE_STDDEV_DELTA = 0.000005f;
    const float OSCILLATE_STDDEV_DELTA_PERCENT = 1.0f;
    const float MAXIMUM_AVG_FORCE_FOR_STOP = 1.0f;
    static const int OSCILLATE_DELTA_SAMPLE_SIZE = 500;
    static const int OSCILLATE_RUN_COUNT = 5;
    static const int STDDEV_INCREASES_BEFORE_SWITCH_TO_OSCILLATE = 500;
    static const int FINETUNE_DELTA_SAMPLE_SIZE = 50;
    static const int FINETUNE_SMOOTHING_SIZE = 10;
    static const int INITIAL_SMOOTHING_SIZE = 50;

    CircularBuffer<float, FINETUNE_DELTA_SAMPLE_SIZE> _prevStdDevs;
    CircularBuffer<float, FINETUNE_DELTA_SAMPLE_SIZE> _prevAvgForces;
    CircularBuffer<float, OSCILLATE_DELTA_SAMPLE_SIZE> _prevCaptureStdDevs;

    enum class ChangeDetectionPhase { Initial, FineTune, Oscillate, Finished };

    ChangeDetectionPhase _changeDetectionPhase = ChangeDetectionPhase::Initial;

    ForceDirectedDisplacements* _displacements;

    float _forceStdDeviation = 0;
    float _forceMean = 0;
    float _prevUnstableStdDev = 0;

    int _unstableIterationCount = 0;
    int _increasingStdDevIterationCount = 0;

    void fineTuneChangeDetection();
    void oscillateChangeDetection();
    void initialChangeDetection();
    void finishChangeDetection();

public:
    ForceDirectedLayout(const IGraphComponent& graphComponent,
                        ForceDirectedDisplacements& displacements,
                        NodeLayoutPositions& positions,
                        const LayoutSettings* settings) :
        Layout(graphComponent, positions, settings, Iterative::Yes,
            Dimensionality::ThreeDee, 0.4f, 4),
        _displacements(&displacements)
    {}

    bool finished() const override { return _changeDetectionPhase == ChangeDetectionPhase::Finished; }
    void unfinish() override;

    void executeReal(bool firstIteration) override;
};

class ForceDirectedLayoutFactory : public LayoutFactory
{
private:
    ForceDirectedDisplacements _displacements;

public:
    explicit ForceDirectedLayoutFactory(GraphModel* graphModel);

    QString name() const override { return QStringLiteral("ForceDirected"); }
    QString displayName() const override { return QObject::tr("Force Directed"); }
    std::unique_ptr<Layout> create(ComponentId componentId, NodeLayoutPositions& nodePositions) override;
};

#endif // FORCEDIRECTEDLAYOUT_H
