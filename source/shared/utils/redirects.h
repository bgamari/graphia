#ifndef REDIRECTS_H
#define REDIRECTS_H

#include "shared/utils/preferences.h"

#include <QString>

namespace u
{
static QString redirectLink(const QString& shortName, QString linkText = {})
{
    auto baseUrl = u::pref("servers/redirects").toString();

    if(linkText.isEmpty())
    {
        linkText = shortName;
        linkText[0] = linkText[0].toUpper();
    }

    return QString(R"(<a href="%1/%2">%3</a>)").arg(baseUrl, shortName, linkText);
}
}

#endif // REDIRECTS_H
