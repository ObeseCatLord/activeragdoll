#include <vector>
#include <algorithm>
#include <chrono>
#include <cmath>

#include "config.h"
#include "pluginapi.h"
#include "version.h"

using namespace PlanckPluginAPI;

// A message used to fetch PLANCK's interface
struct PlanckMessage {
    enum { kMessage_GetInterface = 0x92F38745 }; // Randomly generated
    void *(*GetApiFunction)(unsigned int revisionNumber) = nullptr;
};

// Interface classes are stored statically
PlanckInterface001 g_interface001;
PlanckInterface002 g_interface002;

namespace {
    constexpr size_t kRemoteCommandLimit002 = 1024;
    constexpr size_t kLocalEventLimit002 = 512;
    constexpr size_t kSeenEventLimit002 = 2048;
    constexpr size_t kGripAdmissionLimit002 = kRemoteCommandLimit002;
    constexpr std::uint64_t kSeenEventLifetimeMs002 = 30000;
    constexpr size_t kCancelledSessionLimit002 = 2048;
    constexpr std::uint64_t kCancelledSessionLifetimeMs002 = 10 * 60 * 1000;

    std::uint64_t NowMs002()
    {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    PlanckResult002 Result002(PlanckResultCode002 code, std::uint64_t sequence = 0)
    {
        return { sizeof(PlanckResult002), code, sequence };
    }

    bool IsFinite002(float value, float limit)
    {
        return std::isfinite(value) && std::abs(value) <= limit;
    }

    bool IsFiniteVector002(const PlanckVector3_002 &value, float limit)
    {
        return IsFinite002(value.x, limit) && IsFinite002(value.y, limit) && IsFinite002(value.z, limit);
    }

    bool IsValidHeader002(const PlanckRequestHeader002 &header, size_t size)
    {
        return header.size >= size && header.reserved == 0 && header.sourceSession != 0 && header.eventId != 0;
    }

    bool IsValidGripState002(const PlanckRemoteGripState002 &state)
    {
        const float quatLengthSq = state.worldRotation.x * state.worldRotation.x + state.worldRotation.y * state.worldRotation.y +
            state.worldRotation.z * state.worldRotation.z + state.worldRotation.w * state.worldRotation.w;
        return IsFiniteVector002(state.worldPosition, 10000000.f) &&
            IsFiniteVector002(state.linearVelocity, 1000000.f) &&
            IsFiniteVector002(state.angularVelocity, 1000000.f) &&
            IsFinite002(state.worldRotation.x, 1.f) && IsFinite002(state.worldRotation.y, 1.f) &&
            IsFinite002(state.worldRotation.z, 1.f) && IsFinite002(state.worldRotation.w, 1.f) &&
            quatLengthSq >= 0.25f && quatLengthSq <= 4.f && IsFinite002(state.ttlSeconds, 10.f) &&
            state.ttlSeconds >= 0.02f && state.ttlSeconds <= 10.f && state.reserved == 0;
    }

    bool HasNonEmptyNulNodeName002(const char (&name)[kPlanckInterface002NodeNameCapacity])
    {
        if (name[0] == '\0') return false;
        for (const char *it = name; it != name + kPlanckInterface002NodeNameCapacity; ++it) {
            const unsigned char character = static_cast<unsigned char>(*it);
            if (character == '\0') return true;
            if (character < 0x20 || character == 0x7f) return false;
        }
        return false;
    }

    bool IsValidLocalNodeName002(const char (&name)[kPlanckInterface002NodeNameCapacity], bool required)
    {
        if (!required && name[0] == '\0') return true;
        return HasNonEmptyNulNodeName002(name);
    }

    bool HasSaneLocalEventFields002(const PlanckLocalPhysicalEvent002 &event)
    {
        const float quatLengthSq = event.rotation.x * event.rotation.x + event.rotation.y * event.rotation.y +
            event.rotation.z * event.rotation.z + event.rotation.w * event.rotation.w;
        return IsFiniteVector002(event.position, 10000000.f) && IsFiniteVector002(event.velocity, 1000000.f) &&
            IsFiniteVector002(event.linearVelocity, 1000000.f) && IsFiniteVector002(event.angularVelocity, 1000000.f) &&
            IsFiniteVector002(event.sourcePosition, 10000000.f) &&
            IsFinite002(event.rotation.x, 1.f) && IsFinite002(event.rotation.y, 1.f) &&
            IsFinite002(event.rotation.z, 1.f) && IsFinite002(event.rotation.w, 1.f) && quatLengthSq >= 0.25f && quatLengthSq <= 4.f &&
            IsFinite002(event.impulseMultiplier, 10.f) && event.impulseMultiplier >= 0.f &&
            IsFinite002(event.ttlSeconds, 10.f) && event.ttlSeconds >= 0.f;
    }

    bool IsValidLocalEvent002(const PlanckLocalPhysicalEvent002 &event)
    {
        if (event.targetFormId == 0 || event.reserved[0] != 0 || event.reserved[1] != 0 || event.reserved[2] != 0 ||
            !IsValidLocalNodeName002(event.nodeName, false) || !HasSaneLocalEventFields002(event)) return false;
        switch (event.kind) {
        case PlanckLocalPhysicalEventKind002::HitImpulse:
            return IsValidLocalNodeName002(event.nodeName, true) &&
                IsFiniteVector002(event.position, 10000000.f) && IsFiniteVector002(event.velocity, 1000000.f) &&
                IsFinite002(event.impulseMultiplier, 10.f) && event.impulseMultiplier >= 0.f;
        case PlanckLocalPhysicalEventKind002::RagdollEnter:
            return IsFiniteVector002(event.sourcePosition, 10000000.f);
        case PlanckLocalPhysicalEventKind002::RagdollExit:
            return true;
        case PlanckLocalPhysicalEventKind002::GripBegin:
        case PlanckLocalPhysicalEventKind002::GripUpdate: {
            const PlanckRemoteGripState002 state{ event.position, event.rotation, event.linearVelocity, event.angularVelocity, event.ttlSeconds, 0 };
            return event.gripId != 0 && IsValidLocalNodeName002(event.nodeName, true) && IsValidGripState002(state);
        }
        case PlanckLocalPhysicalEventKind002::GripEnd:
            return event.gripId != 0;
        default:
            return false;
        }
    }
}

// Constructs and returns an API of the revision number requested
void *GetApi(unsigned int revisionNumber) {
    switch (revisionNumber) {
    case 1:	_MESSAGE("Interface revision 1 requested"); return &g_interface001;
    case 2: _MESSAGE("Interface revision 2 requested"); return &g_interface002;
    }
    return nullptr;
}

// Handles skse mod messages requesting to fetch API functions from PLANCK
void PlanckPluginAPI::ModMessageHandler(SKSEMessagingInterface::Message *message) {
    if (message->type == PlanckMessage::kMessage_GetInterface) {
        PlanckMessage *planckMessage = (PlanckMessage *)message->data;
        planckMessage->GetApiFunction = GetApi;
        _MESSAGE("Provided PLANCK plugin interface to \"%s\"", message->sender);
    }
}

// PLANCK build numbers are made up as follows: V01.00.05.00
constexpr int planckBuildNumber = ACTIVERAGDOLL_VERSION_MAJOR * 1000000 + ACTIVERAGDOLL_VERSION_MINOR * 10000 + ACTIVERAGDOLL_VERSION_PATCH * 100 + ACTIVERAGDOLL_VERSION_BETA;

// Fetches the PLANCK version number
unsigned int PlanckInterface001::GetBuildNumber() {
    return planckBuildNumber;
}

bool PlanckInterface001::Deprecated1(const std::string_view &name, double &out) {
    return Config::GetSettingDouble(name, out);
}

bool PlanckInterface001::Deprecated2(const std::string &name, double val) {
    return Config::SetSettingDouble(name, val);
}

bool PlanckInterface001::GetSettingDouble(const char *name, double &out) {
    return Config::GetSettingDouble(name, out);
}

bool PlanckInterface001::SetSettingDouble(const char *name, double val) {
    return Config::SetSettingDouble(name, val);
}

void PlanckInterface001::AddIgnoredActor(Actor *actor) {
    std::scoped_lock lock(ignoredActorsLock);
    ignoredActors.insert(actor);
}

void PlanckInterface001::RemoveIgnoredActor(Actor *actor) {
    std::scoped_lock lock(ignoredActorsLock);
    ignoredActors.erase(actor);
}

void PlanckInterface001::AddAggressionIgnoredActor(Actor *actor) {
    std::scoped_lock lock(aggressionIgnoredActorsLock);
    aggressionIgnoredActors.insert(actor);
}

void PlanckInterface001::RemoveAggressionIgnoredActor(Actor *actor) {
    std::scoped_lock lock(aggressionIgnoredActorsLock);
    aggressionIgnoredActors.erase(actor);
}

void PlanckInterface001::SetAggressionLowTopic(Actor *actor, TESTopic *topic) {
    std::scoped_lock lock(aggressionTopicsLock);
    if (topic) {
        lowAggressionTopics[actor] = topic;
    }
    else {
        lowAggressionTopics.erase(actor);
    }
}

void PlanckInterface001::SetAggressionHighTopic(Actor *actor, TESTopic *topic) {
    std::scoped_lock lock(aggressionTopicsLock);
    if (topic) {
        highAggressionTopics[actor] = topic;
    }
    else {
        highAggressionTopics.erase(actor);
    }
}

void PlanckInterface001::AddRagdollCollisionIgnoredActor(Actor *actor) {
    std::scoped_lock lock(ragdollCollisionIgnoredActorsLock);
    ragdollCollisionIgnoredActors.insert(actor);
}

void PlanckInterface001::RemoveRagdollCollisionIgnoredActor(Actor *actor) {
    std::scoped_lock lock(ragdollCollisionIgnoredActorsLock);
    ragdollCollisionIgnoredActors.erase(actor);
}

bool PlanckInterface001::IsRagdollCollisionIgnored(TESObjectREFR *actor) {
    std::scoped_lock lock(ragdollCollisionIgnoredActorsLock);
    return ragdollCollisionIgnoredActors.find(actor) != ragdollCollisionIgnoredActors.end();
}

PlanckHitData PlanckInterface001::GetLastHitData()
{
    return lastHitData; // deliberate copy
}

TESHitEvent *PlanckInterface001::GetCurrentHitEvent() {
    return currentHitEvent;
}

PlanckResult002 PlanckInterface002::GetCapabilities(const PlanckCapabilitiesRequest002 &request, PlanckCapabilitiesResult002 &result) noexcept
{
    if (request.size < sizeof(request) || request.reserved != 0 || result.size < sizeof(result)) return InvalidRequest();
    result = { sizeof(result), kPlanckInterface002Revision,
        kPlanckFeature002_RemoteHitImpulse | kPlanckFeature002_RemoteRagdoll |
        kPlanckFeature002_RemoteGripImpulseDrive | kPlanckFeature002_LocalPhysicalEvents |
        kPlanckFeature002_LocalEventRebase,
        static_cast<UInt32>(kRemoteCommandLimit002), static_cast<UInt32>(kLocalEventLimit002) };
    return Result002(PlanckResultCode002::Accepted);
}

PlanckResult002 PlanckInterface002::InvalidRequest() noexcept
{
    std::scoped_lock lock(remoteLock);
    ++rejectedRequests;
    if ((rejectedRequests & (rejectedRequests - 1)) == 0) _MESSAGE("PLANCK interface 002 rejected %u duplicate/invalid requests", rejectedRequests);
    return Result002(PlanckResultCode002::InvalidRequest);
}

void PlanckInterface002::ForgetGripAdmissionsForSession(UInt64 sourceSession) noexcept
{
    if (sourceSession == 0) return;
    for (auto it = gripAdmissions.begin(); it != gripAdmissions.end();) {
        if (it->first.sourceSession == sourceSession) it = gripAdmissions.erase(it);
        else ++it;
    }
}

void PlanckInterface002::ForgetGripAdmissionsForTarget(UInt32 targetFormId) noexcept
{
    if (targetFormId == 0) return;
    for (auto it = gripAdmissions.begin(); it != gripAdmissions.end();) {
        if (it->second == targetFormId) it = gripAdmissions.erase(it);
        else ++it;
    }
}

PlanckResult002 PlanckInterface002::Enqueue(RemoteCommand002 command, const PlanckRequestHeader002 &header, UInt32 targetFormId) noexcept
{
    std::scoped_lock lock(remoteLock);
    const auto rejectLocked = [this]() {
        ++rejectedRequests;
        if ((rejectedRequests & (rejectedRequests - 1)) == 0)
            _MESSAGE("PLANCK interface 002 rejected %u duplicate/invalid requests", rejectedRequests);
        return Result002(PlanckResultCode002::InvalidRequest);
    };
    const std::uint64_t now = NowMs002();
    PruneCancelledRemoteSessions(now);
    // A clear is an irreversible lifecycle boundary for a source-session
    // token. Do not allow a racing network producer to requeue stale work
    // after it has been cancelled.
    if (remoteAdmissionDisabled || cancelledRemoteSessions.contains(header.sourceSession))
        return Result002(PlanckResultCode002::Duplicate);
    while (!seenEvents.empty() && seenEvents.front().expiresAt <= now) seenEvents.pop_front();
    if (std::any_of(seenEvents.begin(), seenEvents.end(), [&header](const SeenEvent &seen) {
        return seen.session == header.sourceSession && seen.event == header.eventId;
    })) {
        ++rejectedRequests;
        if ((rejectedRequests & (rejectedRequests - 1)) == 0) _MESSAGE("PLANCK interface 002 rejected %u duplicate/invalid requests", rejectedRequests);
        return Result002(PlanckResultCode002::Duplicate);
    }

    GripAdmissionKey gripKey{};
    bool hasGripAdmission = false;
    bool addGripAdmission = false;
    bool removeGripAdmission = false;
    bool removeTargetAdmissions = false;
    switch (command.type) {
    case RemoteCommandType002::BeginGrip:
        gripKey = { header.sourceSession, command.request.beginGrip.gripId };
        hasGripAdmission = true;
        if (const auto existing = gripAdmissions.find(gripKey); existing != gripAdmissions.end()) {
            // A duplicate Begin may refresh the same grip, but it can never
            // retarget this authenticated producer+GripId.
            if (existing->second != targetFormId) return rejectLocked();
        }
        else {
            if (gripAdmissions.size() >= kGripAdmissionLimit002) {
                ++overflowRequests;
                if ((overflowRequests & (overflowRequests - 1)) == 0)
                    _MESSAGE("PLANCK interface 002 grip admission overflow (%u rejections)", overflowRequests);
                return Result002(PlanckResultCode002::QueueFull);
            }
            addGripAdmission = true;
        }
        break;
    case RemoteCommandType002::UpdateGrip:
        gripKey = { header.sourceSession, command.request.updateGrip.gripId };
        hasGripAdmission = true;
        break;
    case RemoteCommandType002::EndGrip:
        gripKey = { header.sourceSession, command.request.endGrip.gripId };
        hasGripAdmission = true;
        removeGripAdmission = true;
        break;
    case RemoteCommandType002::RagdollExit:
        removeTargetAdmissions = true;
        break;
    default:
        break;
    }
    if (hasGripAdmission && command.type != RemoteCommandType002::BeginGrip) {
        const auto existing = gripAdmissions.find(gripKey);
        if (existing == gripAdmissions.end() || existing->second != targetFormId)
            return rejectLocked();
    }
    if (remoteCommands.size() >= kRemoteCommandLimit002 || seenEvents.size() >= kSeenEventLimit002) {
        ++overflowRequests;
        if ((overflowRequests & (overflowRequests - 1)) == 0) _MESSAGE("PLANCK interface 002 queue overflow (%u rejections)", overflowRequests);
        return Result002(PlanckResultCode002::QueueFull);
    }
    // Reserve every container before consuming a sequence or mutating any
    // structure. If allocation fails here the caller gets an explicit
    // AllocationFailure and no partial admission survives.
    try {
        seenEvents.reserve(seenEvents.size() + 1);
        remoteCommands.reserve(remoteCommands.size() + 1);
        if (addGripAdmission) gripAdmissions.reserve(gripAdmissions.size() + 1);
    }
    catch (...) {
        return Result002(PlanckResultCode002::AllocationFailure, nextSequence);
    }

    const std::uint64_t sequence = nextSequence++;
    const auto priorSeenCount = seenEvents.size();
    const auto priorCommandCount = remoteCommands.size();
    try {
        seenEvents.push_back({ header.sourceSession, header.eventId, targetFormId, now + kSeenEventLifetimeMs002 });
        remoteCommands.push_back(command);
        if (addGripAdmission) gripAdmissions.emplace(gripKey, targetFormId);
    }
    catch (...) {
        // The pre-reserves above make these pushes non-throwing in practice;
        // the count guards keep the rollback exact under any partial failure.
        while (remoteCommands.size() > priorCommandCount) remoteCommands.pop_back();
        while (seenEvents.size() > priorSeenCount) seenEvents.pop_back();
        if (nextSequence != 0) --nextSequence;
        return Result002(PlanckResultCode002::AllocationFailure, sequence);
    }
    if (removeTargetAdmissions) {
        for (auto it = gripAdmissions.begin(); it != gripAdmissions.end();) {
            if (it->first.sourceSession == header.sourceSession && it->second == targetFormId) it = gripAdmissions.erase(it);
            else ++it;
        }
    }
    return Result002(PlanckResultCode002::Accepted, sequence);
}

PlanckResult002 PlanckInterface002::SubmitRemoteHitImpulse(const PlanckRemoteHitImpulseRequest002 &request) noexcept
{
    if (!IsValidHeader002(request.header, sizeof(request)) || request.targetFormId == 0 ||
        !HasNonEmptyNulNodeName002(request.nodeName) ||
        !IsFiniteVector002(request.position, 10000000.f) || !IsFiniteVector002(request.velocity, 1000000.f) ||
        !IsFinite002(request.impulseMultiplier, 10.f) || request.impulseMultiplier < 0.f) return InvalidRequest();
    RemoteCommand002 command{}; command.type = RemoteCommandType002::HitImpulse; command.request.hitImpulse = request;
    return Enqueue(command, request.header, request.targetFormId);
}

PlanckResult002 PlanckInterface002::SubmitRemoteRagdoll(const PlanckRemoteRagdollRequest002 &request) noexcept
{
    if (!IsValidHeader002(request.header, sizeof(request)) || request.targetFormId == 0 || request.reserved != 0 || !IsFiniteVector002(request.sourcePosition, 10000000.f)) return InvalidRequest();
    RemoteCommand002 command{}; command.type = RemoteCommandType002::Ragdoll; command.request.ragdoll = request;
    return Enqueue(command, request.header, request.targetFormId);
}

PlanckResult002 PlanckInterface002::SubmitRemoteRagdollExit(const PlanckRemoteRagdollExitRequest002 &request) noexcept
{
    if (!IsValidHeader002(request.header, sizeof(request)) || request.targetFormId == 0 || request.reserved != 0) return InvalidRequest();
    RemoteCommand002 command{}; command.type = RemoteCommandType002::RagdollExit; command.request.ragdollExit = request;
    return Enqueue(command, request.header, request.targetFormId);
}

PlanckResult002 PlanckInterface002::BeginRemoteGrip(const PlanckBeginRemoteGripRequest002 &request) noexcept
{
    if (!IsValidHeader002(request.header, sizeof(request)) || request.gripId == 0 || request.targetFormId == 0 ||
        !HasNonEmptyNulNodeName002(request.nodeName) || !IsValidGripState002(request.state)) return InvalidRequest();
    RemoteCommand002 command{}; command.type = RemoteCommandType002::BeginGrip; command.request.beginGrip = request;
    return Enqueue(command, request.header, request.targetFormId);
}

PlanckResult002 PlanckInterface002::UpdateRemoteGrip(const PlanckUpdateRemoteGripRequest002 &request) noexcept
{
    if (!IsValidHeader002(request.header, sizeof(request)) || request.gripId == 0 || request.targetFormId == 0 || request.reserved != 0 || !IsValidGripState002(request.state)) return InvalidRequest();
    RemoteCommand002 command{}; command.type = RemoteCommandType002::UpdateGrip; command.request.updateGrip = request;
    return Enqueue(command, request.header, request.targetFormId);
}

PlanckResult002 PlanckInterface002::EndRemoteGrip(const PlanckEndRemoteGripRequest002 &request) noexcept
{
    if (!IsValidHeader002(request.header, sizeof(request)) || request.gripId == 0 || request.targetFormId == 0 || request.reserved != 0) return InvalidRequest();
    RemoteCommand002 command{}; command.type = RemoteCommandType002::EndGrip; command.request.endGrip = request;
    return Enqueue(command, request.header, request.targetFormId);
}

PlanckResult002 PlanckInterface002::ClearRemoteSession(const PlanckClearRemoteSessionRequest002 &request) noexcept
{
    if (!IsValidHeader002(request.header, sizeof(request))) return InvalidRequest();
    std::scoped_lock lock(remoteLock);
    const std::uint64_t now = NowMs002();
    PruneCancelledRemoteSessions(now);

    // This operation is intentionally not queued: cancellation must win over
    // every queued mutation even when the command queue is full. Marking the
    // session first also lets the game-thread drain reject a command that was
    // copied out immediately before this lock was acquired.
    ++cancellationGeneration;
    if (cancellationGeneration == 0) ++cancellationGeneration;
    if (!remoteAdmissionDisabled && !cancelledRemoteSessions.contains(request.header.sourceSession)) {
        if (cancelledRemoteSessions.size() >= kCancelledSessionLimit002) {
            DisableRemoteAdmission();
        }
        else {
            // Construct the tombstone before touching live state, then commit
            // both entries. If the order push throws after the map emplace,
            // erase the map entry so no orphaned tombstone can evade pruning.
            const auto tombstone = std::make_pair(request.header.sourceSession,
                CancelledSession{ now + kCancelledSessionLifetimeMs002, cancellationGeneration });
            try {
                cancelledRemoteSessionOrder.push_back(request.header.sourceSession);
                try {
                    cancelledRemoteSessions.insert(tombstone);
                }
                catch (...) {
                    cancelledRemoteSessionOrder.pop_back();
                    throw;
                }
            }
            catch (...) {
                return Result002(PlanckResultCode002::AllocationFailure, nextSequence);
            }
        }
    }
    remoteCommands.erase(std::remove_if(remoteCommands.begin(), remoteCommands.end(),
        [&request](const RemoteCommand002 &command) {
            return command.SourceSession() == request.header.sourceSession;
        }), remoteCommands.end());
    seenEvents.erase(std::remove_if(seenEvents.begin(), seenEvents.end(),
        [&request](const SeenEvent &seen) {
            return seen.session == request.header.sourceSession;
        }), seenEvents.end());
    ForgetGripAdmissionsForSession(request.header.sourceSession);
    const std::uint64_t sequence = nextSequence++;
    if (nextSequence == 0) ++nextSequence;
    return Result002(PlanckResultCode002::Accepted, sequence);
}

PlanckResult002 PlanckInterface002::DequeueLocalPhysicalEvent(const PlanckDequeueLocalPhysicalEventRequest002 &request, PlanckLocalPhysicalEvent002 &result) noexcept
{
    if (request.size < sizeof(request) || request.reserved != 0 || result.size < sizeof(result)) return InvalidRequest();
    std::scoped_lock lock(remoteLock);
    if (localEvents.empty()) return Result002(PlanckResultCode002::Empty);
    result = localEvents.front();
    localEvents.pop_front();
    return Result002(PlanckResultCode002::Accepted, result.eventId);
}

PlanckResult002 PlanckInterface002::DiscardLocalPhysicalEvents(const PlanckDiscardLocalPhysicalEventsRequest002 &request) noexcept
{
    if (request.size < sizeof(request) || request.reserved != 0)
        return InvalidRequest();
    std::scoped_lock lock(remoteLock);
    localEvents.clear();
    return Result002(PlanckResultCode002::Accepted, request.lifecycleGeneration);
}

void PlanckInterface002::DrainRemoteCommands(std::vector<RemoteCommand002> &out) noexcept
{
    std::scoped_lock lock(remoteLock);
    out.reserve(out.size() + remoteCommands.size());
    while (!remoteCommands.empty()) {
        out.push_back(remoteCommands.front());
        remoteCommands.pop_front();
    }
}

void PlanckInterface002::EnqueueLocalPhysicalEvent(const PlanckLocalPhysicalEvent002 &event) noexcept
{
    std::scoped_lock lock(remoteLock);
    if (!IsValidLocalEvent002(event)) {
        ++rejectedRequests;
        if ((rejectedRequests & (rejectedRequests - 1)) == 0) _MESSAGE("PLANCK interface 002 rejected %u duplicate/invalid requests", rejectedRequests);
        return;
    }
    PlanckLocalPhysicalEvent002 copy = event;
    copy.size = sizeof(copy);
    copy.eventId = nextLocalEventId++;
    if (nextLocalEventId == 0) ++nextLocalEventId;

    const auto isUpdate = [](const PlanckLocalPhysicalEvent002 &value) {
        return value.kind == PlanckLocalPhysicalEventKind002::GripUpdate;
    };
    const auto isTerminal = [](const PlanckLocalPhysicalEvent002 &value) {
        return value.kind == PlanckLocalPhysicalEventKind002::GripEnd ||
            value.kind == PlanckLocalPhysicalEventKind002::RagdollExit;
    };
    const auto logOverflow = [this]() {
        ++overflowRequests;
        if ((overflowRequests & (overflowRequests - 1)) == 0) _MESSAGE("PLANCK interface 002 local event overflow (%u drops)", overflowRequests);
    };
    if (localEvents.size() >= kLocalEventLimit002) {
        if (isUpdate(copy)) {
            const auto existing = std::find_if(localEvents.begin(), localEvents.end(), [&copy, &isUpdate](const PlanckLocalPhysicalEvent002 &queued) {
                return isUpdate(queued) && queued.targetFormId == copy.targetFormId && queued.gripId == copy.gripId;
            });
            if (existing != localEvents.end()) {
                const auto eventId = existing->eventId;
                *existing = copy;
                existing->eventId = eventId;
                return;
            }
            logOverflow();
            return;
        }

        const auto disposable = std::find_if(localEvents.begin(), localEvents.end(),
            [&isUpdate, &isTerminal, &copy](const PlanckLocalPhysicalEvent002 &queued) {
                if (isUpdate(queued)) return true;
                // A terminal edge may displace any nonterminal edge. Ordinary
                // events never displace queued terminal state.
                return isTerminal(copy) && !isTerminal(queued);
            });
        if (disposable == localEvents.end()) {
            logOverflow();
            return;
        }
        localEvents.erase(disposable);
        logOverflow();
    }
    localEvents.push_back(copy);
}

void PlanckInterface002::ResetRemoteState() noexcept
{
    std::scoped_lock lock(remoteLock);
    remoteCommands.clear();
    seenEvents.clear();
    gripAdmissions.clear();
    cancelledRemoteSessions.clear();
    cancelledRemoteSessionOrder.clear();
    cancellationGeneration = 0;
    completedCancellationGeneration = 0;
    remoteAdmissionDisabled = false;
}

void PlanckInterface002::ForgetRemoteTarget(UInt32 targetFormId) noexcept
{
    std::scoped_lock lock(remoteLock);
    seenEvents.erase(std::remove_if(seenEvents.begin(), seenEvents.end(), [targetFormId](const SeenEvent &seen) {
        return seen.targetFormId == targetFormId;
    }), seenEvents.end());
    ForgetGripAdmissionsForTarget(targetFormId);
}

bool PlanckInterface002::IsRemoteSessionCancelled(UInt64 sourceSession) noexcept
{
    std::scoped_lock lock(remoteLock);
    PruneCancelledRemoteSessions(NowMs002());
    return sourceSession != 0 && (remoteAdmissionDisabled || cancelledRemoteSessions.contains(sourceSession));
}

UInt64 PlanckInterface002::GetCancellationGeneration() noexcept
{
    std::scoped_lock lock(remoteLock);
    return cancellationGeneration;
}

void PlanckInterface002::CompleteCancellationSweep(UInt64 observedCancellationGeneration) noexcept
{
    std::scoped_lock lock(remoteLock);
    if (observedCancellationGeneration != 0 && observedCancellationGeneration == cancellationGeneration)
        completedCancellationGeneration = observedCancellationGeneration;
    PruneCancelledRemoteSessions(NowMs002());
}

void PlanckInterface002::PruneCancelledRemoteSessions(std::uint64_t now) noexcept
{
    UInt32 evicted = 0;
    while (!cancelledRemoteSessionOrder.empty()) {
        const std::uint64_t session = cancelledRemoteSessionOrder.front();
        const auto entry = cancelledRemoteSessions.find(session);
        if (entry == cancelledRemoteSessions.end()) {
            cancelledRemoteSessionOrder.pop_front();
            continue;
        }
        if (entry->second.expiresAt > now || entry->second.generation > completedCancellationGeneration)
            break;
        cancelledRemoteSessions.erase(entry);
        cancelledRemoteSessionOrder.pop_front();
        ++evicted;
    }
    if (evicted != 0) {
        cancelledSessionEvictions += evicted;
        _MESSAGE("PLANCK interface 002 expired %u cancellation tombstones (%u aggregate)", evicted, cancelledSessionEvictions);
    }
}

void PlanckInterface002::DisableRemoteAdmission() noexcept
{
    remoteAdmissionDisabled = true;
    ++cancellationCapacityFailures;
    if ((cancellationCapacityFailures & (cancellationCapacityFailures - 1)) == 0)
        _MESSAGE("PLANCK interface 002 cancellation tombstone capacity reached %u times; remote admission is fail-closed", cancellationCapacityFailures);
}
