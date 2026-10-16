#pragma once

#include <metin2/game/app/typedef.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/input.h>

#include <cstdint>
#include <ctime>

namespace metin2::game::inline protocol {

    class client_desc : public desc {
      public:
        client_desc();
        ~client_desc() override;

        auto type() -> uint8_t override { return DESC_TYPE_CONNECTOR; }
        void destroy() override;
        void set_phase(int phase) override;

        auto connect(int phase_when_succeed = 0) -> bool;
        void setup(const char *host, uint16_t port);

        void set_retry_when_closed(bool);

        void db_packet_header(uint8_t header, uint32_t handle, uint32_t size);
        void db_packet(uint8_t header, uint32_t handle, const void *data, uint32_t size);
        void packet(const void *data, int size);
        auto is_retry_when_closed() -> bool;

        void update(uint32_t t);
        void update_channel_status(uint32_t t, bool force);

        void reset();

      protected:
        int phase_when_succeed_;
        bool retry_when_closed_;
        time_t last_try_to_connect_time_;
        time_t t_last_channel_status_update_time_;

        input_db input_db_;
        input_p2_p input_p2_p_;
    };

    extern client_desc *db_clientdesc;
    extern client_desc *auth_master_desc;
    extern client_desc *netmarble_db_desc;
}
