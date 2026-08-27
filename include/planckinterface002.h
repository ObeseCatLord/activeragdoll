#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "skse64/PluginAPI.h"

namespace PlanckPluginAPI {
    // Version 2 is intentionally data-only: requests may originate on arbitrary threads.
    // Coordinates and velocities use Skyrim world units; nodeName must contain a NUL within 64 bytes.
    // Callers set every input size and out-result size to sizeof(their structure); larger future sizes are accepted.
    constexpr std::uint32_t kPlanckInterface002Revision = 2;
    constexpr std::uint32_t kPlanckInterface002NodeNameCapacity = 64;

    enum PlanckFeature002 : std::uint64_t {
        kPlanckFeature002_RemoteHitImpulse = 1ull << 0,
        kPlanckFeature002_RemoteRagdoll = 1ull << 1,
        // This is a bounded impulse target drive, not a HIGGS hand/proxy constraint.
        kPlanckFeature002_RemoteGripImpulseDrive = 1ull << 2,
        kPlanckFeature002_LocalPhysicalEvents = 1ull << 3,
        // Atomically drops locally captured events before a client lifecycle
        // generation is admitted for transmission.
        kPlanckFeature002_LocalEventRebase = 1ull << 4,
        // Appended Interface002 telemetry surface.  Consumers must negotiate
        // this bit before calling DequeueRemoteCompletionReceipt.
        kPlanckFeature002_RemoteCompletionReceipts = 1ull << 5,
    };

    // Stable result contract: existing values never change or reorder.
    // AllocationFailure is appended so older consumers that only compare
    // Accepted/Empty continue to treat it as a rejection.
    enum class PlanckResultCode002 : std::uint32_t {
        Accepted = 0,
        Empty,
        InvalidRequest,
        Duplicate,
        QueueFull,
        Unsupported,
        AllocationFailure,
    };

    struct PlanckVector3_002 { float x, y, z; };
    struct PlanckQuaternion_002 { float x, y, z, w; };

    struct PlanckRequestHeader002 {
        std::uint32_t size;
        std::uint32_t reserved;
        std::uint64_t sourceSession;
        std::uint64_t eventId;
    };

    struct PlanckRemoteHitImpulseRequest002 {
        PlanckRequestHeader002 header;
        std::uint32_t targetFormId;
        float impulseMultiplier;
        char nodeName[kPlanckInterface002NodeNameCapacity];
        PlanckVector3_002 position;
        PlanckVector3_002 velocity;
    };

    struct PlanckRemoteRagdollRequest002 {
        PlanckRequestHeader002 header;
        std::uint32_t targetFormId;
        std::uint32_t reserved;
        PlanckVector3_002 sourcePosition;
    };

    struct PlanckRemoteRagdollExitRequest002 {
        PlanckRequestHeader002 header;
        std::uint32_t targetFormId;
        std::uint32_t reserved;
    };

    struct PlanckRemoteGripState002 {
        PlanckVector3_002 worldPosition;
        PlanckQuaternion_002 worldRotation;
        PlanckVector3_002 linearVelocity;
        PlanckVector3_002 angularVelocity;
        float ttlSeconds;
        std::uint32_t reserved;
    };

    struct PlanckBeginRemoteGripRequest002 {
        PlanckRequestHeader002 header;
        std::uint64_t gripId;
        std::uint32_t targetFormId;
        char nodeName[kPlanckInterface002NodeNameCapacity];
        PlanckRemoteGripState002 state;
    };

    struct PlanckUpdateRemoteGripRequest002 {
        PlanckRequestHeader002 header;
        std::uint64_t gripId;
        std::uint32_t targetFormId;
        std::uint32_t reserved;
        PlanckRemoteGripState002 state;
    };

    struct PlanckEndRemoteGripRequest002 {
        PlanckRequestHeader002 header;
        std::uint64_t gripId;
        std::uint32_t targetFormId;
        std::uint32_t reserved;
    };

    struct PlanckClearRemoteSessionRequest002 {
        PlanckRequestHeader002 header;
    };

    struct PlanckDiscardLocalPhysicalEventsRequest002 {
        std::uint32_t size;
        std::uint32_t reserved;
        std::uint64_t lifecycleGeneration;
    };

    struct PlanckCapabilitiesRequest002 { std::uint32_t size; std::uint32_t reserved; };
    struct PlanckCapabilitiesResult002 {
        std::uint32_t size;
        std::uint32_t interfaceRevision;
        std::uint64_t featureBits;
        std::uint32_t maxPendingCommands;
        std::uint32_t maxLocalEvents;
    };

    struct PlanckDequeueLocalPhysicalEventRequest002 { std::uint32_t size; std::uint32_t reserved; };
    struct PlanckDequeueRemoteCompletionReceiptRequest002 { std::uint32_t size; std::uint32_t reserved; };

    enum class PlanckRemoteCompletionKind002 : std::uint8_t {
        HitImpulse = 1, Ragdoll, RagdollExit, GripBegin, GripUpdate, GripEnd, GripDrive,
    };

    // A receipt is emitted only after game/physics-thread processing.  A
    // GripBeginAdmitted receipt means controller admission, never a physics
    // drive; the first successful drive emits GripDriveApplied separately.
    enum class PlanckRemoteCompletionStatus002 : std::uint8_t {
        Applied = 1,
        Cancelled,
        TargetMissing,
        RootUnavailable,
        WorldUnavailable,
        NodeUnavailable,
        RigidBodyUnavailable,
        InvalidState,
        LeaseInvalid,
        GripBeginAdmitted,
        GripDriveApplied,
    };

    struct PlanckRemoteCompletionReceipt002 {
        std::uint32_t size;
        PlanckRemoteCompletionKind002 kind;
        PlanckRemoteCompletionStatus002 status;
        std::uint16_t reserved;
        std::uint64_t sourceSession;
        std::uint64_t eventId;
        std::uint64_t sequence;
        std::uint64_t gripId;
        std::uint32_t targetFormId;
        std::uint32_t reserved2;
    };

    // The original hit fields remain the prefix. New fields are appended so an
    // older consumer can reject by size without misreading the hit payload.
    enum class PlanckLocalPhysicalEventKind002 : std::uint8_t {
        HitImpulse = 1,
        RagdollEnter,
        RagdollExit,
        GripBegin,
        GripUpdate,
        GripEnd,
    };

    struct PlanckLocalPhysicalEvent002 {
        std::uint32_t size;
        std::uint32_t targetFormId;
        std::uint64_t eventId;
        PlanckVector3_002 position;
        PlanckVector3_002 velocity;
        std::uint32_t flags;
        char nodeName[kPlanckInterface002NodeNameCapacity];
        PlanckLocalPhysicalEventKind002 kind;
        std::uint8_t reserved[3];
        std::uint64_t gripId;
        PlanckQuaternion_002 rotation;
        PlanckVector3_002 linearVelocity;
        PlanckVector3_002 angularVelocity;
        PlanckVector3_002 sourcePosition;
        float impulseMultiplier;
        float ttlSeconds;
    };

    struct PlanckResult002 {
        std::uint32_t size;
        PlanckResultCode002 code;
        std::uint64_t sequence;
    };

    static_assert(std::is_trivially_copyable<PlanckRequestHeader002>::value);
    static_assert(std::is_trivially_copyable<PlanckRemoteHitImpulseRequest002>::value);
    static_assert(std::is_trivially_copyable<PlanckRemoteRagdollRequest002>::value);
    static_assert(std::is_trivially_copyable<PlanckRemoteRagdollExitRequest002>::value);
    static_assert(std::is_trivially_copyable<PlanckBeginRemoteGripRequest002>::value);
    static_assert(std::is_trivially_copyable<PlanckUpdateRemoteGripRequest002>::value);
    static_assert(std::is_trivially_copyable<PlanckEndRemoteGripRequest002>::value);
    static_assert(std::is_trivially_copyable<PlanckClearRemoteSessionRequest002>::value);
    static_assert(std::is_trivially_copyable<PlanckDiscardLocalPhysicalEventsRequest002>::value);
    static_assert(std::is_trivially_copyable<PlanckCapabilitiesRequest002>::value);
    static_assert(std::is_trivially_copyable<PlanckCapabilitiesResult002>::value);
    static_assert(std::is_trivially_copyable<PlanckDequeueLocalPhysicalEventRequest002>::value);
    static_assert(std::is_trivially_copyable<PlanckDequeueRemoteCompletionReceiptRequest002>::value);
    static_assert(std::is_trivially_copyable<PlanckRemoteCompletionReceipt002>::value);
    static_assert(std::is_standard_layout<PlanckRemoteCompletionReceipt002>::value);
    static_assert(std::is_trivially_copyable<PlanckLocalPhysicalEvent002>::value);
    static_assert(std::is_standard_layout<PlanckLocalPhysicalEvent002>::value);
    static_assert(sizeof(PlanckLocalPhysicalEvent002) == 184);
    static_assert(offsetof(PlanckLocalPhysicalEvent002, eventId) == 0x08);
    static_assert(offsetof(PlanckLocalPhysicalEvent002, nodeName) == 0x2C);
    static_assert(offsetof(PlanckLocalPhysicalEvent002, kind) == 0x6C);
    static_assert(offsetof(PlanckLocalPhysicalEvent002, gripId) == 0x70);
    static_assert(offsetof(PlanckLocalPhysicalEvent002, rotation) == 0x78);
    static_assert(offsetof(PlanckLocalPhysicalEvent002, sourcePosition) == 0xA0);
    static_assert(offsetof(PlanckLocalPhysicalEvent002, ttlSeconds) == 0xB0);
    static_assert(std::is_trivially_copyable<PlanckResult002>::value);
    static_assert(sizeof(PlanckRequestHeader002) == 0x18);
    static_assert(sizeof(PlanckRemoteHitImpulseRequest002) == 0x78);
    static_assert(sizeof(PlanckRemoteRagdollRequest002) == 0x30);
    static_assert(sizeof(PlanckRemoteRagdollExitRequest002) == 0x20);
    static_assert(sizeof(PlanckRemoteGripState002) == 0x3C);
    static_assert(sizeof(PlanckBeginRemoteGripRequest002) == 0xA0);
    static_assert(sizeof(PlanckUpdateRemoteGripRequest002) == 0x68);
    static_assert(sizeof(PlanckEndRemoteGripRequest002) == 0x28);
    static_assert(sizeof(PlanckClearRemoteSessionRequest002) == 0x18);
    static_assert(sizeof(PlanckDiscardLocalPhysicalEventsRequest002) == 0x10);
    static_assert(sizeof(PlanckCapabilitiesRequest002) == 0x08);
    static_assert(sizeof(PlanckCapabilitiesResult002) == 0x18);
    static_assert(sizeof(PlanckDequeueLocalPhysicalEventRequest002) == 0x08);
    static_assert(sizeof(PlanckDequeueRemoteCompletionReceiptRequest002) == 0x08);
    static_assert(sizeof(PlanckRemoteCompletionReceipt002) == 0x30);
    static_assert(sizeof(PlanckResult002) == 0x10);

    // noexcept is part of the method contract but not of the MSVC name
    // mangling or vtable layout. This data-only interface has exactly one
    // implementer (g_interface002 in activeragdoll.dll); the standalone
    // bridge declares its own mirror struct and binds by vtable slot, so
    // adding noexcept does not change the exported ABI.
    struct IPlanckInterface002 {
        virtual PlanckResult002 GetCapabilities(const PlanckCapabilitiesRequest002 &request, PlanckCapabilitiesResult002 &result) noexcept = 0;
        virtual PlanckResult002 SubmitRemoteHitImpulse(const PlanckRemoteHitImpulseRequest002 &request) noexcept = 0;
        virtual PlanckResult002 SubmitRemoteRagdoll(const PlanckRemoteRagdollRequest002 &request) noexcept = 0;
        virtual PlanckResult002 SubmitRemoteRagdollExit(const PlanckRemoteRagdollExitRequest002 &request) noexcept = 0;
        virtual PlanckResult002 BeginRemoteGrip(const PlanckBeginRemoteGripRequest002 &request) noexcept = 0;
        virtual PlanckResult002 UpdateRemoteGrip(const PlanckUpdateRemoteGripRequest002 &request) noexcept = 0;
        virtual PlanckResult002 EndRemoteGrip(const PlanckEndRemoteGripRequest002 &request) noexcept = 0;
        virtual PlanckResult002 ClearRemoteSession(const PlanckClearRemoteSessionRequest002 &request) noexcept = 0;
        virtual PlanckResult002 DequeueLocalPhysicalEvent(const PlanckDequeueLocalPhysicalEventRequest002 &request, PlanckLocalPhysicalEvent002 &result) noexcept = 0;
        // Appended to preserve the interface002 prefix vtable layout used by
        // existing consumers. New consumers require the feature bit above.
        virtual PlanckResult002 DiscardLocalPhysicalEvents(const PlanckDiscardLocalPhysicalEventsRequest002 &request) noexcept = 0;
        // Appended after all existing Interface002 slots.  Call only when
        // kPlanckFeature002_RemoteCompletionReceipts was advertised.
        virtual PlanckResult002 DequeueRemoteCompletionReceipt(const PlanckDequeueRemoteCompletionReceiptRequest002 &request, PlanckRemoteCompletionReceipt002 &result) noexcept = 0;
    };
    static_assert(std::is_abstract<IPlanckInterface002>::value);
    static_assert(std::is_same<decltype(&IPlanckInterface002::GetCapabilities),
        PlanckResult002 (IPlanckInterface002::*)(const PlanckCapabilitiesRequest002 &, PlanckCapabilitiesResult002 &) noexcept>::value);
    static_assert(std::is_same<decltype(&IPlanckInterface002::SubmitRemoteHitImpulse),
        PlanckResult002 (IPlanckInterface002::*)(const PlanckRemoteHitImpulseRequest002 &) noexcept>::value);
    static_assert(std::is_same<decltype(&IPlanckInterface002::SubmitRemoteRagdoll),
        PlanckResult002 (IPlanckInterface002::*)(const PlanckRemoteRagdollRequest002 &) noexcept>::value);
    static_assert(std::is_same<decltype(&IPlanckInterface002::SubmitRemoteRagdollExit),
        PlanckResult002 (IPlanckInterface002::*)(const PlanckRemoteRagdollExitRequest002 &) noexcept>::value);
    static_assert(std::is_same<decltype(&IPlanckInterface002::BeginRemoteGrip),
        PlanckResult002 (IPlanckInterface002::*)(const PlanckBeginRemoteGripRequest002 &) noexcept>::value);
    static_assert(std::is_same<decltype(&IPlanckInterface002::UpdateRemoteGrip),
        PlanckResult002 (IPlanckInterface002::*)(const PlanckUpdateRemoteGripRequest002 &) noexcept>::value);
    static_assert(std::is_same<decltype(&IPlanckInterface002::EndRemoteGrip),
        PlanckResult002 (IPlanckInterface002::*)(const PlanckEndRemoteGripRequest002 &) noexcept>::value);
    static_assert(std::is_same<decltype(&IPlanckInterface002::ClearRemoteSession),
        PlanckResult002 (IPlanckInterface002::*)(const PlanckClearRemoteSessionRequest002 &) noexcept>::value);
    static_assert(std::is_same<decltype(&IPlanckInterface002::DequeueLocalPhysicalEvent),
        PlanckResult002 (IPlanckInterface002::*)(const PlanckDequeueLocalPhysicalEventRequest002 &, PlanckLocalPhysicalEvent002 &) noexcept>::value);
    static_assert(std::is_same<decltype(&IPlanckInterface002::DiscardLocalPhysicalEvents),
        PlanckResult002 (IPlanckInterface002::*)(const PlanckDiscardLocalPhysicalEventsRequest002 &) noexcept>::value);
    static_assert(std::is_same<decltype(&IPlanckInterface002::DequeueRemoteCompletionReceipt),
        PlanckResult002 (IPlanckInterface002::*)(const PlanckDequeueRemoteCompletionReceiptRequest002 &, PlanckRemoteCompletionReceipt002 &) noexcept>::value);

    // Header-only client helper so consumers do not need PLANCK implementation objects or headers.
    inline IPlanckInterface002 *GetPlanckInterface002(const PluginHandle &pluginHandle, SKSEMessagingInterface *messagingInterface)
    {
        struct Message { void *(*getApi)(unsigned int) = nullptr; };
        static IPlanckInterface002 *interface002 = nullptr;
        if (interface002) return interface002;
        Message message;
        messagingInterface->Dispatch(pluginHandle, 0x92F38745, &message, sizeof(Message *), "PLANCK");
        interface002 = static_cast<IPlanckInterface002 *>(message.getApi ? message.getApi(kPlanckInterface002Revision) : nullptr);
        return interface002;
    }
}
