#ifndef _COMMON_H_
#define _COMMON_H_

enum TLogType { 
    ltHint, ltDebug, ltMessage, ltError
};

enum TStreamCodec {
    scUnknown = -1,
    scStored = 0,
    scSHRUNK,
    scReduced1,
    scReduced2,
    scReduced3,
    scReduced4,
    scImploded,
    scToken,
    scDeflate,
    scDeflate64
};

#endif
