TEMPLATE = lib
CONFIG += qt warn_on
QT -= gui

DEFINES += QUAZIP_BUILD
CONFIG(staticlib): DEFINES += QUAZIP_STATIC

DESTDIR = ../../lib
DLLDESTDIR = ../../bin

# Input
include(quazip.pri)

unix{
    LIBS += -lz
}
!unix {
    DEFINES += NOMINMAX
    include(../Config/win.pri)
    LIBS += -lzlib
}

