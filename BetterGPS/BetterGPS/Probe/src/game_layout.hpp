#pragma once

#include <cstddef>
#include <cstdint>

namespace bettergps::layout
{
    // ETS2LA/plugin 1.61 Windows layouts.
    // POD mirrors. Class identification separately invokes get_unit_descriptor.

#pragma pack(push, 1)
    struct NodeItemWin
    {
        std::int32_t x, y, z;
        std::uint32_t flags;
        float qw, qx, qy, qz;
        void* backward;
        void* forward;
        std::uint64_t uid;
    };
    static_assert(sizeof(NodeItemWin) == 0x38);
    struct ArrayDynWin
    {
        void* vtable;          // 0x00 (array_t virtual dtor)
        void* value;           // 0x08
        std::uint64_t size;    // 0x10
        std::uint64_t capacity;// 0x18
        void* allocator;       // 0x20
    };
    static_assert(sizeof(ArrayDynWin) == 0x28);

    struct ArrayLocalHeaderWin
    {
        void* vtable;          // 0x00
        void* value;           // 0x08
        std::uint64_t size;    // 0x10
        std::uint64_t capacity;// 0x18
    };
    static_assert(sizeof(ArrayLocalHeaderWin) == 0x20);

    struct PhysicalRouteItemWin
    {
        void* node;                         // 0x00
        std::uint8_t pad_0008[4];          // 0x08
        float distance_from_previous;       // 0x0C, meters
        float time_from_previous;           // 0x10, seconds
        float total_distance_till_end;      // 0x14, meters
        float total_time_till_end;          // 0x18, seconds
        std::uint8_t pad_001C[4];          // 0x1C
    };
    static_assert(sizeof(PhysicalRouteItemWin) == 0x20);

    struct RouteItemWin
    {
        std::uint32_t type;                 // 0x00: 0=node, 1=prefab
        std::uint32_t node_id;              // 0x04
        void* item_ptr;                     // 0x08
        std::uint8_t pad_0010[8];           // 0x10
        float distance_from_previous;       // 0x18, meters
        float time_from_previous;           // 0x1C, seconds
        std::uint16_t unknown_20;           // 0x20
        std::uint16_t unknown_22;           // 0x22
        float total_distance_till_end;      // 0x24, meters
        float total_time_till_end;          // 0x28, seconds
        std::uint8_t pad_002C[4];           // 0x2C
    };
    static_assert(sizeof(RouteItemWin) == 0x30);
#pragma pack(pop)

    // gps_manager_t (Windows, ETS2LA/plugin 1.61)
    constexpr std::size_t GPS_SIMPLE_ROUTE_SOURCE = 0x008;
    constexpr std::size_t SIMPLE_ROUTE_TASK = 0x020;
    constexpr std::size_t GPS_ROUTE_TASK = GPS_SIMPLE_ROUTE_SOURCE + SIMPLE_ROUTE_TASK; // 0x028

    constexpr std::size_t GPS_TRIP_DISTANCE = 0x274;
    constexpr std::size_t GPS_TRIP_TIME = 0x278;
    constexpr std::size_t GPS_NEXT_CHECKPOINT_DISTANCE = 0x27C;
    constexpr std::size_t GPS_NEXT_CHECKPOINT_TIME = 0x280;
    constexpr std::size_t GPS_VOICE_EVENTS = 0x480;

    // route_task_t (Windows, ETS2LA/plugin 1.61)
    constexpr std::size_t ROUTE_TASK_PHYSICAL_ITEMS = 0x050;
    constexpr std::size_t ROUTE_TASK_ROUTE_ITEMS = 0x078;

    // Item UIDs
    constexpr std::size_t NODE_ITEM_UID = 0x030;
    constexpr std::size_t KDOP_ITEM_UID = 0x040; // prefab_item_t derives from kdop item at offset 0

    constexpr std::size_t GPS_MANAGER_SIZE = 0x5C8;
    constexpr std::size_t ROUTE_TASK_SIZE = 0x160;
}
