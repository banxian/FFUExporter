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
private:
    void LoadTables(const QString& s2312file, const QString& imagecharfile, const QString& ffucharmapfile);
    int __cdecl logprintf(const char * _Format, ...);
    void AddLog(QString content, TLogType logtype = ltMessage);

signals:
    void logStored(QString content, TLogType logtype);

private slots:
    void onLoadTableClicked();
    void onStringValueModified();
    void onUTF16toShift2312Clicked();
    void onShift32toUTF16Clicked();
};

#endif