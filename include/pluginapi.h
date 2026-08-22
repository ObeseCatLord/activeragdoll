#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <vector>

#include "skse64/PluginAPI.h"
#include "skse64/GameReferences.h"

#include "planckinterface001.h"
#include "planckinterface002.h"


namespace PlanckPluginAPI {
    // Keep the producer-side request layouts pinned independently of the
    // standalone bridge consumer.  These assertions cover every request
    // consumed through Interface002, not just the local-event suffix.
    static_assert(offsetof(PlanckRequestHeader002, size) == 0x00);
    static_assert(offsetof(PlanckRequestHeader002, sourceSession) == 0x08);
    static_assert(offsetof(PlanckRequestHeader002, eventId) == 0x10);
    static_assert(offsetof(PlanckRemoteHitImpulseRequest002, targetFormId) == 0x18);
    static_assert(offsetof(PlanckRemoteHitImpulseRequest002, impulseMultiplier) == 0x1C);
    static_assert(offsetof(PlanckRemoteHitImpulseRequest002, nodeName) == 0x20);
    static_assert(offsetof(PlanckRemoteHitImpulseRequest002, position) == 0x60);
    static_assert(offsetof(PlanckRemoteHitImpulseRequest002, velocity) == 0x6C);
    static_assert(offsetof(PlanckRemoteRagdollRequest002, targetFormId) == 0x18);
    static_assert(offsetof(PlanckRemoteRagdollRequest002, reserved) == 0x1C);
    static_assert(offsetof(PlanckRemoteRagdollRequest002, sourcePosition) == 0x20);
    static_assert(offsetof(PlanckRemoteRagdollExitRequest002, targetFormId) == 0x18);
    static_assert(offsetof(PlanckRemoteRagdollExitRequest002, reserved) == 0x1C);
    static_assert(offsetof(PlanckBeginRemoteGripRequest002, gripId) == 0x18);
    static_assert(offsetof(PlanckBeginRemoteGripRequest002, targetFormId) == 0x20);
    static_assert(offsetof(PlanckBeginRemoteGripRequest002, nodeName) == 0x24);
    static_assert(offsetof(PlanckBeginRemoteGripRequest002, state) == 0x64);
    static_assert(offsetof(PlanckUpdateRemoteGripRequest002, gripId) == 0x18);
    static_assert(offsetof(PlanckUpdateRemoteGripRequest002, targetFormId) == 0x20);
    static_assert(offsetof(PlanckUpdateRemoteGripRequest002, reserved) == 0x24);
    static_assert(offsetof(PlanckUpdateRemoteGripRequest002, state) == 0x28);
    static_assert(offsetof(PlanckEndRemoteGripRequest002, gripId) == 0x18);
    static_assert(offsetof(PlanckEndRemoteGripRequest002, targetFormId) == 0x20);
    static_assert(offsetof(PlanckEndRemoteGripRequest002, reserved) == 0x24);
    static_assert(offsetof(PlanckClearRemoteSessionRequest002, header) == 0x00);
    static_assert(offsetof(PlanckDiscardLocalPhysicalEventsRequest002, lifecycleGeneration) == 0x08);
    // Handles skse mod messages requesting to fetch API functions from PLANCK
    void ModMessageHandler(SKSEMessagingInterface::Message *message);

    // This object provides access to PLANCK's mod support API version 1
    struct PlanckInterface001 : IPlanckInterface001
    {
        virtual unsigned int GetBuildNumber();

        virtual bool Deprecated1(const std::string_view & name, double &out);
        virtual bool Deprecated2(const std::string & name, double val);

        virtual bool GetSettingDouble(const char *name, double &out);
        virtual bool SetSettingDouble(const char *name, double val);

        virtual void AddIgnoredActor(Actor *actor);
        virtual void RemoveIgnoredActor(Actor *actor);

        virtual void AddAggressionIgnoredActor(Actor *actor);
        virtual void RemoveAggressionIgnoredActor(Actor *actor);

        virtual void SetAggressionLowTopic(Actor *actor, TESTopic *topic);
        virtual void SetAggressionHighTopic(Actor *actor, TESTopic *topic);

        virtual void AddRagdollCollisionIgnoredActor(Actor *actor);
        virtual void RemoveRagdollCollisionIgnoredActor(Actor *actor);

        virtual PlanckHitData GetLastHitData();
        virtual TESHitEvent *GetCurrentHitEvent();

        bool IsRagdollCollisionIgnored(TESObjectREFR *actor);

        PlanckHitData lastHitData;
        TESHitEvent *currentHitEvent = nullptr;

        std::mutex ignoredActorsLock;
        std::unordered_set<Actor *> ignoredActors;

        std::mutex aggressionIgnoredActorsLock;
        std::unordered_set<Actor *> aggressionIgnoredActors;

        std::mutex aggressionTopicsLock;
        std::unordered_map<Actor *, TESTopic *> lowAggressionTopics;
        std::unordered_map<Actor *, TESTopic *> highAggressionTopics;

        std::mutex ragdollCollisionIgnoredActorsLock;
        std::unordered_set<TESObjectREFR *> ragdollCollisionIgnoredActors;
    };

    enum class RemoteCommandType002 : UInt8 { HitImpulse, Ragdoll, RagdollExit, BeginGrip, UpdateGrip, EndGrip };
    struct RemoteCommand002 {
        RemoteCommandType002 type;
        union {
            PlanckRemoteHitImpulseRequest002 hitImpulse;
            PlanckRemoteRagdollRequest002 ragdoll;
            PlanckRemoteRagdollExitRequest002 ragdollExit;
            PlanckBeginRemoteGripRequest002 beginGrip;
            PlanckUpdateRemoteGripRequest002 updateGrip;
            PlanckEndRemoteGripRequest002 endGrip;
        } request;

        [[nodiscard]] UInt64 SourceSession() const
        {
            switch (type) {
            case RemoteCommandType002::HitImpulse: return request.hitImpulse.header.sourceSession;
            case RemoteCommandType002::Ragdoll: return request.ragdoll.header.sourceSession;
            case RemoteCommandType002::RagdollExit: return request.ragdollExit.header.sourceSession;
            case RemoteCommandType002::BeginGrip: return request.beginGrip.header.sourceSession;
            case RemoteCommandType002::UpdateGrip: return request.updateGrip.header.sourceSession;
            case RemoteCommandType002::EndGrip: return request.endGrip.header.sourceSession;
            }
            return 0;
        }
    };
    static_assert(std::is_trivially_copyable<RemoteCommand002>::value);

    struct PlanckInterface002 : IPlanckInterface002
    {
        PlanckResult002 GetCapabilities(const PlanckCapabilitiesRequest002 &request, PlanckCapabilitiesResult002 &result) noexcept override;
        PlanckResult002 SubmitRemoteHitImpulse(const PlanckRemoteHitImpulseRequest002 &request) noexcept override;
        PlanckResult002 SubmitRemoteRagdoll(const PlanckRemoteRagdollRequest002 &request) noexcept override;
        PlanckResult002 SubmitRemoteRagdollExit(const PlanckRemoteRagdollExitRequest002 &request) noexcept override;
        PlanckResult002 BeginRemoteGrip(const PlanckBeginRemoteGripRequest002 &request) noexcept override;
        PlanckResult002 UpdateRemoteGrip(const PlanckUpdateRemoteGripRequest002 &request) noexcept override;
        PlanckResult002 EndRemoteGrip(const PlanckEndRemoteGripRequest002 &request) noexcept override;
        PlanckResult002 ClearRemoteSession(const PlanckClearRemoteSessionRequest002 &request) noexcept override;
        PlanckResult002 DequeueLocalPhysicalEvent(const PlanckDequeueLocalPhysicalEventRequest002 &request, PlanckLocalPhysicalEvent002 &result) noexcept override;
        PlanckResult002 DiscardLocalPhysicalEvents(const PlanckDiscardLocalPhysicalEventsRequest002 &request) noexcept override;

        void DrainRemoteCommands(std::vector<RemoteCommand002> &out) noexcept;
        void EnqueueLocalPhysicalEvent(const PlanckLocalPhysicalEvent002 &event) noexcept;
        void ResetRemoteState() noexcept;
        void ForgetRemoteTarget(UInt32 targetFormId) noexcept;
        [[nodiscard]] bool IsRemoteSessionCancelled(UInt64 sourceSession) noexcept;
        [[nodiscard]] UInt64 GetCancellationGeneration() noexcept;
        void CompleteCancellationSweep(UInt64 cancellationGeneration) noexcept;

    private:
        PlanckResult002 Enqueue(RemoteCommand002 command, const PlanckRequestHeader002 &header, UInt32 targetFormId) noexcept;
        PlanckResult002 InvalidRequest() noexcept;
        void PruneCancelledRemoteSessions(std::uint64_t now) noexcept;
        void ForgetGripAdmissionsForSession(UInt64 sourceSession) noexcept;
        void ForgetGripAdmissionsForTarget(UInt32 targetFormId) noexcept;
        void DisableRemoteAdmission() noexcept;
        std::mutex remoteLock;
        std::deque<RemoteCommand002> remoteCommands;
        std::deque<PlanckLocalPhysicalEvent002> localEvents;
        struct SeenEvent { std::uint64_t session, event; UInt32 targetFormId; std::uint64_t expiresAt; };
        std::deque<SeenEvent> seenEvents;
        struct GripAdmissionKey {
            UInt64 sourceSession;
            UInt64 gripId;

            bool operator==(const GripAdmissionKey &other) const
            {
                return sourceSession == other.sourceSession && gripId == other.gripId;
            }
        };
        struct GripAdmissionKeyHash {
            std::size_t operator()(const GripAdmissionKey &key) const noexcept
            {
                const auto mixed = key.sourceSession ^ (key.gripId + 0x9e3779b97f4a7c15ULL +
                    (key.sourceSession << 6) + (key.sourceSession >> 2));
                return static_cast<std::size_t>(mixed);
            }
        };
        // Bound by remote-command capacity and keyed by the source session,
        // which is the authenticated producer token for Interface002.
        std::unordered_map<GripAdmissionKey, UInt32, GripAdmissionKeyHash> gripAdmissions;
        struct CancelledSession {
            std::uint64_t expiresAt;
            std::uint64_t generation;
        };
        std::unordered_map<std::uint64_t, CancelledSession> cancelledRemoteSessions;
        std::deque<std::uint64_t> cancelledRemoteSessionOrder;
        std::uint64_t nextSequence = 1;
        std::uint64_t nextLocalEventId = 1;
        std::uint64_t cancellationGeneration = 0;
        std::uint64_t completedCancellationGeneration = 0;
        UInt32 rejectedRequests = 0;
        UInt32 overflowRequests = 0;
        UInt32 cancelledSessionEvictions = 0;
        UInt32 cancellationCapacityFailures = 0;
        bool remoteAdmissionDisabled = false;
    };
}

extern PlanckPluginAPI::PlanckInterface001 g_interface001;
extern PlanckPluginAPI::PlanckInterface002 g_interface002;
