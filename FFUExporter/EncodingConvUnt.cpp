#include <QtCore/QTextCodec>
#include "EncodingConvUnt.h"
#include "ui_StringEncodingConvFrm.h"
#include <utility>
#include "AddonFuncUnt.h"
#include "DbCentre.h"


using std::make_pair;

TEncodingConvFrm::TEncodingConvFrm(QWidget *parent) :
QWidget(parent),
ui(new Ui::TEncodingConvFrm)
{
    ui->setupUi(this);
    QPalette palette;
    palette.setColor(QPalette::Base, palette.color(QPalette::Window));
    //ui->inputFnameEdt->setPalette(palette);

    QString s2312filename = QApplication::applicationDirPath() + "/freeshift2312.txt";
    QString pscharfilename = QApplication::applicationDirPath() + "/adv2312ex_forPS_UTF16.txt";
    QString ffucharfilename = QApplication::applicationDirPath() + "/advfont29x37_charmap.txt";

    LoadTables(s2312filename, pscharfilename, ffucharfilename);
}

TEncodingConvFrm::~TEncodingConvFrm()
{
    delete ui;
}

void TEncodingConvFrm::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void TEncodingConvFrm::onLoadTableClicked()
{

}

void TEncodingConvFrm::onStringValueModified()
{
    QString unistr = ui->unicodeEdt->toPlainText();

    QString hex;
    bool bigendian = ui->bigendianChk->isChecked();

    for (int i = 0; i < unistr.length(); i++) {
        QChar qc = unistr.at(i);
        unsigned short w = qc.unicode();
        
        Uni2S2312Map::const_iterator it = fUni_to_S2312.find(w);

        if (it != fUni_to_S2312.end()) {
            unsigned short sjiscode = it->second.isS2312?it->second.s2312code:it->second.sjiscode;
            if (it->second.singlebyte) {
                hex += QString("%1 ").arg(sjiscode & 0xFF, 2, 16, QLatin1Char('0')).toUpper();
            } else {
                hex += QString("%1 %2 ").arg(bigendian?(sjiscode >> 8):(sjiscode & 0xFF), 2, 16, QLatin1Char('0')).arg(bigendian?(sjiscode & 0xFF):(sjiscode >> 8), 2, 16, QLatin1Char('0')).toUpper();
            }
        } else if (w == '\r' || w == '\n') {
            hex += qc;
        } else {
            hex += "00 00 ";
        }
    }

    ui->hexView->setPlainText(hex);
}

void TEncodingConvFrm::LoadTables( const QString& s2312filename, const QString& imagecharfilename, const QString& ffucharmapfilename )
{
    QFile charlistfile(s2312filename);
    charlistfile.open(QFile::ReadOnly);
    QByteArray charlistsbuf = charlistfile.readAll();
    charlistfile.close();

    // TODO: shiftjis vs gbk
    int s2312glyphcount = 0;
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
                fS2312_to_GlyphIndex.insert(make_pair(w, s2312glyphcount));
            } else {
                // TODO: bypass glyph
                break;
            }
        } else {
            charlist++;
            if (w == '\n' || w == '\r') {
                continue;
            }
            newchars.push_back(w);
            fS2312_to_GlyphIndex.insert(make_pair(w, s2312glyphcount));
        }

        s2312glyphcount++;
    }

    QFile pslistfile(imagecharfilename);
    pslistfile.open(QFile::ReadOnly);
    QByteArray pslistsbuf = pslistfile.readAll();
    pslistfile.close();

    int psglyphcount = 0;
    const unsigned short* pslist = (const unsigned short*)pslistsbuf.constData();
    const unsigned short* pslistend = (const unsigned short*)(pslistsbuf.constData() + pslistsbuf.size());
    while (pslist < pslistend) {
        unsigned short w = *pslist++;
        if (w == 0xFEFF || w == '\n' || w == '\r') {
            continue;
        }
        UnicodeLookupRecItem item;
        item.isS2312 = true;
        item.isSJIS = false;
        item.s2312code = newchars[psglyphcount];
        item.singlebyte = item.s2312code < 0x100;
        fUni_to_S2312.insert(make_pair(w, item));

        psglyphcount++;
    }

    QFile ffucharlistfile(ffucharmapfilename);
    ffucharlistfile.open(QFile::ReadOnly);
    QByteArray charlistsbuf2 = ffucharlistfile.readAll();
    ffucharlistfile.close();

    QTextCodec *codec = QTextCodec::codecForName("Shift-JIS");

    int ffunonkanjicount = 0;
    const unsigned char* charlist2 = (const unsigned char*)charlistsbuf2.constData();
    const unsigned char* charlist2end = (const unsigned char*)charlistsbuf2.constData() + charlistsbuf2.size();
    while (charlist2 < charlist2end) {
        // read chars
        QByteArray ansi;
        unsigned short w = *charlist2;
        if (IsXORShiftJISLeadingByte(w)) {
            // TODO: bounds check
            unsigned char c2 = charlist2[1];
            charlist2 += 2;
            if (c2 >= 0x40 && c2 <= 0xFC && c2 != 0x7F) {
                // need reverse endian
                ansi.append(w);
                ansi.append(c2);
                w = w << 8 | c2;
            } else {
                // TODO: bypass glyph
                break;
            }
        } else {
            charlist2++;
            if (w == '\n' || w == '\r') {
                continue;
            }
            ansi.append(w);
        }
        QString unistr = codec->toUnicode(ansi);
        if (unistr.length() == 1) {
            UnicodeLookupRecItem item;
            item.isS2312 = false;
            item.isSJIS = true;
            item.sjiscode = w; // in memory, LO,HI
            item.singlebyte = w < 0x100;
            unsigned short uc = unistr[0].unicode();
            
            Uni2S2312Map::iterator it = fUni_to_S2312.find(uc);
            if (it == fUni_to_S2312.end()) {
                fUni_to_S2312.insert(make_pair(uc, item));
            } else {
                it->second.isS2312 = true;
                it->second.sjiscode = w;
                if (it->second.singlebyte != item.singlebyte) {
                    AddLog(QString("singlebyte flag mismatched on U+%1 (%2)" ).arg(w, 4, 16, QLatin1Char('0')).arg(QChar(w)), ltError);
                }
            }

            ffunonkanjicount++;
        }
    }
    //delete codec;
    ui->s2312mapcountView->setText(QString("%1").arg(s2312glyphcount));
    ui->psmapcountView->setText(QString("%1").arg(psglyphcount));
    ui->stdglyphcountView->setText(QString("%1").arg(ffunonkanjicount));
}

void TEncodingConvFrm::onUTF16toShift2312Clicked()
{
    QString utf16filename = QFileDialog::getOpenFileName(this, tr("Open Translated UTF16 File"), PathSetting.LastChnTextFolder, "txt Files (*.txt);;All Files (*.*)", 0, 0);
    if (utf16filename.isEmpty()) {
        return;
    }
    PathSetting.LastChnTextFolder = QFileInfo(utf16filename).path();

    QFile utf16file(utf16filename);
    utf16file.open(QFile::ReadOnly);
    QByteArray utf16buf = utf16file.readAll();
    utf16file.close();

    unsigned short bom = *(unsigned short*)utf16buf.constData();
    if (bom != 0xFEFF && bom != 0xFFFE) {
        AddLog("No BOM found. please check your input file", ltError);
        return;
    }
    bool utf16bigendian = bom == 0xFFFE;

    QByteArray s2312buf;
    int converted = 0;
    const unsigned short* utf16ptr = (const unsigned short*)utf16buf.constData() + 1;
    const unsigned short* utf16ptrend = (const unsigned short*)(utf16buf.constData() + utf16buf.size());
    while (utf16ptr < utf16ptrend) {
        unsigned short w = *utf16ptr++;
        if (utf16bigendian){
            w = _byteswap_ushort(w);
        }

        Uni2S2312Map::const_iterator it = fUni_to_S2312.find(w);

        if (it != fUni_to_S2312.end()) {
            if (it->second.singlebyte) {
                s2312buf.append((unsigned char)it->second.isS2312?it->second.s2312code:it->second.sjiscode);
            } else {
                unsigned short sjiscode = _byteswap_ushort(it->second.isS2312?it->second.s2312code:it->second.sjiscode);
                s2312buf.append((char*)&sjiscode, 2);
            }
        } else if (w < ' ') {
            s2312buf.append((char)w);
        } else if (w == 0x2015) {
            unsigned short sjiscode = 0x5C81;
            s2312buf.append((char*)&sjiscode, 2);
        } else {
            AddLog(QString("Exception: character U+%1 (%2) out of ffu range!\n").arg(w, 4, 16, QLatin1Char('0')).arg(QChar(w)), ltError);
            return;
        }

        converted++;
    }

    QString s2312filename = QFileDialog::getSaveFileName(this, tr("Save Shift2312 File as"), PathSetting.LastChnTextFolder, "txt Files (*.txt);;All Files (*.*)", 0, 0);
    if (s2312filename.isEmpty()) {
        return;
    }
    PathSetting.LastChnTextFolder = QFileInfo(s2312filename).path();

    QFile s2312file(s2312filename);
    s2312file.open(QFile::WriteOnly);
    s2312file.write(s2312buf);
    s2312file.close();

}

void TEncodingConvFrm::onShift32toUTF16Clicked()
{

}

int __cdecl TEncodingConvFrm::logprintf(const char * _Format, ... )
{
    va_list ext;

    va_start( ext, _Format );
    //vprintf(_Format, marker);
#ifdef __GNUC__
    int len = vsnprintf(NULL, 0, _Format, ext);
    // FIXME: different than a _vsnprintf one
#else
    int len = _vscprintf(_Format, ext);
#endif
    QByteArray buf;
    buf.resize(len + 1);
    buf.fill(0);
#ifdef __GNUC__
    len = vsnprintf(buf.data(), len, _Format, ext);
#else
    len = _vsnprintf_s(buf.data(), buf.size(), len, _Format, ext);
#endif
    va_end(ext);

    AddLog(buf, ltMessage);

    return len;
}

void TEncodingConvFrm::AddLog( QString content, TLogType logtype )
{
    emit logStored(content, logtype);
}