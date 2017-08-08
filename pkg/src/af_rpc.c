/*
 * af_rpc.c -- RPC API implementation
 *
 * Copyright (c) 2015-2016 Afero, Inc. All rights reserved.
 *
 * Clif Liu
 */

#include <arpa/inet.h>
#include <stddef.h>
#include <string.h>
#include "af_rpc.h"
#include "af_log.h"

#define LENGTH_SIZE (sizeof(uint16_t))
#define TYPE_SIZE   (sizeof(uint16_t))

/* all integers are encoded in big endian */

static void setlen(uint8_t *buf, uint16_t len)
{
    len = htons(len);
    memcpy(buf, &len, sizeof(len));
}

static uint16_t getlen(uint8_t *buf)
{
    uint16_t len;
    memcpy(&len, buf, sizeof(len));
    return ntohs(len);
}

int af_rpc_get_size_of_type(uint16_t type)
{
    static int af_size_of_int_type[8] = { -1, 1, 2, 4, -1, 1, 2, 4 };

    if (type & AF_RPC_TYPE_FLAG_INTEGER) {
        return af_size_of_int_type[(type & 0x7000) >> 12];
    } else {
        return type & 0x3fff;
    }
}

int
af_rpc_add_uint8_to_buffer(uint8_t *buf, int bufSize, uint8_t value)
{
    int pos;

    if (buf == NULL || bufSize <= 0) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    if (bufSize >= LENGTH_SIZE) {
        pos = getlen(buf);
    } else {
        AFLOG_ERR("%s:bufSize=%d:buffer too small", __FUNCTION__, bufSize);
        return AF_RPC_ERR_BAD_TLV;
    }

    if (pos + TYPE_SIZE + sizeof(value) > bufSize) {
        AFLOG_ERR("%s:bufSize=%d:buffer overflow", __FUNCTION__, bufSize);
        return AF_RPC_ERR_BUFFER_OVERFLOW;
    }

    /* Position already at current data length. Store uint type.*/
    setlen(&buf[pos], AF_RPC_TYPE_UINT8);
    pos += TYPE_SIZE;

    /* Store the value. */
    buf[pos++] = value;

    /* Set the length in the buffer. */
    setlen(buf, pos);

    return pos;
}


int
af_rpc_add_uint16_to_buffer(uint8_t *buf, int bufSize, uint16_t value)
{
    int pos;
    uint16_t rValue;

    if (buf == NULL || bufSize <= 0) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    if (bufSize >= LENGTH_SIZE) {
        pos = getlen(buf);
    } else {
        AFLOG_ERR("%s:bufSize=%d:buffer too small", __FUNCTION__, bufSize);
        return AF_RPC_ERR_BAD_TLV;
    }

    if (pos + TYPE_SIZE + sizeof(value) > bufSize) {
        AFLOG_ERR("%s:bufSize=%d:buffer overflow", __FUNCTION__, bufSize);
        return AF_RPC_ERR_BUFFER_OVERFLOW;
    }

    /* Position already at current data length. Store uint type.*/
    setlen(&buf[pos], AF_RPC_TYPE_UINT16);
    pos += TYPE_SIZE;

    /* Store the value. */
    rValue = htons(value);
    memcpy(&buf[pos], &rValue, sizeof(rValue));
    pos += sizeof(rValue);

    /* Set the length in the buffer. */
    setlen(buf, pos);

    return pos;
}

int
af_rpc_add_uint32_to_buffer(uint8_t *buf, int bufSize, uint32_t value)
{
    int pos = 0;
    uint32_t rValue;

    if (buf == NULL || bufSize <= 0) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    if (bufSize >= LENGTH_SIZE) {
        pos = getlen(buf);
    } else {
        AFLOG_ERR("%s:bufSize=%d:buffer too small", __FUNCTION__, bufSize);
        return AF_RPC_ERR_BAD_TLV;
    }

    if (pos + TYPE_SIZE + sizeof(value) > bufSize) {
        AFLOG_ERR("%s:bufSize=%d:buffer overflow", __FUNCTION__, bufSize);
        return AF_RPC_ERR_BUFFER_OVERFLOW;
    }

    /* Position already at current data length. Store uint type.*/
    setlen(&buf[pos], AF_RPC_TYPE_UINT32);
    pos += TYPE_SIZE;

    /* Store the value. */
    rValue = htonl(value);
    memcpy(&buf[pos], &rValue, sizeof(rValue));
    pos += sizeof(rValue);

    /* Set the length in the buffer. */
    setlen(buf, pos);
    return pos;
}

int
af_rpc_add_int8_to_buffer(uint8_t *buf, int bufSize, int8_t value)
{
    int pos;

    if (buf == NULL || bufSize <= 0) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    if (bufSize >= LENGTH_SIZE) {
        pos = getlen(buf);
    } else {
        AFLOG_ERR("%s:bufSize=%d:buffer too small", __FUNCTION__, bufSize);
        return AF_RPC_ERR_BAD_TLV;
    }

    if (pos + TYPE_SIZE + sizeof(value) > bufSize) {
        AFLOG_ERR("%s:bufSize=%d:buffer overflow", __FUNCTION__, bufSize);
        return AF_RPC_ERR_BUFFER_OVERFLOW;
    }

    /* Position already at current data length. Store uint type.*/
    setlen(&buf[pos], AF_RPC_TYPE_INT8);
    pos += TYPE_SIZE;

    /* Store the value. */
    buf[pos++] = *(uint8_t *)&value;

    /* Set the length in the buffer. */
    setlen(buf, pos);

    return pos;
}


int
af_rpc_add_int16_to_buffer(uint8_t *buf, int bufSize, int16_t value)
{
    int pos;
    uint16_t rValue;

    if (buf == NULL || bufSize <= 0) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    if (bufSize >= LENGTH_SIZE) {
        pos = getlen(buf);
    } else {
        AFLOG_ERR("%s:bufSize=%d:buffer too small", __FUNCTION__, bufSize);
        return AF_RPC_ERR_BAD_TLV;
    }

    if (pos + TYPE_SIZE + sizeof(value) > bufSize) {
        AFLOG_ERR("%s:bufSize=%d:buffer overflow", __FUNCTION__, bufSize);
        return AF_RPC_ERR_BUFFER_OVERFLOW;
    }

    /* Position already at current data length. Store uint type.*/
    setlen(&buf[pos], AF_RPC_TYPE_INT16);
    pos += TYPE_SIZE;

    /* Store the value. */
    rValue = htons(*(uint16_t *)&value);
    memcpy(&buf[pos], &rValue, sizeof(rValue));
    pos += sizeof(rValue);

    /* Set the length in the buffer. */
    setlen(buf, pos);

    return pos;
}

int
af_rpc_add_int32_to_buffer(uint8_t *buf, int bufSize, int32_t value)
{
    int pos = 0;
    uint32_t rValue;

    if (buf == NULL || bufSize <= 0) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    if (bufSize >= LENGTH_SIZE) {
        pos = getlen(buf);
    } else {
        AFLOG_ERR("%s:bufSize=%d:buffer too small", __FUNCTION__, bufSize);
        return AF_RPC_ERR_BAD_TLV;
    }

    if (pos + TYPE_SIZE + sizeof(value) > bufSize) {
        AFLOG_ERR("%s:bufSize=%d:buffer overflow", __FUNCTION__, bufSize);
        return AF_RPC_ERR_BUFFER_OVERFLOW;
    }

    /* Position already at current data length. Store uint type.*/
    setlen(&buf[pos], AF_RPC_TYPE_INT32);
    pos += TYPE_SIZE;

    /* Store the value. */
    rValue = htonl(*(uint32_t *)&value);
    memcpy(&buf[pos], &rValue, sizeof(rValue));
    pos += sizeof(rValue);

    /* Set the length in the buffer. */
    setlen(buf, pos);
    return pos;
}

int
af_rpc_add_blob_to_buffer(uint8_t *buf, int bufSize, void *blob, int blobSize)
{
    int pos = 0;

    if (buf == NULL || bufSize <= 0 || blob == NULL || blobSize <= 0 || blobSize > AF_RPC_MAX_BLOB_SIZE) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    if (bufSize >= LENGTH_SIZE) {
        pos = getlen(buf);
    } else {
        AFLOG_ERR("%s:bufSize=%d:buffer too small", __FUNCTION__, bufSize);
        return AF_RPC_ERR_BAD_TLV;
    }

    if (pos + TYPE_SIZE + blobSize > bufSize) {
        AFLOG_ERR("%s:bufSize=%d:buffer overflow", __FUNCTION__, bufSize);
        return AF_RPC_ERR_BUFFER_OVERFLOW;
    }

    /* Position already at current data length. Store uint type.*/
    setlen(&buf[pos], AF_RPC_TYPE_BLOB(blobSize));
    pos += TYPE_SIZE;

    /* Store the value. */
    memcpy(&buf[pos], blob, blobSize);
    pos += blobSize;

    /* Set the length in the buffer. */
    setlen(buf, pos);
    return pos;
}


int
af_rpc_create_buffer_with_params(uint8_t *buf, int bufSize, af_rpc_param_t *params, int num_params)
{
    int i, pos;
    int retVal = 0;

    if (buf == NULL || bufSize <= 0 || params == NULL || num_params < 0) {
        retVal = AF_RPC_ERR_BAD_PARAM;
        goto exit;
    }

    if (bufSize >= LENGTH_SIZE) {
        pos = LENGTH_SIZE;
    } else {
        AFLOG_ERR("af_rpc_create_buffer_with_params:overflow:bufSize=%d:buffer overflow", bufSize);
        retVal = AF_RPC_ERR_BUFFER_OVERFLOW;
        goto exit;
    }

    for (i = 0; i < num_params; i++) {
        uint16_t type = (uint16_t )params[i].type;

        /* Get the size of the type */
        int size = af_rpc_get_size_of_type(type);
        if (size < 0 || size > AF_RPC_MAX_BLOB_SIZE) {
            AFLOG_ERR("af_rpc_create_buffer_with_params:bad_type:param=%d,type=%04x:bad type", i, type);
            retVal = AF_RPC_ERR_BAD_TYPE;
            goto exit;
        }

        if (pos + sizeof(uint16_t) + size > bufSize) {
            AFLOG_ERR("af_rpc_create_buffer_with_params:overflow:param=%d:buffer overflow", i);
            retVal = AF_RPC_ERR_BUFFER_OVERFLOW;
            goto exit;
        }

        if (AF_RPC_TYPE_IS_INTEGER(type)) {

            /* make sure no other bits are set */
            if ((type & AF_RPC_TYPE_INTEGER_TYPE_MASK) != type) {
                AFLOG_ERR("af_rpc_create_buffer_with_params:bad_int1:param=%d,type=%04x:bad integer type", i, type);
                retVal = AF_RPC_ERR_BAD_TYPE;
                goto exit;
            }

            /* store the integer type */
            setlen(&buf[pos], type);
            pos += sizeof(uint16_t);

            switch (type) {
                case AF_RPC_TYPE_INT8 :
                case AF_RPC_TYPE_UINT8 :
                    buf[pos++] = (uint8_t)(ptrdiff_t)params[i].base;
                    break;
                case AF_RPC_TYPE_INT16 :
                case AF_RPC_TYPE_UINT16 :
                {
                    uint16_t value;
                    value = htons((uint16_t)(ptrdiff_t)params[i].base);
                    memcpy(&buf[pos], &value, sizeof(value));
                    pos += 2;
                    break;
                }
                case AF_RPC_TYPE_INT32 :
                case AF_RPC_TYPE_UINT32 :
                {
                    uint32_t value;
                    value = htonl((uint32_t)(ptrdiff_t)params[i].base);
                    memcpy(&buf[pos], &value, sizeof(value));
                    pos += 4;
                    break;
                }
                default :
                    /* bad integer type */
                    AFLOG_ERR("af_rpc_create_buffer_with_params:bad_int2:param=%d,type=%04x:bad integer type", i, type);
                    retVal = AF_RPC_ERR_BAD_TYPE;
                    goto exit;
                    // break;
            }
        } else {
            /* store the type */
            setlen(&buf[pos], type);
            pos += sizeof(uint16_t);

            /* check and store blob value */
            if (params[i].base == NULL) {
                AFLOG_ERR("af_rpc_create_buffer_with_params:null:param=%d:null param", i);
                retVal = AF_RPC_ERR_BAD_PARAM;
                goto exit;
            }
            memcpy(&buf[pos], params[i].base, size);
            pos += size;
        }
    }

    setlen(buf, pos);
    return pos;

exit:
    return retVal;
}

int af_rpc_get_params_from_buffer(af_rpc_param_t *params, int numParams, uint8_t *buf, int bufSize, int permissive)
{
    int param = 0, pos = 0, len;
    int retVal = 0;

    if (params == NULL || numParams < 0 || buf == NULL || bufSize <= 0) {
        retVal = AF_RPC_ERR_BAD_PARAM;
        goto exit;
    }

    if (bufSize >= LENGTH_SIZE) {
        len = getlen(buf);
        pos += LENGTH_SIZE;
    } else {
        AFLOG_ERR("af_rpc_get_params_from_buffer:bufSize:bufSize=%d:buffer too small", bufSize);
        retVal = AF_RPC_ERR_BAD_TLV;
        goto exit;
    }

    if (len > bufSize) {
        AFLOG_ERR("af_rpc_get_params_from_buffer:bufSize:len=%d,bufSize=%d:length exceeds buffer size", len, bufSize);
        retVal = AF_RPC_ERR_BAD_TLV;
        goto exit;
    }

    while (pos < len) {
        int type, size, expType;

        /* check for parameter overflow */
        if (param >= numParams) {
            AFLOG_ERR("af_rpc_get_params_from_buffer:param_overflow:param=%d:too many params", param);
            goto exit;
        }

        /* get the type */
        if (pos + sizeof(uint16_t) < len) {
            type = getlen(&buf[pos]);
            pos += sizeof(uint16_t);
        } else {
            AFLOG_ERR("af_rpc_get_params_from_buffer:bufSize:param=%d,pos=%d,bufSize=%d", param, pos, bufSize);
            retVal = AF_RPC_ERR_BAD_TLV;
            goto exit;
        }

        /* get the size from the type */
        size = af_rpc_get_size_of_type(type);
        if (pos + size > len) {
            AFLOG_ERR("af_rpc_get_params_from_buffer:bufSize:param=%d,pos=%d,bufSize=%d", param, pos, bufSize);
            retVal = AF_RPC_ERR_BAD_TLV;
            goto exit;
        }

        /* check type against the expected type */
        expType = params[param].type;

        if (AF_RPC_TYPE_IS_INTEGER(type)) {
            /* make sure type is a valid integer */
            if ((type & AF_RPC_TYPE_INTEGER_TYPE_MASK) != type) {
                AFLOG_ERR("af_rpc_get_params_from_buffer:bad_int1:param=%d,type=%04x:bad integer type", param, type);
                retVal = AF_RPC_ERR_BAD_TYPE;
                goto exit;
            }

            if ((permissive == 0) && (expType != type)) {
                AFLOG_ERR("af_rpc_get_params_from_buffer:type_mismatch:param=%d,type=%04x,expected=%04x:type mismatch",
                           param, type, expType);
                retVal = AF_RPC_ERR_BAD_TYPE;
                goto exit;
            }

            params[param].type = type;

            switch (type) {
                case AF_RPC_TYPE_INT8 :
                {
                    int8_t val = (int8_t)buf[pos++];
                    params[param].base = (void *)(ptrdiff_t)val;
                    break;
                }
                case AF_RPC_TYPE_UINT8 :
                {
                    uint8_t val = buf[pos++];
                    params[param].base = (void *)(ptrdiff_t)val;
                    break;
                }
                case AF_RPC_TYPE_INT16 :
                {
                    uint16_t val;
                    memcpy(&val, &buf[pos], sizeof(val));
                    val = ntohs(val);
                    uint32_t val32 = val;
                    if (val & 0x8000) {
                        val32 |= 0xffff0000;
                    }
                    params[param].base = (void *)(ptrdiff_t)val32;
                    pos += 2;
                    break;
                }
                case AF_RPC_TYPE_UINT16 :
                {
                    uint16_t val;
                    memcpy(&val, &buf[pos], sizeof(val));
                    val = ntohs(val);
                    params[param].base = (void *)(ptrdiff_t)val;
                    pos += 2;
                    break;
                }
                case AF_RPC_TYPE_INT32 :
                case AF_RPC_TYPE_UINT32 :
                {
                    uint32_t val;
                    memcpy(&val, &buf[pos], sizeof(val));
                    val = ntohl(val);
                    params[param].base = (void *)(ptrdiff_t)val;
                    pos += 4;
                    break;
                }
                default :
                    AFLOG_ERR("af_rpc_get_params_from_buffer:bad_int:param=%d,type=%04x:bad integer type",
                              param, type);
                    retVal = AF_RPC_ERR_BAD_TYPE;
                    goto exit;
                    // break;
            }
        } else { /* this is a blob */
            if (permissive == 0 && ! AF_RPC_TYPE_IS_BLOB(expType)) {
                AFLOG_ERR("af_rpc_get_params_from_buffer:type_mismatch:param=%d,type=%04x,expected=%04x:type mismatch on blob",
                           param, type, expType);
                retVal = AF_RPC_ERR_BAD_TYPE;
                goto exit;
            }

            params[param].base = &buf[pos];
            params[param].type = type;
            pos += size;
        }
        param++;
    }

    /* check if number of parameters matches */
    if ((permissive == 0) && (param != numParams)) {
        return AF_RPC_ERR_WRONG_NUM_PARAMS;
    }

    /* make sure TLV contains no extraneous characters */
    if (pos != bufSize) {
        return AF_RPC_ERR_BAD_TLV;
    }

    return param;

exit:
    return retVal;
}

int af_rpc_get_uint8_from_buffer_at_pos(uint8_t *value, uint8_t *buf, int bufSize, int pos)
{
    if (value == NULL || buf == NULL || bufSize <= LENGTH_SIZE) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    uint16_t len = getlen(buf);
    if (len > bufSize) {
        return AF_RPC_ERR_BAD_TLV;
    }

    if (pos == 0) {
        pos = LENGTH_SIZE;
    }

    if (pos > len) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    if (pos + TYPE_SIZE + sizeof(*value) > len) {
        return AF_RPC_ERR_BAD_TLV;
    }

    uint16_t type = getlen(&buf[pos]);
    if (type != AF_RPC_TYPE_UINT8) {
        return AF_RPC_ERR_BAD_TYPE;
    }
    pos += TYPE_SIZE;
    *value = buf[pos++];

    return pos;
}

int af_rpc_get_uint16_from_buffer_at_pos(uint16_t *value, uint8_t *buf, int bufSize, int pos)
{
    uint16_t iVal;

    if (value == NULL || buf == NULL || bufSize <= LENGTH_SIZE) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    uint16_t len = getlen(buf);
    if (len > bufSize) {
        return AF_RPC_ERR_BAD_TLV;
    }

    if (pos == 0) {
        pos = LENGTH_SIZE;
    }

    if (pos > len) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    if (pos + TYPE_SIZE + sizeof(*value) > len) {
        return AF_RPC_ERR_BAD_TLV;
    }

    uint16_t type = getlen(&buf[pos]);
    if (type != AF_RPC_TYPE_UINT16) {
        return AF_RPC_ERR_BAD_TYPE;
    }
    pos += TYPE_SIZE;
    memcpy(&iVal, &buf[pos], sizeof(*value));
    *value = ntohs(iVal);
    pos += sizeof(*value);
    return pos;
}

int af_rpc_get_uint32_from_buffer_at_pos(uint32_t *value, uint8_t *buf, int bufSize, int pos)
{
    uint32_t iVal;

    if (value == NULL || buf == NULL || bufSize <= LENGTH_SIZE) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    uint16_t len = getlen(buf);
    if (len > bufSize) {
        return AF_RPC_ERR_BAD_TLV;
    }

    if (pos == 0) {
        pos = LENGTH_SIZE;
    }

    if (pos > len) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    if (pos + TYPE_SIZE + sizeof(*value) > len) {
        return AF_RPC_ERR_BAD_TLV;
    }

    uint16_t type = getlen(&buf[pos]);
    if (type != AF_RPC_TYPE_UINT32) {
        return AF_RPC_ERR_BAD_TYPE;
    }
    pos += TYPE_SIZE;

    memcpy(&iVal, &buf[pos], sizeof(*value));
    *value = ntohl(iVal);

    pos += sizeof(*value);
    return pos;
}

int af_rpc_get_int8_from_buffer_at_pos(int8_t *value, uint8_t *buf, int bufSize, int pos)
{
    if (value == NULL || buf == NULL || bufSize <= LENGTH_SIZE) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    uint16_t len = getlen(buf);
    if (len > bufSize) {
        return AF_RPC_ERR_BAD_TLV;
    }

    if (pos == 0) {
        pos = LENGTH_SIZE;
    }

    if (pos > len) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    if (pos + TYPE_SIZE + sizeof(*value) > len) {
        return AF_RPC_ERR_BAD_TLV;
    }

    uint16_t type = getlen(&buf[pos]);
    if (type != AF_RPC_TYPE_INT8) {
        return AF_RPC_ERR_BAD_TYPE;
    }
    pos += TYPE_SIZE;
    *value = *(int8_t *)&buf[pos++];

    return pos;
}

int af_rpc_get_int16_from_buffer_at_pos(int16_t *value, uint8_t *buf, int bufSize, int pos)
{
    uint16_t iVal, uVal;

    if (value == NULL || buf == NULL || bufSize <= LENGTH_SIZE) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    uint16_t len = getlen(buf);
    if (len > bufSize) {
        return AF_RPC_ERR_BAD_TLV;
    }

    if (pos == 0) {
        pos = LENGTH_SIZE;
    }

    if (pos > len) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    if (pos + TYPE_SIZE + sizeof(*value) > len) {
        return AF_RPC_ERR_BAD_TLV;
    }

    uint16_t type = getlen(&buf[pos]);
    if (type != AF_RPC_TYPE_INT16) {
        return AF_RPC_ERR_BAD_TYPE;
    }
    pos += TYPE_SIZE;

    memcpy(&iVal, &buf[pos], sizeof(*value));
    uVal = ntohs(iVal);
    *value = *(int16_t *)&uVal;
    pos += sizeof(*value);

    return pos;
}

int af_rpc_get_int32_from_buffer_at_pos(int32_t *value, uint8_t *buf, int bufSize, int pos)
{
    uint32_t iVal, uVal;

    if (value == NULL || buf == NULL || bufSize <= LENGTH_SIZE) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    uint16_t len = getlen(buf);
    if (len > bufSize) {
        return AF_RPC_ERR_BAD_TLV;
    }

    if (pos == 0) {
        pos = LENGTH_SIZE;
    }

    if (pos > len) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    if (pos + TYPE_SIZE + sizeof(*value) > len) {
        return AF_RPC_ERR_BAD_TLV;
    }

    uint16_t type = getlen(&buf[pos]);
    if (type != AF_RPC_TYPE_INT32) {
        return AF_RPC_ERR_BAD_TYPE;
    }
    pos += TYPE_SIZE;

    memcpy(&iVal, &buf[pos], sizeof(*value));
    uVal = ntohl(iVal);
    *value = *(int32_t *)&uVal;

    pos += sizeof(*value);
    return pos;
}

int
af_rpc_init_buffer(uint8_t *txBuffer, int bufSize)
{
    if (txBuffer == NULL || bufSize < LENGTH_SIZE) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    /* Set the length in the buffer. */
    setlen(txBuffer, LENGTH_SIZE);
    return LENGTH_SIZE;
}

int af_rpc_get_blob_with_length_from_buffer_at_pos(void *blob, int *blobSize, uint8_t *buf, int bufSize, int pos)
{
    if (buf == NULL || bufSize <= LENGTH_SIZE || blob == NULL || blobSize == 0 || *blobSize <= 0) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    uint16_t len = getlen(buf);
    if (len > bufSize) {
        return AF_RPC_ERR_BAD_TLV;
    }

    if (pos == 0) {
        pos = LENGTH_SIZE;
    }

    if (pos > len) {
        return AF_RPC_ERR_BAD_PARAM;
    }

    uint16_t type = getlen(&buf[pos]);
    if (!AF_RPC_TYPE_IS_BLOB(type)) {
        return AF_RPC_ERR_BAD_TYPE;
    }
    pos += TYPE_SIZE;
    uint16_t size = AF_RPC_BLOB_SIZE(type);

    if (pos + size > len) {
        return AF_RPC_ERR_BAD_TLV;
    }

    if (size > *blobSize) {
        return AF_RPC_ERR_BUFFER_OVERFLOW;
    }

    memcpy(blob, &buf[pos], size);
    *blobSize = size;
    pos += size;

    return pos;
}
