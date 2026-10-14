#pragma once

#include <common/singleton.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>

using TValueMap = std::map<std::string, std::string>;

class CConfig : public singleton<CConfig> {
  public:
    CConfig() = default;

    ~CConfig() override;

    auto LoadFile(const char *filename) -> bool;
    auto GetValue(const char *key, int *dest) -> bool;
    auto GetValue(const char *key, char *dest, size_t destSize) -> bool;
    auto GetWord(FILE *fp, char *dest) -> bool;
    auto GetLine(FILE *fp, char *dest) -> bool;
    auto GetTwoValue(const char *key, uint32_t *dest1, uint32_t *dest2) -> bool;
    void NextLine(FILE *fp);

  private:
    void Destroy();
    auto GetParam(const char *key, int index, uint32_t *Param) -> bool;

    auto Get(const char *key) -> const char *;
    auto Search(const char *key) -> std::string *;

  private:
    TValueMap m_valueMap;
};
