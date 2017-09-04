#include "GBINObj.h"
#include <stdlib.h>
#include <string.h>

signed int func_gbin_normalize_endian(const void *buf, size_t len,
                                      void *outheader)
{
    GBINHeaderFooter *header = (GBINHeaderFooter *)((char *)buf + len - 0x40);
    unsigned char* buff = (unsigned __int8 *)buf;
    // GBNL?
    if (strncmp(header->magic, "GBN", 3)) {
        return 1;
    }
    uint8_t endian6 = header->endian_06;
    if (header->endian_06 & 2) {
        // already mixed endian
        return 0;
    }
    // big-endian
    if (header->magic[3] == 'B' && !(header->endian_06 & 1)) {
        func_swapendian_len((unsigned __int8 *)&header->field_04, 2);
        func_swapendian_len((unsigned __int8 *)&header->field_08, 4);
        func_swapendian_len((unsigned __int8 *)&header->field_0c, 4);
        func_swapendian_len((unsigned __int8 *)&header->flags, 4);
        func_swapendian_len((unsigned __int8 *)&header->struct_offset, 4);
        func_swapendian_len((unsigned __int8 *)&header->struct_count, 4);
        func_swapendian_len((unsigned __int8 *)&header->struct_size, 4);
        func_swapendian_len((unsigned __int8 *)&header->types_count, 4); // types_count is dword
        func_swapendian_len((unsigned __int8 *)&header->types_offset, 4);
        func_swapendian_len((unsigned __int8 *)&header->field_28, 4);
        func_swapendian_len((unsigned __int8 *)&header->string_offset, 4);
        func_swapendian_len((unsigned __int8 *)&header->field_30, 2);
        int types_count = (signed __int16)header->types_count; // Load DWORD, use short
        if (types_count > 0) {
            int typesdelta = 0;
            int types_counter = (signed __int16)header->types_count;
            do {
                GBINTypeDescriptor *types = (GBINTypeDescriptor *)&buff[header->types_offset + typesdelta];
                func_swapendian_len((unsigned __int8 *)&types->type, 2);
                func_swapendian_len((unsigned __int8 *)&types->offset, 2);
                --types_counter;
                typesdelta += 4;
            } while (types_counter);
        }
        int rev30 = 4;
        int revc = header->field_0c; // always 4, alignment or var int32 length
        signed int struct_count = header->struct_count;
        if (header->field_30) {
            rev30 = header->field_30; // zero or 4, affect to type4, string
        }
        for (signed int i = 0; i < struct_count; ++i) {
            unsigned char* structitem = 0;
            if (i < (signed int)header->struct_count) {
                // Every struct is header.struct_size
                structitem = &buff[header->struct_offset + header->struct_size * i];
            }
            if (types_count > 0) {
                int typedelta = 0;
                //types_count_m2 = types_count;
                do {
                    func_gbin_swap_structitem(
                        structitem,
                        (GBINTypeDescriptor *)&buff[header->types_offset + typedelta],
                        revc, rev30);
                    --types_count;
                    typedelta += 4;
                } while (types_count);
            }
        }
        // useless in loop
        endian6 = header->endian_06 | 1;
        header->endian_06 = endian6;
    }
    if (header->flags & 1) {
        // have string
//#ifdef _DEBUG
        int stringcount = 0;
        unsigned char* structitemm1 = &buff[header->struct_offset];
        int typescount = (signed __int16)header->types_count;
        __int32 structcount = header->struct_count; // should be signed int
        __int32 structsize = header->struct_size;  // signed?
        int16_t *offsets = (int16_t *)malloc(2 * typescount);
        int l = 0;
        if (typescount > 0) {
            int16_t* offsetsm1 = offsets;
            GBINTypeDescriptor *lptype = (GBINTypeDescriptor *)&buff[header->types_offset];
            do {
                if (lptype->type == gtSTRING) {
                    // only record string's offsets
                    ++stringcount;
                    *offsetsm1 = lptype->offset;
                    ++offsetsm1;
                }
                ++l;
                ++lptype;
            } while (l < typescount);
        }
        // < count, += size
        for (signed int j = 0; j < (signed __int32)structcount; structitemm1 += structsize) {
            int m = 0;
            if (stringcount > 0) {
                int16_t *offsetsm2 = offsets;
                do {
                    int offset = *offsetsm2;
                    unsigned char* strmemptr = 0;
                    int stritemoff = *(unsigned __int32 *)&structitemm1[offset];
                    // offset?
                    if (stritemoff != -1) {
                        strmemptr = &buff[header->string_offset + stritemoff];
                    }
                    ++m;
                    //*(unsigned __int32*)&structitemm1[offset] = (unsigned __int32)strmemptr; //from file offset to memory address?!
                    ++offsetsm2;
                } while (m < stringcount);
            }
            ++j;
        }
        free(offsets);
//#endif
        endian6 = header->endian_06;
    }
    header->endian_06 = endian6 | 2; // become 3 (mixed endian?)

    if (outheader) {
        memcpy(outheader, header, 0x40u);
    }
    return 0;
}

void func_swapendian_len(unsigned char *buf, int len)
{
    int lendiv = len >> 1;
    int index = 0;
    if (len >> 1 > 0) {
        unsigned char* buftail = &buf[len];
        unsigned char* bufm = buf;
        do {
            unsigned char *tail2 = &buftail[-index++];
            unsigned char b3 = *(tail2 - 1);
            *(tail2 - 1) = *bufm;
            *bufm++ = b3;
        } while (index < lendiv);
    }
}

void func_gbin_swap_structitem(unsigned char *structitems,
                               GBINTypeDescriptor *types, int headerrevc,
                               int headerrev30)
{

    switch (types->type) {
    case gtUINT32:
        // not uint, just record with length field
        {
            int lenfielddiv2 = headerrevc >> 1;
            unsigned char *len32ptr = &structitems[types->offset];
            int i = 0;
            if (headerrevc >> 1 > 0) {
                // 4?
                unsigned char* tail = &len32ptr[headerrevc];
                do {
                    // useless now
                    unsigned char* looptail = &tail[-i++]; // 4
                    unsigned char b3 = *(looptail - 1); // 3<->0, 2<->1
                    *(looptail - 1) = *len32ptr; // 3 <- 0
                    *len32ptr++ = b3; // 0 -> 3
                } while (i < lenfielddiv2);
            }
        }
        break;
    case gtUINT8:
        return;
    case gtUINT16:
        // fixed
        {
            int offset = types->offset;
            unsigned char b1 = structitems[offset + 1];
            structitems[offset + 1] = structitems[offset];
            structitems[offset] = b1;
        }
        break;
    case gtFLOAT:
        // yes, is fixed
        {
            unsigned char* floatptr = &structitems[types->offset];
            unsigned char floatb3 = floatptr[3];
            floatptr[3] = *floatptr;
            *floatptr = floatb3;
            unsigned char floatb2 = floatptr[2];
            floatptr[2] = floatptr[1];
            floatptr[1] = floatb2;
        }
        break;
    case gtTYPE4:
        // var int32?
        {
            int rev30div2 = headerrev30 >> 1;
            unsigned char *len2ptr = &structitems[types->offset];
            int j = 0;
            if (headerrev30 >> 1 > 0) {
                unsigned char* tail2 = &len2ptr[headerrev30];
                do {
                    unsigned char* tail2a = &tail2[-j++];
                    unsigned char tb3 = *(tail2a - 1);
                    *(tail2a - 1) = *len2ptr;
                    *len2ptr++ = tb3;
                } while (j < rev30div2);
            }
        }
        break;
    case gtSTRING:
        // have size/offset header.
        {
            int rev30div2 = headerrev30 >> 1;
            unsigned char* lenstrptr = &structitems[types->offset];
            int k = 0;
            if (headerrev30 >> 1 > 0) {
                unsigned char* tail3 = &lenstrptr[headerrev30];
                do {
                    unsigned char* tail3a = &tail3[-k++];
                    unsigned char prev = *(tail3a - 1);
                    *(tail3a - 1) = *lenstrptr;
                    *lenstrptr++ = prev;
                } while (k < rev30div2);
            }
        }
        break;
    case gtUINT64: // or double
        // fixed length
        {
            signed int count1 = 2;
            unsigned char* t6ptr1 = &structitems[types->offset];
            unsigned char* t6ptr2 = &structitems[types->offset];
            // 7<->0
            // 6<->1
            // 5<->2
            // 4<->3
            do {
                --count1;
                char b7 = t6ptr1[7];
                t6ptr1[7] = *t6ptr2; // 7 <-> 0
                *t6ptr2 = b7;
                char b6 = t6ptr1[6];
                t6ptr1[6] = t6ptr2[1]; // 6 <-> 1
                t6ptr1 -= 2;
                t6ptr2[1] = b6;
                t6ptr2 += 2;
            } while (count1);
        }
        break;
    case gtFIXINT32:
        // fixed, too
        {
            unsigned char* t7ptr = &structitems[types->offset];
            char b3 = t7ptr[3];
            t7ptr[3] = *t7ptr; // 3<->0
            *t7ptr = b3;
            char b2 = t7ptr[2];
            t7ptr[2] = t7ptr[1]; // 2<->1
            t7ptr[1] = b2;
        }
        break;
    }
}