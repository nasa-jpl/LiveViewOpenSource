#include "lvmainwindow.h"

LVMainWindow::LVMainWindow(QSettings *settings, QWidget *parent)
    : QMainWindow(parent)
{   
    // Hardcoded default window size
    this->resize(1560, 1000);

    this->settings = new QSettings(QStandardPaths::writableLocation(
                                       QStandardPaths::ConfigLocation)
                                   + "/lvconfig.ini", QSettings::IniFormat);

    QPixmap icon_pixmap(":images/icon.png");
    this->setWindowIcon(QIcon(icon_pixmap));
    this->setWindowTitle("LiveView 4.0");

    // Load the worker thread
    workerThread = new QThread;
    fw = new FrameWorker(settings, workerThread);
    fw->moveToThread(workerThread);
    QFutureWatcher<void> fwWatcher;
    // Reserve proper take object error handling for later
    connect(fw, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
    connect(workerThread, SIGNAL(started()), fw, SLOT(captureFrames()));
    connect(fw, SIGNAL(finished()), workerThread, SLOT(quit()));

    connect(workerThread, SIGNAL(finished()), workerThread, SLOT(deleteLater()));

    connect(fw, &FrameWorker::startSaving, this, [&](){
        saveAct->setEnabled(false);
        saveAsAct->setEnabled(false);
    });
    connect(fw, &FrameWorker::doneSaving, this, [&](){
        saveAct->setEnabled(true);
        saveAsAct->setEnabled(true);
    });

    if (fw->running()) {
        workerThread->start();
        DSLoop = QtConcurrent::run(fw, &FrameWorker::captureDSFrames);
        SDLoop = QtConcurrent::run(fw, &FrameWorker::captureSDFrames);
        fwWatcher.setFuture(SDLoop);
        connect(&fwWatcher, &QFutureWatcher<void>::finished, fw, &FrameWorker::deleteLater);
    } else {
        connect(fw, &FrameWorker::finished, fw, &FrameWorker::deleteLater);
    }

    QWidget* mainWidget = new QWidget(this);
    tab_widget = new QTabWidget(this);

    raw_display = new frameview_widget(fw, BASE, settings);
    dsf_display = new frameview_widget(fw, DSF, settings);
    sdv_display = new frameview_widget(fw, STD_DEV, settings);
    hst_display = new histogram_widget(fw);
    spec_display = new line_widget(fw, SPECTRAL_PROFILE);
    spec_mean_display = new line_widget(fw, SPECTRAL_MEAN);
    spat_display = new line_widget(fw, SPATIAL_PROFILE);
    spat_mean_display = new line_widget(fw, SPATIAL_MEAN);
    fft_display = new fft_widget(fw);

    // Set these two to be in the precision slider by default
    dsf_display->setPrecision(true);
    sdv_display->setPrecision(true);

    tab_widget->addTab(raw_display, QString("Live View"));
    tab_widget->addTab(dsf_display, QString("Dark Subtraction"));
    tab_widget->addTab(sdv_display, QString("Standard Deviation"));
    tab_widget->addTab(hst_display, QString("Histogram"));
    tab_widget->addTab(spec_display, QString("Spectral Profile"));
    tab_widget->addTab(spec_mean_display, QString("Spectral Mean"));
    tab_widget->addTab(spat_display, QString("Spatial Profile"));
    tab_widget->addTab(spat_mean_display, QString("Spatial Mean"));
    tab_widget->addTab(fft_display, QString("FFT of Plane Mean"));

    server = new SaveServer(this);
    connect(server, &SaveServer::startSavingRemote,
            fw, &FrameWorker::captureFramesRemote);

    /*
     * It's pretty bizarre to send the tab widget into the ControlsBox,
     * but the reference is preserved so that the ControlsBox GUI elements will
     * control the current tab in context.
     */
    cbox = new ControlsBox(fw, tab_widget,
                           server->ipAdress.toString(), server->port);
    connect(cbox->browseButton, &QPushButton::clicked, this, &LVMainWindow::saveAs);
    connect(this, &LVMainWindow::saveRequest, cbox, &ControlsBox::acceptSave);

    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->addWidget(tab_widget);
    mainLayout->addWidget(cbox);

    mainWidget->setLayout(mainLayout);
    this->setCentralWidget(mainWidget);

    createActions();
    createMenus();

    camDialog = new CameraViewDialog(fw->Camera);

    compDialog = new ComputeDevDialog(fw->STDFilter->getDeviceList());
    connect(compDialog, &ComputeDevDialog::device_changed,
            this, &LVMainWindow::change_compute_device);

    dsfDialog = new DSFPrefDialog();
    connect(dsfDialog, &DSFPrefDialog::applyMaskFromFile,
            fw, &FrameWorker::applyMask);
    connect(dsfDialog, &DSFPrefDialog::accepted, fw, [this](){
        fw->setMaskSettings(dsfDialog->getMaskFile(),
                            dsfDialog->getAvgdFrames());
    });
}

LVMainWindow::~LVMainWindow()
{
    fw->stop();
    if (DSLoop.isStarted())
        DSLoop.waitForFinished();
    if (SDLoop.isStarted())
        SDLoop.waitForFinished();
    delete cbox;
    delete raw_display;
    delete dsf_display;
    delete sdv_display;
    delete hst_display;
    delete spec_display;
    delete spec_mean_display;
    delete spat_display;
    delete spat_mean_display;
    delete fft_display;
    delete camDialog;
    delete compDialog;
    delete dsfDialog;
}

void LVMainWindow::errorString(const QString &errstr)
{
    qWarning() << errstr;
}

void LVMainWindow::createActions()
{
    openAct = new QAction("&Open", this);
    openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip("Open an existing data set");
    connect(openAct, &QAction::triggered, this, &LVMainWindow::open);

    saveAct = new QAction("&Save", this);
    saveAct->setShortcuts(QKeySequence::Save);
    saveAct->setStatusTip("Save frames to file");
    connect(saveAct, &QAction::triggered, this, &LVMainWindow::save);

    saveAsAct = new QAction("Save &As...", this);
    saveAsAct->setShortcuts(QKeySequence::SaveAs);
    saveAsAct->setStatusTip("Select a location to save frames");
    connect(saveAsAct, &QAction::triggered, this, &LVMainWindow::saveAs);

    resetAct = new QAction("&Reset", this);
    resetAct->setShortcuts(QKeySequence::Refresh);
    resetAct->setStatusTip("Restart the data stream");
    connect(resetAct, &QAction::triggered, this, &LVMainWindow::reset);

    exitAct = new QAction("E&xit", this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip("Exit LiveView");
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    compAct = new QAction("Change Compute Device...", this);
    compAct->setStatusTip("Use a different computing type for OpenCL calculations.");
    connect(compAct, &QAction::triggered, this, &LVMainWindow::show_deviceModelView);

    dsfAct = new QAction("Dark Subtraction", this);
    dsfAct->setShortcut(QKeySequence::Underline); // This specifies the Ctrl+U key combo.
    dsfAct->setStatusTip("Modify settings when collecting dark subtraction frames.");
    connect(dsfAct, &QAction::triggered, this, &LVMainWindow::show_dsfModelView);

    remapAct = new QAction("Remap Pixels", this);
    remapAct->setStatusTip("Take the two's compliment of data.");
    remapAct->setCheckable(true);
    remapAct->setChecked(false);
    connect(remapAct, &QAction::triggered, this, [this]() {
        fw->pixRemap = remapAct->isChecked();
    });

    darkModeAct = new QAction("&Dark Mode (Takes Effect on Restart)", this);
    darkModeAct->setCheckable(true);
    darkModeAct->setChecked(settings->value(QString("dark"), false).toBool());
    connect(darkModeAct, &QAction::triggered, this, [this](){
        settings->setValue(QString("dark"), darkModeAct->isChecked());
    });

    gradActs = QList<QAction*>();
    gradActGroup = new QActionGroup(this);
    QMetaEnum qme = QMetaEnum::fromType<QCPColorGradient::GradientPreset>();
    for (int i = 0; i < qme.keyCount(); ++i) {
        gradActs.append(new QAction(qme.key(i), this));
        gradActGroup->addAction(gradActs.at(i));
        gradActs.at(i)->setCheckable(true);
        connect(gradActs.at(i), &QAction::triggered, this, [this, i](){
            settings->setValue(QString("gradient"), i);
            changeGradients();
        });
    }
    gradActs.at(settings->value(QString("gradient"), 0).toInt())->setChecked(true);

    fmtActGroup = new QActionGroup(this);
    BILact = new QAction("BIL", this);
    BILact->setCheckable(true);
    connect(BILact, &QAction::triggered, this, [this]() {
        cbox->bit_org = fwBIL;
    });
    BIPact = new QAction("BIP", this);
    BIPact->setCheckable(true);
    connect(BIPact, &QAction::triggered, this, [this]() {
        cbox->bit_org = fwBIP;
    });
    BSQact = new QAction("BSQ", this);
    BSQact->setCheckable(true);
    connect(BSQact, &QAction::triggered, this, [this]() {
        cbox->bit_org = fwBSQ;
    });
    fmtActGroup->addAction(BILact);
    fmtActGroup->addAction(BIPact);
    fmtActGroup->addAction(BSQact);

    BILact->setChecked(true);
    cbox->bit_org = fwBIL;

    camViewAct = new QAction("Camera Info", this);
    connect(camViewAct, &QAction::triggered, this, &LVMainWindow::show_camModelView);
}

void LVMainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    // saveAct->setEnabled(false);
    fileMenu->addAction(saveAsAct);
    formatSubMenu = fileMenu->addMenu("Data Format");
    formatSubMenu->addAction(BILact);
    formatSubMenu->addAction(BIPact);
    formatSubMenu->addAction(BSQact);
    fileMenu->addAction(resetAct);
    // These two items will not appear in MacOS because they are handled automatically by the
    // application menu.
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    prefMenu = menuBar()->addMenu("&Computation");
    prefMenu->addAction(compAct);
    prefMenu->addAction(dsfAct);
    prefMenu->addAction(remapAct);

    viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(darkModeAct);
    gradientSubMenu = viewMenu->addMenu("&Gradient");
    gradientSubMenu->addActions(gradActs);

    aboutMenu = menuBar()->addMenu("&About");
    aboutMenu->addAction(camViewAct);

}

#ifndef QT_NO_CONTEXTMENU
void LVMainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.addAction(openAct);
    menu.addAction(saveAct);
    menu.addAction(saveAsAct);
    menu.addAction(resetAct);
    menu.addAction(exitAct);
    menu.exec(event->globalPos());

    QMenu compMenu(this);
    compMenu.addAction(compAct);
    compMenu.addAction(dsfAct);
    compMenu.addAction(remapAct);
    compMenu.exec(event->globalPos());
}
#endif // QT_NO_CONTEXTMENU

void LVMainWindow::open()
{
    default_dir = settings->value(QString("save_dir"),
                                  QStandardPaths::writableLocation(
                                      QStandardPaths::HomeLocation)).toString();

    QString temp_dir = QFileDialog::getExistingDirectory(
                this, "Open Data Directory", default_dir,
                QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!temp_dir.isEmpty()) {
        source_dir = temp_dir;
        fw->resetDir(source_dir.toLatin1().data());
    }
}

void LVMainWindow::save()
{
    // If there is no file name to save to file, open a filedialog.
    if (cbox->saveFileNameEdit->text().isEmpty()) {
        saveAs();
    } else {
        // otherwise, just send the request to save frames.
        emit saveRequest();
    }
}

void LVMainWindow::saveAs()
{
    QString save_filename = QFileDialog::getSaveFileName(
                                this, "Save Raw Frames", default_dir,
                                "Raw Camera Frames (*.raw);;Data Files (*.dat);;All files (*.*)");

    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    cbox->saveFileNameEdit->setText(save_filename);
    emit saveRequest();
}

void LVMainWindow::reset()
{
    fw->resetDir(source_dir.toLatin1().data());
}

void LVMainWindow::show_camModelView()
{
    camDialog->show();
}

void LVMainWindow::show_deviceModelView()
{
    compDialog->show();
}

void LVMainWindow::show_dsfModelView()
{
    dsfDialog->show();
}

void LVMainWindow::change_compute_device(QString dev_name)
{
    fw->STDFilter->change_device(dev_name);
}

void LVMainWindow::changeGradients()
{
    int value = settings->value(QString("gradient"), QCPColorGradient::gpJet).toInt();
    raw_display->getColorMap()->setGradient(QCPColorGradient(static_cast<QCPColorGradient::GradientPreset>(value)));
    dsf_display->getColorMap()->setGradient(QCPColorGradient(static_cast<QCPColorGradient::GradientPreset>(value)));
    sdv_display->getColorMap()->setGradient(QCPColorGradient(static_cast<QCPColorGradient::GradientPreset>(value)));
}
