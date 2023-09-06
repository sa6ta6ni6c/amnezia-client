#include "systemController.h"

#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QQuickItem>
#include <QStandardPaths>
#include <QUrl>

#ifdef Q_OS_ANDROID
    #include "platforms/android/android_controller.h"
#endif

#ifdef Q_OS_IOS
    #include "platforms/ios/MobileUtils.h"
    #include <CoreFoundation/CoreFoundation.h>
#endif

SystemController::SystemController(const std::shared_ptr<Settings> &settings, QObject *parent)
    : QObject(parent), m_settings(settings)
{
}

void SystemController::saveFile(QString fileName, const QString &data)
{
#if defined Q_OS_ANDROID
    AndroidController::instance()->shareConfig(data, fileName);
    return;
#endif

#ifdef Q_OS_IOS
    QUrl fileUrl = QDir::tempPath() + "/" + fileName;
    QFile file(fileUrl.toString());
#else
    QUrl fileUrl = QUrl(fileName);
    QFile file(fileUrl.toLocalFile());
#endif

    // todo check if save successful
    file.open(QIODevice::WriteOnly);
    file.write(data.toUtf8());
    file.close();

#ifdef Q_OS_IOS
    QStringList filesToSend;
    filesToSend.append(fileUrl.toString());
    MobileUtils::shareText(filesToSend);
    return;
#else
    QFileInfo fi(fileUrl.toLocalFile());
    QDesktopServices::openUrl(fi.absoluteDir().absolutePath());
#endif
}

QString SystemController::getFileName()
{
    auto mainFileDialog = m_qmlRoot->findChild<QObject>("mainFileDialog").parent();
    if (!mainFileDialog) {
        return "";
    }
    QMetaObject::invokeMethod(mainFileDialog, "open", Qt::DirectConnection);

    QEventLoop wait;
    QObject::connect(this, &SystemController::fileDialogAccepted, &wait, &QEventLoop::quit);
    wait.exec();

    auto fileName = mainFileDialog->property("selectedFile").toString();

#ifdef Q_OS_IOS
    CFURLRef url = CFURLCreateWithFileSystemPath(
            kCFAllocatorDefault,
            CFStringCreateWithCharacters(0, reinterpret_cast<const UniChar *>(fileName.unicode()), fileName.length()),
            kCFURLPOSIXPathStyle, 0);

    if (!CFURLStartAccessingSecurityScopedResource(url)) {
        qDebug() << "Could not access path " << QUrl::fromLocalFile(fileName).toString();
    }

    return fileName;
#endif

#ifdef Q_OS_ANDROID
    // patch for files containing spaces etc
    const QString sep { "raw%3A%2F" };
    if (fileName.startsWith("content://") && fileName.contains(sep)) {
        QString contentUrl = fileName.split(sep).at(0);
        QString rawUrl = fileName.split(sep).at(1);
        rawUrl.replace(" ", "%20");
        fileName = contentUrl + sep + rawUrl;
    }

    return fileName;
#endif

    return QUrl(fileName).toLocalFile();
}

void SystemController::setQmlRoot(QObject *qmlRoot)
{
    m_qmlRoot = qmlRoot;
}
