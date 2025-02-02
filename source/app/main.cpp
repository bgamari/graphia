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

#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QtGlobal>
#include <QIcon>
#include <QMessageBox>
#include <QStyleHints>
#include <QGuiApplication>
#include <QWidget>
#include <QWindow>
#include <QScreen>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>
#include <QCommandLineParser>
#include <QProcess>
#include <QSettings>

#include <iostream>
#include <chrono>

#include <fcntl.h>
#include <cstdio>
#include <cstdlib>

#ifdef Q_OS_WINDOWS
#include <io.h>
#define S_IRUSR S_IREAD
#define S_IWUSR S_IWRITE
#endif

#include "application.h"
#include "limitconstants.h"
#include "ui/document.h"
#include "ui/graphquickitem.h"
#include "ui/visualisations/visualisationmappingplotitem.h"
#include "ui/hovermousepassthrough.h"
#include "ui/enrichmentheatmapitem.h"
#include "ui/iconitem.h"

#include "shared/utils/threadpool.h"
#include "shared/utils/preferences.h"
#include "shared/utils/qmlpreferences.h"
#include "shared/utils/qmlutils.h"
#include "shared/utils/scopetimer.h"
#include "shared/utils/macosfileopeneventfilter.h"
#include "shared/utils/modelcompleter.h"
#include "shared/utils/debugger.h"
#include "shared/utils/apppathname.h"
#include "shared/ui/visualisations/defaultgradients.h"
#include "shared/ui/visualisations/defaultpalettes.h"

#include "rendering/openglfunctions.h"
#include "rendering/graphrenderer.h"

#include "updates/changelog.h"
#include "updates/updater.h"

#include <qtsingleapplication/qtsingleapplication.h>
#include <breakpad/crashhandler.h>

#include "watchdog.h"

using namespace std::chrono_literals;

static QString resolvedExeName(const QString& baseExeName)
{
#ifdef Q_OS_LINUX
    if(qEnvironmentVariableIsSet("APPIMAGE"))
        return qgetenv("APPIMAGE");
#endif

    return baseExeName;
}

static void configureXDG()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    auto dotDesktopFileContent = QStringLiteral(
        "[Desktop Entry]\n"
        "Name=%1\n"
        "Comment=Visualise and analyse graphs\n"
        "Exec=%2 %U\n"
        "Icon=%1.svg\n"
        "Terminal=false\n"
        "Type=Application\n"
        "Encoding=UTF-8\n"
        "Categories=Application;Graphics;Science;\n"
        "StartupWMClass=%1\n"
        "MimeType=x-scheme-handler/%3\n"
        "X-KDE-Protocols=%3;\n")
        .arg(Application::name(), u::appPathName(),
        Application::nativeExtension());

    auto applicationsDirname = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    auto genericDirname = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if(applicationsDirname.isEmpty() || genericDirname.isEmpty())
    {
        std::cerr << "Could not determine XDG directories.\n";
        return;
    }

    auto dotDesktopFilename = QDir(applicationsDirname)
        .filePath(QStringLiteral("%1.desktop").arg(Application::name()));
    auto dotDesktopFile = QFile(dotDesktopFilename);

    auto iconsDirname = QDir(genericDirname).filePath(QStringLiteral("icons"));
    auto iconsDir = QDir(iconsDirname);
    auto iconFilename = iconsDir.filePath(QStringLiteral("%1.svg").arg(Application::name()));
    auto iconPermissions = QFileDevice::ReadOwner|QFileDevice::WriteOwner|
        QFileDevice::ReadGroup|QFileDevice::WriteGroup|QFileDevice::ReadOther;

    auto schemeHandlerRegistrationCommand = QStringLiteral("xdg-mime default %1.desktop x-scheme-handler/%2")
        .arg(Application::name(), Application::nativeExtension());

    auto success = ((iconsDir.exists() || iconsDir.mkpath(iconsDir.absolutePath())) &&
        dotDesktopFile.open(QIODevice::WriteOnly) && dotDesktopFile.write(dotDesktopFileContent.toUtf8()) >= 0 &&
        (QFileInfo::exists(iconFilename) ||
            (QFile::copy(QStringLiteral(":/icon/Icon.svg"), iconFilename) &&
            QFile::setPermissions(iconFilename, iconPermissions))) &&
        QProcess::startDetached(schemeHandlerRegistrationCommand)) || false;

    if(!success)
        std::cerr << "Failed to configure for XDG.\n";
#endif
}

static int stdoutFd = -1;
static int stderrFd = -1;
static QString stdoutFilename;
static QString stderrFilename;

void captureConsoleOutput()
{
    auto appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    stdoutFilename = QStringLiteral("%1/stdout.txt").arg(appDataLocation);
    stderrFilename = QStringLiteral("%1/stderr.txt").arg(appDataLocation);

    stdoutFd = open(stdoutFilename.toLocal8Bit().constData(), O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR);
    stderrFd = open(stderrFilename.toLocal8Bit().constData(), O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR);

    auto stdoutDupResult = dup2(stdoutFd, fileno(stdout));
    auto stderrDupResult = dup2(stderrFd, fileno(stderr));

    if(stdoutDupResult < 0 || stderrDupResult < 0)
        std::cerr << "Failed to redirect stdout/stderr\n";

    std::atexit([]
    {
        if(stdoutFd >= 0)
            close(stdoutFd);

        if(stderrFd >= 0)
            close(stderrFd);
    });
}

int start(int argc, char *argv[])
{
    SharedTools::QtSingleApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

    OpenGLFunctions::setDefaultFormat();

    if(qEnvironmentVariableIsSet("VOGL_CMD_LINE"))
        qDebug() << "Vogl detected; disabling shared OpenGL context (QtWebEngine will not function!)";
    else
        SharedTools::QtSingleApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    SharedTools::QtSingleApplication app(QStringLiteral(PRODUCT_NAME), argc, argv);

    Application::setAppDir(QCoreApplication::applicationDirPath());

    if(!u::isDebuggerPresent() && app.isRunning())
    {
        if(app.sendMessage(QCoreApplication::arguments().join(QStringLiteral("\n"))))
            return 0;
    }

    // Wait until the application is active before setting the focus window
    QObject::connect(&app, &SharedTools::QtSingleApplication::applicationStateChanged,
    [&app]
    {
        if(app.activationWindow() == nullptr)
            app.setActivationWindow(QApplication::focusWindow());
    });

    QCoreApplication::setOrganizationName(QStringLiteral("Graphia"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("graphia.app"));
    QCoreApplication::setApplicationName(QStringLiteral(PRODUCT_NAME));
    QCoreApplication::setApplicationVersion(QStringLiteral(VERSION));

    if(!u::isDebuggerPresent())
        captureConsoleOutput();

    QCommandLineParser commandLineParser;

    commandLineParser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    commandLineParser.addHelpOption();
    commandLineParser.addOptions(
    {
        {{"u", "dontUpdate"}, QObject::tr("Don't update now, but remind later.")}
    });

    commandLineParser.process(QCoreApplication::arguments());

    Q_INIT_RESOURCE(update_keys);

    if(!commandLineParser.isSet(QStringLiteral("dontUpdate")) && Updater::updateAvailable())
    {
        QStringList restartArguments = QCoreApplication::arguments();
        restartArguments[0] = resolvedExeName(restartArguments.at(0));

        if(Updater::showUpdatePrompt(restartArguments))
        {
            // The updater will restart the application once finished, so quit now
            return 0;
        }
    }

    QGuiApplication::styleHints()->setMousePressAndHoldInterval(500);

    QIcon mainIcon;
    mainIcon.addFile(QStringLiteral(":/icon/Icon512x512.png"));
    mainIcon.addFile(QStringLiteral(":/icon/Icon256x256.png"));
    mainIcon.addFile(QStringLiteral(":/icon/Icon128x128.png"));
    mainIcon.addFile(QStringLiteral(":/icon/Icon64x64.png"));
    mainIcon.addFile(QStringLiteral(":/icon/Icon32x32.png"));
    mainIcon.addFile(QStringLiteral(":/icon/Icon16x16.png"));
    QApplication::setWindowIcon(mainIcon);

    QIcon::setThemeName(QStringLiteral("Tango"));

    // Since Qt is responsible for managing OpenGL, we need
    // to give it a hint that we want a debug context
    if(qEnvironmentVariableIntValue("OPENGL_DEBUG") > 0)
        qputenv("QSG_OPENGL_DEBUG", "1");

    if(!OpenGLFunctions::hasOpenGLSupport())
    {
        QString vendor = OpenGLFunctions::vendor();
        vendor.replace(QStringLiteral(" "), QStringLiteral("+"));
        QString driversUrl = QStringLiteral(R"(https://www.google.com/search?q=%1+video+driver+download&btnI)").arg(vendor);

        QMessageBox messageBox(QMessageBox::Critical, QObject::tr("OpenGL support"),
            QObject::tr("The installed version of OpenGL is insufficient to run %1. "
                        R"(Please install the latest <a href="%2">video drivers</a> available from )"
                        "your vendor and try again.").arg(Application::name(), driversUrl),
            QMessageBox::Close);

        messageBox.setTextFormat(Qt::RichText);
        messageBox.exec();

        return 1;
    }

    configureXDG();

    const char* uri = Application::uri();
    const int maj = Application::majorVersion();
    const int min = Application::minorVersion();

    qmlRegisterType<Application>                     (uri, maj, min, "Application");
    qmlRegisterType<LimitConstants>                  (uri, maj, min, "LimitConstants");
    qmlRegisterType<Document>                        (uri, maj, min, "Document");
    qmlRegisterType<GraphQuickItem>                  (uri, maj, min, "Graph");
    qmlRegisterType<IconItem>                        (uri, maj, min, "NamedIcon");
    qmlRegisterType<ModelCompleter>                  (uri, maj, min, "ModelCompleter");
    qmlRegisterType<QmlPreferences>                  (uri, maj, min, "Preferences");
    qmlRegisterType<HoverMousePassthrough>           (uri, maj, min, "HoverMousePassthrough");
    qmlRegisterType<EnrichmentHeatmapItem>           (uri, maj, min, "EnrichmentHeatmap");
    qmlRegisterUncreatableType<EnrichmentTableModel> (uri, maj, min, "EnrichmentRoles",
                                                      QStringLiteral("Exposed purely for results Enumerator"));
    qmlRegisterType<VisualisationMappingPlotItem>    (uri, maj, min, "VisualisationMappingPlot");
    qmlRegisterType<ChangeLog>                       (uri, maj, min, "ChangeLog");

    qmlRegisterSingletonType<QmlUtils>               (uri, maj, min, "QmlUtils", &QmlUtils::qmlInstance);

    qRegisterMetaType<size_t>("size_t");

    ThreadPoolSingleton threadPool;
    ScopeTimerManager scopeTimerManager;

    u::definePref(QStringLiteral("visuals/defaultNodeColor"),               "#0000FF");
    u::definePref(QStringLiteral("visuals/defaultEdgeColor"),               "#FFFFFF");
    u::definePref(QStringLiteral("visuals/multiElementColor"),              "#FF0000");
    u::definePref(QStringLiteral("visuals/backgroundColor"),                "#C0C0C0");
    u::definePref(QStringLiteral("visuals/highlightColor"),                 "#FFFFFF");

    u::definePref(QStringLiteral("visuals/defaultNormalNodeSize"),          0.333);
    u::definePref(QStringLiteral("visuals/defaultNormalEdgeSize"),          0.25);

    u::definePref(QStringLiteral("visuals/showNodeText"),                   QVariant::fromValue(static_cast<int>(TextState::Selected)));
    u::definePref(QStringLiteral("visuals/showEdgeText"),                   QVariant::fromValue(static_cast<int>(TextState::Selected)));
    u::definePref(QStringLiteral("visuals/textFont"),                       SharedTools::QtSingleApplication::font().family());
    u::definePref(QStringLiteral("visuals/textSize"),                       24.0f);
    u::definePref(QStringLiteral("visuals/edgeVisualType"),                 QVariant::fromValue(static_cast<int>(EdgeVisualType::Cylinder)));
    u::definePref(QStringLiteral("visuals/textAlignment"),                  QVariant::fromValue(static_cast<int>(TextAlignment::Right)));
    u::definePref(QStringLiteral("visuals/showMultiElementIndicators"),     true);
    u::definePref(QStringLiteral("visuals/savedGradients"),                 Defaults::GRADIENT_PRESETS);
    u::definePref(QStringLiteral("visuals/defaultGradient"),                Defaults::GRADIENT);
    u::definePref(QStringLiteral("visuals/savedPalettes"),                  Defaults::PALETTE_PRESETS);
    u::definePref(QStringLiteral("visuals/defaultPalette"),                 Defaults::PALETTE);

    u::definePref(QStringLiteral("visuals/projection"),                     QVariant::fromValue(static_cast<int>(Projection::Perspective)));

    u::definePref(QStringLiteral("visuals/minimumComponentRadius"),         2.0);
    u::definePref(QStringLiteral("visuals/transitionTime"),                 1.0);

    u::definePref(QStringLiteral("visuals/disableMultisampling"),           false);

    u::definePref(QStringLiteral("misc/maxUndoLevels"),                     25);

    u::definePref(QStringLiteral("misc/showGraphMetrics"),                  false);
    u::definePref(QStringLiteral("misc/showLayoutSettings"),                false);

    u::definePref(QStringLiteral("misc/focusFoundNodes"),                   true);
    u::definePref(QStringLiteral("misc/focusFoundComponents"),              true);
    u::definePref(QStringLiteral("misc/stayInComponentMode"),               false);

    u::definePref(QStringLiteral("misc/disableHubbles"),                    false);

    u::definePref(QStringLiteral("misc/hasSeenTutorial"),                   false);

    u::definePref(QStringLiteral("misc/autoBackgroundUpdateCheck"),         true);

    u::definePref(QStringLiteral("screenshot/width"),                       1920);
    u::definePref(QStringLiteral("screenshot/height"),                      1080);
    u::definePref(QStringLiteral("screenshot/path"),
        QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString());

    u::definePref(QStringLiteral("servers/redirects"),                      "https://redirects.graphia.app");
    u::definePref(QStringLiteral("servers/updates"),                        "https://updates.graphia.app");
    u::definePref(QStringLiteral("servers/crashreports"),                   "https://crashreports.graphia.app");
    u::definePref(QStringLiteral("servers/tracking"),                       "https://tracking.graphia.app");

    u::updateOldPrefs();

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:///qml"));
    engine.load(QUrl(QStringLiteral("qrc:///qml/main.qml")));
    if(engine.rootObjects().empty())
    {
        QMessageBox::critical(nullptr, QObject::tr("Error"),
                              QObject::tr("The user interface failed to load."),
                              QMessageBox::Close);
        return 2;
    }

    auto rootObjects = engine.rootObjects();
    QObject* mainWindow = rootObjects.first();
    QObject::connect(&app, &SharedTools::QtSingleApplication::messageReceived,
    mainWindow, [mainWindow](const QString& message, QObject*)
    {
        auto arguments = message.split(QStringLiteral("\n"));
        arguments.pop_front(); // Executable

        QMetaObject::invokeMethod(mainWindow, "processArguments", Q_ARG(QVariant, arguments));
    });

    MacOsFileOpenEventFilter macOsfileOpenEventFilter;
    app.installEventFilter(&macOsfileOpenEventFilter);
    QObject::connect(&macOsfileOpenEventFilter, &MacOsFileOpenEventFilter::externalOpen,
    mainWindow, [mainWindow](const QString& argument)
    {
        QMetaObject::invokeMethod(mainWindow, "processArguments", Q_ARG(QVariant, QStringList{argument}));
    });

    int qmlExitCode = 0;
    QObject::connect(&engine, &QQmlApplicationEngine::exit,
        [&qmlExitCode](int code) { qmlExitCode = code; });

    Watchdog watchDog;

    // Poke the watch dog every now and again so that it doesn't break/crash us
    QTimer keepAliveTimer;
    QObject::connect(&keepAliveTimer, &QTimer::timeout, &watchDog, &Watchdog::reset);
    keepAliveTimer.start(1s);

#ifndef _DEBUG
    CrashHandler c(Application::resolvedExe(QStringLiteral("CrashReporter")));
    c.onCrash([mainWindow](const QString& directory)
    {
        QVariant state;

        bool success = QMetaObject::invokeMethod(mainWindow, "currentState",
            Qt::DirectConnection, Q_RETURN_ARG(QVariant, state));

        if(success)
        {
            QFile file(QDir(directory).filePath("state.txt"));
            std::cerr << "Writing " << file.fileName().toStdString() << "\n";

            file.open(QIODevice::ReadWrite);
            QTextStream stream(&file);
            stream << state.toString();
            file.close();
        }
        else
        {
            auto index = mainWindow->metaObject()->indexOfMethod("currentState()");
            std::cerr << "Failed to invoke 'currentState' (" << index << ")\n";
        }

        if(!stdoutFilename.isEmpty() && !stderrFilename.isEmpty())
        {
            close(stdoutFd);
            close(stderrFd);

            stdoutFd = stderrFd = -1;

            QFile::copy(stdoutFilename, QDir(directory).filePath("stdout.txt"));
            QFile::copy(stderrFilename, QDir(directory).filePath("stderr.txt"));
        }
    });
#endif

    auto exitCode = QCoreApplication::exec();
    return qmlExitCode != 0 ? qmlExitCode : exitCode;
}

int main(int argc, char *argv[])
{
    u::setAppPathName(argv[0]);

    // The "real" main is separate to limit the scope of QtSingleApplication,
    // otherwise a restart causes the exiting instance to get activated
    auto exitCode = start(argc, argv);

    if(static_cast<ExitType>(exitCode) == ExitType::Restart)
    {
        auto exeName = resolvedExeName(argv[0]);

        if(Updater::updateAvailable() && Updater::showUpdatePrompt({exeName}))
        {
            // If there is an update available, save a bit of time by
            // skipping the restart and starting the updater directly
            std::cerr << "Restarting to install update...\n";
        }
        else
        {
            std::cerr << "Restarting " << exeName.toStdString() << "...\n";
            if(!QProcess::startDetached(exeName, {}))
                std::cerr << "  ...failed\n";
        }
    }

    return exitCode;
}
