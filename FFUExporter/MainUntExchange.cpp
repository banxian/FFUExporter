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
    bool isgstr = *(uint32_t *)gbinbuf.data() == 'LTSG' || *(uint32_t *)gbinbuf.data() == 'BTSG';//gbinfilename.endsWith(".gstr", Qt::CaseInsensitive);

    uint32_t headeroffset = isgstr?0:(gbinbuf.size() - 0x40);
    uint32_t magic = *(uint32_t *)(gbinbuf.data() + headeroffset);

    if (magic == 'BNBG' || magic == 'BTSG') {
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
    QTextCodec* codec = 0;
    TEncodingConvFrm* conv = 0;
    bool useS2312 = false;
    if (header->flags & 1) {
        useS2312 = PopupQuestion(tr("Select Codec"), tr("YES for official Japanese GBIN\n NO for Localized Chinese GBIN.")) == QMessageBox::No;
        if (!useS2312) {
            codec = QTextCodec::codecForName("Shift-JIS");

            if (!codec) {
                writeLog("Shift-JIS codec is unavailable", ltError);
                qDebug() << QTextCodec::availableCodecs();
                excel->dynamicCall("Quit()"); 
                return;
            }
        } else {
            conv = new TEncodingConvFrm(); // only use as data and utility function, not widget?
            QObject::connect(conv, SIGNAL(logStored(QString, TLogType)),
                this, SLOT(writeLog(QString, TLogType)));
            QMetaObject::invokeMethod(conv, "loadDebugTables"); // Qt::QueuedConnection
            // wait done?
        }
    }

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
    int y = 2;
    for (int i = 0; i < header->struct_count; i++, y++, structptr += header->struct_size) {
        x = 0;
        for (FieldTypeVec::const_iterator it = fieldtypes.begin(); it != fieldtypes.end(); it++, x++) {
            const char* fieldptr = structptr + it->offset;
            QAxObject *range = firstsheet->querySubObject("Range(const QVariant&)", QVariant(QString("%1%2:%1%2").arg(IntToColumnBase(x)).arg(y)));
            range->dynamicCall("Clear()");
            if (it->type == gtULONG) {
                uint32_t val = *(uint32_t*)fieldptr;
                if (iskanji && x == 4 && val < 0xFFFF) {
                    char ansi[3] = {val >> 8, val, 0};
                    QString str = useS2312?conv->Shift2312ToUnicode(QByteArray::fromRawData(ansi, 2)):codec->toUnicode(ansi);
                    range->dynamicCall("SetValue(const QVariant&)", QString("''%1'").arg(str));//QString("%1:'%2'").arg(val, 4, 16, QChar('0')).arg(str));
                } else {
                    range->dynamicCall("SetValue(const QVariant&)", val);
                }
            }
            if (it->type == gtSTRING) {
                uint32_t offset = *(uint32_t*)fieldptr;
                QString str = useS2312?conv->Shift2312ToUnicode(QByteArray::fromRawData(&strtables[offset], strlen(&strtables[offset]))):codec->toUnicode(&strtables[offset]);
                range->dynamicCall("SetValue(const QVariant&)", str);//QString("%1:%2").arg(offset).arg(str));
            }
            if (it->type == gtFLOAT) {
                // float32_t?
                float val = *(float*)fieldptr;
                range->dynamicCall("SetValue(const QVariant&)", (double)val);
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

void TMainFrm::onExcel2GBINClicked()
{
    QAxObject* excel = new QAxObject("Excel.Application", 0);
    if (excel == NULL) {
        PopupErrorHint(tr("Excel Launch Failed."), tr("FFUExporter need Excel component to open excel workbook. \nPlease install excel (97 ~ 2007) at first."));
        return;
    }
    double version = excel->property("Version").toDouble();
    QString extfilter = "*.xls";
    if (version - 11.99 >= 0) {
        extfilter.prepend("*.xlsx;");
    }
    QString excelfilename = QFileDialog::getOpenFileName(this, tr("Open Microsoft Excel Worksheet File"), PathSetting.LastImportFolder, QString("Excel Files (%1);;All Files (*.*)").arg(extfilter), 0, 0);
    if (excelfilename.isEmpty()) {
        excel->dynamicCall("Quit()"); 
        return;
    }
    PathSetting.LastImportFolder = QFileInfo(excelfilename).path();

    //excel->dynamicCall("SetVisible(bool)", true);
    QAxObject *workbooks = excel->querySubObject( "Workbooks" );
    QString excelfilenamewin = excelfilename.replace('/', '\\');
    QAxObject *workbook = workbooks->querySubObject("Open(const QString&)", excelfilenamewin); 
    //int workbookformat =  workbook->property("FileFormat").toInt();
    QAxObject *sheets = workbook->querySubObject("Sheets"); 
    QAxObject *firstsheet = sheets->querySubObject("Item(const QVariant&)", 1); // QVariant(QString::fromLocal8Bit("sheet1"))
    firstsheet->dynamicCall("Select()"); 

    excel->dynamicCall("SetScreenUpdating(bool)",false); 

    QAxObject *usedrange = firstsheet->querySubObject("UsedRange()");
    if (usedrange == NULL) {
        PopupErrorHint(tr("Empty Sheet"), tr("Your Excel workbook doesn't have any data."));
        excel->dynamicCall("Quit()");
        return;
    }
    const int rowcount = usedrange->querySubObject("Rows()")->dynamicCall("Count").toInt();
    const int columncount = usedrange->querySubObject("Columns()")->dynamicCall("Count").toInt();

    typedef std::vector<GBINTypeDescriptor> FieldTypeVec;
    FieldTypeVec fieldtypes;
    QAxObject *cells = firstsheet->querySubObject("Cells()");
    // first row must be field type
    QByteArray fieldbuf;
    int fieldoffset = 0;
    bool havetext = false;
    for (int x = 0; x < columncount; x++) {
        QAxObject *cell = cells->querySubObject("Item(const QVariant&, const QVariant&)", QVariant(1), QVariant(x + 1));
        QVariant cellvalue = cell->dynamicCall("Value(QVariant)", 10);
        GBINTypeDescriptor field;
        field.type = GBINTypeFromStr(cellvalue.toString().toAscii());
        field.offset = fieldoffset;
        fieldtypes.push_back(field);
        switch (field.type) {
        case gtULONG:
        case gtSTRING:
            fieldoffset += 4;
            break;
        case gtFLOAT:
        case gtFIXINT32:
            fieldoffset += 4;
            break;
        case gtUINT64:
            fieldoffset += 8;
            break;
        case gtUINT16:
            fieldoffset += 2;
            break;
        case gtUINT8:
            fieldoffset += 1;
            break;
        }
        fieldbuf.append((char*)&field, sizeof(GBINTypeDescriptor));
    }

    TEncodingConvFrm* conv = 0;
    QTextCodec* codec = 0;
    bool useS2312 = PopupQuestion(tr("Select Codec"), tr("YES for official Japanese Excel\n NO for Localized Chinese Excel.")) == QMessageBox::No;
    if (!useS2312) {
        codec = QTextCodec::codecForName("Shift-JIS");

        if (!codec) {
            writeLog("Shift-JIS codec is unavailable", ltError);
            qDebug() << QTextCodec::availableCodecs();
            excel->dynamicCall("Quit()"); 
            return;
        }
    } else {
        conv = new TEncodingConvFrm(); // only use as data and utility function, not widget?
        QObject::connect(conv, SIGNAL(logStored(QString, TLogType)),
            this, SLOT(writeLog(QString, TLogType)));
        QMetaObject::invokeMethod(conv, "loadDebugTables"); // Qt::QueuedConnection
        // wait done?
    }

    typedef QHash<QByteArray, unsigned int> StrOffsetMap;
    QByteArray stringtable, recordsbuf, buf;
    StrOffsetMap stroffmap;
    int strpos = 0;

    for (int y = 2; y < rowcount + 1; y++) {
        for (int x = 0; x < columncount; x++) {
            QAxObject *cell = cells->querySubObject("Item(const QVariant&, const QVariant&)", QVariant(y), QVariant(x + 1));
            QVariant cellvalue = cell->dynamicCall("Value(QVariant)", 10);
            switch (fieldtypes[x].type) {
            case gtULONG:
                {
                    uint32_t u32rec = 0;
                    QString strval = cellvalue.toString();
                    if (strval.startsWith('\'') && strval.endsWith('\'')) {
                        strval = strval.mid(1, strval.length() - 2);
                        if (!strval.isEmpty()) {
                            QByteArray ansi = useS2312?conv->UnicodeStrToShift2312(strval):codec->fromUnicode(strval);
                            u32rec = _byteswap_ushort(*(uint16_t*)ansi.data());
                        }
                    } else {
                        u32rec = strval.toUInt();
                    }
                    recordsbuf.append((char*)&u32rec, sizeof(uint32_t));
                }
                break;
            case gtSTRING:
                {
                    QByteArray strrec = useS2312?conv->UnicodeStrToShift2312(cellvalue.toString()):codec->fromUnicode(cellvalue.toString());//conv->UnicodeStrToShift2312(cellvalue.toString());
                    StrOffsetMap::const_iterator sit = stroffmap.find(strrec);
                    uint32_t u32rec;
                    if (sit == stroffmap.end()) {
                        u32rec = strpos;
                        stringtable.append(strrec);
                        stringtable.append((char)0); // always
                        stroffmap.insert(strrec, strpos);
                        strpos += strrec.size() + 1;
                    } else {
                        u32rec = sit.value();
                    }
                    recordsbuf.append((char*)&u32rec, sizeof(uint32_t));
                }
                break;
            case gtFLOAT:
                {
                    float val = cellvalue.toDouble();
                    recordsbuf.append((char*)&val, sizeof(float));
                }
                break;
            }
        }
    }

    workbooks->dynamicCall("Close()");
    excel->dynamicCall("SetScreenUpdating(bool)", true); 
    excel->dynamicCall("Quit()");

    QString gbinfilename = QFileDialog::getSaveFileName(this, tr("Save Shift2312 GBIN File as"), PathSetting.LastExportFolder, "GBIN Files (*.gbin);;GSTR Files (*.gstr);;All Files (*.*)", 0, 0);
    if (gbinfilename.isEmpty()) {
        return;
    }
    PathSetting.LastExportFolder = QFileInfo(gbinfilename).path();

    GBINHeaderFooter newheader;
    memset(&newheader, 0, sizeof(newheader));
    newheader.field_04 = 1;
    newheader.field_08 = 16;
    newheader.longsize = 4;
    newheader.ptrsize = 4;
    int stringtablesize = ((stringtable.size() + 15) / 16)*16;
    int recordsbufsize = ((recordsbuf.size() + 15) / 16)*16;
    int fielddefsize = ((sizeof(GBINTypeDescriptor) * fieldtypes.size() + 15)/16)*16;
    newheader.struct_count = rowcount - 1;
    newheader.struct_size = fieldoffset;
    newheader.types_count = fieldtypes.size();
    if (stringtablesize) {
        newheader.flags |= 1;
        newheader.string_count = stroffmap.size();
    }

    paddingqbytearray(recordsbuf, recordsbufsize);
    paddingqbytearray(fieldbuf, fielddefsize);
    paddingqbytearray(stringtable, stringtablesize);

    if (gbinfilename.endsWith(".gbin", Qt::CaseInsensitive)) {
        *(uint32_t*)newheader.magic = 'LNBG';
        // Records + FieldDefine + StringTable + Header
        newheader.struct_offset = 0;
        newheader.types_offset = recordsbufsize;
        newheader.string_offset = stringtablesize?recordsbufsize+fielddefsize:0;
        buf.append(recordsbuf);
        buf.append(fieldbuf);
        buf.append(stringtable);
        buf.append((char*)&newheader, sizeof(newheader));
    } else {
        *(uint32_t*)newheader.magic = 'LTSG';
        // Header + Records + FieldDefine + StringTable
        newheader.struct_offset = sizeof(GBINHeaderFooter);
        newheader.types_offset = sizeof(GBINHeaderFooter) + recordsbufsize;
        newheader.string_offset = stringtablesize?sizeof(GBINHeaderFooter)+recordsbufsize+fielddefsize:0;
        buf.append((char*)&newheader, sizeof(newheader));
        buf.append(recordsbuf);
        buf.append(fieldbuf);
        buf.append(stringtable);
    }

    QFile gbinfile(gbinfilename);
    gbinfile.open(QFile::WriteOnly);
    gbinfile.write(buf);
    gbinfile.close();

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
