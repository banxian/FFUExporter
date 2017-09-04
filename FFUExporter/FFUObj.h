#ifndef _FFU_OBJECT_H
#define _FFU_OBJECT_H

#include <QtCore/QtCore>
#include "FFUTypes.h"
#include <vector>
#include <map>

// shared?
typedef std::vector<SizeRecItem> GlyphSizeVec;
typedef std::vector<CharListRecItem> CharListVec;


struct GlyphDataBundle
{
    QByteArray aligneddata;
    unsigned __int16 sjiscode; // 2nd byte is lo8, 1st byte is hi8
    SizeRecItem sizerec;
};

typedef std::map<unsigned __int16, GlyphDataBundle> GlyphDataMap;

// char -> charlist -> sizeindex -> offset.
// verified charlist begin/end should ordered, but sizeindex can be shuffle


class FFUStore
{
private:
    bool fIsBigendian;
    GlyphDataMap fGlyphs;
    FFUHeader fLEHeader;
    unsigned __int32 fOldColorPalatte[256];
public:
    bool LoadFromBuffer(const void* buf, unsigned int size);
    bool SaveToBuffer(QByteArray& buf);
    FFUHeader GetLEHeader();
    bool AddGlyph(unsigned __int16 sjiscode, const GlyphDataBundle& bundle);
};

// utils
void SwapFFUHeaders(FFUHeader* header);
void SwapFFUTables(const FFUHeader* header, void* content);

#endif