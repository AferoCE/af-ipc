/*
 * af_rpc.h -- RPC API definitions
 *
 * Copyright (c) 2015 Afero, Inc. All rights reserved.
 *
 * Clif Liu
 */

#ifndef __AF_RPC_H__
#define __AF_RPC_H__

#include <stdint.h>

/* The type is a uint16_t and has slightly different meanings in different contexts
 *
 * Blob type:
 *
 *   15  14  13  12  11  10  9   8   7   6   5   4   3   2   1   0
 * -----------------------------------------------------------------
 * | 0 | S | l | l | l | l | l | l | l | l | l | l | l | l | l | l |
 * -----------------------------------------------------------------
 *
 * Bit 15 is 0
 * Bit 14 is 1 if the blob can be displayed as a string (and is terminated by '\0')
 *
 * Integer type
 *
 *   15  14  13  12  11  10  9   8   7   6   5   4   3   2   1   0
 * -----------------------------------------------------------------
 * | 1 | T | T | T | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
 * -----------------------------------------------------------------
 *
 * Bits 14 to 12 are defined as follows:
 *
 *   13  12  11
 * -------------
 * | 0 | 0 | 0 |    Any sized uint (only allowed in af_rpc_get_values_for_buffer)
 * -------------
 * | 0 | 0 | 1 |    uint8
 * -------------
 * | 0 | 1 | 0 |    uin16
 * -------------
 * | 0 | 1 | 1 |    uint32
 * -------------
 * | 1 | 0 | 0 |    Any sized int (only allowed in af_rpc_get_values_for_buffer)
 * -------------
 * | 1 | 0 | 1 |    int8
 * -------------
 * | 1 | 1 | 0 |    int16
 * -------------
 * | 1 | 1 | 1 |    int32
 * -------------
 *
 */

#define AF_RPC_TYPE_FLAG_INTEGER        (0x8000)
#define AF_RPC_TYPE_INTEGER_TYPE_MASK   (0xF000)
#define AF_RPC_TYPE_FLAG_BLOB_IS_STRING (0x4000)

/* basic types */
#define AF_RPC_TYPE_UINT8       (AF_RPC_TYPE_FLAG_INTEGER | (0x1 << 12))
#define AF_RPC_TYPE_UINT16      (AF_RPC_TYPE_FLAG_INTEGER | (0x2 << 12))
#define AF_RPC_TYPE_UINT32      (AF_RPC_TYPE_FLAG_INTEGER | (0x3 << 12))
#define AF_RPC_TYPE_INT8        (AF_RPC_TYPE_FLAG_INTEGER | (0x5 << 12))
#define AF_RPC_TYPE_INT16       (AF_RPC_TYPE_FLAG_INTEGER | (0x6 << 12))
#define AF_RPC_TYPE_INT32       (AF_RPC_TYPE_FLAG_INTEGER | (0x7 << 12))

#define AF_RPC_TYPE_IS_INTEGER(_x)       (((_x) & AF_RPC_TYPE_FLAG_INTEGER) != 0)
#define AF_RPC_TYPE_IS_UNSIGNED_INT(_x)  (((_x) & 0xC000) == 0x8000)
#define AF_RPC_TYPE_IS_SIGNED_INT(_x)    (((_x) & 0xC000) == 0xC000)
#define AF_RPC_TYPE_IS_BLOB(_x)          (((_x) & AF_RPC_TYPE_FLAG_INTEGER) == 0)

#define AF_RPC_TYPE_BLOB(_size)     ((uint16_t)(_size) & 0x3fff)
#define AF_RPC_BLOB_SIZE(_type)     (_type & 0x3fff)
#define AF_RPC_BLOB_IS_STRING(_type) (((_type) & AF_RPC_TYPE_FLAG_BLOB_IS_STRING) != 0)
#define AF_RPC_TYPE_STRING(_size)   (((uint16_t)(_size) & 0x3fff) | AF_RPC_TYPE_FLAG_BLOB_IS_STRING)

#define AF_RPC_MAX_BLOB_SIZE (16383)

/*
    Parameter structure
*/

typedef struct {
    void *base;
    uint32_t type; /* stored as a uint16_t in the actual TLV structure */
} af_rpc_param_t;

/*
    The following macros allow you to set up the RPC parameters for the integer types
*/

#define AF_RPC_SET_PARAM_AS_UINT8(_param,_value) \
    { \
        _param.base = (void *)(uint32_t)_value; \
        _param.type = AF_RPC_TYPE_UINT8; \
    }

#define AF_RPC_SET_PARAM_AS_UINT16(_param,_value) \
    { \
        _param.base = (void *)(uint32_t)_value; \
        _param.type = AF_RPC_TYPE_UINT16; \
    }

#define AF_RPC_SET_PARAM_AS_UINT32(_param,_value) \
    { \
        _param.base = (void *)(uint32_t)_value; \
        _param.type = AF_RPC_TYPE_UINT32; \
    }

#define AF_RPC_SET_PARAM_AS_INT8(_param,_value) \
    { \
        _param.base = (void *)(int32_t)_value; \
        _param.type = AF_RPC_TYPE_INT8; \
    }

#define AF_RPC_SET_PARAM_AS_INT16(_param,_value) \
    { \
        _param.base = (void *)(int32_t)_value; \
        _param.type = AF_RPC_TYPE_INT16; \
    }

#define AF_RPC_SET_PARAM_AS_INT32(_param,_value) \
    { \
        _param.base = (void *)(int32_t)_value; \
        _param.type = AF_RPC_TYPE_INT32; \
    }

#define AF_RPC_SET_PARAM_AS_BLOB(_param,_blob,_len) \
    { \
        _param.base = _blob; \
        _param.type = _len & 0x3fff; \
    }

#define AF_RPC_SET_PARAM_AS_STRING(_param,_string,_len) \
    { \
        _param.base = _string; \
        _param.type = AF_RPC_TYPE_FLAG_BLOB_IS_STRING | (_len & 0x3fff); \
    }


/* Note: when the data is of uint8_t, int8_t, uint16_t, int16_t,
 *       uint32_t, int32_t type, the AF_RPC implementation stores the
 *       data in the base of af_rpc_param_t (base is not used as a
 *       pointer).
 *       The macros only work with the data that are manipulated by
 *       the AF_RPC APIs.
 */
#define AF_RPC_GET_UINT8_PARAM(_param) \
	( ((uint8_t) _param.base) )

#define AF_RPC_GET_INT8_PARAM(_param) \
	( ((int8_t) _param.base) )

#define AF_RPC_GET_UINT16_PARAM(_param) \
	( ((uint16_t) _param.base) )

#define AF_RPC_GET_INT16_PARAM(_param) \
	( ((int16_t) _param.base) )

#define AF_RPC_GET_UINT32_PARAM(_param) \
	( ((uint32_t) _param.base) )

#define AF_RPC_GET_INT32_PARAM(_param) \
	( ((int32_t) _param.base) )

#define AF_RPC_GET_BLOB_PARAM(_param) \
	( ((void *)(_param.base)) )


#define AF_RPC_ERR_BUFFER_OVERFLOW  (-1)
#define AF_RPC_ERR_BAD_PARAM        (-2)
#define AF_RPC_ERR_BAD_TYPE         (-3)
#define AF_RPC_ERR_BAD_TLV          (-4)
#define AF_RPC_ERR_TYPE_MISMATCH    (-5)
#define AF_RPC_ERR_WRONG_NUM_PARAMS (-6)

int af_rpc_create_buffer_with_params(uint8_t *buf, int bufSize, af_rpc_param_t *params, int num_params);

/*
    Building the TLV structure incrementally without the af_rpc_param_t structure

    Each of these functions returns the buffer index of the position that would hold
    the next value or a negative number indicating an error condition
*/

int af_rpc_init_buffer(uint8_t *buf, int bufSize);
int af_rpc_add_uint8_to_buffer(uint8_t *buf, int bufSize, uint8_t value);
int af_rpc_add_uint16_to_buffer(uint8_t *buf, int bufSize, uint16_t value);
int af_rpc_add_uint32_to_buffer(uint8_t *buf, int bufSize, uint32_t value);
int af_rpc_add_int8_to_buffer(uint8_t *buf, int bufSize, int8_t value);
int af_rpc_add_int16_to_buffer(uint8_t *buf, int bufSize, int16_t value);
int af_rpc_add_int32_to_buffer(uint8_t *buf, int bufSize, int32_t value);
int af_rpc_add_blob_to_buffer(uint8_t *buf, int bufSize, void *blob, int blobSize);

int af_rpc_get_size_of_type(uint16_t type);

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_a) (sizeof(_a)/sizeof(_a[0]))
#endif

#define AF_RPC_STRICT     (0)
#define AF_RPC_PERMISSIVE (1)

int af_rpc_get_params_from_buffer(af_rpc_param_t *params, int numParams, uint8_t *buf, int bufSize, int permissive);

/*
   Functions to read back the TLV structure incrementally

   Each function returns a negative number on error or the position of the next parameter if it succeeds.
   If pos is 0, these functions assume you want the first parameter (normally pos == 2).
*/

int af_rpc_get_uint8_from_buffer_at_pos(uint8_t *value, uint8_t *buf, int bufSize, int pos);
int af_rpc_get_uint16_from_buffer_at_pos(uint16_t *value, uint8_t *buf, int bufSize, int pos);
int af_rpc_get_uint32_from_buffer_at_pos(uint32_t *value, uint8_t *buf, int bufSize, int pos);
int af_rpc_get_int8_from_buffer_at_pos(int8_t *value, uint8_t *buf, int bufSize, int pos);
int af_rpc_get_int16_from_buffer_at_pos(int16_t *value, uint8_t *buf, int bufSize, int pos);
int af_rpc_get_int32_from_buffer_at_pos(int32_t *value, uint8_t *buf, int bufSize, int pos);

/*
   Pass in the maximum blob length by reference. On return the length contains the actual length of the data.
*/
int af_rpc_get_blob_with_length_from_buffer_at_pos(void *value, int *length, uint8_t *buf, int bufSize, int pos);

#endif // __AF_RPC_H__
