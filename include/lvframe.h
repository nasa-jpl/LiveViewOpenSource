#ifndef LVFRAME_H
#define LVFRAME_H

#include <QtGlobal>

#include "constants.h"

struct LVFrame
{
    uint16_t* raw_data;
    uint16_t* dsf_data;
    // float* stdev_data;

    LVFrame(const uint16_t width, const uint16_t height)
    {
        try {
            raw_data = new uint16_t[width * height];
            dsf_data = new uint16_t[width * height];
            // stdev_data = new float[width * height];
        } catch (std::bad_alloc&) {
            qFatal("Not enough memory to allocate frame buffer.");
            throw;
        }

    }
    ~LVFrame()
    {
        delete raw_data;
        delete dsf_data;
        // delete stdev_data;
    }
};

#endif // LVFRAME_H