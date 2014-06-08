#include "mainwindow.h"
#include <qgsapplication.h>
#include "stable.h"
#include "cdtpluginloader.h"
#include "cdtsegmentationInterface.h"
#include "cdtattributesinterface.h"
#include "cdtclassifierinterface.h"
#include <stxxl.h>

QList<CDTSegmentationInterface *>   segmentationPlugins;
QList<CDTAttributesInterface *>     attributesPlugins;
QList<CDTClassifierInterface *>     classifierPlugins;

//TODO  File system
//TODO  Some QButtonGroup
//TODO  Project tree Checkbox
//TODO  Translation
//TODO  CDTBaseObject more independent
//TODO  Segmentation plugins
//TODO  Recent file
//TODO  Segmentation Border Color
//TODO  AttributeTable fields
//TODO  Log System
//TODO  Comment
//TODO  Unit Test(Auto)

//BUG   Same name
//BUG   Attribute dockwidget size
//BUG   PCA and transform information to show

bool initDatabase()
{
    /// Add database
    if (QSqlDatabase::drivers().contains("QSQLITE")==false)
    {
        QMessageBox::critical(NULL,QObject::tr("Error"),QObject::tr("Can't find QSQLITE driver, this application couldn't run!"));
        return false;
    }
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE","category");

    QString dbPath;
    dbPath = ":memory:";
    //    dbPath = "C:/Users/cgz/Documents/data/lalalla.db";

    //    QTemporaryFile dbFile;
    //    dbFile.open();
    //    dbPath = dbFile.fileName();
    //    dbFile.close();

    db.setDatabaseName(dbPath);
    if (!db.open())
    {
        QMessageBox::warning(NULL, QObject::tr("Unable to open database"),
                             QObject::tr("An error occurred while opening the connection: ")
                             + db.lastError().text());
        return false;
    }

    QSqlQuery query(db);
    bool ret ;

    ///  Create project table(id text,name text).
    ret = query.exec("CREATE TABLE project (id text NOT NULL, name text NOT NULL,Primary Key(id) )");
    if (ret == false)
    {
        QMessageBox::critical(NULL,QObject::tr("Error"),QObject::tr("create table project failed!\nerror:")+query.lastError().text());
        return false;
    }

    ///  Create image layer table(id text,name text,path text,projectID text).
    ret = query.exec("CREATE TABLE imagelayer (id text NOT NULL, name text NOT NULL,path text NOT NULL,projectID text NOT NULL,Primary Key(id) )");
    if (ret == false)
    {
        QMessageBox::critical(NULL,QObject::tr("Error"),QObject::tr("create table imagelayer failed!\nerror:")+query.lastError().text());
        return false;
    }

    ///  Create segmentation layer table(id text,name text,shapefilePath text,markfilePath text,imageID text).
    ret = query.exec("CREATE TABLE segmentationlayer"
                     "(id text NOT NULL, "
                     "name text NOT NULL,"
                     "shapefilePath text NOT NULL,"
                     "markfilePath text NOT NULL,"
                     "imageID text NOT NULL,"
                     "Primary Key(id) )");
    if (ret == false)
    {
        QMessageBox::critical(NULL,QObject::tr("Error"),QObject::tr("create table segmentationlayer failed!\nerror:")+query.lastError().text());
        return false;
    }

    ///  Create classification layer table(id text,name text,data blob,clsinfo blob,segmentationID text).
    ret = query.exec("CREATE TABLE classificationlayer"
                     "(id text NOT NULL, "
                     "name text NOT NULL,"
                     "method text NOT NULL,"
                     "params blob,"
                     "data blob,"
                     "clsinfo blob,"
                     "segmentationID text NOT NULL,"
                     "Primary Key(id) )");
    if (ret == false)
    {
        QMessageBox::critical(NULL,QObject::tr("Error"),QObject::tr("create table classificationlayer failed!\nerror:")+query.lastError().text());
        return false;
    }

    /// Create category table(name text,color blob,imageID text).
    ret = query.exec("CREATE TABLE category "
                     "(id text NOT NULL,"
                     "name text NOT NULL, "
                     "color blob, "
                     "imageID text NOT NULL,"
                     "Primary Key(id),"
                     "UNIQUE (name,imageID) )");
    if (ret == false)
    {
        QMessageBox::critical(NULL,QObject::tr("Error"),QObject::tr("create table category failed!\nerror:")+query.lastError().text());
        return false;
    }


    ///  Create segmentation sample_segmentation table(id text,name text,shapefilePath text,markfilePath text,imageID text).
    ret = query.exec("CREATE TABLE sample_segmentation"
                     "(id text NOT NULL, "
                     "name text NOT NULL, "
                     "segmentationid text NOT NULL,"
                     "Primary Key(id) )");
    if (ret == false)
    {
        QMessageBox::critical(NULL,QObject::tr("Error"),QObject::tr("create table sample_segmentation failed!\nerror:")+query.lastError().text());
        return false;
    }

    ///  Create segmentation samples table(id text,name text,shapefilePath text,markfilePath text,imageID text).
    ret = query.exec("CREATE TABLE samples"
                     "(objectid text NOT NULL, "
                     "categoryid text NOT NULL,"
                     "sampleid text NOT NULL,"
                     "Primary Key(objectid,categoryid,sampleid) )");
    if (ret == false)
    {
        QMessageBox::critical(NULL,QObject::tr("Error"),QObject::tr("create table samples failed!\nerror:")+query.lastError().text());
        return false;
    }

    return true;
}

void clearDatabase()
{
    QSqlDatabase db = QSqlDatabase::database("category");
    QString databaseName = db.databaseName();
    db.removeDatabase("category");
    if (databaseName != ":memory:")
    {
        QFile::remove(databaseName);
    }
}

#ifdef Q_OS_WIN
void initStxxl()
{
    stxxl::config * cfg = stxxl::config::get_instance();
    stxxl::disk_config disk((QDir::tempPath()+"\\cdtstudio_stxxl").toLocal8Bit().constData(), 800 * 1024 * 1024, "wincall delete");
    cfg->add_disk(disk);
}
#endif

int main(int argc, char *argv[])
{
    QgsApplication a(argc, argv,true);

    QTranslator appTranslator;
    appTranslator.load(":/trans/" + QLocale::system().name()+".qm");
    a.installTranslator(&appTranslator);

    QgsApplication::setPluginPath(QDir::currentPath()+"/Plugins");
    QgsApplication::initQgis();

    if (initDatabase()==false)
        return 0;

#ifdef Q_OS_WIN
    initStxxl();
#endif

    MainWindow w;
    //    w.showMaximized();
    w.show();

    segmentationPlugins = CDTPluginLoader<CDTSegmentationInterface>::getPlugins();
    attributesPlugins   = CDTPluginLoader<CDTAttributesInterface>::getPlugins();
    classifierPlugins   = CDTPluginLoader<CDTClassifierInterface>::getPlugins();

    int ret = a.exec();

    clearDatabase();

    return ret;
}
