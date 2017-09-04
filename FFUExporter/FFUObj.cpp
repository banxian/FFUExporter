#include "FFUObj.h"
#include <intrin.h>
#include <utility>

using std::make_pair;

bool FFUStore::LoadFromBuffer( const void* buf, unsigned int size )
{
    QByteArray contents((const char*)buf, size); // copied

    FFUHeader header = *(FFUHeader*)contents.constData(); // do copy

    if (header.magic != FUMAGIC && header.magic != UFMAGIC) {
        return false;
    }
    fIsBigendian = header.magic == UFMAGIC;
    if (fIsBigendian) {
        SwapFFUHeaders(&header);
        SwapFFUTables(&header, contents.data()); // quick n dirty
    }
    fLEHeader = header;
    if (header.havePalette & 1) {
        memcpy(fOldColorPalatte, (char*)buf + 0x28, 0x400);
    }

    // charlist begin/end cause lookup stop, should be sorted. mapIndex can be random order
    // glyph size is "grouped" by each charlist rec, can be some what shuffle.
    // we prefer official sort style: all sorted during open
    const char* constbuf = contents.constData();
    //const SizeRecItem* glyphsizes = (const SizeRecItem*)(constbuf + header.sizeTableOffset);
    const CharListRecItem* charlists = (const CharListRecItem*)(constbuf + header.charListOffset);
    const CharListRecItem* charlistsEnd = ((const CharListRecItem*)(constbuf + header.charListOffset)) + header.charListCount; // need assert?

    while (charlists != charlistsEnd) {
        // get mapindex for glyph "groups"
        const SizeRecItem* glyphsizes = ((const SizeRecItem*)(constbuf + header.sizeTableOffset)) + charlists->mapIndex;
        for (unsigned short w = charlists->charBegin; w != charlists->charEnd; w++) {
            GlyphDataBundle bundle;
            bundle.sjiscode = w;
            if (glyphsizes->dataOffset + glyphsizes->dataSize + header.fontDataOffset <= size) {
                bundle.aligneddata.append(constbuf + header.fontDataOffset + glyphsizes->dataOffset, glyphsizes->dataSize);
            } else {
                bundle.aligneddata.resize(glyphsizes->dataSize); // error
                bundle.aligneddata.fill(0);
            }
            bundle.sizerec = *glyphsizes;
            fGlyphs.insert(make_pair(w, bundle));
            glyphsizes++;
        }
        charlists++;
    }

    return true;
}

bool FFUStore::SaveToBuffer( QByteArray& buf )
{
    FFUHeader newheader = fLEHeader;
    QByteArray newcontent;
    // TODO: fix header
    int headersize = ((newheader.havePalette&1)?0x28:0x20);
    newcontent.append((char*)&newheader, headersize);
    if (newheader.havePalette & 1) {
        newcontent.append((char*)&fOldColorPalatte[0], 0x400);
        newheader.charListOffset = 0x400 + 0x28;
    } else {
        newheader.charListOffset = 0x20;
    }
    newheader.glyphCount = fGlyphs.size();
    // build new charlist
    unsigned short wbegin = 0, wprev = 0;
    CharListVec charlist;
    int glyphcount = fGlyphs.size();
    int currindex = 0;
    int currdatapos = 0;
    SizeRecItem* glyphsizes = (SizeRecItem*)malloc(fGlyphs.size() * sizeof(SizeRecItem));
    void* oldglyphsizes = glyphsizes;
    for (GlyphDataMap::const_iterator it = fGlyphs.begin(); it != fGlyphs.end(); it++) {
        unsigned short w = it->first;
        if (wbegin == 0) {
            wbegin = w;
        }
        if (wprev != 0) {
            if (wprev + 1 != w || currindex == glyphcount - 1) {
                // breaked!
                CharListRecItem item;
                item.charBegin = wbegin;
                // three case
                if ((currindex == glyphcount - 1) && wprev + 1 == w){
                    item.charEnd = w + 1;
                    item.mapIndex = currindex - (w - wbegin);
                //} else if ((currindex == glyphcount - 1) && wprev + 1 != w) {

                } else {
                    item.charEnd = wprev + 1;
                    item.mapIndex = currindex - (wprev + 1 - wbegin);
                }
                charlist.push_back(item);
                wbegin = w; // a new begining
            }
        }
        *glyphsizes = it->second.sizerec;
        glyphsizes->dataOffset = currdatapos;
        currdatapos += glyphsizes->dataSize;
        wprev = w;
        currindex++;
        glyphsizes++;
    }
    newheader.charListCount = charlist.size();
    newheader.sizeTableOffset = newheader.charListOffset + charlist.size() * sizeof(CharListRecItem);
    newcontent.append((char*)&charlist[0], charlist.size() * sizeof(CharListRecItem));
    newcontent.append((char*)oldglyphsizes, fGlyphs.size() * sizeof(SizeRecItem));
    newheader.fontDataOffset = newheader.sizeTableOffset + newheader.glyphCount * sizeof(SizeRecItem);
    free(oldglyphsizes); // or -size
    for (GlyphDataMap::const_iterator it = fGlyphs.begin(); it != fGlyphs.end(); it++) {
        newcontent.append(it->second.aligneddata);
    }
    newheader.fileSize = newcontent.size();

    if (fIsBigendian) {
        SwapFFUTables(&newheader, newcontent.data());
        SwapFFUHeaders(&newheader);
    }
    newcontent.replace(0, headersize, (char*)&newheader, headersize);

    buf = newcontent;

    return true;
}

bool FFUStore::AddGlyph( unsigned __int16 sjiscode, const GlyphDataBundle& bundle )
{
    GlyphDataMap::iterator it = fGlyphs.find(sjiscode);
    if (it == fGlyphs.end()) {
        fGlyphs.insert(make_pair(sjiscode, bundle));
    } else {
        it->second = bundle;
    }
    return true;
}

FFUHeader FFUStore::GetLEHeader()
{
    return fLEHeader;
}

#define SWAPU32(x) x = _byteswap_ulong(x)
#define SWAPU16(x) x = _byteswap_ushort(x)


void SwapFFUHeaders( FFUHeader* header )
{
    // don't touch endian?
    SWAPU16(header->magic);
    SWAPU16(header->charListCount);
    SWAPU32(header->glyphCount); // guess
    //unsigned int rev8; // 01181803,00012D21
    SWAPU32(header->fileSize); // all
    SWAPU32(header->charListOffset); // 0x20, 0x428
    SWAPU32(header->sizeTableOffset);
    SWAPU32(header->fontDataOffset);
    //SWAPU32(header->rev24); // zero, why not swap because offical code
    if (header->endianB == 2) {
        header->endianB = 1;
    } else {
        header->endianB = 2;
    }
}

void SwapFFUTables( const FFUHeader* header, void* content )
{
    // swap charmap and sizetable but no colortabl;e
    // CharListRecItem is 3 uint32
    unsigned int* charmapptr = (unsigned int*)((char*)content + header->charListOffset);
    for (int i = (header->sizeTableOffset - header->charListOffset) / 4; i >= 1; i--, charmapptr++) {
        *charmapptr = _byteswap_ulong(*charmapptr);
    }
    SizeRecItem* sizeptr = (SizeRecItem*)((char*)content + header->sizeTableOffset);
    for (int i = header->glyphCount; i >= 1; i--, sizeptr++) {
        sizeptr->dataSize = _byteswap_ushort(sizeptr->dataSize);
        sizeptr->dataOffset = _byteswap_ulong(sizeptr->dataOffset);
    }
}
