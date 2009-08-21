#ifndef QGSKMLCONVERTER_H
#define QGSKMLCONVERTER_H

#include <QColor>
#include <QTextStream>

#include <qgis.h>
#include <qgsfeature.h>

class QFile;

class QgsRenderer;
class QgsSymbol;
class QgsVectorLayer;
class QgsUniqueValueRenderer;

class QgsKmlConverter : public QObject
{
  Q_OBJECT

public:
  QgsKmlConverter();
  ~QgsKmlConverter();

  QString exportLayerToKmlFile( QgsVectorLayer *vlayer );
  QString exportToKmlFile( QgsVectorLayer *vlayer, const QgsFeatureList &flist );

private:
  QString genTempFileName();
  QFile *getTempFile();

  QString convertWktToKml( QString wktFormat );

  int attributeNameIndex( QgsVectorLayer *vlayer);
  int attributeDescrIndex( QgsVectorLayer *vlayer);

  QString styleKmlSingleSymbol( QString styleId, QGis::GeometryType typeOfFeature );
  QString styleKmlUniqueValue( int transp, QString styleId, QList<QgsSymbol *> symbols,
                               QGis::GeometryType typeOfFeature);
  QString placemarkNameKml( QgsVectorLayer *vlayer, QgsAttributeMap attrMap );
  QString placemarkDescriptionKml( QgsVectorLayer *vlayer, QgsAttributeMap attrMap );

  QString wkt2kmlPoint( QString wktPoint);
  QString wkt2kmlLine( QString wktLine);
  QString wkt2kmlPolygon( QString wktPolygon);

  QString vlayerStyleId( QgsFeature *feature, QString styleId, const QgsRenderer *renderer );
  QgsSymbol *symbolForFeature( QgsFeature *feature, const QgsUniqueValueRenderer *urenderer );

  QString htmlString( QString in );
  QRgb rgba2abgr( QColor color );

  QList<QFile *> mTempKmlFiles;
};

#endif // QGSKMLCONVERTER_H
