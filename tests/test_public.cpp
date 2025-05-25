// This software is distributed under the terms of the MIT License.
// Copyright (c) 2022 OpenCyphal

#include <catch.hpp>
#include "exposed.hpp"
#include "serard.h"

static void* serardAlloc(void* const user_reference, const size_t size)
{
    (void) user_reference;
    return malloc(size);
}

static void serardFree(void* const user_reference, const size_t size, void* const pointer)
{
    (void) user_reference;
    (void) size;
    free(pointer);
}

using buffer_t = std::vector<std::uint8_t>;

static bool serardEmitter(void* const user_reference, uint8_t data_size, const uint8_t* data)
{
    REQUIRE(data_size > 0);
    REQUIRE(data != NULL);

    auto* const buffer = reinterpret_cast<buffer_t*>(user_reference);
    buffer->insert(buffer->end(), data, data + data_size);

    return true;
}

TEST_CASE("serardTxPush")
{
    {
        const SerardNodeID node_id = 4321;

        // TODO: test rejection of illegal port ids
        struct SerardTransferMetadata metadata = {
            .priority       = SerardPrioritySlow,
            .transfer_kind  = SerardTransferKindRequest,
            .port_id        = 511,
            .remote_node_id = 1234,
            .transfer_id    = 0xCAFEB0BAUL,
        };

        buffer_t    result_buffer;
        auto* const user_reference = reinterpret_cast<void*>(&result_buffer);
        const auto  ret            = serardTxPush(node_id, &metadata, 0, nullptr, user_reference, &serardEmitter);
        REQUIRE(ret > 0);

        std::array<std::uint8_t, 31> expected = {0x00, 0x0d, 0x01, 0x06, 0xe1, 0x10, 0xd2, 0x04, 0xff, 0xc1, 0xba,
                                                 0xb0, 0xfe, 0xca, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x80,
                                                 0x01, 0x03, 0x6a, 0xc6, 0x01, 0x01, 0x01, 0x01, 0x00};
        REQUIRE(result_buffer.size() == expected.size());
        for (std::size_t i = 0; i < result_buffer.size(); i++)
        {
            REQUIRE(result_buffer[i] == expected[i]);
        }
    }

    {
        const SerardNodeID node_id = 1234;

        struct SerardTransferMetadata metadata = {
            .priority       = SerardPriorityNominal,
            .transfer_kind  = SerardTransferKindMessage,
            .port_id        = 1234,
            .remote_node_id = SERARD_NODE_ID_UNSET,
            .transfer_id    = 0,
        };

        buffer_t    result_buffer;
        auto* const user_reference = reinterpret_cast<void*>(&result_buffer);
        // uavcan.primitive.String.1 containing string “012345678”
        std::array<std::uint8_t, 9> payload = {'0', '1', '2', '3', '4', '5', '6', '7', '8'};
        serardTxPush(node_id, &metadata, payload.size(), payload.data(), user_reference, &serardEmitter);

        std::array<std::uint8_t, 40> expected = {0x00, 0x09, 0x01, 0x04, 0xd2, 0x04, 0xff, 0xff, 0xd2, 0x04,
                                                 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                                                 0x02, 0x80, 0x01, 0x10, 0x08, 0x12, 0x30, 0x31, 0x32, 0x33,
                                                 0x34, 0x35, 0x36, 0x37, 0x38, 0xd2, 0xee, 0x56, 0xc8, 0x00};
        REQUIRE(result_buffer.size() == expected.size());
        for (std::size_t i = 0; i < result_buffer.size(); i++)
        {
            REQUIRE(result_buffer[i] == expected[i]);
        }
    }
}

TEST_CASE("serardRxAccept")
{
    // TODO: test that invalid messages are discarded
    // TODO: whitebox testing of RX state machine
    struct SerardMemoryResource allocator = {
        .user_reference = nullptr,
        .allocate       = &serardAlloc,
        .deallocate     = &serardFree,
    };

    struct SerardRx ins = serardRxInit(allocator, allocator);
    ins.node_id         = 4321;

    SerardRxSubscription subscription = {};
    serardRxSubscribe(&ins, SerardTransferKindMessage, 1234, 100, 1000, &subscription);

    SerardReassembler reassembler = serardReassemblerInit();
    // const std::array<std::uint8_t, 24> buffer      = {0x01, 0x04, 0xD2, 0x04, 0xFF, 0xFF, 0xD2, 0x04,
    //                                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //                                                   0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x08, 0x12};
    const std::array<std::uint8_t, 40> buffer = {0x00, 0x09, 0x01, 0x04, 0xd2, 0x04, 0xff, 0xff, 0xd2, 0x04,
                                                 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                                                 0x02, 0x80, 0x01, 0x10, 0x08, 0x12, 0x30, 0x31, 0x32, 0x33,
                                                 0x34, 0x35, 0x36, 0x37, 0x38, 0xd2, 0xee, 0x56, 0xc8, 0x00};

    size_t                payload_size = buffer.size();
    SerardRxTransfer      out          = {};
    SerardRxSubscription* out_sub      = nullptr;
    const int8_t          ret = serardRxAccept(&ins, &reassembler, 0, &payload_size, buffer.data(), 0, &out, &out_sub);
    REQUIRE(ret == 1);

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
    // serardTxPush(ins.node_id, &metadata, 0, nullptr, user_reference, &serardEmitter);
    // for (unsigned char& it : result_buffer)
    // {
    //     printf("%02x ", it);
    // }
    // printf("\n");

    // struct SerardRxSubscription sub
    // {};
    // serardRxSubscribe(&serard, SerardTransferKindMessage, 1234, 0, 1000, &sub);
    // struct SerardReassembler reassembler
    // {};
    // size_t payload_size = result_buffer.size();
}
