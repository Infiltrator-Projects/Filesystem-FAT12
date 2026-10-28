// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file endian.h
 * @brief Dependency-free fixed-width endian conversion and byte-access primitives.
 *
 * CPU conversion helpers operate on integer values. Load/store helpers operate
 * on byte storage and make no alignment assumptions, making them suitable for
 * packed on-disk or wire-format records. Callers must provide non-NULL storage
 * containing at least 2, 4 or 8 bytes as required; these low-level primitives
 * deliberately perform no bounds checking.
 */
#ifndef INFILTRATR_COMMON_ENDIAN_H
#define INFILTRATR_COMMON_ENDIAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Fixed-width byte swaps; each operation is its own inverse. */
static inline uint16_t infiltratr_bswap16(uint16_t value)
{
    return (uint16_t)((value >> 8) | (value << 8));
}

static inline uint32_t infiltratr_bswap32(uint32_t value)
{
    return ((value & UINT32_C(0x000000ff)) << 24) |
           ((value & UINT32_C(0x0000ff00)) << 8) |
           ((value & UINT32_C(0x00ff0000)) >> 8) |
           ((value & UINT32_C(0xff000000)) >> 24);
}

static inline uint64_t infiltratr_bswap64(uint64_t value)
{
    return ((uint64_t)infiltratr_bswap32((uint32_t)value) << 32) |
           infiltratr_bswap32((uint32_t)(value >> 32));
}

/* Host/endian conversion; input is evaluated exactly once. */
#if defined(_WIN32) || defined(__LITTLE_ENDIAN__) || \
    (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
     __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define INFILTRATR_ENDIAN_HOST_LITTLE 1
#elif defined(__BIG_ENDIAN__) || \
      (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
       __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define INFILTRATR_ENDIAN_HOST_BIG 1
#endif

#if defined(INFILTRATR_ENDIAN_HOST_LITTLE)
#define infiltratr_cpu_to_le16(value) ((uint16_t)(value))
#define infiltratr_cpu_to_le32(value) ((uint32_t)(value))
#define infiltratr_cpu_to_le64(value) ((uint64_t)(value))
#define infiltratr_le16_to_cpu(value) ((uint16_t)(value))
#define infiltratr_le32_to_cpu(value) ((uint32_t)(value))
#define infiltratr_le64_to_cpu(value) ((uint64_t)(value))
#define infiltratr_cpu_to_be16(value) infiltratr_bswap16((uint16_t)(value))
#define infiltratr_cpu_to_be32(value) infiltratr_bswap32((uint32_t)(value))
#define infiltratr_cpu_to_be64(value) infiltratr_bswap64((uint64_t)(value))
#define infiltratr_be16_to_cpu(value) infiltratr_bswap16((uint16_t)(value))
#define infiltratr_be32_to_cpu(value) infiltratr_bswap32((uint32_t)(value))
#define infiltratr_be64_to_cpu(value) infiltratr_bswap64((uint64_t)(value))
#elif defined(INFILTRATR_ENDIAN_HOST_BIG)
#define infiltratr_cpu_to_le16(value) infiltratr_bswap16((uint16_t)(value))
#define infiltratr_cpu_to_le32(value) infiltratr_bswap32((uint32_t)(value))
#define infiltratr_cpu_to_le64(value) infiltratr_bswap64((uint64_t)(value))
#define infiltratr_le16_to_cpu(value) infiltratr_bswap16((uint16_t)(value))
#define infiltratr_le32_to_cpu(value) infiltratr_bswap32((uint32_t)(value))
#define infiltratr_le64_to_cpu(value) infiltratr_bswap64((uint64_t)(value))
#define infiltratr_cpu_to_be16(value) ((uint16_t)(value))
#define infiltratr_cpu_to_be32(value) ((uint32_t)(value))
#define infiltratr_cpu_to_be64(value) ((uint64_t)(value))
#define infiltratr_be16_to_cpu(value) ((uint16_t)(value))
#define infiltratr_be32_to_cpu(value) ((uint32_t)(value))
#define infiltratr_be64_to_cpu(value) ((uint64_t)(value))
#else
static inline int infiltratr_endian_runtime_little(void)
{
    const uint16_t marker = UINT16_C(1);
    return *(const unsigned char *)&marker == 1U;
}
static inline uint16_t infiltratr_endian_cpu_to_le16_fallback(uint16_t value)
{
    return infiltratr_endian_runtime_little() ? value : infiltratr_bswap16(value);
}
static inline uint32_t infiltratr_endian_cpu_to_le32_fallback(uint32_t value)
{
    return infiltratr_endian_runtime_little() ? value : infiltratr_bswap32(value);
}
static inline uint64_t infiltratr_endian_cpu_to_le64_fallback(uint64_t value)
{
    return infiltratr_endian_runtime_little() ? value : infiltratr_bswap64(value);
}
static inline uint16_t infiltratr_endian_cpu_to_be16_fallback(uint16_t value)
{
    return infiltratr_endian_runtime_little() ? infiltratr_bswap16(value) : value;
}
static inline uint32_t infiltratr_endian_cpu_to_be32_fallback(uint32_t value)
{
    return infiltratr_endian_runtime_little() ? infiltratr_bswap32(value) : value;
}
static inline uint64_t infiltratr_endian_cpu_to_be64_fallback(uint64_t value)
{
    return infiltratr_endian_runtime_little() ? infiltratr_bswap64(value) : value;
}
#define infiltratr_cpu_to_le16(value) infiltratr_endian_cpu_to_le16_fallback((uint16_t)(value))
#define infiltratr_cpu_to_le32(value) infiltratr_endian_cpu_to_le32_fallback((uint32_t)(value))
#define infiltratr_cpu_to_le64(value) infiltratr_endian_cpu_to_le64_fallback((uint64_t)(value))
#define infiltratr_le16_to_cpu(value) infiltratr_endian_cpu_to_le16_fallback((uint16_t)(value))
#define infiltratr_le32_to_cpu(value) infiltratr_endian_cpu_to_le32_fallback((uint32_t)(value))
#define infiltratr_le64_to_cpu(value) infiltratr_endian_cpu_to_le64_fallback((uint64_t)(value))
#define infiltratr_cpu_to_be16(value) infiltratr_endian_cpu_to_be16_fallback((uint16_t)(value))
#define infiltratr_cpu_to_be32(value) infiltratr_endian_cpu_to_be32_fallback((uint32_t)(value))
#define infiltratr_cpu_to_be64(value) infiltratr_endian_cpu_to_be64_fallback((uint64_t)(value))
#define infiltratr_be16_to_cpu(value) infiltratr_endian_cpu_to_be16_fallback((uint16_t)(value))
#define infiltratr_be32_to_cpu(value) infiltratr_endian_cpu_to_be32_fallback((uint32_t)(value))
#define infiltratr_be64_to_cpu(value) infiltratr_endian_cpu_to_be64_fallback((uint64_t)(value))
#endif

/** Load unsigned little-endian integers from potentially unaligned bytes. */
static inline uint16_t infiltratr_load_le16(const void *bytes)
{
    const uint8_t *p = (const uint8_t *)bytes;
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline uint32_t infiltratr_load_le32(const void *bytes)
{
    const uint8_t *p = (const uint8_t *)bytes;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint64_t infiltratr_load_le64(const void *bytes)
{
    const uint8_t *p = (const uint8_t *)bytes;
    return (uint64_t)infiltratr_load_le32(p) |
           ((uint64_t)infiltratr_load_le32(p + 4) << 32);
}

/** Load unsigned big-endian integers from potentially unaligned bytes. */
static inline uint16_t infiltratr_load_be16(const void *bytes)
{
    const uint8_t *p = (const uint8_t *)bytes;
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline uint32_t infiltratr_load_be32(const void *bytes)
{
    const uint8_t *p = (const uint8_t *)bytes;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline uint64_t infiltratr_load_be64(const void *bytes)
{
    const uint8_t *p = (const uint8_t *)bytes;
    return ((uint64_t)infiltratr_load_be32(p) << 32) |
           (uint64_t)infiltratr_load_be32(p + 4);
}

/** Store unsigned integers into potentially unaligned little-endian bytes. */
static inline void infiltratr_store_le16(void *bytes, uint16_t value)
{
    uint8_t *p = (uint8_t *)bytes;
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static inline void infiltratr_store_le32(void *bytes, uint32_t value)
{
    uint8_t *p = (uint8_t *)bytes;
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static inline void infiltratr_store_le64(void *bytes, uint64_t value)
{
    uint8_t *p = (uint8_t *)bytes;
    infiltratr_store_le32(p, (uint32_t)value);
    infiltratr_store_le32(p + 4, (uint32_t)(value >> 32));
}

/** Store unsigned integers into potentially unaligned big-endian bytes. */
static inline void infiltratr_store_be16(void *bytes, uint16_t value)
{
    uint8_t *p = (uint8_t *)bytes;
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static inline void infiltratr_store_be32(void *bytes, uint32_t value)
{
    uint8_t *p = (uint8_t *)bytes;
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static inline void infiltratr_store_be64(void *bytes, uint64_t value)
{
    uint8_t *p = (uint8_t *)bytes;
    infiltratr_store_be32(p, (uint32_t)(value >> 32));
    infiltratr_store_be32(p + 4, (uint32_t)value);
}

#ifdef __cplusplus
}
#endif

#endif
