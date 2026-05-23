/*
 * ml_code — Error code definitions and string conversion.
 *
 * Defines ML_OK and a set of negative/positive error codes used throughout
 * clib and mlfw.  ml_code_string() converts a code to a human-readable message.
 */
#ifndef __ML_CODE_H__
#define __ML_CODE_H__

#include "ml_types.h"

enum {
    ML_OK = 0,
    ML_ETINFN = (-1),
    ML_ERROR  = 1000,
    ML_ENOMEM,
    ML_ENODST,
    ML_EPARAM,
    ML_ENOREG,
    ML_ESOCKT,
    ML_ECONNT,
    ML_EAGAIN,
    ML_ENOEXT,
    ML_EEXIST,
    ML_ENOSES,
    ML_ERRPID,
    ML_EALGOM,
    ML_EMCAST,
    ML_EJOIN,
    ML_ELEAVE,
    ML_EMORE,
    ML_EFREG,
};

/* Convert an error code to a human-readable string. */
static inline char *  ml_code_string(ml_int_t code)
{
    switch (code) {
        case ML_OK:                    { return "success"; } break;
        case ML_ERROR:                 { return "system error"; } break;
        case ML_ENOMEM:                { return "no memory error"; } break;
        case ML_ENODST:                { return "no destination error"; } break;
        case ML_EPARAM:                { return "param error"; } break;
        case ML_ENOREG:                { return "no file error"; } break;
        case ML_ESOCKT:                { return "socket error"; } break;
        case ML_ECONNT:                { return "connet error"; } break;
        case ML_EAGAIN:                { return "socket eagain"; } break;
        case ML_ENOEXT:                { return "no exist error"; } break;
        case ML_EEXIST:                { return "already exist error"; } break;
        case ML_ENOSES:                { return "no session error"; } break;
        case ML_ERRPID:                { return "pid error"; } break;
        case ML_EALGOM:                { return "alg error"; } break;
        case ML_EMCAST:                { return "ip not mcast ip"; } break;
        case ML_EJOIN:                 { return "join  mcast  error"; } break;
        case ML_ELEAVE:                { return "leave mcast  error"; } break;
        case ML_EMORE:                 { return "join  mcast  up limit "; } break;
        case ML_EFREG:                 { return "file format error "; } break;
    }
    return "unknow error";
}

#endif
