/***************************************************************************
  qgis2google.cpp
  Quickly send mouse click location to Google Maps or selected object itself to Google Earth.
  -------------------
         begin                : [PluginDate]
         copyright            : [(C) Your Name and Date]
         email                : [Your Email]

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
/*  $Id: plugin.cpp 9327 2008-09-14 11:18:44Z jef $ */

//
// QGIS Specific includes
//

#include <qgisinterface.h>
#include <qgisgui.h>
#include <qgsapplication.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgsrenderer.h>
#include <qgssymbol.h>
#include <qgsvectorlayer.h>

#include "qgis2google.h"

//
// Qt4 Related Includes
//

#include <QAction>
#include <QDockWidget>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QStandardItem>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include "qgsgoogleearthtool.h"
#include "qgskmlsettingsdialog.h"


static const char * const sIdent = "$Id: plugin.cpp 9327 2008-09-14 11:18:44Z jef $";
static const QString sName = QObject::tr( "qgis2google" );
static const QString sDescription = QObject::tr( "Quickly send selected objects or layer to Google Earth." );
static const QString sPluginVersion = QObject::tr( "Version 1.0" );
static const QgisPlugin::PLUGINTYPE sPluginType = QgisPlugin::UI;

//////////////////////////////////////////////////////////////////////
//
// THE FOLLOWING METHODS ARE MANDATORY FOR ALL PLUGINS
//
//////////////////////////////////////////////////////////////////////

/**
 * Constructor for the plugin. The plugin is passed a pointer
 * an interface object that provides access to exposed functions in QGIS.
 * @param theQGisInterface - Pointer to the QGIS interface object
 */
qgis2google::qgis2google( QgisInterface  *theQgisInterface ):
    QgisPlugin( sName, sDescription, sPluginVersion, sPluginType ),
    mQGisIface( theQgisInterface )
{
}

qgis2google::~qgis2google()
{
}

/*
 * Initialize the GUI interface for the plugin - this is only called once when the plugin is
 * added to the plugin registry in the QGIS application.
 */
void qgis2google::initGui()
{
  mToolsToolBar = mQGisIface->addToolBar( sName + " toolbar" );

  mSendToEarthTool = new QgsGoogleEarthTool( mQGisIface->mapCanvas() );

  mSendToEarthAction = mToolsToolBar->addAction( QIcon( ":/qgis2google/feature_to_google_earth.png" ), tr( "Send feature to Google Earth" ) );
  mSendToEarthAction->setCheckable( true );
  connect( mSendToEarthAction, SIGNAL( triggered() ), SLOT( setToolToEarth() ) );

  mLayerToEarthAction = new QAction( QIcon( ":/qgis2google/layer_to_google_earth.png"), tr( "Send layer to Google Earth" ), this );
  connect( mLayerToEarthAction, SIGNAL( triggered() ), mSendToEarthTool, SLOT( exportLayerToKml() ) );
  mQGisIface->addPluginToMenu( sName, mLayerToEarthAction );
  mToolsToolBar->addAction( mLayerToEarthAction );

  mSettingsAction = new QAction( QIcon( ":/qgis2google/settings.png" ), tr( "Settings" ), this );
  connect( mSettingsAction, SIGNAL( triggered() ), SLOT( settings() ) );
  mQGisIface->addPluginToMenu( sName, mSettingsAction );
  mToolsToolBar->addAction( mSettingsAction );

  mInfoAction = new QAction( tr( "About" ), this );
  connect( mInfoAction, SIGNAL( triggered() ), SLOT( about() ) );
  mQGisIface->addPluginToMenu( sName, mInfoAction );

  setDefaultSettings( mQGisIface->activeLayer() );

  connect( mQGisIface, SIGNAL(currentLayerChanged(QgsMapLayer*)), SLOT(setDefaultSettings(QgsMapLayer*)) );
}

//method defined in interface
void qgis2google::help()
{
  //implement me!
}

// Unload the plugin by cleaning up the GUI
void qgis2google::unload()
{
  disconnect( mSendToEarthAction, SIGNAL( triggered() ), this, SLOT( setToolToEarth() ) );
  disconnect( mLayerToEarthAction, SIGNAL( triggered() ), mSendToEarthTool, SLOT( exportLayerToKml() ) );
  disconnect( mSettingsAction, SIGNAL( triggered() ), this, SLOT( settings() ) );
  disconnect( mQGisIface, SIGNAL(currentLayerChanged(QgsMapLayer*)), this, SLOT(setDefaultSettings(QgsMapLayer*)) );
  disconnect( mInfoAction, SIGNAL( triggered() ), this, SLOT( about() ) );

  mQGisIface->removePluginMenu( sName, mLayerToEarthAction );
  mQGisIface->removePluginMenu( sName, mSettingsAction );
  mQGisIface->removePluginMenu( sName, mInfoAction );
  mToolsToolBar->removeAction( mSettingsAction );
  mToolsToolBar->removeAction( mLayerToEarthAction );

  delete mSendToEarthTool;
  delete mToolsToolBar;

  delete mLayerToEarthAction;
  delete mSettingsAction;
  delete mInfoAction;
}

void qgis2google::setToolToEarth()
{
  mQGisIface->mapCanvas()->setMapTool( mSendToEarthTool );
}

void qgis2google::settings()
{
//  setDefaultSettings( mQGisIface->activeLayer() );
  QgsKmlSettingsDialog settingsDialog;
  settingsDialog.exec();
}

void qgis2google::setDefaultSettings( QgsMapLayer *layer )
{
  QgsApplication::setOrganizationName( "gis-lab" );
  QgsApplication::setOrganizationDomain( "gis-lab.info" );
  QgsApplication::setApplicationName( "qgis2google" );

  QSettings settings;

  bool settingsForAllLayers = settings.value( "/qgis2google/settingsforalllayers", 0 ).toBool();
  QgsVectorLayer *vlayer = dynamic_cast<QgsVectorLayer *>(layer);
  if ( vlayer && !settingsForAllLayers )
  {
    QList<QgsSymbol *> symbols = vlayer->renderer()->symbols();
    if ( symbols.size() == 1 )
    {
      QgsSymbol *symbol = symbols.first();
      if ( !symbol )
        return;

      settings.setValue( "/qgis2google/singlevalue", 1 );

      int iOpacity, iOpacityPers;
      QColor color;

      color = symbol->color();
      iOpacity = vlayer->getTransparency();
      iOpacityPers = iOpacity * 100 / 255;

      color.setAlpha( iOpacity );
      settings.setValue( "/qgis2google/label/color", color );
      settings.setValue( "/qgis2google/label/opacity", iOpacityPers );
      settings.setValue( "/qgis2google/label/colormode", "normal" );
      settings.setValue( "/qgis2google/label/scale", 1.0 );

      color = symbol->color();
      color.setAlpha( iOpacity );
      settings.setValue( "/qgis2google/line/color", color );
      settings.setValue( "/qgis2google/line/opacity", iOpacityPers );
      settings.setValue( "/qgis2google/line/colormode", "normal" );
      settings.setValue( "/qgis2google/line/width", symbol->lineWidth() );

      color = symbol->fillColor();
      color.setAlpha( iOpacity );
      settings.setValue( "/qgis2google/poly/color", color );
      settings.setValue( "/qgis2google/poly/opacity", iOpacityPers );
      settings.setValue( "/qgis2google/poly/colormode", "normal" );
      int bPolyStyle = symbol->brush().style() != Qt::NoBrush;
      settings.setValue( "/qgis2google/poly/fill", bPolyStyle );
      bPolyStyle = symbol->pen().style() != Qt::NoPen;
      settings.setValue( "/qgis2google/poly/outline", bPolyStyle );
    }
    else
    {
      settings.setValue( "/qgis2google/singlevalue", 0 );
    }

    settings.setValue( "/qgis2google/point/extrude", 0 );
    settings.setValue( "/qgis2google/point/altitudemode", "clampToGround" );
    settings.setValue( "/qgis2google/point/altitudevalue", -1 );

    settings.setValue( "/qgis2google/line/extrude", 0 );
    settings.setValue( "/qgis2google/line/tessellate", 0 );
    settings.setValue( "/qgis2google/line/altitudemode", "clampToGround" );
    settings.setValue( "/qgis2google/line/altitudevalue", -1 );

    settings.setValue( "/qgis2google/poly/extrude", 0 );
    settings.setValue( "/qgis2google/poly/tessellate", 0 );
    settings.setValue( "/qgis2google/poly/altitudemode", "clampToGround" );
    settings.setValue( "/qgis2google/poly/altitudevalue", -1 );
  }
}

void qgis2google::about()
{
  QDialog dlg( mQGisIface->mainWindow() );
  dlg.setWindowFlags( dlg.windowFlags() | Qt::MSWindowsFixedSizeDialogHint );
  dlg.setWindowFlags( dlg.windowFlags() &~ Qt::WindowContextHelpButtonHint );
  QVBoxLayout *lines = new QVBoxLayout( &dlg );
  lines->addWidget( new QLabel( "<b>" + sPluginVersion + "</b>" ) );
  lines->addWidget( new QLabel( tr( "<b>Developers:</b>" ) ) );
  lines->addWidget( new QLabel( "    Jack iRox" ) );
  lines->addWidget( new QLabel( "    sploid (sploid@mail.ru)" ) );
  lines->addWidget( new QLabel( "    Maxim Dubinin (sim@gis-lab.info)" ) );
  lines->addWidget( new QLabel( tr( "<b>Link:</b>" ) ) );
  QLabel *link = new QLabel( "<a href=\"http://gis-lab.info\">http://gis-lab.info</a>" );
  link->setOpenExternalLinks( true );
  lines->addWidget( link );

  dlg.exec();
}


//////////////////////////////////////////////////////////////////////////
//
//
//  THE FOLLOWING CODE IS AUTOGENERATED BY THE PLUGIN BUILDER SCRIPT
//    YOU WOULD NORMALLY NOT NEED TO MODIFY THIS, AND YOUR PLUGIN
//      MAY NOT WORK PROPERLY IF YOU MODIFY THIS INCORRECTLY
//
//
//////////////////////////////////////////////////////////////////////////


/**
 * Required extern functions needed  for every plugin
 * These functions can be called prior to creating an instance
 * of the plugin class
 */
// Class factory to return a new instance of the plugin class
QGISEXTERN QgisPlugin * classFactory( QgisInterface * theQgisInterfacePointer )
{
  return new qgis2google( theQgisInterfacePointer );
}
// Return the name of the plugin - note that we do not user class members as
// the class may not yet be insantiated when this method is called.
QGISEXTERN QString name()
{
  return sName;
}

// Return the description
QGISEXTERN QString description()
{
  return sDescription;
}

// Return the type (either UI or MapLayer plugin)
QGISEXTERN int type()
{
  return sPluginType;
}

// Return the version number for the plugin
QGISEXTERN QString version()
{
  return sPluginVersion;
}

// Delete ourself
QGISEXTERN void unload( QgisPlugin * thePluginPointer )
{
  delete thePluginPointer;
}
