#ifndef OTA_H
#define OTA_H

#include <Arduino.h>

class OTAUpdater
{
public:
    OTAUpdater();
    void begin();
    void checkForUpdate();
};

#endif // OTA_H