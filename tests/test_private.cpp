// This software is distributed under the terms of the MIT License.
// Copyright (c) 2022 OpenCyphal

#include <catch.hpp>
#include <cstdio>
#include "exposed.hpp"
#include "serard.h"
#include <random>
#include <limits>

using buffer_t = std::vector<std::uint8_t>;

static bool serardEmitter(void* const user_reference, uint8_t data_size, const uint8_t* data)
{
    REQUIRE(data_size > 0);
    REQUIRE(data != NULL);

    auto* const buffer = reinterpret_cast<buffer_t*>(user_reference);
    buffer->insert(buffer->end(), data, data + data_size);

    return true;
}

// Test the cobs encoder state machine on various transitions and edge cases.
// Note that cobsPush does not output a starting nor ending delimiter so
// we don't expect any in the output.
TEST_CASE("cobsPush")
{
    const auto encode = [](auto const in, auto const expected, std::uint8_t start = 0U) -> bool {
        buffer_t             out;
        exposed::CobsEncoder encoder(serardEmitter, &out, start);
        for (size_t i = 0; i < in.size(); i++)
        {
            exposed::cobsPush(&encoder, in[i]);
        }
        exposed::cobsFlush(&encoder);

        bool ret = out == expected;
        for (const auto byte : out)
        {
            ret = ret && (byte != exposed::COBS_FRAME_DELIMITER);
        }

        // for (auto x : out)
        //     printf("%02x ", x);
        // printf("\n");

        return ret;
    };

    // Testing input bytes != COBS_FRAME_DELIMITER, and ignoring the 255
    // byte jump limit
    // * these should always be passed through to the output pointer
    // * the encoder state machine should increment its write pointer
    // * the "chunk" pointer should remain stable
    {
        using u8_limits = std::numeric_limits<std::uint8_t>;
        for (std::uint32_t val = u8_limits::min(); val <= u8_limits::max(); val++)
        {
            const std::uint8_t in_byte = static_cast<std::uint8_t>(val);
            if (in_byte == exposed::COBS_FRAME_DELIMITER)
            {
                continue;
            }

            const std::array<std::uint8_t, 1> in  = {in_byte};
            const buffer_t                    out = {0x02, in_byte};
            REQUIRE(encode(in, out));
        }
    }

    {
        const std::array<std::uint8_t, 2> in       = {0x00, 0x00};
        const buffer_t                    expected = {0x01, 0x01, 0x01};
        REQUIRE(encode(in, expected));
    }

    {
        const std::array<std::uint8_t, 2> in       = {0x01, 0x00};
        const buffer_t                    expected = {0x02, 0x01, 0x01};
        REQUIRE(encode(in, expected));
    }

    {
        const std::array<std::uint8_t, 2> in       = {0x02, 0x00};
        const buffer_t                    expected = {0x02, 0x02, 0x01};
        REQUIRE(encode(in, expected));
    }

    {
        const std::array<std::uint8_t, 2> in       = {0x03, 0x00};
        const buffer_t                    expected = {0x02, 0x03, 0x01};
        REQUIRE(encode(in, expected));
    }

    {
        const std::array<std::uint8_t, 3> in       = {0x00, 0x00, 0x00};
        const buffer_t                    expected = {0x01, 0x01, 0x01, 0x01};
        REQUIRE(encode(in, expected));
    }

    {
        const std::array<std::uint8_t, 3> in       = {0x00, 0x01, 0x00};
        const buffer_t                    expected = {0x01, 0x02, 0x01, 0x01};
        REQUIRE(encode(in, expected));
    }

    {
        std::array<std::uint8_t, 256> in{};
        for (std::size_t i = 0x01; i <= 0xFF; i++)
        {
            in[i - 1] = static_cast<std::uint8_t>(i);
        }
        in[255] = 0x00;

        buffer_t expected(258);
        expected[0] = 0xFF;
        for (std::size_t i = 0x01; i <= 0xFE; i++)
        {
            expected[i] = static_cast<std::uint8_t>(i);
        }
        expected[255] = 0x02;
        expected[256] = 0xFF;
        expected[257] = 0x01;
        REQUIRE(encode(in, expected));
    }
}

// TODO: cobs decoding byte state machine (pull from kocherga?)

TEST_CASE("cobsDecode")
{
    using Result = exposed::CobsDecodeResult;

    auto decode = [&](const auto& in, auto& out) -> void {
        auto        reassembler   = serardReassemblerInit();
        std::size_t in_index      = 0;
        std::size_t out_index     = 0;
        bool        leading_delim = true;
        for (std::size_t i = 0; i < in.size(); i++)
        {
            auto       inout_byte = in[i];
            const auto result     = exposed::cobsDecodeByte(&reassembler, &inout_byte);
            switch (result)
            {
            case Result::DELIMITER:
                REQUIRE((leading_delim || (i == (in.size() - 1))));
                break;
            case Result::NONE:
                leading_delim = false;
                break;
            case Result::DATA:
                REQUIRE(out_index < out.size());
                out[out_index++] = inout_byte;
                leading_delim    = false;
                break;
            default:
                REQUIRE(false);
                break;
            }
        }

        REQUIRE(out_index == out.size());
    };

    {
        const std::array<std::uint8_t, 3> in = {0x01, 0x01, 0x00};
        std::array<std::uint8_t, 1>       out{};
        decode(in, out);

        const std::array<std::uint8_t, 1> expected = {0x00};
        REQUIRE(expected == out);
    }

    {
        const std::array<std::uint8_t, 3> in = {0x02, 0x01, 0x00};
        std::array<std::uint8_t, 1>       out{};
        decode(in, out);

        const std::array<std::uint8_t, 1> expected = {0x01};
        REQUIRE(expected == out);
    }

    {
        const std::array<std::uint8_t, 3> in = {0x02, 0x02, 0x00};
        std::array<std::uint8_t, 1>       out{};
        decode(in, out);

        const std::array<std::uint8_t, 1> expected = {0x02};
        REQUIRE(expected == out);
    }

    {
        const std::array<std::uint8_t, 3> in = {0x02, 0x03, 0x00};
        std::array<std::uint8_t, 1>       out{};
        decode(in, out);

        const std::array<std::uint8_t, 1> expected = {0x03};
        REQUIRE(expected == out);
    }

    {
        const std::array<std::uint8_t, 4> in = {0x01, 0x01, 0x01, 0x00};
        std::array<std::uint8_t, 2>       out{};
        decode(in, out);

        const std::array<std::uint8_t, 2> expected = {0x00, 0x00};
        REQUIRE(expected == out);
    }

    {
        const std::array<std::uint8_t, 4> in = {0x01, 0x02, 0x01, 0x00};
        std::array<std::uint8_t, 2>       out{};
        decode(in, out);

        const std::array<std::uint8_t, 2> expected = {0x00, 0x01};
        REQUIRE(expected == out);
    }

    {
        std::array<std::uint8_t, 258> in{};
        in[0] = 0xFF;
        for (std::size_t i = 0x01; i <= 0xFE; i++)
        {
            in[i] = static_cast<std::uint8_t>(i);
        }
        in[255] = 0x02;
        in[256] = 0xFF;
        in[257] = 0x00;

        std::array<std::uint8_t, 255> expected{};
        for (std::size_t i = 0x01; i <= 0xFF; i++)
        {
            expected[i - 1] = static_cast<std::uint8_t>(i);
        }

        std::array<std::uint8_t, 255> out{};
        decode(in, out);
        REQUIRE(expected == out);
    }
}

TEST_CASE("HeaderCRC")
{
    exposed::HeaderCRC crc = 0xFFFFU;

    crc = exposed::headerCRCAdd(crc, 1, "1");
    crc = exposed::headerCRCAdd(crc, 1, "2");
    crc = exposed::headerCRCAdd(crc, 1, "3");
    REQUIRE(0x5BCEU == crc);
    crc = exposed::headerCRCAdd(crc, 6, "456789");
    REQUIRE(0x29B1U == crc);
}

static void testTransferCRC(void)
{
    exposed::TransferCRC crc = exposed::TRANSFER_CRC_INITIAL;
    crc                      = exposed::transferCRCAdd(crc, 3, "123");
    crc                      = exposed::transferCRCAdd(crc, 6, "456789");
    REQUIRE(0x1CF96D7CUL == crc);
    REQUIRE(0xE3069283UL == (crc ^ exposed::TRANSFER_CRC_OUTPUT_XOR));
    crc = exposed::transferCRCAdd(crc,
                                  4,
                                  "\x83"  // Least significant byte first.
                                  "\x92"
                                  "\x06"
                                  "\xE3");
    REQUIRE(0xB798B438UL == crc);
    REQUIRE(0x48674BC7UL == (crc ^ exposed::TRANSFER_CRC_OUTPUT_XOR));
}

TEST_CASE("hostToLittle")
{
    {
        std::array<std::uint8_t, 2> buffer{};
        exposed::hostToLittle16(0x0102, buffer.data());
        const std::array<std::uint8_t, 2> expected = {0x02, 0x01};
        REQUIRE(expected == buffer);
    }

    {
        std::array<std::uint8_t, 4> buffer{};
        exposed::hostToLittle32(0x01020304, buffer.data());
        const std::array<std::uint8_t, 4> expected = {0x04, 0x03, 0x02, 0x01};
        REQUIRE(expected == buffer);
    }

    {
        std::array<std::uint8_t, 8> buffer{};
        exposed::hostToLittle64(0x0102030405060708, buffer.data());
        const std::array<std::uint8_t, 8> expected = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
        REQUIRE(expected == buffer);
    }
}

TEST_CASE("littleToHost")
{
    {
        std::array<std::uint8_t, 2> buffer = {0x02, 0x01};
        const auto                  ret    = exposed::littleToHost16(buffer.data());
        REQUIRE(0x0102 == ret);
    }

    {
        std::array<std::uint8_t, 4> buffer = {0x04, 0x03, 0x02, 0x01};
        const auto                  ret    = exposed::littleToHost32(buffer.data());
        REQUIRE(0x01020304 == ret);
    }

    {
        std::array<std::uint8_t, 8> buffer = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
        const auto                  ret    = exposed::littleToHost64(buffer.data());
        REQUIRE(0x0102030405060708 == ret);
    }
}

TEST_CASE("txMakeSessionSpecifier")
{
    REQUIRE(0x0afe == exposed::txMakeSessionSpecifier(SerardTransferKindMessage, 0xafe));
    REQUIRE(0xc1fe == exposed::txMakeSessionSpecifier(SerardTransferKindRequest, 0x1fe));
    REQUIRE(0x81fe == exposed::txMakeSessionSpecifier(SerardTransferKindResponse, 0x1fe));
}

TEST_CASE("txMakeHeader")
{
    const SerardNodeID node_id = 1234;

    {
        struct SerardTransferMetadata metadata = {
            .priority       = SerardPriorityNominal,
            .transfer_kind  = SerardTransferKindMessage,
            .port_id        = 1234,
            .remote_node_id = 4321,
            .transfer_id    = 0,
        };

        std::array<std::uint8_t, 24> buffer   = {0};
        std::array<std::uint8_t, 24> expected = {0x01, 0x04, 0xD2, 0x04, 0xE1, 0x10, 0xD2, 0x04,
                                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x4A, 0xD6};
        exposed::txMakeHeader(node_id, &metadata, &buffer);
        REQUIRE(expected == buffer);
    }

    {
        struct SerardTransferMetadata metadata = {
            .priority       = SerardPriorityImmediate,
            .transfer_kind  = SerardTransferKindResponse,
            .port_id        = 123,
            .remote_node_id = 4321,
            .transfer_id    = 0,
        };

        std::array<std::uint8_t, 24> buffer   = {0};
        std::array<std::uint8_t, 24> expected = {0x01, 0x01, 0xD2, 0x04, 0xE1, 0x10, 0x7B, 0x80,
                                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0xC0, 0xFF};
        exposed::txMakeHeader(node_id, &metadata, &buffer);
        REQUIRE(expected == buffer);
    }

    {
        struct SerardTransferMetadata metadata = {
            .priority       = SerardPriorityOptional,
            .transfer_kind  = SerardTransferKindRequest,
            .port_id        = 456,
            .remote_node_id = 4321,
            .transfer_id    = 0xCAFEB0BAUL,
        };

        std::array<std::uint8_t, 24> buffer   = {0};
        std::array<std::uint8_t, 24> expected = {0x01, 0x07, 0xD2, 0x04, 0xE1, 0x10, 0xC8, 0xC1,
                                                 0xBA, 0xB0, 0xFE, 0xCA, 0x00, 0x00, 0x00, 0x00,
                                                 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x73, 0x08};
        exposed::txMakeHeader(node_id, &metadata, &buffer);
        REQUIRE(expected == buffer);
    }
}

void* serardAlloc(void* const user_reference, const size_t size)
{
    (void) user_reference;
    return malloc(size);
}

void serardFree(void* const user_reference, const size_t size, void* const pointer)
{
    (void) user_reference;
    (void) size;
    free(pointer);
}

TEST_CASE("rxTryParseHeader")
{
    struct SerardMemoryResource allocator = {
        .user_reference = nullptr,
        .allocate       = &serardAlloc,
        .deallocate     = &serardFree,
    };
    struct SerardRx serard = serardInit(allocator, allocator);

    exposed::RxTransferModel out{};

    {
        std::array<std::uint8_t, 24> buffer = {0x01, 0x04, 0xD2, 0x04, 0xE1, 0x10, 0xD2, 0x04, 0x00, 0x00, 0x00, 0x00,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x4A, 0xD6};
        const bool                   valid  = exposed::rxTryParseHeader(0, buffer.begin(), &out);
        REQUIRE(valid);
        REQUIRE(out.transfer_id == 0);
        REQUIRE(out.transfer_kind == SerardTransferKindMessage);
        REQUIRE(out.destination_node_id == 4321);
        REQUIRE(out.source_node_id == 1234);
        REQUIRE(out.port_id == 1234);
        REQUIRE(out.priority == SerardPriorityNominal);
        REQUIRE(out.timestamp_usec == 0);
    }

    {
        std::array<std::uint8_t, 24> buffer = {0x01, 0x01, 0xD2, 0x04, 0xE1, 0x10, 0x7B, 0x80, 0x00, 0x00, 0x00, 0x00,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0xC0, 0xFF};
        const bool                   valid  = exposed::rxTryParseHeader(0x12345678, buffer.begin(), &out);
        REQUIRE(valid);
        REQUIRE(out.transfer_id == 0);
        REQUIRE(out.transfer_kind == SerardTransferKindResponse);
        REQUIRE(out.destination_node_id == 4321);
        REQUIRE(out.source_node_id == 1234);
        REQUIRE(out.port_id == 123);
        REQUIRE(out.priority == SerardPriorityImmediate);
        REQUIRE(out.timestamp_usec == 0x12345678);
    }

    {
        std::array<std::uint8_t, 24> buffer = {0x01, 0x07, 0xD2, 0x04, 0xE1, 0x10, 0xEA, 0xC0, 0xBA, 0xB0, 0xFE, 0xCA,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0xDB, 0x89};
        const bool                   valid  = exposed::rxTryParseHeader(1, buffer.begin(), &out);
        REQUIRE(valid);
        REQUIRE(out.transfer_id == 0xCAFEB0BAUL);
        REQUIRE(out.transfer_kind == SerardTransferKindRequest);
        REQUIRE(out.destination_node_id == 4321);
        REQUIRE(out.source_node_id == 1234);
        REQUIRE(out.port_id == 234);
        REQUIRE(out.priority == SerardPriorityOptional);
        REQUIRE(out.timestamp_usec == 1);
    }
}

TEST_CASE("serardRxAcceptInternal")
{
    using State = exposed::ReassemblerState;

    // TODO: test that invalid messages are discarded
    // TODO: whitebox testing of RX state machine
    struct SerardMemoryResource allocator = {
        .user_reference = nullptr,
        .allocate       = &serardAlloc,
        .deallocate     = &serardFree,
    };

    // non-anonymous node with no subscriptions:
    // feed a message and make sure the state machine
    // goes through the right transitions and validates,
    // then discards it as unimportant
    {
        struct SerardRx serard            = serardInit(allocator, allocator);
        serard.node_id                    = 4321;
        SerardReassembler     reassembler = serardReassemblerInit();
        SerardRxTransfer      out;
        SerardRxSubscription* out_sub = nullptr;

        // initially in rejection state
        REQUIRE(State::REJECT == static_cast<State>(reassembler.state));

        // stay in reject as long as non-delimiters are passed
        const std::array<std::uint8_t, 8> junk = {0x12, 0x34, 0x56, 0x78, 0x01, 0x01, 0xca, 0xfe};
        for (const auto byte : junk)
        {
            std::size_t  inout_size = 1U;
            const int8_t ret        = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
            REQUIRE(0U == ret);
            REQUIRE(State::REJECT == static_cast<State>(reassembler.state));
            REQUIRE(0U == reassembler.counter);
        }

        // feed in a delimiter, the state should transition
        // we should be able to tolerate multiple delimiters
        for (std::size_t i = 0; i < 4; i++)
        {
            const std::uint8_t byte       = exposed::COBS_FRAME_DELIMITER;
            std::size_t        inout_size = 1U;
            const int8_t       ret = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
            REQUIRE(0U == ret);
            REQUIRE(State::DELIMITER == static_cast<State>(reassembler.state));
        }

        const std::array<std::uint8_t, 25> header_enc = {0x09, 0x01, 0x04, 0xd2, 0x04, 0xe1, 0x10, 0xd2, 0x04,
                                                         0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                                                         0x01, 0x02, 0x80, 0x01, 0x10, 0x4a, 0xd6};
        const std::array<std::uint8_t, 24> header_raw = {
            0x01, 0x04, 0xD2, 0x04, 0xE1, 0x10, 0xD2, 0x04, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x4A, 0xD6,
        };

        // feed in the first byte of the header - this is a cobs overhead byte
        // so the state machine should stay stable
        {
            REQUIRE(State::DELIMITER == static_cast<State>(reassembler.state));
            const auto   byte       = header_enc[0];
            std::size_t  inout_size = 1U;
            const int8_t ret        = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
            REQUIRE(0U == ret);
        }

        // feed in the second byte of the header - the state machine
        // should transition to latch the header
        {
            REQUIRE(State::DELIMITER == static_cast<State>(reassembler.state));
            const auto   byte       = header_enc[1];
            std::size_t  inout_size = 1U;
            const int8_t ret        = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
            REQUIRE(0U == ret);

            REQUIRE(State::HEADER == static_cast<State>(reassembler.state));
            REQUIRE(1U == reassembler.counter);
            REQUIRE(byte == reassembler.header[0]);
        }

        // feed in the header (exept last byte)
        for (std::size_t i = 2; i < header_enc.size() - 1; i++)
        {
            const auto   byte       = header_enc[i];
            std::size_t  inout_size = 1U;
            const int8_t ret        = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
            REQUIRE(0U == ret);

            REQUIRE(State::HEADER == static_cast<State>(reassembler.state));
        }

        // feed in the last byte of the header - the state machine
        // should validate and reject the header (since we aren't subscribed)
        {
            REQUIRE(State::HEADER == static_cast<State>(reassembler.state));
            const auto   byte       = header_enc[24];
            std::size_t  inout_size = 1U;
            const int8_t ret        = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
            REQUIRE(0U == ret);

            // the API is not required to preserve the header on rejected transfers
            // (or in general) but this implementation does, so verify that it
            // correctly decoded and parsed the header
            REQUIRE(State::REJECT == static_cast<State>(reassembler.state));
            REQUIRE(24U == reassembler.counter);
            for (std::size_t i = 0; i < 24U; i++)
            {
                INFO(i);
                INFO((int) header_raw[i]);
                INFO((int) reassembler.header[i]);
                REQUIRE(header_raw[i] == reassembler.header[i]);
            }
        }

        // keep feeding a mock payload - the state machine should
        // continue to reject it
        const std::array<std::uint8_t, 13> payload_enc = {
            0x30,
            0x31,
            0x32,
            0x33,
            0x34,
            0x35,
            0x36,
            0x37,
            0x38,
            0xd2,
            0xee,
            0x56,
            0xc8,
        };

        for (const auto byte : payload_enc)
        {
            std::size_t  inout_size = 1U;
            const int8_t ret        = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
            REQUIRE(0U == ret);
            REQUIRE(State::REJECT == static_cast<State>(reassembler.state));
        }

        // feed in a delimiter, the state should transition
        const std::uint8_t byte       = exposed::COBS_FRAME_DELIMITER;
        std::size_t        inout_size = 1U;
        const int8_t       ret        = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
        REQUIRE(0U == ret);
        REQUIRE(State::DELIMITER == static_cast<State>(reassembler.state));
    }

    // try the same message again, but this time, subscribe to it
    {
        struct SerardRx serard = serardInit(allocator, allocator);
        serard.node_id         = 4321;

        SerardRxSubscription sub;
        serardRxSubscribe(&serard, SerardTransferKindMessage, 1234, 16, 1000, &sub);

        SerardReassembler     reassembler = serardReassemblerInit();
        SerardRxTransfer      out;
        SerardRxSubscription* out_sub = nullptr;

        // initially in rejection state
        REQUIRE(State::REJECT == static_cast<State>(reassembler.state));

        // stay in reject as long as non-delimiters are passed
        const std::array<std::uint8_t, 8> junk = {0x12, 0x34, 0x56, 0x78, 0x01, 0x01, 0xca, 0xfe};
        for (const auto byte : junk)
        {
            std::size_t  inout_size = 1U;
            const int8_t ret        = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
            REQUIRE(0U == ret);
            REQUIRE(State::REJECT == static_cast<State>(reassembler.state));
            REQUIRE(0U == reassembler.counter);
        }

        // feed in a delimiter, the state should transition
        // we should be able to tolerate multiple delimiters
        for (std::size_t i = 0; i < 4; i++)
        {
            const std::uint8_t byte       = exposed::COBS_FRAME_DELIMITER;
            std::size_t        inout_size = 1U;
            const int8_t       ret = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
            REQUIRE(0U == ret);
            REQUIRE(State::DELIMITER == static_cast<State>(reassembler.state));
        }

        const std::array<std::uint8_t, 25> header_enc = {0x09, 0x01, 0x04, 0xd2, 0x04, 0xe1, 0x10, 0xd2, 0x04,
                                                         0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                                                         0x01, 0x02, 0x80, 0x01, 0x10, 0x4a, 0xd6};
        const std::array<std::uint8_t, 24> header_raw = {
            0x01, 0x04, 0xD2, 0x04, 0xE1, 0x10, 0xD2, 0x04, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x4A, 0xD6,
        };

        // feed in the first byte of the header - this is a cobs overhead byte
        // so the state machine should stay stable
        {
            REQUIRE(State::DELIMITER == static_cast<State>(reassembler.state));
            const auto   byte       = header_enc[0];
            std::size_t  inout_size = 1U;
            const int8_t ret        = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
            REQUIRE(0U == ret);
        }

        // feed in the second byte of the header - the state machine
        // should transition to latch the header
        {
            REQUIRE(State::DELIMITER == static_cast<State>(reassembler.state));
            const auto   byte       = header_enc[1];
            std::size_t  inout_size = 1U;
            const int8_t ret        = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
            REQUIRE(0U == ret);

            REQUIRE(State::HEADER == static_cast<State>(reassembler.state));
            REQUIRE(1U == reassembler.counter);
            REQUIRE(byte == reassembler.header[0]);
        }

        // feed in the header (exept last byte)
        for (std::size_t i = 2; i < header_enc.size() - 1; i++)
        {
            const auto   byte       = header_enc[i];
            std::size_t  inout_size = 1U;
            const int8_t ret        = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
            REQUIRE(0U == ret);

            REQUIRE(State::HEADER == static_cast<State>(reassembler.state));
        }

        // feed in the last byte of the header - the state machine
        // should validate and reject the header (since we aren't subscribed)
        {
            REQUIRE(State::HEADER == static_cast<State>(reassembler.state));
            const auto   byte       = header_enc[24];
            std::size_t  inout_size = 1U;
            const int8_t ret        = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
            REQUIRE(0U == ret);

            // this time, the reassembler counter will be reset
            // to count payload bytes
            REQUIRE(0U == reassembler.counter);

            // the API is not required to preserve the header on rejected transfers
            // (or in general) but this implementation does, so verify that it
            // correctly decoded and parsed the header
            REQUIRE(State::PAYLOAD == static_cast<State>(reassembler.state));
            for (std::size_t i = 0; i < 24U; i++)
            {
                INFO(i);
                INFO((int) header_raw[i]);
                INFO((int) reassembler.header[i]);
                REQUIRE(header_raw[i] == reassembler.header[i]);
            }
        }

        // keep feeding a mock payload - the state machine should
        // continue to reject it
        const std::array<std::uint8_t, 13> payload = {
            0x30,
            0x31,
            0x32,
            0x33,
            0x34,
            0x35,
            0x36,
            0x37,
            0x38,
            0xd2,
            0xee,
            0x56,
            0xc8,
        };

        for (const auto byte : payload)
        {
            std::size_t  inout_size = 1U;
            const int8_t ret        = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
            REQUIRE(0U == ret);
            REQUIRE(State::PAYLOAD == static_cast<State>(reassembler.state));
        }

        // feed in a delimiter, the state should transition
        const std::uint8_t byte       = exposed::COBS_FRAME_DELIMITER;
        std::size_t        inout_size = 1U;
        const int8_t       ret        = serardRxAccept(&serard, &reassembler, 0, &inout_size, &byte, 0, &out, &out_sub);
        REQUIRE(1U == ret);
        REQUIRE(State::DELIMITER == static_cast<State>(reassembler.state));

        REQUIRE(0U == inout_size);
        REQUIRE(out_sub == &sub);
        REQUIRE(payload.size() == reassembler.counter);
        REQUIRE((payload.size() - exposed::TRANSFER_CRC_SIZE_BYTES) == out.payload_size);
        for (std::size_t i = 0; i < payload.size(); i++)
        {
            INFO(i);
            REQUIRE(payload[i] == static_cast<const std::uint8_t*>(out.payload)[i]);
        }
    }

    // TODO: make all of these reordered constant first
}

TEST_CASE("rxInitTransferMetaDataFromModel")
{
    exposed::RxTransferModel model{

    };
}

TEST_CASE("serardRxAccept")
{
    // TODO: test that invalid messages are discarded
    // TODO: whitebox testing of RX state machine
    // struct SerardMemoryResource allocator = {
    //     .user_reference = nullptr,
    //     .deallocate     = &serardFree,
    //     .allocate       = &serardAlloc,
    // };
    // struct Serard serard = serardInit(allocator, allocator);
    // serard.node_id       = 4321;
    //
    // SerardRxSubscription sub{};
    // serardRxSubscribe(&serard, SerardTransferKindMessage, 1234, 0, 1000, &sub);
    //
    // std::array<std::uint8_t, 31> buffer = {0x00, 0x0d, 0x01, 0x06, 0xe1, 0x10, 0xd2, 0x04, 0xff, 0xc1, 0xba,
    //                                        0xb0, 0xfe, 0xca, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x80,
    //                                        0x01, 0x03, 0x6a, 0xc6, 0x01, 0x01, 0x01, 0x01, 0x00};
    // SerardReassembler            reassembler{};
    // size_t                       payload_size = buffer.size();
    // SerardRxTransfer             out{};
    // SerardRxSubscription*        out_sub = nullptr;
    // const int8_t ret = serardRxAccept(&serard, &reassembler, 0, &payload_size, buffer.data(), &out, &out_sub);
    // // TODO: make all of these reordered constant first
    // REQUIRE(ret == 1);
    // struct SerardTransferMetadata metadata = {
    //     .priority       = SerardPriorityNominal,
    //     .transfer_kind  = SerardTransferKindMessage,
    //     .port_id        = 1234,
    //     .remote_node_id = SERARD_NODE_ID_UNSET,
    //     .transfer_id    = 0,
    // };
    //
    // buffer_t    result_buffer;
    // auto* const user_reference = reinterpret_cast<void*>(&result_buffer);
    // serardTxPush(&serard, &metadata, 0, nullptr, user_reference, &serardEmitter);
    // for (unsigned char& it : result_buffer)
    // {
    //     printf("%02x ", it);
    // }
    // printf("\n");
    //
    // struct SerardRxSubscription sub
    // {};
    // serardRxSubscribe(&serard, SerardTransferKindMessage, 1234, 0, 1000, &sub);
    // struct SerardReassembler reassembler
    // {};
    // size_t payload_size = result_buffer.size();
    // struct SerardRxTransfer out
    // {};
    // struct SerardRxSubscription* out_sub = nullptr;
    // const int8_t ret = serardRxAccept(&serard, &reassembler, 0, &payload_size, result_buffer.data(), &out,
    // &out_sub); printf("%d\n", ret); REQUIRE(ret == 1); REQUIRE(out_sub == &sub);
}
