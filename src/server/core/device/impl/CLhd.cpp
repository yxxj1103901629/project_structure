#include "CLhd.h"

CLhd::CLhd() : m_deviceAttribute(nullptr) {}

CLhd::~CLhd() {}

void CLhd::init(const nlohmann::json &config)
{
    (void)config;
}

void CLhd::process(const std::string &data)
{
    (void)data;
}
