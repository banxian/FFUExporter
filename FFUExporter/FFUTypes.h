#ifndef _FFU_TYPES_H
#define _FFU_TYPES_H

#pragma pack(push, 4)
struct FFUHeader
{
    unsigned short magic; // UFS
    unsigned short charListCount; // and endian
    unsigned int glyphCount; // guess
    //unsigned int rev8; // 01181803,00012D21
    unsigned __int8 depth;
    unsigned __int8 defWidth;
    unsigned __int8 defHeight;
    unsigned __int8 endianB; // 01/02
    unsigned __int8 revC; // 0x01000000
    unsigned __int8 revD;
    unsigned __int8 revE;
    unsigned __int8 havePalette; // bit0 palette
    unsigned int fileSize; // all
    unsigned int charListOffset; // 0x20, 0x428
    unsigned int sizeTableOffset;
    unsigned int fontDataOffset;
    unsigned __int8 rev20; // only in havePalette?
    unsigned __int8 rev21;
    unsigned __int8 rev22;
    unsigned __int8 rev23;
    unsigned int rev24; // zero
};
struct SizeRecItem
{
    unsigned __int8 layoutWidth;
    unsigned __int8 height;
    unsigned short dataSize;
    unsigned int dataOffset; // delta to font data payloads
};
struct CharListRecItem
{
    // semi bigendian.
    unsigned int charBegin; // >=
    unsigned int charEnd; // <
    unsigned int mapIndex; // from 0
};
#pragma pack(pop)
#pragma pack(push, 1)
#pragma pack(pop)

#define FUMAGIC 0x4655
#define UFMAGIC 0x5546


#endif