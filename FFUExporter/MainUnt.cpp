#include "MainUnt.h"
#include "ui_MainFrm.h"
#include <QtGui/QFileDialog>
#include "DbCentre.h"
#include "AddonFuncUnt.h"
#include <assert.h>
#include <intrin.h>
#include <QtCore/QDateTime>
#include <QtGui/QPainter>
#include <vector>
#include "FFUTypes.h"
#include "FFUObj.h"
#include "ui_ChartGenerateOptionDlg.h"
#include "ui_ImportOptionsDlg.h"
#include "EncodingConvUnt.h"
#include "GBINObj.h"
#include "STCM2Obj.h"


TMainFrm::TMainFrm(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TMainFrm)
{
    ui->setupUi(this);

    LoadAppSettings();

    if (StateSetting.MainFrmState.isEmpty() == false) {
        restoreState(StateSetting.MainFrmState);
    }
    if (StateSetting.MessageLayoutState.isEmpty() == false) {
        ui->splitterforoutput->restoreState(StateSetting.MessageLayoutState);
    }

    QApplication::setApplicationName(tr("FFUExporter"));
}

TMainFrm::~TMainFrm()
{
    StateSetting.MainFrmState = saveState();
    StateSetting.MessageLayoutState = ui->splitterforoutput->saveState();

    SaveAppSettings();

    delete ui;
}

void TMainFrm::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}




//typedef std::vector<SizeRecItem> GlyphSizeVec;
//typedef std::vector<CharListRecItem> CharListVec;

void TMainFrm::onOpenFFUClicked()
{
    QString ffufilename = QFileDialog::getOpenFileName(this, tr("Open ffu Texture File"), PathSetting.LastProjectFolder, "ffu Files (*.ffu);;All Files (*.*)", 0, 0);
    if (ffufilename.isEmpty()) {
        return;
    }
    PathSetting.LastProjectFolder = QFileInfo(ffufilename).path();
    QFile file(ffufilename);
    file.open(QFile::ReadOnly);

    QByteArray contents = file.readAll();

    file.close();

    FFUHeader header = *(FFUHeader*)contents.constData(); // do copy

    if (header.magic != FUMAGIC && header.magic != UFMAGIC) {
        writeLog("missing header magic. Invalid ffu files.", ltError);
        return;
    }
    bool bigendian = header.magic == UFMAGIC;
    if (bigendian) {
        SwapFFUHeaders(&header);
        SwapFFUTables(&header, contents.data()); // quick n dirty
    }

    bool drawextrastripe = false;
    int glyphgap = 2;
    QDialog *dialog = new QDialog;

    Ui::TChartGenerateOptionsDlg optiondlg;
    optiondlg.setupUi(dialog);
    int execret = dialog->exec();
    if (execret == QDialog::Accepted) {
        drawextrastripe = optiondlg.extraStripeChk->isChecked();
        glyphgap = optiondlg.gapEdt->value();
    }
    delete dialog;

    fLastFFUFilename = ffufilename;
    setWindowTitle(QApplication::applicationName() + " - " + QFileInfo(ffufilename).fileName() + "[*]");
    setWindowModified(false);

    writeLog(QString("glyphCount:%1, charListOffset:0x%2, sizeTableOffset:0x%3, fontDataOffset:0x%4").arg(header.glyphCount).arg(header.charListOffset, 8, 16, QLatin1Char('0')).arg(header.sizeTableOffset, 8, 16, QLatin1Char('0')).arg(header.fontDataOffset, 8, 16, QLatin1Char('0')), ltMessage);
    writeLog(QString("defWidth:%1, defHeight:%2, color:%3").arg(header.defWidth).arg(header.defHeight).arg(1 << header.depth), ltMessage);


    unsigned long minwidth = 0;
    unsigned long minheight = 0;

    const SizeRecItem* glyphsizes = (const SizeRecItem*)(contents.constData() + header.sizeTableOffset);
    unsigned int glyphcount = header.glyphCount;
    while (glyphcount) {
        unsigned int w = qMin(32u, glyphcount);
        int localwidth = 0;
        int localheight = 0;
        for (unsigned int i = 0; i < w; i++) {
            // font pixel in file is aligned with 4byte(8 pixel)
            if (drawextrastripe) {
                localwidth += glyphsizes->dataSize * 2 / glyphsizes->height;
            } else {
                localwidth += glyphsizes->layoutWidth;
            }
            if (localheight < glyphsizes->height) {
                localheight = glyphsizes->height;
            }
            glyphsizes++;
        }
        glyphcount -= w;
        localwidth += (w - 1) * glyphgap;
        if (localwidth > minwidth) {
            minwidth = localwidth;
        }
        minheight += localheight + glyphgap;
    }

    if (minwidth == 0) {
        minwidth = 32;
    }
    if (minheight == 0) {
        minheight = 32;
    }

    writeLog(QString("select canvas width: %1, height: %2").arg(minwidth).arg(minheight), ltHint);

    QImage image = QImage(minwidth, minheight, QImage::Format_Indexed8);
    QImage maskimage = QImage(minwidth, minheight, QImage::Format_ARGB32);
    image.fill(16);
    maskimage.fill(0);
    QPainter maskpainter(&maskimage);
    QVector < QRgb > optscale4;

    if (header.havePalette & 1) {
        const unsigned int* ffupattle = (const unsigned int*)(contents.constData() + 0x28);
        for (unsigned i = 0; i < 16; i++) {
            optscale4.append(*ffupattle++);
        }
    } else {
        // index 0 = full transparent
        // index 15 = full opaque
        optscale4.append(0x00000000);
        optscale4.append(0xFFFF00FF);
        optscale4.append(0xFFFF00FF);
        optscale4.append(0xFFFF00FF);
        optscale4.append(0xFFFF00FF);
        optscale4.append(0xFFFF00FF);
        optscale4.append(0xFFFF00FF);
        optscale4.append(0xFFFF00FF);
        optscale4.append(0xFFFF00FF); // 8
        for (unsigned i = 0x22000000; i != 0x10000000u; i += 0x22000000) {
            optscale4.append(i | 0xFFFFFF);
        }
    }    
    optscale4.append(0x00FF00FF); // transparent pink
    image.setColorTable(optscale4);

    unsigned int bpl = image.bytesPerLine();
    uchar *dest_data = image.bits();
    //memset(dest_data, 16, minwidth * minheight); // transparent?

    unsigned int levelstat[16] = {0,};
    unsigned int levelstatfull[16] = {0,};

    glyphsizes = (const SizeRecItem*)(contents.constData() + header.sizeTableOffset);
    glyphcount = header.glyphCount;
    unsigned int desty = 0; // for qimage
    while (glyphcount) {
        unsigned int w = qMin(32u, glyphcount);
        int destx = 0; // for qimage
        int localheight = 0;

        for (unsigned int i = 0; i < w; i++) {
            // font pixel in file is aligned with 4byte(8 pixel)
            unsigned int srcwidth = glyphsizes->dataSize * 2 / glyphsizes->height;
            const unsigned char* srcptr = (const unsigned char*)contents.constData() + header.fontDataOffset + glyphsizes->dataOffset;
            if (glyphsizes->layoutWidth && glyphsizes->height) {
                maskpainter.drawRect(destx, desty, glyphsizes->layoutWidth - 1, glyphsizes->height - 1);
            }

            //uchar* destptr = dest_data + desty * bpl + destx;
            for (int y = 0; y < glyphsizes->height; y++) {
                // must loop every src to count and deal
                uchar* destptr = dest_data + (desty + y) * bpl + destx;
                for (int x = 0; x < srcwidth; x+=2) {
                    // but only draw on dest with layout width
                    if (drawextrastripe || (x < glyphsizes->layoutWidth)) {
                        *destptr++ = (*srcptr >> 4);
                        *destptr++ = (*srcptr & 0xF);
                    }
                    if (x < glyphsizes->layoutWidth) {
                        levelstat[*srcptr >> 4]++;
                        levelstat[*srcptr & 0xF]++;
                    }
                    levelstatfull[*srcptr >> 4]++;
                    levelstatfull[*srcptr & 0xF]++;

                    *srcptr++;
                }
                // bug?
                //destptr += bpl - (drawextrastripe?srcwidth:glyphsizes->layoutWidth); // nextline
            }

            // TODO: check glyphsizes bounds?
            destx += (drawextrastripe?srcwidth:glyphsizes->layoutWidth) + glyphgap;
            if (localheight < glyphsizes->height) {
                localheight = glyphsizes->height;
            }
            glyphsizes++;
        }
        glyphcount -= w;
        desty += localheight + glyphgap;
    }
    maskpainter.end();
    QImage checkedimage = image.convertToFormat(QImage::Format_ARGB32);
    ReplaceAlphaWithChecker(checkedimage);
    ui->s8iview->setPixmap(QPixmap::fromImage(checkedimage));
    ui->maskview->setPixmap(QPixmap::fromImage(maskimage));
#ifdef _DEBUG
    writeLog(QString("Levels:"), ltHint);
    for (int i = 0; i < 16; i++) {
        writeLog(QString("[%1]: %2%3").arg(i).arg(levelstat[i]).arg(levelstatfull[i]==levelstat[i]?"":QString("(+%1)").arg(levelstatfull[i]-levelstat[i])), ltHint);
    }
    QFileInfo fileinfo(ffufilename);
    image.save(fileinfo.path() + "/" + fileinfo.completeBaseName() + (drawextrastripe?"_w.png":"_n.png"), "png");
    maskimage.save(fileinfo.path() + "/" + fileinfo.completeBaseName() + (drawextrastripe?"_w_mask.png":"_n_mask.png"), "png");
    //image.convertToFormat(QImage::Format_ARGB32).save(fileinfo.path() + "/" + fileinfo.completeBaseName() + "32_n.png", "png");
    const CharListRecItem* charlists = (const CharListRecItem*)(contents.constData() + header.charListOffset);
    const CharListRecItem* charlistsEnd = (const CharListRecItem*)(contents.constData() + header.sizeTableOffset);
    QFile charmapfile(fileinfo.path() + "/" + fileinfo.completeBaseName() + "_charmap.txt");
    charmapfile.open(QFile::WriteOnly);
    QByteArray listbuf;
    while (charlists != charlistsEnd) {
        if ((charlists->charBegin < 0x100) && (charlists->charEnd <= 0x100)) {
            for (unsigned char c = charlists->charBegin; c != charlists->charEnd; c++) {
                listbuf.append(c);
            }
        } else {
            for (unsigned short w = charlists->charBegin; w != charlists->charEnd; w++) {
                unsigned short wr = _byteswap_ushort(w);
                listbuf.append((char*)&wr, sizeof(wr));
            }
        }
        charlists++;
    }
    charmapfile.write(listbuf);
    charmapfile.close();
#endif

}


void TMainFrm::onImportRenderedImageClicked()
{
    if (fLastFFUFilename.isEmpty()) {
        QString ffufilename = QFileDialog::getOpenFileName(this, tr("Open ffu Texture File"), PathSetting.LastProjectFolder, "ffu Files (*.ffu);;All Files (*.*)", 0, 0);
        if (ffufilename.isEmpty()) {
            return;
        }
        PathSetting.LastProjectFolder = QFileInfo(ffufilename).path();
        fLastFFUFilename = ffufilename;
    }

    QString pngfilename = QFileDialog::getOpenFileName(this, tr("Open Rendered Font Image"), PathSetting.LastGlyphMatrixFolder, "png Files (*.png);;All Files (*.*)", 0, 0);
    if (pngfilename.isEmpty()) {
        return;
    }
    QImage charimage(pngfilename);
    if (charimage.format() != QImage::Format_Indexed8) {
        return;
    }
    PathSetting.LastGlyphMatrixFolder = QFileInfo(pngfilename).path();
    QString charlistfilename = QFileDialog::getOpenFileName(this, tr("Open Charlist File"), PathSetting.LastCharListFolder, "txt Files (*.txt);;All Files (*.*)", 0, 0);
    if (charlistfilename.isEmpty()) {
        return;
    }
    PathSetting.LastCharListFolder = QFileInfo(charlistfilename).path();

    QFile file(fLastFFUFilename);
    file.open(QFile::ReadOnly);

    QByteArray contents = file.readAll();

    file.close();

    FFUHeader header = *(FFUHeader*)contents.constData(); // do copy

    if (header.magic != FUMAGIC) {
        writeLog("missing header magic. Invalid ffu files.", ltError);
        return;
    }

    QFile charlistfile(charlistfilename);
    charlistfile.open(QFile::ReadOnly);
    QByteArray charlistsbuf = charlistfile.readAll();
    charlistfile.close();

    // TODO: shiftjis vs gbk
    unsigned short wbegin = 0, wprev = 0;
    CharListVec addoncharlist;
    int newglyphcount = 0;
    const unsigned char* charlist = (const unsigned char*)charlistsbuf.constData();
    const unsigned char* charlistend = (const unsigned char*)charlistsbuf.constData() + charlistsbuf.size();
    while (charlist < charlistend) {
        // read chars
        unsigned short w = *charlist;
        if (IsFullShiftJISLeadingByte(w)) {
            // TODO: bounds check
            unsigned char c = charlist[1];
            // need reverse endian
            w = w << 8 | c;
            charlist += 2;
        } else {
            charlist++;
            if (w == '\n') {
                continue;
            }
        }
        if (wbegin == 0) {
            wbegin = w;
        }
        if (wprev != 0) {
            if (wprev + 1 != w || charlist == charlistend) {
                // breaked!
                CharListRecItem item;
                item.charBegin = wbegin;
                item.charEnd = wprev + 1;
                addoncharlist.push_back(item);
            }
        }
        wprev = w;
        newglyphcount++;
    }
    FFUHeader newheader = header;
    newheader.sizeTableOffset += addoncharlist.size() * sizeof(CharListRecItem);
    newheader.fontDataOffset += addoncharlist.size() * sizeof(CharListRecItem);
    newheader.fileSize += addoncharlist.size() * sizeof(CharListRecItem);
    newheader.charListCount += addoncharlist.size();
    newheader.glyphCount += newglyphcount;
    newheader.fontDataOffset += newglyphcount * sizeof(SizeRecItem);
    newheader.fileSize += newglyphcount * sizeof(SizeRecItem);
    GlyphSizeVec addonglyphsize;
    addonglyphsize.resize(newglyphcount);
    QByteArray addonfontdata;
    unsigned int srcy = 0, srcx = 0;
    const unsigned char* src_data = charimage.constBits();
    unsigned int bpl = charimage.bytesPerLine();
    int localheight = 0;
    int colcount = 0;
    for (GlyphSizeVec::iterator it = addonglyphsize.begin(); it != addonglyphsize.end(); it++) {
        it->layoutWidth = header.defWidth;
        it->height = header.defHeight;
        it->dataSize = ((it->layoutWidth + 7) / 8) * 4 * it->height;
        int alignedWidth = ((it->layoutWidth + 7) / 8) * 8;

        unsigned char* fontdata = (unsigned char*)malloc(it->dataSize);
        unsigned char* destptr = fontdata;
        //const unsigned char* srcptr = src_data + srcy * bpl + srcx;
        for (int y = 0; y < it->height; y++) {
            const unsigned char* srcptr = src_data + (srcy + y) * bpl + srcx;
            for (int x = 0; x < it->layoutWidth; x+=2) {
                // 8bit (quad bit in byte)
                *destptr++ = (srcptr[1] & 0xF) | (srcptr[0] << 4 & 0xF0);
                srcptr+=2;
            }
            for (int x = alignedWidth - it->layoutWidth; x > 0; x-=2) {
                *destptr++ = 0;
            }
            //srcptr += bpl - it->layoutWidth;
        }
        addonfontdata.append((char*)fontdata, it->dataSize);
        free(fontdata);
        srcx += it->layoutWidth;
        if (localheight < it->height) {
            localheight = it->height;
        }
        colcount++;
        // TODO: check bouns or count?
        if (colcount == 32) {
            colcount = 0;
            srcx = 0;
            srcy += localheight;
        }
    }
    newheader.fileSize += addonfontdata.size();

    bool drawextrastripe = false;

    CharListVec orig_charlist;
    int charlistcount = (header.sizeTableOffset - header.charListOffset) / sizeof(CharListRecItem);
    orig_charlist.resize(charlistcount);
    memcpy(&orig_charlist[0], contents.constData() + header.charListOffset, charlistcount * sizeof(CharListRecItem));
    orig_charlist.insert(orig_charlist.end(), addoncharlist.begin(), addoncharlist.end());

    GlyphSizeVec orig_glyphsize;
    orig_glyphsize.resize(header.glyphCount);
    memcpy(&orig_glyphsize[0], contents.constData() + header.sizeTableOffset, header.glyphCount * sizeof(SizeRecItem));
    orig_glyphsize.insert(orig_glyphsize.end(), addonglyphsize.begin(), addonglyphsize.end());


    unsigned int orig_colortable[256] = {0,};
    if (header.havePalette & 1) {
        memcpy(&orig_colortable[0], contents.constData() + 0x28, 0x400);
    }

    QByteArray newcontent;
    // TODO: fix header
    int headersize = ((newheader.havePalette&1)?0x28:0x20);
    newcontent.append((char*)&newheader, headersize);
    if (newheader.havePalette & 1) {
        newcontent.append((char*)&orig_colortable[0], 0x400);
    }
    // TODO: resort glyphsizes?
    int currdatapos = 0;
    for (GlyphSizeVec::iterator it = orig_glyphsize.begin(); it != orig_glyphsize.end(); it++) {
        it->dataOffset = currdatapos;
        currdatapos += it->dataSize;
    }
    int currindex = 0;
    for (CharListVec::iterator it = orig_charlist.begin(); it != orig_charlist.end(); it++) {
        it->mapIndex = currindex;
        currindex += it->charEnd - it->charBegin; // do not need swap?!
    }
    newcontent.append((char*)&orig_charlist[0], orig_charlist.size() * sizeof(CharListRecItem));
    newcontent.append((char*)&orig_glyphsize[0], orig_glyphsize.size() * sizeof(SizeRecItem));

    newcontent.append((char*)contents.constData() + header.fontDataOffset, header.fileSize - header.fontDataOffset); // oldheader
    newcontent.append(addonfontdata);
    //const unsigned char* srcptr = (const unsigned char*)contents.constData() + header.fontDataOffset + glyphsizes->dataOffset;
    QString ffufilename = QFileDialog::getSaveFileName(this, tr("Save ffu Texture File"), PathSetting.LastProjectFolder, "ffu Files (*.ffu);;All Files (*.*)", 0, 0);
    if (ffufilename.isEmpty()) {
        return;
    }
    PathSetting.LastProjectFolder = QFileInfo(ffufilename).path();

    QFile ffufile(ffufilename);
    ffufile.open(QFile::WriteOnly);
    ffufile.write(newcontent);
    ffufile.close();
}


void TMainFrm::onConvertFFUFilesClicked()
{
    QString ffufilename = QFileDialog::getOpenFileName(this, tr("Open ffu Texture File"), PathSetting.LastProjectFolder, "ffu Files (*.ffu);;All Files (*.*)", 0, 0);
    if (ffufilename.isEmpty()) {
        return;
    }
    PathSetting.LastProjectFolder = QFileInfo(ffufilename).path();
    fLastFFUFilename = ffufilename;

    QFile file(ffufilename);
    file.open(QFile::ReadOnly);

    QByteArray contents = file.readAll();

    file.close();

    FFUStore ffustore;
    ffustore.LoadFromBuffer(contents.constData(), contents.size());

    QByteArray newcontent;
    ffustore.SaveToBuffer(newcontent);

    ffufilename = QFileDialog::getSaveFileName(this, tr("Save ffu Texture File"), PathSetting.LastProjectFolder, "ffu Files (*.ffu);;All Files (*.*)", 0, 0);
    if (ffufilename.isEmpty()) {
        return;
    }
    PathSetting.LastProjectFolder = QFileInfo(ffufilename).path();

    QFile ffufile(ffufilename);
    ffufile.open(QFile::WriteOnly);
    ffufile.write(newcontent);
    ffufile.close();
}

enum IndexShiftMode
{
    ism0to8,
    ism0_9to15,
    ism0_2_8to15,
};

void TMainFrm::onRebuildKanjiPartClicked()
{
    QString ffufilename = QFileDialog::getOpenFileName(this, tr("Open ffu Texture File"), PathSetting.LastProjectFolder, "ffu Files (*.ffu);;All Files (*.*)", 0, 0);
    if (ffufilename.isEmpty()) {
        return;
    }
    PathSetting.LastProjectFolder = QFileInfo(ffufilename).path();
    fLastFFUFilename = ffufilename;

    QFile file(ffufilename);
    file.open(QFile::ReadOnly);

    QByteArray contents = file.readAll();

    file.close();

    FFUStore ffustore;
    ffustore.LoadFromBuffer(contents.constData(), contents.size());

    QString pngfilename = QFileDialog::getOpenFileName(this, tr("Open Rendered Font Image"), PathSetting.LastGlyphMatrixFolder, "png Files (*.png);;All Files (*.*)", 0, 0);
    if (pngfilename.isEmpty()) {
        return;
    }
    QImage charimage(pngfilename);
    if (charimage.format() != QImage::Format_Indexed8) {
        return;
    }
    PathSetting.LastGlyphMatrixFolder = QFileInfo(pngfilename).path();
    QString charlistfilename = QFileDialog::getOpenFileName(this, tr("Open Charlist File"), PathSetting.LastCharListFolder, "txt Files (*.txt);;All Files (*.*)", 0, 0);
    if (charlistfilename.isEmpty()) {
        return;
    }
    PathSetting.LastCharListFolder = QFileInfo(charlistfilename).path();


    QDialog *dialog = new QDialog;
    FFUHeader header = ffustore.GetLEHeader();


    QFile charlistfile(charlistfilename);
    charlistfile.open(QFile::ReadOnly);
    QByteArray charlistsbuf = charlistfile.readAll();
    charlistfile.close();

    // TODO: shiftjis vs gbk
    int newglyphcount = 0;
    const unsigned char* charlist = (const unsigned char*)charlistsbuf.constData();
    const unsigned char* charlistend = (const unsigned char*)charlistsbuf.constData() + charlistsbuf.size();
    std::vector<unsigned __int16> newchars;
    while (charlist < charlistend) {
        // read chars
        unsigned short w = *charlist;
        if (IsXORShiftJISLeadingByte(w)) {
            // TODO: bounds check
            unsigned char c = charlist[1];
            charlist += 2;
            if (c >= 0x40 && c <= 0xFC && c != 0x7F) {
                // need reverse endian
                w = w << 8 | c;
                newchars.push_back(w);
            } else {
                writeLog(QString("found bad character: %1%2").arg(w, 2, 16, QLatin1Char('0')).arg(c, 2, 16, QLatin1Char('0')), ltError);
                // TODO: bypass glyph
                break;
            }
        } else {
            charlist++;
            if (w == '\n') {
                continue;
            }
            newchars.push_back(w);
        }

        newglyphcount++;
    }
    writeLog(QString("charlist contains %1 chars.").arg(newglyphcount), ltHint);

    unsigned short defWidth = header.defWidth;
    unsigned short defHeight = header.defHeight;
    Ui::TImportOptionsDlg optiondlg;
    int newnewglyphcount = newglyphcount;
    int glyphgap = 2;
    IndexShiftMode pixelshiftmode = ism0to8;

    optiondlg.setupUi(dialog);
    optiondlg.fixedSizeEdt->setText(QString("%1x%2").arg(defWidth, 2, 10, QLatin1Char('0')).arg(defHeight, 2, 10, QLatin1Char('0')));
    optiondlg.destCountEdt->setText(QString("%1").arg(newglyphcount));
    optiondlg.gapEdt->setValue(glyphgap);
    optiondlg.fixedPaletteCombo->setCurrentIndex(pixelshiftmode);
    int execret = dialog->exec();
    if (execret == QDialog::Accepted) {
        QStringList sizes = optiondlg.fixedSizeEdt->text().split('x');
        if (sizes.size()) {
            defWidth = sizes[0].toUShort();
        }
        if (sizes.size() > 1) {
            defHeight = sizes[1].toUShort();
        }
        newnewglyphcount = optiondlg.destCountEdt->text().toUInt();
        glyphgap = optiondlg.gapEdt->value();
        pixelshiftmode = (IndexShiftMode)optiondlg.fixedPaletteCombo->currentIndex();
    }
    delete dialog;
    if (newnewglyphcount < newglyphcount) {
        newglyphcount = newnewglyphcount;
        writeLog(QString("use %1 glyph of charlist.").arg(newglyphcount), ltHint);
        newchars.resize(newglyphcount);
    }
    

    unsigned int srcy = 0, srcx = 0;
    const unsigned char* src_data = charimage.constBits();
    unsigned int bpl = charimage.bytesPerLine();
    int localheight = 0;
    int colcount = 0;
    for (std::vector<unsigned __int16>::iterator it = newchars.begin(); it != newchars.end(); it++) {
        GlyphDataBundle bundle;
        bundle.sizerec.layoutWidth = defWidth;
        bundle.sizerec.height = defHeight;
        bundle.sizerec.dataSize = ((bundle.sizerec.layoutWidth + 7) / 8) * 4 * bundle.sizerec.height;
        bundle.sizerec.dataOffset = 0xFFFFFFFF; // -1?
        int alignedWidth = ((bundle.sizerec.layoutWidth + 7) / 8) * 8;
        bool oddwidth = bundle.sizerec.layoutWidth & 1;

        bundle.sjiscode = *it;
        bundle.aligneddata.resize(bundle.sizerec.dataSize);

        unsigned char* fontdata = (unsigned char*)bundle.aligneddata.data();
        unsigned char* destptr = fontdata;
        //const unsigned char* srcptr = src_data + srcy * bpl + srcx;
        for (int y = 0; y < bundle.sizerec.height; y++) {
            const unsigned char* srcptr = src_data + (srcy + y) * bpl + srcx;
            int x;
            for (x = 0; x < bundle.sizerec.layoutWidth; x+=2) {
                // 8bit (quad bit in byte)
                // need check bounds if layoutWidth is odd(don't know but oddwidth can be short-ciruit)
                unsigned char p2 = (oddwidth && x == bundle.sizerec.layoutWidth - 1)?0:(srcptr[1] & 0xF);
                unsigned char pix = (p2) | (srcptr[0] << 4 & 0xF0);
                if (pixelshiftmode == ism0_9to15 && pix) {
                    if (pix & 0xF) {
                        pix |= 0x8;
                    }
                    if (pix & 0xF0) {
                        pix |= 0x80;
                    }
                }
                *destptr++ = pix;
                srcptr+=2;
            }
            // TODO: odd/even?
            for ( ;x < alignedWidth; x+=2) {
                *destptr++ = 0;
            }
            //srcptr += bpl - it->layoutWidth;
        }
        ffustore.AddGlyph(*it, bundle);
        srcx += bundle.sizerec.layoutWidth + glyphgap;
        if (localheight < bundle.sizerec.height) {
            localheight = bundle.sizerec.height;
        }
        colcount++;
        // TODO: check bouns or count?
        if (colcount == 32) {
            colcount = 0;
            srcx = 0;
            srcy += localheight + glyphgap;
        }
    }

    QByteArray newcontent;
    ffustore.SaveToBuffer(newcontent);

    ffufilename = QFileDialog::getSaveFileName(this, tr("Save ffu Texture File as"), PathSetting.LastProjectFolder, "ffu Files (*.ffu);;All Files (*.*)", 0, 0);
    if (ffufilename.isEmpty()) {
        return;
    }
    PathSetting.LastProjectFolder = QFileInfo(ffufilename).path();

    QFile ffufile(ffufilename);
    ffufile.open(QFile::WriteOnly);
    ffufile.write(newcontent);
    ffufile.close();
}


void TMainFrm::onStringEncodingConvClicked()
{
    TEncodingConvFrm *converterfrm = new TEncodingConvFrm(); // Top one?

    QObject::connect(converterfrm, SIGNAL(logStored(QString, TLogType)),
        this, SLOT(writeLog(QString, TLogType)));

    converterfrm->show();
}


void TMainFrm::onGBINConvertClicked()
{
    QString gbinfilename = QFileDialog::getOpenFileName(this, tr("Open Big-endian GBIN File"), PathSetting.LastGBINFolder, "GBIN Files (*.gbin);;All Files (*.*)", 0, 0);
    if (gbinfilename.isEmpty()) {
        return;
    }
    PathSetting.LastGBINFolder = QFileInfo(gbinfilename).path();

    QFile gbnbfile(gbinfilename);
    gbnbfile.open(QFile::ReadOnly);
    QByteArray gbnbbuf = gbnbfile.readAll();
    gbnbfile.close();

    unsigned __int32 magic = *(unsigned __int32*)(gbnbbuf.data() + gbnbbuf.size() - 0x40);

    if (magic != 'BNBG') {
        writeLog("not a bigendian GBIN file.", ltDebug);
        return;
    }

    if (func_gbin_normalize_endian(gbnbbuf.data(), gbnbbuf.size(), 0)) {
        writeLog("normalize endian failed.", ltError);
        return;
    }
    GBINHeaderFooter* header = (GBINHeaderFooter*)(gbnbbuf.data() + gbnbbuf.size() - 0x40);
    *(unsigned __int32*)header = 'LNBG';
    header->endian_06 = 0; // from 3 to 0

    gbinfilename = QFileDialog::getSaveFileName(this, tr("Save Little-endian GBIN File as"), PathSetting.LastGBINFolder, "GBIN Files (*.gbin);;All Files (*.*)", 0, 0);
    if (gbinfilename.isEmpty()) {
        return;
    }
    PathSetting.LastGBINFolder = QFileInfo(gbinfilename).path();

    QFile gbnlfile(gbinfilename);
    gbnlfile.open(QFile::WriteOnly);
    gbnlfile.write(gbnbbuf);
    gbnlfile.close();

}


void TMainFrm::onExtractScriptsStringsClicked()
{
    QString stcmfolder = QFileDialog::getExistingDirectory(this, tr("Open Folder Contains Original Script Files"), PathSetting.LastScriptsFolder);
    if (stcmfolder.isEmpty()) {
        return;
    }
    PathSetting.LastScriptsFolder = stcmfolder;

    QVector <QString> scriptlist;

    QDirIterator dit(stcmfolder, QStringList() << "*.DAT", QDir::Files, QDirIterator::Subdirectories);
    while (dit.hasNext()) {
        //qDebug() << dit.next();
        scriptlist.push_back(dit.next());
    }
    for (QVector <QString>::const_iterator it = scriptlist.begin(); it != scriptlist.end(); it++) {
        STCM2Store store;
        QFile file(*it);
        int stcmfolderlen = stcmfolder.length();
        if (it->startsWith(stcmfolder)) {
            qDebug() << it->mid(stcmfolderlen);
        } else {
            // never got here
            qDebug() << *it;
        }

        file.open(QFile::ReadOnly);

        QByteArray contents = file.readAll();

        file.close();
        if (!store.LoadFromBuffer(contents.constData(), contents.size())) {
            continue;
        }
        if (!store.ContainsDialog()) {
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


//////////////////////////////////////////////////////////////////////////
// End of world
void TMainFrm::writeLog( QString content, TLogType logtype /*= ltMessage*/ )
{
    if (content.isEmpty()) {
        content = "NULL";
    }
    QStringList list;
    list = content.split("\n");
    for (QStringList::iterator it = list.begin(); it != list.end(); it++) {
        if ((*it).isEmpty() && it + 1 == list.end()) {
            break;
        }
        int prevrowcount = ui->logView->rowCount();
        ui->logView->setRowCount(prevrowcount + 1);
        QTableWidgetItem *item;
        item = new QTableWidgetItem(QDateTime::currentDateTime().toString("hh:mm:ss"));
        ui->logView->setItem(prevrowcount, 0, item);
        item = new QTableWidgetItem(LogTypeToString(logtype));
        ui->logView->setItem(prevrowcount, 1, item);
        item = new QTableWidgetItem(*it);
        ui->logView->setItem(prevrowcount, 2, item);

        ui->logView->scrollToItem(item);
    }
    if (ui->logView->rowCount() < 80) {
        //ui->logView->resizeRowsToContents();
        //ui->logView->resizeColumnsToContents();
    }
}

QString LogTypeToString( TLogType logtype )
{
    switch (logtype)
    {
    case ltHint:
        return "Hint";
        break;
    case ltDebug:
        return "Debug";
        break;
    case ltMessage:
        return "Message";
        break;
    case ltError:
        return "Error";
        break;
    }
    return "Unkown";
}
