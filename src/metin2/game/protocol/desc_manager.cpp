#include <metin2/config/config.h>
#include <metin2/game/protocol/desc_manager.h>

#include <metin2/game/app/crc32.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_p2p.h>
#include <metin2/game/protocol/ip_ban.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/net/connection.h>
#include <metin2/net/io.h>
#include <metin2/proto/length.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace metin2::game::inline protocol {

    desc_manager::desc_manager() : destroyed_(false) {
        initialize();
    }

    desc_manager::~desc_manager() {
        destroy();
    }

    void desc_manager::initialize() {
        sockets_connected_ = 0;
        handle_count_ = 0;
        local_user_count_ = 0;
        memset(empire_user_count_, 0, sizeof(empire_user_count_));
    }

    void desc_manager::destroy() {
        if (destroyed_) {
            return;
        }
        destroyed_ = true;

        desc_set::iterator i = set_desc_.begin();

        while (i != set_desc_.end()) {
            desc *d = *i;
            desc_set::iterator ci = i;
            ++i;

            if (d->type() == DESC_TYPE_CONNECTOR)
                continue;

            if (d->is_phase(PHASE_P2P))
                continue;

            destroy_desc(d, false);
            set_desc_.erase(ci);
        }

        i = set_desc_.begin();

        while (i != set_desc_.end()) {
            desc *d = *i;
            desc_set::iterator ci = i;
            ++i;

            destroy_desc(d, false);
            set_desc_.erase(ci);
        }

        set_client_desc_.clear();

        map_login_name_.clear();
        map_handle_.clear();

        initialize();
    }

    auto desc_manager::create_handshake() -> uint32_t {
        char crc_buf[8];
        crc_t crc;
        desc_handshake_map::iterator it;

    RETRY:
        do {
            uint32_t val = thecore_random() % (1024 * 1024);

            *(uint32_t *)(crc_buf) = val;
            *((uint32_t *)crc_buf + 1) = get_global_time();

            crc = get_cr_c32(crc_buf, 8);
            it = map_handshake_.find(crc);
        } while (it != map_handshake_.end());

        if (crc == 0)
            goto RETRY;

        return (crc);
    }

    auto desc_manager::accept_desc(net::tcp::socket socket) -> desc * {
        auto connection = net::connection::create(std::move(socket), MAX_CLIENT_INPUT, MAX_CLIENT_OUTPUT);
        const char *host = connection->remote_address().c_str();

        if (metin2::config::is_auth()) {
            in_addr addr{};

            if (inet_pton(AF_INET, host, &addr) == 1 && is_ban_ip(addr)) {
                metin2::log::info("Refused connection from banned address {}", host);
                connection->close();
                return nullptr;
            }
        }

        if (sockets_connected_ >= MAX_ALLOW_USER) {
            metin2::log::warn("Connection refused: limit of {:d} connections reached", MAX_ALLOW_USER);
            connection->close();
            return nullptr;
        }

        desc *newd = new desc;
        crc_t handshake = create_handshake();

        if (!newd->setup(std::move(connection), ++handle_count_, handshake)) {
            newd->destroy();
            delete newd;
            return nullptr;
        }

        map_handshake_.insert(desc_handshake_map::value_type(handshake, newd));
        map_handle_.insert(desc_handle_map::value_type(newd->handle(), newd));

        set_desc_.insert(newd);
        ++sockets_connected_;
        return (newd);
    }

    auto desc_manager::accept_p2p_desc(net::tcp::socket socket) -> desc * {
        desc_p2p *pkDesc = new desc_p2p;

        if (!pkDesc->setup(net::connection::create(std::move(socket), MAX_SERVER_INPUT, MAX_SERVER_OUTPUT))) {
            metin2::log::error("Cannot set up accepted peer connection");
            pkDesc->destroy();
            delete pkDesc;
            return nullptr;
        }

        set_desc_.insert(pkDesc);
        ++sockets_connected_;

        metin2::log::info("Accepted peer connection from {}", pkDesc->host_name());
        p2p_manager::instance().register_acceptor(pkDesc);
        return (pkDesc);
    }

    void desc_manager::connect_account(const std::string &login, desc *d) {
        metin2::log::debug("Account {} bound to connection", login.c_str());
        map_login_name_.insert(desc_loginname_map::value_type(login, d));
    }

    void desc_manager::disconnect_account(const std::string &login) {
        metin2::log::debug("Account {} unbound from connection", login.c_str());
        map_login_name_.erase(login);
    }

    void desc_manager::destroy_desc(desc *d, bool erase_from_set) {
        if (erase_from_set)
            set_desc_.erase(d);

        if (d->handshake())
            map_handshake_.erase(d->handshake());

        if (d->handle() != 0) {
            map_handle_.erase(d->handle());
            map_handle_random_key_.erase(d->handle());
        } else
            set_client_desc_.erase((client_desc *)d);

        d->destroy();

        delete d;
        --sockets_connected_;
    }

    void desc_manager::destroy_closed() {
        desc_set::iterator i = set_desc_.begin();

        while (i != set_desc_.end()) {
            desc *d = *i;
            desc_set::iterator ci = i;
            ++i;

            if (d->is_phase(PHASE_CLOSE)) {
                if (d->type() == DESC_TYPE_CONNECTOR) {
                    client_desc *client_desc = (game::protocol::client_desc *)d;

                    if (client_desc->is_retry_when_closed()) {
                        client_desc->reset();
                        continue;
                    }
                }

                destroy_desc(d, false);
                set_desc_.erase(ci);
            }
        }
    }

    auto desc_manager::find_by_login_name(const std::string &login) -> desc * {
        desc_loginname_map::iterator it = map_login_name_.find(login);

        if (map_login_name_.end() == it)
            return nullptr;

        return (it->second);
    }

    auto desc_manager::find_by_handle(uint32_t handle) -> desc * {
        desc_handle_map::iterator it = map_handle_.find(handle);

        if (map_handle_.end() == it)
            return nullptr;

        return (it->second);
    }

    auto desc_manager::client_set() -> const desc_manager::desc_set & {
        return set_desc_;
    }

    struct name_with_desc_func {
        const char *name;

        name_with_desc_func(const char *name_value) : name(name_value) {
        }

        auto operator()(desc *d) -> bool {
            if (d->get_character() && !strcmp(d->get_character()->name(), name))
                return true;

            return false;
        }
    };

    auto desc_manager::find_by_character_name(const char *name) -> desc * {
        desc_set::iterator it = std::find_if(set_desc_.begin(), set_desc_.end(), name_with_desc_func(name));
        return (it == set_desc_.end()) ? nullptr : (*it);
    }

    auto desc_manager::create_connection_desc(const char *host, uint16_t port, int phase_when_succeed, bool retry_when_closed) -> client_desc * {
        client_desc *newd;

        newd = new client_desc;

        newd->setup(host, port);
        newd->connect(phase_when_succeed);
        newd->set_retry_when_closed(retry_when_closed);

        set_desc_.insert(newd);
        set_client_desc_.insert(newd);

        ++sockets_connected_;
        return (newd);
    }

    struct func_try_connect {
        void operator()(desc *d) {
            ((client_desc *)d)->connect();
        }
    };

    void desc_manager::try_connect() {
        func_try_connect f;
        std::for_each(set_client_desc_.begin(), set_client_desc_.end(), f);
    }

    auto desc_manager::is_p2p_desc_exist(const char *host, uint16_t port) -> bool {
        client_desc_set::iterator it = set_client_desc_.begin();

        while (it != set_client_desc_.end()) {
            client_desc *d = *(it++);

            if (!strcmp(d->p2p_host(), host) && d->p2p_port() == port)
                return true;
        }

        return false;
    }

    class func_who {
      public:
        int total_count;
        int empire_user_count[EMPIRE_MAX_NUM];

        func_who() {
            total_count = 0;
            memset(empire_user_count, 0, sizeof(empire_user_count));
        }

        void operator()(desc *d) {
            if (d->get_character()) {
                ++total_count;
                ++empire_user_count[d->empire()];
            }
        }
    };

    void desc_manager::update_local_user_count() {
        const desc_set &c_ref_set = client_set();
        func_who f;
        f = std::for_each(c_ref_set.begin(), c_ref_set.end(), f);

        local_user_count_ = f.total_count;
        std::memcpy(empire_user_count_, f.empire_user_count, sizeof(empire_user_count_));

        empire_user_count_[1] += p2p_manager::instance().empire_user_count(1);
        empire_user_count_[2] += p2p_manager::instance().empire_user_count(2);
        empire_user_count_[3] += p2p_manager::instance().empire_user_count(3);
    }

    void desc_manager::user_count(int &total, int **pai_empire_user_count, int &local_count) {
        *pai_empire_user_count = &empire_user_count_[0];

        int iCount = p2p_manager::instance().count();
        if (iCount < 0) {
            metin2::log::error("Peer user count is negative ({:d})", iCount);
        }
        total = local_user_count_ + iCount;
        local_count = local_user_count_;
    }

    auto desc_manager::make_random_key(uint32_t handle) -> uint32_t {
        uint32_t random_key = thecore_random();
        map_handle_random_key_[handle] = random_key;
        return random_key;
    }

    auto desc_manager::check_random_key(uint32_t handle, uint32_t key) -> bool {
        auto it = map_handle_random_key_.find(handle);
        return it != map_handle_random_key_.end() && it->second == key && find_by_handle(handle) != nullptr;
    }

    auto desc_manager::find_by_login_key(uint32_t key) -> desc * {
        std::map<uint32_t, login_key *>::iterator it = map_login_key_.find(key);

        if (it == map_login_key_.end())
            return nullptr;

        return it->second->desc;
    }

    auto desc_manager::create_login_key(desc *d) -> uint32_t {
        uint32_t dwKey = 0;

        do {
            dwKey = number(1, INT_MAX);

            if (map_login_key_.find(dwKey) != map_login_key_.end())
                continue;

            login_key *pkKey = new login_key(dwKey, d);
            d->set_login_key(pkKey);
            map_login_key_.insert(std::make_pair(dwKey, pkKey));
            break;
        } while (1);

        return dwKey;
    }

    void desc_manager::process_expired_login_key() {
        uint32_t dwCurrentTime = get_dword_time();

        std::map<uint32_t, login_key *>::iterator it, it2;

        it = map_login_key_.begin();

        while (it != map_login_key_.end()) {
            it2 = it++;

            if (it2->second->expire_time == 0)
                continue;

            if (dwCurrentTime - it2->second->expire_time > 60000) {
                delete it2->second;
                map_login_key_.erase(it2);
            }
        }
    }
}
