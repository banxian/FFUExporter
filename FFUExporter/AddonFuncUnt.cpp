#include <QtCore/QtGlobal>
#include <QtCore/QtEndian>
#include <QtCore/QCryptographicHash>
#include <QtGui/QPainter>
#include <zlib\zlib.h>
#include <QtCore/QDir>
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACX)
#   include <unistd.h>
#elif defined(Q_OS_MACX)
#   include <mach/mach.h>
#   include <mach/machine.h>
#endif
#include <QMap>
#include <QApplication>
#include <stdarg.h>

#include "AddonFuncUnt.h"


unsigned char QuadBit2Hex(unsigned char num)
{
    if (num < 10) {
        return num + '0';
    } else {
        return num + '7';
    }
}

unsigned char Hex2QuadBit(unsigned char chr)
{
    if (chr < 'A') {
        return chr - '0';
    } else {
        return chr - '7';
    }
}

quint16 readuint16(QIODevice& qfile, bool bigendian /*= false*/, bool peek /*= false*/)
{
    quint16 Result;
    qfile.read((char*)&Result, sizeof(Result));
    if (bigendian) {
        Result = qFromBigEndian(Result);
    } else {
        Result = qFromLittleEndian(Result);
    }
    if (peek) {
        qfile.seek(qfile.pos() - sizeof(Result));
    }
    return Result;
}

quint32 readuint32(QIODevice& qfile, bool bigendian /*= false*/, bool peek /*= false*/)
{
    quint32 Result;
    qfile.read((char*)&Result, sizeof(Result));
    if (bigendian) {
        Result = qFromBigEndian(Result);
    } else {
        Result = qFromLittleEndian(Result);
    }
    if (peek) {
        qfile.seek(qfile.pos() - sizeof(Result));
    }
    return Result;
}

void ReplaceAlphaWithChecker( QImage& qimage )
{
    // TODO: optimize for huge piece
    // Inplace Draw, Inplace QImage format assign / Premulti
    QImage box(32, 32, qimage.format());
    QPainter pmp(&box);
    pmp.fillRect(0, 0, 16, 16, Qt::lightGray);
    pmp.fillRect(16, 16, 16, 16, Qt::lightGray);
    pmp.fillRect(0, 16, 16, 16, Qt::darkGray);
    pmp.fillRect(16, 0, 16, 16, Qt::darkGray);
    pmp.end();

    QImage viaimage = QImage(qimage);
    QPainter checkermaker(&qimage);
    QBrush checker;
    checker.setTextureImage(box);
    checkermaker.fillRect(qimage.rect(), checker);
    checkermaker.drawImage(0, 0, viaimage);
    checkermaker.end();

    if (qimage.format() == QImage::Format_ARGB32) {
        qimage = qimage.convertToFormat(QImage::Format_RGB32);
    }
}

int greedydiv(int n, int m)
{
    int Result = (n + m - 1) / m;
    return Result;
}

bool SaveByteArrayToFile(const QString& filename, const QByteArray& content)
{
    QDir dir(QFileInfo(filename).path());
    if (dir.exists() == false) {
        QDir::root().mkpath(dir.path());
        //_wmkdir(QDir::toNativeSeparators(QFileInfo(filename).path()).utf16());
    }
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(content);
        file.close();
        return true;
    }
    return false;
}

// KOL
std::string Int2Str(int Value)
{
    char buf[16];
    char* dst;
    bool minus = (Value < 0);
    if (minus) {
        Value = 0 - Value;
    }
    dst = &buf[15];
    *dst = 0;
    unsigned d = Value;
    do {
        dst--;
        *dst = (d % 10) + '0';
        d = d / 10;
    } while (d > 0);
    if (minus) {
        dst--;
        *dst = '-';
    }

    return std::string(dst);
}

std::string Int2Hex(DWORD Value, int Digits)
{
    char Buf[9];
    char* Dest;

    Dest = &Buf[8];
    *Dest = 0;
    do {
        Dest--;
        *Dest = '0';
        if (Value != 0) {
            *Dest = QuadBit2Hex(Value & 0xF);
            Value = Value >> 4;
        }
        Digits--;
    } while (Value != 0 || Digits > 0);
    return Dest;
}

unsigned Hex2Int(const std::string& Value)
{
    // TODO: remove 0x
    unsigned result = 0;
    int ilen = Value.length();
    for (int i = 0; i < ilen; i++) {
        char b1 = Value[i];
        if (b1 >= '0' && b1 <= '9') {
            result = (result << 4) | (b1 - '0');
        } else if (b1 >= 'A' && b1 <= 'F') {
            result = (result << 4) | (b1 - 'A' + 10);
        } else if (b1 >= 'a' && b1 <= 'f') {
            result = (result << 4) | (b1 - 'a' + 10);
        } else {
            break;
        }
    }

    return result;
}

int S2Int( const char* S )
{
    int M;
    int Result = 0;
    if (S == NULL) 
        return Result;
    M = 1;
    if (*S == '-') {
        M = -1;
        S++;
    } else if (*S == '+'){
        S++;
    }
    //while S^ in [ '0'..'9' ] do
    while (*S >= '0' && *S <= '9')
    {
        Result = Result * 10 + int( *S ) - int( '0' );
        S++;
    }
    if (M < 0){
        Result = -Result;
    }
    return Result;
}

int Str2Int( const std::string Value )
{
    return S2Int(Value.c_str());
}

const char* __DelimiterLast(const char* Str, const char* Delimiters)
{
    const char* P, *F;

    P = Str;
    const char* Result = P + strlen(Str);
    while (*Delimiters != 0) {
        F = strrchr(P, *Delimiters);
        if (F != NULL) {
            if (*Result == 0 || unsigned(F) > unsigned(Result)) {
                Result = F;
            }
        }
        Delimiters++;
    }
    return Result;
}

std::string ExtractFilePath(const std::string& Path)
{
    const char* P;
    const char* P0;

    P0 = Path.c_str();
    P = __DelimiterLast(P0, ":\\/");
    if (*P == 0) {
        return "";
    } else {
        return std::string(Path).substr(0, P - P0 + 1);
    }
}

std::string ExtractFileName(const std::string& Path)
{
    const char* P = __DelimiterLast(Path.c_str(), ":\\/");
    if (*P == 0) {
        return Path;
    } else {
        return std::string(P + 1);
    }
}

std::string ExtractFileExt(const std::string& Path)
{
    const char* P = __DelimiterLast(Path.c_str(), ".");
    return std::string(P);
}

std::string ExtractFileNameWOext(const std::string& Path)
{
    std::string Result = ExtractFileName(Path);
    return Result.substr(0, Result.size() - ExtractFileExt(Path).size());
}

std::string ReplaceFileExt(const std::string& Path, const std::string& NewExt)
{
    std::string Result = ExtractFilePath(Path) +
                         ExtractFileNameWOext(ExtractFileName(Path)) +
                         NewExt;
    return Result;
}

std::string ExcludeTrailingChar(const std::string& S, char C)
{
    std::string Result = S;
    if (Result.empty() == false) {
        if (Result[ Result.size() - 1 ] == C) {
            Result.resize(Result.size() - 1);
        }
    }
    return Result;
}

std::string ExcludeTrailingPathDelimiter(const std::string& S)
{
    return ExcludeTrailingChar(S, '\\');
}

std::string Int2Digs(int Value, int Digits)
{
    std::string result;
    std::string M;
    result = Int2Str(Value);
    M = "";
    if (Value < 0) {
        M = "-";
        result = result.substr(result.size() - 2, 2);
    }
    if (Digits >= 0) {
        while ((M + result).length() < Digits) {
            result = std::string("0") + result;
        }
    } else {
        while (result.length() < - Digits) {
            result = std::string("0") + result;
        }
    }
    result = M + result;
    return result;
}

void myReplace(std::string& str, const std::string& old, const std::string& newstr)
{
    size_t pos = 0;
    while ((pos = str.find(old, pos)) != std::string::npos) {
        str.replace(pos, old.length(), newstr);
        pos += newstr.length();
    }
}

std::string EscapeString(const std::string& Source)
{
    std::string Result = Source;
    // Common
    myReplace(Result, "\\", "\\\\");
    myReplace(Result, "\t", "\\t");
    myReplace(Result, "\r", "\\r");
    myReplace(Result, "\n", "\\n");
    return Result;
}

std::string StripeSlashed(const std::string& Value)
{
    std::string result;
    int i = 0;
    while (i < Value.length()) {
        if ((unsigned char) Value[i] >= 0x81 && (unsigned char) Value[i] <= 0xFE) {
            result = result + Value.substr(i, 2);   // TODO: utf8
            i += 2;
        } else {
            if (Value[i] == '\\') {
                if (i + 1 < Value.length()) {
                    if (Value[i + 1] == '\\') { // [\\]
                        result = result + "\\";
                    }
                    if (Value[i + 1] == 'r' ||  Value[i + 1] == 'R') { // [\r]
                        result = result + "\r";
                    }
                    if (Value[i + 1] == 'n' ||  Value[i + 1] == 'N') { // [\n]
                        result = result + "\n";
                    }
                    if (Value[i + 1] == 't' ||  Value[i + 1] == 'T') { // [\t]
                        result = result + "\t";
                    }
                    if (Value[i + 1] == '\\' ||  Value[i + 1] == 'r' ||  Value[i + 1] == 'R' ||  Value[i + 1] == 'n' ||  Value[i + 1] == 'N' ||  Value[i + 1] == 't' ||  Value[i + 1] == 'T') {
                        i++;
                    } else {
                        result = result + "\\";
                        //MainFrm.AddLog( String( "Invalied Slashed chars! " ) + Value.substr( 1 - 1, i ) + "<-", 0, 1 );
                    }
                }
            } else {
                result = result + Value[i];
            }
            i++;
        }
    }
    return result;
}

int __cdecl qstrprintf( QByteArray& str, const char * _Format, ... )
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
    str.resize(len + 1);
    str.fill(0);
#ifdef __GNUC__
    len = vsnprintf(str.data(), len, _Format, ext);
#else
    len = _vsnprintf_s(str.data(), str.size(), len, _Format, ext);
#endif
    va_end(ext);

    return len;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static bool randomized = false;
int random(int from, int to) {
    int result;
    if (!randomized) {
#ifdef _WIN32
        srand(GetTickCount());
#else
        srand(time(NULL));
#endif
        randomized = true;
    }
    //result = floor((double(rand())/ RAND_MAX) * (to - from)) + from;
    result = rand()%(to - from + 1) + from;
    return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

bool IsFullShiftJISLeadingByte( char c )
{
    unsigned char uc = static_cast<unsigned char>(c);
    if ((uc >= 0x80 && uc <= 0xA0) || (uc >= 0xE0 && uc <= 0xFF)) {
        return true;
    }
    return false;
}

bool IsXORShiftJISLeadingByte( unsigned char c )
{
    if ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC)) {
        return true;
    }
    return false;
}

/*
* The memmem() function finds the start of the first occurrence of the
* substring 'needle' of length 'nlen' in the memory area 'haystack' of
* length 'hlen'.
*
* The return value is a pointer to the beginning of the sub-string, or
* NULL if the substring is not found.
*/
void *memmem(const void *haystack, size_t hlen, const void *needle, size_t nlen)
{
    int needle_first;
    const void *p = haystack;
    size_t plen = hlen;

    if (!nlen)
        return NULL;

    needle_first = *(unsigned char *)needle;

    while (plen >= nlen && (p = memchr(p, needle_first, plen - nlen + 1)))
    {
        if (!memcmp(p, needle, nlen))
            return (void *)p;

        p = (char*)p + 1;
        plen = hlen - ((char*)p - (char*)haystack);
    }

    return NULL;
}