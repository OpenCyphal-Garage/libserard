// This software is distributed under the terms of the MIT License.
// Copyright (c) 2022 OpenCyphal

#include <catch.hpp>
#include "exposed.hpp"
#include "serard.h"

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

using buffer_t = std::vector<std::uint8_t>;

bool serardEmitter(void* const user_reference, uint8_t data_size, const uint8_t* data)
{
    REQUIRE(data_size > 0);
    REQUIRE(data != NULL);

    auto* const buffer = reinterpret_cast<buffer_t*>(user_reference);
    buffer->insert(buffer->end(), data, data + data_size);

    return true;
}

TEST_CASE("serardTxPush")
{
    struct SerardMemoryResource allocator = {
        .user_reference = nullptr,
        .allocate       = &serardAlloc,
        .deallocate     = &serardFree,
    };

    {
        struct Serard serard = serardInit(allocator, allocator);
        serard.node_id       = 4321;

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
        const auto  ret            = serardTxPush(&serard, &metadata, 0, nullptr, user_reference, &serardEmitter);
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
        struct Serard serard = serardInit(allocator, allocator);
        serard.node_id       = 1234;

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
        serardTxPush(&serard, &metadata, payload.size(), payload.data(), user_reference, &serardEmitter);

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
