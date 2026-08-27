#pragma once

#include <atomic>
#include <array>
#include <condition_variable>
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
    [[nodiscard]] constexpr bool CanClaimRemoteApply002(bool resetInProgress, bool admissionDisabled,
        bool sessionCancelled, bool claimActive) noexcept
    {
        return !resetInProgress && !admissionDisabled && !sessionCancelled && !claimActive;
    }
    static_assert(CanClaimRemoteApply002(false, false, false, false));
    static_assert(!CanClaimRemoteApply002(true, false, false, false));
    static_assert(!CanClaimRemoteApply002(false, true, false, false));
    static_assert(!CanClaimRemoteApply002(false, false, true, false));
    static_assert(!CanClaimRemoteApply002(false, false, false, true));

    [[nodiscard]] constexpr bool ReleasesGripDriveReceiptReservation002(bool hasReservation,
        PlanckRemoteCompletionStatus002 status) noexcept
    {
        return hasReservation && status != PlanckRemoteCompletionStatus002::GripBeginAdmitted;
    }
    static_assert(!ReleasesGripDriveReceiptReservation002(false, PlanckRemoteCompletionStatus002::Cancelled));
    static_assert(!ReleasesGripDriveReceiptReservation002(true, PlanckRemoteCompletionStatus002::GripBeginAdmitted));
    static_assert(ReleasesGripDriveReceiptReservation002(true, PlanckRemoteCompletionStatus002::InvalidState));

    enum class RemoteResetQuiescence002 : UInt8 { Quiesced, RetryRequired };

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
        UInt64 sequence{};
        bool hasGripDriveReceiptReservation{};
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
        [[nodiscard]] UInt64 EventId() const
        {
            switch (type) {
            case RemoteCommandType002::HitImpulse: return request.hitImpulse.header.eventId;
            case RemoteCommandType002::Ragdoll: return request.ragdoll.header.eventId;
            case RemoteCommandType002::RagdollExit: return request.ragdollExit.header.eventId;
            case RemoteCommandType002::BeginGrip: return request.beginGrip.header.eventId;
            case RemoteCommandType002::UpdateGrip: return request.updateGrip.header.eventId;
            case RemoteCommandType002::EndGrip: return request.endGrip.header.eventId;
            }
            return 0;
        }
        [[nodiscard]] UInt64 GripId() const
        {
            switch (type) {
            case RemoteCommandType002::BeginGrip: return request.beginGrip.gripId;
            case RemoteCommandType002::UpdateGrip: return request.updateGrip.gripId;
            case RemoteCommandType002::EndGrip: return request.endGrip.gripId;
            default: return 0;
            }
        }
        [[nodiscard]] UInt32 TargetFormId() const
        {
            switch (type) {
            case RemoteCommandType002::HitImpulse: return request.hitImpulse.targetFormId;
            case RemoteCommandType002::Ragdoll: return request.ragdoll.targetFormId;
            case RemoteCommandType002::RagdollExit: return request.ragdollExit.targetFormId;
            case RemoteCommandType002::BeginGrip: return request.beginGrip.targetFormId;
            case RemoteCommandType002::UpdateGrip: return request.updateGrip.targetFormId;
            case RemoteCommandType002::EndGrip: return request.endGrip.targetFormId;
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
        PlanckResult002 DequeueRemoteCompletionReceipt(const PlanckDequeueRemoteCompletionReceiptRequest002 &request, PlanckRemoteCompletionReceipt002 &result) noexcept override;

        [[nodiscard]] bool DrainRemoteCommands(std::vector<RemoteCommand002> &out) noexcept;
        void EnqueueLocalPhysicalEvent(const PlanckLocalPhysicalEvent002 &event) noexcept;
        // Reset is split so the game thread can terminalize retained grip
        // controllers while new physics claims remain barred.
        [[nodiscard]] RemoteResetQuiescence002 BeginRemoteReset() noexcept;
        // Used only after an exceptional wait result. It performs bounded
        // blocking retries and fail-stops if safe quiescence cannot be proven.
        void RequireRemoteResetQuiescence() noexcept;
        void FinishRemoteReset() noexcept;
        void ForgetRemoteTarget(UInt32 targetFormId) noexcept;
        [[nodiscard]] bool IsRemoteSessionCancelled(UInt64 sourceSession) noexcept;
        [[nodiscard]] bool TryClaimRemoteCommand(const RemoteCommand002 &command) noexcept;
        [[nodiscard]] bool TryClaimRemoteGripDrive(UInt64 sourceSession, UInt64 beginEventId) noexcept;
        void ReleaseRemoteGripDriveClaim(UInt64 sourceSession, UInt64 beginEventId) noexcept;
        [[nodiscard]] UInt64 GetCancellationGeneration() noexcept;
        void CompleteCancellationSweep(UInt64 cancellationGeneration) noexcept;
        void CompleteRemoteCommand(const RemoteCommand002 &command, PlanckRemoteCompletionStatus002 status) noexcept;
        void CompleteRemoteGripDrive(UInt64 sourceSession, UInt64 eventId, UInt64 gripId, UInt32 targetFormId,
                                     PlanckRemoteCompletionStatus002 status) noexcept;

    private:
        PlanckResult002 Enqueue(RemoteCommand002 command, const PlanckRequestHeader002 &header, UInt32 targetFormId) noexcept;
        PlanckResult002 InvalidRequest() noexcept;
        void PruneCancelledRemoteSessions(std::uint64_t now) noexcept;
        void ForgetGripAdmissionsForSession(UInt64 sourceSession) noexcept;
        void ForgetGripAdmissionsForTarget(UInt32 targetFormId) noexcept;
        void DisableRemoteAdmission() noexcept;
        enum class ApplyClaimKind : UInt8 { None, Command, GripDrive };
        [[nodiscard]] bool TryClaimRemoteApplyLocked(UInt64 sourceSession, UInt64 operationId, ApplyClaimKind kind) noexcept;
        [[nodiscard]] bool ReleaseRemoteApplyClaimLocked(UInt64 sourceSession, UInt64 operationId, ApplyClaimKind kind) noexcept;
        [[nodiscard]] bool AppendRemoteCompletionReceiptLocked(const PlanckRemoteCompletionReceipt002 &receipt) noexcept;
        void CompleteQueuedRemoteCommandLocked(const RemoteCommand002 &command,
                                                PlanckRemoteCompletionStatus002 status) noexcept;
        void ReleaseGripDriveReceiptReservationLocked() noexcept;
        [[nodiscard]] RemoteResetQuiescence002 WaitForRemoteResetQuiescenceLocked(
            std::unique_lock<std::mutex> &lock) noexcept;
        std::mutex remoteLock;
        std::condition_variable remoteClaimChanged;
        std::deque<RemoteCommand002> remoteCommands;
        std::deque<PlanckLocalPhysicalEvent002> localEvents;
        // Completion receipts use preallocated ring storage. Admission reserves
        // a slot for every command terminalization and, for BeginGrip, its
        // first-drive outcome before the command can enter the queue.
        static constexpr std::size_t kRemoteCompletionReceiptLimit002 = 2048;
        std::array<PlanckRemoteCompletionReceipt002, kRemoteCompletionReceiptLimit002> remoteCompletionReceipts{};
        std::size_t remoteCompletionReceiptHead = 0;
        std::size_t remoteCompletionReceiptCount = 0;
        std::size_t reservedRemoteCompletionReceipts = 0;
        std::size_t inFlightRemoteCommands = 0;
        std::size_t reservedGripDriveReceipts = 0;
        UInt64 claimedSourceSession = 0;
        UInt64 claimedOperationId = 0;
        ApplyClaimKind claimedApplyKind = ApplyClaimKind::None;
        bool resetInProgress = false;
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
        UInt32 completionReceiptOverflows = 0;
        UInt32 cancelledSessionEvictions = 0;
        UInt32 cancellationCapacityFailures = 0;
        bool remoteAdmissionDisabled = false;
    };
}

extern PlanckPluginAPI::PlanckInterface001 g_interface001;
extern PlanckPluginAPI::PlanckInterface002 g_interface002;
