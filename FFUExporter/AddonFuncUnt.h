#ifndef _ADDON_FUNC_UNIT
#define _ADDON_FUNC_UNIT

#include <string>
#include <QtGui/QImage>
#include <QtCore/QFile>
#include <Windows.h>


bool SaveByteArrayToFile( const QString& filename, const QByteArray& content );

int greedydiv(int n, int m);

// KOL
// Int2Str
std::string Int2Str( int Value );
/* Converts integer Value into string with hex number. Digits parameter
   determines minimal number of digits (will be completed by adding
   necessary number of leading zeros). */
std::string Int2Hex( DWORD Value, int Digits );
/* Converts integer to string, inserting necessary number of leading zeroes
to provide desired length of string, given by Digits parameter. If
resulting string is greater then Digits, string is not truncated anyway. */
std::string Int2Digs( int Value, int Digits );
// Returns only path part from exact path to file.
std::string ExtractFilePath( const std::string& Path );
// Extracts file name from exact path to file.
std::string ExtractFileName( const std::string& Path );
// Extracts extension from file name (returns it with dot '.' first)
std::string ExtractFileExt( const std::string& Path );
// Extracts file name from path to file or from filename.
std::string ExtractFileNameWOext( const std::string& Path );
/* Returns address of the last of delimiters given by Delimiters parameter
among characters of Str. If there are no delimiters found, position of
the null terminator in Str is returned. This function is intended
mainly to use in filename parsing functions. */
const char* __DelimiterLast( const char* Str, const char* Delimiters );
std::string ReplaceFileExt( const std::string& Path, const std::string& NewExt );
std::string ExcludeTrailingPathDelimiter(const std::string& S);
unsigned Hex2Int( const std::string& Value );

std::string EscapeString(const std::string& Source);
std::string StripeSlashed(const std::string& Value);
int __cdecl qstrprintf( QByteArray& str, const char * _Format, ... );
int random(int from, int to);
int S2Int( const char* S );
int Str2Int( const std::string Value );

void ReplaceAlphaWithChecker( QImage& qimage );
bool IsFullShiftJISLeadingByte(char c);
bool IsXORShiftJISLeadingByte(unsigned char c);

void *memmem(const void *haystack, size_t hlen, const void *needle, size_t nlen);

#endif