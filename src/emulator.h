#pragma once

#include <unordered_map>
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

    emulator() {
        write_rom_page(0, 0, rom128p0, sizeof(rom128p0));
        write_rom_page(1, 0, rom128p1, sizeof(rom128p1));
        write_rom_page(2, 0, romtrdos, sizeof(romtrdos));
    }

    void write_ram(int addr, const char *data, std::size_t len) {
        for (fast_u16 i = 0; i != len; ++i)
            base::set_memory_byte(addr + i, data[i]);
    }

    void set_port_value(const fast_u16 port, const fast_u8 value) {
        port_values[port & 0xFFFF] = value & 0xFF;
    }

    char *process_frame() {
        base::run();
        render_screen();
        return (char *)window_pixels;
    }

    fast_u8 on_input(fast_u16 addr) {
        return port_values.count(addr) ? port_values[addr]
                                       : base::on_input(addr);
    }
  private:
    base::pixels_buffer_type pixels_buffer;
    base::pixels_buffer_type *window_pixels = &pixels_buffer;

    static const unsigned num_of_keyboard_ports = 8;
    typedef least_u8 keyboard_state_type[num_of_keyboard_ports];
    keyboard_state_type keyboard_state;
    std::unordered_map<fast_u16, fast_u8> port_values;

    void render_screen() {
        base::render_screen();
        base::get_frame_pixels(*window_pixels);
    }
};
