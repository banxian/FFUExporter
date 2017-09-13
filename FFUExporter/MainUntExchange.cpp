#include "MainUnt.h"
#include <ui_MainFrm.h>
#include <ActiveQt/QAxObject>
#include <QtGui/QMessageBox>
#include <QtGui/QFileDialog>
#include <QtGUI/QCleanLooksStyle>
#include <QtGUI/QProgressBar>
#include <QtGUI/QLabel>
#include "DbCentre.h"
#include "dirtyint.h"
#include "GBINObj.h"
#include <vector>
#include "AddonFuncUnt.h"
#include <QtCore/QTextCodec>
#include <QtCore/QDebug>


enum {
    xlOpenXMLWorkbook = 51,
    xlExcel8 = 56
};


void TMainFrm::onGBIN2ExcelClicked()
{
    QAxObject* excel = new QAxObject("Excel.Application", 0);
    if (excel == NULL) {
        PopupErrorHint(tr("Excel Launch Failed."), tr("FFUExporter need Excel component to open excel workbook. \nPlease install excel (97 ~ 2007) at first."));
        return;
    }
    double version = excel->property("Version").toDouble();
    QString ExcelFilter = "Excel Files (*.xls)";
    bool is2007 = version - 11.99 >= 0;
    if (is2007) {
        ExcelFilter.prepend("Excel 2007 Files (*.xlsx);;");
    }
    QString gbinfilename = QFileDialog::getOpenFileName(this, tr("Open Big-endian GBIN File"), PathSetting.LastGBINFolder, "GBIN Files (*.gbin);;All Files (*.*)", 0, 0);
    if (gbinfilename.isEmpty()) {
        return;
    }
    QFileInfo gbininfo(gbinfilename);
    PathSetting.LastGBINFolder = gbininfo.path();

    QFile gbnbfile(gbinfilename);
    gbnbfile.open(QFile::ReadOnly);
    QByteArray gbnbbuf = gbnbfile.readAll();
    gbnbfile.close();

    uint32_t magic = *(uint32_t *)(gbnbbuf.data() + gbnbbuf.size() - 0x40);

    if (magic == 'BNBG') {
        writeLog("normailize bigendian GBIN file.", ltDebug);
        if (func_gbin_normalize_endian(gbnbbuf.data(), gbnbbuf.size(), 0)) {
            writeLog("normalize endian failed.", ltError);
            return;
        }
        GBINHeaderFooter* header = (GBINHeaderFooter*)(gbnbbuf.data() + gbnbbuf.size() - 0x40);
        *(uint32_t*)header = 'LNBG';
        header->endian_06 = 0; // from 3 to 0
    }
    const char * buffer = gbnbbuf.data();
    typedef std::vector<GBINTypeDescriptor> FieldTypeVec;
    FieldTypeVec fieldtypes;
    const GBINHeaderFooter* header = (GBINHeaderFooter*)(buffer + gbnbbuf.size() - 0x40);
    const GBINTypeDescriptor* types = (GBINTypeDescriptor*)(buffer + header->types_offset);
    for (int i = 0; i < header->types_count; i++, types++) {
        fieldtypes.push_back(*types);
    }


    QString excelfilename = QFileDialog::getSaveFileName(this, tr("Save Microsoft Excel Worksheet File"), 
        QString("%1/%2.xls%3").arg(PathSetting.LastExportFolder).arg(gbininfo.completeBaseName()).arg(is2007?"x":""), QString("%1;;All Files (*.*)").arg(ExcelFilter), 0, 0
        );
    if (excelfilename.isEmpty()) {
        excel->dynamicCall("Quit()"); 
        return;
    }
    PathSetting.LastExportFolder = QFileInfo(excelfilename).path();
    bool iskanji = gbininfo.fileName().compare("dbKanji.gbin", Qt::CaseInsensitive) == 0;

    excel->dynamicCall("SetVisible(bool)", true);
    QAxObject *workbooks = excel->querySubObject("Workbooks");
    QString excelfilenamewin = excelfilename.replace('/', '\\');
    QAxObject *workbook = workbooks->querySubObject("Add()");
    QAxObject *sheets = workbook->querySubObject("Sheets"); 
    QAxObject *firstsheet = sheets->querySubObject( "Item(const QVariant&)", 1); 
    firstsheet->dynamicCall("Select()"); 

    excel->dynamicCall("SetScreenUpdating(bool)", false);

    int x = 0;
    for (FieldTypeVec::const_iterator it = fieldtypes.begin(); it != fieldtypes.end(); it++) {
        QAxObject *range = firstsheet->querySubObject("Range(const QVariant&)", QVariant(QString("%1%2:%1%2").arg(IntToColumnBase(x)).arg(1)));
        range->dynamicCall("Clear()");
        range->dynamicCall("SetValue(const QVariant&)", GBINType2Str((GBINType)it->type));
        x++;
    }
    const char* strtables = buffer + header->string_offset;
    const char* structptr = buffer + header->struct_offset;
    QTextCodec* codec;
    if (header->flags & 1) {
        codec = QTextCodec::codecForName("Shift-JIS");

        if (!codec) {
            writeLog("Shift-JIS codec is unavailable", ltError);
            qDebug() << QTextCodec::availableCodecs();
            return;
        }

    }
    int y = 2;
    for (int i = 0; i < header->struct_count; i++, y++, structptr += header->struct_size) {
        x = 0;
        for (FieldTypeVec::const_iterator it = fieldtypes.begin(); it != fieldtypes.end(); it++, x++) {
            const char* fieldptr = structptr + it->offset;
            QAxObject *range = firstsheet->querySubObject("Range(const QVariant&)", QVariant(QString("%1%2:%1%2").arg(IntToColumnBase(x)).arg(y)));
            range->dynamicCall("Clear()");
            if (it->type == gtUINT32) {
                uint32_t val = *(uint32_t*)fieldptr;
                if (iskanji && x == 4 && val < 0xFFFF) {
                    char ansi[3] = {val >> 8, val, 0};
                    QString str = codec->toUnicode(ansi);
                    range->dynamicCall("SetValue(const QVariant&)", QString("%1:'%2'").arg(val, 4, 16, QChar('0')).arg(str));
                } else {
                    range->dynamicCall("SetValue(const QVariant&)", val);
                }
            }
            if (it->type == gtSTRING) {
                //QString str = &strtables[*(uint32_t*)fieldptr];
                uint32_t offset = *(uint32_t*)fieldptr;
                QString str = codec->toUnicode(&strtables[offset]);
                range->dynamicCall("SetValue(const QVariant&)", QString("%1:%2").arg(offset).arg(str));
            }
        }
    }
    QAxObject *range = firstsheet->querySubObject("Range(const QVariant&)", QVariant(QString("%1%2:%1%2").arg(IntToColumnBase(0)).arg(2)));
    range->dynamicCall("Select()");
    QAxObject* window = excel->querySubObject("ActiveWindow");
    window->setProperty("FreezePanes", true);

    workbook->dynamicCall("SaveAs(const QString&, const QVariant&)", excelfilenamewin, (excelfilenamewin.endsWith(".xlsx", Qt::CaseInsensitive)?51:56)); // xlOpenXMLWorkbook xlExcel8

    excel->dynamicCall("SetScreenUpdating(bool)", true); 
    excel->dynamicCall("Quit()");

    delete excel;
}