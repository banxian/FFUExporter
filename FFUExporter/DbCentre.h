#ifndef _DBCENTRE_H
#define _DBCENTRE_H

#include <QtSql/QSqlDatabase>
#include <QtCore/QString>

typedef struct tagPathRecItem {
    QString LastProjectFolder; // Scheme? vi?
    QString LastGlyphMatrixFolder;
    QString LastCharListFolder;
    QString LastCHSTextFolder;
    QString LastGBINFolder;
    QString LastExportFolder;
    QString LastImportFolder;
    QString LastScriptsFolder;
    QString LastCHSScriptsFolder;
    QString LastDatabasesFolder;
    QString LastCHSDatabasesFolder;
    int LastSelectedItemIndex;
} TPathRecItem;

typedef struct tagStateRecItem {
    bool WindowMaxium;
    QByteArray MainFrmState;
    QByteArray ProjectLayoutState;
    QByteArray MessageLayoutState;
    bool RegEditorMaxium;
    QByteArray RegFrmState;
    QByteArray KeyLayoutState;
} TStateRecItem;

extern TPathRecItem PathSetting;
extern TStateRecItem StateSetting;


void LoadAppSettings( void );
void SaveAppSettings( void );



#endif
