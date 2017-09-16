#include "GBINObj.h"
#include <stdlib.h>
#include <string.h>
#include <QString>
#include <QHash>
#include <math.h>
#include <QDebug>


signed int func_gbin_normalize_endian(void *buf, size_t len, void *outheader)
{
    bool isgstr = *(uint32_t *)buf == 'LTSG' || *(uint32_t *)buf == 'BTSG' ;

    GBINHeaderFooter *header = (GBINHeaderFooter *)((char *)buf + (isgstr?0:(len - sizeof(GBINHeaderFooter))));
    unsigned char* buff = (uint8_t *)buf;
    // GBNL?
    if ( strncmp(header->magic, "GBN", 3) && strncmp(header->magic, "GST", 3) ) {
        return 1;
    }

    // big-endian
    if ( header->magic[3] == 'B' && !(header->endian_06 & 1) ) {
        func_swapendian_len((uint8_t *)&header->field_04, 2);   // short
        func_swapendian_len((uint8_t *)&header->field_08, 4);
        func_swapendian_len((uint8_t *)&header->longsize, 4);   // native longsize, used mostly
        func_swapendian_len((uint8_t *)&header->flags, 4);
        func_swapendian_len((uint8_t *)&header->struct_offset, 4);
        func_swapendian_len((uint8_t *)&header->struct_count, 4);
        func_swapendian_len((uint8_t *)&header->struct_size, 4);
        func_swapendian_len((uint8_t *)&header->types_count, 4); // types_count is dword
        func_swapendian_len((uint8_t *)&header->types_offset, 4);
        func_swapendian_len((uint8_t *)&header->string_count, 4);
        func_swapendian_len((uint8_t *)&header->string_offset, 4);
        func_swapendian_len((uint8_t *)&header->ptrsize, 2);    // short
        int types_count = (signed __int16)header->types_count;  // Load DWORD, use short
        if ( types_count > 0 ) {
            int typesdelta = 0;
            int types_counter = (signed __int16)header->types_count;
            do {
                GBINTypeDescriptor *types = (GBINTypeDescriptor *)&buff[header->types_offset + typesdelta];
                func_swapendian_len((uint8_t *)&types->type, 2);
                func_swapendian_len((uint8_t *)&types->offset, 2);
                --types_counter;
                typesdelta += 4;
            } while ( types_counter );
        }
        int ptrsize = 4;
        int longsize = header->longsize;                               // always 4, alignment or var int32 length
        signed int struct_count = header->struct_count;
        if ( header->ptrsize )  {
            ptrsize = header->ptrsize;                          // zero or 4, affect to type4, string
        }
        for ( signed int i = 0; i < struct_count; ++i ) {
            unsigned char* structitem = 0;
            if ( i < (signed int)header->struct_count ) {
                // Every struct is header.struct_size
                structitem = &buff[header->struct_offset + header->struct_size * i];
            }
            if ( types_count > 0 ) {
                int typesdelta2 = 0;
                int types_counter = types_count;
                do {
                    func_gbin_swap_structitem(structitem, (GBINTypeDescriptor *)&buff[header->types_offset + typesdelta2], longsize, ptrsize);
                    --types_counter;
                    typesdelta2 += 4;
                } while ( types_counter );
            }
        }
        // useless in loop
        //header->endian_06 |= 1;
        header->magic[3] = 'L';
    }
    if ( header->flags & 1 ) {
        // have string table
#ifdef _DEBUG
        int stringcount = 0;
        unsigned char* structitemm1 = &buff[header->struct_offset];
        int typescount = (signed __int16)header->types_count;
        int32_t structcount = header->struct_count; // should be signed int
        int32_t structsize = header->struct_size; // signed?
        int16_t* offsets = (int16_t *)malloc(2 * typescount);
        int l = 0;
        // every struct have typescount field.
        // each field have type.type and located at type.offset
        if ( typescount > 0 ) {
            int16_t* offsetsm1 = offsets;
            GBINTypeDescriptor *lptype = (GBINTypeDescriptor *)&buff[header->types_offset];
            do {
                if ( lptype->type == gtSTRING )
                {
                    // only record string's offsets
                    ++stringcount;
                    *offsetsm1 = lptype->offset;
                    ++offsetsm1;
                }
                ++l;
                ++lptype;
            } while ( l < typescount );
        }
        // < count, += size
        for ( signed __int32 j = 0; j < (signed __int32)structcount; structitemm1 += structsize ) {
            int m = 0;
            if ( stringcount > 0 ) {
                int16_t* offsetsm2 = offsets;
                do {
                    int offset = *offsetsm2;
                    unsigned char* strmemptr = 0;
                    int stritemoff = *(uint32_t *)&structitemm1[offset];
                    // offset?
                    if ( stritemoff != -1 ) {
                        strmemptr = &buff[header->string_offset + stritemoff];
                    }
                    ++m;
                    //*(uint32_t*)&structitemm1[offset] = (uint32)strmemptr; //from file offset to memory address?! only valied for 32bit arch
                    ++offsetsm2;
                } while ( m < stringcount );
            }
            ++j;
        }
        free(offsets);
#endif
    }
    //header->endian_06 |= 2; // become 3 (mixed endian?)

    if ( outheader ) {
        memcpy(outheader, header, sizeof(GBINHeaderFooter));
    }
    return 0;
}

signed int func_gbin_to_big_endian(void *buf, size_t len)
{
    // determinate GSTL|GSTB
    bool isgstr = *(uint32_t *)buf == 'LTSG' || *(uint32_t *)buf == 'BTSG'; // no need B?

    GBINHeaderFooter *header = (GBINHeaderFooter *)((char *)buf + (isgstr?0:(len - sizeof(GBINHeaderFooter))));
    unsigned char* buff = (uint8_t *)buf;
    // GBNL?
    if ( strncmp(header->magic, "GBN", 3) && strncmp(header->magic, "GST", 3)) {
        return 1;
    }

    // big-endian
    if ( header->magic[3] == 'L' && !(header->endian_06 & 1) ) {
        int types_count = (signed __int16)header->types_count; // Load DWORD, use short
        int ptrsize = 4;
        int longsize = header->longsize;                               // always 4, alignment or var int32 length
        signed int struct_count = header->struct_count;
        if ( header->ptrsize )  {
            ptrsize = header->ptrsize;                          // zero or 4, affect to type4, string
        }
        for ( signed int i = 0; i < struct_count; ++i ) {
            unsigned char* structitem = 0;
            if ( i < (signed int)header->struct_count ) {
                // Every struct is header.struct_size
                structitem = &buff[header->struct_offset + header->struct_size * i];
            }
            if ( types_count > 0 ) {
                int typesdelta2 = 0;
                int types_counter = types_count;
                do {
                    func_gbin_swap_structitem(structitem, (GBINTypeDescriptor *)&buff[header->types_offset + typesdelta2], longsize, ptrsize);
                    --types_counter;
                    typesdelta2 += 4;
                } while ( types_counter );
            }
        }
        if ( types_count > 0 ) {
            int typesdelta = 0;
            int types_counter = (signed __int16)header->types_count;
            do {
                GBINTypeDescriptor *types = (GBINTypeDescriptor *)&buff[header->types_offset + typesdelta];
                func_swapendian_len((uint8_t *)&types->type, 2);
                func_swapendian_len((uint8_t *)&types->offset, 2);
                --types_counter;
                typesdelta += 4;
            } while ( types_counter );
        }
        func_swapendian_len((uint8_t *)&header->field_04, 2);
        func_swapendian_len((uint8_t *)&header->field_08, 4);
        func_swapendian_len((uint8_t *)&header->longsize, 4); // native long
        func_swapendian_len((uint8_t *)&header->flags, 4);
        func_swapendian_len((uint8_t *)&header->struct_offset, 4);
        func_swapendian_len((uint8_t *)&header->struct_count, 4);
        func_swapendian_len((uint8_t *)&header->struct_size, 4);
        func_swapendian_len((uint8_t *)&header->types_count, 4); // types_count is dword
        func_swapendian_len((uint8_t *)&header->types_offset, 4);
        func_swapendian_len((uint8_t *)&header->string_count, 4);
        func_swapendian_len((uint8_t *)&header->string_offset, 4);
        func_swapendian_len((uint8_t *)&header->ptrsize, 2);

        header->magic[3] = 'B';
    }

    return 0;
}

void func_swapendian_len(unsigned char *buf, int len)
{
    int swapcount = len >> 1;
    int index = 0;
    if (len >> 1 > 0) {
        unsigned char* buftail = &buf[len];
        unsigned char* bufm = buf;
        do {
            unsigned char *tail2 = &buftail[-index++];
            unsigned char b3 = *(tail2 - 1);
            *(tail2 - 1) = *bufm;
            *bufm++ = b3;
        } while (index < swapcount);
    }
}

void func_gbin_swap_structitem(unsigned char *structitems, GBINTypeDescriptor *types, int longsize, int ptrsize)
{
    switch (types->type) {
    case gtULONG:
        // must be a POD field. maybe native long
        {
            int swapcount = longsize >> 1;
            unsigned char *fieldptr = &structitems[types->offset];
            int i = 0;
            if (longsize >> 1 > 0) {
                // 4?
                unsigned char* tail = &fieldptr[longsize];
                do {
                    // useless now
                    unsigned char* looptail = &tail[-i++]; // 4
                    unsigned char b3 = *(looptail - 1); // 3<->0, 2<->1
                    *(looptail - 1) = *fieldptr; // 3 <- 0
                    *fieldptr++ = b3; // 0 -> 3
                } while (i < swapcount);
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
            int swapcount = ptrsize >> 1;
            unsigned char *offptr = &structitems[types->offset];
            int i = 0;
            if (ptrsize >> 1 > 0) {
                unsigned char* tail = &offptr[ptrsize];
                do {
                    unsigned char* tail2 = &tail[-i++];
                    unsigned char tb3 = *(tail2 - 1);
                    *(tail2 - 1) = *offptr;
                    *offptr++ = tb3;
                } while (i < swapcount);
            }
        }
        break;
    case gtSTRING:
        // size should be int_ptr
        {
            int swapcount = ptrsize >> 1;
            unsigned char* offptr = &structitems[types->offset];
            int i = 0;
            if (ptrsize >> 1 > 0) {
                unsigned char* tail = &offptr[ptrsize];
                do {
                    unsigned char* tail2 = &tail[-i++];
                    unsigned char tb3 = *(tail2 - 1);
                    *(tail2 - 1) = *offptr;
                    *offptr++ = tb3;
                } while (i < swapcount);
            }
        }
        break;
    case gtUINT64: // or double
        // fixed length
        {
            signed int swapcount = 2;
            unsigned char* t6ptr1 = &structitems[types->offset];
            unsigned char* t6ptr2 = &structitems[types->offset];
            // 7<->0
            // 6<->1
            // 5<->2
            // 4<->3
            do {
                --swapcount;
                char b7 = t6ptr1[7];
                t6ptr1[7] = *t6ptr2; // 7 <-> 0
                *t6ptr2 = b7;
                char b6 = t6ptr1[6];
                t6ptr1[6] = t6ptr2[1]; // 6 <-> 1
                t6ptr1 -= 2;
                t6ptr2[1] = b6;
                t6ptr2 += 2;
            } while (swapcount);
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

const char* GBINTypeNames[] = {
    "gtULONG",
    "gtUINT8",
    "gtUINT16",
    "gtFLOAT",
    "gtTYPE4",
    "gtSTRING",
    "gtUINT64",
    "gtFIXINT32",
};

const char* GBINType2Str( GBINType type )
{
    if (type <= gtFIXINT32) {
        return GBINTypeNames[type];
    } else {
        return 0;
    }
}

uint16_t GBINTypeFromStr( const char* name )
{
    for (uint16_t i = gtULONG; i <= gtFIXINT32; i++) {
        if (strcmp(name, GBINTypeNames[i - gtULONG]) == 0) {
            return i;
        }
    }
    return 0xFFFF;
}

bool GBINStore::LoadFromBuffer( void* buf, unsigned int size )
{
    bool isgstr = *(uint32_t *)buf == 'LTSG' || *(uint32_t *)buf == 'BTSG';
    if (isgstr) {
        fFileType = ftGSTL;
    }

    uint32_t headeroffset = isgstr?0:(size - 0x40);
    uint32_t magic = *(uint32_t *)((char*)buf + headeroffset);

    if (magic == 'BNBG') {
        fFileType = ftGBNB;
        if (func_gbin_normalize_endian(buf, size, 0)) {
            return false;
        }
    } else if (magic == 'LNBG') {
        fFileType = ftGBNL;
    }
    const char* buffer = (const char*)buf;
    const GBINHeaderFooter* header = (GBINHeaderFooter*)(buffer + headeroffset);
    const GBINTypeDescriptor* types = (GBINTypeDescriptor*)(buffer + header->types_offset);
    fMayhaveText = false;
    for (int i = 0; i < header->types_count; i++, types++) {
        GBINFieldDefineBundle field = *types;
        if (i == header->types_count - 1) {
            field.size = header->struct_size - field.offset;
        } else {
            field.size = types[1].offset - types->offset;
        }
        fFieldDefines.push_back(field);
        if (types->type == gtSTRING) {
            fMayhaveText = true;
        }
        if (types->type != gtSTRING && types->type != gtULONG && types->type != gtFLOAT) {
            qDebug() << GBINType2Str((GBINType)types->type);
        }
    }
    if ((header->flags & 1)) {
        // have string table
    }
    if (header->longsize != 4 || header->ptrsize != 4) {
        qDebug() << "longsize:" << header->longsize << "ptrsize:" << header->ptrsize;
    }

    const char* strtables = buffer + header->string_offset;
    const char* structptr = buffer + header->struct_offset;

    for (int i = 0; i < header->struct_count; i++, structptr += header->struct_size) {
        GBINComplexRecord item;
        for (GBINFieldDefineBundleVec::const_iterator it = fFieldDefines.begin(); it != fFieldDefines.end(); it++) {
            const char* fieldptr = structptr + it->offset;
            GBINFieldRecItem fielditem;
            fielditem.rawdata.append(fieldptr, it->size); // empty = assign

            if (it->type == gtULONG) {
                fielditem.u32rec = *(uint32_t*)fieldptr;
            } else if (it->type == gtSTRING) {
                uint32_t offset = *(uint32_t*)fieldptr;
                fielditem.u32rec = offset;
                fielditem.strrec.append(&strtables[offset]);
            } else {

            }
            item.push_back(fielditem);
        }
        fComplexRecords.push_back(item);
    }
    fHeader = *header;

    return true;
}

bool GBINStore::ExportTranslationText( QByteArray& buf )
{
    if (fMayhaveText == false) {
        return false;
    }
    int tf = 0;
    for (GBINFieldDefineBundleVec::const_iterator it = fFieldDefines.begin(); it != fFieldDefines.end(); it++) {
        if (it->type == gtSTRING) {
            tf++;
        }
    }
    int cw = floor(log10((double)fComplexRecords.size() * tf))+1;
    int id = 1, index = 0;
    for (GBINComplexRecordVec::const_iterator it = fComplexRecords.begin(); it != fComplexRecords.end(); it++, index++) {
        int i = 0;
        for (GBINFieldDefineBundleVec::const_iterator fit = fFieldDefines.begin(); fit != fFieldDefines.end(); fit++, i++) {
            //*it
            if (fit->type == gtSTRING) {
                buf.append(QString(";=================== %1: %2: FIELD%3\r\n").arg(id++, cw, 10, QLatin1Char('0')).arg(index).arg(i));
                buf.append((*it)[i].strrec);
                buf.append("\r\n");
            }
        }
        if (it+1 != fComplexRecords.end())
        {
            buf.append("\r\n");
        }
    }
    return true;
}

bool GBINStore::ContainsText()
{
    return fMayhaveText && (fComplexRecords.empty() == false);
}


void paddingqbytearray( QByteArray &recordsbuf, int recordsbufsize )
{
    int oldlen = recordsbuf.size();
    recordsbuf.resize(recordsbufsize);
    memset(recordsbuf.data() + oldlen, 0, recordsbufsize - oldlen);
}

bool GBINStore::SaveToBuffer( QByteArray& buf )
{
    // GBIN = Records + FieldDefine + StringTable + Header
    // GSTR = Header + Records + FieldDefine + StringTable
    GBINHeaderFooter newheader = fHeader;
    switch (fFileType) {
    case ftGSTL:
    case ftGSTB:
        *(uint32_t*)newheader.magic = 'LTSG';
        break;
    case ftGBNB:
    case ftGBNL:
        *(uint32_t*)newheader.magic = 'LNBG';
        break;
    }
    // BuildRecords
    typedef QHash<QByteArray, unsigned int> StrOffsetMap;
    QByteArray stringtable, recordsbuf;
    StrOffsetMap stroffmap;
    int index = 0;
    int strpos = 0;
    if (fMayhaveText && (fComplexRecords.empty() == false)) {
        for (GBINComplexRecordVec::iterator it = fComplexRecords.begin(); it != fComplexRecords.end(); it++, index++) {
            int i = 0;
            for (GBINFieldDefineBundleVec::const_iterator fit = fFieldDefines.begin(); fit != fFieldDefines.end(); fit++, i++) {
                if (fit->type == gtSTRING) {
                    StrOffsetMap::const_iterator sit = stroffmap.find((*it)[i].strrec);
                    if (sit == stroffmap.end()) {
                        (*it)[i].u32rec = strpos;
                        stringtable.append((*it)[i].strrec);
                        stringtable.append((char)0); // always
                        stroffmap.insert((*it)[i].strrec, strpos);
                        strpos += (*it)[i].strrec.size() + 1;
                    } else {
                        (*it)[i].u32rec = sit.value();
                    }
                    // TODO: string may have 4 or 8 byte offset (reserve for 64bit library?!)
                    (*it)[i].rawdata.fill(0);
                    *(uint32_t*)((*it)[i].rawdata.data()) = (*it)[i].u32rec;
                }
                if (fit->type == gtULONG) {
                    (*it)[i].rawdata.fill(0);
                    *(uint32_t*)((*it)[i].rawdata.data()) = (*it)[i].u32rec;
                }
                recordsbuf.append((*it)[i].rawdata);
            }
            // TODO: append zero padding?
        }
    }
    QByteArray fieldbuf;
    for (GBINFieldDefineBundleVec::const_iterator fit = fFieldDefines.begin(); fit != fFieldDefines.end(); fit++) {
        fieldbuf.append((char*)&(*fit), sizeof(GBINTypeDescriptor));
    }
    int stringtablesize = ((stringtable.size() + 15) / 16)*16;
    int recordsbufsize = ((recordsbuf.size() + 15) / 16)*16;
    int fielddefsize = ((sizeof(GBINTypeDescriptor) * fFieldDefines.size() + 15)/16)*16;
    newheader.struct_count = fComplexRecords.size();
    newheader.types_count = fFieldDefines.size();
    newheader.string_count = stroffmap.size();
    paddingqbytearray(recordsbuf, recordsbufsize);
    paddingqbytearray(fieldbuf, fielddefsize);
    paddingqbytearray(stringtable, stringtablesize);
    if (fFileType == ftGSTL || fFileType == ftGSTB) {
        // Header + Records + FieldDefine + StringTable
        newheader.struct_offset = sizeof(GBINHeaderFooter);
        newheader.types_offset = sizeof(GBINHeaderFooter) + recordsbufsize;
        newheader.string_offset = stringtablesize?sizeof(GBINHeaderFooter)+recordsbufsize+fielddefsize:0;
        buf.append((char*)&newheader, sizeof(newheader));
        buf.append(recordsbuf);
        buf.append(fieldbuf);
        buf.append(stringtable);
    } else {
        // Records + FieldDefine + StringTable + Header
        newheader.struct_offset = 0;
        newheader.types_offset = recordsbufsize;
        newheader.string_offset = stringtablesize?recordsbufsize+fielddefsize:0;
        buf.append(recordsbuf);
        buf.append(fieldbuf);
        buf.append(stringtable);
        buf.append((char*)&newheader, sizeof(newheader));
    }
    if (fFileType == ftGSTB || fFileType == ftGBNB) {
        func_gbin_to_big_endian(buf.data(), buf.size());
    }

    return true;
}

bool GBINStore::ReplaceString( int row, int column, const std::string& newstr )
{
    if (row < fComplexRecords.size() && column < fFieldDefines.size()) {
        if (fFieldDefines[column].type == gtSTRING) {
            fComplexRecords[row][column].strrec.resize(newstr.size());
            memcpy(fComplexRecords[row][column].strrec.data(), newstr.c_str(), newstr.size());
            return true;
        }
    }
    return false;
}

bool GBINStore::ReplaceULONG( int row, int column, const unsigned int newval )
{
    if (row < fComplexRecords.size() && column < fFieldDefines.size()) {
        if (fFieldDefines[column].type == gtULONG) {
            fComplexRecords[row][column].u32rec = newval;
            return true;
        }
    }
    return false;
}
