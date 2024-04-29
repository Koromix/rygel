/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>

#include "about_dialog.hpp"

AboutDialog::AboutDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);
    setWindowTitle(tr("About %1").arg(QApplication::applicationName()));

    connect(closeButton, &QPushButton::clicked, this, &AboutDialog::close);
#ifdef BUGS_URL
    connect(reportBugButton, &QPushButton::clicked, &AboutDialog::openBugReports);
#else
    reportBugButton->hide();
#endif
    connect(licenseButton, &QPushButton::clicked, &AboutDialog::openLicense);

    versionLabel->setText(QString("%1\n%2")
                          .arg(QCoreApplication::applicationName(),
                               QCoreApplication::applicationVersion()));
#ifdef WEBSITE_URL
    websiteLabel->setText(QString("<a href=\"%1\">%1</a>").arg(WEBSITE_URL));
#endif
}

void AboutDialog::openWebsite()
{
#ifdef WEBSITE_URL
    QDesktopServices::openUrl(QUrl(WEBSITE_URL));
#endif
}

void AboutDialog::openBugReports()
{
#ifdef BUGS_URL
    QDesktopServices::openUrl(QUrl(BUGS_URL));
#endif
}

void AboutDialog::openLicense()
{
    QDesktopServices::openUrl(QUrl("http://unlicense.org/"));
}
