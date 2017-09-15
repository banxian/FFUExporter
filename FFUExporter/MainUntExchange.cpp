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
#include <QtCore/QDirIterator>
#include "EncodingConvUnt.h"



enum {
    xlExcel12 = 50,
    xlOpenXMLWorkbook = 51,
    xlOpenXMLWorkbookMacroEnabled = 52,
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
    QString gbinfilename = QFileDialog::getOpenFileName(this, tr("Open Big-endian GBIN File"), PathSetting.LastGBINFolder, "GBIN Files (*.gbin);;GSTR Files (*.gstr);;All Files (*.*)", 0, 0);
    if (gbinfilename.isEmpty()) {
        excel->dynamicCall("Quit()"); 
        return;
    }
    QFileInfo gbininfo(gbinfilename);
    PathSetting.LastGBINFolder = gbininfo.path();

    QFile gbinfile(gbinfilename);
    gbinfile.open(QFile::ReadOnly);
    QByteArray gbinbuf = gbinfile.readAll();
    gbinfile.close();
    bool isgstr = *(uint32_t *)gbinbuf.data() == 'LTSG';//gbinfilename.endsWith(".gstr", Qt::CaseInsensitive);

    uint32_t headeroffset = isgstr?0:(gbinbuf.size() - 0x40);
    uint32_t magic = *(uint32_t *)(gbinbuf.data() + headeroffset);

    if (magic == 'BNBG') {
        writeLog("Normalize bigendian GBIN file.", ltDebug);
        if (func_gbin_normalize_endian(gbinbuf.data(), gbinbuf.size(), 0)) {
            writeLog("Normalize endian failed.", ltError);
            return;
        }
        GBINHeaderFooter* header = (GBINHeaderFooter*)(gbinbuf.data() + headeroffset);
        *(uint32_t*)header = 'LNBG';
        header->endian_06 = 0; // from 3 to 0
    }
    const char * buffer = gbinbuf.data();
    typedef std::vector<GBINTypeDescriptor> FieldTypeVec;
    FieldTypeVec fieldtypes;
    const GBINHeaderFooter* header = (GBINHeaderFooter*)(buffer + headeroffset);
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

#ifdef _DEBUG
    excel->dynamicCall("SetVisible(bool)", true);
#else
    excel->dynamicCall("SetVisible(bool)", false);
#endif
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
    QTextCodec* codec = 0;
    TEncodingConvFrm* conv = 0;
    bool useS2312 = false;
    if (header->flags & 1) {
        codec = QTextCodec::codecForName("Shift-JIS");

        if (!codec) {
            writeLog("Shift-JIS codec is unavailable", ltError);
            qDebug() << QTextCodec::availableCodecs();
            excel->dynamicCall("Quit()"); 
            return;
        }

        conv = new TEncodingConvFrm(); // only use as data and utility function, not widget?
        QObject::connect(conv, SIGNAL(logStored(QString, TLogType)),
            this, SLOT(writeLog(QString, TLogType)));
        QMetaObject::invokeMethod(conv, "loadDebugTables"); // Qt::QueuedConnection
        // wait done?
        useS2312 = PopupQuestion(tr("Select Codec"), tr("YES for official Japanese GBIN\n NO for Localized Chinese GBIN.")) == QMessageBox::No;
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
                    QString str = useS2312?conv->Shift2312ToUnicode(QByteArray::fromRawData(ansi, 2)):codec->toUnicode(ansi);
                    range->dynamicCall("SetValue(const QVariant&)", QString("%1:'%2'").arg(val, 4, 16, QChar('0')).arg(str));
                } else {
                    range->dynamicCall("SetValue(const QVariant&)", val);
                }
            }
            if (it->type == gtSTRING) {
                //QString str = &strtables[*(uint32_t*)fieldptr];
                uint32_t offset = *(uint32_t*)fieldptr;
                QString str = useS2312?conv->Shift2312ToUnicode(QByteArray::fromRawData(&strtables[offset], strlen(&strtables[offset]))):codec->toUnicode(&strtables[offset]);
                range->dynamicCall("SetValue(const QVariant&)", QString("%1:%2").arg(offset).arg(str));
            }
        }
    }
    if (conv) {
        delete conv;
    }
    QAxObject *range = firstsheet->querySubObject("Range(const QVariant&)", QVariant(QString("%1%2:%1%2").arg(IntToColumnBase(0)).arg(2)));
    range->dynamicCall("Select()");
    QAxObject* window = excel->querySubObject("ActiveWindow");
    window->setProperty("FreezePanes", true);

    // auto fit column width
    QAxObject *columns = firstsheet->querySubObject("Columns(const QVariant&)", QVariant(QString("%1:%2").arg(IntToColumnBase(0)).arg(IntToColumnBase(header->types_count))));
    columns->dynamicCall("AutoFit()");
    
    // TODO: disable overwrite prompt or delete/backup target file?!
    excel->setProperty("DisplayAlerts", false);
    workbook->dynamicCall("SaveAs(const QString&, const QVariant&)", excelfilenamewin, (excelfilenamewin.endsWith(".xlsx", Qt::CaseInsensitive)?51:56)); // xlOpenXMLWorkbook xlExcel8

    excel->dynamicCall("SetScreenUpdating(bool)", true); 
    excel->dynamicCall("Quit()");

    delete excel;
}

void TMainFrm::onExtractDatabasesStringsClicked()
{
    QString gbinfolder = QFileDialog::getExistingDirectory(this, tr("Open Folder Contains Original Database Files"), PathSetting.LastDatabasesFolder);
    if (gbinfolder.isEmpty()) {
        return;
    }
    PathSetting.LastDatabasesFolder = gbinfolder;

    QVector <QString> databaselist;

    QDirIterator dit(gbinfolder, QStringList() << "*.gbin" << "*.gstr", QDir::Files, QDirIterator::Subdirectories);
    QString gbinfolder2 = QFileInfo(gbinfolder).absoluteFilePath(); // normalize path separator
    while (dit.hasNext()) {
        //qDebug() << dit.next();
        databaselist.push_back(dit.next());
    }
    for (QVector <QString>::const_iterator it = databaselist.begin(); it != databaselist.end(); it++) {
        int basefolderlen = gbinfolder2.length();
        if (it->startsWith(gbinfolder2)) {
            qDebug() << it->mid(basefolderlen);
        } else {
            // never got here
            qDebug() << *it;
        }

         QFile gbinfile(*it);
         gbinfile.open(QFile::ReadOnly);
         QByteArray gbinbuf = gbinfile.readAll();
         gbinfile.close();
//         bool isgstr = *(uint32_t *)gbinbuf.data() == 'LTSG';
// 
//         uint32_t headeroffset = isgstr?0:(gbinbuf.size() - 0x40);
//         uint32_t magic = *(uint32_t *)(gbinbuf.data() + headeroffset);
// 
//         if (magic == 'BNBG') {
//             writeLog(QString("Normalize bigendian GBIN file %1.").arg(it->mid(basefolderlen)), ltDebug);
//             if (func_gbin_normalize_endian(gbinbuf.data(), gbinbuf.size(), 0)) {
//                 writeLog("Normalize endian failed.", ltError);
//                 return;
//             }
//             GBINHeaderFooter* header = (GBINHeaderFooter*)(gbinbuf.data() + headeroffset);
//             *(uint32_t*)header = 'LNBG';
//             header->endian_06 = 0; // from 3 to 0
//         }
//         const char * buffer = gbinbuf.data();
//         typedef std::vector<GBINTypeDescriptor> FieldTypeVec;
//         FieldTypeVec fieldtypes;
//         const GBINHeaderFooter* header = (GBINHeaderFooter*)(buffer + headeroffset);
//         const GBINTypeDescriptor* types = (GBINTypeDescriptor*)(buffer + header->types_offset);
//         QByteArray textscontent;
//         for (int index = 0; index < header->types_count; index++, types++) {
//             fieldtypes.push_back(*types);
//         }
//         if ((header->flags & 1) == 0) {
//             continue;
//         }
//         bool iskanji = QFileInfo(*it).fileName().compare("dbKanji.gbin", Qt::CaseInsensitive) == 0;
//         const char* strtables = buffer + header->string_offset;
//         const char* structptr = buffer + header->struct_offset;
//         QTextCodec* codec = 0;
// 
//         int y = 2;
//         for (int index = 0; index < header->struct_count; index++, y++, structptr += header->struct_size) {
//             int x = 0;
//             for (FieldTypeVec::const_iterator it = fieldtypes.begin(); it != fieldtypes.end(); it++, x++) {
//                 const char* fieldptr = structptr + it->offset;
// 
//                 if (it->type == gtUINT32) {
//                     uint32_t val = *(uint32_t*)fieldptr;
//                     if (iskanji && x == 4 && val < 0xFFFF) {
//                         char ansi[3] = {val >> 8, val, 0};
//                         textscontent.append('\'');
//                         textscontent.append(ansi);
//                         textscontent.append("\'\r\n");
//                     } else {
//                         //range->dynamicCall("SetValue(const QVariant&)", val);
//                     }
//                 }
//                 if (it->type == gtSTRING) {
//                     uint32_t offset = *(uint32_t*)fieldptr;
//                     textscontent.append(&strtables[offset]);
//                     textscontent.append("\r\n");
//                 }
//             }
//         }
        GBINStore store;
        if (!store.LoadFromBuffer(gbinbuf.data(), gbinbuf.size())) {
            continue;
        }
        if (!store.ContainsText()) {
            continue;
        }

        QByteArray textscontent;
        store.ExportTranslationText(textscontent);

        QString txtfilename = (*it) + ".txt";
        QFile txtfile(txtfilename);
        txtfile.open(QFile::WriteOnly);
        txtfile.write(textscontent);
        txtfile.close();
        //break;
    }
}

void TMainFrm::onMergeDatabaseFolderClicked()
{
    QString gbinfolder = QFileDialog::getExistingDirectory(this, tr("Open Folder Contains Original Database Files"), PathSetting.LastDatabasesFolder);
    if (gbinfolder.isEmpty()) {
        return;
    }
    PathSetting.LastDatabasesFolder = gbinfolder;

    QVector <QString> databaselist;

    QDirIterator dit(gbinfolder, QStringList() << "*.gbin" << "*.gstr", QDir::Files, QDirIterator::Subdirectories);
    QString gbinfolder2 = QFileInfo(gbinfolder).absoluteFilePath(); // normalize path separator
    while (dit.hasNext()) {
        //qDebug() << dit.next();
        databaselist.push_back(dit.next());
    }
    if (databaselist.empty()) {
        return;
    }
    QString outfolder = QFileDialog::getExistingDirectory(this, tr("Save Localized Database Files To Folder"), PathSetting.LastCHSDatabasesFolder);
    if (outfolder.isEmpty()) {
        return;
    }
    PathSetting.LastCHSDatabasesFolder = outfolder; // windows separator

    TEncodingConvFrm* conv = new TEncodingConvFrm(); // only use as data and utility function, not widget?
    QObject::connect(conv, SIGNAL(logStored(QString, TLogType)),
        this, SLOT(writeLog(QString, TLogType)));
    QMetaObject::invokeMethod(conv, "loadDebugTables"); // Qt::QueuedConnection

    for (QVector <QString>::const_iterator it = databaselist.begin(); it != databaselist.end(); it++) {
        if (!it->startsWith(gbinfolder2)) {
            // never got here
            continue;
        }

        QString utf16filename = *it + "_CHS_utf16.txt";
        if (QFileInfo(utf16filename).exists() == false) {
            continue;
        }

        GBINStore store;
        QFile gbinfile(*it);
        gbinfile.open(QFile::ReadOnly);
        QByteArray gbinbuf = gbinfile.readAll();
        gbinfile.close();
        bool isgstr = *(uint32_t *)gbinbuf.data() == 'LTSG';

        uint32_t headeroffset = isgstr?0:(gbinbuf.size() - 0x40);
        uint32_t magic = *(uint32_t *)(gbinbuf.data() + headeroffset);

        int basefolderlen = gbinfolder2.length();
        if (magic == 'BNBG') {
            writeLog(QString("Normalize bigendian GBIN file %1.").arg(it->mid(basefolderlen)), ltDebug);
        }
        if (!store.LoadFromBuffer(gbinbuf.data(), gbinbuf.size())) {
            continue;
        }
        if (!store.ContainsText()) {
            continue;
        }

        ImportCHSTextToGBINStore(store, utf16filename, conv);

        QString newgbinfilename = outfolder + it->mid(basefolderlen);
        QString newdbdir = newgbinfilename.left(newgbinfilename.lastIndexOf("/"));
        QDir::root().mkpath(newdbdir);

        QByteArray s2312content;
        store.SaveToBuffer(s2312content);

        QFile ngbfile(newgbinfilename);
        ngbfile.open(QFile::WriteOnly);
        ngbfile.write(s2312content);
        ngbfile.close();
    }
    delete conv;
}

void TMainFrm::ImportCHSTextToGBINStore( GBINStore &store, const QString& utf16filename, TEncodingConvFrm* conv )
{
    QFile utf16file(utf16filename);
    utf16file.open(QFile::ReadOnly | QFile::Text);
    QTextCodec* codec = QTextCodec::codecForUtfText(utf16file.peek(4), 0);
    if (codec == 0) {
        codec = QTextCodec::codecForName("UTF-16LE");
    }
    QTextStream utf16ts(&utf16file);
    utf16ts.setCodec(codec);
    while(!utf16ts.atEnd()) {
        QString line = utf16ts.readLine();
        if (line.startsWith("//")) {
            continue;
        }
        if (line.startsWith(";=================== ")) {
            QStringList tails = line.mid(sizeof(";=================== ") - 1).remove(' ').split(':');
            int row = tails[1].toUInt();
            if (tails[2].startsWith("FIELD")) {
                unsigned int col = tails[2].mid(5).toUInt();
                QString name = utf16ts.readLine();
                store.ReplaceString(row, col, conv->UnicodeStrToShift2312Str(name));
            }
        }
    }
    utf16file.close();
}
