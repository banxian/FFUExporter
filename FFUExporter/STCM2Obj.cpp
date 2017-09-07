#include "STCM2Obj.h"
#include "AddonFuncUnt.h"
#include <string>
#include <vector>
#include <xutility>


STCM2Store::STCM2Store(void) : fInstID(0), fDataID(0)
{
}

STCM2Store::~STCM2Store(void)
{
}

bool STCM2Store::LoadFromBuffer( const void* buf, unsigned int size )
{
    if (size < sizeof(STCM2Header)) {
        return false;
    }
    STCM2Header header = *(STCM2Header*)buf;
    if (strncmp(header.magic, "STCM2", 5u)) {
        return false;
    }

    // TODO: store codestartpos
    const char* codestart = (char*)memmem(buf, size, "CODE_START_", sizeof("CODE_START_"));
    if (codestart == 0) {
        return false;
    }

    fScriptType = QuickAnalyst(buf, size);

    ParseNakedCodes(buf, size);

    int codestartpos = codestart - (char*)buf;

    int nakeoppos = codestartpos + sizeof("CODE_START_");
    int exportdatapos = header.export_offset?header.export_offset - sizeof("EXPORT_DATA"):size;
    int opsize;
    // TODO: use fOffsetIDMap instead opID (slower?)
    int opID = 1;

    for (int oppos = nakeoppos; oppos < exportdatapos; oppos += opsize, opID++) {
        STCM2InstructionHeader* nakecode = (STCM2InstructionHeader*)((char*)buf + oppos);
        opsize = nakecode->size;
        int payloadsize = nakecode->size - sizeof(STCM2InstructionHeader) - nakecode->param_count * sizeof(STCM2Parameter);

        if (nakecode->is_call) {
            continue;
        }
        if (nakecode->opcode_or_offset == SET_SPEAKERNAME) {
            // combine name and texts
            bool intext = false;
            DialogBundle text;
            text.name = "NULL";
            // name's first param is a text?
            if (extractpayload(nakecode, buf, payloadsize, text.name, &text.hasname, QString("name param is not file offset at %1").arg(oppos, 8, 16, QLatin1Char('0')))) {
                if (text.nameID == -1) {
                    text.nameID = opID;
                }
            }
            for (oppos += opsize, opID++; oppos < exportdatapos; oppos += opsize, opID++) {
                STCM2InstructionHeader* nakecode = (STCM2InstructionHeader*)((char*)buf + oppos);
                opsize = nakecode->size;
                int payloadsize = nakecode->size - sizeof(STCM2InstructionHeader) - nakecode->param_count * sizeof(STCM2Parameter);

                if (nakecode->is_call) {
                    continue;
                }
                if (nakecode->opcode_or_offset == SET_SPEAKERNAME && intext == false) {
                    // bug
                    qDebug() << QString("double take name opcode at %1").arg(oppos, 8, 16, QLatin1Char('0'));
                    fDialogs.push_back(text);
                    // finish this one and wait real one
                    //oppos -= opsize; // sub wrong size and come back after break (oppos += opsize, opID++)
                    opsize = 0; // more buggy
                    opID--;
                    break;
                } else if (nakecode->opcode_or_offset == ADD_DIALOGUE) {
                    intext = true;
                    std::string line = "LINE";
                    if (extractpayload(nakecode, buf, payloadsize, line, &text.hastext, QString("line param is not file offset at %1").arg(oppos, 8, 16, QLatin1Char('0')))) {
                        if (text.textID == -1) {
                            text.textID = opID;
                        }
                        text.texts.push_back(line);
                    }
                } else if (intext) {
                    // 4BB?
                    fDialogs.push_back(text);
                    if (nakecode->opcode_or_offset != FINISH_SPEAK) {
                        qDebug() << QString("opcode: %1 at %2").arg(nakecode->opcode_or_offset, 8, 16, QLatin1Char('0')).arg(oppos, 8, 16, QLatin1Char('0'));
                    }
                    break;
                } else {
                    // 462, bypass and wait to text
                }
            }
        } else if (nakecode->opcode_or_offset == ADD_DIALOGUE) {
            if (fScriptType != stStorage) {
                qDebug() << QString("wild line found at %1").arg(oppos, 8, 16, QLatin1Char('0'));
            }
            DialogBundle text;
            text.name = "NULL";
            std::string line = "LINE";
            if (extractpayload(nakecode, buf, payloadsize, line, &text.hastext, QString("line param is not file offset at %1").arg(oppos, 8, 16, QLatin1Char('0')))) {
                text.textID = opID;
                text.texts.push_back(line);
                fDialogs.push_back(text);
            }
        }
    }

    return true;
}

bool STCM2Store::ExportTranslationText( QByteArray& buf )
{
    int index = 1;
    int context = 1;
    //int width = fDialogs.empty()?0:(floor(log10((double)max(fDialogs.back().nameID, fDialogs.back().textID)))+1);
    if (fScriptType == stScenario) {
        int cw = floor(log10((double)fDialogs.size() * 2))+1;
        buf.append(QString("// %1 dialogue\r\n").arg(fDialogs.size()));
        for (DialogVec::const_iterator it = fDialogs.begin(); it != fDialogs.end(); it++, index++) {
            // almost dialog have both name and text, but some buggy script lost one of them
            if (it->hasname) {
                buf.append(QString(";=================== %1: %2: NAME%3\r\n").arg(context++, cw, 10, QLatin1Char('0')).arg(it->nameID).arg(index));
                buf.append(it->name.c_str());
                buf.append("\r\n");
            }
            if (it->hastext) {
                buf.append(QString(";=================== %1: %2: TEXTS%3[%4]\r\n").arg(context++, cw, 10, QLatin1Char('0')).arg(it->textID).arg(index).arg(it->texts.size()));
                for (StringVec::const_iterator sit = it->texts.begin(); sit != it->texts.end(); sit++) {
                    if (sit!=it->texts.begin()) {
                        buf.append("\\n");
                    }
                    buf.append(sit->c_str());
                }
                buf.append("\r\n");
            }
            if (it+1 != fDialogs.end())
            {
                buf.append("\r\n");
            }
        }
    }
    if (fScriptType == stStorage) {
        int cw = floor(log10((double)fDialogs.size()))+1;
        // every line separate?!
        buf.append(QString("// %1 records\r\n").arg(fDialogs.size()));
        for (DialogVec::const_iterator it = fDialogs.begin(); it != fDialogs.end(); it++, index++) {
            // hasname and hastext never co-exist
            if (it->hasname) {
                buf.append(QString(";=================== %1: %2: NAME%3\r\n").arg(context++, cw, 10, QLatin1Char('0')).arg(it->nameID).arg(index));
                buf.append(it->name.c_str());
                buf.append("\r\n");
            }
            if (it->hastext) {
                buf.append(QString(";=================== %1: %2: TEXT%3\r\n").arg(context++, cw, 10, QLatin1Char('0')).arg(it->nameID).arg(index));
                StringVec::const_iterator sit = it->texts.begin();
                if (sit != it->texts.end()){
                    buf.append(sit->c_str());
                    buf.append("\r\n");
                } else {
                    buf.append("ERROR");
                    buf.append("\r\n");
                }
            }
            if (it+1 != fDialogs.end())
            {
                buf.append("\r\n");
            }

        }
    }
    return true;
}

bool STCM2Store::ContainsDialog()
{
    return fDialogs.empty() == false;
}

STCM2Store::ScriptType STCM2Store::QuickAnalyst(const void* buf, unsigned int size)
{
    STCM2Header header = *(STCM2Header*)buf;

    const char* codestart = (char*)memmem(buf, size, "CODE_START_", sizeof("CODE_START_"));
    if (codestart == 0) {
        return stInvalid;
    }

    int codestartpos = codestart - (char*)buf;

    int nakeoppos = codestartpos + sizeof("CODE_START_");
    int exportdatapos = header.export_offset?header.export_offset - sizeof("EXPORT_DATA"):size;
    int opsize;
    int namecount = 0, dialoguecount = 0;

    for (int oppos = nakeoppos; oppos < exportdatapos; oppos += opsize) {
        STCM2InstructionHeader* nakecode = (STCM2InstructionHeader*)((char*)buf + oppos);
        opsize = nakecode->size;
        int payloadsize = nakecode->size - sizeof(STCM2InstructionHeader) - nakecode->param_count * sizeof(STCM2Parameter);

        if (nakecode->is_call) {
            continue;
        }
        if (nakecode->opcode_or_offset == SET_SPEAKERNAME && payloadsize) {
            namecount++;
        } else if (nakecode->opcode_or_offset == ADD_DIALOGUE && payloadsize) {
            dialoguecount++;
        }
    }
    if (namecount && dialoguecount) {
        return stScenario;
    }
    if (namecount == 0 && dialoguecount == 0) {
        return stEmpty;
    }
    return stStorage;
}

bool STCM2Store::ParseNakedCodes( const void* buf, unsigned int size )
{
    STCM2Header header = *(STCM2Header*)buf;

    const char* codestart = (char*)memmem(buf, size, "CODE_START_", sizeof("CODE_START_"));
    if (codestart == 0) {
        return false;
    }

    int codestartpos = codestart - (char*)buf;

    fHeaderExtra = QByteArray::fromRawData((char*)buf + sizeof(STCM2Header), codestartpos - sizeof(STCM2Header));

    int nakeoppos = codestartpos + sizeof("CODE_START_");
    int exportdatapos = header.export_offset?header.export_offset - sizeof("EXPORT_DATA"):size;
    int opsize;

    DualIntMap OffsetIDMap;

    for (int oppos = nakeoppos; oppos < exportdatapos; oppos += opsize) {
        STCM2InstructionHeader* nakecode = (STCM2InstructionHeader*)((char*)buf + oppos);
        opsize = nakecode->size;
        int payloadsize = nakecode->size - sizeof(STCM2InstructionHeader) - nakecode->param_count * sizeof(STCM2Parameter);

        CodeBundle code;
        code.codeID = GenerateID();
        code.code = *nakecode;
        OffsetIDMap.insert(std::make_pair(oppos, code.codeID));

        if (nakecode->is_call == 0) {
            if (payloadsize && (nakecode->param_count == 1) && (nakecode->opcode_or_offset == SET_SPEAKERNAME || nakecode->opcode_or_offset == ADD_DIALOGUE)) {
                const STCM2Parameter* param = (STCM2Parameter*)(nakecode + 1);
                uint32_t pt = ExtractSTCM2ParamType(param->param_0);
                if (pt != MEM_OFFSET) {
                    // error!
                    qDebug() << QString("param is not file offset at %1").arg(oppos, 8, 16, QLatin1Char('0'));
                } else {
                    const STCM2Data* data = (STCM2Data*)((char*)buf + ExtractSTCM2ParamValue(param->param_0));
                    STCM2ParameterEx paramex(*param);
                    paramex.textInLocalPayload = ((STCM2Parameter*)data == (param + 1));
                    if (data->length && paramex.textInLocalPayload) {
                        // DONE: overload operator =
                        paramex.payloadDelta = 0;
                        paramex.data = *data;
                        paramex.data.body.resize(data->length);
                        memcpy(&paramex.data.body[0], (char*)(data + 1), data->length);
                    }
                    code.params.push_back(paramex);
                }
            }
        }
        // not SET_SPEAKERNAME or ADD_DIALOGUE, may be op or call
        if (nakecode->param_count && code.params.empty()) {
            const STCM2Parameter* param = (STCM2Parameter*)(nakecode + 1);
            const STCM2Parameter* paramend = param + nakecode->param_count;
            for (;param<paramend;param++) {
                STCM2ParameterEx paramex(*param);
                if (param->param_0 == COLL_LINK) {
                    qDebug() << QString("Found Collection link belong to code: %1").arg(oppos, 8, 16, QLatin1Char('0'));
                }
                uint32_t pt = ExtractSTCM2ParamType(param->param_0);
                if (pt == MEM_OFFSET) {
                    unsigned int target = ExtractSTCM2ParamValue(param->param_0);
                    char* dest = (char*)buf + target;
                    paramex.textInLocalPayload = ((dest >= (char*)paramend) && (target < oppos + opsize));
                    paramex.payloadDelta = (char*)dest - (char*)paramend;
                }
                code.params.push_back(paramex);
            }
        }
        if (payloadsize) {
            code.payload.resize(payloadsize);
            memcpy(&code.payload[0], ((STCM2Parameter*)(nakecode + 1) + nakecode->param_count), payloadsize);
        }
        fNakedCodes.push_back(code);
    }
    for (CodeBundleVec::iterator it = fNakedCodes.begin(); it != fNakedCodes.end(); it++) {
        for (ParameterExVec::iterator pit = it->params.begin(); pit != it->params.end(); pit++) {
            uint32_t pt = ExtractSTCM2ParamType(pit->param_0);
            if (pt == SPECIAL) {
                if (pit->param_0 == INSTR_PTR0 || pit->param_0 == INSTR_PTR1) {
                    // TODO: store map and??
                    DualIntMap::const_iterator lit = OffsetIDMap.find(pit->param_4);
                    if (lit != OffsetIDMap.end()) {
                        pit->linkedToID = true;
                        pit->linkedID = lit->second;
                    }
                }
                if (pit->param_0 == COLL_LINK) {
                }
            }
        }
    }
    if (header.export_count && header.export_offset) {
        STCM2ExportEntry* expent = (STCM2ExportEntry*)((char*)buf + header.export_offset);
        for (int i = 0; i < header.export_count; expent++, i++){
            STCM2ExportEntryEx expentex(*expent);
            if (expent->type == 0) {
                DualIntMap::const_iterator lit = OffsetIDMap.find(expent->offset);
                if (lit != OffsetIDMap.end()) {
                    expentex.linkedToID = true;
                    expentex.linkedID = lit->second;
                }
            }
            if (expent->type == 1) {

            }
            fExports.push_back(expentex);
        }
    }
    fHeader = header;

    return true;
}

bool STCM2Store::SaveToBuffer( QByteArray& buf )
{
    FixupOffsets();
    STCM2Header newheader = fHeader;
    QByteArray newcontent;
    newcontent.append((char*)&newheader, sizeof(newheader));
    newcontent.append(fHeaderExtra);
    newcontent.append("CODE_START_");
    newcontent.append((char)0);
    for (CodeBundleVec::iterator it = fNakedCodes.begin(); it != fNakedCodes.end(); it++) {
        newcontent.append((char*)&it->code, sizeof(it->code));
        for (ParameterExVec::iterator pit = it->params.begin(); pit != it->params.end(); pit++) {
            newcontent.append((char*)&pit->param_0, sizeof(STCM2Parameter));
        }
        if (it->payload.empty() == false) {
            newcontent.append((char*)&it->payload[0], it->payload.size());
        }
    }
    newcontent.append("EXPORT_DATA");
    newcontent.append((char)0);
    for (ExportEntryExVec::iterator it = fExports.begin(); it != fExports.end(); it++) {
        newcontent.append((char*)&it->type, sizeof(STCM2ExportEntry));
    }

    buf = newcontent;
    return true;
}

bool STCM2Store::FixupOffsets()
{
    DualIntMap IDOffsetMap;

    int oppos = sizeof(STCM2Header) + fHeaderExtra.size() + sizeof("CODE_START_");
    int opsize;
    for (CodeBundleVec::iterator it = fNakedCodes.begin(); it != fNakedCodes.end(); it++, oppos += opsize) {
        opsize = sizeof(STCM2InstructionHeader) + it->code.param_count * sizeof(STCM2Parameter) + it->payload.size();
        IDOffsetMap.insert(std::make_pair(it->codeID, oppos));
        bool shortcircuit = false;
        if (it->code.is_call == 0) {
            if ((it->code.opcode_or_offset == ADD_DIALOGUE || it->code.opcode_or_offset == SET_SPEAKERNAME) && it->params[0].textInLocalPayload) {
                it->params[0].param_0 = oppos + sizeof(STCM2InstructionHeader) + it->code.param_count * sizeof(STCM2Parameter);
                shortcircuit = true;
            }
        }
        if (shortcircuit == false && it->params.empty() == false && it->payload.empty() == false) {
            for (ParameterExVec::iterator pit = it->params.begin(); pit != it->params.end(); pit++) {
                if (pit->textInLocalPayload) {
                    pit->param_0 = oppos + sizeof(STCM2InstructionHeader) + it->code.param_count * sizeof(STCM2Parameter) + pit->payloadDelta;
                }
            }
        }
    }

    fHeader.export_offset = oppos + sizeof("EXPORT_DATA");

    for (CodeBundleVec::iterator it = fNakedCodes.begin(); it != fNakedCodes.end(); it++) {
        for (ParameterExVec::iterator pit = it->params.begin(); pit != it->params.end(); pit++) {
            if (pit->linkedToID) {
                DualIntMap::const_iterator lit = IDOffsetMap.find(pit->linkedID);
                if (lit != IDOffsetMap.end()) {
                    pit->param_4 = lit->second;
                }
            }
        }
    }
    for (ExportEntryExVec::iterator it = fExports.begin(); it != fExports.end(); it++) {
        if (it->type == 0 && it->linkedToID) {
            DualIntMap::const_iterator lit = IDOffsetMap.find(it->linkedID);
            if (lit != IDOffsetMap.end()) {
                it->offset = lit->second;
            }
        }
        // TODO: link to data
        if (it->type == 1 && it->linkedToID) {

        }
    }

    return true;
}


int STCM2Store::GenerateID()
{
    // TODO: lock
    return fInstID.fetchAndAddOrdered(1) + 1;
}

int STCM2Store::GenerateID2()
{
    return fDataID.fetchAndAddOrdered(1) + 1;
}

bool STCM2Store::ReplaceDialogueDebug( int textID, const std::string& newstr )
{
    // TODO: build ID to index loopup table after dialogue count changed
    for (CodeBundleVec::iterator it = fNakedCodes.begin(); it != fNakedCodes.end(); it++) {
        if (it->codeID == textID && it->code.is_call == 0 && (it->code.opcode_or_offset == ADD_DIALOGUE || it->code.opcode_or_offset == SET_SPEAKERNAME)) {
            CodeBundle& code = *it;
            return UpdateLocalPayloadText(code, newstr);
        }
    }
    return false;
}

bool STCM2Store::UpdateLocalPayloadText( CodeBundle &code, const std::string &newstr )
{
    // first update data
    int newlen = ((newstr.length() + 1 + 3) / 4) * 4;
    int newdwordcount = newlen / 4;
    if (code.params[0].textInLocalPayload) {
        // Update data
        code.params[0].data.length = newlen;
        code.params[0].data.offset_unit = newdwordcount;
        code.payload.resize(newlen + sizeof(STCM2Data));
        STCM2Data* data = (STCM2Data*)(&code.payload[0]);
        *data = code.params[0].data;
        char* strptr = (char*)(data + 1); // (char*)data + sizeof(STCM2Data);
        memset(strptr + newstr.length(), 0, newlen - newstr.length());
        memcpy(strptr, newstr.c_str(), newstr.length());
        code.code.size = sizeof(STCM2InstructionHeader) + code.code.param_count * sizeof(STCM2Parameter) + code.payload.size(); // or fix in FixupOffsets?
        return true;
    }
    return false;
}

bool STCM2Store::RemoveDialogueDebug( int textID )
{
    for (CodeBundleVec::iterator it = fNakedCodes.begin(); it != fNakedCodes.end(); it++) {
        if (it->codeID == textID && it->code.is_call == 0 && (it->code.opcode_or_offset == ADD_DIALOGUE)) {
            fNakedCodes.erase(it);
            return true;
        }
    }
    return false;
}

int STCM2Store::InsertDialogueDebug( int baseID, const std::string& newstr )
{
    for (CodeBundleVec::iterator it = fNakedCodes.begin(); it != fNakedCodes.end(); it++) {
        if (it->codeID == baseID && it->code.is_call == 0 && (it->code.opcode_or_offset == ADD_DIALOGUE)) {
            CodeBundle code = *it; // copy
            code.codeID = GenerateID();
            if (UpdateLocalPayloadText(code, newstr)) {
                fNakedCodes.insert(++it, code); // it got invalid after insertion. need reassign by index
                return code.codeID; // but we just return
            }
            return -1;
        }
    }
    return -1;
}

bool extractpayload( STCM2InstructionHeader* nakecode, const void* buf, int payloadsize, std::string& field, bool* hasfield, const QString& errmsg )
{
    bool result = false;
    if (payloadsize && nakecode->param_count == 1) {
        const STCM2Parameter* param = (STCM2Parameter*)(nakecode + 1);
        uint32_t pt = ExtractSTCM2ParamType(param->param_0);
        if (pt != MEM_OFFSET) {
            // error!
            qDebug() << errmsg; //QString("name param is not file offset at %1").arg(oppos, 8, 16, QLatin1Char('0'));
            field = "ERROR";
        } else {
            const STCM2Data* data = (STCM2Data*)((char*)buf + ExtractSTCM2ParamValue(param->param_0));
            if (data->length) {
                field.assign((char*)(data + 1), strlen((char*)(data + 1)));
            }
            if (hasfield){
                *hasfield = true;
            }
            result = true;
        }
    }
    return result;
}

uint32_t ExtractSTCM2ParamType(uint32_t x) {
    return x >> 30;
}
uint32_t ExtractSTCM2ParamValue(uint32_t x) {
    return x & 0x3fffffff;
}
