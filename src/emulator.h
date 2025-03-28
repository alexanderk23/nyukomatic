#pragma once

#include <z80.h>

#include "pentagon128.h"
#include "roms/128p0.h"
#include "roms/128p1.h"
#include "roms/trdos.h"

using z80::fast_u16;
using z80::fast_u8;
using z80::least_u16;
using z80::least_u32;
using z80::least_u8;

class emulator : public zx::pentagon128<emulator> {
  public:
    typedef zx::pentagon128<emulator> base;
    // typedef typename base::pixels_buffer_type window_pixels_type;

    emulator() {
        // window_pixels = static_cast<window_pixels_type *>(std::malloc(sizeof(window_pixels_type)));
        // if (!window_pixels)
        //     LOG_E << "emulator: not enough memory" << std::endl;

        for (fast_u16 i = 0; i < sizeof(rom128p0); i++) {
            base::set_rom_page_byte(0, i, rom128p0[i]);
            base::set_rom_page_byte(1, i, rom128p1[i]);
            base::set_rom_page_byte(2, i, romtrdos[i]);
        }
    }

    // ~emulator() {}

    void write_ram(int addr, const char *data, std::size_t len) {
        for (fast_u16 i = 0; i != len; ++i)
            base::set_memory_byte(addr + i, data[i]);
    }

    void write_ram_page(const int page, const char *data) {
        for (fast_u16 i = 0; i < 16384; i++)
            base::set_ram_page_byte(page, i, data[i]);
    }

    char *process_frame() {
        base::run();
        render_screen();
        return (char *)window_pixels;
    }

  private:
    // window_pixels_type *window_pixels;
    base::pixels_buffer_type pixels_buffer;
    base::pixels_buffer_type *window_pixels = &pixels_buffer;

    static const unsigned num_of_keyboard_ports = 8;
    typedef least_u8 keyboard_state_type[num_of_keyboard_ports];
    keyboard_state_type keyboard_state;

    void render_screen() {
        base::render_screen();
        base::get_frame_pixels(*window_pixels);
    }
};
