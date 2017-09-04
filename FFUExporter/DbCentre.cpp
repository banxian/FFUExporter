#include <QtGui/QApplication>
#include <QtCore/QSettings>
#include <QtCore/QFile>
#include <QtSql/QtSQL>
#include "DbCentre.h"

TPathRecItem PathSetting; // imp
TStateRecItem StateSetting; // imp

void LoadAppSettings( void )
{
    QSettings settings(QApplication::applicationDirPath() + "/ConfFFUExporter.ini",
        QSettings::IniFormat);
    PathSetting.LastProjectFolder = settings.value("Path/LastProjectFolder", "").toString();
    PathSetting.LastGlyphMatrixFolder = settings.value("Path/LastGlyphMatrixFolder", "").toString();
    PathSetting.LastCharListFolder = settings.value("Path/LastCharListFolder", "").toString();
    PathSetting.LastChnTextFolder = settings.value("Path/LastChnTextFolder", "").toString();
    PathSetting.LastGBINFolder = settings.value("Path/LastGBINFolder", "").toString();
    PathSetting.LastExportFolder = settings.value("Path/LastExportFolder", "").toString();
    PathSetting.LastImportFolder = settings.value("Path/LastImportFolder", "").toString();
    PathSetting.LastScriptsFolder = settings.value("Path/LastScriptsFolder", "").toString();
    PathSetting.LastSelectedItemIndex = settings.value("Path/LastSelectedItemIndex", 0).toInt();

    StateSetting.WindowMaxium = settings.value("State/WindowMaxium", true).toBool();
    StateSetting.MainFrmState = settings.value("State/MainFrmState", QByteArray()).toByteArray();
    StateSetting.ProjectLayoutState = settings.value("State/ProjectLayoutState", QByteArray()).toByteArray();
    StateSetting.MessageLayoutState = settings.value("State/MessageLayoutState", QByteArray()).toByteArray();

    StateSetting.RegEditorMaxium = settings.value("State/RegEditorMaxium", true).toBool();
    StateSetting.RegFrmState = settings.value("State/RegEditorMaxium", QByteArray()).toByteArray();
    StateSetting.KeyLayoutState = settings.value("State/KeyLayoutState", QByteArray()).toByteArray();
}

void SaveAppSettings( void )
{
    QSettings settings(QApplication::applicationDirPath() + "/ConfFFUExporter.ini",
        QSettings::IniFormat);
    settings.beginGroup("Path");
    settings.setValue("LastProjectFolder", PathSetting.LastProjectFolder);
    settings.setValue("LastGlyphMatrixFolder", PathSetting.LastGlyphMatrixFolder);
    settings.setValue("LastCharListFolder", PathSetting.LastCharListFolder);
    settings.setValue("LastChnTextFolder", PathSetting.LastChnTextFolder);
    settings.setValue("LastGBINFolder", PathSetting.LastGBINFolder);
    settings.setValue("LastExportFolder", PathSetting.LastExportFolder);
    settings.setValue("LastImportFolder", PathSetting.LastImportFolder);
    settings.setValue("LastScriptsFolder", PathSetting.LastScriptsFolder);
    settings.setValue("LastSelectedItemIndex", PathSetting.LastSelectedItemIndex);
    settings.endGroup();

    settings.beginGroup("State");
    settings.setValue("WindowMaxium", StateSetting.WindowMaxium);
    settings.setValue("MainFrmState", StateSetting.MainFrmState);
    settings.setValue("ProjectLayoutState", StateSetting.ProjectLayoutState);
    settings.setValue("MessageLayoutState", StateSetting.MessageLayoutState);

    settings.setValue("RegEditorMaxium", StateSetting.RegEditorMaxium);
    settings.setValue("RegFrmState", StateSetting.RegFrmState);
    settings.setValue("KeyLayoutState", StateSetting.KeyLayoutState);
    settings.endGroup();
}

