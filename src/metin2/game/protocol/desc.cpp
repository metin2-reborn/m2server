#include <metin2/config/config.h>
#include <metin2/game/protocol/desc.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/protocol/sequence.h>
#include <metin2/game/text/locale.hpp>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/net/connection.h>
#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <asio/error.hpp>

#include <cassert>
#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <span>
#include <system_error>
#include <utility>

#include <arpa/inet.h>

namespace metin2::game::inline protocol {

    desc::desc() {
        initialize();
    }

    desc::~desc() {
    }

    void desc::initialize() {
        destroyed_ = false;

        input_processor_ = nullptr;
        connection_.reset();
        phase_ = PHASE_CLOSE;
        handle_ = 0;

        port_ = 0;
        last_try_to_connect_time_ = 0;

        handshake_ = 0;
        handshake_sent_time_ = 0;
        handshake_retry_ = 0;
        client_time_ = 0;
        handshaking_ = false;

        buffered_output_.clear();

        ping_event_ = nullptr;
        character_ = nullptr;
        memset(&account_table_, 0, sizeof(account_table_));

        log_file_ = nullptr;

        p2_p_port_ = 0;

        admin_mode_ = false;
        mark_login_ = false;
        mark_owner_handle_ = 0;
        pong_ = true;
        channel_status_requested_ = false;

        current_sequence_ = 0;

        m_pk_login_key_ = nullptr;
        login_key_ = 0;

        crc_magic_cube_idx_ = 0;
        proc_crc_ = 0;
        file_crc_ = 0;

        disconnect_event = nullptr;

        seq_vector.clear();
    }

    void desc::destroy() {
        if (destroyed_) {
            return;
        }
        destroyed_ = true;

        if (m_pk_login_key_)
            m_pk_login_key_->expire();

        if (get_account_table().id)
            desc_manager::instance().disconnect_account(get_account_table().login);

        if (log_file_) {
            fclose(log_file_);
            log_file_ = nullptr;
        }

        if (character_) {
            character_->disconnect("connection closed");
            character_ = nullptr;
        }

        event_cancel(&ping_event_);
        event_cancel(&disconnect_event);

        if (!metin2::config::is_auth()) {
            if (account_table_.login[0] && account_table_.passwd[0]) {
                logout_packet pack;

                strlcpy(pack.login, account_table_.login, sizeof(pack.login));
                strlcpy(pack.passwd, account_table_.passwd, sizeof(pack.passwd));

                db_clientdesc->db_packet(HEADER_GD_LOGOUT, handle_, &pack, sizeof(logout_packet));
            }
        }

        if (connection_) {
            metin2::log::info("Closing connection of {} (handle {:d})", host_.c_str(), handle_);

            cipher_.clean_up();

            connection_->close();
            connection_.reset();
        }

        seq_vector.clear();
    }

    EVENTFUNC(ping_event) {
        desc::desc_event_info *info = dynamic_cast<desc::desc_event_info *>(event->info);

        if (info == nullptr) {
            metin2::log::error("Ping event has no info");
            return 0;
        }

        desc *desc = info->desc;

        if (desc->is_admin_mode())
            return (passes_per_sec(metin2::config::server().ping_seconds));

        if (!desc->is_pong()) {
            metin2::log::info("No ping reply from {}, closing connection", desc->host_name());

            desc->set_phase(PHASE_CLOSE);

            return (passes_per_sec(metin2::config::server().ping_seconds));
        } else {
            packet_gc_ping p;
            p.header = HEADER_GC_PING;
            desc->packet(&p, sizeof(struct packet_gc_ping));
            desc->set_pong(false);
        }

        desc->send_handshake(get_dword_time(), 0);

        return (passes_per_sec(metin2::config::server().ping_seconds));
    }

    auto desc::is_pong() -> bool {
        return pong_;
    }

    void desc::set_pong(bool b) {
        pong_ = b;
    }

    auto desc::setup(std::shared_ptr<net::connection> connection, uint32_t handle, uint32_t handshake) -> bool {
        connection_ = std::move(connection);
        host_ = connection_->remote_address();
        port_ = connection_->remote_port();
        handle_ = handle;

        connection_->start(*this);

        desc_event_info *info = alloc_event_info<desc_event_info>();

        info->desc = this;
        assert(ping_event_ == nullptr);

        ping_event_ = event_create(ping_event, info, passes_per_sec(metin2::config::server().ping_seconds));

        set_phase(PHASE_HANDSHAKE);
        start_handshake(handshake);

        metin2::log::info("New connection from {} (handle {:d}, handshake {:d}, {})",
            host_.c_str(), handle_, handshake_, static_cast<const void *>(this));

        return true;
    }

    auto desc::on_receive(std::span<std::uint8_t> input, std::size_t received) -> std::size_t {
        if (cipher_.activated())
            cipher_.decrypt(input.data() + input.size() - received, received);

        std::size_t consumed = 0;

        while (input_processor_) {
            int iBytesProceed = 0;
            const bool bDone = input_processor_->process(this, input.data() + consumed, (int)(input.size() - consumed), iBytesProceed);
            consumed += iBytesProceed;

            if (bDone)
                return consumed;
        }

        return input.size();
    }

    void desc::on_close(const std::error_code &error) {
        if (error && error != asio::error::eof)
            metin2::log::info("Connection of {} lost: {}", host_.c_str(), error.message().c_str());

        set_phase(PHASE_CLOSE);
    }

    void desc::buffered_packet(const void *data_value, int size) {
        if (phase_ == PHASE_CLOSE)
            return;

        const auto *data = static_cast<const uint8_t *>(data_value);
        buffered_output_.insert(buffered_output_.end(), data, data + size);
    }

    void desc::packet(const void *data_value, int size_value) {
        assert(size_value > 0);

        if (phase_ == PHASE_CLOSE || !connection_)
            return;

        if (relay_name_.length() != 0) {
            packet_gg_relay p;

            p.header = HEADER_GG_RELAY;
            strlcpy(p.name, relay_name_.c_str(), sizeof(p.name));
            p.size = size_value;

            relay_name_.clear();

            if (!connection_->send(&p, sizeof(p)) || !connection_->send(data_value, size_value))
                phase_ = PHASE_CLOSE;

            return;
        }

        if (!buffered_output_.empty()) {
            const auto *data = static_cast<const uint8_t *>(data_value);
            buffered_output_.insert(buffered_output_.end(), data, data + size_value);

            data_value = buffered_output_.data();
            size_value = (int)buffered_output_.size();
        }

        const std::span<uint8_t> out = connection_->prepare(size_value);

        if (out.empty()) {
            metin2::log::warn("Output buffer of {} exceeds its limit, closing connection", host_.c_str());
            phase_ = PHASE_CLOSE;
        } else {
            std::memcpy(out.data(), data_value, size_value);

            if (cipher_.activated())
                cipher_.encrypt(out.data(), out.size());
        }

        buffered_output_.clear();
    }

    void desc::large_packet(const void *data, int size) {
        if (connection_)
            connection_->raise_output_limit(size);

        metin2::log::debug("Sending large packet ({:d} bytes)", size);

        packet(data, size);
    }

    void desc::set_phase(int phase_value) {
        phase_ = phase_value;

        packet_gc_phase pack;
        pack.header = HEADER_GC_PHASE;
        pack.phase = phase_value;
        packet(&pack, sizeof(packet_gc_phase));

        switch (phase_) {
        case PHASE_CLOSE:
            input_processor_ = &input_close_;
            break;

        case PHASE_HANDSHAKE:
            input_processor_ = &input_handshake_;
            break;

        case PHASE_SELECT:
        case PHASE_LOGIN:
        case PHASE_LOADING:
            input_processor_ = &input_login_;
            break;

        case PHASE_GAME:
        case PHASE_DEAD:
            input_processor_ = &input_main_;
            break;

        case PHASE_AUTH:
            input_processor_ = &input_auth_;
            metin2::log::debug("Connection {} entered auth phase", static_cast<const void *>(this));
            break;
        }
    }

    void desc::bind_account_table(account_table *account_table_value) {
        assert(account_table_value != nullptr);

        // A second login on this connection must not leave the login map pointing here.
        if (account_table_.id && strcmp(account_table_.login, account_table_value->login) != 0)
            desc_manager::instance().disconnect_account(account_table_.login);

        std::memcpy(&account_table_, account_table_value, sizeof(account_table));
        desc_manager::instance().connect_account(account_table_.login, this);
    }

    void desc::start_handshake(uint32_t handshake) {
        handshake_ = handshake;

        send_handshake(get_dword_time(), 0);

        handshake_retry_ = 0;
    }

    void desc::send_handshake(uint32_t cur_time, int32_t new_delta) {
        packet_gc_handshake pack;

        pack.header = HEADER_GC_HANDSHAKE;
        pack.handshake = handshake_;
        pack.time = cur_time;
        pack.delta = new_delta;

        packet(&pack, sizeof(packet_gc_handshake));

        handshake_sent_time_ = cur_time;
        handshaking_ = true;
    }

    auto desc::handshake_process(uint32_t time, int32_t delta, bool infinite_retry) -> bool {
        uint32_t dwCurTime = get_dword_time();

        if (delta < 0) {
            metin2::log::warn("Negative handshake delta {:d} from {}", delta, host_.c_str());
            return false;
        }

        int bias = (int)(dwCurTime - (time + delta));

        if (bias >= 0 && bias <= 50) {
            if (infinite_retry) {
                uint8_t bHeader = HEADER_GC_TIME_SYNC;
                packet(&bHeader, sizeof(uint8_t));
            }

            if (get_character())
                metin2::log::debug("Handshake done: client time {:d}, server time {:d}, player {}", client_time_, dwCurTime, get_character()->name());
            else
                metin2::log::debug("Handshake done: client time {:d}, server time {:d}, delta {:d}", client_time_, dwCurTime, static_cast<long>(delta));

            client_time_ = dwCurTime;
            handshaking_ = false;
            return true;
        }

        int32_t lNewDelta = (int32_t)(dwCurTime - time) / 2;

        if (lNewDelta < 0) {
            metin2::log::debug("Handshake: negative delta {:d}, using send time instead", lNewDelta);
            lNewDelta = (dwCurTime - handshake_sent_time_) / 2;
        }

        metin2::log::debug("Handshake retry: server time {:d}, client time {:d}, delta {:d}, sent at {:d}, new delta {:d}", dwCurTime, time, delta, handshake_sent_time_, lNewDelta);

        if (!infinite_retry)
            if (++handshake_retry_ > HANDSHAKE_RETRY_LIMIT) {
                metin2::log::warn("Handshake retry limit {:d} reached (player {}), closing connection",
                    HANDSHAKE_RETRY_LIMIT, get_character() ? get_character()->name() : "none");
                set_phase(PHASE_CLOSE);
                return false;
            }

        send_handshake(dwCurTime, lNewDelta);
        return false;
    }

    auto desc::is_handshaking() -> bool {
        return handshaking_;
    }

    auto desc::client_time() -> uint32_t {
        return client_time_;
    }

    void desc::send_key_agreement() {
        packet_key_agreement packet_value;

        size_t data_length = packet_key_agreement::max_data_len;
        size_t agreed_length = cipher_.prepare(packet_value.data, &data_length);
        if (agreed_length == 0) {
            set_phase(PHASE_CLOSE);
            return;
        }
        assert(data_length <= packet_key_agreement::max_data_len);

        packet_value.header = HEADER_GC_KEY_AGREEMENT;
        packet_value.agreed_length = (uint16_t)agreed_length;
        packet_value.data_length = (uint16_t)data_length;

        packet(&packet_value, sizeof(packet_value));
    }

    void desc::send_key_agreement_completed() {
        packet_key_agreement_completed packet_value;

        packet_value.header = HEADER_GC_KEY_AGREEMENT_COMPLETED;

        packet(&packet_value, sizeof(packet_value));
    }

    auto desc::finish_handshake(size_t agreed_length, const void *buffer, size_t length) -> bool {
        if (!cipher_.activate(false, agreed_length, buffer, length))
            return false;

        // The completion packet already went out in the clear; the client switches on when it arrives.
        cipher_.set_activated(true);
        return true;
    }

    auto desc::is_cipher_prepared() -> bool {
        return cipher_.is_key_prepared();
    }

    void desc::set_relay(const char *name) {
        relay_name_ = name;
    }

    void desc::bind_character(character *ch) {
        character_ = ch;
    }

    void desc::send_output_now() {
        if (connection_)
            connection_->write_now();
    }

    void desc::flush_output() {
        if (!connection_ || connection_->pending_output() == 0)
            return;

        metin2::log::debug("Flushing {:d} bytes of output", connection_->pending_output());

        if (connection_->flush(std::chrono::seconds(20)))
            metin2::log::debug("Output flushed");
        else
            metin2::log::warn("Cannot flush output of {} within 20 seconds", host_.c_str());
    }

    EVENTFUNC(disconnect_event) {
        desc::desc_event_info *info = dynamic_cast<desc::desc_event_info *>(event->info);

        if (info == nullptr) {
            metin2::log::error("Disconnect event has no info");
            return 0;
        }

        desc *d = info->desc;

        d->disconnect_event = nullptr;
        d->set_phase(PHASE_CLOSE);
        return 0;
    }

    auto desc::delayed_disconnect(int sec) -> bool {
        if (disconnect_event != nullptr) {
            return false;
        }

        desc_event_info *info = alloc_event_info<desc_event_info>();
        info->desc = this;

        disconnect_event = event_create(game::protocol::disconnect_event, info, passes_per_sec(sec));
        return true;
    }

    void desc::disconnect_of_same_login() {
        if (get_character()) {
            if (disconnect_event)
                return;

            get_character()->chat_packet(CHAT_TYPE_INFO, lc_text("다른 컴퓨터에서 로그인 하여 접속을 종료 합니다."));
            delayed_disconnect(5);
        } else {
            set_phase(PHASE_CLOSE);
        }
    }

    void desc::set_admin_mode() {
        admin_mode_ = true;
    }

    auto desc::is_admin_mode() -> bool {
        return admin_mode_;
    }

    auto desc::sequence() -> uint8_t {
        return gc_ab_sequence[current_sequence_];
    }

    void desc::set_next_sequence() {
        if (++current_sequence_ == SEQUENCE_MAX_NUM)
            current_sequence_ = 0;
    }

    void desc::send_login_success_packet() {
        account_table &rTable = get_account_table();

        packet_gc_login_success p;

        p.header = HEADER_GC_LOGIN_SUCCESS_NEWSLOT;

        p.handle = handle();
        p.random_key = desc_manager::instance().make_random_key(handle());
        std::memcpy(p.players, rTable.players, sizeof(rTable.players));

        for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i) {

            if (!proxy_ip.empty())
                rTable.players[i].addr = inet_addr(proxy_ip.c_str());

            guild *g = guild_manager::instance().linked_guild(rTable.players[i].id);

            if (g) {
                p.guild_id[i] = g->id();
                strlcpy(p.guild_name[i], g->name(), sizeof(p.guild_name[i]));
            } else {
                p.guild_id[i] = 0;
                p.guild_name[i][0] = '\0';
            }
        }

        packet(&p, sizeof(packet_gc_login_success));
    }

    void desc::set_login_key(uint32_t key) {
        login_key_ = key;
    }

    void desc::set_login_key(login_key *key_value) {
        // The replaced key must not keep pointing at this connection.
        if (m_pk_login_key_ && m_pk_login_key_ != key_value)
            m_pk_login_key_->expire();

        m_pk_login_key_ = key_value;
        metin2::log::debug("Login key {:d} bound to connection", m_pk_login_key_->key);
    }

    auto desc::get_login_key() -> uint32_t {
        if (m_pk_login_key_)
            return m_pk_login_key_->key;

        return login_key_;
    }

    void desc::assemble_crc_magic_cube(uint8_t proc_piece, uint8_t file_piece) {
        static uint8_t abXORTable[32] =
            {
                102,
                30,
                0,
                0,
                0,
                0,
                0,
                0,
                188,
                44,
                0,
                0,
                0,
                0,
                0,
                0,
                39,
                201,
                0,
                0,
                0,
                0,
                0,
                0,
                43,
                5,
                0,
                0,
                0,
                0,
                0,
                0,
        };

        proc_piece = (proc_piece ^ abXORTable[crc_magic_cube_idx_]);
        file_piece = (file_piece ^ abXORTable[crc_magic_cube_idx_ + 1]);

        proc_crc_ |= static_cast<uint32_t>(proc_piece) << crc_magic_cube_idx_;
        file_crc_ |= static_cast<uint32_t>(file_piece) << crc_magic_cube_idx_;

        crc_magic_cube_idx_ += 8;

        if (!(crc_magic_cube_idx_ & 31)) {
            proc_crc_ = 0;
            file_crc_ = 0;
            crc_magic_cube_idx_ = 0;
        }
    }

    void desc::push_seq(uint8_t hdr, uint8_t seq) {
        if (seq_vector.size() >= 20) {
            seq_vector.erase(seq_vector.begin());
        }

        seq_t info = {hdr, seq};
        seq_vector.push_back(info);
    }

    auto desc::empire() -> uint8_t {
        return account_table_.empire;
    }

    void desc::chat_packet(uint8_t type, const char *format, ...) {
        char chatbuf[CHAT_MAX_LEN + 1];
        va_list args;

        va_start(args, format);
        int len = vsnprintf(chatbuf, sizeof(chatbuf), format, args);
        va_end(args);

        if (len < 0)
            return;

        if (len >= (int)sizeof(chatbuf)) {
            len = (int)utf8_truncated_size(chatbuf, sizeof(chatbuf) - 1, sizeof(chatbuf) - 1);
            chatbuf[len] = '\0';
        }

        struct packet_gc_chat pack_chat;

        pack_chat.header = HEADER_GC_CHAT;
        pack_chat.size = sizeof(struct packet_gc_chat) + len;
        pack_chat.type = type;
        pack_chat.id = 0;
        pack_chat.empire = empire();

        temp_buffer buf;
        buf.write(&pack_chat, sizeof(struct packet_gc_chat));
        buf.write(chatbuf, len);

        packet(buf.read_peek(), buf.size());
    }
}
