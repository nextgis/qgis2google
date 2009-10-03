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
//  Q_DECLARE_TR_FUNCTIONS(QgsKmlConverter);

public:
  QgsKmlConverter();
  ~QgsKmlConverter();

  void exportLayerToKmlFile( QgsVectorLayer *vlayer );
  void exportFeaturesToKmlFile( QgsVectorLayer *vlayer, const QgsFeatureList &flist );

signals:
  void kmlFileCreated(QString fileName);

private:
  QString createTempFileAndOutput(QTextStream *&out);
  void writeHeadAndStyleInKmlFile( QTextStream *out, const QgsRenderer *renderer,
                                   QList<QgsSymbol *> symbols, QString layerName,
                                   QString styleId, bool bSingleSymbol, bool bUniqueValue,
                                   int transp, QGis::GeometryType geomType );
  QString generateTempFileName();
  QFile *getTempFile();

  QString convertWkbToKml( QgsGeometry *geometry );

  int attributeNameIndex( QgsVectorLayer *vlayer);
  int attributeDescriprionIndex( QgsVectorLayer *vlayer);

  QString styleKmlSingleSymbol( int transp, QgsSymbol *symbol, QString styleId,
                                QGis::GeometryType typeOfFeature );
  QString styleKmlUniqueValue( int transp, QString styleId, QList<QgsSymbol *> symbols,
                               QGis::GeometryType typeOfFeature);
  QString placemarkNameKml( QgsVectorLayer *vlayer, QgsAttributeMap attrMap );
  QString placemarkDescriptionKml( QgsVectorLayer *vlayer, QgsAttributeMap attrMap );

  QString featureStyleId( QgsSymbol *symbol, QString styleId );
  QgsSymbol *symbolForFeature( QgsFeature *feature, const QgsUniqueValueRenderer *urenderer );

  QString removeEscapeChars( QString in );
  QRgb rgba2abgr( QColor color );

  QList<QFile *> mTempKmlFiles;
};

#endif // QGSKMLCONVERTER_H
