#ifndef ENCODINGCONVERTERUNT_H
#define ENCODINGCONVERTERUNT_H

#include <QtGUI/QWidget>
#include <QtGUI/QFileDialog>
#include <map>
#include "Common.h"


typedef std::map<unsigned short, int> S2312GlyphIndexMap; // Index in file
typedef std::map<int, unsigned short> GlyphIndexS2312Map;

struct UnicodeLookupRecItem
{
    bool isS2312;
    bool isSJIS;
    bool singlebyte;
    unsigned short s2312code;
    unsigned short sjiscode;
};

typedef std::map<unsigned short, UnicodeLookupRecItem> Uni2S2312Map;

namespace Ui {
    class TEncodingConvFrm;
}

class STCM2Store;

class TEncodingConvFrm : public QWidget {
    Q_OBJECT
public:
    TEncodingConvFrm(QWidget *parent = 0);
    ~TEncodingConvFrm();

protected:
    void changeEvent(QEvent *e);

private:
    Ui::TEncodingConvFrm *ui;

protected:
    QFileDialog OpenSaveExeDialog, OpenSaveXipDialog;

private:
    S2312GlyphIndexMap fS2312_to_GlyphIndex;
    Uni2S2312Map fUni_to_S2312;
    unsigned short fS2312_to_Uni[256+128*256]; // ascii + halfwidth katanaka + symbols(sjis) + hanzi(s2312). have some gaps but I don't care
private:
    void LoadTables(const QString& s2312file, const QString& imagecharfile, const QString& ffucharmapfile);
    void BuildReverseLookupTable();
    int __cdecl logprintf(const char * _Format, ...);
    void AddLog(QString content, TLogType logtype = ltMessage);
    void ImportCHSTextToStore( STCM2Store &store, const QString& utf16filename );
public:
    QByteArray UnicodeStrToShift2312(const QString& str);
    std::string UnicodeStrToShift2312Str(const QString& str);
    QString Shift2312ToUnicode(const QByteArray& str);

signals:
    void logStored(QString content, TLogType logtype);

private slots:
    void onLoadTableClicked();
    void onStringValueModified();
    void onUTF16toShift2312Clicked();
    void onShift2312toUTF16Clicked();
    void onMergeSTCM2Clicked();
    void onMergeScriptFolderClicked();

    void loadDebugTables();
};

#endif