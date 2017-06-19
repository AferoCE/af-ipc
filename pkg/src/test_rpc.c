#include <stdio.h>
#include "af_rpc.h"

uint8_t sBuffer[256];

void print_buffer(uint8_t *buffer, int size)
{
    int i;
    for (i = 0; i < size; i++) {
        printf ((i == 0 ? "%02x" : " %02x"), buffer[i]);
    }
    printf ("\n");
}

int create_and_print(uint8_t *buffer, int bufSize, af_rpc_param_t *params, int numParams)
{
    int result = af_rpc_create_buffer_with_params(buffer, bufSize, params, numParams);
    printf ("result = %d\n", result);
    if (result < 0) {
        return result;
    }
    print_buffer (buffer, result);
    return result;
}

int main(int argc, char *argv[])
{
    int8_t a8 = 10, b8 = -10, c8 = 100, d8 = -100;
    int16_t a16 = 1000, b16 = -1000, c16 = 30000, d16 = -30000;
    int32_t a32 = 100000, b32 = -100000, c32 = 1000000000, d32 = -1000000000;
    uint8_t ua8 = 20, ub8 = 200;
    uint16_t ua16 = 1000, ub16 = 40000;
    uint32_t ua32 = 100000, ub32 = 3000000000;
    int size;

    printf("u is_int_type=%d\n", AF_RPC_TYPE_IS_SIGNED_INT(AF_RPC_TYPE_UINT8));
    printf("u is_uint_type=%d\n", AF_RPC_TYPE_IS_UNSIGNED_INT(AF_RPC_TYPE_UINT8));
    printf("i is_int_type=%d\n", AF_RPC_TYPE_IS_SIGNED_INT(AF_RPC_TYPE_INT8));
    printf("i is_uint_type=%d\n", AF_RPC_TYPE_IS_UNSIGNED_INT(AF_RPC_TYPE_INT8));

    af_rpc_param_t params8[] = {
        { (void *)(int32_t)a8, AF_RPC_TYPE_INT8 },
        { (void *)(int32_t)b8, AF_RPC_TYPE_INT8 },
        { (void *)(int32_t)c8, AF_RPC_TYPE_INT8 },
        { (void *)(int32_t)d8, AF_RPC_TYPE_INT8 },
        { (void *)(uint32_t)ua8, AF_RPC_TYPE_UINT8 },
        { (void *)(uint32_t)ub8, AF_RPC_TYPE_UINT8 }
    };

    size = create_and_print(sBuffer, sizeof(sBuffer), params8, ARRAY_SIZE(params8));

    af_rpc_param_t paramsr8[] = {
        { 0, AF_RPC_TYPE_INT8 },
        { 0, AF_RPC_TYPE_INT8 },
        { 0, AF_RPC_TYPE_INT8 },
        { 0, AF_RPC_TYPE_INT8 },
        { 0, AF_RPC_TYPE_UINT8 },
        { 0, AF_RPC_TYPE_UINT8 },
    };

    af_rpc_param_t paramsr[6];

    int nr, i, pos;

    nr = af_rpc_get_params_from_buffer(paramsr8, ARRAY_SIZE(paramsr8), sBuffer, size, 0);
    printf ("nr=%d\n", nr);
    for (i = 0; i < 4; i++) {
        printf ("paramsr8[%d] = %d\n", i, (int32_t)paramsr8[i].base);
    }
    for (i = 4; i < 6; i++) {
        printf ("paramsr8[%d] = %u\n", i, (uint32_t)paramsr8[i].base);
    }

    nr = af_rpc_get_params_from_buffer(paramsr, ARRAY_SIZE(paramsr), sBuffer, size, 1);
    printf ("nr=%d\n", nr);
    for (i = 0; i < 4; i++) {
        printf ("paramsr[%d] = %d type=%04x\n", i, (int32_t)paramsr[i].base, paramsr[i].type);
    }
    for (i = 4; i < 6; i++) {
        printf ("paramsr[%d] = %u type=%04x\n", i, (uint32_t)paramsr[i].base, paramsr[i].type);
    }

    af_rpc_init_buffer(sBuffer, sizeof(sBuffer));
    pos = af_rpc_add_int8_to_buffer(sBuffer, sizeof(sBuffer), a8);
    pos = af_rpc_add_int8_to_buffer(sBuffer, sizeof(sBuffer), b8);
    pos = af_rpc_add_int8_to_buffer(sBuffer, sizeof(sBuffer), c8);
    pos = af_rpc_add_int8_to_buffer(sBuffer, sizeof(sBuffer), d8);
    pos = af_rpc_add_uint8_to_buffer(sBuffer, sizeof(sBuffer), ua8);
    pos = af_rpc_add_uint8_to_buffer(sBuffer, sizeof(sBuffer), ub8);

    print_buffer(sBuffer, pos);

    int8_t x8;
    uint8_t ux8;

    pos = af_rpc_get_int8_from_buffer_at_pos(&x8, sBuffer, sizeof(sBuffer), 0);
    printf ("pos=%d a8=%d\n", pos, x8);
    pos = af_rpc_get_int8_from_buffer_at_pos(&x8, sBuffer, sizeof(sBuffer), pos);
    printf ("pos=%d b8=%d\n", pos, x8);
    pos = af_rpc_get_int8_from_buffer_at_pos(&x8, sBuffer, sizeof(sBuffer), pos);
    printf ("pos=%d c8=%d\n", pos, x8);
    pos = af_rpc_get_int8_from_buffer_at_pos(&x8, sBuffer, sizeof(sBuffer), pos);
    printf ("pos=%d d8=%d\n", pos, x8);
    pos = af_rpc_get_uint8_from_buffer_at_pos(&ux8, sBuffer, sizeof(sBuffer), pos);
    printf ("pos=%d ua8=%d\n", pos, ux8);
    pos = af_rpc_get_uint8_from_buffer_at_pos(&ux8, sBuffer, sizeof(sBuffer), pos);
    printf ("pos=%d ub8=%d\n", pos, ux8);

    af_rpc_param_t params16[] = {
        { (void *)(int32_t)a16, AF_RPC_TYPE_INT16 },
        { (void *)(int32_t)b16, AF_RPC_TYPE_INT16 },
        { (void *)(int32_t)c16, AF_RPC_TYPE_INT16 },
        { (void *)(int32_t)d16, AF_RPC_TYPE_INT16 },
        { (void *)(uint32_t)ua16, AF_RPC_TYPE_UINT16 },
        { (void *)(uint32_t)ub16, AF_RPC_TYPE_UINT16 }
    };

    size = create_and_print(sBuffer, sizeof(sBuffer), params16, ARRAY_SIZE(params16));

    af_rpc_param_t paramsr16[] = {
        { 0, AF_RPC_TYPE_INT16 },
        { 0, AF_RPC_TYPE_INT16 },
        { 0, AF_RPC_TYPE_INT16 },
        { 0, AF_RPC_TYPE_INT16 },
        { 0, AF_RPC_TYPE_UINT16 },
        { 0, AF_RPC_TYPE_UINT16 }
    };

    nr = af_rpc_get_params_from_buffer(paramsr16, ARRAY_SIZE(paramsr16), sBuffer, size, 0);
    printf ("nr=%d\n", nr);
    for (i = 0; i < 4; i++) {
        printf ("paramsr16[%d] = %d\n", i, (int32_t)paramsr16[i].base);
    }
    for (i = 4; i < 6; i++) {
        printf ("paramsr16[%d] = %u\n", i, (uint32_t)paramsr16[i].base);
    }

    nr = af_rpc_get_params_from_buffer(paramsr, ARRAY_SIZE(paramsr), sBuffer, size, 1);
    printf ("nr=%d\n", nr);
    for (i = 0; i < 4; i++) {
        printf ("paramsr[%d] = %d type=%04x\n", i, (int32_t)paramsr[i].base, paramsr[i].type);
    }
    for (i = 4; i < 6; i++) {
        printf ("paramsr[%d] = %u type=%04x\n", i, (uint32_t)paramsr[i].base, paramsr[i].type);
    }

    af_rpc_init_buffer(sBuffer, sizeof(sBuffer));
    pos = af_rpc_add_int16_to_buffer(sBuffer, sizeof(sBuffer), a16);
    pos = af_rpc_add_int16_to_buffer(sBuffer, sizeof(sBuffer), b16);
    pos = af_rpc_add_int16_to_buffer(sBuffer, sizeof(sBuffer), c16);
    pos = af_rpc_add_int16_to_buffer(sBuffer, sizeof(sBuffer), d16);
    pos = af_rpc_add_uint16_to_buffer(sBuffer, sizeof(sBuffer), ua16);
    pos = af_rpc_add_uint16_to_buffer(sBuffer, sizeof(sBuffer), ub16);

    int16_t x16;
    uint16_t ux16;

    pos = af_rpc_get_int16_from_buffer_at_pos(&x16, sBuffer, sizeof(sBuffer), 0);
    printf ("pos=%d a16=%d\n", pos, x16);
    pos = af_rpc_get_int16_from_buffer_at_pos(&x16, sBuffer, sizeof(sBuffer), pos);
    printf ("pos=%d b16=%d\n", pos, x16);
    pos = af_rpc_get_int16_from_buffer_at_pos(&x16, sBuffer, sizeof(sBuffer), pos);
    printf ("pos=%d c16=%d\n", pos, x16);
    pos = af_rpc_get_int16_from_buffer_at_pos(&x16, sBuffer, sizeof(sBuffer), pos);
    printf ("pos=%d d16=%d\n", pos, x16);
    pos = af_rpc_get_uint16_from_buffer_at_pos(&ux16, sBuffer, sizeof(sBuffer), pos);
    printf ("pos=%d ua16=%d\n", pos, ux16);
    pos = af_rpc_get_uint16_from_buffer_at_pos(&ux16, sBuffer, sizeof(sBuffer), pos);
    printf ("pos=%d ub16=%d\n", pos, ux16);

    af_rpc_param_t params32[] = {
        { (void *)(int32_t)a32, AF_RPC_TYPE_INT32 },
        { (void *)(int32_t)b32, AF_RPC_TYPE_INT32 },
        { (void *)(int32_t)c32, AF_RPC_TYPE_INT32 },
        { (void *)(int32_t)d32, AF_RPC_TYPE_INT32 },
        { (void *)(uint32_t)ua32, AF_RPC_TYPE_UINT32 },
        { (void *)(uint32_t)ub32, AF_RPC_TYPE_UINT32 }
    };

    size = create_and_print(sBuffer, sizeof(sBuffer), params32, ARRAY_SIZE(params32));

    af_rpc_param_t paramsr32[] = {
        { 0, AF_RPC_TYPE_INT32 },
        { 0, AF_RPC_TYPE_INT32 },
        { 0, AF_RPC_TYPE_INT32 },
        { 0, AF_RPC_TYPE_INT32 },
        { 0, AF_RPC_TYPE_UINT32 },
        { 0, AF_RPC_TYPE_UINT32 }
    };

    nr = af_rpc_get_params_from_buffer(paramsr32, ARRAY_SIZE(paramsr32), sBuffer, size, 0);
    printf ("nr=%d\n", nr);
    for (i = 0; i < 4; i++) {
        printf ("paramsr32[%d] = %d\n", i, (int32_t)paramsr32[i].base);
    }
    for (i = 4; i < 6; i++) {
        printf ("paramsr32[%d] = %u\n", i, (uint32_t)paramsr32[i].base);
    }

    nr = af_rpc_get_params_from_buffer(paramsr, ARRAY_SIZE(paramsr), sBuffer, size, 1);
    printf ("nr=%d\n", nr);
    for (i = 0; i < 4; i++) {
        printf ("paramsr[%d] = %d type=%04x\n", i, (int32_t)paramsr[i].base, paramsr[i].type);
    }
    for (i = 4; i < 6; i++) {
        printf ("paramsr[%d] = %u type=%04x\n", i, (uint32_t)paramsr[i].base, paramsr[i].type);
    }

    af_rpc_init_buffer(sBuffer, sizeof(sBuffer));
    pos = af_rpc_add_int32_to_buffer(sBuffer, sizeof(sBuffer), a32);
    pos = af_rpc_add_int32_to_buffer(sBuffer, sizeof(sBuffer), b32);
    pos = af_rpc_add_int32_to_buffer(sBuffer, sizeof(sBuffer), c32);
    pos = af_rpc_add_int32_to_buffer(sBuffer, sizeof(sBuffer), d32);
    pos = af_rpc_add_uint32_to_buffer(sBuffer, sizeof(sBuffer), ua32);
    pos = af_rpc_add_uint32_to_buffer(sBuffer, sizeof(sBuffer), ub32);

    int32_t x32;
    uint32_t ux32;

    pos = af_rpc_get_int32_from_buffer_at_pos(&x32, sBuffer, sizeof(sBuffer), 0);
    printf ("pos=%d a32=%d\n", pos, x32);
    pos = af_rpc_get_int32_from_buffer_at_pos(&x32, sBuffer, sizeof(sBuffer), pos);
    printf ("pos=%d b32=%d\n", pos, x32);
    pos = af_rpc_get_int32_from_buffer_at_pos(&x32, sBuffer, sizeof(sBuffer), pos);
    printf ("pos=%d c32=%d\n", pos, x32);
    pos = af_rpc_get_int32_from_buffer_at_pos(&x32, sBuffer, sizeof(sBuffer), pos);
    printf ("pos=%d d32=%d\n", pos, x32);
    pos = af_rpc_get_uint32_from_buffer_at_pos(&ux32, sBuffer, sizeof(sBuffer), pos);
    printf ("pos=%d ua32=%u\n", pos, ux32);
    pos = af_rpc_get_uint32_from_buffer_at_pos(&ux32, sBuffer, sizeof(sBuffer), pos);
    printf ("pos=%d ub32=%u\n", pos, ux32);

    af_rpc_param_t paramscmd[] = {
        { (void *)(uint32_t)0x01, AF_RPC_TYPE_UINT8 },
        { (void *)(uint32_t)0x40, AF_RPC_TYPE_UINT8 },
        { (void *)(int32_t)-10, AF_RPC_TYPE_INT8 }
    };

    size = create_and_print(sBuffer, sizeof(sBuffer), paramscmd, ARRAY_SIZE(paramscmd));
    if (size < 0) {
        return 1;
    }

    uint8_t subsys;
    pos = af_rpc_get_uint8_from_buffer_at_pos(&subsys, sBuffer, size, 0);
    printf ("pos = %d\n", pos);
    if (pos < 0) {
        return 1;
    }
    printf ("subsys=%d\n", subsys);

    uint8_t opcode;
    pos = af_rpc_get_uint8_from_buffer_at_pos(&opcode, sBuffer, size, pos);
    printf ("pos = %d\n", pos);
    if (pos < 0) {
        return 1;
    }
    printf ("opcode=%d\n", opcode);

    uint8_t param1;
    pos = af_rpc_get_uint8_from_buffer_at_pos(&param1, sBuffer, size, pos);
    printf ("pos = %d\n", pos);
    if (pos < 0) {
        printf("correctly found type mismatch\n");
    }

    uint8_t blob[] = { 0xDE, 0xAD, 0xD0, 0x0D, 0xFE, 0xED, 0xFA, 0xCE };
    uint8_t num = 0x20;
#define TEST_STRING "My test string"

    af_rpc_param_t params_blob[] = {
        { (void *)(uint32_t)num, AF_RPC_TYPE_UINT8 },
        { blob, AF_RPC_TYPE_BLOB(sizeof(blob)) },
        { TEST_STRING, AF_RPC_TYPE_STRING(sizeof(TEST_STRING)) }
    };

    size = create_and_print(sBuffer, sizeof(sBuffer), params_blob, ARRAY_SIZE(params_blob));

    af_rpc_param_t params_blobr[] = {
        { 0, AF_RPC_TYPE_UINT8 },
        { 0, AF_RPC_TYPE_BLOB(0) },
        { 0, AF_RPC_TYPE_STRING(0) }
    };

    nr = af_rpc_get_params_from_buffer(params_blobr, ARRAY_SIZE(params_blobr), sBuffer, size, 0);
    printf ("nr=%d\n", nr);
    printf ("rnum = %u\n", (uint32_t)params_blobr[0].base);
    size = AF_RPC_BLOB_SIZE(params_blobr[1].type);
    for (i = 0; i < params_blobr[1].type; i++) {
        printf ((i == 0 ? "%02x" : " %02x"), ((uint8_t *)params_blobr[1].base)[i]);
    }
    printf ("\n");
    printf ("rstring = \"%s\"\n", (char *)params_blobr[2].base);

    /* blob test */
    af_rpc_init_buffer(sBuffer, sizeof(sBuffer));

    pos = af_rpc_add_blob_to_buffer(sBuffer, sizeof(sBuffer), blob, sizeof(blob));
    pos = af_rpc_add_blob_to_buffer(sBuffer, sizeof(sBuffer), TEST_STRING, sizeof(TEST_STRING));

    print_buffer (sBuffer, pos);

    uint8_t blobBuf[100];
    int len = sizeof(blobBuf);
    pos = af_rpc_get_blob_with_length_from_buffer_at_pos(blobBuf, &len, sBuffer, sizeof(sBuffer), 0);
    print_buffer(blobBuf, len);

    len = sizeof(blobBuf);
    pos = af_rpc_get_blob_with_length_from_buffer_at_pos(blobBuf, &len, sBuffer, sizeof(sBuffer), pos);
    printf ("len=%d string=\"%s\"\n", len, blobBuf);

    return 0;
}
