#include "lvmainwindow.h"

LVMainWindow::LVMainWindow(QWidget *parent)
    : QMainWindow(parent)
{   
    // Hardcoded default window size
    this->resize(1440, 900);

    QPixmap icon_pixmap(":images/icon.png");
    this->setWindowIcon(QIcon(icon_pixmap));
    this->setWindowTitle("LiveView 4.0");

    // Load the worker thread
    QThread *workerThread = new QThread();
    fw = new FrameWorker(workerThread);
    fw->moveToThread(workerThread);
    // Reserve proper take object error handling for later
    connect(fw, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
    connect(workerThread, SIGNAL(started()), fw, SLOT(captureFrames()));
    connect(fw, SIGNAL(finished()), workerThread, SLOT(quit()));
    connect(fw, SIGNAL(finished()), fw, SLOT(deleteLater()));
    connect(workerThread, SIGNAL(finished()), workerThread, SLOT(deleteLater()));

    if (fw->running()) {
        workerThread->start();
        DSLoop = QtConcurrent::run(fw, &FrameWorker::captureDSFrames);
        SDLoop = QtConcurrent::run(fw, &FrameWorker::captureSDFrames);
    }

    QWidget* mainWidget = new QWidget();
    tab_widget = new QTabWidget();

    raw_display = new frameview_widget(fw, BASE);
    dsf_display = new frameview_widget(fw, DSF);
    sdv_display = new frameview_widget(fw, STD_DEV);
    hst_display = new histogram_widget(fw);
    spec_display = new line_widget(fw, SPECTRAL_PROFILE);
    spat_display = new line_widget(fw, SPATIAL_PROFILE);

    // Set these two to be in the precision slider by default
    dsf_display->setPrecision(true);
    sdv_display->setPrecision(true);

    tab_widget->addTab(raw_display, QString("Live View"));
    tab_widget->addTab(dsf_display, QString("Dark Subtraction"));
    tab_widget->addTab(sdv_display, QString("Standard Deviation"));
    tab_widget->addTab(hst_display, QString("Histogram"));
    tab_widget->addTab(spec_display, QString("Spectral Profile"));
    tab_widget->addTab(spat_display, QString("Spatial Profile"));

    /*
     * It's pretty bizarre to send the tab widget into the ControlsBox, but the reference is
     * preserved so that the ControlsBox GUI elements will control the current tab in context.
     */
    cbox = new ControlsBox(fw, tab_widget);

    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->addWidget(tab_widget);
    mainLayout->addWidget(cbox);

    mainWidget->setLayout(mainLayout);
    this->setCentralWidget(mainWidget);

}

LVMainWindow::~LVMainWindow()
{
    fw->stop();
    SDLoop.waitForFinished();
    DSLoop.waitForFinished();
}

void LVMainWindow::errorString(const QString &errstr)
{
    qWarning() << errstr;
}
