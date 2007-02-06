/*=========================================================================

   Program: ParaView
   Module:    pqPlotViewModule.cxx

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.1. 

   See License_v1.1.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/
#include "pqPlotViewModule.h"

#include "vtkDataSet.h"
#include "vtkImageData.h"
#include "vtkSMAbstractViewModuleProxy.h"
#include "vtkSMGenericViewDisplayProxy.h"
#include "vtkSMProxy.h"

#include <QPixmap>
#include <QImage>
#include <QPointer>
#include <QtDebug>

#include "pqDisplay.h"
#include "pqHistogramChart.h"
#include "pqHistogramWidget.h"
#include "pqLineChart.h"
#include "pqLineChartModel.h"
#include "pqLineChartWidget.h"
#include "pqPipelineSource.h"
#include "pqVTKHistogramModel.h"
#include "pqVTKLineChartModel.h"

//-----------------------------------------------------------------------------
class pqPlotViewModuleInternal
{
public:
  QPointer<QWidget> PlotWidget;
  QPointer<QObject> VTKModel;
  int MaxNumberOfVisibleDisplays;
  pqPlotViewModuleInternal()
    {
    this->MaxNumberOfVisibleDisplays = -1;
    }
  ~pqPlotViewModuleInternal()
    {
    delete this->VTKModel;
    delete this->PlotWidget;
    }
};

//-----------------------------------------------------------------------------
pqPlotViewModule::pqPlotViewModule(ViewModuleTypes type,
  const QString& group, const QString& name, 
  vtkSMAbstractViewModuleProxy* renModule, pqServer* server, QObject* _parent)
: pqGenericViewModule(type, group, name, renModule, server, _parent)
{
  this->Internal = new pqPlotViewModuleInternal();

  switch (this->getViewType())
    {
  case BAR_CHART:
      {
      pqHistogramWidget* widget = new pqHistogramWidget();
      pqVTKHistogramModel* model = new pqVTKHistogramModel(this);
      widget->getHistogram().setModel(model);
      this->Internal->PlotWidget = widget;
      this->Internal->VTKModel = model;
      this->Internal->MaxNumberOfVisibleDisplays = 1;
      }
    break;

  case XY_PLOT:
      {
      pqLineChartWidget* widget = new pqLineChartWidget();
      pqVTKLineChartModel* model = new pqVTKLineChartModel(this);
      widget->getLineChart().setModel(model);
      this->Internal->PlotWidget = widget; 
      this->Internal->VTKModel = model;
      this->Internal->MaxNumberOfVisibleDisplays = -1;
      }
    break;

  default:
    qDebug() << "PlotType: " << type << " not supported yet.";
    }
  QObject::connect(this, SIGNAL(displayVisibilityChanged(pqDisplay*, bool)),
    this, SLOT(visibilityChanged(pqDisplay*)));
  QObject::connect(this, SIGNAL(displayAdded(pqDisplay*)),
    this, SLOT(visibilityChanged(pqDisplay*)));

  QObject::connect(this, SIGNAL(endRender()), this, SLOT(renderInternal()));
}

//-----------------------------------------------------------------------------
pqPlotViewModule::~pqPlotViewModule()
{
  delete this->Internal;
}

//-----------------------------------------------------------------------------
QWidget* pqPlotViewModule::getWidget()
{
  return this->Internal->PlotWidget;
}

//-----------------------------------------------------------------------------
void pqPlotViewModule::setWindowParent(QWidget* p)
{
  if (this->Internal->PlotWidget)
    {
    this->Internal->PlotWidget->setParent(p);
    }
  else
    {
    qDebug() << "setWindowParent() failed since PlotWidget not yet created.";
    }
}
//-----------------------------------------------------------------------------
QWidget* pqPlotViewModule::getWindowParent() const
{
  if (this->Internal->PlotWidget)
    {
    return this->Internal->PlotWidget->parentWidget();
    }
  qDebug() << "getWindowParent() failed since PlotWidget not yet created.";
  return 0;
}

//-----------------------------------------------------------------------------
void pqPlotViewModule::visibilityChanged(pqDisplay* disp)
{
  if (disp->isVisible())
    {
    int max_visible = this->Internal->MaxNumberOfVisibleDisplays-1;
    int cc=0;
    QList<pqDisplay*> dislays = this->getDisplays();
    foreach(pqDisplay* d, dislays)
      {
      if (d != disp && d->isVisible())
        {
        cc++;
        if (cc > max_visible)
          {
          d->setVisible(false);
          }
        }
      }
    }
}

//-----------------------------------------------------------------------------
void pqPlotViewModule::forceRender()
{
  this->Superclass::forceRender();
}

//-----------------------------------------------------------------------------
void pqPlotViewModule::renderInternal()
{
  // Now update the GUI.
  switch (this->getViewType())
    {
  case BAR_CHART:
    this->renderBarChart();
    break;

  case XY_PLOT:
    this->renderXYPlot();
    break;

  default:
    qDebug() << "Incorrect plot type: " << this->getViewType();
    }
}

//-----------------------------------------------------------------------------
void pqPlotViewModule::renderXYPlot()
{
  pqVTKLineChartModel* model = 
    qobject_cast<pqVTKLineChartModel*>(this->Internal->VTKModel);
  if (!model)
    {
    qDebug() << "Cannot locate pqVTKLineChartModel.";
    return;
    }

  QList<pqDisplay*> displays = this->getDisplays();
  QList<pqDisplay*> visibleDisplays;
  foreach (pqDisplay* display, displays)
    {
    if (display->isVisible())
      {
      visibleDisplays.push_back(display);
      }
    }
  model->update(visibleDisplays);
}

//-----------------------------------------------------------------------------
void pqPlotViewModule::renderBarChart()
{
  pqVTKHistogramModel* model = 
    qobject_cast<pqVTKHistogramModel*>(this->Internal->VTKModel);
  if (!model)
    {
    qDebug() << "Cannot locate pqVTKHistogramModel.";
    return;
    }

  QList<pqDisplay*> displays = this->getDisplays();
  foreach (pqDisplay* display, displays)
    {
    if (display->isVisible())
      {
      vtkSMGenericViewDisplayProxy* disp = 
        vtkSMGenericViewDisplayProxy::SafeDownCast(display->getProxy());
      model->updateData(disp->GetOutput());
      return;
      }
    }
  model->updateData((vtkDataObject*)0);
}


//-----------------------------------------------------------------------------
vtkImageData* pqPlotViewModule::captureImage(int magnification)
{
  QPixmap grabbedPixMap = QPixmap::grabWidget(this->getWidget());
  grabbedPixMap = grabbedPixMap.scaled(grabbedPixMap.size().width()*magnification,
    grabbedPixMap.size().height()*magnification);

  // Now we need to convert this pixmap to vtkImageData.
  QImage image = grabbedPixMap.toImage();

  vtkImageData* vtkimage = vtkImageData::New();
  vtkimage->SetScalarTypeToUnsignedChar();
  vtkimage->SetNumberOfScalarComponents(3);
  vtkimage->SetDimensions(image.size().width(), image.size().height(), 1);
  vtkimage->AllocateScalars();

  QSize imgSize = image.size();

  unsigned char* data = static_cast<unsigned char*>(vtkimage->GetScalarPointer());
  for (int y=0; y < imgSize.height(); y++)
    {
    int index=(imgSize.height()-y-1) * imgSize.width()*3;
    for (int x=0; x< imgSize.width(); x++)
      {
      QRgb color = image.pixel(x, y);
      data[index++] = qRed(color);
      data[index++] = qGreen(color);
      data[index++] = qBlue(color);
      }
    }

  // Update image extents based on window position.
  int *position = this->getViewModuleProxy()->GetWindowPosition();
  int extents[6];
  vtkimage->GetExtent(extents);
  for (int cc=0; cc < 4; cc++)
    {
    extents[cc] += position[cc/2]*magnification;
    }
  vtkimage->SetExtent(extents);

  return vtkimage;
}

//-----------------------------------------------------------------------------
bool pqPlotViewModule::saveImage(int width, int height, 
    const QString& filename)
{
  if (width != 0 && height != 0)
    {
    this->getWidget()->resize(width, height);
    }

  QPixmap grabbedPixMap = QPixmap::grabWidget(this->getWidget());
  return grabbedPixMap.save(filename);
}
