#include "STCM2Obj.h"
#include "AddonFuncUnt.h"
#include <string>
#include <vector>


STCM2Store::STCM2Store(void)
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

    const char* codestart = (char*)memmem(buf, size, "CODE_START_", sizeof("CODE_START_"));
    if (codestart == 0) {
        return false;
    }

    fScriptType = QuickAnalyst(buf, size);

    int codestartpos = codestart - (char*)buf;

    fHeaderExtra = QByteArray::fromRawData((char*)buf + sizeof(STCM2Header), codestartpos - sizeof(STCM2Header));

    int nakeoppos = codestartpos + sizeof("CODE_START_");
    int exportdatapos = header.export_offset?header.export_offset - sizeof("EXPORT_DATA"):size;
    int opsize;
    int opID = 0;

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
            text.hasname = false;
            text.hastext = false;
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
                    // finish this one and wait real one
                    fDialogs.push_back(text);
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
            text.hasname = false;
            text.hastext = false;
            text.name = "NULL";
            std::string line = "LINE";
            if (extractpayload(nakecode, buf, payloadsize, line, &text.hastext, QString("line param is not file offset at %1").arg(oppos, 8, 16, QLatin1Char('0')))) {
                text.textID = opID;
                text.texts.push_back(line);
                fDialogs.push_back(text);
            }
        }
    }

    fHeader = header;
    return true;
}

bool STCM2Store::SaveToBuffer( QByteArray& buf )
{
    int index = 0;
    if (fScriptType == stScenario) {
        buf.append(QString("// %1 dialogue\r\n").arg(fDialogs.size()));
        for (DialogVec::const_iterator it = fDialogs.begin(); it != fDialogs.end(); it++, index++) {
            if (it->hasname) {
                buf.append(QString(";=================== %1: NAME%2\r\n").arg(it->nameID).arg(index));
                buf.append(it->name.c_str());
                buf.append("\r\n");
            }
            if (it->hastext) {
                buf.append(QString(";=================== %1: TEXTS%2[%3]\r\n").arg(it->textID).arg(index).arg(it->texts.size()));
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
        // every line separate?!
        buf.append(QString("// %1 records\r\n").arg(fDialogs.size()));
        for (DialogVec::const_iterator it = fDialogs.begin(); it != fDialogs.end(); it++, index++) {
            if (it->hasname) {
                buf.append(QString(";=================== %1: NAME%2\r\n").arg(it->nameID).arg(index));
                buf.append(it->name.c_str());
                buf.append("\r\n");
            }
            if (it->hastext) {
                buf.append(QString(";=================== %1: TEXT%3\r\n").arg(it->nameID).arg(index));
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
