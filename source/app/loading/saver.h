#ifndef SAVER_H
#define SAVER_H

#include "shared/loading/progressfn.h"

#include <QUrl>
#include <QStringList>
#include <QString>
#include <QByteArray>

class Document;
class IPluginInstance;

class Saver
{
private:
    QUrl _fileUrl;

    Document* _document = nullptr;
    const IPluginInstance* _pluginInstance = nullptr;
    QByteArray _uiData;
    QByteArray _pluginUiData;

public:
    static const int MaxHeaderSize = 1 << 12;

    explicit Saver(const QUrl& fileUrl) { _fileUrl = fileUrl; }

    QUrl fileUrl() const { return _fileUrl; }

    void setDocument(Document* document) { _document = document; }
    void setPluginInstance(const IPluginInstance* pluginInstance) { _pluginInstance = pluginInstance; }
    void setUiData(const QByteArray& uiData) { _uiData = uiData; }
    void setPluginUiData(const QByteArray& pluginUiData) { _pluginUiData = pluginUiData; }

    bool encode(const ProgressFn& progressFn);
};

#endif // SAVER_H
