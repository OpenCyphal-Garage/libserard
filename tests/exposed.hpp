// This software is distributed under the terms of the MIT License.
// Copyright (c) 2024 Cyphal Development Team.

#pragma once

#include "serard.h"
#include <cstdarg>
#include <cstdint>
#include <limits>
#include <array>
#include <stdexcept>

/// Definitions that are not exposed by the library but that are needed for testing.
/// Please keep them in sync with the library by manually updating as necessary.
namespace exposed
{
constexpr std::uint8_t COBS_FRAME_DELIMITER = 0U;
constexpr std::size_t  COBS_LOOKAHEAD_SIZE  = 256U;

using HeaderCRC   = std::uint16_t;
using TransferCRC = std::uint32_t;

struct RxSession
{
    struct SerardTreeNode base = {};

    SerardMicrosecond transfer_timestamp_usec = std::numeric_limits<std::uint64_t>::max();
    SerardTransferID  transfer_id             = std::numeric_limits<std::uint8_t>::max();
    SerardNodeID      source_node_id          = 0U;
    std::uint8_t      redundant_iface_index   = std::numeric_limits<std::uint8_t>::max();
};

struct CobsEncoder
{
    SerardTxEmit                                  emitter;
    void*                                         user_reference;
    std::uint8_t                                  in;
    std::array<std::uint8_t, COBS_LOOKAHEAD_SIZE> fifo;

    CobsEncoder(SerardTxEmit const _emitter, void* const _user_reference, std::uint8_t _in) :
        emitter(_emitter), user_reference(_user_reference), in(_in), fifo()
    {}
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

auto               cobsPush(struct CobsEncoder* const encoder, std::uint8_t const byte) -> void;
auto               cobsFlush(struct CobsEncoder* const encoder) -> void;
[[nodiscard]] auto cobsDecodeByte(struct SerardReassembler* const reassembler,
                                  std::uint8_t* const             inout_byte) -> CobsDecodeResult;

auto               hostToLittle16(std::uint16_t const in, std::uint8_t* const out) -> void;
auto               hostToLittle32(std::uint32_t const in, std::uint8_t* const out) -> void;
auto               hostToLittle64(std::uint64_t const in, std::uint8_t* const out) -> void;
[[nodiscard]] auto littleToHost16(const std::uint8_t* const in) -> std::uint16_t;
[[nodiscard]] auto littleToHost32(const std::uint8_t* const in) -> std::uint32_t;
[[nodiscard]] auto littleToHost64(const std::uint8_t* const in) -> std::uint64_t;

[[nodiscard]] auto txMakeSessionSpecifier(const enum SerardTransferKind transfer_kind,
                                          const SerardPortID            port_id) -> std::uint16_t;
auto               txMakeHeader(const SerardNodeID                         node_id,
                                const struct SerardTransferMetadata* const metadata,
                                void* const                                buffer) -> void;

[[nodiscard]] auto rxSubscriptionPredicateOnSession(const void* const                  user_reference,
                                                    const struct SerardTreeNode* const node) -> std::int8_t;
[[nodiscard]] auto rxSubscriptionPredicateOnPortID(const void* const                  user_reference,
                                                   const struct SerardTreeNode* const node) -> std::int8_t;
[[nodiscard]] auto rxTryParseHeader(const std::uint8_t* const            payload,
                                    struct SerardTransferMetadata* const out_metadata,
                                    SerardNodeID* const                  out_destination_node_id) -> bool;
[[nodiscard]] auto rxValidateHeader(struct SerardRx* const          ins,
                                    struct SerardReassembler* const reassembler,
                                    struct SerardRxTransfer* const  out_transfer) -> std::int8_t;
auto               rxSessionUpdate(struct SerardRx* const                ins,
                                   struct SerardInternalRxSession* const rxs,
                                   const struct SerardRxTransfer* const  transfer,
                                   const std::uint8_t                    redundant_iface_index,
                                   const SerardMicrosecond               transfer_id_timeout_usec) -> void;
[[nodiscard]] auto rxAcceptTransfer(struct SerardRx* const          ins,
                                    struct SerardReassembler* const reassembler,
                                    struct SerardRxTransfer* const  transfer,
                                    const std::uint8_t              redundant_iface_index) -> std::int8_t;
[[nodiscard]] auto rxAcceptByte(struct SerardRx* const          ins,
                                struct SerardReassembler* const reassembler,
                                const SerardMicrosecond         timestamp_usec,
                                const std::uint8_t              payload_byte,
                                const std::uint8_t              redundant_iface_index,
                                struct SerardRxTransfer* const  out_transfer) -> std::int8_t;
}
}  // namespace exposed
