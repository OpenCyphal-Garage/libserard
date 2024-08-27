/// This software is distributed under the terms of the MIT License.
/// Copyright (c) 2022-2024 OpenCyphal.
/// Author: Pavel Kirienko <pavel@opencyphal.org>
/// Author: Kalyan Sriram <coder.kalyan@gmail.com>

#include "serard.h"
#include "_serard_cavl.h"
#include <bits/stdint-uintn.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <stdio.h>

// --------------------------------------------- BUILD CONFIGURATION ---------------------------------------------

/// Define this macro to include build configuration header.
/// Usage example with CMake: "-DSERARD_CONFIG_HEADER=\"${CMAKE_CURRENT_SOURCE_DIR}/my_serard_config.h\""
#ifdef SERARD_CONFIG_HEADER
#    include SERARD_CONFIG_HEADER
#endif

/// By default, this macro resolves to the standard assert(). The user can redefine this if necessary.
/// To disable assertion checks completely, make it expand into `(void)(0)`.
#ifndef SERARD_ASSERT
// Intentional violation of MISRA: inclusion not at the top of the file to eliminate unnecessary dependency on assert.h.
#    include <assert.h>  // NOSONAR
// Intentional violation of MISRA: assertion macro cannot be replaced with a function definition.
#    define SERARD_ASSERT(x) assert(x)  // NOSONAR
#endif

/// This macro is needed for testing and for library development.
#ifndef SERARD_PRIVATE
#    define SERARD_PRIVATE static inline
#endif

/// This macro expands to a no-op at compile time.
#define SERARD_UNUSED(x) ((void) (x))

#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L)
#    error "Unsupported language: ISO C99 or a newer version is required."
#endif

// --------------------------------------------- COMMON DEFINITIONS ---------------------------------------------

#define BITS_PER_BYTE 8U
#define BYTE_MAX 0xFFU
#define BYTE0_OFFSET 0U
#define BYTE1_OFFSET 8U
#define BYTE2_OFFSET 16U
#define BYTE3_OFFSET 24U
#define BYTE4_OFFSET 32U
#define BYTE5_OFFSET 40U
#define BYTE6_OFFSET 48U
#define BYTE7_OFFSET 56U

#define HEADER_CRC_SIZE_BYTES 2U
#define HEADER_SIZE_NO_CRC 22U
#define HEADER_SIZE (HEADER_SIZE_NO_CRC + HEADER_CRC_SIZE_BYTES)
#define HEADER_VERSION 1U
#define HEADER_USER_DATA 0U

#define HEADER_OFFSET_VERSION 0U
#define HEADER_OFFSET_PRIORITY 1U
#define HEADER_OFFSET_SOURCE_ID 2U
#define HEADER_OFFSET_DEST_ID 4U
#define HEADER_OFFSET_DATA_SPECIFIER 6U
#define HEADER_OFFSET_TRANSFER_ID 8U
#define HEADER_OFFSET_FRAME_INDEX 16U
#define HEADER_OFFSET_USER_DATA 20U
#define HEADER_OFFSET_CRC 22U

#define COBS_OVERHEAD_RATE 254U
#define COBS_FRAME_DELIMITER 0U

#define DATA_SPECIFIER_PORT_MASK 0x3FFFU
#define SERVICE_NOT_MESSAGE 0x8000U
#define REQUEST_NOT_RESPONSE 0x4000U
#define FRAME_INDEX 0U
#define END_OF_TRANSFER (1U << 31U)

SERARD_PRIVATE struct SerardTreeNode* avlTrivialFactory(void* const user_reference)
{
    return (struct SerardTreeNode*) user_reference;
}

// --------------------------------------------- HEADER CRC ---------------------------------------------

typedef uint16_t HeaderCRC;

#define HEADER_CRC_INITIAL 0xFFFFU
#define HEADER_CRC_RESIDUE 0x0000U

SERARD_PRIVATE HeaderCRC headerCRCAddByte(const HeaderCRC crc, const uint8_t byte)
{
    static const uint16_t CRCTable[256] = {
        0x0000U, 0x1021U, 0x2042U, 0x3063U, 0x4084U, 0x50A5U, 0x60C6U, 0x70E7U, 0x8108U, 0x9129U, 0xA14AU, 0xB16BU,
        0xC18CU, 0xD1ADU, 0xE1CEU, 0xF1EFU, 0x1231U, 0x0210U, 0x3273U, 0x2252U, 0x52B5U, 0x4294U, 0x72F7U, 0x62D6U,
        0x9339U, 0x8318U, 0xB37BU, 0xA35AU, 0xD3BDU, 0xC39CU, 0xF3FFU, 0xE3DEU, 0x2462U, 0x3443U, 0x0420U, 0x1401U,
        0x64E6U, 0x74C7U, 0x44A4U, 0x5485U, 0xA56AU, 0xB54BU, 0x8528U, 0x9509U, 0xE5EEU, 0xF5CFU, 0xC5ACU, 0xD58DU,
        0x3653U, 0x2672U, 0x1611U, 0x0630U, 0x76D7U, 0x66F6U, 0x5695U, 0x46B4U, 0xB75BU, 0xA77AU, 0x9719U, 0x8738U,
        0xF7DFU, 0xE7FEU, 0xD79DU, 0xC7BCU, 0x48C4U, 0x58E5U, 0x6886U, 0x78A7U, 0x0840U, 0x1861U, 0x2802U, 0x3823U,
        0xC9CCU, 0xD9EDU, 0xE98EU, 0xF9AFU, 0x8948U, 0x9969U, 0xA90AU, 0xB92BU, 0x5AF5U, 0x4AD4U, 0x7AB7U, 0x6A96U,
        0x1A71U, 0x0A50U, 0x3A33U, 0x2A12U, 0xDBFDU, 0xCBDCU, 0xFBBFU, 0xEB9EU, 0x9B79U, 0x8B58U, 0xBB3BU, 0xAB1AU,
        0x6CA6U, 0x7C87U, 0x4CE4U, 0x5CC5U, 0x2C22U, 0x3C03U, 0x0C60U, 0x1C41U, 0xEDAEU, 0xFD8FU, 0xCDECU, 0xDDCDU,
        0xAD2AU, 0xBD0BU, 0x8D68U, 0x9D49U, 0x7E97U, 0x6EB6U, 0x5ED5U, 0x4EF4U, 0x3E13U, 0x2E32U, 0x1E51U, 0x0E70U,
        0xFF9FU, 0xEFBEU, 0xDFDDU, 0xCFFCU, 0xBF1BU, 0xAF3AU, 0x9F59U, 0x8F78U, 0x9188U, 0x81A9U, 0xB1CAU, 0xA1EBU,
        0xD10CU, 0xC12DU, 0xF14EU, 0xE16FU, 0x1080U, 0x00A1U, 0x30C2U, 0x20E3U, 0x5004U, 0x4025U, 0x7046U, 0x6067U,
        0x83B9U, 0x9398U, 0xA3FBU, 0xB3DAU, 0xC33DU, 0xD31CU, 0xE37FU, 0xF35EU, 0x02B1U, 0x1290U, 0x22F3U, 0x32D2U,
        0x4235U, 0x5214U, 0x6277U, 0x7256U, 0xB5EAU, 0xA5CBU, 0x95A8U, 0x8589U, 0xF56EU, 0xE54FU, 0xD52CU, 0xC50DU,
        0x34E2U, 0x24C3U, 0x14A0U, 0x0481U, 0x7466U, 0x6447U, 0x5424U, 0x4405U, 0xA7DBU, 0xB7FAU, 0x8799U, 0x97B8U,
        0xE75FU, 0xF77EU, 0xC71DU, 0xD73CU, 0x26D3U, 0x36F2U, 0x0691U, 0x16B0U, 0x6657U, 0x7676U, 0x4615U, 0x5634U,
        0xD94CU, 0xC96DU, 0xF90EU, 0xE92FU, 0x99C8U, 0x89E9U, 0xB98AU, 0xA9ABU, 0x5844U, 0x4865U, 0x7806U, 0x6827U,
        0x18C0U, 0x08E1U, 0x3882U, 0x28A3U, 0xCB7DU, 0xDB5CU, 0xEB3FU, 0xFB1EU, 0x8BF9U, 0x9BD8U, 0xABBBU, 0xBB9AU,
        0x4A75U, 0x5A54U, 0x6A37U, 0x7A16U, 0x0AF1U, 0x1AD0U, 0x2AB3U, 0x3A92U, 0xFD2EU, 0xED0FU, 0xDD6CU, 0xCD4DU,
        0xBDAAU, 0xAD8BU, 0x9DE8U, 0x8DC9U, 0x7C26U, 0x6C07U, 0x5C64U, 0x4C45U, 0x3CA2U, 0x2C83U, 0x1CE0U, 0x0CC1U,
        0xEF1FU, 0xFF3EU, 0xCF5DU, 0xDF7CU, 0xAF9BU, 0xBFBAU, 0x8FD9U, 0x9FF8U, 0x6E17U, 0x7E36U, 0x4E55U, 0x5E74U,
        0x2E93U, 0x3EB2U, 0x0ED1U, 0x1EF0U,
    };
    return (uint16_t) ((uint16_t) (crc << BITS_PER_BYTE) ^
                       CRCTable[(uint16_t) ((uint16_t) (crc >> BITS_PER_BYTE) ^ byte) & BYTE_MAX]);
}

SERARD_PRIVATE HeaderCRC headerCRCAdd(const HeaderCRC crc, const size_t size, const void* const data)
{
    SERARD_ASSERT((data != NULL) || (size == 0U));
    uint16_t       out = crc;
    const uint8_t* p   = (const uint8_t*) data;
    for (size_t i = 0; i < size; i++)
    {
        out = headerCRCAddByte(out, *p);
        ++p;
    }
    return out;
}

// --------------------------------------------- TRANSFER CRC ---------------------------------------------

typedef uint32_t TransferCRC;

#define TRANSFER_CRC_INITIAL 0xFFFFFFFFUL
#define TRANSFER_CRC_OUTPUT_XOR 0xFFFFFFFFUL
#define TRANSFER_CRC_RESIDUE_BEFORE_OUTPUT_XOR 0xB798B438UL
#define TRANSFER_CRC_RESIDUE_AFTER_OUTPUT_XOR (TRANSFER_CRC_RESIDUE_BEFORE_OUTPUT_XOR ^ TRANSFER_CRC_OUTPUT_XOR)
#define TRANSFER_CRC_SIZE_BYTES 4U

SERARD_PRIVATE TransferCRC transferCRCAddByte(const TransferCRC crc, const uint8_t byte)
{
    static const TransferCRC CRCTable[256] = {
        0x00000000UL, 0xF26B8303UL, 0xE13B70F7UL, 0x1350F3F4UL, 0xC79A971FUL, 0x35F1141CUL, 0x26A1E7E8UL, 0xD4CA64EBUL,
        0x8AD958CFUL, 0x78B2DBCCUL, 0x6BE22838UL, 0x9989AB3BUL, 0x4D43CFD0UL, 0xBF284CD3UL, 0xAC78BF27UL, 0x5E133C24UL,
        0x105EC76FUL, 0xE235446CUL, 0xF165B798UL, 0x030E349BUL, 0xD7C45070UL, 0x25AFD373UL, 0x36FF2087UL, 0xC494A384UL,
        0x9A879FA0UL, 0x68EC1CA3UL, 0x7BBCEF57UL, 0x89D76C54UL, 0x5D1D08BFUL, 0xAF768BBCUL, 0xBC267848UL, 0x4E4DFB4BUL,
        0x20BD8EDEUL, 0xD2D60DDDUL, 0xC186FE29UL, 0x33ED7D2AUL, 0xE72719C1UL, 0x154C9AC2UL, 0x061C6936UL, 0xF477EA35UL,
        0xAA64D611UL, 0x580F5512UL, 0x4B5FA6E6UL, 0xB93425E5UL, 0x6DFE410EUL, 0x9F95C20DUL, 0x8CC531F9UL, 0x7EAEB2FAUL,
        0x30E349B1UL, 0xC288CAB2UL, 0xD1D83946UL, 0x23B3BA45UL, 0xF779DEAEUL, 0x05125DADUL, 0x1642AE59UL, 0xE4292D5AUL,
        0xBA3A117EUL, 0x4851927DUL, 0x5B016189UL, 0xA96AE28AUL, 0x7DA08661UL, 0x8FCB0562UL, 0x9C9BF696UL, 0x6EF07595UL,
        0x417B1DBCUL, 0xB3109EBFUL, 0xA0406D4BUL, 0x522BEE48UL, 0x86E18AA3UL, 0x748A09A0UL, 0x67DAFA54UL, 0x95B17957UL,
        0xCBA24573UL, 0x39C9C670UL, 0x2A993584UL, 0xD8F2B687UL, 0x0C38D26CUL, 0xFE53516FUL, 0xED03A29BUL, 0x1F682198UL,
        0x5125DAD3UL, 0xA34E59D0UL, 0xB01EAA24UL, 0x42752927UL, 0x96BF4DCCUL, 0x64D4CECFUL, 0x77843D3BUL, 0x85EFBE38UL,
        0xDBFC821CUL, 0x2997011FUL, 0x3AC7F2EBUL, 0xC8AC71E8UL, 0x1C661503UL, 0xEE0D9600UL, 0xFD5D65F4UL, 0x0F36E6F7UL,
        0x61C69362UL, 0x93AD1061UL, 0x80FDE395UL, 0x72966096UL, 0xA65C047DUL, 0x5437877EUL, 0x4767748AUL, 0xB50CF789UL,
        0xEB1FCBADUL, 0x197448AEUL, 0x0A24BB5AUL, 0xF84F3859UL, 0x2C855CB2UL, 0xDEEEDFB1UL, 0xCDBE2C45UL, 0x3FD5AF46UL,
        0x7198540DUL, 0x83F3D70EUL, 0x90A324FAUL, 0x62C8A7F9UL, 0xB602C312UL, 0x44694011UL, 0x5739B3E5UL, 0xA55230E6UL,
        0xFB410CC2UL, 0x092A8FC1UL, 0x1A7A7C35UL, 0xE811FF36UL, 0x3CDB9BDDUL, 0xCEB018DEUL, 0xDDE0EB2AUL, 0x2F8B6829UL,
        0x82F63B78UL, 0x709DB87BUL, 0x63CD4B8FUL, 0x91A6C88CUL, 0x456CAC67UL, 0xB7072F64UL, 0xA457DC90UL, 0x563C5F93UL,
        0x082F63B7UL, 0xFA44E0B4UL, 0xE9141340UL, 0x1B7F9043UL, 0xCFB5F4A8UL, 0x3DDE77ABUL, 0x2E8E845FUL, 0xDCE5075CUL,
        0x92A8FC17UL, 0x60C37F14UL, 0x73938CE0UL, 0x81F80FE3UL, 0x55326B08UL, 0xA759E80BUL, 0xB4091BFFUL, 0x466298FCUL,
        0x1871A4D8UL, 0xEA1A27DBUL, 0xF94AD42FUL, 0x0B21572CUL, 0xDFEB33C7UL, 0x2D80B0C4UL, 0x3ED04330UL, 0xCCBBC033UL,
        0xA24BB5A6UL, 0x502036A5UL, 0x4370C551UL, 0xB11B4652UL, 0x65D122B9UL, 0x97BAA1BAUL, 0x84EA524EUL, 0x7681D14DUL,
        0x2892ED69UL, 0xDAF96E6AUL, 0xC9A99D9EUL, 0x3BC21E9DUL, 0xEF087A76UL, 0x1D63F975UL, 0x0E330A81UL, 0xFC588982UL,
        0xB21572C9UL, 0x407EF1CAUL, 0x532E023EUL, 0xA145813DUL, 0x758FE5D6UL, 0x87E466D5UL, 0x94B49521UL, 0x66DF1622UL,
        0x38CC2A06UL, 0xCAA7A905UL, 0xD9F75AF1UL, 0x2B9CD9F2UL, 0xFF56BD19UL, 0x0D3D3E1AUL, 0x1E6DCDEEUL, 0xEC064EEDUL,
        0xC38D26C4UL, 0x31E6A5C7UL, 0x22B65633UL, 0xD0DDD530UL, 0x0417B1DBUL, 0xF67C32D8UL, 0xE52CC12CUL, 0x1747422FUL,
        0x49547E0BUL, 0xBB3FFD08UL, 0xA86F0EFCUL, 0x5A048DFFUL, 0x8ECEE914UL, 0x7CA56A17UL, 0x6FF599E3UL, 0x9D9E1AE0UL,
        0xD3D3E1ABUL, 0x21B862A8UL, 0x32E8915CUL, 0xC083125FUL, 0x144976B4UL, 0xE622F5B7UL, 0xF5720643UL, 0x07198540UL,
        0x590AB964UL, 0xAB613A67UL, 0xB831C993UL, 0x4A5A4A90UL, 0x9E902E7BUL, 0x6CFBAD78UL, 0x7FAB5E8CUL, 0x8DC0DD8FUL,
        0xE330A81AUL, 0x115B2B19UL, 0x020BD8EDUL, 0xF0605BEEUL, 0x24AA3F05UL, 0xD6C1BC06UL, 0xC5914FF2UL, 0x37FACCF1UL,
        0x69E9F0D5UL, 0x9B8273D6UL, 0x88D28022UL, 0x7AB90321UL, 0xAE7367CAUL, 0x5C18E4C9UL, 0x4F48173DUL, 0xBD23943EUL,
        0xF36E6F75UL, 0x0105EC76UL, 0x12551F82UL, 0xE03E9C81UL, 0x34F4F86AUL, 0xC69F7B69UL, 0xD5CF889DUL, 0x27A40B9EUL,
        0x79B737BAUL, 0x8BDCB4B9UL, 0x988C474DUL, 0x6AE7C44EUL, 0xBE2DA0A5UL, 0x4C4623A6UL, 0x5F16D052UL, 0xAD7D5351UL,
    };
    return (crc >> BITS_PER_BYTE) ^ CRCTable[byte ^ (crc & BYTE_MAX)];
}

/// Do not forget to apply the output XOR when done.
SERARD_PRIVATE TransferCRC transferCRCAdd(const uint32_t crc, const size_t size, const void* const data)
{
    SERARD_ASSERT((data != NULL) || (size == 0U));
    uint32_t       out = crc;
    const uint8_t* p   = (const uint8_t*) data;
    for (size_t i = 0; i < size; i++)
    {
        out = transferCRCAddByte(out, *p);
        ++p;
    }
    return out;
}

/// The memory requirement model provided in the documentation assumes that the maximum size of this structure never
/// exceeds 56 bytes on any conventional platform.
/// A user that needs a detailed analysis of the worst-case memory consumption may compute the size of this structure
/// for the particular platform at hand manually or by evaluating its sizeof().
/// The fields are ordered to minimize the amount of padding on all conventional platforms.
struct SerardInternalRxSession
{
    struct SerardTreeNode base;  ///< Used to manage this node in the session tree.

    SerardMicrosecond transfer_timestamp_usec;  ///< Used to validate transfer delay against restart timeout.
    SerardTransferID  transfer_id;              ///< Used to deduplicate transfers on redundant or unreliable networks.
    SerardNodeID      source_node_id;           ///< Sessions are maintained per unique remote node (ID).
    uint8_t           redundant_transport_index;  ///< Arbitrary value in [0, 255].
};

/// High-level transfer model.
/// TODO: is this needed?
struct RxTransferModel
{
    SerardMicrosecond       timestamp_usec;
    enum SerardPriority     priority;
    enum SerardTransferKind transfer_kind;
    SerardPortID            port_id;
    SerardNodeID            source_node_id;
    SerardNodeID            destination_node_id;
    SerardTransferID        transfer_id;
};

// --------------------------------------------- COBS ---------------------------------------------

// TODO: can this be smaller?
struct CobsEncoder
{
    size_t loc;
    size_t chunk;
};

#define STATE_REJECT 0U
#define STATE_DELIMITER 1U
#define STATE_HEADER 2U
#define STATE_PAYLOAD 3U

enum CobsDecodeResult
{
    COBS_DECODE_DELIMITER = 0U,
    COBS_DECODE_NONE,
    COBS_DECODE_DATA,
};

SERARD_PRIVATE void cobsEncodeByte(struct CobsEncoder* const encoder, uint8_t const byte, uint8_t* const out_buffer)
{
    SERARD_ASSERT(out_buffer != NULL);

    // unconditionally insert the input byte at the end
    // of the write buffer (this works for delimiters as well)
    const size_t prev_loc      = encoder->loc;
    out_buffer[encoder->loc++] = byte;

    // update the chunk offset and move the chunk pointer
    // if encountering a delimiter OR the current chunk is full
    const bool delim      = byte == COBS_FRAME_DELIMITER;
    const bool chunk_full = ((encoder->loc - encoder->chunk) >= BYTE_MAX) && !delim;
    if (chunk_full || delim)
    {
        const size_t  offset       = prev_loc - encoder->chunk;
        const uint8_t chunk_offset = chunk_full ? BYTE_MAX : ((uint8_t) offset);
        out_buffer[encoder->chunk] = chunk_offset;
        encoder->chunk             = prev_loc;
    }

    // if the chunk is full, we also need to reserve an extra
    // byte for the next chunk pointer (the input byte was not
    // a delimiter)
    if (chunk_full)
    {
        encoder->chunk             = encoder->loc++;
        out_buffer[encoder->chunk] = COBS_FRAME_DELIMITER;
    }
}

SERARD_PRIVATE void cobsEncodeIncremental(struct CobsEncoder* const encoder,
                                          size_t const              payload_size,
                                          const uint8_t* const      payload,
                                          uint8_t* const            out_buffer)
{
    SERARD_ASSERT(payload != NULL);
    SERARD_ASSERT(out_buffer != NULL);

    for (size_t i = 0; i < payload_size; i++)
    {
        cobsEncodeByte(encoder, payload[i], out_buffer);
    }
}

SERARD_PRIVATE size_t cobsEncodingSize(size_t const payload_size)
{
    // COBS encoded frames are bounded by n + ceil(n / 254)
    const size_t overhead = (payload_size + COBS_OVERHEAD_RATE - 1) / COBS_OVERHEAD_RATE;
    return payload_size + overhead;
}

SERARD_PRIVATE enum CobsDecodeResult cobsDecodeByte(struct SerardReassembler* const reassembler,
                                                    uint8_t* const                  inout_byte)
{
    const uint8_t byte = *inout_byte;
    if (byte == COBS_FRAME_DELIMITER)
    {
        reassembler->code = BYTE_MAX;
        reassembler->copy = 0;
        return COBS_DECODE_DELIMITER;
    }

    const uint8_t old_copy = reassembler->copy;
    reassembler->copy--;
    if (old_copy != 0)
    {
        *inout_byte = byte;
        return COBS_DECODE_DATA;
    }

    const uint8_t old_code = reassembler->code;
    SERARD_ASSERT(byte >= 1);
    reassembler->copy = byte - 1;
    reassembler->code = byte;
    if (old_code != BYTE_MAX)
    {
        *inout_byte = 0;
        return COBS_DECODE_DATA;
    }

    return COBS_DECODE_NONE;
}

// --------------------------------------------- ENDIAN ---------------------------------------------

// the following functions are intentionally unrolled to prevent
// a size-optimizing compiler from looping, degrading performance
SERARD_PRIVATE void hostToLittle16(uint16_t const in, uint8_t* const out)
{
    SERARD_ASSERT(out != NULL);
    out[0] = (uint8_t) ((in >> BYTE0_OFFSET) & BYTE_MAX);
    out[1] = (uint8_t) ((in >> BYTE1_OFFSET) & BYTE_MAX);
}

SERARD_PRIVATE void hostToLittle32(uint32_t const in, uint8_t* const out)
{
    SERARD_ASSERT(out != NULL);
    out[0] = (uint8_t) ((in >> BYTE0_OFFSET) & BYTE_MAX);
    out[1] = (uint8_t) ((in >> BYTE1_OFFSET) & BYTE_MAX);
    out[2] = (uint8_t) ((in >> BYTE2_OFFSET) & BYTE_MAX);
    out[3] = (uint8_t) ((in >> BYTE3_OFFSET) & BYTE_MAX);
}

SERARD_PRIVATE void hostToLittle64(uint64_t const in, uint8_t* const out)
{
    SERARD_ASSERT(out != NULL);
    out[0] = (uint8_t) ((in >> BYTE0_OFFSET) & BYTE_MAX);
    out[1] = (uint8_t) ((in >> BYTE1_OFFSET) & BYTE_MAX);
    out[2] = (uint8_t) ((in >> BYTE2_OFFSET) & BYTE_MAX);
    out[3] = (uint8_t) ((in >> BYTE3_OFFSET) & BYTE_MAX);
    out[4] = (uint8_t) ((in >> BYTE4_OFFSET) & BYTE_MAX);
    out[5] = (uint8_t) ((in >> BYTE5_OFFSET) & BYTE_MAX);
    out[6] = (uint8_t) ((in >> BYTE6_OFFSET) & BYTE_MAX);
    out[7] = (uint8_t) ((in >> BYTE7_OFFSET) & BYTE_MAX);
}

SERARD_PRIVATE uint16_t littleToHost16(const uint8_t* const in)
{
    SERARD_ASSERT(in != NULL);
    uint16_t out = 0;
    out |= (uint16_t) in[0] << BYTE0_OFFSET;
    out |= (uint16_t) in[1] << BYTE1_OFFSET;
    return out;
}

SERARD_PRIVATE uint32_t littleToHost32(const uint8_t* const in)
{
    SERARD_ASSERT(in != NULL);
    uint32_t out = 0;
    out |= (uint32_t) in[0] << BYTE0_OFFSET;
    out |= (uint32_t) in[1] << BYTE1_OFFSET;
    out |= (uint32_t) in[2] << BYTE2_OFFSET;
    out |= (uint32_t) in[3] << BYTE3_OFFSET;
    return out;
}

SERARD_PRIVATE uint64_t littleToHost64(const uint8_t* const in)
{
    SERARD_ASSERT(in != NULL);
    uint64_t out = 0;
    out |= (uint64_t) in[0] << BYTE0_OFFSET;
    out |= (uint64_t) in[1] << BYTE1_OFFSET;
    out |= (uint64_t) in[2] << BYTE2_OFFSET;
    out |= (uint64_t) in[3] << BYTE3_OFFSET;
    out |= (uint64_t) in[4] << BYTE4_OFFSET;
    out |= (uint64_t) in[5] << BYTE5_OFFSET;
    out |= (uint64_t) in[6] << BYTE6_OFFSET;
    out |= (uint64_t) in[7] << BYTE7_OFFSET;
    return out;
}

// --------------------------------------------- TRANSMISSION ---------------------------------------------

SERARD_PRIVATE uint16_t txMakeSessionSpecifier(const enum SerardTransferKind transfer_kind, const SerardPortID port_id)
{
    SERARD_ASSERT(transfer_kind <= SerardTransferKindRequest);

    const uint16_t snm = (transfer_kind == SerardTransferKindMessage) ? 0U : SERVICE_NOT_MESSAGE;
    const uint16_t rnr = (transfer_kind == SerardTransferKindRequest) ? REQUEST_NOT_RESPONSE : 0U;
    const uint16_t id  = (uint16_t) port_id;
    const uint16_t out = id | snm | rnr;
    return out;
}

SERARD_PRIVATE void txMakeHeader(const struct Serard* const                 ins,
                                 const struct SerardTransferMetadata* const metadata,
                                 uint8_t* const                             buffer)
{
    SERARD_ASSERT(ins != NULL);
    SERARD_ASSERT(metadata != NULL);
    SERARD_ASSERT(buffer != NULL);

    const uint16_t data_specifier_snm = txMakeSessionSpecifier(metadata->transfer_kind, metadata->port_id);
    const uint32_t frame_index_eot    = FRAME_INDEX | END_OF_TRANSFER;

    buffer[HEADER_OFFSET_VERSION]  = HEADER_VERSION;
    buffer[HEADER_OFFSET_PRIORITY] = (uint8_t) metadata->priority;
    hostToLittle16(ins->node_id, &buffer[HEADER_OFFSET_SOURCE_ID]);
    hostToLittle16(metadata->remote_node_id, &buffer[HEADER_OFFSET_DEST_ID]);
    hostToLittle16(data_specifier_snm, &buffer[HEADER_OFFSET_DATA_SPECIFIER]);
    hostToLittle64(metadata->transfer_id, &buffer[HEADER_OFFSET_TRANSFER_ID]);
    hostToLittle32(frame_index_eot, &buffer[HEADER_OFFSET_FRAME_INDEX]);
    hostToLittle16(HEADER_USER_DATA, &buffer[HEADER_OFFSET_USER_DATA]);

    const HeaderCRC crc           = headerCRCAdd(HEADER_CRC_INITIAL, HEADER_SIZE_NO_CRC, buffer);
    buffer[HEADER_OFFSET_CRC]     = (uint8_t) ((crc >> BYTE1_OFFSET) & BYTE_MAX);
    buffer[HEADER_OFFSET_CRC + 1] = (uint8_t) ((crc >> BYTE0_OFFSET) & BYTE_MAX);
}

// --------------------------------------------- RECEPTION ---------------------------------------------

// TODO: test this
SERARD_PRIVATE void rxInitTransferMetadataFromModel(const struct RxTransferModel* const  frame,
                                                    struct SerardTransferMetadata* const out_transfer)
{
    SERARD_ASSERT(frame != NULL);
    SERARD_ASSERT(out_transfer != NULL);

    out_transfer->priority       = frame->priority;
    out_transfer->transfer_kind  = frame->transfer_kind;
    out_transfer->port_id        = frame->port_id;
    out_transfer->remote_node_id = frame->source_node_id;
    out_transfer->transfer_id    = frame->transfer_id;
}

// TODO: test this
SERARD_PRIVATE int8_t
rxSubscriptionPredicateOnSession(void* const user_reference,  // NOSONAR Cavl API requires pointer to non-const.
                                 const struct SerardTreeNode* const node)
{
    const SerardNodeID  sought    = *((const SerardNodeID*) user_reference);
    const SerardNodeID  other     = ((const struct SerardInternalRxSession*) (const void*) node)->source_node_id;
    static const int8_t NegPos[2] = {-1, +1};
    // Clang-Tidy mistakenly identifies a narrowing cast to int8_t here, which is incorrect.
    return (sought == other) ? 0 : NegPos[sought > other];  // NOLINT no narrowing conversion is taking place here
}

SERARD_PRIVATE int8_t
rxSubscriptionPredicateOnPortID(void* const user_reference,  // NOSONAR Cavl API requires pointer to non-const.
                                const struct SerardTreeNode* const node)
{
    const SerardPortID  sought    = *((const SerardPortID*) user_reference);
    const SerardPortID  other     = ((const struct SerardRxSubscription*) (const void*) node)->port_id;
    static const int8_t NegPos[2] = {-1, +1};
    // Clang-Tidy mistakenly identifies a narrowing cast to int8_t here, which is incorrect.
    return (sought == other) ? 0 : NegPos[sought > other];  // NOLINT no narrowing conversion is taking place here
}

SERARD_PRIVATE int8_t
rxSubscriptionPredicateOnStruct(void* const user_reference,  // NOSONAR Cavl API requires pointer to non-const.
                                const struct SerardTreeNode* const node)
{
    return rxSubscriptionPredicateOnPortID(&((struct SerardRxSubscription*) user_reference)->port_id, node);
}

// Returns truth if the frame is valid and parsed successfully.
// False if the frame is not a valid Cyphal/CAN frame.
bool rxTryParseHeader(const SerardMicrosecond       timestamp_usec,
                      const uint8_t* const          payload,
                      struct RxTransferModel* const out)
{
    SERARD_ASSERT(out != NULL);
    SERARD_ASSERT(payload != NULL);

    bool valid          = false;
    out->timestamp_usec = timestamp_usec;

    valid                    = payload[HEADER_OFFSET_VERSION] == HEADER_VERSION;
    out->priority            = (enum SerardPriority) payload[HEADER_OFFSET_PRIORITY];
    valid                    = valid && (out->priority <= SerardPriorityOptional);
    out->source_node_id      = (SerardNodeID) littleToHost16(&payload[HEADER_OFFSET_SOURCE_ID]);
    out->destination_node_id = (SerardNodeID) littleToHost16(&payload[HEADER_OFFSET_DEST_ID]);

    uint16_t const data_specifier_snm = littleToHost16(&payload[HEADER_OFFSET_DATA_SPECIFIER]);
    out->port_id                      = data_specifier_snm & DATA_SPECIFIER_PORT_MASK;
    const bool snm                    = (data_specifier_snm & SERVICE_NOT_MESSAGE) != 0;
    const bool rnr                    = (data_specifier_snm & REQUEST_NOT_RESPONSE) != 0;
    if (snm)
    {
        out->transfer_kind = rnr ? SerardTransferKindRequest : SerardTransferKindResponse;
        valid              = valid && (out->port_id <= SERARD_SERVICE_ID_MAX);
    }
    else
    {
        out->transfer_kind = SerardTransferKindMessage;
        valid              = valid && !rnr;
        valid              = valid && (out->port_id <= SERARD_SUBJECT_ID_MAX);
    }

    out->transfer_id               = littleToHost64(&payload[HEADER_OFFSET_TRANSFER_ID]);
    uint32_t const frame_index_eot = littleToHost32(&payload[HEADER_OFFSET_FRAME_INDEX]);
    const uint32_t frame_index     = frame_index_eot & ~END_OF_TRANSFER;
    const bool     eot             = (frame_index_eot & END_OF_TRANSFER) != 0;
    valid                          = valid && (frame_index == 0);
    valid                          = valid && eot;

    // application of the CRC to the entire header shall yield zero
    const HeaderCRC header_crc = headerCRCAdd(HEADER_CRC_INITIAL, HEADER_SIZE, (void*) payload);
    valid                      = valid && (header_crc == HEADER_CRC_RESIDUE);
    // printf("crc valid: %d %04x %02x%02x\n",
    //        valid,
    //        headerCRCAdd(HEADER_CRC_INITIAL, HEADER_SIZE_NO_CRC, (void*) payload),
    //        payload[HEADER_OFFSET_CRC],
    //        payload[HEADER_OFFSET_CRC + 1]);

    return valid;
}

// 0: not ready with full header
// 1: read header, but invalid or not subscribed
// 2: subscribed to valid header, latch payload
// TODO: test this
SERARD_PRIVATE int8_t rxTryValidateHeader(struct Serard* const            ins,
                                          struct SerardReassembler* const reassembler,
                                          const SerardMicrosecond         timestamp_usec,
                                          struct SerardRxTransfer* const  out_transfer)
{
    if (reassembler->counter < HEADER_SIZE)
    {
        return 0;
    }

    struct RxTransferModel model;
    if (rxTryParseHeader(timestamp_usec, reassembler->header, &model))
    {
        // TODO: is the below atually true? (definitely no recursion, idk about loops)
        // This is the reason the function has a logarithmic time complexity of the number of subscriptions.
        // Note also that this one of the two variable-complexity operations in the RX pipeline; the other
        // one is memcpy(). Excepting these two cases, the entire RX pipeline contains neither loops nor
        // recursion.
        if ((model.destination_node_id == SERARD_NODE_ID_UNSET) || (model.destination_node_id == ins->node_id))
        {
            struct SerardRxSubscription* const sub = (struct SerardRxSubscription*) (void*)
                cavlSearch((struct SerardTreeNode**) &ins->rx_subscriptions[(size_t) model.transfer_kind],
                           &model.port_id,
                           &rxSubscriptionPredicateOnPortID,
                           NULL);

            // no subscription to this message, discard
            if (sub == NULL)
            {
                return 1;
            }

            // found a subscription, so proceed with processing the payload
            reassembler->sub     = sub;
            reassembler->counter = 0;

            // copy information into output transfer
            // TODO: figure out what this is doing, see if correct
            rxInitTransferMetadataFromModel(&model, &out_transfer->metadata);

            const size_t payload_extent   = sub->extent + TRANSFER_CRC_SIZE_BYTES;
            reassembler->max_payload_size = cobsEncodingSize(payload_extent);
            out_transfer->payload_extent  = reassembler->max_payload_size;
            SERARD_ASSERT(out_transfer->payload_extent > 0);

            out_transfer->payload =
                ins->memory_payload.allocate(ins->memory_payload.user_reference, out_transfer->payload_extent);
            if (out_transfer->payload == NULL)
            {
                return -SERARD_ERROR_MEMORY;
            }

            return 2;
        }

        // mis-addressed transfer, discard
        return 1;
    }

    // invalid frame header (including failed header CRC)
    return 1;
}

/// RX session state machine update is the most intricate part of any Cyphal transport implementation.
/// The state model used here is derived from the reference pseudocode given in the original UAVCAN v0 specification.
/// The Cyphal/CAN v1 specification, which this library is an implementation of, does not provide any reference
/// pseudocode. Instead, it takes a higher-level, more abstract approach, where only the high-level requirements
/// are given and the particular algorithms are left to be implementation-defined. Such abstract approach is much
/// advantageous because it allows implementers to choose whatever solution works best for the specific application at
/// hand, while the wire compatibility is still guaranteed by the high-level requirements given in the specification.
SERARD_PRIVATE void rxSessionUpdate(struct Serard* const                  ins,
                                    struct SerardInternalRxSession* const rxs,
                                    const struct SerardRxTransfer* const  transfer,
                                    const uint8_t                         redundant_transport_index,
                                    const SerardMicrosecond               transfer_id_timeout_usec,
                                    const size_t                          extent)
{
    SERARD_ASSERT(ins != NULL);
    SERARD_ASSERT(rxs != NULL);
    SERARD_ASSERT(transfer != NULL);
    SERARD_ASSERT(rxs->transfer_id <= SERARD_TRANSFER_ID_MAX);

    const struct SerardTransferMetadata* metadata = &transfer->metadata;
    SERARD_ASSERT(metadata->transfer_id <= SERARD_TRANSFER_ID_MAX);

    const bool tid_timed_out = (transfer->timestamp_usec > rxs->transfer_timestamp_usec) &&
                               ((transfer->timestamp_usec - rxs->transfer_timestamp_usec) > transfer_id_timeout_usec);

    // The monotonic 64 bit transfer ID in UAVCAN/Serial shall not wrap.
    const bool not_monotonic = (metadata->transfer_id - rxs->transfer_id) > 1;

    const bool need_restart =
        tid_timed_out || ((rxs->redundant_transport_index == redundant_transport_index) && not_monotonic);

    if (need_restart)
    {
        rxs->transfer_id               = metadata->transfer_id;
        rxs->redundant_transport_index = redundant_transport_index;
    }
}

// TODO: test this
SERARD_PRIVATE int8_t rxAcceptTransfer(struct Serard* const            ins,
                                       struct SerardReassembler* const reassembler,
                                       struct SerardRxTransfer* const  transfer,
                                       const SerardMicrosecond         timestamp_usec,
                                       const uint8_t                   redundant_transport_index)
{
    SERARD_ASSERT(ins != NULL);
    SERARD_ASSERT(transfer != NULL);
    SERARD_ASSERT(reassembler != NULL);

    const struct SerardTransferMetadata* const metadata     = &transfer->metadata;
    const struct SerardRxSubscription* const   subscription = reassembler->sub;
    SERARD_ASSERT(metadata->transfer_id <= SERARD_TRANSFER_ID_MAX);

    // TODO: maybe we can just use the out_transfer->size to track the counter?
    const size_t payload_size = reassembler->counter;
    TransferCRC  payload_crc  = TRANSFER_CRC_INITIAL;
    payload_crc               = transferCRCAdd(payload_crc, payload_size, transfer->payload);
    payload_crc               = payload_crc ^ TRANSFER_CRC_OUTPUT_XOR;
    const bool valid          = payload_crc == TRANSFER_CRC_RESIDUE_AFTER_OUTPUT_XOR;

    if (!valid)
    {
        return 0;
    }

    // TODO: do we need to discount the transfer crc size when outputting payload size?
    transfer->payload_size   = payload_size;
    transfer->timestamp_usec = timestamp_usec;

    if (metadata->remote_node_id <= SERARD_NODE_ID_MAX)
    {
        struct SerardInternalRxSession* rxs =
            (struct SerardInternalRxSession*) cavlSearch((struct SerardTreeNode**) &subscription->sessions,
                                                         (void*) &metadata->remote_node_id,
                                                         &rxSubscriptionPredicateOnSession,
                                                         NULL);

        if (rxs == NULL)
        {
            rxs = (struct SerardInternalRxSession*) ins->memory_rx_session
                      .allocate(ins->memory_rx_session.user_reference, sizeof(struct SerardInternalRxSession));

            if (rxs != NULL)
            {
                rxs->transfer_timestamp_usec   = transfer->timestamp_usec;
                rxs->source_node_id            = metadata->remote_node_id;
                rxs->transfer_id               = metadata->transfer_id;
                rxs->redundant_transport_index = redundant_transport_index;

                SERARD_UNUSED(cavlSearch((struct SerardTreeNode**) &subscription->sessions,
                                         (void*) &metadata->remote_node_id,
                                         &rxSubscriptionPredicateOnSession,
                                         NULL));
                rxSessionUpdate(ins,
                                rxs,
                                transfer,
                                redundant_transport_index,
                                subscription->transfer_id_timeout_usec,
                                subscription->extent);

                return 1;
            }
            else
            {
                return -SERARD_ERROR_MEMORY;
            }
        }
    }
    else
    {
        SERARD_ASSERT(metadata->remote_node_id == SERARD_NODE_ID_UNSET);
        // Anonymous transfers are stateless. No need to update the state machine,
        // just blindly accept it.
        return 1;
    }

    return 1;
}

// --------------------------------------------- PUBLIC API ---------------------------------------------

struct Serard serardInit(const struct SerardMemoryResource memory_payload,
                         const struct SerardMemoryResource memory_rx_session)
{
    SERARD_ASSERT(memory_payload.allocate != NULL);
    SERARD_ASSERT(memory_payload.deallocate != NULL);
    SERARD_ASSERT(memory_rx_session.allocate != NULL);
    SERARD_ASSERT(memory_rx_session.deallocate != NULL);

    struct Serard serard = {
        .user_reference    = NULL,
        .node_id           = SERARD_NODE_ID_UNSET,
        .memory_payload    = memory_payload,
        .memory_rx_session = memory_rx_session,
        .rx_subscriptions  = {NULL, NULL, NULL},
    };

    return serard;
}

struct SerardReassembler serardReassemblerInit(void)
{
    struct SerardReassembler reassembler = {
        .code             = BYTE_MAX,
        .copy             = 0,
        .state            = STATE_REJECT,
        .counter          = 0,
        .header           = {0},
        .sub              = NULL,
        .max_payload_size = 0,
    };

    return reassembler;
};

int8_t serardTxPush(struct Serard* const                       ins,
                    const struct SerardTransferMetadata* const metadata,
                    const size_t                               payload_size,
                    const void* const                          payload,
                    void* const                                user_reference,
                    const SerardTxEmit                         emitter)
{
    if ((ins == NULL) || (metadata == NULL) || (emitter == NULL) || (metadata->priority > SERARD_PRIORITY_MAX) ||
        (metadata->transfer_kind > SERARD_TRANSFER_KIND_MAX))
    {
        return -SERARD_ERROR_ARGUMENT;
    }

    const SerardPortID port_id_max =
        (SerardTransferKindMessage == metadata->transfer_kind) ? SERARD_SUBJECT_ID_MAX : SERARD_SERVICE_ID_MAX;
    if (metadata->port_id > port_id_max)
    {
        return -SERARD_ERROR_ARGUMENT;
    }

    const size_t   header_payload_size = HEADER_SIZE + payload_size + TRANSFER_CRC_SIZE_BYTES;
    const size_t   max_frame_size = cobsEncodingSize(header_payload_size) + 2U;  // 2 bytes extra for frame delimiters
    uint8_t* const buffer         = ins->memory_payload.allocate(ins->memory_payload.user_reference, max_frame_size);
    if (buffer == NULL)
    {
        return -SERARD_ERROR_MEMORY;
    }

    size_t buffer_offset           = 0;
    buffer[buffer_offset++]        = COBS_FRAME_DELIMITER;
    struct CobsEncoder encoder     = (struct CobsEncoder){.loc = 1, .chunk = 0};
    uint8_t* const     frame_start = &buffer[buffer_offset];

    uint8_t header[HEADER_SIZE];
    txMakeHeader(ins, metadata, header);
    cobsEncodeIncremental(&encoder, HEADER_SIZE, header, frame_start);

    if (payload_size > 0)
    {
        cobsEncodeIncremental(&encoder, payload_size, payload, frame_start);
    }
    TransferCRC crc = transferCRCAdd(TRANSFER_CRC_INITIAL, payload_size, payload) ^ TRANSFER_CRC_OUTPUT_XOR;
    cobsEncodeByte(&encoder, (uint8_t) ((crc >> BYTE0_OFFSET) & BYTE_MAX), frame_start);
    cobsEncodeByte(&encoder, (uint8_t) ((crc >> BYTE1_OFFSET) & BYTE_MAX), frame_start);
    cobsEncodeByte(&encoder, (uint8_t) ((crc >> BYTE2_OFFSET) & BYTE_MAX), frame_start);
    cobsEncodeByte(&encoder, (uint8_t) ((crc >> BYTE3_OFFSET) & BYTE_MAX), frame_start);

    cobsEncodeByte(&encoder, COBS_FRAME_DELIMITER, frame_start);
    buffer_offset += encoder.loc;

    size_t bytes_transmitted = 0;
    while (bytes_transmitted < buffer_offset)
    {
        const size_t  bytes_left = buffer_offset - bytes_transmitted;
        const uint8_t chunk_size = (bytes_left > BYTE_MAX) ? BYTE_MAX : ((uint8_t) bytes_left);
        bool          out        = emitter(user_reference, chunk_size, &buffer[bytes_transmitted]);
        if (!out)
        {
            ins->memory_payload.deallocate(ins->memory_payload.user_reference, max_frame_size, buffer);
            return 0;
        }
        bytes_transmitted += chunk_size;
    }

    ins->memory_payload.deallocate(ins->memory_payload.user_reference, max_frame_size, buffer);
    return 1;
}

int8_t serardRxAccept(struct Serard* const                ins,
                      struct SerardReassembler* const     reassembler,
                      SerardMicrosecond const             timestamp_usec,
                      size_t* const                       inout_payload_size,
                      const uint8_t* const                payload,
                      const uint8_t                       redundant_transport_index,
                      struct SerardRxTransfer* const      out_transfer,
                      struct SerardRxSubscription** const out_subscription)
{
    const size_t in_payload_size = *inout_payload_size;
    *inout_payload_size          = 0;

    // TODO: https://github.com/OpenCyphal/pycyphal/issues/112
    for (size_t i = 0; i < in_payload_size; i++)
    {
        uint8_t                     cobs_byte = payload[i];
        const enum CobsDecodeResult result    = cobsDecodeByte(reassembler, &cobs_byte);
        const uint8_t               state     = reassembler->state;

        // consume without updating the state machine, these are not
        // part of the original bytestream
        if (result == COBS_DECODE_NONE)
        {
            continue;
        }

        const bool delim = result == COBS_DECODE_DELIMITER;

        switch (state)
        {
        case STATE_REJECT:
            // discard incoming bytes until a delimiter is detected
            if (delim)
            {
                reassembler->state = STATE_DELIMITER;
            }
            break;
        case STATE_DELIMITER:
            // discard delimiter byte(s) until data byte is detected
            // then start latching the header
            if (!delim)
            {
                reassembler->state     = STATE_HEADER;
                reassembler->header[0] = cobs_byte;
                reassembler->counter   = 1;
            }
            break;
        case STATE_HEADER:
            // because the header is fixed size, we consider a premature delimiter
            // as invalid and discard the transfer, resetting the state machine
            if (delim)
            {
                reassembler->state = STATE_DELIMITER;
                break;
            }

            // latch the incoming byte into the header buffer
            SERARD_ASSERT(result == COBS_DECODE_DATA);
            reassembler->header[reassembler->counter++] = cobs_byte;

            const int8_t ret = rxTryValidateHeader(ins, reassembler, timestamp_usec, out_transfer);
            if (ret < 0)
            {
                // rx pipeline encountered error
                reassembler->state = STATE_REJECT;
                return ret;
            }
            else if (ret == 1)
            {
                // invalid or mis-addressed header, reject rest of frame
                reassembler->state = STATE_REJECT;
            }
            else if (ret == 2)
            {
                // valid header, continue with payload
                reassembler->state = STATE_PAYLOAD;
            }
            break;
        case STATE_PAYLOAD:
            // there is no pre-determined header size, so we consume bytes
            // until we discover a delimiter, or we hit the message extent
            // and bail to limit memory usage
            if (delim)
            {
                reassembler->state = STATE_DELIMITER;
                const int8_t ret =
                    rxAcceptTransfer(ins, reassembler, out_transfer, timestamp_usec, redundant_transport_index);
                if (ret < 0)
                {
                    return ret;
                }
                else if (ret > 0)
                {
                    *inout_payload_size = in_payload_size - i - 1;
                    *out_subscription   = reassembler->sub;
                    return 1;
                }

                break;
            }

            SERARD_ASSERT(result == COBS_DECODE_DATA);
            if (reassembler->counter >= reassembler->max_payload_size)
            {
                // payload exceeded extent, discard frame
                reassembler->state = STATE_REJECT;
                break;
            }

            uint8_t* const payload          = (uint8_t*) out_transfer->payload;
            payload[reassembler->counter++] = cobs_byte;
            break;
        }
    }

    return 0;
}

int8_t serardRxSubscribe(struct Serard* const               ins,
                         const enum SerardTransferKind      transfer_kind,
                         const SerardPortID                 port_id,
                         const size_t                       extent,
                         const SerardMicrosecond            transfer_id_timeout_usec,
                         struct SerardRxSubscription* const out_subscription)
{
    int8_t       out = -SERARD_ERROR_ARGUMENT;
    const size_t tk  = (size_t) transfer_kind;

    if ((ins != NULL) && (out_subscription != NULL) && (tk < SERARD_NUM_TRANSFER_KINDS))
    {
        out = serardRxUnsubscribe(ins, transfer_kind, port_id);
        if (out >= 0)
        {
            out_subscription->port_id                  = port_id;
            out_subscription->extent                   = extent;
            out_subscription->transfer_id_timeout_usec = transfer_id_timeout_usec;
            out_subscription->sessions                 = NULL;

            const struct SerardTreeNode* const node = cavlSearch((struct SerardTreeNode**) &ins->rx_subscriptions[tk],
                                                                 out_subscription,
                                                                 &rxSubscriptionPredicateOnStruct,
                                                                 &avlTrivialFactory);
            SERARD_ASSERT(node == &out_subscription->base);
            out = (out > 0) ? 0 : 1;
        }
    }

    return out;
}

int8_t serardRxUnsubscribe(struct Serard* const          ins,
                           const enum SerardTransferKind transfer_kind,
                           const SerardPortID            port_id)
{
    int8_t       ret = -SERARD_ERROR_ARGUMENT;
    const size_t tk  = (size_t) transfer_kind;
    if ((ins != NULL) && (tk < SERARD_NUM_TRANSFER_KINDS))
    {
        SerardPortID                       port_id_mutable = port_id;
        struct SerardRxSubscription* const sub =
            (struct SerardRxSubscription*) (void*) cavlSearch((struct SerardTreeNode**) &ins->rx_subscriptions[tk],
                                                              &port_id_mutable,
                                                              &rxSubscriptionPredicateOnPortID,
                                                              NULL);
        if (sub != NULL)
        {
            cavlRemove((struct SerardTreeNode**) &ins->rx_subscriptions[tk], &sub->base);
            SERARD_ASSERT(sub->port_id == port_id);
            ret = 1;
            // TODO: we should be doing this in O(n), not O(n log n), and without unecessary rotation
            while (sub->sessions != NULL)
            {
                cavlRemove((struct SerardTreeNode**) &ins->rx_subscriptions[tk],
                           (struct SerardTreeNode*) ins->rx_subscriptions[tk]);
            }
        }
        else
        {
            ret = 0;
        }
    }

    return ret;
}
