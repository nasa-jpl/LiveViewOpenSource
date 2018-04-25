#include "frameview_widget.h"

frameview_widget::frameview_widget(image_t image_type, FrameWorker* fw, QWidget *parent) :
        LVTabApplication(parent),
        image_type(image_type)
{
    this->image_type = image_type;
    frame_handler = fw;

    switch(image_type) {
    case BASE:
        ceiling = UINT16_MAX;
        p_getFrame = &FrameWorker::getFrame;
        break;
    case DSF:
        ceiling = 100;
        p_getFrame = &FrameWorker::getDSFrame;
        break;
    case STD_DEV:
        ceiling = 100;
        p_getFrame = &FrameWorker::getDSFrame;
        break;
    default:
        ceiling = UINT16_MAX;
        p_getFrame = &FrameWorker::getFrame;
    }

    floor = 0;

    frHeight = frame_handler->getFrameHeight();
    frWidth = frame_handler->getFrameWidth();

    qcp = new QCustomPlot(this);
    qcp->setNotAntialiasedElement(QCP::aeAll);
    QSizePolicy qsp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    qsp.setHeightForWidth(true);
    qcp->setSizePolicy(qsp);
    qcp->heightForWidth(200);
    qcp->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    qcp->axisRect()->setupFullAxesBox(true);
    qcp->xAxis->setLabel("x");
    qcp->yAxis->setLabel("y");
    qcp->yAxis->setRangeReversed(true);

    colorMap = new QCPColorMap(qcp->xAxis, qcp->yAxis);
    colorMapData = NULL;

    // qcp->addPlottable(colorMap);

    colorScale = new QCPColorScale(qcp);
    qcp->plotLayout()->addElement(0, 1, colorScale);
    colorScale->setType(QCPAxis::atRight);
    colorMap->setColorScale(colorScale);
    colorMap->data()->setValueRange(QCPRange(0, frHeight-1));
    colorMap->data()->setKeyRange(QCPRange(0, frWidth-1));
    colorMap->setDataRange(QCPRange(floor, ceiling));
    colorMap->setGradient(QCPColorGradient::gpJet);
    colorMap->setInterpolate(false);
    colorMap->setAntialiased(false);

    QCPMarginGroup* marginGroup = new QCPMarginGroup(qcp);
    qcp->axisRect()->setMarginGroup(QCP::msBottom | QCP::msTop, marginGroup);
    colorScale->setMarginGroup(QCP::msBottom | QCP::msTop, marginGroup);

    qcp->rescaleAxes();
    qcp->axisRect()->setBackgroundScaled(false);

    fpsLabel = new QLabel("NaN");
    zoomXCheck = new QCheckBox("Zoom on X axis only");
    zoomYCheck = new QCheckBox("Zoom on Y axis only");
    zoomXCheck->setChecked(false);
    zoomYCheck->setChecked(false);

    layout = new QGridLayout();
    layout->addWidget(qcp, 0, 0, 8, 8);
    layout->addWidget(fpsLabel, 8, 0, 1, 2);
    layout->addWidget(zoomXCheck, 8, 2, 1, 2);
    layout->addWidget(zoomYCheck, 8 ,4, 1, 2);
    this->setLayout(layout);

    fps = 0;
    fpsclock.start();

    connect(&rendertimer, SIGNAL(timeout()), this, SLOT(handleNewFrame()));
    connect(qcp->yAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(colorMapScrolledY(QCPRange)));
    connect(qcp->xAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(colorMapScrolledX(QCPRange)));
    connect(zoomXCheck, SIGNAL(toggled(bool)), this, SLOT(setScrollX(bool)));
    connect(zoomYCheck, SIGNAL(toggled(bool)), this, SLOT(setScrollY(bool)));

    colorMapData = new QCPColorMapData(frWidth, frHeight, QCPRange(0, frWidth-1), QCPRange(0, frHeight-1));
    colorMap->setData(colorMapData);
    if (frame_handler->running()) {
        rendertimer.start(FRAME_DISPLAY_PERIOD_MSECS);
    }
}

frameview_widget::~frameview_widget()
{
    delete qcp;
}

void frameview_widget::handleNewFrame()
{
    if (!this->isHidden() && (frame_handler->running())) {

        uint16_t* image_data = (frame_handler->*p_getFrame)();
        for (int col = 0; col < frWidth; col++) {
            for (int row = 0; row < frHeight; row++ ) {
                colorMap->data()->setCell(col, row, image_data[row * frWidth + col]); // y-axis NOT reversed
            }
        }
        qcp->replot();
        count++;
    }
    if (count % 50 == 0 && count != 0) {
        fps = 50.0 / fpsclock.restart() * 1000.0;
        fps_string = QString::number(fps, 'f', 1);
        fpsLabel->setText(QString("Display: %1 fps").arg(fps_string));
    }
}

void frameview_widget::colorMapScrolledY(const QCPRange &newRange)
{
    /*! \brief Controls the behavior of zooming the plot.
     * \param newRange Mouse wheel scrolled range.
     * Color Maps must not allow the user to zoom past the dimensions of the frame.
     */
    QCPRange boundedRange = newRange;
    double lowerRangeBound = 0;
    double upperRangeBound = frHeight-1;
    if (boundedRange.size() > upperRangeBound - lowerRangeBound) {
        boundedRange = QCPRange(lowerRangeBound, upperRangeBound);
    } else {
        double oldSize = boundedRange.size();
        if (boundedRange.lower < lowerRangeBound) {
            boundedRange.lower = lowerRangeBound;
            boundedRange.upper = lowerRangeBound+oldSize;
        } if (boundedRange.upper > upperRangeBound) {
            boundedRange.lower = upperRangeBound - oldSize;
            boundedRange.upper = upperRangeBound;
        }
    }
    qcp->yAxis->setRange(boundedRange);
}
void frameview_widget::colorMapScrolledX(const QCPRange &newRange)
{
    /*! \brief Controls the behavior of zooming the plot.
     * \param newRange Mouse wheel scrolled range.
     * Color Maps must not allow the user to zoom past the dimensions of the frame.
     */
    QCPRange boundedRange = newRange;
    double lowerRangeBound = 0;
    double upperRangeBound = frWidth-1;
    if (boundedRange.size() > upperRangeBound - lowerRangeBound) {
        boundedRange = QCPRange(lowerRangeBound, upperRangeBound);
    } else {
        double oldSize = boundedRange.size();
        if (boundedRange.lower < lowerRangeBound) {
            boundedRange.lower = lowerRangeBound;
            boundedRange.upper = lowerRangeBound + oldSize;
        }
        if (boundedRange.upper > upperRangeBound) {
            boundedRange.lower = upperRangeBound - oldSize;
            boundedRange.upper = upperRangeBound;
        }
    }
    qcp->xAxis->setRange(boundedRange);
}
void frameview_widget::setScrollY(bool Xenabled) {
    scrollXenabled = !Xenabled;
    qcp->setInteraction(QCP::iRangeDrag, true);
    qcp->setInteraction(QCP::iRangeZoom, true);

    if (!scrollXenabled && scrollYenabled) {
        qcp->axisRect()->setRangeZoom(Qt::Vertical);
        qcp->axisRect()->setRangeDrag(Qt::Vertical);
    } else if (scrollXenabled && scrollYenabled) {
        qcp->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);
        qcp->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    } else if (scrollXenabled && !scrollYenabled) {
        qcp->axisRect()->setRangeZoom(Qt::Horizontal);
        qcp->axisRect()->setRangeDrag(Qt::Horizontal);
    } else {
        qcp->setInteraction(QCP::iRangeDrag, false);
        qcp->setInteraction(QCP::iRangeZoom, false);
    }
}
void frameview_widget::setScrollX(bool Yenabled) {
    scrollYenabled = !Yenabled;
    qcp->setInteraction(QCP::iRangeDrag, true);
    qcp->setInteraction(QCP::iRangeZoom, true);

    if (!scrollYenabled && scrollXenabled) {
        qcp->axisRect()->setRangeZoom(Qt::Horizontal);
        qcp->axisRect()->setRangeDrag(Qt::Horizontal);
    } else if (scrollXenabled && scrollYenabled) {
        qcp->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);
        qcp->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    } else if (!scrollXenabled && scrollYenabled) {
        qcp->axisRect()->setRangeZoom(Qt::Vertical);
        qcp->axisRect()->setRangeDrag(Qt::Vertical);
    } else {
        qcp->setInteraction(QCP::iRangeDrag, false);
        qcp->setInteraction(QCP::iRangeZoom, false);
    }
}

void frameview_widget::rescaleRange()
{
    /*! \brief Set the color scale of the display to the last used values for this widget */
    colorScale->setDataRange(QCPRange(floor, ceiling));
}
