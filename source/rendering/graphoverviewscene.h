#ifndef GRAPHOVERVIEWSCENE_H
#define GRAPHOVERVIEWSCENE_H

#include "scene.h"
#include "graphrenderer.h"
#include "openglfunctions.h"
#include "transition.h"

#include "../graph/graph.h"
#include "../graph/grapharray.h"

#include <vector>
#include <mutex>
#include <memory>

#include <QRect>

class GraphModel;

class GraphOverviewScene :
        public Scene,
        public GraphInitialiser,
        protected OpenGLFunctions
{
    Q_OBJECT

public:
    GraphOverviewScene(GraphRenderer* graphRenderer);

    void initialise();
    void update(float t);
    void render();
    void setSize(int width, int height);

    bool transitionActive() const;

    void onShow();
    void onHide();

    struct LayoutData
    {
        LayoutData() : _alpha(1.0f) {}
        LayoutData(const QRect& rect, float alpha) :
            _rect(rect), _alpha(alpha) {}

        QRect _rect;
        float _alpha;
    };

    const ComponentArray<LayoutData>& componentLayout() { return _componentLayout; }

    void zoom(float delta);
    int renderSizeDivisor() { return _renderSizeDivisor; }
    void setRenderSizeDivisor(int divisor);

    void startTransitionFromComponentMode(ComponentId focusComponentId,
                                          float duration = 0.3f,
                                          Transition::Type transitionType = Transition::Type::EaseInEaseOut,
                                          std::function<void()> finishedFunction = []{});
    void startTransitionToComponentMode(ComponentId focusComponentId,
                                        float duration = 0.3f,
                                        Transition::Type transitionType = Transition::Type::EaseInEaseOut,
                                        std::function<void()> finishedFunction = []{});

private:
    GraphRenderer* _graphRenderer;
    std::shared_ptr<GraphModel> _graphModel;

    int _width;
    int _height;

    int _renderSizeDivisor;
    ComponentArray<int> _renderSizeDivisors;

    ComponentArray<LayoutData> _previousComponentLayout;
    ComponentArray<LayoutData> _componentLayout;
    void layoutComponents();

    std::vector<ComponentId> _removedComponentIds;
    std::vector<ComponentMergeSet> _componentMergeSets;
    void startTransition(float duration = 1.0f,
                         Transition::Type transitionType = Transition::Type::EaseInEaseOut,
                         std::function<void()> finishedFunction = []{});

    std::vector<ComponentId> _componentIds;

private slots:
    void onGraphWillChange(const Graph* graph);
    void onGraphChanged(const Graph* graph);
    void onComponentAdded(const ImmutableGraph* graph, ComponentId componentId, bool hasSplit);
    void onComponentWillBeRemoved(const ImmutableGraph* graph, ComponentId componentId, bool hasMerged);
    void onComponentSplit(const ImmutableGraph* graph, const ComponentSplitSet& componentSplitSet);
    void onComponentsWillMerge(const ImmutableGraph* graph, const ComponentMergeSet& componentMergeSet);
};

#endif // GRAPHOVERVIEWSCENE_H
