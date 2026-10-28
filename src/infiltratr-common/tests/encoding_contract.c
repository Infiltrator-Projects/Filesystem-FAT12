// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file encoding_contract.c
 * @brief Portable endian and UTF-8 public-contract regression coverage.
 */
#include "infiltratr/endian.h"
#include "infiltratr/utf8.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_endian(void)
{
    assert(infiltratr_bswap16(UINT16_C(0x1234)) == UINT16_C(0x3412));
    assert(infiltratr_bswap32(UINT32_C(0x12345678)) == UINT32_C(0x78563412));
    assert(infiltratr_bswap64(UINT64_C(0x0123456789abcdef)) ==
           UINT64_C(0xefcdab8967452301));

    const uint16_t le16 = infiltratr_cpu_to_le16(UINT16_C(0x1234));
    const uint32_t le32 = infiltratr_cpu_to_le32(UINT32_C(0x12345678));
    const uint64_t le64 = infiltratr_cpu_to_le64(UINT64_C(0x0123456789abcdef));
    uint8_t bytes16[2];
    uint8_t bytes32[4];
    uint8_t bytes64[8];
    memcpy(bytes16, &le16, sizeof(bytes16));
    memcpy(bytes32, &le32, sizeof(bytes32));
    memcpy(bytes64, &le64, sizeof(bytes64));
    assert(bytes16[0] == 0x34U && bytes16[1] == 0x12U);
    assert(bytes32[0] == 0x78U && bytes32[1] == 0x56U &&
           bytes32[2] == 0x34U && bytes32[3] == 0x12U);
    assert(bytes64[0] == 0xefU && bytes64[7] == 0x01U);
    assert(infiltratr_le16_to_cpu(le16) == UINT16_C(0x1234));
    assert(infiltratr_le32_to_cpu(le32) == UINT32_C(0x12345678));
    assert(infiltratr_le64_to_cpu(le64) == UINT64_C(0x0123456789abcdef));
}

static void test_utf8(void)
{
    static const uint8_t ascii[] = {'I', 'N', 'F', 'S'};
    static const uint8_t euro[] = {0xe2U, 0x82U, 0xacU};
    static const uint8_t maximum[] = {0xf4U, 0x8fU, 0xbfU, 0xbfU};
    static const uint8_t embedded_nul[] = {'a', 0x00U, 'b'};
    static const uint8_t overlong[] = {0xc0U, 0x80U};
    static const uint8_t surrogate[] = {0xedU, 0xa0U, 0x80U};
    static const uint8_t too_large[] = {0xf4U, 0x90U, 0x80U, 0x80U};
    static const uint8_t truncated[] = {0xe2U, 0x82U};
    static const uint8_t stray[] = {0x80U};

    assert(infiltratr_utf8_validate(NULL, 0U));
    assert(!infiltratr_utf8_validate(NULL, 1U));
    assert(infiltratr_utf8_validate(ascii, sizeof(ascii)));
    assert(infiltratr_utf8_validate(euro, sizeof(euro)));
    assert(infiltratr_utf8_validate(maximum, sizeof(maximum)));
    assert(infiltratr_utf8_validate(embedded_nul, sizeof(embedded_nul)));
    assert(!infiltratr_utf8_validate(overlong, sizeof(overlong)));
    assert(!infiltratr_utf8_validate(surrogate, sizeof(surrogate)));
    assert(!infiltratr_utf8_validate(too_large, sizeof(too_large)));
    assert(!infiltratr_utf8_validate(truncated, sizeof(truncated)));
    assert(!infiltratr_utf8_validate(stray, sizeof(stray)));

    char encoded[4] = {(char)0xaa, (char)0xbb, (char)0xcc, (char)0xdd};
    size_t encoded_length = 99U;
    assert(infiltratr_utf8_encode_codepoint(0x24U, encoded,
                                             sizeof(encoded), &encoded_length));
    assert(encoded_length == 1U && (unsigned char)encoded[0] == 0x24U);
    assert(infiltratr_utf8_encode_codepoint(0x20acU, encoded,
                                             sizeof(encoded), &encoded_length));
    assert(encoded_length == 3U &&
           (unsigned char)encoded[0] == 0xe2U &&
           (unsigned char)encoded[1] == 0x82U &&
           (unsigned char)encoded[2] == 0xacU);
    assert(infiltratr_utf8_encode_codepoint(0x10ffffU, encoded,
                                             sizeof(encoded), &encoded_length));
    assert(encoded_length == 4U &&
           (unsigned char)encoded[0] == 0xf4U &&
           (unsigned char)encoded[1] == 0x8fU &&
           (unsigned char)encoded[2] == 0xbfU &&
           (unsigned char)encoded[3] == 0xbfU);

    encoded[0] = (char)0x55;
    encoded_length = 77U;
    assert(!infiltratr_utf8_encode_codepoint(0xd800U, encoded,
                                              sizeof(encoded), &encoded_length));
    assert((unsigned char)encoded[0] == 0x55U && encoded_length == 77U);
    assert(!infiltratr_utf8_encode_codepoint(0x110000U, encoded,
                                              sizeof(encoded), &encoded_length));
    assert((unsigned char)encoded[0] == 0x55U && encoded_length == 77U);
    assert(!infiltratr_utf8_encode_codepoint(0x20acU, encoded, 2U,
                                              &encoded_length));
    assert((unsigned char)encoded[0] == 0x55U && encoded_length == 77U);
}

int main(void)
{
    test_endian();
    test_utf8();
    puts("Libraries endian/UTF-8 contract tests passed.");
    return 0;
}
