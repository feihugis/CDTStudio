#ifndef CDTPROJECTTREEITEM_H
#define CDTPROJECTTREEITEM_H

#include <QStandardItem>
#include "cdtbaseobject.h"
#include "qgsmapcanvas.h"

class CDTProjectTreeItem : public QStandardItem
{
public:
    enum CDTItemType{
        PROJECT_ROOT,
        IMAGE_ROOT,
        SEGMENTION_ROOT,
        SEGMENTION,
        EXTRACTION_ROOT,
        EXTRACTION,
        CLASSIFICATION_ROOT,
        CLASSIFICATION,
        METHOD_PARAMS,
        PARAM,
        VALUE
    };

    enum LayerType{
        EMPTY,
        GROUP,
        RASTER,
        VECTOR
    };

    CDTProjectTreeItem(CDTItemType tp,LayerType ly, const QString &text,CDTBaseObject *crspdObject );
    CDTItemType getType()const {return _itemType;}
    CDTBaseObject* correspondingObject()const {return corrObject;}

    QgsMapLayer *mapLayer()const;
    void setMapLayer(QgsMapLayer* layer);


private:
    void initAlignment();
    void initFont();
    void initColor();
    void initCheckState();
    CDTItemType _itemType;
    LayerType layerType;
    CDTBaseObject* corrObject;
    QgsMapLayer* qgsMapLayer;
};

#endif // CDTPROJECTTREEITEM_H
