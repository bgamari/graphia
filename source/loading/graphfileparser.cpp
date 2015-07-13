#include "graphfileparser.h"

#include "../graph/graph.h"

#include "../utils/namethread.h"

GraphFileParserThread::GraphFileParserThread(Graph& graph,
                                             std::unique_ptr<GraphFileParser> graphFileParser) :
    _graph(graph),
    _graphFileParser(std::move(graphFileParser))
{}

GraphFileParserThread::~GraphFileParserThread()
{
    cancel();

    if(_thread.joinable())
        _thread.join();
}

void GraphFileParserThread::start()
{
    _thread = std::thread(&GraphFileParserThread::run, this);
}

void GraphFileParserThread::cancel()
{
    if(_thread.joinable())
        _graphFileParser->cancel();
}

void GraphFileParserThread::run()
{
    nameCurrentThread("Parser");

    connect(_graphFileParser.get(), &GraphFileParser::progress, this, &GraphFileParserThread::progress);

    bool result;

    _graph.performTransaction(
        [&](Graph& graph)
        {
            result = _graphFileParser->parse(graph);
        });

    emit complete(result);
}
