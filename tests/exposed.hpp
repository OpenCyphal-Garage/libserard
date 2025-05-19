// This software is distributed under the terms of the MIT License.
// Copyright (c) 2024 Cyphal Development Team.

#pragma once

#include "serard.h"
#include <cstdarg>
#include <cstdint>
#include <limits>
#include <stdexcept>

/// Definitions that are not exposed by the library but that are needed for testing.
/// Please keep them in sync with the library by manually updating as necessary.
namespace exposed
{
constexpr std::uint8_t COBS_FRAME_DELIMITER = 0U;

using HeaderCRC   = std::uint16_t;
using TransferCRC = std::uint32_t;

struct CobsEncoder
{
    SerardTxEmit emitter;
    void*        user_reference;
    std::uint8_t in;
    std::uint8_t fifo[256];

    CobsEncoder(SerardTxEmit const _emitter, void* const _user_reference, std::uint8_t _in) :
        emitter(_emitter), user_reference(_user_reference), in(_in), fifo(0)
    {}

    // CobsEncoder(std::size_t const _loc, std::size_t const _chunk)
    // {
    //     loc   = _loc;
    //     chunk = _chunk;
    // }
};

struct RxSession
{
    struct SerardTreeNode base = {0};

    SerardMicrosecond transfer_timestamp_usec = std::numeric_limits<std::uint64_t>::max();
    SerardNodeID      source_node_id          = 0U;
    std::size_t       total_payload_size      = 0U;
    std::size_t       payload_size            = 0U;
    std::uint8_t*     payload                 = nullptr;
    SerardTransferID  transfer_id             = std::numeric_limits<std::uint8_t>::max();
    // std::uint8_t      redundant_iface_index   = std::numeric_limits<std::uint8_t>::max();
    // bool              toggle                  = false;
};

struct RxTransferModel
{
    SerardMicrosecond  timestamp_usec      = std::numeric_limits<std::uint64_t>::max();
    SerardPriority     priority            = SerardPriorityOptional;
    SerardTransferKind transfer_kind       = SerardTransferKindMessage;
    SerardPortID       port_id             = std::numeric_limits<std::uint16_t>::max();
    SerardNodeID       source_node_id      = SERARD_NODE_ID_UNSET;
    SerardNodeID       destination_node_id = SERARD_NODE_ID_UNSET;
    SerardTransferID   transfer_id         = std::numeric_limits<std::uint8_t>::max();
};

enum class ReassemblerState
{
    REJECT = 0U,
    DELIMITER,
    HEADER,
    PAYLOAD,
};

enum class CobsDecodeResult
{
    DELIMITER = 0U,
    NONE,
    DATA,
};

constexpr TransferCRC TRANSFER_CRC_INITIAL    = 0xFFFFFFFFUL;
constexpr TransferCRC TRANSFER_CRC_OUTPUT_XOR = 0xFFFFFFFFUL;

constexpr std::size_t TRANSFER_CRC_SIZE_BYTES = sizeof(TransferCRC);

// Extern C effectively discards the outer namespaces.
extern "C" {

[[nodiscard]] auto headerCRCAddByte(const HeaderCRC crc, const std::uint8_t byte) -> HeaderCRC;
[[nodiscard]] auto headerCRCAdd(const HeaderCRC crc, const std::size_t size, const void* const data) -> HeaderCRC;
[[nodiscard]] auto transferCRCAddByte(const TransferCRC crc, const std::uint8_t byte) -> TransferCRC;
[[nodiscard]] auto transferCRCAdd(const TransferCRC crc, const std::size_t size, const void* const data) -> TransferCRC;

void               cobsPush(struct CobsEncoder* const encoder, std::uint8_t const byte);
void               cobsFlush(struct CobsEncoder* const encoder);
[[nodiscard]] auto cobsDecodeByte(struct SerardReassembler* const reassembler,
                                  uint8_t* const                  inout_byte) -> CobsDecodeResult;

void               hostToLittle16(uint16_t const in, uint8_t* const out);
void               hostToLittle32(uint32_t const in, uint8_t* const out);
void               hostToLittle64(uint64_t const in, uint8_t* const out);
[[nodiscard]] auto littleToHost16(const uint8_t* const in) -> std::uint16_t;
[[nodiscard]] auto littleToHost32(const uint8_t* const in) -> std::uint32_t;
[[nodiscard]] auto littleToHost64(const uint8_t* const in) -> std::uint64_t;

[[nodiscard]] auto txMakeSessionSpecifier(const enum SerardTransferKind transfer_kind,
                                          const SerardPortID            port_id) -> std::uint16_t;
void txMakeHeader(const SerardNodeID node_id, const struct SerardTransferMetadata* const metadata, void* const buffer);

void               rxInitTransferMetadataFromModel(const struct RxTransferModel* const  frame,
                                                   struct SerardTransferMetadata* const out_transfer);
[[nodiscard]] auto rxTryParseHeader(const SerardMicrosecond       timestamp_usec,
                                    const std::uint8_t* const     payload,
                                    struct RxTransferModel* const out) -> bool;
[[nodiscard]] auto rxTryValidateHeader(struct SerardRx* const          ins,
                                       struct SerardReassembler* const reassembler,
                                       const SerardMicrosecond         timestamp_usec,
                                       struct SerardRxTransfer* const  out_transfer) -> std::int8_t;
[[nodiscard]] auto rxAcceptTransfer(struct SerardRx* const          ins,
                                    struct SerardRxTransfer* const  transfer,
                                    struct SerardReassembler* const reassembler,
                                    const SerardMicrosecond         timestamp_usec) -> bool;
//
// auto rxSessionWritePayload(CanardInstance* const ins,
//                            RxSession* const      rxs,
//                            const std::size_t     extent,
//                            const std::size_t     payload_size,
//                            const void* const     payload) -> std::int8_t;
//
// void rxSessionRestart(CanardInstance* const ins, RxSession* const rxs);
//
// auto rxSessionUpdate(CanardInstance* const     ins,
//                      RxSession* const          rxs,
//                      const RxFrameModel* const frame,
//                      const std::uint8_t        redundant_iface_index,
//                      const CanardMicrosecond   transfer_id_timeout_usec,
//                      const std::size_t         extent,
//                      CanardRxTransfer* const   out_transfer) -> std::int8_t;
}
}  // namespace exposed
