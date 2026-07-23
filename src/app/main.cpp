// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "app/MainWindow.h"

#include "backends/Signer.h"
#include "cli/Cli.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTimer>
#include <QTranslator>

namespace {
void setAppIdentity() {
    QCoreApplication::setOrganizationName(QStringLiteral("Feather PDF"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("github.com/s4rt4"));
    QCoreApplication::setApplicationName(QStringLiteral("Feather PDF"));
    QCoreApplication::setApplicationVersion(QStringLiteral(FEATHERPDF_VERSION));
}
} // namespace

int main(int argc, char** argv) {
    // Headless command-line mode: when the first argument names a sub-command
    // (merge, split, …) or a help/version flag, run the CLI with no window. Use
    // the off-screen platform so it works over SSH and on machines with no
    // display (operations only rasterize to memory, never to a screen).
    const QString firstArg = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    if (Cli::isCommand(firstArg)) {
        if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "offscreen");
        QGuiApplication cliApp(argc, argv);
        setAppIdentity();
        return Cli::run(QCoreApplication::arguments());
    }

    QApplication app(argc, argv);
    setAppIdentity();
    // Lets the GNOME shell / Wayland associate the window with its .desktop file.
    QGuiApplication::setDesktopFileName(QStringLiteral(FEATHERPDF_APP_ID));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/feather-logo.svg")));

    // Language: "system" follows the OS locale, otherwise an explicit "id"/"en"
    // (set in Preferences). Install our catalog plus Qt's own strings (standard
    // dialog buttons) so the whole UI switches. Must be installed before any
    // widget is built so its tr() calls pick up the translation.
    const QString lang =
        QSettings().value(QStringLiteral("app/language"), QStringLiteral("system")).toString();
    const QLocale locale = lang == QLatin1String("system") ? QLocale::system() : QLocale(lang);
    QTranslator appTranslator;
    if (appTranslator.load(locale, QStringLiteral("feather"), QStringLiteral("_"),
                           QStringLiteral(":/i18n")))
        app.installTranslator(&appTranslator);
    QTranslator qtTranslator;
    if (qtTranslator.load(locale, QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);

    // The design system: tokens + global style sheet, following the system
    // light/dark preference (ui-guidelines §2).
    Theme::instance().apply();

    // Point the signing stack at the shared NSS store, so signing certificates
    // and any registered PKCS#11 security devices resolve from one place. Must
    // happen before the first signature operation.
    Signer::useNssDatabase(QDir::homePath() + QStringLiteral("/.pki/nssdb"));

    MainWindow window;
    window.show();

    // Open the document AFTER the window is shown and the event loop is running.
    // Loading is synchronous (QtPdf parses + QPdfView lays out every page on the
    // UI thread); doing it before exec() leaves the process unresponsive to
    // Wayland before any window maps, which stalls the whole compositor on huge
    // documents. Queuing it lets the shell paint first, then go busy. The real
    // fix - off-thread render - lands with the custom render pipeline.
    if (argc > 1) {
        const QString path = QString::fromLocal8Bit(argv[1]);
        QTimer::singleShot(0, &window, [&window, path] { window.openPath(path); });
    }

    return app.exec();
}
