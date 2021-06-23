#-------------------------------------------------
#
# Project created by QtCreator 2009-12-11T11:02:11
#
#-------------------------------------------------

TARGET = FFUExporter
TEMPLATE = app

CONFIG += qt \
    debug_and_relase
CONFIG(debug, debug|release) {
    TARGET = FFUExporter_d
    DESTDIR = ../Target/GCC/Debug
    MOC_DIR += ./tmp/moc/Debug
    OBJECTS_DIR += ./tmp/obj/Debug
}
else {
    TARGET = FFUExporter
    DESTDIR = ../Target/GCC/Release
    MOC_DIR += ./tmp/moc/Release
    OBJECTS_DIR += ./tmp/obj/Release
}
DEPENDPATH += .
UI_DIR += ./tmp
RCC_DIR += ./tmp
INCLUDEPATH += ./tmp \
    $MOC_DIR \
    . 

SOURCES += main.cpp\
        MainUnt.cpp\
        MainUntExchange.cpp\
        AddonFuncUnt.cpp\
        EncodingConvUnt.cpp\
        DbCentre.cpp\
        FFUObj.cpp\
        GBINObj.cpp\
        STCM2Obj.cpp

HEADERS  += MainUnt.h\
        EncodingConvUnt.h\
        AddonFuncUnt.h\
        Common.h\
        DbCentre.h\
        FFUObj.h\
        FFUTypes.h\
        GBINObj.h\
        STCM2Obj.h\
        STCM2Types.h\
        targetver.h

FORMS    += MainFrm.ui\
        ChartGenerateOptionDlg.ui\
        ImportOptionsDlg.ui\
        StringEncodingConvFrm.ui

RESOURCES += FFUExporter.qrc

win32 {
    RC_FILE += FFUExporter.rc
}
