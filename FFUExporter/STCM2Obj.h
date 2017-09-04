#ifndef _STCM2_OBJECT_H
#define _STCM2_OBJECT_H

#include "STCM2Types.h"
#include <QtCore/QtCore>
#include <vector>
#include <map>
#include <string>


typedef std::vector < std::string > StringVec;

struct DialogBundle
{
    bool hasname;
    std::string name;
    int nameID;
    bool hastext;
    StringVec texts;
    int textID; // first text op
    DialogBundle():
    hasname(false), nameID(-1), hastext(false), textID(-1)
    {
    }
};

typedef std::vector<DialogBundle> DialogVec;


class STCM2Store
{
private:
    STCM2Header fHeader;
    QByteArray fHeaderExtra; // GLOBAL_DATA?
    DialogVec fDialogs;
    enum ScriptType {
        stInvalid,
        stScenario,
        stStorage,
        stEmpty
    };
    ScriptType fScriptType;
public:
    STCM2Store(void);
    ~STCM2Store(void);
private:
    ScriptType QuickAnalyst(const void* buf, unsigned int size);
public:
    bool LoadFromBuffer(const void* buf, unsigned int size);
    bool SaveToBuffer(QByteArray& buf);
    bool ContainsDialog();
};

uint32_t ExtractSTCM2ParamType(uint32_t x);
uint32_t ExtractSTCM2ParamValue(uint32_t x);
bool extractpayload( STCM2InstructionHeader* nakecode, const void* buf, int payloadsize, std::string& field, bool* hasfield, const QString& errmsg );

#endif