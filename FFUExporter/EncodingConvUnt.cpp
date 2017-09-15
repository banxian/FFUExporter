#include <QtCore/QTextCodec>
#include "EncodingConvUnt.h"
#include "ui_StringEncodingConvFrm.h"
#include <utility>
#include "AddonFuncUnt.h"
#include "DbCentre.h"
#include "STCM2Obj.h"


using std::make_pair;

TEncodingConvFrm::TEncodingConvFrm(QWidget *parent) :
QWidget(parent),
ui(new Ui::TEncodingConvFrm)
{
    ui->setupUi(this);
    QPalette palette;
    palette.setColor(QPalette::Base, palette.color(QPalette::Window));
    //ui->inputFnameEdt->setPalette(palette);
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

void TEncodingConvFrm::loadDebugTables()
{
    QString s2312filename = QApplication::applicationDirPath() + "/freeshift2312.txt";
    QString pscharfilename = QApplication::applicationDirPath() + "/adv2312ex_forPS_UTF16.txt";
    QString ffucharfilename = QApplication::applicationDirPath() + "/advfont29x37_charmap.txt";

    LoadTables(s2312filename, pscharfilename, ffucharfilename);
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
    // TODO: build a custom QTextCodec
    QFile s2312charlistfile(s2312filename); // charlist for shift2312 FFU. only contains hanzi parts. generate by 010Script. lead byte 88..9F,E0..FC b2 40..7E,80..FC
    s2312charlistfile.open(QFile::ReadOnly);
    QByteArray scharlistsbuf = s2312charlistfile.readAll();
    s2312charlistfile.close();

    // TODO: shiftjis vs gbk
    int s2312glyphcount = 0;
    const unsigned char* charlist = (const unsigned char*)scharlistsbuf.constData();
    const unsigned char* charlistend = (const unsigned char*)scharlistsbuf.constData() + scharlistsbuf.size();
    std::vector<unsigned __int16> newchars;
    // freeshift2312.txt by 010Script
    while (charlist < charlistend) {
        // read chars
        unsigned short w = *charlist;
        if (IsXORShiftJISLeadingByte(w)) {
            // TODO: bounds check
            unsigned char b2 = charlist[1];
            charlist += 2;
            if (b2 >= 0x40 && b2 <= 0xFC && b2 != 0x7F) {
                // need reverse endian
                w = w << 8 | b2;
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

    QFile pslistfile(imagecharfilename); // source list use to produce shift2312 ffu. every character is correspond to ffu's glyph.
    pslistfile.open(QFile::ReadOnly);
    QByteArray pslistsbuf = pslistfile.readAll(); // unicode
    pslistfile.close();

    int psglyphcount = 0;
    const unsigned short* pslist = (const unsigned short*)pslistsbuf.constData();
    const unsigned short* pslistend = (const unsigned short*)(pslistsbuf.constData() + pslistsbuf.size());
    // gb2312 + some extended standardized hanzi table
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

    QFile ffucharlistfile(ffucharmapfilename); // charlist file from FFU charlist records. contains ascii, half-width katakana and full-width glyphs
    ffucharlistfile.open(QFile::ReadOnly);
    QByteArray charlistsbuf2 = ffucharlistfile.readAll();
    ffucharlistfile.close();

    QTextCodec *codec = QTextCodec::codecForName("Shift-JIS");

    if (!codec) {
        AddLog("Shift-JIS codec is unavailable", ltError);
        qDebug() << QTextCodec::availableCodecs();
        return;
    }

    int ffunonkanjicount = 0;
    const unsigned char* charlist2 = (const unsigned char*)charlistsbuf2.constData();
    const unsigned char* charlist2end = (const unsigned char*)charlistsbuf2.constData() + charlistsbuf2.size();
    // only non-kanji part is usefull(lead byte < 0x88)
    while (charlist2 < charlist2end) {
        // read chars
        QByteArray ansi;
        unsigned short w = *charlist2;
        if (IsXORShiftJISLeadingByte(w)) {
            // TODO: bounds check
            unsigned char b2 = charlist2[1];
            charlist2 += 2;
            if (b2 >= 0x40 && b2 <= 0xFC && b2 != 0x7F) {
                // need reverse endian
                ansi.append(w);
                ansi.append(b2);
                w = w << 8 | b2;
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
            if (w == 0x815C || w == 0x8160) {
                AddLog(QString("strange characters U+%1 (%2) from ANSI %3" ).arg(uc, 4, 16, QLatin1Char('0')).arg(QChar(uc)).arg(w, 4, 16, QLatin1Char('0')), ltMessage);
            }
            
            Uni2S2312Map::iterator it = fUni_to_S2312.find(uc);
            if (it == fUni_to_S2312.end()) {
                fUni_to_S2312.insert(make_pair(uc, item));
            } else {
                it->second.isSJIS = true;
                it->second.sjiscode = w;
                if (it->second.singlebyte != item.singlebyte) {
                    // should never got here.
                    AddLog(QString("singlebyte flag mismatched on U+%1 (%2) from ANSI %3" ).arg(uc, 4, 16, QLatin1Char('0')).arg(QChar(uc)).arg(w, 4, 16, QLatin1Char('0')), ltError);
                }
            }

            ffunonkanjicount++;
        } else {
            // never
        }
    }

    ui->s2312mapcountView->setText(QString("%1").arg(s2312glyphcount));
    ui->psmapcountView->setText(QString("%1").arg(psglyphcount));
    ui->stdglyphcountView->setText(QString("%1").arg(ffunonkanjicount));

    memset(fS2312_to_Uni, 0xFF, sizeof(fS2312_to_Uni));
    for (Uni2S2312Map::const_iterator it = fUni_to_S2312.begin(); it != fUni_to_S2312.end(); it++) {
        if (it->second.singlebyte) {
            // 0..255
            fS2312_to_Uni[it->second.sjiscode] = it->first;
        } else if (it->second.isS2312) {
            // first 2312
            int index = it->second.s2312code - 0x8000 + 256;
            fS2312_to_Uni[index] = it->first;
        } else if (it->second.isSJIS) {
            int index = it->second.sjiscode - 0x8000 + 256;
            if (fS2312_to_Uni[index] == 0xFFFF) {
                fS2312_to_Uni[index] = it->first;
            } else {
                // overlap
            }
        } else {
            // empty?!
            AddLog(QString("empty record found on U+%1 (%2)" ).arg(it->first, 4, 16, QLatin1Char('0')).arg(QChar(it->first)), ltError);
        }
    }
}

void TEncodingConvFrm::onUTF16toShift2312Clicked()
{
    QString utf16filename = QFileDialog::getOpenFileName(this, tr("Open Translated UTF16 File"), PathSetting.LastCHSTextFolder, "txt Files (*.txt);;All Files (*.*)", 0, 0);
    if (utf16filename.isEmpty()) {
        return;
    }
    PathSetting.LastCHSTextFolder = QFileInfo(utf16filename).path();

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
            // used in neptools?
            unsigned short sjiscode = 0x5C81;
            s2312buf.append((char*)&sjiscode, 2);
        } else if (w == 0xFF5E) {
            // u+301C
            unsigned short sjiscode = 0x6081;
            s2312buf.append((char*)&sjiscode, 2);
        } else {
            AddLog(QString("Exception: character U+%1 (%2) out of ffu range!\n").arg(w, 4, 16, QLatin1Char('0')).arg(QChar(w)), ltError);
            return;
        }

        converted++;
    }

    QString s2312filename = QFileDialog::getSaveFileName(this, tr("Save Shift2312 File as"), PathSetting.LastCHSTextFolder, "txt Files (*.txt);;All Files (*.*)", 0, 0);
    if (s2312filename.isEmpty()) {
        return;
    }
    PathSetting.LastCHSTextFolder = QFileInfo(s2312filename).path();

    QFile s2312file(s2312filename);
    s2312file.open(QFile::WriteOnly);
    s2312file.write(s2312buf);
    s2312file.close();
}

void TEncodingConvFrm::onShift2312toUTF16Clicked()
{

}

QByteArray TEncodingConvFrm::UnicodeStrToShift2312( const QString& str )
{
    QByteArray s2312buf;
    for (QString::const_iterator cit = str.begin(); cit < str.end(); cit++) {
        unsigned short w = cit->unicode();
        if (w == 0xFF5E) {
            w = 0x301C; // 8160
        }
        Uni2S2312Map::const_iterator sit = fUni_to_S2312.find(w);

        if (sit != fUni_to_S2312.end()) {
            if (sit->second.singlebyte) {
                s2312buf.append((unsigned char)sit->second.isS2312?sit->second.s2312code:sit->second.sjiscode);
            } else {
                unsigned short sjiscode = _byteswap_ushort(sit->second.isS2312?sit->second.s2312code:sit->second.sjiscode);
                s2312buf.append((char*)&sjiscode, 2);
            }
        } else if (w < ' ') {
            s2312buf.append((char)w);
        } else if (w == 0x2015) {
            // neptools brick. workaround if ffu charmap doesn't contains such characters
            unsigned short sjiscode = 0x5C81;
            s2312buf.append((char*)&sjiscode, 2);
        } else {
            AddLog(QString("Exception: character U+%1 (%2) out of ffu charmap range!\n").arg(w, 4, 16, QLatin1Char('0')).arg(QChar(w)), ltError);
            return s2312buf;
        }
    }
    return s2312buf;
}

std::string TEncodingConvFrm::UnicodeStrToShift2312Str( const QString& str )
{
    QByteArray ba = UnicodeStrToShift2312(str);
    std::string ret;
    ret.assign(ba.constData(), ba.size());
    return ret;
}

QString TEncodingConvFrm::Shift2312ToUnicode( const QByteArray& str )
{
    QString unistr;
    // TODO: use array instead map
    const unsigned char* charptr = (const unsigned char*)str.constData();
    const unsigned char* charptrend = (const unsigned char*)str.constData() + str.size();

    while (charptr < charptrend) {
        // read chars
        unsigned short w = *charptr;
        if (IsXORShiftJISLeadingByte(w)) {
            // TODO: bounds check
            unsigned char c = charptr[1];
            charptr += 2;
            if (c >= 0x40 && c <= 0xFC && c != 0x7F) {
                // need reverse endian
                w = w << 8 | c;
                //newchars.push_back(w);
                int index = w - 0x8000 + 256;
                unistr.append((fS2312_to_Uni[index]));
            } else {
                // error
                break;
            }
        } else {
            charptr++;
            if (w < 0x100) {
                unistr.append((fS2312_to_Uni[w]));
            } else {
                // bug?!
            }
        }
    }
    return unistr;
}

void TEncodingConvFrm::onMergeSTCM2Clicked()
{
    QString stcm2filename = QFileDialog::getOpenFileName(this, tr("Open Original STCM2 File"), PathSetting.LastCHSTextFolder, "DAT Files (*.DAT);;All Files (*.*)", 0, 0);
    if (stcm2filename.isEmpty()) {
        return;
    }
    PathSetting.LastCHSTextFolder = QFileInfo(stcm2filename).path();
    STCM2Store store;
    QFile file(stcm2filename);

    file.open(QFile::ReadOnly);

    QByteArray contents = file.readAll();

    file.close();
    if (!store.LoadFromBuffer(contents.constData(), contents.size())) {
        return;
    }
    if (!store.ContainsDialog()) {
        return;
    }

    QString utf16filename = QFileDialog::getOpenFileName(this, tr("Open Translated UTF16 File"), PathSetting.LastCHSTextFolder, "txt Files (*.txt);;All Files (*.*)", 0, 0);
    if (utf16filename.isEmpty()) {
        return;
    }
    PathSetting.LastCHSTextFolder = QFileInfo(utf16filename).path();

    ImportCHSTextToStore(store, utf16filename);

    QByteArray s2312content;
    store.SaveToBuffer(s2312content);

    QString newdatfilename = QFileDialog::getSaveFileName(this, tr("Save Shift2312 STCM2 File as"), PathSetting.LastCHSTextFolder, "DAT Files (*.DAT);;All Files (*.*)", 0, 0);
    if (newdatfilename.isEmpty()) {
        return;
    }
    PathSetting.LastCHSTextFolder = QFileInfo(newdatfilename).path();

    QFile datfile(newdatfilename);
    datfile.open(QFile::WriteOnly);
    datfile.write(s2312content);
    datfile.close();
}


void TEncodingConvFrm::onMergeScriptFolderClicked()
{
    QString stcmfolder = QFileDialog::getExistingDirectory(this, tr("Open Folder Contains Original Script Files"), PathSetting.LastScriptsFolder);
    if (stcmfolder.isEmpty()) {
        return;
    }
    PathSetting.LastScriptsFolder = stcmfolder; // windows separator

    QVector <QString> scriptlist;

    QDirIterator dit(stcmfolder, QStringList() << "*.DAT", QDir::Files, QDirIterator::Subdirectories);
    QString stcmfolder2 = QFileInfo(stcmfolder).absoluteFilePath(); // normalize path separator
    while (dit.hasNext()) {
        //qDebug() << dit.next();
        scriptlist.push_back(dit.next());
    }
    if (scriptlist.empty()) {
        return;
    }
    QString outfolder = QFileDialog::getExistingDirectory(this, tr("Save Localized Script Files To Folder"), PathSetting.LastCHSScriptsFolder);
    if (outfolder.isEmpty()) {
        return;
    }
    PathSetting.LastCHSScriptsFolder = outfolder; // windows separator

    // TODO: replace button to QProgressBar?
    ui->mergeScriptFolderBtn->setEnabled(false);

    for (QVector <QString>::const_iterator it = scriptlist.begin(); it != scriptlist.end(); it++) {
        if (!it->startsWith(stcmfolder2)) {
            // never got here
            continue;
        }

        QString utf16filename = *it + "_CHS_utf16.txt";
        if (QFileInfo(utf16filename).exists() == false) {
            continue;
        }

        STCM2Store store;
        QFile file(*it);
        file.open(QFile::ReadOnly);
        QByteArray contents = file.readAll();
        file.close();

        if (!store.LoadFromBuffer(contents.constData(), contents.size())) {
            continue;
        }
        if (!store.ContainsDialog()) {
            continue;
        }

        ImportCHSTextToStore(store, utf16filename);

        int stcmfolderlen = stcmfolder2.length();
        QString newdatfilename = outfolder + it->mid(stcmfolderlen);
        QString newdatdir = newdatfilename.left(newdatfilename.lastIndexOf("/"));
        QDir::root().mkpath(newdatdir);

        QByteArray s2312content;
        store.SaveToBuffer(s2312content);

        QFile ndfile(newdatfilename);
        ndfile.open(QFile::WriteOnly);
        ndfile.write(s2312content);
        ndfile.close();
    }

    ui->mergeScriptFolderBtn->setEnabled(true);
}


void TEncodingConvFrm::ImportCHSTextToStore( STCM2Store &store, const QString& utf16filename )
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
            int opID = tails[1].toUInt();
            if (tails[2].startsWith("NAME")) {
                QString name = utf16ts.readLine();
                store.ReplaceDialogueDebug(opID, UnicodeStrToShift2312Str(name));
            }
            if (tails[2].startsWith("TEXTS")) {
                unsigned int orgcount = tails[2].right(2).left(1).toUInt();
                QStringList texts = utf16ts.readLine().split("\\n");
                int index = 0;
                QStringList::const_iterator tit = texts.begin();
                for (; tit != texts.end() && index < orgcount; tit++, index++) {
                    store.ReplaceDialogueDebug(opID + index, UnicodeStrToShift2312Str(*tit));
                }
                if (texts.size() < orgcount) {
                    // remove tail
                    for (; index < orgcount; index++) {
                        if (false == store.RemoveDialogueDebug(opID + index)) {
                            AddLog(QString("Remove opcode %1 failed, for translation %3").arg(opID + index).arg(utf16filename), ltError);
                        }
                    }
                }
                if (texts.size() > orgcount) {
                    // Append?
                    int baseID = opID + index - 1;
                    for (; tit != texts.end(); tit++) {
                        int newID = store.InsertDialogueDebug(baseID, UnicodeStrToShift2312Str(*tit));
                        if (newID == -1) {
                            AddLog(QString("Insert \"%1\" failed after opcode %2, for translation %3").arg(*tit).arg(baseID).arg(utf16filename), ltError);
                            break;
                        }
                        baseID = newID;
                    }
                }
            }
        }
    }
    utf16file.close();
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


