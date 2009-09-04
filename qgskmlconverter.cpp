#include <QFile>
#include <QMessageBox>

#include <qgsapplication.h>
#include <qgsgeometry.h>
#include <qgslogger.h>
#include <qgsmapcanvas.h>
#include <qgsrenderer.h>
#include <qgssymbol.h>
#include <qgsvectorlayer.h>
#include <qgsuniquevaluerenderer.h>

#include "qgskmlconverter.h"

#define STYLEIDDELIMIT "."

const QString myPathToIcon = "http://maps.google.com/mapfiles/kml/shapes/donut.png";

QgsKmlConverter::QgsKmlConverter()
{
  QgsApplication::setOrganizationName( "gis-lab" );
  QgsApplication::setOrganizationDomain( "gis-lab.info" );
  QgsApplication::setApplicationName( "qgis2google" );
}

QgsKmlConverter::~QgsKmlConverter()
{
  while ( mTempKmlFiles.count() )
  {
    QFile *file = mTempKmlFiles.takeFirst();
    file->remove();
    delete file;
  }
}

QString QgsKmlConverter::exportLayerToKmlFile( QgsVectorLayer *vlayer )
{
  if ( vlayer )
  {
    QgsFeatureList featureList;
    QgsRectangle rect = vlayer->extent();

    QgsApplication::setOverrideCursor( Qt::WaitCursor );
    vlayer->select( rect, false );
    featureList = vlayer->selectedFeatures();
    vlayer->invertSelection();
    QgsApplication::restoreOverrideCursor();

    return exportToKmlFile( vlayer, featureList );
  }
  return QString();
}

QString QgsKmlConverter::exportToKmlFile( QgsVectorLayer *vlayer, const QgsFeatureList &flist )
{
  QgsApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );
  QFile *tempFile = getTempFile();
  if ( !tempFile->exists() )
    return QString();

  // set codec for kml file
  QTextCodec *codec = QTextCodec::codecForName( "UTF-8" );
  QTextStream out( tempFile );

  out.setAutoDetectUnicode( false );
  out.setCodec( codec );

  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl
      << "<kml xmlns=\"http://earth.google.com/kml/2.2\"" << endl
      << "xmlns:gx=\"http://www.google.com/kml/ext/2.2\">" << endl;

  out << "<Document>" << endl
      << "<name>" << removeEscapeChars( vlayer->name() ) << "</name>" << endl;

  const QgsRenderer *renderer = vlayer->renderer();
  QList< QgsSymbol *> symbols = renderer->symbols();
  QString styleId = "styleOf-" + vlayer->name();
  bool bSingleSymbol = renderer->name() == "Single Symbol";
  bool bUniqueValue = renderer->name() == "Unique Value";

  QSettings settings;
  bool bOverideUniqueValue = settings.value( "/qgis2google/overridelayerstyle" ).toBool();
  if ( bOverideUniqueValue )
  {
    bSingleSymbol = true;
    bUniqueValue = false;
  }

  if ( bSingleSymbol )
  {
    // create style kml for one symbol and convert getted line to html (e.g. replace & to &amp;)
    out << removeEscapeChars( styleKmlSingleSymbol( styleId, vlayer->geometryType() ) ) << endl;
  }
  else if ( bUniqueValue )
  {
    // create style kml for many symbols and convert getted line to html (e.g. replace & to &amp;)
    out << removeEscapeChars( styleKmlUniqueValue( vlayer->getTransparency(), styleId, symbols,
                                            vlayer->geometryType() ) ) << endl;
  }
  else
  {
    // dont process other renderers symbols
    return "";
  }

  // export eatch feature to kml format
  foreach ( QgsFeature feature, flist )
  {
    QgsGeometry *geometry = feature.geometry();
    if ( geometry )
    {
      out << "<Placemark>" << endl;
      // try to find name of feature from attribute table and set one for kml's placemark as html
      if ( bSingleSymbol )
      {
        out << removeEscapeChars( placemarkNameKml( vlayer, feature.attributeMap() ) ) << endl;
      }
      else // Unique Value
      {
        const QgsUniqueValueRenderer *urenderer = dynamic_cast<const QgsUniqueValueRenderer *>( renderer );
        QgsSymbol *symbol = symbolForFeature( &feature, urenderer );
        if ( symbol )
          out << "<name>" + removeEscapeChars( symbol->lowerValue() ) + "</name>" << endl;
      }

      // try to find placemark description in attribute table (it should be in html format)
      out << placemarkDescriptionKml( vlayer, feature.attributeMap() ) << endl;

      if ( bSingleSymbol )
      {
        out << "<styleUrl>" << removeEscapeChars( styleId ) << "</styleUrl>" << endl;
      }
      else // Unique Value
      {
        out << "<styleUrl>" << removeEscapeChars( featureStyleId( &feature, styleId, renderer ) ) << "</styleUrl>" << endl;
      }

      // convert wkt to kml and write to kml file
      QString wktFormat = geometry->exportToWkt();
      out << convertWktToKml( wktFormat ) << endl;
      out << "</Placemark>" << endl;
    }
  }

  out << "</Document>" << endl
      << "</kml>" << endl;

  QgsApplication::restoreOverrideCursor();
  return tempFile->fileName();
}

// try to find feature's name in attribute table
int QgsKmlConverter::attributeNameIndex( QgsVectorLayer *vlayer )
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

// create kml string with feature's name from attribute table
QString QgsKmlConverter::placemarkNameKml( QgsVectorLayer *vlayer, QgsAttributeMap attrMap )
{
  QString result;
  QTextStream out( &result );

  int index = attributeNameIndex( vlayer );

  if ( index > -1 )
  {
    QString name = attrMap.value( index ).toString();
    if ( !name.isEmpty() )
    {
      out << "<name>" << removeEscapeChars( name ) << "</name>";
    }
  }
  return result;
}

// try to find feature's description in attribute table
int QgsKmlConverter::attributeDescrIndex( QgsVectorLayer *vlayer )
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

// create kml string with feature's description from attribute table
QString QgsKmlConverter::placemarkDescriptionKml( QgsVectorLayer *vlayer, QgsAttributeMap attrMap )
{
  QString result;
  QTextStream out( &result );

  int index = attributeDescrIndex( vlayer );

  if ( index > -1 )
  {
    QString description = attrMap.value( index ).toString();
    if ( !description.isEmpty() )
    {
      out << "<description>" << removeEscapeChars( description ) << "</description>";
    }
  }
  return result;
}

// in kml instead argb is abgr color modele
QRgb QgsKmlConverter::rgba2abgr( QColor color )
{
  qreal redF = color.redF();
  qreal blueF = color.blueF();
  color.setRedF( blueF );
  color.setBlueF( redF );

  return color.rgba();
}

// find symbol (that contains color, size, fill settings) wich feature will be draw
QgsSymbol *QgsKmlConverter::symbolForFeature( QgsFeature *feature, const QgsUniqueValueRenderer *urenderer )
{
  //first find out the value
  const QgsAttributeMap& attrs = feature->attributeMap();
  QString value = attrs[ urenderer->classificationField() ].toString();

  QList<QgsSymbol *> symbols = urenderer->symbols();
  foreach( QgsSymbol *symbol, symbols )
  {
    if ( symbol->lowerValue() == value )
      return symbol;
  }
  return NULL;
}

// create kml style identificator for feature
QString QgsKmlConverter::featureStyleId( QgsFeature *feature, QString styleId, const QgsRenderer *renderer )
{
  const QgsUniqueValueRenderer *uniqValRenderer = dynamic_cast<const QgsUniqueValueRenderer *>( renderer );
  QgsSymbol *symbol = symbolForFeature( feature, uniqValRenderer );

  if ( symbol )
    return styleId + STYLEIDDELIMIT + symbol->lowerValue();
  else
    return "";
}

// create string with kml style description section, all values takes from settings
QString QgsKmlConverter::styleKmlSingleSymbol( QString styleId, QGis::GeometryType typeOfFeature )
{
  double tmpDouble;
  QSettings settings;
  QString result, colorMode;
  QTextStream out( &result );
  QColor color;

  out << "<Style id=\"" << styleId << "\">" << endl;

  color = settings.value( "/qgis2google/label/color" ).value<QColor>();
  colorMode = settings.value( "/qgis2google/label/colormode" ).toString();
  tmpDouble = settings.value( "/qgis2google/label/scale" ).toDouble();
  out << "<LabelStyle>" << endl
      << "<color>" << hex << rgba2abgr( color ) << dec << "</color>" << endl
      << "<colorMode>" << colorMode << "</colorMode>" << endl
      << "<scale>" << tmpDouble << "</scale>" << endl
      << "</LabelStyle>" << endl;

//  switch ( typeOfFeature )
//  {
//  case QGis::Point:
//    {
      color = settings.value( "/qgis2google/icon/color" ).value<QColor>();
      colorMode = settings.value( "/qgis2google/icon/colormode" ).toString();
      tmpDouble = settings.value( "/qgis2google/icon/scale" ).toDouble();
      out << "<IconStyle>" << endl
          << "<color>" << hex << rgba2abgr( color ) << dec << "</color>" << endl
          << "<colorMode>" << colorMode << "</colorMode>" << endl
          << "<scale>" << tmpDouble << "</scale>" << endl
          << "<Icon>" << endl << "<href>" << myPathToIcon << "</href>" << endl << "</Icon>" << endl
          << "</IconStyle>" << endl;
//      break;
//    }
//  case QGis::Line:
//    {
      color = settings.value( "/qgis2google/line/color" ).value<QColor>();
      colorMode = settings.value( "/qgis2google/line/colormode" ).toString();
      tmpDouble = settings.value( "/qgis2google/line/width" ).toDouble();
      out << "<LineStyle>" << endl
          << "<color>" << hex << rgba2abgr( color ) << dec << "</color>" << endl
          << "<colorMode>" << colorMode << "</colorMode>" << endl
          << "<width>" << tmpDouble << "</width>" << endl
          << "</LineStyle>" << endl;
//      break;
//    }
//  case QGis::Polygon:
//    {
      color = settings.value( "/qgis2google/poly/color" ).value<QColor>();
      colorMode = settings.value( "/qgis2google/poly/colormode" ).toString();
      int fill = settings.value( "/qgis2google/poly/fill" ).toInt();
      int outline = settings.value( "/qgis2google/poly/outline" ).toInt();
      out << "<PolyStyle>" << endl
          << "<color>" << hex << rgba2abgr( color ) << dec << "</color>" << endl
          << "<colorMode>" << colorMode << "</colorMode>" << endl
          << "<fill>" << fill << "</fill>" << endl
          << "<outline>" << outline << "</outline>" << endl
          << "</PolyStyle>" << endl;
//    }
//  case QGis::UnknownGeometry:
//    break;
//  }

  out << "</Style>";

  return result;
}

// create string with kml style description section for each symbols, all values takes from settings
QString QgsKmlConverter::styleKmlUniqueValue( int transp, QString styleId, QList<QgsSymbol *> symbols,
                                              QGis::GeometryType typeOfFeature )
{
  double scale = 1.0;
  QSettings settings;
  QString result, colorMode( "normal" );
  QTextStream out( &result );
  QColor color, fillColor;

  foreach( QgsSymbol *symbol, symbols )
  {
    QString symbolName;

    if ( !symbol->lowerValue().isEmpty() )
      symbolName = symbol->lowerValue();

    out << endl << "<Style id=\"" << styleId + STYLEIDDELIMIT + symbolName << "\">" << endl;

    color = symbol->color();
    color.setAlpha( transp );
    fillColor = symbol->fillColor();
    fillColor.setAlpha( transp );

    out << "<LabelStyle>" << endl
        << "<color>" << hex << rgba2abgr( color ) << dec << "</color>" << endl
        << "<colorMode>" << colorMode << "</colorMode>" << endl
        << "<scale>" << scale << "</scale>" << endl
        << "</LabelStyle>" << endl;

//    switch ( typeOfFeature )
//    {
//    case QGis::Point:
//      {
        out << "<IconStyle>" << endl
            << "<color>" << hex << rgba2abgr( fillColor ) << dec << "</color>" << endl
            << "<colorMode>" << colorMode << "</colorMode>" << endl
            << "<scale>" << scale << "</scale>" << endl
            << "<Icon>" << endl << "<href>" << myPathToIcon << "</href>" << endl << "</Icon>" << endl
            << "</IconStyle>" << endl;
//        break;
//      }
//    case QGis::Line:
//      {
        double lineWidth = symbol->lineWidth();
        out << "<LineStyle>" << endl
            << "<color>" << hex << rgba2abgr( color ) << dec << "</color>" << endl
            << "<colorMode>" << colorMode << "</colorMode>" << endl
            << "<width>" << lineWidth << "</width>" << endl
            << "</LineStyle>" << endl;
//        break;
//      }
//    case QGis::Polygon:
//      {
        int bPolyStyle = symbol->brush().style() != Qt::NoBrush;
        int fill = bPolyStyle;
        bPolyStyle = symbol->pen().style() != Qt::NoPen;
        int outline = bPolyStyle;
        out << "<PolyStyle>" << endl
            << "<color>" << hex << rgba2abgr( fillColor ) << dec << "</color>" << endl
            << "<colorMode>" << colorMode << "</colorMode>" << endl
            << "<fill>" << fill << "</fill>" << endl
            << "<outline>" << outline << "</outline>" << endl
            << "</PolyStyle>" << endl;
//        break;
//      }
//    case QGis::UnknownGeometry:
//      break;
//    }

    out << "</Style>";
  }
  return result;
}

// convert wkt coordinates of point to kml with current values of altitude, extrude and altitude mode
QString QgsKmlConverter::wkt2kmlPoint( QString wktPoint)
{
  QString result;
  QSettings settings;
  QTextStream out( &result );
  int altitudeVal = settings.value( "/qgis2google/point/altitudevalue" ).toInt();
  QString altitudeMode = settings.value( "/qgis2google/point/altitudemode" ).toString();

  wktPoint = wktPoint.replace( " ", "," );
  if ( altitudeMode != "clampToGround" && altitudeMode != "clampToSeaFloor" && altitudeVal != -1 )
    wktPoint = wktPoint + "," + QString::number( altitudeVal );

  int extrude = settings.value( "/qgis2google/point/extrude" ).toInt();
  out << "<Point>" << endl
      << "<extrude>" << extrude << "</extrude>" << endl
      << "<altitudeMode>" << altitudeMode << "</altitudeMode>" << endl
      << "<coordinates>" << wktPoint << "</coordinates>" << endl
      << "</Point>";

  return result;
}

// convert wkt coordinates of line to kml with current values of altitude, extrude and altitude mode
QString QgsKmlConverter::wkt2kmlLine( QString wktLine)
{
  QString result;
  QSettings settings;
  QTextStream out( &result );
  int altitudeVal = settings.value( "/qgis2google/line/altitudevalue" ).toInt();
  QString altitudeMode = settings.value( "/qgis2google/line/altitudemode" ).toString();

  wktLine = wktLine.replace( " ", "," );
  wktLine = wktLine.replace( ",,", " " );
  if ( altitudeMode != "clampToGround" && altitudeMode != "clampToSeaFloor" && altitudeVal != -1 )
    wktLine = wktLine.replace( " ", QString( ",%1 " ).arg( altitudeVal ) );

  int extrude = settings.value( "/qgis2google/line/extrude" ).toInt();
  int tessellate = settings.value( "/qgis2google/line/tessellate" ).toInt();
  out << "<LineString>" << endl
      << "<extrude>" << extrude << "</extrude>" << endl
      << "<tessellate>" << tessellate << "</tessellate>" << endl
      << "<altitudeMode>" << altitudeMode << "</altitudeMode>" << endl
      << "<coordinates>" << wktLine << "</coordinates>" << endl
      << "</LineString>";

  return result;
}

// convert wkt coordinates of polygon to kml with current values of altitude, extrude and altitude mode
QString QgsKmlConverter::wkt2kmlPolygon( QString wktPolygon)
{
  QString result;
  QTextStream out( &result );
  QSettings settings;

  int altitudeVal = settings.value( "/qgis2google/poly/altitudevalue" ).toInt();
  QString altitudeMode = settings.value( "/qgis2google/poly/altitudemode" ).toString();
  QStringList polygonList = wktPolygon.split( "),(" );

  QStringList polygonKmlList;
  foreach( QString poly, polygonList )
  {
    poly = poly.replace( ",", ",," );
    poly = poly.replace( " ", "," );
    poly = poly.replace( ",,", " " );
    if ( altitudeMode != "clampToGround" && altitudeMode != "clampToSeaFloor" && altitudeVal != -1 )
      poly = poly.replace( " ", QString( ",%1 " ).arg( altitudeVal ) );
    polygonKmlList.append( poly );
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


// convert wkt coordinates to kml string
QString QgsKmlConverter::convertWktToKml( QString wkt )
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

    out << "<MultiGeometry>" << endl;
    foreach ( QString polygon, mpolygonList )
    {
      out << wkt2kmlPolygon( polygon ) << endl;
    }
    out << "</MultiGeometry>";
  }
  else
  {
    QgsLogger::debug( tr( "Error: Unable to convert wkt to kml" ) );
    Q_ASSERT( 0 );
  }

  return result;
}

// generate name for temporary file
QString QgsKmlConverter::genTempFileName()
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

// open for write temporary file
QFile *QgsKmlConverter::getTempFile()
{
  QString tempFileName = genTempFileName();
  if ( !tempFileName.isEmpty() )
  {
    QFile *tempFile = new QFile( tempFileName );
    if ( !tempFile->open( QIODevice::WriteOnly ) )
    {
      QMessageBox::critical( NULL, tr( "File open" ), tr( "Unable to open the temprory file %1" )
                             .arg( tempFile->fileName() ) );
      QgsLogger::debug( tr( "Unable to open the temprory file %1" ) );
      Q_ASSERT( 0 );
      return NULL;
    }
    mTempKmlFiles.push_back( tempFile );
    return tempFile;
  }
  return NULL;
}

// remove escape character from kml file string
QString QgsKmlConverter::removeEscapeChars( QString in )
{
  return in.replace( QRegExp( "&(?!amp;)" ), "&amp;" );
}
