#ifndef LVMAINWINDOW_H
#define LVMAINWINDOW_H

#include <QMainWindow>
#include <QMenu>
#include <QThread>
#include <QVBoxLayout>
#include <QDebug>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QSettings>

#include "image_type.h"
#include "frameview_widget.h"
#include "histogram_widget.h"
#include "line_widget.h"
#include "fft_widget.h"
#include "controlsbox.h"
#include "saveserver.h"
#include "cameraviewdialog.h"
#include "computedevdialog.h"
#include "dsfprefdialog.h"

class LVMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    LVMainWindow(QSettings *settings, QWidget *parent = nullptr);
    ~LVMainWindow() override;

public slots:
    void errorString(const QString &);

signals:
    void saveRequest();

protected:
#ifndef QT_NO_CONTEXTMENU
    void contextMenuEvent(QContextMenuEvent *event) override;
#endif // QT_NO_CONTEXTMENU

private:
    QThread *workerThread;
    void createActions();
    void createMenus();
    void changeGradients();

    QMenu *fileMenu;
    QMenu *prefMenu;
    QMenu *viewMenu;
    QMenu *gradientSubMenu;
    QMenu *formatSubMenu;
    QMenu *aboutMenu;
    QAction *openAct;
    QAction *saveAct;
    QAction *saveAsAct;
    QAction *resetAct;
    QAction *exitAct;
    QAction *compAct;
    QAction *dsfAct;
    QAction *remapAct;

    QAction *darkModeAct;
    QActionGroup *gradActGroup;
    QList<QAction*> gradActs;

    QActionGroup *fmtActGroup;
    QAction *BILact;
    QAction *BIPact;
    QAction *BSQact;

    QAction *camViewAct;

    FrameWorker *fw;
    QFuture<void> DSLoop;
    QFuture<void> SDLoop;
    QTabWidget *tab_widget;
    frameview_widget *raw_display;
    frameview_widget *dsf_display;
    frameview_widget *sdv_display;
    histogram_widget *hst_display;
    line_widget *spec_display;
    line_widget *spec_mean_display;
    line_widget *spat_display;
    line_widget *spat_mean_display;
    fft_widget *fft_display;
    ControlsBox *cbox;

    SaveServer *server;

    ComputeDevDialog *compDialog;
    DSFPrefDialog *dsfDialog;
    CameraViewDialog *camDialog;

    QString default_dir;
    QString source_dir;
    QSettings *settings;

private slots:
    void open();
    void save();
    void saveAs();
    void reset();
    void show_deviceModelView();
    void show_dsfModelView();
    void show_camModelView();
    void change_compute_device(QString dev_name);
};

#endif // LVMAINWINDOW_H
