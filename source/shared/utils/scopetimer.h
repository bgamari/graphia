/* Copyright © 2013-2021 Graphia Technologies Ltd.
 *
 * This file is part of Graphia.
 *
 * Graphia is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Graphia is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Graphia.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCOPE_TIMER_H
#define SCOPE_TIMER_H

#include "shared/utils/singleton.h"

#include <map>
#include <deque>
#include <mutex>

#include <QString>
#include <QElapsedTimer>

// Include this header and insert SCOPE_TIMER into your code OR
// use SCOPE_TIMER_MULTISAMPLES(<numSamples>) OR
// manually create a ScopeTimer timer(<uniqueName>);

class ScopeTimer
{
public:
    explicit ScopeTimer(QString name, size_t numSamples = 1);
    ~ScopeTimer();

    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;
    ScopeTimer(ScopeTimer&&) = delete;
    ScopeTimer& operator=(ScopeTimer&&) = delete;

    void stop();

private:
    QString _name;
    size_t _numSamples;
    QElapsedTimer _elapsedTimer;
};

#if defined(__GNUC__) || defined(__clang__)
#define SCOPE_TIMER_FUNCTION QLatin1String(__PRETTY_FUNCTION__) /* NOLINT cppcoreguidelines-macro-usage */
#else
#define SCOPE_TIMER_FUNCTION QLatin1String(__func__)
#endif

#ifdef BUILD_SOURCE_DIR
#define SCOPE_TIMER_FILENAME QStringLiteral(__FILE__).replace( /* NOLINT cppcoreguidelines-macro-usage */ \
    QStringLiteral(BUILD_SOURCE_DIR), QString())
#else
#define SCOPE_TIMER_FILENAME QLatin1String(__FILE__)
#endif

#define SCOPE_TIMER_CONCAT2(a, b) a ## b /* NOLINT cppcoreguidelines-macro-usage */
#define SCOPE_TIMER_CONCAT(a, b) SCOPE_TIMER_CONCAT2(a, b) /* NOLINT cppcoreguidelines-macro-usage */
#define SCOPE_TIMER_INSTANCE_NAME SCOPE_TIMER_CONCAT(_scopeTimer, __COUNTER__) /* NOLINT cppcoreguidelines-macro-usage */
#define SCOPE_TIMER_FILE_LINE __FILE__ ## ":" ## __LINE__ /* NOLINT cppcoreguidelines-macro-usage */
#define SCOPE_TIMER_MULTISAMPLES(samples) /* NOLINT cppcoreguidelines-macro-usage */ \
    ScopeTimer SCOPE_TIMER_INSTANCE_NAME( \
        QStringLiteral("%1:%2 %3") \
            .arg(SCOPE_TIMER_FILENAME) \
            .arg(__LINE__) \
            .arg(SCOPE_TIMER_FUNCTION), samples);
#define SCOPE_TIMER SCOPE_TIMER_MULTISAMPLES(1) /* NOLINT cppcoreguidelines-macro-usage */

class ScopeTimerManager : public Singleton<ScopeTimerManager>
{
public:
    void submit(const QString& name, qint64 elapsed, size_t numSamples);
    void reportToQDebug() const;

private:
    mutable std::mutex _mutex;
    std::map<QString, std::deque<qint64>> _results;
};

#endif // SCOPE_TIMER_H
