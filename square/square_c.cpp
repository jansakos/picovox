#include "square_c.h"
#include "square.h"

struct tandy_t {
    tandysound_t device;
};

tandy_t *tandy_create(void)
{
    try {
        return new tandy_t{};
    } catch (...) {
        return nullptr;
    }
}

void tandy_write(tandy_t *tandy, uint8_t data)
{
    if (!tandy)
        return;

    tandy->device.write_register(0xC0, data);
}

int32_t tandy_get_sample(tandy_t *tandy)
{
    if (!tandy)
        return 0;

    int32_t sample = 0;
    tandy->device.generator().generate_frames(&sample, 1);
    return sample;
}

void tandy_destroy(tandy_t *tandy)
{
    if (!tandy)
        return;

    delete tandy;
}