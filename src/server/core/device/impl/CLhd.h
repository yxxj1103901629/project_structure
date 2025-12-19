#ifndef CLHD_H
#define CLHD_H

#include "../base/IDevice.h"

class CLhd : public IDevice
{
public:
    CLhd();
    ~CLhd() override;

    void init(const nlohmann::json& config) override;
    void process(const std::string& data) override;

private:
    CDeviceAttribute* m_deviceAttribute;
};

#endif // CLHD_H
