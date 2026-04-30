// include/fcemu/cartridge.h
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace fcemu {

struct INesHeader {
    char magic[4];
    uint8_t prg_rom_size;
    uint8_t chr_rom_size;
    uint8_t flags6;
    uint8_t flags7;
    uint8_t prg_ram_size;
    uint8_t flags9;
    uint8_t flags10;
    uint8_t reserved[5];
};

enum class MirrorMode { Horizontal, Vertical, FourScreen, Single0, Single1 };

class Mapper {
public:
    virtual ~Mapper() = default;
    virtual uint8_t cpu_read(uint16_t addr) = 0;
    virtual void cpu_write(uint16_t addr, uint8_t val) = 0;
    virtual uint8_t ppu_read(uint16_t addr) = 0;
    virtual void ppu_write(uint16_t addr, uint8_t val) = 0;
    virtual void scanline_irq(int scanline) {}
    virtual void reset() = 0;
};

class Cartridge {
public:
    Cartridge();
    ~Cartridge();
    bool load_rom(const std::string& path);
    bool load_rom_data(const std::vector<uint8_t>& data);
    int mapper_number() const { return mapper_number_; }
    const std::string& game_name() const { return game_name_; }
    bool has_battery() const { return has_battery_; }
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t val);
    uint8_t ppu_read(uint16_t addr);
    void ppu_write(uint16_t addr, uint8_t val);
    uint8_t* battery_ram() { return battery_ram_.data(); }
    void save_battery_ram(const std::string& path);
    void load_battery_ram(const std::string& path);
    bool irq_pending() const { return irq_pending_; }
    void clear_irq() { irq_pending_ = false; }
    void notify_scanline(int scanline);
    MirrorMode mirror_mode() const { return mirror_mode_; }

private:
    std::vector<uint8_t> prg_rom_;
    std::vector<uint8_t> chr_rom_;
    std::vector<uint8_t> chr_ram_;
    std::vector<uint8_t> battery_ram_;
    int mapper_number_;
    std::string game_name_;
    bool has_battery_;
    MirrorMode mirror_mode_;
    bool irq_pending_;
    std::unique_ptr<Mapper> mapper_;

    static std::unique_ptr<Mapper> create_mapper(int number, Cartridge& cart);
    bool parse_header(const std::vector<uint8_t>& data, INesHeader& hdr);
};

} // namespace fcemu
