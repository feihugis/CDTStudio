#include "cdtsegmentationlayer.h"
#include "stable.h"
#include "qtcolorpicker.h"
#include "cdtfilesystem.h"
#include "mainwindow.h"
#include "cdtprojectlayer.h"
#include "cdtimagelayer.h"
#include "cdtclassificationlayer.h"
#include "cdtprojecttreeitem.h"
#include "MapTools/cdtmaptoolselecttrainingsamples.h"
#include "cdtvariantconverter.h"
#include "cdtattributedockwidget.h"
#include "cdtattributesinterface.h"
#include "wizardnewclassification.h"
#include "dialogdecisionfusion.h"
#include "dialoggenerateattributes.h"

extern QList<CDTAttributesInterface *>     attributesPlugins;
QList<CDTSegmentationLayer *> CDTSegmentationLayer::layers;

QDataStream &operator<<(QDataStream &out, const SampleElement &sample)
{
    out<<sample.ObjectID<<sample.categoryID<<sample.sampleID;
    return out;
}

QDataStream &operator>>(QDataStream &in, SampleElement &sample)
{
    in>>sample.ObjectID>>sample.categoryID>>sample.sampleID;
    return in;
}

CDTSegmentationLayer::CDTSegmentationLayer(QUuid uuid, QObject *parent)
    : CDTBaseLayer(uuid,parent)
//      actionGenerateAttributes(),
//      actionAddClassifications(),
//      actionRemoveSegmentation(),
//      actionAddDecisionFusion(),
//      actionExportShapefile(),
//      actionRemoveAllClassifications(),
//      actionRename(),
//      actionEditDBInfo(),
//      actionChangeBorderColor(),
//      actionSetLayerTransparency()
{
    layers.push_back(this);

    keyItem   = new CDTProjectTreeItem(CDTProjectTreeItem::SEGMENTION,CDTProjectTreeItem::VECTOR,QString(),this);
    classificationRootItem = new CDTProjectTreeItem(CDTProjectTreeItem::CLASSIFICATION_ROOT,CDTProjectTreeItem::EMPTY,tr("Classification"),this);
    keyItem->appendRow(classificationRootItem);


    //Actions
    QWidgetAction *actionChangeBorderColor = new QWidgetAction(this);
    QWidgetAction *actionSetLayerTransparency = new QWidgetAction(this);
    QAction *actionRename = new QAction(QIcon(":/Icon/Rename.png"),tr("Rename Segmentation"),this);
    QAction *actionGenerateAttributes = new QAction(QIcon(":/Icon/AddProperty.png"),tr("Generate Attributes"),this);
    QAction *actionEditDBInfo = new QAction(QIcon(":/Icon/DataSource.png"),tr("Edit Attribute DB Source"),this);
    QAction *actionExportShapefile = new QAction(QIcon(":/Icon/Export.png"),tr("Export Shapefile"),this);
    QAction *actionRemoveSegmentation = new QAction(QIcon(":/Icon/Remove.png"),tr("Remove Segmentation"),this);
    QAction *actionAddClassifications = new QAction(QIcon(":/Icon/Add.png"),tr("Add Classification"),this);
    QAction *actionRemoveAllClassifications = new QAction(QIcon(":/Icon/Remove.png"),tr("Remove All Classifications"),this);
    QAction *actionAddDecisionFusion = new QAction(tr("Run Decision Fusion"),this);

    actions <<(QList<QAction *>()<<actionChangeBorderColor<<actionSetLayerTransparency<<actionRename
                                <<actionEditDBInfo<<actionGenerateAttributes<<actionExportShapefile)
            <<(QList<QAction *>()<<actionRemoveSegmentation)
            <<(QList<QAction *>()<<actionAddClassifications<<actionRemoveAllClassifications<<actionAddDecisionFusion);

    //Widgets for context menu
    QtColorPicker *borderColorPicker = new QtColorPicker(NULL);
    borderColorPicker->setStandardColors();
    borderColorPicker->setToolTip(tr("Border color"));
    connect(borderColorPicker,SIGNAL(colorChanged(QColor)),SLOT(setBorderColor(QColor)));
    connect(this,SIGNAL(borderColorChanged(QColor)),borderColorPicker,SLOT(setCurrentColor(QColor)));
    connect(this,SIGNAL(destroyed()),borderColorPicker,SLOT(deleteLater()));
    actionChangeBorderColor->setDefaultWidget(borderColorPicker);

    QSlider *sliderTransparency = new QSlider(Qt::Horizontal,NULL);
    sliderTransparency->setMinimum(0);
    sliderTransparency->setMaximum(100);
    sliderTransparency->setToolTip(tr("Layer transparency"));
    connect(sliderTransparency,SIGNAL(valueChanged(int)),SLOT(setLayerTransparency(int)));
    connect(this,SIGNAL(layerTransparencyChanged(int)),sliderTransparency,SLOT(setValue(int)));
    connect(this,SIGNAL(destroyed()),sliderTransparency,SLOT(deleteLater()));
    actionSetLayerTransparency->setDefaultWidget(sliderTransparency);


    //Connections
    connect(actionRename,SIGNAL(triggered()),SLOT(rename()));
    connect(actionEditDBInfo,SIGNAL(triggered()),SLOT(editDBInfo()));
    connect(actionGenerateAttributes,SIGNAL(triggered()),SLOT(generateAttributes()));
    connect(actionExportShapefile,SIGNAL(triggered()),SLOT(exportShapefile()));
    connect(actionRemoveSegmentation,SIGNAL(triggered()),SLOT(remove()));
    connect(actionAddClassifications,SIGNAL(triggered()),SLOT(addClassification()));
    connect(actionRemoveAllClassifications,SIGNAL(triggered()),SLOT(removeAllClassifications()));
    connect(actionAddDecisionFusion,SIGNAL(triggered()),SLOT(decisionFusion()));

    connect(this,SIGNAL(nameChanged()),this,SIGNAL(layerChanged()));
    connect(this,SIGNAL(methodParamsChanged()),this,SIGNAL(layerChanged()));
    connect(this,SIGNAL(removeSegmentation(CDTSegmentationLayer*)),this->parent(),SLOT(removeSegmentation(CDTSegmentationLayer*)));
    //    connect(this,SIGNAL(segmentationChanged()),this->parent(),SIGNAL(imageLayerChanged()));

}

CDTSegmentationLayer::~CDTSegmentationLayer()
{    
    if (id().isNull())
        return;

    QSqlQuery query(QSqlDatabase::database("category"));
    bool ret;
    ret = query.exec("delete from segmentationlayer where id = '"+uuid.toString()+"'");
    if (!ret)
        qWarning()<<"prepare:"<<query.lastError().text();

    ret = query.exec(QString("select id from sample_segmentation where segmentationid = '%1'").arg(uuid.toString()));
    if (!ret)
        qWarning()<<"prepare:"<<query.lastError().text();
    QStringList list;
    while (query.next()) {
        list<<query.value(0).toString();
    }
    foreach (QString sampleid, list) {
        ret = query.exec(QString("delete from object_samples where sampleid = '%1'").arg(sampleid));
        if (!ret)
            qWarning()<<"prepare:"<<query.lastError().text();
    }

    ret = query.exec(QString("delete from sample_segmentation where segmentationid = '%1'").arg(uuid.toString()));
    if (!ret)
        qWarning()<<"prepare:"<<query.lastError().text();

    layers.removeAll(this);
}

/*
void CDTSegmentationLayer::onContextMenuRequest(QWidget *parent)
{    
    QtColorPicker *borderColorPicker = new QtColorPicker(parent);
    borderColorPicker->setStandardColors();
    borderColorPicker->setCurrentColor(borderColor());
    borderColorPicker->setToolTip(tr("Border color"));
    connect(borderColorPicker,SIGNAL(colorChanged(QColor)),SLOT(setBorderColor(QColor)));
    actionChangeBorderColor->setDefaultWidget(borderColorPicker);

    QSlider *sliderTransparency = new QSlider(Qt::Horizontal,parent);
    sliderTransparency->setMinimum(0);
    sliderTransparency->setMaximum(100);
    sliderTransparency->setValue(this->layerTransparency());
    sliderTransparency->setToolTip(tr("Layer transparency"));
    connect(sliderTransparency,SIGNAL(valueChanged(int)),SLOT(setLayerTransparency(int)));
    actionSetLayerTransparency->setDefaultWidget(sliderTransparency);

    QMenu *menu =new QMenu(parent);    
    menu->addAction(actionChangeBorderColor);
    menu->addAction(actionSetLayerTransparency);
    menu->addAction(actionRename);
    menu->addAction(actionEditDBInfo);
    menu->addAction(actionGenerateAttributes);
    menu->addAction(actionExportShapefile);
    menu->addSeparator();
    menu->addAction(actionRemoveSegmentation);
    menu->addSeparator();
    menu->addAction(actionAddClassifications);
    menu->addAction(actionRemoveAllClassifications);
    menu->addAction(actionAddDecisionFusion);
    menu->addSeparator();
    menu->exec(QCursor::pos());

    actionChangeBorderColor->releaseWidget(borderColorPicker);
    actionSetLayerTransparency->releaseWidget(sliderTransparency);
    delete borderColorPicker;
}*/

void CDTSegmentationLayer::rename()
{
    bool ok;
    QString text = QInputDialog::getText(
                NULL, tr("Input Segmentation Name"),
                tr("Segmentation rename:"), QLineEdit::Normal,
                this->name(), &ok);
    if (ok && !text.isEmpty())
        setName(text);
}

void CDTSegmentationLayer::editDBInfo()
{
    DialogDBConnection dlg(databaseURL());
    if (dlg.exec()==QDialog::Accepted)
    {
        if (dlg.dbConnectInfo() == databaseURL())
            return;
        //TODO  update AttributDockWidget
        setDatabaseURL(dlg.dbConnectInfo());
    }
}

void CDTSegmentationLayer::generateAttributes()
{
    MainWindow::getAttributesDockWidget()->clear();


    CDTDatabaseConnInfo dbConnInfo = this->databaseURL();
    QSqlDatabase db = QSqlDatabase::addDatabase(dbConnInfo.dbType,"attribute");
    db.setDatabaseName(dbConnInfo.dbName);
    db.setHostName(dbConnInfo.hostName);
    db.setPort(dbConnInfo.port);

    if (!db.open(dbConnInfo.username, dbConnInfo.password)) {
        QSqlDatabase::removeDatabase("attribute");
        QMessageBox::critical(NULL,tr("Error"),tr("Open database failed!\n information:")+db.lastError().text());
        return;
    }

    CDTImageLayer* imageLayer = (CDTImageLayer*)(parent());
    DialogGenerateAttributes dlg(this->id(),imageLayer->bandCount());
    dlg.exec();
    QSqlDatabase::removeDatabase("attribute");

    MainWindow::getAttributesDockWidget()->setCurrentLayer(this);
}

void CDTSegmentationLayer::exportShapefile()
{
    QString id = shapefilePath();
    fileSystem()->exportFiles(id);
}

void CDTSegmentationLayer::addClassification()
{
    MainWindow::getAttributesDockWidget()->clearTables();
    WizardNewClassification dlg(id());
    int ret = dlg.exec();
    MainWindow::getAttributesDockWidget()->setCurrentLayer(this);

    if (ret == QWizard::Accepted || dlg.isValid())
    {
        CDTClassificationLayer *classification = new CDTClassificationLayer(QUuid::createUuid(),this);
        classification->initClassificationLayer(
                    dlg.name,
                    dlg.method,
                    dlg.params,
                    dlg.label,
                    dlg.categoryID_Index,
                    dlg.normalizeMethod,
                    dlg.pcaParams,
                    dlg.featuresList);
        classificationRootItem->appendRow(classification->standardKeyItem());
        addClassification(classification);
    }
}

void CDTSegmentationLayer::remove()
{
    emit removeSegmentation(this);
}

void CDTSegmentationLayer::removeClassification(CDTClassificationLayer* clf)
{
    int index = classifications.indexOf(clf);
    if (index>=0)
    {
        QStandardItem* keyItem = clf->standardKeyItem();
        keyItem->parent()->removeRow(keyItem->index().row());
        classifications.remove(index);
        emit removeLayer(QList<QgsMapLayer*>()<<clf->canvasLayer());
        delete clf;
        emit layerChanged();
    }
}

void CDTSegmentationLayer::removeAllClassifications()
{
    foreach (CDTClassificationLayer* clf, classifications) {
        QStandardItem* keyItem = clf->standardKeyItem();
        keyItem->parent()->removeRow(keyItem->index().row());
        emit removeLayer(QList<QgsMapLayer*>()<<clf->canvasLayer());
        delete clf;
    }
    classifications.clear();
    emit layerChanged();
}

void CDTSegmentationLayer::decisionFusion()
{
    DialogDecisionFusion dlg(id().toString());
    dlg.exec();

    CDTClassificationLayer *classification = new CDTClassificationLayer(QUuid::createUuid(),this);
    classification->initClassificationLayer(
                dlg.name,
                dlg.method,
                dlg.params,
                dlg.label,
                dlg.categoryID_Index,
                dlg.normalizeMethod,
                dlg.pcaParams,
                dlg.featuresList);
    classificationRootItem->appendRow(classification->standardKeyItem());
    addClassification(classification);

}


QString CDTSegmentationLayer::name() const
{
    QSqlDatabase db = QSqlDatabase::database("category");
    QSqlQuery query(db);
    query.exec("select name from segmentationlayer where id ='" + this->id().toString() +"'");
    query.next();
    return query.value(0).toString();
}

QString CDTSegmentationLayer::shapefilePath() const
{
    QSqlDatabase db = QSqlDatabase::database("category");
    QSqlQuery query(db);
    query.exec("select shapefilePath from segmentationlayer where id ='" + this->id().toString() +"'");
    query.next();
    return query.value(0).toString();
}

QString CDTSegmentationLayer::markfilePath() const
{
    QSqlDatabase db = QSqlDatabase::database("category");
    QSqlQuery query(db);
    query.exec("select markfilePath from segmentationlayer where id ='" + this->id().toString() +"'");
    query.next();
    return query.value(0).toString();
}

QString CDTSegmentationLayer::shapefileTempPath() const
{
    QString temp;
    fileSystem()->getFile(shapefilePath(),temp);
    return temp;
}

QString CDTSegmentationLayer::markfileTempPath() const
{
    QString temp;
    fileSystem()->getFile(markfilePath(),temp);
    return temp;
}

QString CDTSegmentationLayer::method() const
{
    QSqlDatabase db = QSqlDatabase::database("category");
    QSqlQuery query(db);
    query.exec("select method from segmentationlayer where id ='" + this->id().toString() +"'");
    query.next();
    return query.value(0).toString();
}

CDTDatabaseConnInfo CDTSegmentationLayer::databaseURL() const
{
    QSqlDatabase db = QSqlDatabase::database("category");
    QSqlQuery query(db);
    query.exec("select dbUrl from segmentationlayer where id ='" + this->id().toString() +"'");
    query.next();
    CDTDatabaseConnInfo url = variantToData<CDTDatabaseConnInfo>(query.value(0));
    return url;
}

QColor CDTSegmentationLayer::borderColor() const
{
    QSqlDatabase db = QSqlDatabase::database("category");
    QSqlQuery query(db);
    query.exec("select bordercolor from segmentationlayer where id ='" + this->id().toString() +"'");
    query.next();
    return query.value(0).value<QColor>();
}

QString CDTSegmentationLayer::imagePath() const
{
    return ((CDTImageLayer*)parent())->path();
}

int CDTSegmentationLayer::layerTransparency() const
{
    QgsVectorLayer*p = qobject_cast<QgsVectorLayer*>(mapCanvasLayer);
    if (p)
        return p->layerTransparency();
    else
        return -1;
}

QList<QAbstractTableModel *> CDTSegmentationLayer::tableModels()
{
    QList<QAbstractTableModel *> models;
    CDTDatabaseConnInfo dbConnInfo = databaseURL();
    QSqlDatabase db = QSqlDatabase::addDatabase(dbConnInfo.dbType,"attribute");
    db.setDatabaseName(dbConnInfo.dbName);
    db.setHostName(dbConnInfo.hostName);
    db.setPort(dbConnInfo.port);

    if (!db.open(dbConnInfo.username, dbConnInfo.password)) {
        QSqlError err = db.lastError();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase("attribute");
        QMessageBox::critical(NULL,tr("Error"),tr("Open database failed!\n information:")+err.text());
        return models;
    }

    QStringList attributes = attributeNames();
    QStringList originalTables = db.tables();
    QStringList tableNames;
    foreach (QString name, originalTables) {
        if (attributes.contains(name))
            tableNames<<name;
    }
    foreach (QString tableName, tableNames) {
        QSqlTableModel* model = new QSqlTableModel(NULL,db);
        model->setProperty("name",tableName);
        model->setTable(tableName);
        model->select();
        models.append(model);
    }
    return models;
}

void CDTSegmentationLayer::setRenderer(QgsFeatureRendererV2* r)
{
    QgsVectorLayer*p = qobject_cast<QgsVectorLayer*>(mapCanvasLayer);
    if (p)
        p->setRendererV2(r);
}

void CDTSegmentationLayer::setOriginRenderer()
{
    QgsSimpleFillSymbolLayerV2* symbolLayer = new QgsSimpleFillSymbolLayerV2();
    symbolLayer->setColor(QColor(0,0,0,0));
    symbolLayer->setBorderColor(borderColor());
    QgsFillSymbolV2 *fillSymbol = new QgsFillSymbolV2(QgsSymbolLayerV2List()<<symbolLayer);
    QgsSingleSymbolRendererV2* singleSymbolRenderer = new QgsSingleSymbolRendererV2(fillSymbol);
    this->setRenderer(singleSymbolRenderer);
}

QList<CDTSegmentationLayer *> CDTSegmentationLayer::getLayers()
{
    return layers;
}

CDTSegmentationLayer *CDTSegmentationLayer::getLayer(QUuid id)
{
    foreach (CDTSegmentationLayer *layer, layers) {
        if (id == layer->uuid)
            return layer;
    }
    return NULL;
}

void CDTSegmentationLayer::setName(const QString &name)
{
    if (this->name() == name)
        return;
    QSqlQuery query(QSqlDatabase::database("category"));
    query.prepare("UPDATE segmentationlayer set name = ? where id =?");
    query.bindValue(0,name);
    query.bindValue(1,this->id().toString());
    query.exec();

    keyItem->setText(name);
    emit nameChanged(name);
}

void CDTSegmentationLayer::setBorderColor(const QColor &clr)
{
    if (this->borderColor() == clr)
        return;
    QSqlQuery query(QSqlDatabase::database("category"));
    query.prepare("UPDATE segmentationlayer set bordercolor = ? where id =?");
    query.bindValue(0,clr);
    query.bindValue(1,this->id().toString());
    query.exec();

    setOriginRenderer();
    this->mapCanvas->refresh();
    emit borderColorChanged(clr);
    emit layerChanged();
}

void CDTSegmentationLayer::setLayerTransparency(int transparency)
{
    QgsVectorLayer*p = qobject_cast<QgsVectorLayer*>(mapCanvasLayer);
    if (p)
    {
        p->setLayerTransparency(transparency);
        mapCanvas->refresh();
    }
}

void CDTSegmentationLayer::initSegmentationLayer(const QString &name,
                                                 const QString &shpPath,
                                                 const QString &mkPath,
                                                 const QString &method,
                                                 const QVariantMap &params,
                                                 CDTDatabaseConnInfo url,
                                                 const QColor &color)
{
    QString tempShpPath;
    this->fileSystem()->getFile(shpPath,tempShpPath);
    QgsVectorLayer *newLayer = new QgsVectorLayer(/*shpPath*/tempShpPath,QFileInfo(/*shpPath*/tempShpPath).completeBaseName(),"ogr");
    if (!newLayer->isValid())
    {
        QMessageBox::critical(NULL,tr("Error"),tr("Open shapefile ")+tempShpPath+tr(" failed!"));
        delete newLayer;
        return;
    }

    if (mapCanvasLayer)
    {
        QgsMapLayerRegistry::instance()->removeMapLayer(mapCanvasLayer->id());
        delete mapCanvasLayer;
    }
    mapCanvasLayer = newLayer;
    connect(newLayer,SIGNAL(layerTransparencyChanged(int)),this,SIGNAL(layerTransparencyChanged(int)));

    keyItem->setText(name);

    QgsMapLayerRegistry::instance()->addMapLayer(mapCanvasLayer);
    keyItem->setMapLayer(mapCanvasLayer);


    QSqlQuery query(QSqlDatabase::database("category"));
    bool ret ;
    ret = query.prepare("insert into segmentationlayer VALUES(?,?,?,?,?,?,?,?,?)");
    query.bindValue(0,uuid.toString());
    query.bindValue(1,name);
    query.bindValue(2,shpPath);
    query.bindValue(3,mkPath);
    query.bindValue(4,method);
    query.bindValue(5,dataToVariant(params));

    query.bindValue(6,dataToVariant(url));
    query.bindValue(7,color);
    query.bindValue(8,((CDTImageLayer*)parent())->id().toString());
    query.exec();

    //dynamic properties
    foreach (QString key, params.keys()) {
        this->setProperty((QString("   ")+key).toLocal8Bit().constData(),params.value(key.toLocal8Bit().constData()));
    }

    setOriginRenderer();

    emit nameChanged(name);
    emit borderColorChanged(color);
    emit appendLayers(QList<QgsMapLayer*>()<<mapCanvasLayer);
    emit layerChanged();
}

void CDTSegmentationLayer::setDatabaseURL(CDTDatabaseConnInfo url)
{
    //    m_dbUrl = url;
    QSqlQuery query(QSqlDatabase::database("category"));
    query.prepare("UPDATE segmentationlayer set dbUrl = ? where id =?");
    query.bindValue(0,dataToVariant(url));
    query.bindValue(1,this->id().toString());
    query.exec();
    emit layerChanged();
}

void CDTSegmentationLayer::addClassification(CDTClassificationLayer *classification)
{
    classifications.push_back(classification);
    emit layerChanged();
}

void CDTSegmentationLayer::loadSamplesFromStruct(const QMap<QString, QString> &sample_id_name, const QList<SampleElement> &samples)
{
    QSqlQuery query(QSqlDatabase::database("category"));
    query.prepare("insert into sample_segmentation values(?,?,?)");
    foreach (QString id, sample_id_name.keys()) {
        QString name = sample_id_name.value(id);
        query.bindValue(0,id);
        query.bindValue(1,name);
        query.bindValue(2,this->id().toString());
        query.exec();
    }

    query.prepare("insert into object_samples values(?,?,?)");
    foreach (SampleElement sample, samples) {
        query.bindValue(0,sample.ObjectID);
        query.bindValue(1,sample.categoryID.toString());
        query.bindValue(2,sample.sampleID.toString());
        query.exec();
    }
}

void CDTSegmentationLayer::saveSamplesToStruct(QMap<QString, QString> &sample_id_name, QList<SampleElement> &samples) const
{
    sample_id_name.clear();
    samples.clear();

    QSqlQuery query(QSqlDatabase::database("category"));
    query.exec("select id,name from sample_segmentation where segmentationid ='" + this->id().toString() +"'");
    while(query.next())
    {
        sample_id_name.insert(query.value(0).toString(),query.value(1).toString());
    }

    foreach (QString sampleID, sample_id_name.keys()) {
        query.exec("select objectid,categoryid from object_samples where sampleid ='" + sampleID+"'");
        while(query.next())
        {
            samples<<SampleElement(query.value(0).toInt(),query.value(1).toString(),sampleID);
        }
    }
}

QStringList CDTSegmentationLayer::attributeNames()
{
    QStringList list;
    foreach (CDTAttributesInterface *interface, attributesPlugins) {
        list<<interface->tableName();
    }
    return list;
}

QDataStream &operator<<(QDataStream &out, const CDTSegmentationLayer &segmentation)
{    
    QSqlQuery query(QSqlDatabase::database("category"));
    query.exec("select * from segmentationlayer where id ='" + segmentation.id().toString() +"'");
    query.next();

    out <<segmentation.id()         //id
       <<query.value(1).toString() //name
      <<query.value(2).toString() //shapfile
     <<query.value(3).toString() //markfile
    <<query.value(4).toString() //method
    <<query.value(5)//params
    <<query.value(6)//dbUrl
    <<query.value(7);//Border Color

    QMap<QString,QString> sample;
    QList<SampleElement> samples;
    segmentation.saveSamplesToStruct(sample,samples);
    out<<sample<<samples;
    out<<segmentation.classifications.size();
    for (int i=0;i<segmentation.classifications.size();++i)
        out<<*(segmentation.classifications[i]);

    return out;
}


QDataStream &operator>>(QDataStream &in,CDTSegmentationLayer &segmentation)
{
    in>>segmentation.uuid;

    QString name,shp,mark,method;
    in>>name>>shp>>mark>>method;
    QVariant temp;
    in>>temp;
    QVariantMap params = variantToData<QVariantMap>(temp);
    in>>temp;
    CDTDatabaseConnInfo url = variantToData<CDTDatabaseConnInfo>(temp);
    in>>temp;
    QColor color = temp.value<QColor>();

    segmentation.initSegmentationLayer(name,shp,mark,method,params,url,color);


    QMap<QString,QString> sample;
    QList<SampleElement> samples;
    in>>sample>>samples;
    segmentation.loadSamplesFromStruct(sample,samples);

    int count;
    in>>count;
    for (int i=0;i<count;++i)
    {
        CDTClassificationLayer* classification = new CDTClassificationLayer(QUuid(),&segmentation);
        in>>*classification;
        segmentation.classificationRootItem->appendRow(classification->standardKeyItem());
        segmentation.classifications.push_back(classification);
    }
    return in;
}
