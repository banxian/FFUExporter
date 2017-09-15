#ifndef _GBIN_OBJECT_H
#define _GBIN_OBJECT_H

#include "dirtyint.h"
#include <vector>
#include <QtCore/QByteArray>

enum GBINType
{
    gtULONG = 0x0,
    gtUINT8 = 0x1,
    gtUINT16 = 0x2,
    gtFLOAT = 0x3,
    gtTYPE4 = 0x4,
    gtSTRING = 0x5,
    gtUINT64 = 0x6,
    gtFIXINT32 = 0x7,
};

#pragma pack(push, 4)
#pragma pack(pop)
#pragma pack(push, 1)
struct GBINHeaderFooter
{
    char magic[4];
    uint16_t field_04;
    uint8_t endian_06;
    uint8_t field_07;
    uint32_t field_08;
    uint32_t longsize;
    uint32_t flags;
    uint32_t struct_offset;
    uint32_t struct_count;
    uint32_t struct_size;
    uint32_t types_count;
    uint32_t types_offset;
    uint32_t string_count; // set
    uint32_t string_offset;
    int16_t ptrsize;
    uint16_t field_32;
    uint32_t padding[3];
};
struct GBINTypeDescriptor
{
    uint16_t type;
    int16_t offset;
};
#pragma pack(pop)
struct GBINFieldRecItem
{
    QByteArray rawdata;
    QByteArray strrec;
    uint32_t u32rec;
};

struct GBINFieldDefineBundle : GBINTypeDescriptor
{
    int16_t size; // calculate from next item
    GBINFieldDefineBundle(const GBINTypeDescriptor& base) : size(-1) {
        memcpy(&this->type, &base.type, sizeof(base));
    }
    GBINFieldDefineBundle() : size(-1) {
        memset(&this->type, 0, sizeof(GBINTypeDescriptor));
    }
};

typedef std::vector <GBINFieldDefineBundle> GBINFieldDefineBundleVec;
typedef std::vector <GBINFieldRecItem> GBINComplexRecord;
typedef std::vector <GBINComplexRecord> GBINComplexRecordVec;

class GBINStore
{
private:
    GBINHeaderFooter fHeader;
    enum GBINFileType {
        ftGBNL,
        ftGBNB,
        ftGSTL,
        ftGSTB,
    };
    GBINFileType fFileType;
    GBINFieldDefineBundleVec fFieldDefines;
    GBINComplexRecordVec fComplexRecords;
    bool fMayhaveText;
public:
    bool LoadFromBuffer(void* buf, unsigned int size);
    bool ExportTranslationText(QByteArray& buf);
    bool ContainsText();
    bool SaveToBuffer(QByteArray& buf);
    bool ReplaceString(int row, int column, const std::string& newstr);// ReplaceText conflicted with windows api
    bool ReplaceULONG(int row, int column, const unsigned int newval);
};

signed int func_gbin_normalize_endian(void *buf, size_t len, void *outheader);
signed int func_gbin_to_big_endian(void *buf, size_t len);
void func_swapendian_len(unsigned char *buf, int len);
void func_gbin_swap_structitem(unsigned char *structitems, GBINTypeDescriptor *types, int headerrevc, int headerrev30);

const char* GBINType2Str(GBINType type);
uint16_t GBINTypeFromStr(const char* name);

void paddingqbytearray( QByteArray &recordsbuf, int recordsbufsize );
#endif