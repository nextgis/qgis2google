#include <QColor>
#include <QRect>
#include <QStringList>

#include <qgsfeature.h>
#include <qgsmaptool.h>

class QFile;
class QRubberBand;

class QgsVectorLayer;

class QgsGoogleEarthTool : public QgsMapTool
{
  Q_OBJECT
public:
  QgsGoogleEarthTool( QgsMapCanvas *canvas );
  ~QgsGoogleEarthTool( );

public slots:
  void exportLayerToKml();

protected:
  void canvasPressEvent( QMouseEvent *e );
  void canvasMoveEvent( QMouseEvent *e );
  void canvasReleaseEvent( QMouseEvent *e );

private:
  QString genTempFileName();
  QFile *getTempFile();

  bool exportToKml( QgsVectorLayer *vlayer, const QgsFeatureList &flist, QFile *tempFile );
  QString convertWktToKml( QString wktFormat );

  void selecteFeatures( QgsVectorLayer *vlayer, const QgsRectangle &rect );
  QgsFeatureList selectedFeatures( QgsVectorLayer *vlayer, const QRect &rect );
  QgsFeatureList selectOneFeature( QgsVectorLayer *vlayer, const QPoint &pos );

  int attributeNameIndex( QgsVectorLayer *vlayer);
  int attributeDescrIndex( QgsVectorLayer *vlayer);

  QString styleKml( QString styleId );
  QString placemarkNameKml( QgsVectorLayer *vlayer, QgsAttributeMap attrMap );
  QString placemarkDescriptionKml( QgsVectorLayer *vlayer, QgsAttributeMap attrMap );

  QString wkt2kmlPoint( QString wktPoint);
  QString wkt2kmlLine( QString wktLine);
  QString wkt2kmlPolygon( QString wktPolygon);

  QRgb rgba2abgr( QColor color );

  QList<QFile *> mTempKmlFiles;
  //! stores Google Earth select rect
  QRect mGERect;

  //! Flag to indicate a map canvas drag operation is taking place
  bool mDragging;

  //! TODO: to be changed to a canvas item
  QRubberBand *mRubberBand;
};
