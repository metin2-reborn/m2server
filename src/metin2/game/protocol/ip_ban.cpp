
#include <metin2/game/protocol/ip_ban.h>

#include <metin2/common/utils.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>

namespace metin2::game::inline protocol {

    class ip {
      public:
        ip(const ip &r) {
            start_ = r.start_;
            end_ = r.end_;
            mask_ = r.mask_;
        }

        ip(const char *start_value, const char *end_value = nullptr) {
            uint8_t start[4];
            uint8_t end[4];
            uint8_t mask[4];

            read(start_value, start);

            if (end_value && *end_value)
                read(end_value, end);
            else
                memcpy(end, start, sizeof(uint8_t) * 4);

            mask[0] = 255 - (start[0] ^ end[0]);
            mask[1] = 255 - (start[1] ^ end[1]);
            mask[2] = 255 - (start[2] ^ end[2]);
            mask[3] = 255 - (start[3] ^ end[3]);

            start_ = (start[3] << 24) | (start[2] << 16) | (start[1] << 8) | start[0];
            end_ = (end[3] << 24) | (end[2] << 16) | (end[1] << 8) | end[0];
            mask_ = (mask[3] << 24) | (mask[2] << 16) | (mask[1] << 8) | mask[0];

            print();
        }

        ip(struct in_addr in) {
            start_ = in.s_addr;
            end_ = start_;
            mask_ = 4294967295UL;
        }

        auto is_child_of(ip &r) -> bool {
            if ((r.start_ & r.mask_) != (start_ & r.mask_))
                return false;

            uint32_t m = r.mask_ | mask_;

            return (start_ & ~m) == (start_ & ~mask_) && (end_ & ~m) == (end_ & ~mask_);
        }

        auto hash() -> int {
            return (start_ & 0x000000FF);
        }

        void print() {
            struct in_addr in_ip, in_mask, in_end;

            in_ip.s_addr = start_;
            in_mask.s_addr = mask_;
            in_end.s_addr = end_;

            fprintf(stderr, "\t%s", inet_ntoa(in_ip));
            fprintf(stderr, "\t%s", inet_ntoa(in_end));
            fprintf(stderr, "\t%s\tfirst %d\n", inet_ntoa(in_mask), hash());
        }

      protected:
        auto read(const char *s, uint8_t *dest) -> bool {
            uint8_t class_value = 0;
            const char *p = s;

            while (class_value < 3) {
                char num[4];
                char *dot = const_cast<char *>(strchr(p, '.'));

                if (!dot)
                    break;

                strlcpy(num, p, sizeof(num));

                str_to_number(dest[class_value++], num);
                p = dot + 1;
            }

            if (class_value != 3) {
                fprintf(stderr, "error reading start %s\n", s);
                return false;
            }

            str_to_number(dest[class_value], p);
            return true;
        }

        uint32_t start_;
        uint32_t end_;
        uint32_t mask_;
    };

    std::map<int, std::vector<ip>> ban_ip;

    auto load_ban_ip(const char *filename) -> bool {
        FILE *fp = fopen(filename, "r");

        if (!fp)
            return false;

        char buf[256];
        std::string start;
        std::string end;

        fprintf(stderr, "LOADING BANNED IP LIST\n");

        while (fgets(buf, 256, fp)) {
            *strchr(buf, '\n') = '\0';
            char *p = strchr(buf, '\t');

            if (!p) {
                start = buf;
                end.clear();
            } else {
                char *p2 = strchr(p + 1, '\t');

                if (p2)
                    *p2 = '\0';

                end = p + 1;

                *p = '\0';
                start = buf;
            }

            ip ip(start.c_str(), end.c_str());

            auto it = ban_ip.find(ip.hash());

            if (it == ban_ip.end()) {
                std::vector<game::protocol::ip> v;
                v.push_back(ip);
                ban_ip.insert(std::map<uint32_t, std::vector<game::protocol::ip>>::value_type(ip.hash(), v));
            } else
                it->second.push_back(ip);
        }

        fclose(fp);

        return true;
    }

    auto is_ban_ip(struct in_addr in) -> bool {
        ip ip(in);

        auto it = ban_ip.find(ip.hash());

        if (it == ban_ip.end())
            return false;

        auto it2 = it->second.begin();

        while (it2 != it->second.end())
            if (ip.is_child_of(*(it2++)))
                return true;

        return false;
    }
}
