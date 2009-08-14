#include <QDesktopServices>
#include <QDir>
#include <QMessageBox>
#include <QMouseEvent>
#include <QRubberBand>
#include <QUrl>

#include <qgisinterface.h>
#include <qgsapplication.h>
#include <qgsfield.h>
#include <qgsgeometry.h>
#include <qgslogger.h>
#include <qgsmapcanvas.h>
#include <qgsmaptopixel.h>
#include <qgsvectorlayer.h>

#include "qgsgoogleearthtool.h"

QgsGoogleEarthTool::QgsGoogleEarthTool( QgsMapCanvas *canvas )
    : QgsMapTool( canvas ), mDragging( false ), mRubberBand( NULL )
{
  mCursor = QCursor( Qt::PointingHandCursor );
}

QgsGoogleEarthTool::~QgsGoogleEarthTool()
{
  while ( mTempKmlFiles.count() )
  {
    QFile *file = mTempKmlFiles.takeFirst();
    file->remove();
    delete file;
  }
}

void QgsGoogleEarthTool::canvasMoveEvent( QMouseEvent *e )
{
  if ( !( e->buttons() & Qt::LeftButton ) )
    return;

  if ( !mDragging )
  {
    mDragging = true;
    mRubberBand = new QRubberBand( QRubberBand::Rectangle, mCanvas );
    mGERect.setTopLeft( e->pos() );
  }

  mGERect.setBottomRight( e->pos() );
  mRubberBand->setGeometry( mGERect.normalized() );
  mRubberBand->show();
}

void QgsGoogleEarthTool::canvasPressEvent( QMouseEvent *e )
{
  if ( e->button() != Qt::LeftButton )
    return;

  mGERect.setRect( 0, 0, 0, 0 );
}

void QgsGoogleEarthTool::canvasReleaseEvent( QMouseEvent *e )
{
  if ( e->button() != Qt::LeftButton )
    return;

  QgsVectorLayer *vlayer = dynamic_cast<QgsVectorLayer*>( mCanvas->currentLayer() );
  if ( vlayer )
  {
    QgsFeatureList featureList;
    if ( mDragging )
    {
      mDragging = false;
      delete mRubberBand;
      mRubberBand = NULL;

      // store the rectangle
      mGERect.setRight( e->pos().x() );
      mGERect.setBottom( e->pos().y() );

      featureList = selectedFeatures( vlayer, mGERect );
    }
    else
    {
      featureList = selectOneFeature( vlayer, e->pos() );
    }

    QFile *tempFile = getTempFile();
    if ( tempFile )
    {
      if ( exportToKml( vlayer, featureList, tempFile ) )
      {
        QDesktopServices::openUrl( QUrl( "file:///" + tempFile->fileName() ) );
      }
      else
      {
        QgsLogger::debug( tr( "Unable to export the layer in kml file" ) );
      }
    }
  }
  else
  {
    mDragging = false;
    if ( mRubberBand )
    {
      delete mRubberBand;
      mRubberBand = NULL;
    }
  }
}

QString QgsGoogleEarthTool::genTempFileName()
{
  QString tempFileName( "" );
  int currentIndex = 0;
  while ( tempFileName.isEmpty() )
  {
    QString testFileName = QDir::tempPath() + "/qgis2google" + QString::number( currentIndex ) + ".kml";
    if ( !QFile::exists( testFileName ) )
      tempFileName = testFileName;
    currentIndex++;
  }
  return tempFileName;
}

QFile *QgsGoogleEarthTool::getTempFile()
{
  QString tempFileName = genTempFileName();
  if ( !tempFileName.isEmpty() )
  {
    QFile *tempFile = new QFile( tempFileName );
    if ( !tempFile->open( QIODevice::WriteOnly ) )
    {
      QMessageBox::critical( NULL, tr( "File open" ), tr( "Unable to open the temprory file %1" )
                             .arg( tempFile->fileName() ) );
      return NULL;
    }
    mTempKmlFiles.push_back( tempFile );
    return tempFile;
  }
  return NULL;
}

QgsFeatureList QgsGoogleEarthTool::selectOneFeature( QgsVectorLayer *vlayer, const QPoint &pos )
{
    // copy|past from QgsMapToolSelect
    QRect select_rect;
    int boxSize = 0;
    if ( vlayer->geometryType() != QGis::Polygon )
    {
        //if point or line use an artificial bounding box of 10x10 pixels
        //to aid the user to click on a feature accurately
        boxSize = 5;
    }
    else
    {
        //otherwise just use the click point for polys
        boxSize = 1;
    }
    select_rect.setLeft( pos.x() - boxSize );
    select_rect.setRight( pos.x() + boxSize );
    select_rect.setTop( pos.y() - boxSize );
    select_rect.setBottom( pos.y() + boxSize );
    const QgsMapToPixel* transform = mCanvas->getCoordinateTransform();
    QgsPoint ll = transform->toMapCoordinates( select_rect.left(), select_rect.bottom() );
    QgsPoint ur = transform->toMapCoordinates( select_rect.right(), select_rect.top() );

    QgsRectangle searchRect( ll.x(), ll.y(), ur.x(), ur.y() );

    selecteFeatures( vlayer, searchRect );
    return vlayer->selectedFeatures();
}

QgsFeatureList QgsGoogleEarthTool::selectedFeatures( QgsVectorLayer *vlayer, const QRect &rect )
{
  const QgsMapToPixel* coordinateTransform = mCanvas->getCoordinateTransform();

  // set the extent to the Google Earth select
  QgsPoint ll = coordinateTransform->toMapCoordinates( rect.left(), rect.bottom() );
  QgsPoint ur = coordinateTransform->toMapCoordinates( rect.right(), rect.top() );

  QgsRectangle searchRect;
  searchRect.setXMinimum( ll.x() );
  searchRect.setYMinimum( ll.y() );
  searchRect.setXMaximum( ur.x() );
  searchRect.setYMaximum( ur.y() );
  searchRect.normalize();

  selecteFeatures( vlayer, searchRect );
  return vlayer->selectedFeatures();
}

void QgsGoogleEarthTool::selecteFeatures( QgsVectorLayer *vlayer, const QgsRectangle &rect )
{
  // prevent selecting to an empty extent
  if ( rect.width() == 0 || rect.height() == 0 )
  {
    return;
  }

  QgsRectangle searchRect;
  try
  {
    searchRect = toLayerCoordinates( vlayer, rect );
  }
  catch ( QgsCsException &cse )
  {
    Q_UNUSED( cse );
    // catch exception for 'invalid' rectangle and leave existing selection unchanged
    QgsLogger::warning( "Caught CRS exception " + QString( __FILE__ ) + ": " + QString::number( __LINE__ ) );
    QMessageBox::warning( mCanvas, QObject::tr( "CRS Exception" ),
                          QObject::tr( "Selection extends beyond layer's coordinate system." ) );
    return;
  }

  QgsApplication::setOverrideCursor( Qt::WaitCursor );
  vlayer->select( searchRect, false );
  QgsApplication::restoreOverrideCursor();
}

int QgsGoogleEarthTool::attributeNameIndex( QgsVectorLayer *vlayer )
{
  QgsAttributeList attributeList = vlayer->pendingAllAttributesList();

  for( int i = 0; i < attributeList.count(); i++ )
  {
    QString attrDisplayName = vlayer->attributeDisplayName( attributeList.at( i ) );
    if ( attrDisplayName.compare( "name", Qt::CaseInsensitive ) == 0)
    {
      return attributeList.at( i );
    }
  }
  return -1;
}

int QgsGoogleEarthTool::attributeDescrIndex( QgsVectorLayer *vlayer )
{
  QgsAttributeList attributeList = vlayer->pendingAllAttributesList();

  for( int i = 0; i < attributeList.count(); i++ )
  {
    QString attrDisplayName = vlayer->attributeDisplayName( attributeList.at( i ) );
    if ( attrDisplayName.startsWith( "descr", Qt::CaseInsensitive ) )
    {
      return attributeList.at( i );
    }
  }
  return -1;
}

QString QgsGoogleEarthTool::placemarkNameKml( QgsVectorLayer *vlayer, QgsAttributeMap attrMap )
{
  QString result;
  QTextStream out( &result );
  int index = attributeNameIndex( vlayer );

  if ( index > -1 )
  {
    QString name = attrMap.value( index ).toString();
    if ( !name.isEmpty() )
    {
      out << "<name>" << name << "</name>";
    }
  }
  return result;
}

QString QgsGoogleEarthTool::placemarkDescriptionKml( QgsVectorLayer *vlayer, QgsAttributeMap attrMap )
{
  QString result;
  QTextStream out( &result );
  int index = attributeDescrIndex( vlayer );

  if ( index > -1 )
  {
    QString description = attrMap.value( index ).toString();
    if ( !description.isEmpty() )
    {
      out << "<description>" << description << "</description>";
    }
  }
  return result;
}

QRgb QgsGoogleEarthTool::rgba2abgr( QColor color )
{
  qreal redF = color.redF();
  qreal blueF = color.blueF();
  color.setRedF( blueF );
  color.setBlueF( redF );

  return color.rgba();
}

QString QgsGoogleEarthTool::styleKml( QString styleId )
{
  QgsApplication::setOrganizationName( "gis-lab" );
  QgsApplication::setOrganizationDomain( "gis-lab.info" );
  QgsApplication::setApplicationName( "qgis2google" );

  QSettings settings;
  QString result;
  QTextStream out( &result );

  out << "<Style id=\"" << styleId << "\">" << endl;

  QColor lineColor = settings.value( "/qgis2google/line/color" ).value<QColor>();
  QString lineColorMode = settings.value( "/qgis2google/line/colormode" ).toString();
  double lineWidth = settings.value( "/qgis2google/line/width" ).toDouble();
  out << "<LineStyle>" << endl
      << "<color>" << hex << rgba2abgr( lineColor ) << dec << "</color>" << endl
      << "<colorMode>" << lineColorMode << "</colorMode>" << endl
      << "<width>" << lineWidth << "</width>" << endl
      << "</LineStyle>" << endl;

  QColor polyColor = settings.value( "/qgis2google/poly/color" ).value<QColor>();
  QString polyColorMode = settings.value( "/qgis2google/poly/colormode" ).toString();
  int fill = settings.value( "/qgis2google/poly/fill" ).toInt();
  int outline = settings.value( "/qgis2google/poly/outline" ).toInt();
  out << "<PolyStyle>" << endl
      << "<color>" << hex << rgba2abgr( polyColor ) << dec << "</color>" << endl
      << "<colorMode>" << polyColorMode << "</colorMode>" << endl
      << "<fill>" << fill << "</fill>" << endl
      << "<outline>" << outline << "</outline>" << endl
      << "</PolyStyle>" << endl;

  QColor labelColor = settings.value( "/qgis2google/label/color" ).value<QColor>();
  QString labelColorMode = settings.value( "/qgis2google/label/colormode" ).toString();
  double labelScale = settings.value( "/qgis2google/label/scale" ).toDouble();
  out << "<LabelStyle>" << endl
      << "<color>" << hex << rgba2abgr( labelColor ) << dec << "</color>" << endl
      << "<colorMode>" << labelColorMode << "</colorMode>" << endl
      << "<scale>" << labelScale << "</scale>" << endl
      << "</LabelStyle>" << endl;

  out << "</Style>";

  return result;
}

bool QgsGoogleEarthTool::exportToKml( QgsVectorLayer *vlayer, const QgsFeatureList &flist, QFile *tempFile )
{
  QTextCodec *codec = QTextCodec::codecForName( "UTF-8" );
  QTextStream out( tempFile );

  out.setAutoDetectUnicode( false );
  out.setCodec( codec );

  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl
      << "<kml xmlns=\"http://earth.google.com/kml/2.2\"" << endl
      << "xmlns:gx=\"http://www.google.com/kml/ext/2.2\">" << endl;

  out << "<Document>" << endl
      << "<name>" << vlayer->name() << "</name>" << endl;

  QString vlayerStyleId = "styleOf-" + vlayer->name();
  out << styleKml( vlayerStyleId ) << endl;

  foreach ( QgsFeature feature, flist )
  {
    QgsGeometry *geometry = feature.geometry();
    if ( geometry )
    {
      QString wktFormat = geometry->exportToWkt();

      out << "<Placemark>" << endl;
      out << placemarkNameKml( vlayer, feature.attributeMap() ) << endl;
      out << placemarkDescriptionKml( vlayer, feature.attributeMap() ) << endl;
      out << "<styleUrl>" << vlayerStyleId << "</styleUrl>" << endl;
      out << convertWktToKml( wktFormat ) << endl;
      out << "</Placemark>" << endl;
    }
  }

  out << "</Document>" << endl
      << "</kml>" << endl;
  return true;
}

void QgsGoogleEarthTool::exportLayerToKml()
{
  QgsVectorLayer *vlayer = dynamic_cast<QgsVectorLayer*>( mCanvas->currentLayer() );
  if ( vlayer )
  {
    QgsFeatureList featureList;
    QgsRectangle rect = vlayer->extent();

    QgsApplication::setOverrideCursor( Qt::WaitCursor );
    vlayer->select( rect, false );
    featureList = vlayer->selectedFeatures();
    vlayer->invertSelection();
    QgsApplication::restoreOverrideCursor();

    QFile *tempFile = getTempFile();
    if ( tempFile )
    {
      exportToKml( vlayer, featureList, tempFile );
      QDesktopServices::openUrl( QUrl( "file:///" + tempFile->fileName() ) );
    }
  }
}

QString QgsGoogleEarthTool::wkt2kmlPoint( QString wktPoint)
{
  QString result;
  QTextStream out( &result );

  wktPoint = wktPoint.replace( " ", "," );

  QSettings settings;
  int extrude = settings.value( "/qgis2google/point/extrude" ).toInt();
  QString altitudeMode = settings.value( "/qgis2google/point/altitudemode" ).toString();
  out << "<Point>" << endl
      << "<extrude>" << extrude << "</extrude>" << endl
      << "<altitudeMode>" << altitudeMode << "</altitudeMode>" << endl
      << "<coordinates>" << wktPoint << "</coordinates>" << endl
      << "</Point>";

  return result;
}

QString QgsGoogleEarthTool::wkt2kmlLine( QString wktLine)
{
  QString result;
  QTextStream out( &result );

  wktLine = wktLine.replace( " ", "," );
  wktLine = wktLine.replace( ",,", " " );

  QSettings settings;
  int extrude = settings.value( "/qgis2google/line/extrude" ).toInt();
  int tessellate = settings.value( "/qgis2google/line/tessellate" ).toInt();
  QString altitudeMode = settings.value( "/qgis2google/line/altitudemode" ).toString();
  out << "<LineString>" << endl
      << "<extrude>" << extrude << "</extrude>" << endl
      << "<tessellate>" << tessellate << "</tessellate>" << endl
      << "<altitudeMode>" << altitudeMode << "</altitudeMode>" << endl
      << "<coordinates>" << wktLine << "</coordinates>" << endl
      << "</LineString>";

  return result;
}

QString QgsGoogleEarthTool::wkt2kmlPolygon( QString wktPolygon)
{
  QString result;
  QSettings settings;

  QTextStream out( &result );
  int altitudeVal = settings.value( "/qgis2google/poly/altitudevalue" ).toInt();
  QString altitudeMode = settings.value( "/qgis2google/poly/altitudemode" ).toString();
  QStringList polygonList = wktPolygon.split( "),(" );

  QStringList polygonKmlList;
  foreach( QString poly, polygonList )
  {
    poly = poly.replace( ",", ",," );
    qDebug() << poly;
    poly = poly.replace( " ", "," );
    qDebug() << poly;
    poly = poly.replace( ",,", " " );
    qDebug() << poly;
    if ( altitudeMode != "clampToGround" && altitudeMode != "clampToSeaFloor" && altitudeVal != -1 )
      poly = poly.replace( " ", QString( ",%1 " ).arg( altitudeVal ) );
    qDebug() << poly;
    polygonKmlList.append( poly );
    qDebug() << polygonKmlList;
  }

  int extrude = settings.value( "/qgis2google/poly/extrude" ).toInt();
  int tessellate = settings.value( "/qgis2google/poly/tessellate" ).toInt();
  out << "<Polygon>" << endl
      << "<extrude>" << extrude << "</extrude>" << endl
      << "<tessellate>" << tessellate << "</tessellate>" << endl
      << "<gx:altitudeMode>" << altitudeMode << "</gx:altitudeMode>" << endl
      << "<outerBoundaryIs>" << endl << "<LinearRing>" << endl
      << "<coordinates>" << polygonKmlList.at( 0 ) << "</coordinates>" << endl
      << "</LinearRing>" << endl << "</outerBoundaryIs>" << endl;

  for( int i = 1; i < polygonKmlList.count(); i++ )
  {
    out << "<innerBoundaryIs>" << endl << "<LinearRing>" << endl
        << "<coordinates>" << polygonKmlList.at( i ) << "</coordinates>" << endl
        << "</LinearRing>" << endl << "</innerBoundaryIs>" << endl;
  }

  out << "</Polygon>";
  return result;
}

QString QgsGoogleEarthTool::convertWktToKml( QString wkt )
{
  QString result;
  QTextStream out( &result );

  if ( wkt.startsWith( "POINT", Qt::CaseInsensitive ) )
  {
    QString point = wkt.mid( 6, wkt.length() - 7);
    out << wkt2kmlPoint( point );
  }
  else if ( wkt.startsWith( "MULTIPOINT", Qt::CaseInsensitive ) )
  {
    QString point = wkt.mid( 11, wkt.length() - 12);
    out << wkt2kmlPoint( point );
  }
  else if ( wkt.startsWith( "LINESTRING", Qt::CaseInsensitive ) )
  {
    QString line = wkt.mid( 11, wkt.length() - 12);
    out << wkt2kmlLine( line );
  }
  else if ( wkt.startsWith( "MULTILINESTRING", Qt::CaseInsensitive ) )
  {
    QString line = wkt.mid( 17, wkt.length() - 19);
    out << wkt2kmlLine( line );
  }
  else if ( wkt.startsWith( "POLYGON", Qt::CaseInsensitive ) )
  {
    QString polygon = wkt.mid( 9, wkt.length() - 10);
    out << wkt2kmlPolygon( polygon );
  }
  else if ( wkt.startsWith( "MULTIPOLYGON", Qt::CaseInsensitive ) )
  {
    QString mpolygon = wkt.mid( 15, wkt.length() - 18);
    QStringList mpolygonList = mpolygon.split( ")),((" );
    QTextStream out( &result );

    out << "<MultiGeometry>" << endl;
    foreach ( QString polygon, mpolygonList )
    {
      out << wkt2kmlPolygon( polygon ) << endl;
    }
    out << "</MultiGeometry>";
  }
  else
  {
    QMessageBox::critical( NULL, tr( "Convert wkt to kml" ), tr( "Unable to convert wkt to kml" ) );
    qDebug() << "Error unable to convert wkt to kml";
  }

  return result;
}
