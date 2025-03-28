
/*  ZX Spectrum Emulator.
    https://github.com/kosarev/zx

    Copyright (C) 2017-2021 Ivan Kosarev.
    ivan@kosarev.info

    Published under the MIT license.
*/

#include <cstring>
#include <z80.h>

namespace zx {

using z80::fast_u16;
using z80::fast_u32;
using z80::fast_u8;
using z80::least_u16;
using z80::least_u8;

using z80::dec16;
using z80::inc16;
using z80::make16;
using z80::mask16;
using z80::unreachable;

template <typename T>
T non_constexpr() {
    return T();
}

template <typename T>
constexpr T div_exact(T a, T b) {
    return a % b == 0 ? a / b : non_constexpr<T>();
}

template <typename T>
constexpr T div_ceil(T a, T b) {
    return (a + b - 1) / b;
}

template <typename T>
constexpr bool is_multiple_of(T a, T b) {
    return b != 0 && a % b == 0;
}

template <typename T>
constexpr bool round_up(T a, T b) {
    return div_ceil(a, b) * b;
}

typedef fast_u32 events_mask;
const events_mask no_events = 0;
const events_mask machine_stopped = 1u << 0; // TODO: Eliminate.
const events_mask end_of_frame = 1u << 1;
const events_mask ticks_limit_hit = 1u << 2;
const events_mask fetches_limit_hit = 1u << 3;
const events_mask breakpoint_hit = 1u << 4;
const events_mask custom_event = 1u << 31;

typedef fast_u8 memory_marks;
const memory_marks no_marks = 0;
const memory_marks breakpoint_mark = 1u << 0;
const memory_marks visited_instr_mark = 1u << 7;

static const unsigned memory_page_size = 0x4000; // 16K bytes.
typedef least_u8 memory_page_type[memory_page_size];
const unsigned ram_pages_count = 8;
const unsigned rom_pages_count = 4;

typedef fast_u8 ay_regs_type[16];

template <typename D>
class pentagon128 : public z80::z80_cpu<D> {
  public:
    typedef z80::z80_cpu<D> base;
    typedef fast_u32 ticks_type;

    pentagon128() {
        on_reset_memory();
    }

    events_mask get_events() const {
        return events;
    }

    void stop() {
        events |= machine_stopped;
    }

    void on_tick(unsigned t) {
        ticks_since_int += t;

        // Handle stopping by hitting a specified number of ticks.
        if (ticks_to_stop) {
            if (ticks_to_stop > t) {
                ticks_to_stop -= t;
            } else {
                ticks_to_stop = 0;
                events |= ticks_limit_hit;
            }
        }
    }

    ticks_type get_ticks() const {
        return ticks_since_int;
    }

    void set_memory_byte(fast_u16 addr, fast_u8 n) {
        assert(addr < z80::address_space_size);
        // self().on_get_memory()[addr] = static_cast<least_u8>(n);
        auto slot = (addr >> 14) & 3;
        auto val = static_cast<least_u8>(n);
        if (slot == 0) {
            // std::printf("rom write %02x at addr %04x, slot %d, pc=%04x\n", val, addr, slot, self().get_pc());
            // rom_pages[memory_slots[0]][addr & 0x3fff] = val;
        } else {
            ram_pages[memory_slots[slot]][addr & 0x3fff] = val;
        }
    }

    void set_rom_page_byte(fast_u8 page, fast_u16 addr, fast_u8 n) {
        rom_pages[page & 3][addr & 0x3fff] = static_cast<least_u8>(n);
    }

    void set_ram_page_byte(fast_u8 page, fast_u16 addr, fast_u8 n) {
        ram_pages[page & 7][addr & 0x3fff] = static_cast<least_u8>(n);
    }

    fast_u8 on_read(fast_u16 addr) {
        assert(addr < z80::address_space_size);
        // return self().on_get_memory()[addr];
        auto slot = (addr >> 14) & 3;
        if (slot == 0) {
            auto res = rom_pages[memory_slots[0]][addr & 0x3fff];
            return res;
        } else {
            auto res = ram_pages[memory_slots[slot]][addr & 0x3fff];
            return res;
        }
    }

    fast_u8 on_read_page(fast_u8 page, fast_u16 addr) {
        assert(page < ram_pages_count);
        return ram_pages[page][addr & 0x3fff];
    }

    void on_write(fast_u16 addr, fast_u8 n) {
        // Do not alter ROM.
        if (addr >= 0x4000) {
            set_memory_byte(addr, n);
        }
    }

    void handle_contention() {
        return; // pentagon has no contention
    }

    void handle_memory_contention(fast_u16 addr) {
        if (addr >= 0x4000 && addr < 0x8000)
            handle_contention();
    }

    fast_u8 on_fetch_cycle() {
        handle_memory_contention(self().get_pc());
        return base::on_fetch_cycle();
    }

    fast_u8 on_m1_fetch_cycle() {
        // Handle stopping by hitting a specified number of fetches.
        // TODO: Rename fetches_to_stop -> m1_fetches_to_stop.
        if (fetches_to_stop && --fetches_to_stop == 0)
            events |= fetches_limit_hit;

        return base::on_m1_fetch_cycle();
    }

    fast_u8 on_read_cycle(fast_u16 addr) {
        handle_memory_contention(addr);
        return base::on_read_cycle(addr);
    }

    void on_write_cycle(fast_u16 addr, fast_u8 n) {
        // TODO: Render to (current_tick + 1) and then update
        // the byte as the new value is sampled at
        // the 2nd tick of the output cycle.
        // TODO: The "+ 1" thing is still wrong as there may
        // be contentions in the middle.
        render_screen_to_tick(get_ticks() + 1);

        handle_memory_contention(addr);
        base::on_write_cycle(addr, n);
    }

    void handle_contention_tick() {
        handle_memory_contention(addr_bus_value);
        on_tick(1);
    }

    void on_read_cycle_extra_1t() {
        handle_contention_tick();
    }

    void on_read_cycle_extra_2t() {
        handle_contention_tick();
        handle_contention_tick();
    }

    void on_write_cycle_extra_2t() {
        handle_contention_tick();
        handle_contention_tick();
    }

    void handle_port_contention(fast_u16 addr) {
        if (addr < 0x4000 || addr >= 0x8000) {
            if ((addr & 1) == 0) {
                on_tick(1);
                handle_contention();
                on_tick(3);
            } else {
                on_tick(4);
            }
        } else {
            if ((addr & 1) == 0) {
                handle_contention();
                on_tick(1);
                handle_contention();
                on_tick(3);
            } else {
                handle_contention();
                on_tick(1);
                handle_contention();
                on_tick(1);
                handle_contention();
                on_tick(1);
                handle_contention();
                on_tick(1);
            }
        }
    }

    fast_u8 on_input_cycle(fast_u16 addr) {
        handle_port_contention(addr);
        fast_u8 n = self().on_input(addr);
        return n;
    }

    bool is_marked_addr(fast_u16 addr, memory_marks marks) const {
        return (memory_marks[mask16(addr)] & marks) != 0;
    }

    bool is_breakpoint_addr(fast_u16 addr) const {
        return is_marked_addr(addr, breakpoint_mark);
    }

    void mark_addr(fast_u16 addr, memory_marks marks) {
        addr = mask16(addr);
        memory_marks[addr] = static_cast<least_u8>(memory_marks[addr] | marks);
    }

    void mark_addrs(fast_u16 addr, fast_u16 size, memory_marks marks) {
        for (fast_u16 i = 0; i != size; ++i)
            mark_addr(addr + i, marks);
    }

    void on_set_pc(fast_u16 pc) {
        if (dos_active && (pc > 0x3fff)) {
            dos_active = false;
            update_memory_map();
        } else if (!dos_active && ((pc & 0xff00) == 0x3d00)) { // 3d00..3dff
            dos_active = true;
            update_memory_map();
        }

        // Catch breakpoints.
        if (is_breakpoint_addr(pc))
            events |= breakpoint_hit;

        base::on_set_pc(pc);
    }

    fast_u8 on_input(fast_u16 addr) {
        z80::unused(addr);
        return 0xff;
    }

    void update_memory_map() {
        auto rom_page = (port_7ffd_value >> 4) &
                        1; // rom 0 - BASIC128, 1 - BASIC48, 2 - TRDOS
        if (dos_active && (rom_page == 1))
            rom_page = 2;
        memory_slots[0] = rom_page;
        memory_slots[3] = port_7ffd_value & 7; // ram page
        visible_screen_page = ((port_7ffd_value & 0b1000) == 0) ? 5 : 7;
        if (port_7ffd_value & 0b100000)
            memory_locked = true;
    }

    void set_7ffd(fast_u8 n) {
        port_7ffd_value = n;
        update_memory_map();
    }

    void set_border(fast_u8 n) {
        border_color = n & 0x7;
    }

    void on_output_cycle(fast_u16 addr, fast_u8 n) {
        // handle port $7FFD, partial decoding
        if (!memory_locked && ((addr & 0x8002) == 0)) {
            render_screen_to_tick(get_ticks() + 1);
            set_7ffd(n);
        }

        // handle port $FE, partial decoding
        if ((addr & 0x0001) == 0) {
            // TODO: Render to (current_tick + 1) and then update
            // the border color as the new value is sampled at
            // the 2nd tick of the output cycle.
            // TODO: The "+ 1" thing is still wrong as there may
            // be contentions in the middle.
            render_screen_to_tick(get_ticks() + 1);
            set_border(n);
        }

        handle_port_contention(addr);
    }

    void on_reset_memory() {
        std::memset(&ram_pages, 0, sizeof(ram_pages));
        // memory_slots[0] = 1;
        memory_slots[1] = 5;
        memory_slots[2] = 2;
        // memory_slots[3] = 0;
        // visible_screen_page = 5;
        memory_locked = false;
        dos_active = false;
        port_7ffd_value = 0b10000; // reset to ROM48
        update_memory_map();
        base::on_reset_memory();
    }

    void on_set_addr_bus(fast_u16 addr) {
        addr_bus_value = addr;
    }

    void on_3t_exec_cycle() {
        handle_contention_tick();
        handle_contention_tick();
        handle_contention_tick();
    }

    void on_4t_exec_cycle() {
        handle_contention_tick();
        handle_contention_tick();
        handle_contention_tick();
        handle_contention_tick();
    }

    void on_5t_exec_cycle() {
        handle_contention_tick();
        handle_contention_tick();
        handle_contention_tick();
        handle_contention_tick();
        handle_contention_tick();
    }

    static const ticks_type ticks_per_frame = 71680;
    static const ticks_type ticks_per_line = 224;
    static const ticks_type ticks_per_active_int = 32;

    // The dimensions of the viewable area.
    static const unsigned screen_width = 256;
    static const unsigned screen_height = 192;
    static const unsigned border_width = 48;
    static const unsigned top_border_height = 48;
    static const unsigned bottom_border_height = 40;
    static const unsigned raster_start_line = 16 + 16 + 48; // 64;
    static const unsigned top_hidden_lines =
        raster_start_line - top_border_height;
    static const int frame_tick_offset = border_width / 2 - 4 - 63; // TODO

    // Four bits per frame pixel in brightness:grb format.
    static const unsigned bits_per_frame_pixel = 4;

    static const unsigned brightness_bit = 3;
    static const unsigned green_bit = 2;
    static const unsigned red_bit = 1;
    static const unsigned blue_bit = 0;

    static const unsigned brightness_mask = 1u << brightness_bit;
    static const unsigned green_mask = 1u << green_bit;
    static const unsigned red_mask = 1u << red_bit;
    static const unsigned blue_mask = 1u << blue_bit;

    // Eight frame pixels per chunk. The leftmost pixel occupies the most
    // significant bits.
    static const unsigned pixels_per_frame_chunk = 8;

    // The type of frame chunks.
    static const unsigned frame_chunk_width = 32;
    static_assert(bits_per_frame_pixel * pixels_per_frame_chunk <=
                      frame_chunk_width,
                  "The frame chunk width is too small!");
    typedef uint_least32_t frame_chunk;

    static const unsigned frame_width =
        border_width + screen_width + border_width;
    static const unsigned frame_height =
        top_border_height + screen_height + bottom_border_height;

    // We want screen, border and frame widths be multiples of chunk widths to
    // simplify the processing code and to benefit from aligned memory accesses.
    static const unsigned chunks_per_border_width =
        div_exact(border_width, pixels_per_frame_chunk);
    static const unsigned chunks_per_screen_line =
        div_exact(screen_width, pixels_per_frame_chunk);
    static const unsigned chunks_per_frame_line =
        div_exact(frame_width, pixels_per_frame_chunk);

    typedef frame_chunk screen_chunks_type[frame_height][chunks_per_frame_line];

    const screen_chunks_type &get_screen_chunks() {
        return screen_chunks;
    }

    // TODO: Name the constants.
    // TODO: private
    void start_new_frame() {
        ticks_since_int %= ticks_per_frame;
        render_tick = 0;
        sound_render_tick = 0;

        ++frame_counter;
        if (frame_counter % 16 == 0)
            flash_mask ^= 0xffff;
    }

    // TODO: private
    static fast_u16 get_pixel_pattern_addr(unsigned frame_line,
                                           unsigned pixel_in_line) {
        assert(frame_line >= raster_start_line);
        assert(frame_line < raster_start_line + screen_height);
        assert(pixel_in_line >= border_width);
        assert(pixel_in_line < border_width + screen_width);

        fast_u16 addr = 0x4000;

        // Adjust the address according to the third of the
        // screen.
        unsigned line = frame_line - raster_start_line;
        addr += 0x800 * (line / 64);
        line %= 64;

        // See which character line it is.
        addr += 0x20 * (line / 8);
        line %= 8;

        // Then, adjust according to the pixel line within the
        // character line.
        addr += 0x100 * line;

        // Finally, apply the offset in line.
        addr += (pixel_in_line - border_width) / 8;

        return addr;
    }

    // TODO: private
    static fast_u16 get_colour_attrs_addr(unsigned frame_line,
                                          unsigned pixel_in_line) {
        assert(frame_line >= raster_start_line);
        assert(frame_line < raster_start_line + screen_height);
        assert(pixel_in_line >= border_width);
        assert(pixel_in_line < border_width + screen_width);

        fast_u16 addr = 0x5800;

        unsigned line = frame_line - raster_start_line;
        addr += 0x20 * (line / 8);

        addr += (pixel_in_line - border_width) / 8;

        return addr;
    }

    // TODO: Name the constants.
    // TODO: Optimize.
    void render_screen_to_tick(ticks_type end_tick) {
        static_assert(bits_per_frame_pixel == 4,
                      "Unsupported frame pixel format!");
        static_assert(pixels_per_frame_chunk == 8,
                      "Unsupported frame chunk format!");

        // TODO: Render the border by whole chunks when possible.
        while (render_tick < end_tick) {
            // The tick since the beam was at the imaginary
            // beginning (the top left corner) of the frame.
            ticks_type frame_tick = render_tick + frame_tick_offset;

            // Latch screen area bytes. Note that the first time
            // we do that when the beam is outside of the screen
            // area.
            auto frame_line =
                static_cast<unsigned>(frame_tick / ticks_per_line);
            auto pixel_in_line =
                static_cast<unsigned>(frame_tick % ticks_per_line) * 2;

            bool is_screen_latching_area =
                frame_line >= raster_start_line &&
                frame_line < raster_start_line + screen_height &&
                pixel_in_line >= border_width - 8 &&
                pixel_in_line < border_width + screen_width - 8;

            // Render the screen area.
            bool is_screen_area =
                frame_line >= raster_start_line &&
                frame_line < raster_start_line + screen_height &&
                pixel_in_line >= border_width &&
                pixel_in_line < border_width + screen_width;

            unsigned pixel_in_cycle = (pixel_in_line - border_width) % 8;

            if (is_screen_area) {
                if (pixel_in_cycle == 0) {
                    latched_pixel_pattern2 = latched_pixel_pattern;
                    latched_colour_attrs2 = latched_colour_attrs;
                }
            }

            static fast_u8 flipflop = 1;
            if (is_screen_latching_area) {
                if (flipflop) {
                    fast_u16 pattern_addr =
                        get_pixel_pattern_addr(frame_line, pixel_in_line + 8);
                    latched_pixel_pattern =
                        on_read_page(visible_screen_page, pattern_addr);
                } else {
                    fast_u16 attr_addr =
                        get_colour_attrs_addr(frame_line, pixel_in_line + 8);
                    latched_colour_attrs =
                        on_read_page(visible_screen_page, attr_addr);
                }
                flipflop ^= 1;
            }

            if (is_screen_area) {
                auto attr = static_cast<unsigned>(latched_colour_attrs2 & 0xff);
                unsigned brightness =
                    attr >> (6 - brightness_bit) & brightness_mask;
                unsigned ink_color = ((attr >> 0) & 0x7) | brightness;
                unsigned paper_color = ((attr >> 3) & 0x7) | brightness;

                // TODO: We can compute the whole chunk as soon
                //       as we read the bytes. And then just
                //       apply them here.
                unsigned pixels_value = 0;
                fast_u16 pattern = latched_pixel_pattern2;

                if ((attr & 0x80) != 0)
                    pattern ^= flash_mask;

                unsigned chunk_index = pixel_in_line / pixels_per_frame_chunk;
                unsigned pixel_in_chunk =
                    pixel_in_line % pixels_per_frame_chunk;

                pixels_value |= (pattern & ((1u << 7) >> pixel_in_cycle)) != 0
                                    ? (ink_color << 28)
                                    : (paper_color << 28);
                pixels_value |= (pattern & ((1u << 6) >> pixel_in_cycle)) != 0
                                    ? (ink_color << 24)
                                    : (paper_color << 24);
                pixels_value >>= pixel_in_chunk * 4;

                unsigned screen_line = frame_line - top_hidden_lines;
                frame_chunk *line_chunks = screen_chunks[screen_line];
                frame_chunk *chunk = &line_chunks[chunk_index];

                unsigned pixels_mask = 0xff000000 >> (pixel_in_chunk * 4);
                *chunk = (*chunk & ~pixels_mask) | pixels_value;

                ++render_tick;
                continue;
            }

            // Render the border area.
            // TODO: We can simply initialize the render tick
            //       with the first visible tick value and stop
            //       the rendering loop at the last visible tick.
            //       Just don't forget about latching.
            bool is_visible_area = frame_line >= top_hidden_lines &&
                                   frame_line < raster_start_line +
                                                    screen_height +
                                                    bottom_border_height &&
                                   pixel_in_line < frame_width;

            if (is_visible_area) {
                // if(render_tick % 4 == 0) // TODO: on pentagon, border isn't aligned to 4 t-states
                latched_border_color = border_color;

                unsigned chunk_index = pixel_in_line / pixels_per_frame_chunk;
                unsigned pixel_in_chunk =
                    pixel_in_line % pixels_per_frame_chunk;

                // TODO: Support rendering by whole chunks for
                //       better performance.
                unsigned screen_line = frame_line - top_hidden_lines;
                frame_chunk *line_chunks = screen_chunks[screen_line];
                frame_chunk *chunk = &line_chunks[chunk_index];
                unsigned pixels_value =
                    (0x11000000 * latched_border_color) >> (pixel_in_chunk * 4);
                unsigned pixels_mask = 0xff000000 >> (pixel_in_chunk * 4);
                *chunk = (*chunk & ~pixels_mask) | pixels_value;

                ++render_tick;
                continue;
            }

            ++render_tick;
        }
    }

    // TODO: Move to the private section.
    unsigned frame_counter = 0;
    ticks_type render_tick = 0;
    ticks_type sound_render_tick = 0;
    unsigned latched_border_color = 0;
    fast_u16 latched_pixel_pattern = 0;
    fast_u16 latched_colour_attrs = 0;
    fast_u16 latched_pixel_pattern2 = 0;
    fast_u16 latched_colour_attrs2 = 0;
    fast_u16 flash_mask = 0;

    void render_screen() {
        render_screen_to_tick(ticks_per_frame);
    }

    typedef uint_least32_t pixel_type;
    typedef pixel_type pixels_buffer_type[frame_height][frame_width];
    static const std::size_t pixels_buffer_size = sizeof(pixels_buffer_type);

    void get_frame_pixels(pixels_buffer_type &buffer) {
        static_assert(is_multiple_of(frame_width, pixels_per_frame_chunk),
                      "Fractional number of chunks per line is not supported!");
        static_assert(bits_per_frame_pixel == 4,
                      "Unsupported frame pixel format!");
        static_assert(pixels_per_frame_chunk == 8,
                      "Unsupported frame chunk format!");
        pixel_type *pixels = *buffer;
        std::size_t p = 0;
        for (const auto &screen_line : screen_chunks) {
            for (auto chunk : screen_line) {
                pixels[p++] = translate_color((chunk >> 28) & 0xf);
                pixels[p++] = translate_color((chunk >> 24) & 0xf);
                pixels[p++] = translate_color((chunk >> 20) & 0xf);
                pixels[p++] = translate_color((chunk >> 16) & 0xf);
                pixels[p++] = translate_color((chunk >> 12) & 0xf);
                pixels[p++] = translate_color((chunk >> 8) & 0xf);
                pixels[p++] = translate_color((chunk >> 4) & 0xf);
                pixels[p++] = translate_color((chunk >> 0) & 0xf);
            }
        }
    }

    events_mask run() {
        // Normalize the ticks-since-int counter.
        if (ticks_since_int >= ticks_per_frame)
            start_new_frame();

        // Reset events.
        events = no_events;

        // Execute instructions that fit the current frame.
        while (!events && ticks_since_int < ticks_per_frame) {
            if (!int_suppressed) {
                // ~INT is sampled during the last tick of the
                // previous instruction, so we have to see
                // whether ~INT was active during that last tick
                // and not the current tick.
                ticks_type previous_tick = ticks_since_int - 1;
                if (previous_tick < ticks_per_active_int)
                    on_handle_active_int();
            }

            on_step();
        }

        // Signal end-of-frame, if it's the case.
        if (ticks_since_int >= ticks_per_frame)
            events |= end_of_frame;

        return events;
    }

    void on_step() {
        base::on_step();
    }

    bool on_handle_active_int() {
        return base::on_handle_active_int();
    }

  protected:
    using base::self;

    pixel_type translate_color(unsigned c) {
        uint_fast32_t r = 0;
        r |= (c & red_mask) << (16 - red_bit);
        r |= (c & green_mask) << (8 - green_bit);
        r |= (c & blue_mask) << (0 - blue_bit);

        // TODO: Use the real coefficients.
        r *= (c & brightness_mask) ? 0xff : 0xcc; // cc a0
        r |= 0xff000000; // alpha

        return static_cast<pixel_type>(r);
    }

    events_mask events = no_events;

    ticks_type ticks_since_int = 0;
    ticks_type ticks_to_stop = 0; // Null means no limit.
    ticks_type fetches_to_stop = 0; // Null means no limit.

    fast_u16 addr_bus_value = 0;
    unsigned border_color = 0;

    // port 7ffd handling; reset to ROM48
    fast_u8 port_7ffd_value = 0b10000;
    least_u8 memory_slots[4] = {1, 5, 2, 0};
    least_u8 visible_screen_page = 5;
    memory_page_type rom_pages[rom_pages_count];
    memory_page_type ram_pages[ram_pages_count];
    bool memory_locked = false;
    bool dos_active = false;

    // True if interrupts shall not be initiated at the beginning
    // of frames.
    bool int_suppressed = false;

    // True if interrupt can occur after EI instruction. Some
    // emulators such as SPIN allow that, so we should be able to
    // do the same to support RZX files produced by them.
    // bool int_after_ei_allowed = false;
    bool trace_enabled = false;

  private:
    screen_chunks_type screen_chunks;
    least_u8 memory_marks[z80::address_space_size] = {};
};

} // namespace zx
