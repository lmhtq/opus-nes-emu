// include/fcemu/cartridge.h
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace fcemu {

enum class MirrorMode { Horizontal, Vertical, FourScreen, Single0, Single1 };

class Cartridge;

class Mapper {
public:
    explicit Mapper(Cartridge& cart) : cart_(cart) {}
    virtual ~Mapper() = default;

    virtual uint8_t cpu_read(uint16_t addr) = 0;
    virtual void    cpu_write(uint16_t addr, uint8_t val) = 0;
    virtual uint8_t ppu_read(uint16_t addr) = 0;
    virtual void    ppu_write(uint16_t addr, uint8_t val) = 0;

    virtual void reset() {}
    virtual void scanline_tick() {}     // Called by PPU on each visible scanline
    virtual int  number() const = 0;

    virtual void save_state(class Serializer&) const {}
    virtual void load_state(class Deserializer&) {}

protected:
    Cartridge& cart_;
};

class Cartridge {
public:
    Cartridge();
    ~Cartridge();

    bool load_rom(const std::string& path);
    bool load_rom_data(const std::vector<uint8_t>& data);

    int          mapper_number()   const { return mapper_number_; }
    MirrorMode   mirror_mode()     const { return mirror_mode_; }
    void         set_mirror_mode(MirrorMode m) { mirror_mode_ = m; }
    const std::string& game_name() const { return game_name_; }
    bool         has_battery()     const { return has_battery_; }
    const std::string& sha256()    const { return sha256_; }

    uint8_t cpu_read(uint16_t addr);
    void    cpu_write(uint16_t addr, uint8_t val);
    uint8_t ppu_read(uint16_t addr);
    void    ppu_write(uint16_t addr, uint8_t val);

    void notify_scanline();

    bool irq_pending() const { return irq_pending_; }
    void clear_irq() { irq_pending_ = false; }
    void raise_irq() { irq_pending_ = true; }

    bool save_battery_ram(const std::string& path) const;
    bool load_battery_ram(const std::string& path);

    void serialize(class Serializer& s) const;
    void deserialize(class Deserializer& d);

    // Accessors used by mappers.
    std::vector<uint8_t>& prg_rom()      { return prg_rom_; }
    std::vector<uint8_t>& chr()          { return chr_; }
    std::vector<uint8_t>& prg_ram()      { return prg_ram_; }
    bool chr_is_ram() const              { return chr_is_ram_; }

    // For analyzer / save state.
    const std::vector<uint8_t>& prg_rom_const() const { return prg_rom_; }
    const std::vector<uint8_t>& chr_const()     const { return chr_; }

private:
    std::vector<uint8_t> prg_rom_;
    std::vector<uint8_t> chr_;          // CHR ROM or RAM
    std::vector<uint8_t> prg_ram_;      // $6000-$7FFF window (battery or work RAM)
    bool chr_is_ram_;
    int  mapper_number_;
    std::string game_name_;
    std::string sha256_;
    bool has_battery_;
    MirrorMode mirror_mode_;
    bool irq_pending_;
    std::unique_ptr<Mapper> mapper_;

    static std::unique_ptr<Mapper> create_mapper(int number, Cartridge& cart);
};

} // namespace fcemu
