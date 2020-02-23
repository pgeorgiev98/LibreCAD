QT       += gui
TEMPLATE  = lib
CONFIG   += plugin
VERSION   = 1.0.0
TARGET    = $$qtLibraryTarget(gcode)

GENERATED_DIR = ../../generated/plugin/gcode
# Use common project definitions.
include(../../common.pri)

# For plugins
INCLUDEPATH    += ../../librecad/src/plugins

SOURCES += gcode.cpp

HEADERS += gcode.h

# Installation Directory
win32 {
        DESTDIR = ../../windows/resources/plugins
}
unix {
    macx {
        DESTDIR = ../../LibreCAD.app/Contents/Resources/plugins
    }
    else {
        DESTDIR = ../../unix/resources/plugins
    }
}
