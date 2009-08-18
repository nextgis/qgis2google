#ifndef QGSKMLCONVERTER_H
#define QGSKMLCONVERTER_H

#include <QColor>

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

  QString styleKmlSingleSymbol( QString styleId );
  QString styleKmlUniqueValue( int transp, QString styleId, QList<QgsSymbol *> symbols );
  QString placemarkNameKml( QgsVectorLayer *vlayer, QgsAttributeMap attrMap );
  QString placemarkDescriptionKml( QgsVectorLayer *vlayer, QgsAttributeMap attrMap );

  QString wkt2kmlPoint( QString wktPoint);
  QString wkt2kmlLine( QString wktLine);
  QString wkt2kmlPolygon( QString wktPolygon);

  QRgb rgba2abgr( QColor color );
  QString vlayerStyleId( const QgsFeature *feature, QString styleId, const QgsRenderer *renderer );

  QgsSymbol *symbolForFeature( const QgsFeature *f, const QgsUniqueValueRenderer *r );

  QList<QFile *> mTempKmlFiles;
};

#endif // QGSKMLCONVERTER_H
