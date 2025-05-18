#ifndef UPNP_MANAGER_H
#define UPNP_MANAGER_H

#include <string>

#ifdef ENABLE_UPNP
namespace Upnp {

// Initializes the UPnP subsystem. Returns true on success.
bool Initialize();

// Maps `port` on the router to the local host. Returns true if mapping succeeded.
bool MapPort(int port, const std::string& description);

// Removes a previously added port mapping for `port`. Returns true on success.
bool UnmapPort(int port);

// Retrieves the external IP address as a string. Empty on failure.
std::string GetExternalIPAddress();

// Performs any needed cleanup of UPnP resources.
void Shutdown();

}  // namespace Upnp
#else

// Stubs when UPnP is disabled:
namespace Upnp {
inline bool Initialize() { return false; }
inline bool MapPort(int) { return false; }
inline bool UnmapPort(int) { return false; }
inline std::string GetExternalIPAddress() { return {}; }
inline void Shutdown() {}
}  // namespace Upnp
#endif

#endif // UPNP_MANAGER_H