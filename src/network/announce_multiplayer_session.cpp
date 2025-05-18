// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <future>
#include <vector>
#include "announce_multiplayer_session.h"
#include "common/announce_multiplayer_room.h"
#include "common/assert.h"
#include "network/network.h"
#include "network/network_settings.h"

#ifdef ENABLE_WEB_SERVICE
#include "web_service/announce_room_json.h"
#endif

#ifdef ENABLE_UPNP
#include "network/upnp_manager.h"
#endif

namespace Network {

// Time between room is announced to web_service
static constexpr std::chrono::seconds announce_time_interval(15);

AnnounceMultiplayerSession::AnnounceMultiplayerSession() {
#ifdef ENABLE_UPNP
    Upnp::Initialize();
#endif
#ifdef ENABLE_WEB_SERVICE
    backend = std::make_unique<WebService::RoomJson>(NetSettings::values.web_api_url,
                                                     NetSettings::values.citra_username,
                                                     NetSettings::values.citra_token);
#else
    backend = std::make_unique<AnnounceMultiplayerRoom::NullBackend>();
#endif
}

Common::WebResult AnnounceMultiplayerSession::Register() {
    auto room = Network::GetRoom().lock();
    if (!room) {
        return {Common::WebResult::Code::LibError, "Network is not initialized"};
    }
    if (room->GetState() != Network::Room::State::Open) {
        return {Common::WebResult::Code::LibError, "Room is not open"};
    }

#ifdef ENABLE_UPNP
    int port = room->GetRoomInformation().port;
    if (Upnp::MapPort(port, "Azahar 3DS Room")) {
        // UPnP mapping succeeded, retrieve external IP for UI or logs
        std::string extIP = Upnp::GetExternalIPAddress();
        LOG_INFO(Network, "UPnP mapped port {} -> external {}", port, extIP);
    } else {
        LOG_WARN(Network, "UPnP mapping failed, client must forward port {} manually", port);
    }
#endif

    UpdateBackendData(room);
    auto result = backend->Register();
    if (result.result_code != Common::WebResult::Code::Success) {
        return result;
    }
    LOG_INFO(WebService, "Room has been registered");
    room->SetVerifyUID(result.returned_data);
    registered = true;
    return {Common::WebResult::Code::Success};
}

void AnnounceMultiplayerSession::Start() {
    if (announce_multiplayer_thread) {
        Stop();
    }
    shutdown_event.Reset();
    announce_multiplayer_thread =
        std::make_unique<std::thread>(&AnnounceMultiplayerSession::AnnounceMultiplayerLoop, this);
}

void AnnounceMultiplayerSession::Stop() {
    if (announce_multiplayer_thread) {
        shutdown_event.Set();
        announce_multiplayer_thread->join();
        announce_multiplayer_thread.reset();
        backend->Delete();
        registered = false;
#ifdef ENABLE_UPNP
        auto room = Network::GetRoom().lock();
        if (room) {
            int port = room->GetRoomInformation().port;
            Upnp::UnmapPort(port);
        }
        Upnp::Shutdown();
#endif
    }
}

AnnounceMultiplayerSession::CallbackHandle AnnounceMultiplayerSession::BindErrorCallback(
    std::function<void(const Common::WebResult&)> function) {
    std::lock_guard lock(callback_mutex);
    auto handle = std::make_shared<std::function<void(const Common::WebResult&)>>(function);
    error_callbacks.insert(handle);
    return handle;
}

void AnnounceMultiplayerSession::UnbindErrorCallback(CallbackHandle handle) {
    std::lock_guard lock(callback_mutex);
    error_callbacks.erase(handle);
}

AnnounceMultiplayerSession::~AnnounceMultiplayerSession() {
    Stop();
}

void AnnounceMultiplayerSession::UpdateBackendData(std::shared_ptr<Network::Room> room) {
    Network::RoomInformation room_information = room->GetRoomInformation();
    std::vector<Network::Room::Member> memberlist = room->GetRoomMemberList();
    backend->SetRoomInformation(
        room_information.name, room_information.description, room_information.port,
        room_information.member_slots, Network::network_version, room->HasPassword(),
        room_information.preferred_game, room_information.preferred_game_id);
    backend->ClearPlayers();
    for (const auto& member : memberlist) {
        backend->AddPlayer(member.username, member.nickname, member.avatar_url, member.mac_address,
                           member.game_info.id, member.game_info.name);
    }
}

void AnnounceMultiplayerSession::AnnounceMultiplayerLoop() {
    // Invokes all current bound error callbacks.
    const auto ErrorCallback = [this](Common::WebResult result) {
        std::lock_guard<std::mutex> lock(callback_mutex);
        for (auto callback : error_callbacks) {
            (*callback)(result);
        }
    };

    if (!registered) {
        Common::WebResult result = Register();
        if (result.result_code != Common::WebResult::Code::Success) {
            ErrorCallback(result);
            return;
        }
    }

    auto update_time = std::chrono::steady_clock::now();
    std::future<Common::WebResult> future;
    while (!shutdown_event.WaitUntil(update_time)) {
        update_time += announce_time_interval;
        std::shared_ptr<Network::Room> room = Network::GetRoom().lock();
        if (!room) {
            break;
        }
        if (room->GetState() != Network::Room::State::Open) {
            break;
        }
        UpdateBackendData(room);
        Common::WebResult result = backend->Update();
        if (result.result_code != Common::WebResult::Code::Success) {
            ErrorCallback(result);
        }
        if (result.result_string == "404") {
            registered = false;
            // Needs to register the room again
            Common::WebResult new_result = Register();
            if (new_result.result_code != Common::WebResult::Code::Success) {
                ErrorCallback(new_result);
            }
        }
    }
}

AnnounceMultiplayerRoom::RoomList AnnounceMultiplayerSession::GetRoomList() {
    return backend->GetRoomList();
}

bool AnnounceMultiplayerSession::IsRunning() const {
    return announce_multiplayer_thread != nullptr;
}

void AnnounceMultiplayerSession::UpdateCredentials() {
    ASSERT_MSG(!IsRunning(), "Credentials can only be updated when session is not running");

#ifdef ENABLE_WEB_SERVICE
    backend = std::make_unique<WebService::RoomJson>(NetSettings::values.web_api_url,
                                                     NetSettings::values.citra_username,
                                                     NetSettings::values.citra_token);
#endif
}

} // namespace Network
