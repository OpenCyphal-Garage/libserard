/// This software is distributed under the terms of the MIT License.
/// Copyright (c) OpenCyphal.
/// Author: Kalyan Sriram <coder.kalyan@gmail.com>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "serard.h"

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

#define CAVL2_RELATION int8_t
#define CAVL2_T        struct SerardTreeNode
#include "cavl2.h"

#define BITS_PER_BYTE 8U
#define BYTE_MAX      0xFFU

#define BYTE0_OFFSET 0U
#define BYTE1_OFFSET 8U
#define BYTE2_OFFSET 16U
#define BYTE3_OFFSET 24U
#define BYTE4_OFFSET 32U
#define BYTE5_OFFSET 40U
#define BYTE6_OFFSET 48U
#define BYTE7_OFFSET 56U

#define HEADER_CRC_SIZE_BYTES 2U
#define HEADER_SIZE_NO_CRC    22U
#define HEADER_SIZE           (HEADER_SIZE_NO_CRC + HEADER_CRC_SIZE_BYTES)
#define HEADER_VERSION        1U
#define HEADER_USER_DATA      0U

#define HEADER_OFFSET_VERSION        0U
#define HEADER_OFFSET_PRIORITY       1U
#define HEADER_OFFSET_SOURCE_ID      2U
#define HEADER_OFFSET_DEST_ID        4U
#define HEADER_OFFSET_DATA_SPECIFIER 6U
#define HEADER_OFFSET_TRANSFER_ID    8U
#define HEADER_OFFSET_FRAME_INDEX    16U
#define HEADER_OFFSET_USER_DATA      20U
#define HEADER_OFFSET_CRC            22U

#define COBS_FRAME_DELIMITER 0U
#define COBS_LOOKAHEAD_SIZE  256U

#define DATA_SPECIFIER_PORT_MASK 0x3FFFU
#define SERVICE_NOT_MESSAGE      0x8000U
#define REQUEST_NOT_RESPONSE     0x4000U
#define FRAME_INDEX              0U
#define END_OF_TRANSFER          (1U << 31U)

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

#define TRANSFER_CRC_INITIAL                   0xFFFFFFFFUL
#define TRANSFER_CRC_OUTPUT_XOR                0xFFFFFFFFUL
#define TRANSFER_CRC_RESIDUE_BEFORE_OUTPUT_XOR 0xB798B438UL
#define TRANSFER_CRC_RESIDUE_AFTER_OUTPUT_XOR  (TRANSFER_CRC_RESIDUE_BEFORE_OUTPUT_XOR ^ TRANSFER_CRC_OUTPUT_XOR)
#define TRANSFER_CRC_SIZE_BYTES                4U

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
    uint8_t           redundant_iface_index;    ///< Arbitrary value in [0, 255].
};

// --------------------------------------------- COBS ---------------------------------------------

struct CobsEncoder
{
    const SerardTxEmit emitter;
    void*              user_reference;
    uint8_t            in;
    uint8_t            fifo[COBS_LOOKAHEAD_SIZE];
};

enum CobsDecodeResult
{
    COBS_DECODE_DELIMITER = 0U,
    COBS_DECODE_NONE,
    COBS_DECODE_DATA,
};

SERARD_PRIVATE bool cobsFlush(struct CobsEncoder* const encoder);

// Encode one byte of an input buffer into a COBS output stream and
// emit it to the emitter interface as necessary.
SERARD_PRIVATE bool cobsPush(struct CobsEncoder* const encoder, uint8_t const byte)
{
    SERARD_ASSERT(encoder != NULL);

    const bool delim = byte == COBS_FRAME_DELIMITER;
    if (!delim)
    {
        encoder->fifo[encoder->in++] = byte;
    }

    const bool full = encoder->in == (BYTE_MAX - 1);
    if (delim || full)
    {
        return cobsFlush(encoder);
    }

    return true;
}

SERARD_PRIVATE bool cobsFlush(struct CobsEncoder* const encoder)
{
    const uint8_t size = encoder->in;
    SERARD_ASSERT(size < BYTE_MAX);

    // attempt to emit the offset byte
    const uint8_t offset = size + 1;  // C cannot take rvalue references
    if (!encoder->emitter(encoder->user_reference, 1, &offset))
    {
        return false;
    }

    if ((size > 0) && !encoder->emitter(encoder->user_reference, size, encoder->fifo))
    {
        return false;
    }

    encoder->in = 0;
    return true;
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
    out[0] = (uint8_t) ((uint8_t) (in >> BYTE0_OFFSET) & BYTE_MAX);
    out[1] = (uint8_t) ((uint8_t) (in >> BYTE1_OFFSET) & BYTE_MAX);
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
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
    // NOLINTBEGIN(readability-magic-numbers)
    out[0] = (uint8_t) ((in >> BYTE0_OFFSET) & BYTE_MAX);
    out[1] = (uint8_t) ((in >> BYTE1_OFFSET) & BYTE_MAX);
    out[2] = (uint8_t) ((in >> BYTE2_OFFSET) & BYTE_MAX);
    out[3] = (uint8_t) ((in >> BYTE3_OFFSET) & BYTE_MAX);
    out[4] = (uint8_t) ((in >> BYTE4_OFFSET) & BYTE_MAX);
    out[5] = (uint8_t) ((in >> BYTE5_OFFSET) & BYTE_MAX);
    out[6] = (uint8_t) ((in >> BYTE6_OFFSET) & BYTE_MAX);
    out[7] = (uint8_t) ((in >> BYTE7_OFFSET) & BYTE_MAX);
    // NOLINTEND(readability-magic-numbers)
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}

SERARD_PRIVATE uint16_t littleToHost16(const uint8_t* const in)
{
    SERARD_ASSERT(in != NULL);
    uint16_t out = 0;
    out |= (uint16_t) ((uint16_t) in[0] << BYTE0_OFFSET);
    out |= (uint16_t) ((uint16_t) in[1] << BYTE1_OFFSET);
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
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
    // NOLINTBEGIN(readability-magic-numbers)
    out |= (uint64_t) in[0] << BYTE0_OFFSET;
    out |= (uint64_t) in[1] << BYTE1_OFFSET;
    out |= (uint64_t) in[2] << BYTE2_OFFSET;
    out |= (uint64_t) in[3] << BYTE3_OFFSET;
    out |= (uint64_t) in[4] << BYTE4_OFFSET;
    out |= (uint64_t) in[5] << BYTE5_OFFSET;
    out |= (uint64_t) in[6] << BYTE6_OFFSET;
    out |= (uint64_t) in[7] << BYTE7_OFFSET;
    // NOLINTEND(readability-magic-numbers)
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
    return out;
}

// --------------------------------------------- TRANSMISSION ---------------------------------------------

SERARD_PRIVATE uint16_t txMakeSessionSpecifier(const enum SerardTransferKind transfer_kind, const SerardPortID port_id)
{
    SERARD_ASSERT(transfer_kind <= SERARD_TRANSFER_KIND_MAX);
    SERARD_ASSERT(port_id <=
                  ((transfer_kind == SerardTransferKindMessage) ? SERARD_SUBJECT_ID_MAX : SERARD_SERVICE_ID_MAX));

    const uint16_t snm = (transfer_kind == SerardTransferKindMessage) ? 0U : SERVICE_NOT_MESSAGE;
    const uint16_t rnr = (transfer_kind == SerardTransferKindRequest) ? REQUEST_NOT_RESPONSE : 0U;
    const uint16_t id  = (uint16_t) port_id;
    const uint16_t out = (uint16_t) (id | snm) | rnr;
    return out;
}

SERARD_PRIVATE void txMakeHeader(const SerardNodeID                         node_id,
                                 const struct SerardTransferMetadata* const metadata,
                                 uint8_t* const                             buffer)
{
    SERARD_ASSERT(metadata != NULL);
    SERARD_ASSERT(buffer != NULL);
    SERARD_ASSERT((node_id == SERARD_NODE_ID_UNSET) || (node_id <= SERARD_NODE_ID_MAX));
    SERARD_ASSERT((metadata->remote_node_id == SERARD_NODE_ID_UNSET) ||
                  (metadata->remote_node_id <= SERARD_NODE_ID_MAX));

    const uint16_t data_specifier_snm = txMakeSessionSpecifier(metadata->transfer_kind, metadata->port_id);
    const uint32_t frame_index_eot    = FRAME_INDEX | END_OF_TRANSFER;

    buffer[HEADER_OFFSET_VERSION]  = HEADER_VERSION;
    buffer[HEADER_OFFSET_PRIORITY] = (uint8_t) metadata->priority;
    hostToLittle16(node_id, &buffer[HEADER_OFFSET_SOURCE_ID]);
    hostToLittle16(metadata->remote_node_id, &buffer[HEADER_OFFSET_DEST_ID]);
    hostToLittle16(data_specifier_snm, &buffer[HEADER_OFFSET_DATA_SPECIFIER]);
    hostToLittle64(metadata->transfer_id, &buffer[HEADER_OFFSET_TRANSFER_ID]);
    hostToLittle32(frame_index_eot, &buffer[HEADER_OFFSET_FRAME_INDEX]);
    hostToLittle16(HEADER_USER_DATA, &buffer[HEADER_OFFSET_USER_DATA]);

    const HeaderCRC crc           = headerCRCAdd(HEADER_CRC_INITIAL, HEADER_SIZE_NO_CRC, buffer);
    buffer[HEADER_OFFSET_CRC]     = (uint8_t) ((uint8_t) (crc >> BYTE1_OFFSET) & BYTE_MAX);
    buffer[HEADER_OFFSET_CRC + 1] = (uint8_t) ((uint8_t) (crc >> BYTE0_OFFSET) & BYTE_MAX);
}

// --------------------------------------------- RECEPTION ---------------------------------------------

// TODO: test this
SERARD_PRIVATE int8_t
rxSubscriptionPredicateOnSession(const void* const user_reference,  // NOSONAR Cavl API requires pointer to non-const.
                                 const struct SerardTreeNode* const node)
{
    const SerardNodeID sought = *((const SerardNodeID*) user_reference);
    // FIXME: cavl2 provides a container_of macro for this purpose
    const SerardNodeID  other     = ((const struct SerardInternalRxSession*) (const void*) node)->source_node_id;
    static const int8_t NegPos[2] = {-1, +1};
    // Clang-Tidy mistakenly identifies a narrowing cast to int8_t here, which is incorrect.
    return (sought == other) ? 0 : NegPos[sought > other];  // NOLINT no narrowing conversion is taking place here
}

SERARD_PRIVATE int8_t
rxSubscriptionPredicateOnPortID(const void* const user_reference,  // NOSONAR Cavl API requires pointer to non-const.
                                const struct SerardTreeNode* const node)
{
    const SerardPortID sought = *((const SerardPortID*) user_reference);
    // FIXME: cavl2 provides a container_of macro for this purpose
    const SerardPortID  other     = ((const struct SerardRxSubscription*) (const void*) node)->port_id;
    static const int8_t NegPos[2] = {-1, +1};
    // Clang-Tidy mistakenly identifies a narrowing cast to int8_t here, which is incorrect.
    return (sought == other) ? 0 : NegPos[sought > other];  // NOLINT no narrowing conversion is taking place here
}

// Returns truth if the frame is valid and parsed successfully.
// False if the frame is not a valid Cyphal/CAN frame.
SERARD_PRIVATE bool rxTryParseHeader(const uint8_t* const                 payload,
                                     struct SerardTransferMetadata* const out_metadata,
                                     SerardNodeID* const                  out_destination_node_id)
{
    SERARD_ASSERT(payload != NULL);
    SERARD_ASSERT(out_metadata != NULL);
    SERARD_ASSERT(out_destination_node_id != NULL);

    bool valid = true;
    valid      = valid && payload[HEADER_OFFSET_VERSION] == HEADER_VERSION;

    const enum SerardPriority priority = payload[HEADER_OFFSET_PRIORITY];
    valid                              = valid && (priority <= SerardPriorityOptional);
    out_metadata->priority             = priority;

    out_metadata->remote_node_id = (SerardNodeID) littleToHost16(&payload[HEADER_OFFSET_SOURCE_ID]);
    *out_destination_node_id     = (SerardNodeID) littleToHost16(&payload[HEADER_OFFSET_DEST_ID]);

    const uint16_t data_specifier_snm = littleToHost16(&payload[HEADER_OFFSET_DATA_SPECIFIER]);
    const uint16_t port_id            = data_specifier_snm & DATA_SPECIFIER_PORT_MASK;
    const bool     snm                = (data_specifier_snm & SERVICE_NOT_MESSAGE) != 0;
    const bool     rnr                = (data_specifier_snm & REQUEST_NOT_RESPONSE) != 0;
    if (snm)
    {
        valid                       = valid && (port_id <= SERARD_SERVICE_ID_MAX);
        out_metadata->transfer_kind = rnr ? SerardTransferKindRequest : SerardTransferKindResponse;
        out_metadata->port_id       = port_id;
    }
    else
    {
        valid                       = valid && !rnr;
        valid                       = valid && (data_specifier_snm <= SERARD_SUBJECT_ID_MAX);
        out_metadata->transfer_kind = SerardTransferKindMessage;
        out_metadata->port_id       = port_id;
    }

    out_metadata->transfer_id      = littleToHost64(&payload[HEADER_OFFSET_TRANSFER_ID]);
    const uint32_t frame_index_eot = littleToHost32(&payload[HEADER_OFFSET_FRAME_INDEX]);
    const uint32_t frame_index     = frame_index_eot & ~END_OF_TRANSFER;
    const bool     eot             = (frame_index_eot & END_OF_TRANSFER) != 0;
    valid                          = valid && (frame_index == 0);
    valid                          = valid && eot;

    // application of the CRC to the entire header shall yield zero
    const HeaderCRC header_crc = headerCRCAdd(HEADER_CRC_INITIAL, HEADER_SIZE, (void*) payload);
    valid                      = valid && (header_crc == HEADER_CRC_RESIDUE);

    return valid;
}

/// Returns <0 on error. The transfer shall be discarded and the error code returned to the user.
/// Returns 0 if the header is invalid or not subscribed to. The transfer shall be silently discarded.
/// Returns 1 if the header is valid and should be processed further.
SERARD_PRIVATE int8_t rxValidateHeader(struct SerardRx* const          ins,
                                       struct SerardReassembler* const reassembler,
                                       struct SerardRxTransfer* const  out_transfer)
{
    SERARD_ASSERT(ins != NULL);
    SERARD_ASSERT(reassembler != NULL);
    SERARD_ASSERT(out_transfer != NULL);
    SERARD_ASSERT(reassembler->counter == (HEADER_SIZE - 1));

    int8_t ret = 0;

    struct SerardTransferMetadata* const metadata            = &out_transfer->metadata;
    SerardNodeID                         destination_node_id = SERARD_NODE_ID_UNSET;
    if (rxTryParseHeader(reassembler->header, metadata, &destination_node_id))
    {
        if ((destination_node_id == SERARD_NODE_ID_UNSET) || (destination_node_id == ins->node_id))
        {
            // FIXME: this is not true because of COBS
            // This is the reason the function has a logarithmic time complexity of the number
            // of subscriptions. Note that this is the only variable-complexity operation in
            // the RX pipeline. Excepting these two cases, the entire RX pipeline contains
            // neither loops nor recursion.
            struct SerardRxSubscription* const sub = (struct SerardRxSubscription*) (void*)
                cavl2_find((struct SerardTreeNode*) ins->rx_subscriptions[(size_t) metadata->transfer_kind],
                           &metadata->port_id,
                           &rxSubscriptionPredicateOnPortID);

            if (sub != NULL)
            {
                // found a subscription, so proceed with processing the payload
                reassembler->sub = sub;

                // NOTE: currently the transfer header does not include any information
                // about the payload size, so allocate using the subscription extent.
                // This is expected to be fixed in a future version of the protocol, see:
                // https://github.com/OpenCyphal/specification/issues/143
                //
                // Note also that because the payload size is not known in advance,
                // the allocated buffer must accommodate the transfer CRC overhead
                // even though it is not included in the final payload size.
                const size_t extent = sub->extent + TRANSFER_CRC_SIZE_BYTES;
                SERARD_ASSERT(extent > 0);
                reassembler->max_payload_size = extent;

                void* const payload = ins->memory_payload.allocate(ins->memory_payload.user_reference, extent);
                if (payload != NULL)
                {
                    out_transfer->payload        = payload;
                    out_transfer->payload_extent = extent;
                    ret                          = 1;
                }
                else
                {
                    out_transfer->payload        = NULL;
                    out_transfer->payload_extent = 0;
                    ret                          = -SERARD_ERROR_MEMORY;
                }
            }
            else
            {
                // no subscription to this message, silently discard the transfer
                ret = 0;
            }
        }
        else
        {
            // mis-addressed transfer, silently discard the transfer
            ret = 0;
        }
    }
    else
    {
        // invalid header or corrupt CRC, silently discard the transfer
        ret = 0;
    }

    return ret;
}

/// RX session state machine update is the most intricate part of any Cyphal transport implementation.
/// The state model used here is derived from the reference pseudocode given in the original UAVCAN v0 specification.
/// The Cyphal/CAN v1 specification, which this library is an implementation of, does not provide any reference
/// pseudocode. Instead, it takes a higher-level, more abstract approach, where only the high-level requirements
/// are given and the particular algorithms are left to be implementation-defined. Such abstract approach is much
/// advantageous because it allows implementers to choose whatever solution works best for the specific application at
/// hand, while the wire compatibility is still guaranteed by the high-level requirements given in the specification.
SERARD_PRIVATE void rxSessionUpdate(struct SerardRx* const                ins,
                                    struct SerardInternalRxSession* const rxs,
                                    const struct SerardRxTransfer* const  transfer,
                                    const uint8_t                         redundant_iface_index,
                                    const SerardMicrosecond               transfer_id_timeout_usec)
{
    SERARD_ASSERT(ins != NULL);
    SERARD_ASSERT(rxs != NULL);
    SERARD_ASSERT(transfer != NULL);

    const struct SerardTransferMetadata* metadata = &transfer->metadata;

    const bool tid_timed_out = (transfer->timestamp_usec > rxs->transfer_timestamp_usec) &&
                               ((transfer->timestamp_usec - rxs->transfer_timestamp_usec) > transfer_id_timeout_usec);

    // The monotonic 64 bit transfer ID in UAVCAN/Serial shall not wrap.
    const bool not_monotonic = (metadata->transfer_id - rxs->transfer_id) > 1;

    const bool need_restart = tid_timed_out || ((rxs->redundant_iface_index == redundant_iface_index) && not_monotonic);

    if (need_restart)
    {
        rxs->transfer_id           = metadata->transfer_id;
        rxs->redundant_iface_index = redundant_iface_index;
    }
}

// TODO: test this
SERARD_PRIVATE int8_t rxAcceptTransfer(struct SerardRx* const          ins,
                                       struct SerardReassembler* const reassembler,
                                       struct SerardRxTransfer* const  transfer,
                                       const uint8_t                   redundant_iface_index)
{
    SERARD_ASSERT(ins != NULL);
    SERARD_ASSERT(reassembler != NULL);
    SERARD_ASSERT(transfer != NULL);

    const struct SerardTransferMetadata* const metadata     = &transfer->metadata;
    const struct SerardRxSubscription* const   subscription = reassembler->sub;

    // FIXME: maybe we can just use the out_transfer->size to track the counter?
    const size_t payload_size = reassembler->counter - HEADER_SIZE;
    TransferCRC  payload_crc  = TRANSFER_CRC_INITIAL;
    payload_crc               = transferCRCAdd(payload_crc, payload_size, transfer->payload);
    payload_crc               = payload_crc ^ TRANSFER_CRC_OUTPUT_XOR;
    const bool valid          = payload_crc == TRANSFER_CRC_RESIDUE_AFTER_OUTPUT_XOR;

    if (!valid)
    {
        return 0;
    }

    transfer->payload_size = payload_size - TRANSFER_CRC_SIZE_BYTES;

    if (metadata->remote_node_id <= SERARD_NODE_ID_MAX)
    {
        struct SerardInternalRxSession* rxs =
            (struct SerardInternalRxSession*) cavl2_find((struct SerardTreeNode*) subscription->sessions,
                                                         (void*) &metadata->remote_node_id,
                                                         &rxSubscriptionPredicateOnSession);

        if (rxs == NULL)
        {
            rxs = (struct SerardInternalRxSession*) ins->memory_rx_session
                      .allocate(ins->memory_rx_session.user_reference, sizeof(struct SerardInternalRxSession));

            if (rxs != NULL)
            {
                rxs->transfer_timestamp_usec = transfer->timestamp_usec;
                rxs->source_node_id          = metadata->remote_node_id;
                rxs->transfer_id             = metadata->transfer_id;
                rxs->redundant_iface_index   = redundant_iface_index;

                SERARD_UNUSED(cavl2_find((struct SerardTreeNode*) subscription->sessions,
                                         (void*) &metadata->remote_node_id,
                                         &rxSubscriptionPredicateOnSession));
                rxSessionUpdate(ins, rxs, transfer, redundant_iface_index, subscription->transfer_id_timeout_usec);

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

SERARD_PRIVATE int8_t rxAcceptByte(struct SerardRx* const          ins,
                                   struct SerardReassembler* const reassembler,
                                   const SerardMicrosecond         timestamp_usec,
                                   const uint8_t                   payload_byte,
                                   const uint8_t                   redundant_iface_index,
                                   struct SerardRxTransfer* const  out_transfer)
{
    int8_t                      ret           = 0;
    uint8_t                     cobs_byte     = payload_byte;
    const enum CobsDecodeResult result        = cobsDecodeByte(reassembler, &cobs_byte);
    const bool                  state_payload = reassembler->counter >= HEADER_SIZE;

    if (result == COBS_DECODE_NONE)
    {
        // consume without updating the state machine, these are not
        // part of the original bytestream
        ret = 0;
    }
    else if (result == COBS_DECODE_DELIMITER)
    {
        if (state_payload)
        {
            // if the state machine is accepting the payload, try to accept
            // the received transfer payload and return to the user
            ret = rxAcceptTransfer(ins, reassembler, out_transfer, redundant_iface_index);
        }
        else
        {
            // in other cases, the delimiter is premature so consider the transfer
            // to be invalid and silently discard it
            ret = 0;
        }

        // delimiter bytes unconditionally reset the state machine
        reassembler->counter = 0;
        reassembler->discard = false;
    }
    else
    {
        if (state_payload)
        {
            const size_t   offset  = reassembler->counter - HEADER_SIZE;
            uint8_t* const payload = out_transfer->payload;
            payload[offset]        = cobs_byte;
        }
        else
        {
            const size_t offset         = reassembler->counter;
            reassembler->header[offset] = cobs_byte;

            if (offset == 0)
            {
                // record the fragment timestamp when the first header byte is received
                // see: https://github.com/OpenCyphal/pycyphal/issues/112
                out_transfer->timestamp_usec = timestamp_usec;
            }
            else if (offset == (HEADER_SIZE - 1))
            {
                const int8_t out = rxValidateHeader(ins, reassembler, out_transfer);
                if (out < 0)
                {
                    // rx pipeline encountered error, reject transfer and propogate to user
                    reassembler->discard = true;
                    ret                  = out;
                }
                else if (out == 0)
                {
                    // invalid or mis-addressed header, reject rest of frame
                    reassembler->discard = true;
                }
            }
        }

        // if the state machine is accepting the transfer (!discard), advance
        // the counter (and implicitly the state)
        // it is important not to advance the counter if the transfer is discarded
        // in case the payload exceeds the subscription extent so we don't write
        // to uninitialized memory
        if (!reassembler->discard)
        {
            reassembler->counter++;
        }

        // in any case, the transfer is not complete yet
        ret = 0;
    }

    return ret;
}

// --------------------------------------------- PUBLIC API ---------------------------------------------

struct SerardRx serardRxInit(const struct SerardMemoryResource memory_payload,
                             const struct SerardMemoryResource memory_rx_session)
{
    SERARD_ASSERT(memory_payload.allocate != NULL);
    SERARD_ASSERT(memory_payload.deallocate != NULL);
    SERARD_ASSERT(memory_rx_session.allocate != NULL);
    SERARD_ASSERT(memory_rx_session.deallocate != NULL);

    struct SerardRx serard = {
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
        .counter          = 0,
        .discard          = false,
        .header           = {0},
        .sub              = NULL,
        .max_payload_size = 0,
    };

    return reassembler;
};

int8_t serardTxPush(const SerardNodeID                         local_node_id,
                    const struct SerardTransferMetadata* const metadata,
                    const size_t                               payload_size,
                    const void* const                          payload,
                    void* const                                user_reference,
                    const SerardTxEmit                         emitter)
{
    // With exception of the user_reference, input pointers shall not be NULL.
    if ((metadata == NULL) || (emitter == NULL) || ((payload_size > 0) && payload == NULL))
    {
        return -SERARD_ERROR_ARGUMENT;
    }
    // The priority and transfer kind shall not exceed the maximum.
    if ((metadata->priority > SERARD_PRIORITY_MAX) || (metadata->transfer_kind > SERARD_TRANSFER_KIND_MAX))
    {
        return -SERARD_ERROR_ARGUMENT;
    }
    if (SerardTransferKindMessage == metadata->transfer_kind)
    {
        // The remote node-ID shall be SERARD_NODE_ID_UNSET, and the subject-ID shall not
        // exceed SERARD_SUBJECT_ID_MAX.
        if ((metadata->remote_node_id != SERARD_NODE_ID_UNSET) || (metadata->port_id > SERARD_SUBJECT_ID_MAX))
        {
            return -SERARD_ERROR_ARGUMENT;
        }
    }
    else
    {
        // The remote node-ID shall not exceed SERARD_NODE_ID_MAX, and the service-ID shall
        // not exceed SERARD_SERVICE_ID_MAX. The local node shall not be anonymous.
        if ((metadata->remote_node_id > SERARD_NODE_ID_MAX) || (metadata->port_id > SERARD_SERVICE_ID_MAX) ||
            (local_node_id == SERARD_NODE_ID_UNSET))
        {
            return -SERARD_ERROR_ARGUMENT;
        }
    }

    struct CobsEncoder encoder = {
        .emitter        = emitter,
        .user_reference = user_reference,
        .in             = 0,
        .fifo           = {0},
    };

    // emit the leading delimiter
    const uint8_t delimiter = COBS_FRAME_DELIMITER;
    if (!emitter(user_reference, 1, &delimiter))
    {
        return 0;
    }

    uint8_t header[HEADER_SIZE];
    txMakeHeader(local_node_id, metadata, header);
    for (size_t i = 0; i < HEADER_SIZE; i++)
    {
        if (!cobsPush(&encoder, header[i]))
        {
            return 0;
        }
    }

    // if statement is redundant with the for loop but eliding it impedes readability
    if (payload_size > 0)
    {
        const uint8_t* pl = payload;
        SERARD_ASSERT(pl != NULL);

        for (size_t i = 0; i < payload_size; i++)
        {
            if (!cobsPush(&encoder, pl[i]))
            {
                return 0;
            }
        }
    }

    TransferCRC crc = transferCRCAdd(TRANSFER_CRC_INITIAL, payload_size, payload) ^ TRANSFER_CRC_OUTPUT_XOR;
    cobsPush(&encoder, (uint8_t) ((crc >> BYTE0_OFFSET) & BYTE_MAX));
    cobsPush(&encoder, (uint8_t) ((crc >> BYTE1_OFFSET) & BYTE_MAX));
    cobsPush(&encoder, (uint8_t) ((crc >> BYTE2_OFFSET) & BYTE_MAX));
    cobsPush(&encoder, (uint8_t) ((crc >> BYTE3_OFFSET) & BYTE_MAX));

    if (!cobsFlush(&encoder))
    {
        return 0;
    }

    // emit the trailing delimiter
    if (!emitter(user_reference, 1, &delimiter))
    {
        return 0;
    }

    return 1;
}

int8_t serardRxAccept(struct SerardRx* const              ins,
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

    int8_t ret = 0;
    for (size_t i = 0; i < in_payload_size; i++)
    {
        const uint8_t payload_byte = payload[i];
        const int8_t  out =
            rxAcceptByte(ins, reassembler, timestamp_usec, payload_byte, redundant_transport_index, out_transfer);
        if (out != 0)
        {
            if (out < 0)
            {
                // rx pipeline encountered error, stop parsing
            }
            else if (out > 0)
            {
                // a valid transfer was accepted
                const size_t bytes_consumed = i + 1;
                *inout_payload_size         = in_payload_size - bytes_consumed;
                *out_subscription           = reassembler->sub;
            }

            ret = out;
            break;
        }
    }

    return ret;
}

int8_t serardRxSubscribe(struct SerardRx* const             ins,
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

            const struct SerardTreeNode* const node =
                cavl2_find_or_insert((struct SerardTreeNode**) &ins->rx_subscriptions[tk],
                                     &port_id,
                                     &rxSubscriptionPredicateOnPortID,
                                     &out_subscription->base,
                                     &cavl2_trivial_factory);
            SERARD_ASSERT(node == &out_subscription->base);
            out = (out > 0) ? 0 : 1;
        }
    }

    return out;
}

int8_t serardRxUnsubscribe(struct SerardRx* const        ins,
                           const enum SerardTransferKind transfer_kind,
                           const SerardPortID            port_id)
{
    int8_t       ret = -SERARD_ERROR_ARGUMENT;
    const size_t tk  = (size_t) transfer_kind;
    if ((ins != NULL) && (tk < SERARD_NUM_TRANSFER_KINDS))
    {
        SerardPortID                       port_id_mutable = port_id;
        struct SerardRxSubscription* const sub =
            (struct SerardRxSubscription*) (void*) cavl2_find((struct SerardTreeNode*) ins->rx_subscriptions[tk],
                                                              &port_id_mutable,
                                                              &rxSubscriptionPredicateOnPortID);
        if (sub != NULL)
        {
            cavl2_remove((struct SerardTreeNode**) &ins->rx_subscriptions[tk], &sub->base);
            SERARD_ASSERT(sub->port_id == port_id);
            ret = 1;
            // TODO: we should be doing this in O(n), not O(n log n), and without unecessary rotation
            // this can be done with the new iteration api
            while (sub->sessions != NULL)
            {
                cavl2_remove((struct SerardTreeNode**) &ins->rx_subscriptions[tk],
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
