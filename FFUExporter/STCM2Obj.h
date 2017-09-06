#ifndef _STCM2_OBJECT_H
#define _STCM2_OBJECT_H

#include "STCM2Types.h"
#include <QtCore/QtCore>
#include <vector>
#include <map>
#include <string>
#include <qatomic.h>


typedef std::vector < std::string > StringVec;

struct DialogBundle
{
    bool hasname;
    std::string name;
    int nameID;
    bool hastext;
    StringVec texts;
    int textID; // first text op
    DialogBundle():  hasname(false), nameID(-1), hastext(false), textID(-1) {
    }
};

typedef std::vector<DialogBundle> DialogVec;
typedef std::vector<unsigned char> ByteVec;

struct STCM2DataEx: STCM2Data
{
    ByteVec body;
    STCM2DataEx& operator = (const STCM2Data& base) {
        memcpy(&this->type, &base.type, sizeof(base));
        return *this;
    }

};

struct STCM2ParameterEx: STCM2Parameter 
{
    // may have local data, but we only store 4BA/4BC's data? (other op use codebundle's payload as is)
    STCM2DataEx data;
    bool textInLocalPayload;
    int payloadDelta;
    // or point to another op (by ID?)
    bool linkedToID;
    int linkedID;
    STCM2ParameterEx(const STCM2Parameter& base) : textInLocalPayload(false), payloadDelta(-1), linkedToID(false), linkedID(-1) {
        memcpy(&this->param_0, &base.param_0, sizeof(base));
    }
    STCM2ParameterEx() : textInLocalPayload(false), payloadDelta(-1), linkedToID(false), linkedID(-1) {
        memset(&this->param_0, 0, sizeof(STCM2Parameter));
    }

};

typedef std::vector<STCM2ParameterEx> ParameterExVec;

struct CodeBundle {
    STCM2InstructionHeader code;
    int codeID;
    ParameterExVec params;
    ByteVec payload;
};

typedef std::vector<CodeBundle> CodeBundleVec;

struct STCM2ExportEntryEx: STCM2ExportEntry
{
    bool linkedToID;
    int linkedID;
    STCM2ExportEntryEx(const STCM2ExportEntry& base) : linkedToID(false), linkedID(-1) {
        memcpy(&this->type, &base.type, sizeof(base));
    }
    STCM2ExportEntryEx() : linkedToID(false), linkedID(-1) {
        memset(&this->type, 0, sizeof(STCM2ExportEntry));
    }
};

typedef std::vector<STCM2ExportEntryEx> ExportEntryExVec;
typedef std::map<int, int> DualIntMap;

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
    CodeBundleVec fNakedCodes;
    ExportEntryExVec fExports;
    QAtomicInt fInstID;
    QAtomicInt fDataID;
    DualIntMap fOffsetIDMap;
public:
    STCM2Store(void);
    ~STCM2Store(void);
private:
    ScriptType QuickAnalyst(const void* buf, unsigned int size);
    bool ParseNakedCodes(const void* buf, unsigned int size);
    int GenerateID();
    int GenerateID2();
    bool FixupOffsets();
public:
    bool LoadFromBuffer(const void* buf, unsigned int size);
    bool ExportTranslationText(QByteArray& buf);
    bool ContainsDialog();
    bool SaveToBuffer(QByteArray& buf);
    bool ReplaceDialogueDebug(int textID, const std::string& newstr);
};

uint32_t ExtractSTCM2ParamType(uint32_t x);
uint32_t ExtractSTCM2ParamValue(uint32_t x);
bool extractpayload( STCM2InstructionHeader* nakecode, const void* buf, int payloadsize, std::string& field, bool* hasfield, const QString& errmsg );

#endif