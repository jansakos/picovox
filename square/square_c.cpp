#include "square_c.h"
#include "square.h"

// Tandy part
struct tandy_t {
    tandysound_t device;
};

tandy_t *tandy_create(void)
{
    return new tandy_t();
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

//gameblaster part
struct gameblaster_t {
    ::cms_t device;
};

gameblaster_t *gameblaster_create(void) {
    return new gameblaster_t{};
}

void gameblaster_destroy(gameblaster_t *gameblaster) {
    delete gameblaster;
}

void gameblaster_write(gameblaster_t *gameblaster, uint32_t address, uint8_t data) {
    if ((address & 1) == 0)
        gameblaster->device.write_data(address, data);
    else
        gameblaster->device.write_addr(address, data);
}

void gameblaster_get_sample(gameblaster_t *gameblaster, int32_t *left, int32_t *right) {
    int32_t buffer[2] = {0, 0};

    gameblaster->device.generator(0).generate_frames(buffer, 1);
    gameblaster->device.generator(1).generate_frames(buffer, 1);

    *left  = buffer[0];
    *right = buffer[1];
}