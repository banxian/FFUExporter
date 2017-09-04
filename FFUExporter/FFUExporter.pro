#-------------------------------------------------
#
# Project created by QtCreator 2009-12-11T11:02:11
#
#-------------------------------------------------

TARGET = VIPack
TEMPLATE = app

CONFIG += qt \
    debug_and_relase
CONFIG(debug, debug|release) {
    TARGET = HunterEditor_d
    DESTDIR = ../Target/GCC/Debug
    MOC_DIR += ./tmp/moc/Debug
    OBJECTS_DIR += ./tmp/obj/Debug
}
else {
    TARGET = HunterEditor
    DESTDIR = ../Target/GCC/Release
    MOC_DIR += ./tmp/moc/Release
    OBJECTS_DIR += ./tmp/obj/Release
}
DEPENDPATH += .
UI_DIR += ./tmp
RCC_DIR += ./tmp
INCLUDEPATH += ./tmp \
    $MOC_DIR \
    . \
    ./../FvQKOLLite
LIBS += -L"./PpsDES"

SOURCES += main.cpp\
        MainUnt.cpp

HEADERS  += MainUnt.h

FORMS    += MainFrm.ui
