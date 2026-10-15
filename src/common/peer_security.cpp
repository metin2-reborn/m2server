#include "peer_security.h"

#include <cstring>
#include <span>
#include <string>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

auto is_allowed_peer(const char *address, std::span<const std::string> hosts, const char *local) -> bool {
    in_addr remote{};

    if (!address || inet_pton(AF_INET, address, &remote) != 1) {
        return false;
    }

    if (hosts.empty()) {
        return local && std::strcmp(address, local) == 0;
    }

    for (const std::string &host : hosts) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo *result = nullptr;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0) {
            continue;
        }

        bool allowed = false;
        for (auto *p = result; p; p = p->ai_next) {
            const auto *ip = reinterpret_cast<const sockaddr_in *>(p->ai_addr);
            if (ip->sin_addr.s_addr == remote.s_addr) {
                allowed = true;
                break;
            }
        }

        freeaddrinfo(result);

        if (allowed) {
            return true;
        }
    }

    return false;
}
