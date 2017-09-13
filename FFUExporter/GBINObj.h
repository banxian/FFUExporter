#ifndef _GBIN_OBJECT_H
#define _GBIN_OBJECT_H

#include "dirtyint.h"

#pragma pack(push, 4)
struct GBINHeaderFooter
{
    char magic[4];
    uint16_t field_04;
    uint8_t endian_06;
    uint8_t field_07;
    uint32_t field_08;
    uint32_t field_0c;
    uint32_t flags;
    uint32_t struct_offset;
    uint32_t struct_count;
    uint32_t struct_size;
    uint32_t types_count;
    uint32_t types_offset;
    uint32_t field_28;
    uint32_t string_offset;
    int16_t field_30;
    uint16_t field_32;
    uint32_t padding[3];
};
struct GBINTypeDescriptor
{
    uint16_t type;
    int16_t offset;
};
enum GBINType
{
    gtUINT32 = 0x0,
    gtUINT8 = 0x1,
    gtUINT16 = 0x2,
    gtFLOAT = 0x3,
    gtTYPE4 = 0x4,
    gtSTRING = 0x5,
    gtUINT64 = 0x6,
    gtFIXINT32 = 0x7,
};
#pragma pack(pop)
#pragma pack(push, 1)
#pragma pack(pop)

signed int func_gbin_normalize_endian(const void *buf, size_t len, void *outheader);
void func_swapendian_len(unsigned char *buf, int len);
void func_gbin_swap_structitem(unsigned char *structitems, GBINTypeDescriptor *types, int headerrevc, int headerrev30);

const char* GBINType2Str(GBINType type);

#endif