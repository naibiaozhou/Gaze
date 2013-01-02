
#include <iostream>
#include <algorithm>

#include <QtNetwork>
#include <QtWebKit>
#include <QtGui/qapplication.h>

#include "BrowserWindow.hpp"
#include "video/LiveSource.hpp"
#include "video/VideoSource.hpp"
#include "MessageWindow.hpp"
#include "ImageWindow.hpp"
#include "ImageLinkLabel.hpp"
#include "threads/ThreadManager.hpp"


using namespace std;

BrowserWindow::BrowserWindow(const QUrl& url) {

    init();
    view->load(url);

}

BrowserWindow::BrowserWindow() {

    init();
    showBookmarkPage();

}

void BrowserWindow::init() {

    settings = new QSettings("gazebrowser.ini", QSettings::IniFormat);

    QFile file;
    file.setFileName(":js/jquery-1.8.3.min.js");
    file.open(QIODevice::ReadOnly);
    jQuery = file.readAll();
    file.close();

    progress = 0;
    isCalibrating = false;
    
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    view = new QWebView(this);

    connect(view, SIGNAL(titleChanged(QString)), SLOT(adjustTitle()));
    connect(view, SIGNAL(loadProgress(int)), SLOT(setProgress(int)));
    connect(view, SIGNAL(loadFinished(bool)), SLOT(finishLoading(bool)));

    // add our own webpage to control the user-agent
    webpage = new GazeWebPage;
    view->setPage(webpage);

    eye_widget = new CVWidget;
    eye_widget->setWindowModality(Qt::WindowModal);
    eye_widget->setAttribute(Qt::WA_DeleteOnClose, false); //only hide the widget

    bookmarksWin = new BookmarksWindow(settings);
    bookmarksWin->setWindowModality(Qt::WindowModal);
    bookmarksWin->setAttribute(Qt::WA_DeleteOnClose, false);

    settingsWin = new SettingsWindow();
    settingsWin->setWindowModality(Qt::WindowModal);
    settingsWin->setAttribute(Qt::WA_DeleteOnClose, false);

    setupMenus();
    setCentralWidget(view);
}

void BrowserWindow::setupMenus() {
    // Add the Slot to the quit button
    // on mac this will be showed in the unified menu bar
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *quitAction = new QAction("Quit Browser", this);
    quitAction->setMenuRole(QAction::QuitRole);
    connect(quitAction, SIGNAL(triggered()), this, SLOT(quit_gazebrowser()));
    fileMenu->addAction("Bookmarks", this, SLOT(bookmarks()));
    fileMenu->addAction("Preferences", this, SLOT(preferences()));
    fileMenu->addSeparator();
    fileMenu->addAction(quitAction);


    QMenu *navMenu = menuBar()->addMenu(tr("&Navigation"));
    navMenu->addAction("Back", this, SLOT(back()));
    navMenu->addAction("Forward", this, SLOT(forward()));
    navMenu->addSeparator();
    navMenu->addAction("Scroll Up", this, SLOT(scrollUp()));
    navMenu->addAction("Scroll Down", this, SLOT(scrollDown()));
    navMenu->addSeparator();
    navMenu->addAction("Go to page", this, SLOT(goToPage()));
    navMenu->addAction("Show Bookmarks", this, SLOT(showBookmarkPage()));

    QMenu *browserMenu = menuBar()->addMenu(tr("&View"));
    // Zoom
    QMenu *zoomMenu = browserMenu->addMenu(tr("&Zoom"));
    zoomMenu->addAction("Zoom in", this, SLOT(zoomIn()));
    zoomMenu->addAction("Zoom out", this, SLOT(zoomOut()));

    QMenu *gazeMenu = menuBar()->addMenu(tr("&Gaze Actions"));
    QAction *calibrateMenuAction = new QAction("Calibration", this);
    connect(this, SIGNAL(isTracking(bool)), calibrateMenuAction, SLOT(setDisabled(bool)));
    connect(calibrateMenuAction, SIGNAL(triggered()), this, SLOT(start_calibration()));
    gazeMenu->addAction(calibrateMenuAction);
    gazeMenu->addSeparator();
    QAction *stopMenuAction = new QAction("stop tracking", this);
    stopMenuAction->setDisabled(true);
    connect(this, SIGNAL(isTracking(bool)), stopMenuAction, SLOT(setEnabled(bool)));
    connect(stopMenuAction, SIGNAL(triggered()), this, SLOT(stop_tracking()));
    //gazeMenu->addAction("Calibration", this, SLOT(start_calibration()));
    gazeMenu->addAction(stopMenuAction);
    gazeMenu->addSeparator();
    //TODO remove findLinks!
    gazeMenu->addAction("Find Links", this, SLOT(highlightAllLinks()));
    gazeMenu->addSeparator();
    gazeMenu->addAction("Show the Eye Widget", this, SLOT(show_eye_widget()));

}

/**
 * this method is called, as soon as the window is displayed. here
 * we trigger the setUpCamera(). if we do this in the constructor
 * we would block the whole UI until openCV has opened the camera.
 */
void BrowserWindow::showEvent(QShowEvent *event) {
    Q_UNUSED(event);
    QTimer::singleShot(1000, this, SLOT(setUpCamera()));
}

/**
 * opens a connection to the webcam .openCV has problems, if VideoCapture 
 * is created outside the main thread...
 */
void BrowserWindow::setUpCamera() {
    source = new LiveSource;
    //TODO document this, the UI must have been loaded before we start this
    tManager = new ThreadManager(this);
    tManager->goIdle();
}

/*
 * Methods handling the Website-Loading:
 *  - adjustTitle()
 *  - setProgress()
 *  - finishLoading()
 *  - goToPage()
 */
void BrowserWindow::adjustTitle() {
    if (progress <= 0 || progress >= 100)
        setWindowTitle(view->title());
    else
        setWindowTitle(QString("%1 (%2%)").arg(view->title()).arg(progress));
}

void BrowserWindow::setProgress(int p) {
    progress = p;
    adjustTitle();
}

void BrowserWindow::finishLoading(bool) {
    progress = 100;
    adjustTitle();
    view->page()->mainFrame()->evaluateJavaScript(jQuery);

    // TODO: cleanup code and don't check for every site
    if (view->page()->mainFrame()->url().toString() == "qrc:/" && !isCalibrating) {
        QString html = "$('#%1').append("
                "\"<a href='%2'><img src='http://gaze.frickler.ch/screenshot/?screenshot_url=%3' alt=''></a>"
                ""
                "<br>"
                "<a href='%4'>%5</a>\""
                ")";

        settings->beginGroup("BOOKMARKS");
        for (int i = 0; i < 9; i++) {
            QString desc = settings->value("DESC_" + QString::number(i)).toString();
            QString url = settings->value("URL_" + QString::number(i)).toString();

            if (url.length() > 0 && desc.length() > 0) {
                QString code = QString(html).arg(QString::number(i + 1), url, url, url, desc);

                view->page()->mainFrame()->evaluateJavaScript(code);
            }
        }
        settings->endGroup();
    }

    if (isCalibrating)
        this->calibrate();
}

void BrowserWindow::goToPage() {
    // TODO: optimize and add a better layout. Maybe create window to make QLineEdit largerF
    bool ok;
    QString text = QInputDialog::getText(this,
            tr("Go to page"),
            tr("URL:"),
            QLineEdit::Normal,
            "", &ok);
    if (ok && !text.isEmpty()) {


        view->load(QUrl::fromUserInput(text));
    }
}

/*
 * Methods for the "Gaze Actions"
 *  - highlightAllLinks()
 *  - scrollUp()
 *  - scrollDown()
 *  - forward()
 *  - back()
 *  - zoomIn() and zoomOut()
 */

void BrowserWindow::highlightAllLinks() {
    QWebElementCollection linkElements = view->page()->mainFrame()->findAllElements("a");

    QWebElement element;


    ImageWindow* win = new ImageWindow(view);
    win->setWindowModality(Qt::WindowModal);
    // TODO: take window size
    win->resize(800, 800);

    // get viewport of browser window
    QPoint vp = view->page()->mainFrame()->scrollPosition();

    for (int i = 0; i < linkElements.count(); i++) {
        element = linkElements.at(i);

        // Don't show hidden links
        if (element.geometry().width() == 0)
            continue;

        int x = element.geometry().x();
        int y = element.geometry().y();
        int w = view->size().width();
        int h = view->size().height();                

        // only draw if in viewport
        if ((vp.x() <= x && x <= (vp.x() + w)) &&
                (vp.y() <= y && y <= (vp.y() + h))) {

            cout << "X/Y: " << x << "/" << y << endl;

            QImage* image = new QImage(view->page()->viewportSize(), QImage::Format_ARGB32);
            QPainter* painter = new QPainter(image);
            // Take an image with some offset
            QRect r;
            r.setX(max(0, x - vp.x() - 30));
            r.setY(max(0, y - vp.y() - 30));
            r.setWidth(element.geometry().width() + 60);
            r.setHeight(element.geometry().height() + 60);

            view->page()->mainFrame()->render(painter, r);
            painter->end();

            // Copy image
            QImage q = image->copy(r);
            // If greater than 250, shrink
            // TODO: calculate from screen size
            if (q.width() > 250)
                q = q.scaledToWidth(250);
            
            if (q.height() > 250)
                q = q.scaledToHeight(250);

            Link l;
            l.image = q;
            l.href = element.attribute("href");

            win->addLink(l);

        }
    }
    // TODO release of memory from window
    win->show();



}

void BrowserWindow::scrollUp() {
    QString code = "$('html, body').animate({ scrollTop: $('body').scrollTop() - $(window).height() }, 800);";
    view->page()->mainFrame()->evaluateJavaScript(code);
}

void BrowserWindow::scrollDown() {
    QString code = "$('html, body').animate({ scrollTop: $('body').scrollTop() + $(window).height() }, 800);";
    view->page()->mainFrame()->evaluateJavaScript(code);
}

void BrowserWindow::forward() {
    view->forward();
}

void BrowserWindow::back() {
    view->back();
}

void BrowserWindow::zoomIn() {
    qreal zoomFactor = view->zoomFactor();
    zoomFactor++;
    view->setZoomFactor(zoomFactor);
}

void BrowserWindow::zoomOut() {
    qreal zoomFactor = view->zoomFactor();
    zoomFactor--;
    view->setZoomFactor(zoomFactor);

}

/*
 * Gaze Methods:
 *  - start_calibration() - Loads the calibration screen
 *  - calibrate() - starts the calibrationThread
 * 
 */

void BrowserWindow::start_calibration() {
    isCalibrating = true;
    QFile file;
    file.setFileName(":/calibration.html");
    file.open(QIODevice::ReadOnly);
    view->setHtml(file.readAll(), QUrl("qrc:/"));
    file.close();
}

void BrowserWindow::stop_tracking() {
    tManager->goIdle();
}

void BrowserWindow::calibrate() {
    tManager->calibrate();
}

void BrowserWindow::execJsCommand(QString command) {
    view->page()->mainFrame()->evaluateJavaScript(command);
}

/*
 * Some UI Methods 
 * 
 */

void BrowserWindow::quit_gazebrowser() {
    // todo valid?
    delete settings;

    QApplication::exit(0);
}

void BrowserWindow::alertMessage(QString message) {
    MessageWindow m;
    m.showException(message);
}

void BrowserWindow::preferences() {
    settingsWin->show();
}

void BrowserWindow::bookmarks() {
    bookmarksWin->resize(500, 300);
    bookmarksWin->show();
}

void BrowserWindow::showCvImage(cv::Mat mat) {
    eye_widget->sendImage(&mat);
}

void BrowserWindow::showBookmarkPage() {
    QFile file;
    file.setFileName(":/bookmarks.html");
    file.open(QIODevice::ReadOnly);
    view->setHtml(file.readAll(), QUrl("qrc:/"));
    file.close();
}

void BrowserWindow::show_eye_widget() {
    eye_widget->show();
}
void BrowserWindow::trackingStarted(bool tracking){
    emit isTracking(tracking);
}
