#ifndef MAINUNT_H
#define MAINUNT_H

#include <QMainWindow>
#include "Common.h"

namespace Ui {
    class TMainFrm;
}

class GBINStore;
class TEncodingConvFrm;

class TMainFrm : public QMainWindow {
    Q_OBJECT
private:
    //RsrcStore fRsrcStore;
    QString fLastFFUFilename;
public:
    TMainFrm(QWidget *parent = 0);
    ~TMainFrm();

protected:
    void changeEvent(QEvent *e);

private:
    Ui::TMainFrm *ui;

private:
    //void GenerateOriginalSchemeFromStore(const RsrcStore& store, const std::string& inputpath);
    //void MergeLocalizedSchemeToStore(const std::string& schemepath, RsrcStore& store);
    void PopupErrorHint(const QString& title, const QString& content);
    int PopupQuestion(const QString& title, const QString& content);
    void ImportCHSTextToGBINStore( GBINStore &store, const QString& utf16filename, TEncodingConvFrm* conv );

private slots:
    void onOpenFFUClicked();
    void onConvertFFUFilesClicked();
    void onImportRenderedImageClicked();
    void onRebuildKanjiPartClicked();
    void onStringEncodingConvClicked();
    void onGBINConvertClicked();
    void onExtractScriptsStringsClicked();
    void onGBIN2ExcelClicked();
    void onExtractDatabasesStringsClicked();
    void onMergeDatabaseFolderClicked();

public slots:
    void writeLog(QString content, TLogType logtype = ltMessage);
};

QString LogTypeToString( TLogType logtype );

#endif // MAINUNT_H
