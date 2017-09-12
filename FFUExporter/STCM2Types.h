#ifndef _STCM2_TYPES_H
#define _STCM2_TYPES_H

#include "dirtyint.h"

// types takes from neptools and some minor change applied by reverse engineer
struct STCM2Header
{
    char magic[0x20];
    uint32_t export_offset;
    uint32_t export_count;
    uint32_t field_28; // ??
    uint32_t collection_link_offset;
};

struct STCM2ExportEntry
{
    uint32_t type; // 0 = CODE, 1 = DATA
    char name[0x20];
    uint32_t offset;
};

struct STCM2Data
{
    uint32_t type; // 0 or 1 ??
    uint32_t offset_unit; // length/4 for string data, 1 otherwise?
    uint32_t field_8;
    uint32_t length;
    //unsigned char payload[length];
};

struct STCM2InstructionHeader
{
    uint32_t is_call; // 0 or 1
    uint32_t opcode_or_offset;
    uint32_t param_count; // < 16
    uint32_t size;
};

enum STCM2OpCode
{
    SET_SPEAKERNAME = 0x4BC,
    ADD_DIALOGUE = 0x4BA,
    FINISH_SPEAK = 0x4BB,
};

struct STCM2Parameter
{
    uint32_t param_0;
    uint32_t param_4;
    uint32_t param_8;
};


enum STCM2ParamType
{
    MEM_OFFSET = 0,
    IMMEDIATE = 1,
    INDIRECT = 2,
    SPECIAL = 3,
};
enum STCM2ParamTypeSpecial
{
    READ_STACK_MIN = 0xffffff00, // range MIN..MAX
    READ_STACK_MAX = 0xffffff0f,
    READ_4AC_MIN   = 0xffffff20, // range MIN..MAX
    READ_4AC_MAX   = 0xffffff27,
    INSTR_PTR0     = 0xffffff40,
    INSTR_PTR1     = 0xffffff41,
    COLL_LINK      = 0xffffff42,
};

struct CollectionLinkHeader
{
    uint32_t field_00;
    uint32_t offset;
    uint32_t count;
    uint32_t field_0c;
    uint32_t field_10;
    uint32_t field_14;
    uint32_t field_18;
    uint32_t field_1c;
    uint32_t field_20;
    uint32_t field_24;
    uint32_t field_28;
    uint32_t field_2c;
    uint32_t field_30;
    uint32_t field_34;
    uint32_t field_38;
    uint32_t field_3c;
};

struct CollectionLinkEntry
{
    uint32_t name_0;
    uint32_t name_1;
    uint32_t field_08;
    uint32_t field_0c;
    uint32_t field_10;
    uint32_t field_14;
    uint32_t field_18;
    uint32_t field_1c;
};

#endif