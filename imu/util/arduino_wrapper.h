
#include <cstdint>

class TwoWire
{
public:
    void begin();
    uint8_t requestFrom(uint8_t address, uint8_t quantity);
    void beginTransmission(uint8_t address);
};

class SPIClass
{
public:
    void begin();
    uint8_t requestFrom(uint8_t address, uint8_t quantity);
    void beginTransmission(uint8_t address);
};