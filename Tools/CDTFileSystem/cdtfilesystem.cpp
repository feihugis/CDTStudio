#include "cdtfilesystem.h"
#include <QtCore>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <ogr_api.h>
#include "quazip.h"
#include "quazipfile.h"
#include "log4qt/basicconfigurator.h"

class CDTFileInfo
{
public:
    CDTFileInfo(
            QString pre = QString(),
            QString suf = QString(),
            QString filePath = QString())
        :prifix(pre),suffix(suf),path(filePath) {}

    friend class CDTFileSystem;
    friend QDataStream &operator<<(QDataStream &out, const CDTFileInfo &fileInfo);
    friend QDataStream &operator>>(QDataStream &in, CDTFileInfo &fileInfo);

    friend QDataStream &operator<<(QDataStream &out, const CDTFileSystem &files);
    friend QDataStream &operator>>(QDataStream &in, CDTFileSystem &files);

    QString fullPath()const{return prifix+path+suffix;}
private:
    QString prifix;
    QString suffix;
    QString path;
};

QDataStream &operator<<(QDataStream &out, const CDTFileInfo &fileInfo)
{
    out<<fileInfo.prifix<<fileInfo.suffix<<fileInfo.path;
    return out;
}

QDataStream &operator>>(QDataStream &in, CDTFileInfo &fileInfo)
{
    in>>fileInfo.prifix>>fileInfo.suffix>>fileInfo.path;
    return in;
}

class CDTFileSystemPrivate
{
    friend class CDTFileSystem;
    friend QDataStream &operator<<(QDataStream &out, const CDTFileSystem &files);
    friend QDataStream &operator>>(QDataStream &in, CDTFileSystem &files);

    QMap<QString,CDTFileInfo> files;
};

CDTFileSystem::CDTFileSystem(QObject *parent)
    : QObject(parent),
      pData(new CDTFileSystemPrivate)
{    
    Log4Qt::BasicConfigurator::configure();
}

CDTFileSystem::~CDTFileSystem()
{
    foreach (QString id, pData->files.keys()) {
        CDTFileInfo info = pData->files.value(id);
        qDebug()<<info.path;
        if (QFile(info.path).remove()==false)
            logger()->warn("Remove file:%1 failed!",info.path);
        else
            logger()->info("Remove file:%1 succeded!",info.path);
    }
    delete pData;
}

bool CDTFileSystem::registerFile(
        QString id,
        const QString &filePath,
        QString prefix,
        QString suffix)
{
    if (pData->files.contains(id))
    {
        logger()->error("FileID:%1 was exist in file system already!",id);
        return false;
    }
    if (!QFileInfo(filePath).isFile())
    {
        logger()->error("File:%1 was invalid!",filePath);
        return false;
    }
    pData->files.insert(id,CDTFileInfo(prefix,suffix,filePath));
    return true;
}

bool CDTFileSystem::getFile(QString id, QString &filePath)
{
    if (pData->files.contains(id)==false)
    {
        logger()->error("FileID:%1 was not exist in file system!",id);
        return false;
    }
    filePath = pData->files.value(id).fullPath();
    return true;
}

bool CDTFileSystem::GDALGetRasterVSIZipFile(const QString &srcPath, const QString &zipPath,bool deleteSrcFile)
{
    GDALAllRegister();
    GDALDataset *poDS = (GDALDataset *)GDALOpen(srcPath.toUtf8().constData(),GA_ReadOnly);
    if (poDS==NULL)
        return false;

    QuaZip zip(zipPath);
    zip.open(QuaZip::mdCreate);
    QuaZipFile outFile(&zip);

    char **fileList = poDS->GetFileList();
    GDALClose(poDS);

    int count = CSLCount(fileList);
    for(int i=0;i<count;++i)
    {
        QFileInfo fileInfo(QString::fromUtf8(fileList[i]));
        QFile inFile(srcPath);
        inFile.open(QFile::ReadOnly);
        outFile.open(QIODevice::WriteOnly, QuaZipNewInfo(fileInfo.fileName(), fileInfo.filePath()));
        outFile.write(inFile.readAll());
        inFile.close();
        if (deleteSrcFile) inFile.remove();
        outFile.close();
    }

    CSLDestroy(fileList);
    return true;
}

bool CDTFileSystem::GDALGetShapefileVSIZipFile(const QString &srcPath, const QString &zipPath,bool deleteSrcFile)
{    
    OGRRegisterAll();
    QFileInfo info(srcPath);
    if (info.completeSuffix() != "shp")
        return false;

    QStringList suffixs;
    suffixs<<".shp"<<".shx"<<".dbf"<<".sbn"<<".sbx"<<".prj"<<".idm"<<".ind"<<".qix"<<".cpg";
    for (auto iter = suffixs.begin();iter!= suffixs.end();++iter)
        (*iter) = info.completeBaseName() + *iter;
    qDebug()<<suffixs;

    QDir dir = info.dir();
    QStringList files = dir.entryList(suffixs,QDir::Files);
    qDebug()<<files;

    QuaZip zip(zipPath);
    zip.open(QuaZip::mdCreate);
    QuaZipFile outFile(&zip);
    foreach (QString file, files)
    {
        file = dir.absolutePath()+"/"+file;
        QFileInfo fileInfo(file);
        QFile inFile(file);
        inFile.open(QFile::ReadOnly);
        outFile.open(QIODevice::WriteOnly, QuaZipNewInfo(fileInfo.fileName(), fileInfo.filePath()));
        outFile.write(inFile.readAll());        
        inFile.close();
        if (deleteSrcFile) inFile.remove();
        outFile.close();
    }
    return true;
}

QDataStream &operator<<(QDataStream &out, const CDTFileSystem &files)
{
    out<<files.pData->files;

//    QMap<QUuid,QByteArray> filesData;
//    foreach (QUuid id, files.pData->files.keys()) {
//        CDTFileInfo info = files.pData->files.value(id);
//        QFile file(info.path);
//        if (!file.exists())
//        {
//            files.logger()->warn("File:%1 is not exist!",info.path);
//            continue;
//        }
//        if (!file.open(QFile::ReadOnly))
//        {
//            files.logger()->warn("Failed to open file:%1!",info.path);
//            continue;
//        }
//        filesData.insert(id,file.readAll());
//    }
//    out<<filesData;

    foreach (QString id, files.pData->files.keys()) {
        CDTFileInfo info = files.pData->files.value(id);
        QFile file(info.path);

        out<<id;
        if (!file.exists())
        {
            files.logger()->warn("File:%1 is not exist!",info.path);
            out<<QByteArray();
            continue;
        }
        if (!file.open(QFile::ReadOnly))
        {
            files.logger()->warn("Failed to open file:%1!",info.path);
            out<<QByteArray();
            continue;
        }
        out<<file.readAll();
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, CDTFileSystem &files)
{
    in>>files.pData->files;

    for (int i=0;i<files.pData->files.size();++i)
    {
        QString id;
        QByteArray data;
        in>>id>>data;
        CDTFileInfo info = files.pData->files.value(id);
        QString filePath = QDir::tempPath()+"/"+id+"."+QFileInfo(info.path).completeSuffix();
        QFile file(filePath);
        file.open(QFile::WriteOnly);
        file.write(data);

        info.path = filePath;
        files.pData->files.insert(id,info);
    }
    return in;
}
