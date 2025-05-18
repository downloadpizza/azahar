
#include "upnp_manager.h"

#ifdef ENABLE_UPNP
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
#include <iostream>

namespace Upnp {
static UPNPUrls gUrls;
static IGDdatas gData;
static std::string gLocalIP;

bool Initialize() {
    struct UPNPDev* devs = upnpDiscover(2000, nullptr, nullptr, 0, 0, 2, nullptr);
    if (!devs) return false;
    int error;
    if (UPNP_GetValidIGD(devs, &gUrls, &gData, nullptr, 0) != UPNPCOMMAND_SUCCESS) {
        return false;
    }
    char lanIP[40];
    if (UPNP_GetValidIGD(devs, &gUrls, &gData, lanIP, sizeof(lanIP)) == UPNPCOMMAND_SUCCESS) {
        gLocalIP = lanIP;
    }
    return true;
}

bool MapPort(int port, const std::string& description) {
    if (gLocalIP.empty()) return false;
    std::string portStr = std::to_string(port);
    int ret = UPNP_AddPortMapping(
        gUrls.controlURL, gData.first.servicetype,
        portStr.c_str(), portStr.c_str(),
        gLocalIP.c_str(), description.c_str(),
        "TCP", nullptr, "0"
    );
    return (ret == UPNPCOMMAND_SUCCESS);
}

bool UnmapPort(int port) {
    std::string portStr = std::to_string(port);
    int ret = UPNP_DeletePortMapping(
        gUrls.controlURL, gData.first.servicetype,
        portStr.c_str(), "TCP", nullptr
    );
    return (ret == UPNPCOMMAND_SUCCESS);
}

std::string GetExternalIPAddress() {
    char external[40] = {0};
    if (UPNP_GetExternalIPAddress(
            gUrls.controlURL, gData.first.servicetype, external) != UPNPCOMMAND_SUCCESS)
        return {};
    return std::string(external);
}

void Shutdown() {
    // No persistent resources in this client; nothing to free
}

} // namespace Upnp
#endif // ENABLE_UPNP
