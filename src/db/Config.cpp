#include "Config.h"

#include <common/utils.h>
#include <libthecore/log.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

CConfig::~CConfig() {
    Destroy();
}

void CConfig::Destroy() {
    m_valueMap.clear();
}

void CConfig::NextLine(FILE *fp) {
    int c;

    while ((c = fgetc(fp)) != EOF) {
        if (c == '\n') {
            return;
        }
    }
}

auto CConfig::GetWord(FILE *fp, char *tar) -> bool {
    int i = 0;
    int c;

    int semicolon_mode = 0;

    while ((c = fgetc(fp)) != EOF) {
        if (c == 13) {
            continue;
        }

        if (semicolon_mode) {
            if (c == '"') {
                tar[i] = '\0';
                return true;
            }

            tar[i++] = c;
            continue;
        } else {
            if (i == 0) {
                if (c == '"') {
                    semicolon_mode = 1;
                    continue;
                }

                if (c == ' ' || c == '\t' || c == '\n') {
                    continue;
                }
            }

            if ((c == ' ' || c == '\t' || c == '\n')) {
                // tab.
                tar[i] = '\0';
                return true;
            }

            tar[i] = c;
            ++i;
        }
    }

    return (i != 0);
}

auto CConfig::GetLine(FILE *fp, char *dest) -> bool {
    int c;
    int i = 0;
    while ((c = fgetc(fp)) != EOF) {
        if (c == '\n') {
            return true;
        }
        dest[i] = c;
        ++i;
    }
    return true;
}

auto CConfig::LoadFile(const char *filename) -> bool {
    char szTmp[256];
    char comment[256];

    FILE *fp = fopen(filename, "rb");

    if (fp == nullptr) {
        return false;
    }

    int mode = 0;

    while (GetWord(fp, szTmp)) {
        if (strcmp(szTmp, "//") == 0) {
            NextLine(fp);
            continue;
        }

        switch (mode) {
        case 0:
            strlcpy(comment, szTmp, sizeof(comment));
            ++mode;
            break;

        case 1:
            if (*szTmp == '=')
                ++mode;
            break;

        case 2:
            mode = 0;
            m_valueMap.insert(TValueMap::value_type(comment, szTmp));
            break;
        }

        if (mode == 2 && strcmp(comment, "ITEM_ID_RANGE") == 0) {
            GetLine(fp, szTmp);
            m_valueMap.insert(TValueMap::value_type(comment, szTmp));
            mode = 0;
        }
    }

    fclose(fp);
    return true;
}

std::string *CConfig::Search(const char *key) {
    auto i = m_valueMap.find(key);

    if (i == m_valueMap.end()) {
        return nullptr;
    }

    return &i->second;
}

bool CConfig::GetParam(const char *key, int index, uint32_t *Param) {
    std::string *pstStr = Search(key);
    if (!pstStr) {
        return false;
    }

    char szParam[5][32];

    sscanf(pstStr->c_str(), "%s %s %s %s %s", szParam[0], szParam[1], szParam[2], szParam[3], szParam[4]);

    str_to_number(*Param, szParam[index]);

    sys_log(0, "GetParam %d", *Param);
    return true;
}
auto CConfig::Get(const char *key) -> const char * {
    std::string *pstStr = Search(key);

    if (!pstStr) {
        static std::string stTemp = "";
        return stTemp.c_str();
    }

    return pstStr->c_str();
}

auto CConfig::GetValue(const char *key, int *dest) -> bool {
    if (!Search(key)) {
        return false;
    }

    str_to_number(*dest, Get(key));
    return true;
}

auto CConfig::GetValue(const char *key, char *dest, size_t destSize) -> bool {
    if (!Search(key)) {
        return false;
    }

    strlcpy(dest, Get(key), destSize);

    if (!*dest) {
        return false;
    }

    return true;
}

auto CConfig::GetTwoValue(const char *key, uint32_t *dest1, uint32_t *dest2) -> bool {
    if (!GetParam(key, 0, dest1)) {
        return false;
    }

    if (!GetParam(key, 1, dest2)) {
        return false;
    }

    return true;
}
