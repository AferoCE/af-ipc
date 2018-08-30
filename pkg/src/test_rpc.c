#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "af_rpc.h"

uint8_t sBuffer[256];

int sFailures = 0, sSuccesses = 0;
#define TEST(_expr) { int s = (_expr) ; printf("LINE %04d %s:%s\n", __LINE__, s ? "SUCC" : "FAIL", #_expr); if (s) sSuccesses++; else sFailures++; }

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


    TEST(AF_RPC_TYPE_IS_SIGNED_INT(AF_RPC_TYPE_UINT8) == false);
    TEST(AF_RPC_TYPE_IS_UNSIGNED_INT(AF_RPC_TYPE_UINT8) == true);
    TEST(AF_RPC_TYPE_IS_SIGNED_INT(AF_RPC_TYPE_INT8) == true);
    TEST(AF_RPC_TYPE_IS_UNSIGNED_INT(AF_RPC_TYPE_INT8) == false);

    af_rpc_param_t params8[] = {
        { (void *)(ptrdiff_t)a8, AF_RPC_TYPE_INT8 },
        { (void *)(ptrdiff_t)b8, AF_RPC_TYPE_INT8 },
        { (void *)(ptrdiff_t)c8, AF_RPC_TYPE_INT8 },
        { (void *)(ptrdiff_t)d8, AF_RPC_TYPE_INT8 },
        { (void *)(ptrdiff_t)ua8, AF_RPC_TYPE_UINT8 },
        { (void *)(ptrdiff_t)ub8, AF_RPC_TYPE_UINT8 }
    };

    size = create_and_print(sBuffer, sizeof(sBuffer), params8, ARRAY_SIZE(params8));
    TEST(size == 2 + 6 * 3);

    af_rpc_param_t paramsr8[] = {
        { 0, AF_RPC_TYPE_INT8 },
        { 0, AF_RPC_TYPE_INT8 },
        { 0, AF_RPC_TYPE_INT8 },
        { 0, AF_RPC_TYPE_INT8 },
        { 0, AF_RPC_TYPE_UINT8 },
        { 0, AF_RPC_TYPE_UINT8 },
    };

    af_rpc_param_t paramsr[6];

    int nr, pos;

    nr = af_rpc_get_params_from_buffer(paramsr8, ARRAY_SIZE(paramsr8), sBuffer, size, AF_RPC_STRICT);
    TEST(nr == 6);

    TEST((int32_t)(ptrdiff_t)paramsr8[0].base == a8);
    TEST((int32_t)(ptrdiff_t)paramsr8[1].base == b8);
    TEST((int32_t)(ptrdiff_t)paramsr8[2].base == c8);
    TEST((int32_t)(ptrdiff_t)paramsr8[3].base == d8);
    TEST((uint32_t)(ptrdiff_t)paramsr8[4].base == ua8);
    TEST((uint32_t)(ptrdiff_t)paramsr8[5].base == ub8);

    nr = af_rpc_get_params_from_buffer(paramsr, ARRAY_SIZE(paramsr), sBuffer, size, AF_RPC_PERMISSIVE);
    TEST(nr == 6);
    TEST((int32_t)(ptrdiff_t)paramsr[0].base == a8);
    TEST(paramsr[0].type == AF_RPC_TYPE_INT8);
    TEST((int32_t)(ptrdiff_t)paramsr[1].base == b8);
    TEST(paramsr[1].type == AF_RPC_TYPE_INT8);
    TEST((int32_t)(ptrdiff_t)paramsr[2].base == c8);
    TEST(paramsr[2].type == AF_RPC_TYPE_INT8);
    TEST((int32_t)(ptrdiff_t)paramsr[3].base == d8);
    TEST(paramsr[3].type == AF_RPC_TYPE_INT8);
    TEST((uint32_t)(ptrdiff_t)paramsr[4].base == ua8);
    TEST(paramsr[4].type == AF_RPC_TYPE_UINT8);
    TEST((uint32_t)(ptrdiff_t)paramsr[5].base == ub8);
    TEST(paramsr[5].type == AF_RPC_TYPE_UINT8);

    af_rpc_init_buffer(sBuffer, sizeof(sBuffer));
    pos = af_rpc_add_int8_to_buffer(sBuffer, sizeof(sBuffer), a8);
    TEST(pos == 5);
    pos = af_rpc_add_int8_to_buffer(sBuffer, sizeof(sBuffer), b8);
    TEST(pos == 8);
    pos = af_rpc_add_int8_to_buffer(sBuffer, sizeof(sBuffer), c8);
    TEST(pos == 11);
    pos = af_rpc_add_int8_to_buffer(sBuffer, sizeof(sBuffer), d8);
    TEST(pos == 14);
    pos = af_rpc_add_uint8_to_buffer(sBuffer, sizeof(sBuffer), ua8);
    TEST(pos == 17);
    pos = af_rpc_add_uint8_to_buffer(sBuffer, sizeof(sBuffer), ub8);
    TEST(pos == 20);

    print_buffer(sBuffer, pos);

    int8_t x8;
    uint8_t ux8;

    pos = af_rpc_get_int8_from_buffer_at_pos(&x8, sBuffer, sizeof(sBuffer), 0);
    TEST(pos == 5);
    TEST(x8 == a8);
    pos = af_rpc_get_int8_from_buffer_at_pos(&x8, sBuffer, sizeof(sBuffer), pos);
    TEST(pos == 8);
    TEST(x8 == b8);
    pos = af_rpc_get_int8_from_buffer_at_pos(&x8, sBuffer, sizeof(sBuffer), pos);
    TEST(pos == 11);
    TEST(x8 == c8);
    pos = af_rpc_get_int8_from_buffer_at_pos(&x8, sBuffer, sizeof(sBuffer), pos);
    TEST(pos == 14);
    TEST(x8 == d8);
    pos = af_rpc_get_uint8_from_buffer_at_pos(&ux8, sBuffer, sizeof(sBuffer), pos);
    TEST(pos == 17);
    TEST(ux8 == ua8);
    pos = af_rpc_get_uint8_from_buffer_at_pos(&ux8, sBuffer, sizeof(sBuffer), pos);
    TEST(pos == 20);
    TEST(ux8 == ub8);

    af_rpc_param_t params16[] = {
        { (void *)(ptrdiff_t)a16, AF_RPC_TYPE_INT16 },
        { (void *)(ptrdiff_t)b16, AF_RPC_TYPE_INT16 },
        { (void *)(ptrdiff_t)c16, AF_RPC_TYPE_INT16 },
        { (void *)(ptrdiff_t)d16, AF_RPC_TYPE_INT16 },
        { (void *)(ptrdiff_t)ua16, AF_RPC_TYPE_UINT16 },
        { (void *)(ptrdiff_t)ub16, AF_RPC_TYPE_UINT16 }
    };

    size = create_and_print(sBuffer, sizeof(sBuffer), params16, ARRAY_SIZE(params16));
    TEST(size == 2 + 6 * (2 + 2));

    af_rpc_param_t paramsr16[] = {
        { 0, AF_RPC_TYPE_INT16 },
        { 0, AF_RPC_TYPE_INT16 },
        { 0, AF_RPC_TYPE_INT16 },
        { 0, AF_RPC_TYPE_INT16 },
        { 0, AF_RPC_TYPE_UINT16 },
        { 0, AF_RPC_TYPE_UINT16 }
    };

    nr = af_rpc_get_params_from_buffer(paramsr16, ARRAY_SIZE(paramsr16), sBuffer, size, AF_RPC_STRICT);
    TEST(nr == 6);
    TEST((int32_t)(ptrdiff_t)paramsr16[0].base == a16);
    TEST((int32_t)(ptrdiff_t)paramsr16[1].base == b16);
    TEST((int32_t)(ptrdiff_t)paramsr16[2].base == c16);
    TEST((int32_t)(ptrdiff_t)paramsr16[3].base == d16);
    TEST((uint32_t)(ptrdiff_t)paramsr16[4].base == ua16);
    TEST((uint32_t)(ptrdiff_t)paramsr16[5].base == ub16);

    nr = af_rpc_get_params_from_buffer(paramsr, ARRAY_SIZE(paramsr), sBuffer, size, AF_RPC_PERMISSIVE);
    TEST(nr == 6);
    TEST((int32_t)(ptrdiff_t)paramsr16[0].base == a16);
    TEST(paramsr[0].type == AF_RPC_TYPE_INT16);
    TEST((int32_t)(ptrdiff_t)paramsr16[1].base == b16);
    TEST(paramsr[1].type == AF_RPC_TYPE_INT16);
    TEST((int32_t)(ptrdiff_t)paramsr16[2].base == c16);
    TEST(paramsr[2].type == AF_RPC_TYPE_INT16);
    TEST((int32_t)(ptrdiff_t)paramsr16[3].base == d16);
    TEST(paramsr[3].type == AF_RPC_TYPE_INT16);
    TEST((uint32_t)(ptrdiff_t)paramsr16[4].base == ua16);
    TEST(paramsr[4].type == AF_RPC_TYPE_UINT16);
    TEST((uint32_t)(ptrdiff_t)paramsr16[5].base == ub16);
    TEST(paramsr[5].type == AF_RPC_TYPE_UINT16);

    af_rpc_init_buffer(sBuffer, sizeof(sBuffer));
    pos = af_rpc_add_int16_to_buffer(sBuffer, sizeof(sBuffer), a16);
    TEST(pos == 6);
    pos = af_rpc_add_int16_to_buffer(sBuffer, sizeof(sBuffer), b16);
    TEST(pos == 10);
    pos = af_rpc_add_int16_to_buffer(sBuffer, sizeof(sBuffer), c16);
    TEST(pos == 14);
    pos = af_rpc_add_int16_to_buffer(sBuffer, sizeof(sBuffer), d16);
    TEST(pos == 18);
    pos = af_rpc_add_uint16_to_buffer(sBuffer, sizeof(sBuffer), ua16);
    TEST(pos == 22);
    pos = af_rpc_add_uint16_to_buffer(sBuffer, sizeof(sBuffer), ub16);
    TEST(pos == 26);

    int16_t x16;
    uint16_t ux16;

    pos = af_rpc_get_int16_from_buffer_at_pos(&x16, sBuffer, sizeof(sBuffer), 0);
    TEST(pos == 6);
    TEST(x16 == a16);
    pos = af_rpc_get_int16_from_buffer_at_pos(&x16, sBuffer, sizeof(sBuffer), pos);
    TEST(pos == 10);
    TEST(x16 == b16);
    pos = af_rpc_get_int16_from_buffer_at_pos(&x16, sBuffer, sizeof(sBuffer), pos);
    TEST(pos == 14);
    TEST(x16 == c16);
    pos = af_rpc_get_int16_from_buffer_at_pos(&x16, sBuffer, sizeof(sBuffer), pos);
    TEST(pos == 18);
    TEST(x16 == d16);
    pos = af_rpc_get_uint16_from_buffer_at_pos(&ux16, sBuffer, sizeof(sBuffer), pos);
    TEST(pos == 22);
    TEST(ux16 == ua16);
    pos = af_rpc_get_uint16_from_buffer_at_pos(&ux16, sBuffer, sizeof(sBuffer), pos);
    TEST(pos == 26);
    TEST(ux16 == ub16);

    af_rpc_param_t params32[] = {
        { (void *)(ptrdiff_t)a32, AF_RPC_TYPE_INT32 },
        { (void *)(ptrdiff_t)b32, AF_RPC_TYPE_INT32 },
        { (void *)(ptrdiff_t)c32, AF_RPC_TYPE_INT32 },
        { (void *)(ptrdiff_t)d32, AF_RPC_TYPE_INT32 },
        { (void *)(ptrdiff_t)ua32, AF_RPC_TYPE_UINT32 },
        { (void *)(ptrdiff_t)ub32, AF_RPC_TYPE_UINT32 }
    };

    size = create_and_print(sBuffer, sizeof(sBuffer), params32, ARRAY_SIZE(params32));
    TEST(size == 2 + 6 * (2 + 4));

    af_rpc_param_t paramsr32[] = {
        { 0, AF_RPC_TYPE_INT32 },
        { 0, AF_RPC_TYPE_INT32 },
        { 0, AF_RPC_TYPE_INT32 },
        { 0, AF_RPC_TYPE_INT32 },
        { 0, AF_RPC_TYPE_UINT32 },
        { 0, AF_RPC_TYPE_UINT32 }
    };

    nr = af_rpc_get_params_from_buffer(paramsr32, ARRAY_SIZE(paramsr32), sBuffer, size, AF_RPC_STRICT);
    TEST(nr == 6);
    TEST((int32_t)(ptrdiff_t)paramsr32[0].base == a32);
    TEST((int32_t)(ptrdiff_t)paramsr32[1].base == b32);
    TEST((int32_t)(ptrdiff_t)paramsr32[2].base == c32);
    TEST((int32_t)(ptrdiff_t)paramsr32[3].base == d32);
    TEST((uint32_t)(ptrdiff_t)paramsr32[4].base == ua32);
    TEST((uint32_t)(ptrdiff_t)paramsr32[5].base == ub32);

    nr = af_rpc_get_params_from_buffer(paramsr, ARRAY_SIZE(paramsr), sBuffer, size, AF_RPC_PERMISSIVE);
    TEST(nr == 6);

    TEST((int32_t)(ptrdiff_t)paramsr[0].base == a32);
    TEST(paramsr[0].type == AF_RPC_TYPE_INT32);
    TEST((int32_t)(ptrdiff_t)paramsr[1].base == b32);
    TEST(paramsr[1].type == AF_RPC_TYPE_INT32);
    TEST((int32_t)(ptrdiff_t)paramsr[2].base == c32);
    TEST(paramsr[2].type == AF_RPC_TYPE_INT32);
    TEST((int32_t)(ptrdiff_t)paramsr[3].base == d32);
    TEST(paramsr[3].type == AF_RPC_TYPE_INT32);
    TEST((uint32_t)(ptrdiff_t)paramsr[4].base == ua32);
    TEST(paramsr[4].type == AF_RPC_TYPE_UINT32);
    TEST((uint32_t)(ptrdiff_t)paramsr[5].base == ub32);
    TEST(paramsr[5].type == AF_RPC_TYPE_UINT32);

    af_rpc_init_buffer(sBuffer, sizeof(sBuffer));
    pos = af_rpc_add_int32_to_buffer(sBuffer, sizeof(sBuffer), a32);
    TEST(pos == 8);
    pos = af_rpc_add_int32_to_buffer(sBuffer, sizeof(sBuffer), b32);
    TEST(pos == 14);
    pos = af_rpc_add_int32_to_buffer(sBuffer, sizeof(sBuffer), c32);
    TEST(pos == 20);
    pos = af_rpc_add_int32_to_buffer(sBuffer, sizeof(sBuffer), d32);
    TEST(pos == 26);
    pos = af_rpc_add_uint32_to_buffer(sBuffer, sizeof(sBuffer), ua32);
    TEST(pos == 32);
    pos = af_rpc_add_uint32_to_buffer(sBuffer, sizeof(sBuffer), ub32);
    TEST(pos == 38);

    int32_t x32;
    uint32_t ux32;

    pos = af_rpc_get_int32_from_buffer_at_pos(&x32, sBuffer, sizeof(sBuffer), 0);
    TEST(pos == 8);
    TEST(x32 == a32);
    pos = af_rpc_get_int32_from_buffer_at_pos(&x32, sBuffer, sizeof(sBuffer), pos);
    TEST(pos == 14);
    TEST(x32 == b32);
    pos = af_rpc_get_int32_from_buffer_at_pos(&x32, sBuffer, sizeof(sBuffer), pos);
    TEST(pos == 20);
    TEST(x32 == c32);
    pos = af_rpc_get_int32_from_buffer_at_pos(&x32, sBuffer, sizeof(sBuffer), pos);
    TEST(pos == 26);
    TEST(x32 == d32);
    pos = af_rpc_get_uint32_from_buffer_at_pos(&ux32, sBuffer, sizeof(sBuffer), pos);
    TEST(pos == 32);
    TEST(ux32 == ua32);
    pos = af_rpc_get_uint32_from_buffer_at_pos(&ux32, sBuffer, sizeof(sBuffer), pos);
    TEST(pos == 38);
    TEST(ux32 == ub32);

    af_rpc_param_t paramscmd[] = {
        { (void *)(uint32_t)0x01, AF_RPC_TYPE_UINT8 },
        { (void *)(uint32_t)0x40, AF_RPC_TYPE_UINT8 },
        { (void *)(int32_t)-10, AF_RPC_TYPE_INT8 }
    };

    size = create_and_print(sBuffer, sizeof(sBuffer), paramscmd, ARRAY_SIZE(paramscmd));
    TEST(size == 2 + (2 + 1) + (2 + 1) + (2 + 1));

    uint8_t subsys;
    pos = af_rpc_get_uint8_from_buffer_at_pos(&subsys, sBuffer, size, 0);
    TEST(pos == 5);
    TEST(subsys == 1);

    uint8_t opcode;
    pos = af_rpc_get_uint8_from_buffer_at_pos(&opcode, sBuffer, size, pos);
    TEST(pos == 8);
    TEST(opcode == 64);

    uint8_t param1;
    pos = af_rpc_get_uint8_from_buffer_at_pos(&param1, sBuffer, size, pos);
    TEST(pos == AF_RPC_ERR_BAD_TYPE);

    uint8_t blob[] = { 0xDE, 0xAD, 0xD0, 0x0D, 0xFE, 0xED, 0xFA, 0xCE };
    uint8_t num = 0x20;
    char string[] = "My test string";

    af_rpc_param_t params_blob[] = {
        { (void *)(ptrdiff_t)num, AF_RPC_TYPE_UINT8 },
        { blob, AF_RPC_TYPE_BLOB(sizeof(blob)) },
        { string, AF_RPC_TYPE_STRING(sizeof(string)) },
        { blob, AF_RPC_TYPE_BLOB(0) }
    };

    size = create_and_print(sBuffer, sizeof(sBuffer), params_blob, ARRAY_SIZE(params_blob));
    TEST(size == 2 + (2 + sizeof(uint8_t)) + (2 + sizeof(blob)) + (2 + sizeof(string)) + 2);

    af_rpc_param_t params_blobr[] = {
        { 0, AF_RPC_TYPE_UINT8 },
        { 0, AF_RPC_TYPE_BLOB(0) },
        { 0, AF_RPC_TYPE_STRING(0) },
        { 0, AF_RPC_TYPE_BLOB(0) }
    };

    nr = af_rpc_get_params_from_buffer(params_blobr, ARRAY_SIZE(params_blobr), sBuffer, size, AF_RPC_PERMISSIVE);
    TEST(nr == 4);
    TEST((uint32_t)(ptrdiff_t)params_blobr[0].base == num);
    TEST(params_blobr[0].type == AF_RPC_TYPE_UINT8);

    TEST(AF_RPC_TYPE_IS_BLOB(params_blobr[1].type) == true);
    size = AF_RPC_BLOB_SIZE(params_blobr[1].type);
    TEST(size == sizeof(blob));
    TEST(memcmp(params_blobr[1].base, blob, size) == 0);

    TEST(AF_RPC_TYPE_IS_BLOB(params_blobr[2].type) == true);
    TEST(AF_RPC_BLOB_SIZE(params_blobr[2].type) == sizeof(string));
    TEST(strcmp((char *)params_blobr[2].base, string) == 0);

    TEST(AF_RPC_TYPE_IS_BLOB(params_blobr[3].type) == true);
    TEST(AF_RPC_BLOB_SIZE(params_blobr[3].type) == 0);

    /* blob test */
    af_rpc_init_buffer(sBuffer, sizeof(sBuffer));

    pos = af_rpc_add_blob_to_buffer(sBuffer, sizeof(sBuffer), blob, sizeof(blob));
    TEST(pos == 2 + (2 + sizeof(blob)));
    pos = af_rpc_add_blob_to_buffer(sBuffer, sizeof(sBuffer), string, sizeof(string));
    TEST(pos == 2 + (2 + sizeof(blob)) + (2 + sizeof(string)));

    /* empty blob test */
    pos = af_rpc_add_blob_to_buffer(sBuffer, sizeof(sBuffer), NULL, 0);
    TEST(pos == 2 + (2 + sizeof(blob)) + (2 + sizeof(string)) + 2);

    print_buffer (sBuffer, pos);

    uint8_t blobBuf[100];
    int len = sizeof(blobBuf);
    pos = af_rpc_get_blob_with_length_from_buffer_at_pos(blobBuf, &len, sBuffer, sizeof(sBuffer), 0);
    TEST(len == sizeof(blob));
    TEST(memcmp(blobBuf, blob, len) == 0);

    len = sizeof(blobBuf);
    pos = af_rpc_get_blob_with_length_from_buffer_at_pos(blobBuf, &len, sBuffer, sizeof(sBuffer), pos);
    TEST(len == sizeof(string));
    TEST(strcmp((char *)blobBuf, string) == 0);

    len = sizeof(blobBuf);
    pos = af_rpc_get_blob_with_length_from_buffer_at_pos(blobBuf, &len, sBuffer, sizeof(sBuffer), pos);
    TEST(len == 0);

    printf("NUMFAIL=%d PERCSUCC=%d\n", sFailures, sSuccesses * 100 / (sSuccesses + sFailures));
    return sFailures != 0;
}
