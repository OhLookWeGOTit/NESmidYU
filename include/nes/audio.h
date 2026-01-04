#pragma once
#include <cstdint>
#include <vector>
#include <array>

namespace nes {
class APU {
public:
    APU();
    uint8_t read_register(uint16_t addr);
    void write_register(uint16_t addr, uint8_t value);
    void step(); // Advance 1 APU cycle
    void generate_audio(int samples, std::vector<int16_t>& buffer); // Generate PCM samples
    void reset();

private:
    // Frame counter
    uint8_t frame_counter_;
    uint8_t frame_mode_;
    int frame_step_;
    bool frame_irq_;

    // Channels
    struct Pulse {
        uint8_t duty_, length_, envelope_, sweep_;
        uint16_t timer_;
        uint8_t sequencer_, length_counter_, envelope_counter_, sweep_counter_;
        bool enabled_, envelope_start_, sweep_reload_;
        void step_timer();
        void step_envelope();
        void step_sweep();
        void step_length();
        uint8_t output();
    } pulse1_, pulse2_;

    struct Triangle {
        uint8_t linear_, length_;
        uint16_t timer_;
        uint8_t sequencer_, length_counter_, linear_counter_;
        bool enabled_, linear_reload_;
        void step_timer();
        void step_linear();
        void step_length();
        uint8_t output();
    } triangle_;

    struct Noise {
        uint8_t envelope_, length_;
        uint16_t timer_;
        uint8_t length_counter_, envelope_counter_, shift_register_;
        bool enabled_, envelope_start_;
        uint8_t mode_;
        void step_timer();
        void step_envelope();
        void step_length();
        uint8_t output();
    } noise_;

    struct DMC {
        uint8_t direct_load_, sample_address_, sample_length_;
        uint16_t timer_, address_, length_;
        uint8_t shift_register_, bit_count_, output_;
        bool enabled_, loop_, irq_enable_, irq_;
        void step_timer();
        uint8_t output();
    } dmc_;

    // Mixer
    float mix_pulse(float p1, float p2);
    float mix_tnd(float t, float n, float d);

    // Lookup tables
    static const std::array<uint8_t, 32> length_table_;
    static const std::array<uint16_t, 16> noise_period_table_;
    static const std::array<uint16_t, 16> dmc_period_table_;
};
}