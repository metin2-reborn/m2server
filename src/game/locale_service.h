#pragma once

#include <string>

namespace metin2::game {

bool LocaleService_Init(const std::string& c_rstServiceName);
void LocaleService_LoadLocaleStringFile();
void LocaleService_LoadEmpireTextConvertTables();
void LocaleService_TransferDefaultSetting();
const std::string& LocaleService_GetBasePath();
const std::string& LocaleService_GetMapPath();
const std::string& LocaleService_GetQuestPath();

int check_name(const char * str);
}
