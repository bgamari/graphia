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

#ifndef STATIC_BLOCK_H
#define STATIC_BLOCK_H

#include <QCoreApplication>
#include <QTimer>

#define STATIC_BLOCK_ID_CONCAT(p, l) p ## l // NOLINT cppcoreguidelines-macro-usage
#define STATIC_BLOCK_ID_EXPAND(p, l) STATIC_BLOCK_ID_CONCAT(p, l) // NOLINT cppcoreguidelines-macro-usage
#define STATIC_BLOCK_ID STATIC_BLOCK_ID_EXPAND(static_block_,  __LINE__) // NOLINT cppcoreguidelines-macro-usage

#define STATIC_BLOCK_2(f, c) /* NOLINT cppcoreguidelines-macro-usage */ \
    static void f(); \
    namespace \
    { \
    static const struct c \
    { \
        inline c() \
        { \
            if(!QCoreApplication::instance()->startingUp()) \
            { \
                /* This will only occur from a DLL, where we need to delay the \
                initialisation until later so we can guarantee it occurs \
                after any other static initialisation */ \
                QTimer::singleShot(0, [] { f(); }); \
            } \
            else \
                f(); \
        } \
    } STATIC_BLOCK_ID_CONCAT(c, _instance); \
    } \
    static void f()

#define STATIC_BLOCK_1(id) /* NOLINT cppcoreguidelines-macro-usage */ \
    STATIC_BLOCK_2( \
        STATIC_BLOCK_ID_CONCAT(id, _function), \
        STATIC_BLOCK_ID_CONCAT(id, _class) \
    )

#define static_block STATIC_BLOCK_1(STATIC_BLOCK_ID) // NOLINT cppcoreguidelines-macro-usage

#endif // STATIC_BLOCK_H
