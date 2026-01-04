#include "nes/apu.h"
#include <cmath>
#include <algorithm>

using namespace nes;

const std::array<uint8_t, 32> APU::length_table_ = {
    10,254,20,2,40,4,80,6,160,8,60,10,14,12,26,14,
    12,16,24,18,48,20,96,22,192,24,72,26,16,28,32,30
};

const std::array<uint16_t, 16> APU::noise_period_table_ = {
    4,8,16,32,64,96,128,160,202,254,380,508,762,1016,2034,4068
};

const std::array<uint16_t, 16> APU::dmc_period_table_ = {
    428,380,340,320,286,254,226,214,190,160,142,128,106,84,72,54
};

APU::APU() { reset(); }

void APU::reset() {
    frame_counter_ = 0; frame_mode_ = 0; frame_step_ = 0; frame_irq_ = false;
    pulse1_ = {}; pulse2_ = {}; triangle_ = {}; noise_ = {}; dmc_ = {};
}

uint8_t APU::read_register(uint16_t addr) {
    switch (addr) {
        case 0x4015: {
            uint8_t status = (pulse1_.length_counter_ > 0 ? 1 : 0) |
                             (pulse2_.length_counter_ > 0 ? 2 : 0) |
                             (triangle_.length_counter_ > 0 ? 4 : 0) |
                             (noise_.length_counter_ > 0 ? 8 : 0) |
                             (dmc_.length_ > 0 ? 16 : 0) |
                             (frame_irq_ ? 64 : 0) |
                             (dmc_.irq_ ? 128 : 0);
            frame_irq_ = false;
            return status;
        }
        default: return 0;
    }
}

void APU::write_register(uint16_t addr, uint8_t value) {
    switch (addr) {
        case 0x4000: pulse1_.duty_ = (value >> 6) & 3; pulse1_.length_ = value & 0x20; pulse1_.envelope_ = value & 0x0F; break;
        case 0x4001: pulse1_.sweep_ = value; pulse1_.sweep_reload_ = true; break;
        case 0x4002: pulse1_.timer_ = (pulse1_.timer_ & 0xFF00) | value; break;
        case 0x4003: pulse1_.timer_ = (pulse1_.timer_ & 0x00FF) | ((value & 7) << 8); pulse1_.length_counter_ = length_table_[value >> 3]; pulse1_.envelope_start_ = true; break;
        case 0x4004: pulse2_.duty_ = (value >> 6) & 3; pulse2_.length_ = value & 0x20; pulse2_.envelope_ = value & 0x0F; break;
        case 0x4005: pulse2_.sweep_ = value; pulse2_.sweep_reload_ = true; break;
        case 0x4006: pulse2_.timer_ = (pulse2_.timer_ & 0xFF00) | value; break;
        case 0x4007: pulse2_.timer_ = (pulse2_.timer_ & 0x00FF) | ((value & 7) << 8); pulse2_.length_counter_ = length_table_[value >> 3]; pulse2_.envelope_start_ = true; break;
        case 0x4008: triangle_.linear_ = value & 0x7F; triangle_.length_ = value & 0x80; break;
        case 0x400A: triangle_.timer_ = (triangle_.timer_ & 0xFF00) | value; break;
        case 0x400B: triangle_.timer_ = (triangle_.timer_ & 0x00FF) | ((value & 7) << 8); triangle_.length_counter_ = length_table_[value >> 3]; triangle_.linear_reload_ = true; break;
        case 0x400C: noise_.length_ = value & 0x20; noise_.envelope_ = value & 0x0F; break;
        case 0x400E: noise_.timer_ = noise_period_table_[value & 0x0F]; noise_.mode_ = (value >> 7) & 1; break;
        case 0x400F: noise_.length_counter_ = length_table_[value >> 3]; noise_.envelope_start_ = true; break;
        case 0x4010: dmc_.irq_enable_ = (value >> 7) & 1; dmc_.loop_ = (value >> 6) & 1; dmc_.timer_ = dmc_period_table_[value & 0x0F]; break;
        case 0x4011: dmc_.direct_load_ = value & 0x7F; break;
        case 0x4012: dmc_.sample_address_ = value; break;
        case 0x4013: dmc_.sample_length_ = value; break;
        case 0x4015: {
            pulse1_.enabled_ = value & 1; pulse2_.enabled_ = (value >> 1) & 1; triangle_.enabled_ = (value >> 2) & 1; noise_.enabled_ = (value >> 3) & 1; dmc_.enabled_ = (value >> 4) & 1;
            if (!pulse1_.enabled_) pulse1_.length_counter_ = 0; if (!pulse2_.enabled_) pulse2_.length_counter_ = 0; if (!triangle_.enabled_) triangle_.length_counter_ = 0; if (!noise_.enabled_) noise_.length_counter_ = 0; if (!dmc_.enabled_) dmc_.length_ = 0;
            dmc_.irq_ = false;
            break;
        }
        case 0x4017: frame_mode_ = (value >> 7) & 1; frame_counter_ = 0; frame_step_ = 0; if (frame_mode_) step(); break;
    }
}

void APU::step() {
    frame_counter_++;
    if (frame_counter_ % 2 == 0) {
        pulse1_.step_timer(); pulse2_.step_timer(); noise_.step_timer(); dmc_.step_timer();
    }
    if (frame_counter_ % 4 == 0) {
        triangle_.step_timer();
    }
    if (frame_mode_ == 0) {
        if (frame_counter_ == 3728) { pulse1_.step_envelope(); pulse2_.step_envelope(); triangle_.step_linear(); noise_.step_envelope(); }
        if (frame_counter_ == 7456) { pulse1_.step_envelope(); pulse2_.step_envelope(); triangle_.step_linear(); noise_.step_envelope(); pulse1_.step_length(); pulse2_.step_length(); triangle_.step_length(); noise_.step_length(); }
        if (frame_counter_ == 11185) { pulse1_.step_envelope(); pulse2_.step_envelope(); triangle_.step_linear(); noise_.step_envelope(); }
        if (frame_counter_ == 14914) { frame_counter_ = 0; pulse1_.step_envelope(); pulse2_.step_envelope(); triangle_.step_linear(); noise_.step_envelope(); pulse1_.step_length(); pulse2_.step_length(); triangle_.step_length(); noise_.step_length(); if (!frame_mode_) frame_irq_ = true; }
    } else {
        if (frame_counter_ == 3728) { pulse1_.step_envelope(); pulse2_.step_envelope(); triangle_.step_linear(); noise_.step_envelope(); }
        if (frame_counter_ == 7456) { pulse1_.step_envelope(); pulse2_.step_envelope(); triangle_.step_linear(); noise_.step_envelope(); pulse1_.step_length(); pulse2_.step_length(); triangle_.step_length(); noise_.step_length(); }
        if (frame_counter_ == 11185) { pulse1_.step_envelope(); pulse2_.step_envelope(); triangle_.step_linear(); noise_.step_envelope(); }
        if (frame_counter_ == 18640) { frame_counter_ = 0; }
    }
    pulse1_.step_sweep(); pulse2_.step_sweep();
}

void APU::generate_audio(int samples, std::vector<int16_t>& buffer) {
    buffer.resize(samples);
    for (int i = 0; i < samples; ++i) {
        step();
        float p1 = pulse1_.output(), p2 = pulse2_.output(), t = triangle_.output(), n = noise_.output(), d = dmc_.output();
        float pulse_out = mix_pulse(p1, p2);
        float tnd_out = mix_tnd(t, n, d);
        float mixed = pulse_out + tnd_out;
        buffer[i] = static_cast<int16_t>(std::clamp(mixed * 32767.0f, -32768.0f, 32767.0f));
    }
}

float APU::mix_pulse(float p1, float p2) {
    if (p1 + p2 == 0) return 0;
    return 95.88f / ((8128.0f / (p1 + p2)) + 100.0f);
}

float APU::mix_tnd(float t, float n, float d) {
    if (t + n + d == 0) return 0;
    return 159.79f / ((1.0f / ((t / 8227.0f) + (n / 12241.0f) + (d / 22638.0f))) + 100.0f);
}

// Pulse implementation
void APU::Pulse::step_timer() {
    if (timer_ == 0) return;
    if (--timer_ == 0) {
        sequencer_ = (sequencer_ + 1) & 7;
        timer_ = (timer_ & 0xFF00) | (timer_ & 0x00FF); // reload low byte
    }
}

void APU::Pulse::step_envelope() {
    if (envelope_start_) { envelope_counter_ = 15; envelope_start_ = false; }
    else if (envelope_counter_ > 0) envelope_counter_--;
}

void APU::Pulse::step_sweep() {
    if (sweep_reload_) { sweep_counter_ = (sweep_ >> 4) & 7; sweep_reload_ = false; }
    if (sweep_counter_ > 0) sweep_counter_--;
    else {
        sweep_counter_ = (sweep_ >> 4) & 7;
        if (sweep_ & 0x80 && length_counter_ > 0) {
            uint16_t shift = timer_ >> (sweep_ & 7);
            if (sweep_ & 0x08) timer_ -= shift; else timer_ += shift;
        }
    }
}

void APU::Pulse::step_length() { if (length_counter_ > 0 && !(length_ & 0x20)) length_counter_--; }

uint8_t APU::Pulse::output() {
    if (length_counter_ == 0 || timer_ < 8) return 0;
    uint8_t vol = (envelope_ & 0x10) ? envelope_ & 0x0F : envelope_counter_;
    static const std::array<uint8_t, 4> duties = {0x01, 0x81, 0x87, 0x7E};
    return (duties[duty_] & (1 << sequencer_)) ? vol : 0;
}

// Triangle implementation
void APU::Triangle::step_timer() {
    if (timer_ == 0) return;
    if (--timer_ == 0) {
        sequencer_ = (sequencer_ + 1) & 31;
        timer_ = (timer_ & 0xFF00) | (timer_ & 0x00FF);
    }
}

void APU::Triangle::step_linear() {
    if (linear_reload_) { linear_counter_ = linear_; linear_reload_ = false; }
    else if (linear_counter_ > 0) linear_counter_--;
    if (!(length_ & 0x80)) linear_counter_ = 0;
}

void APU::Triangle::step_length() { if (length_counter_ > 0 && !(length_ & 0x80)) length_counter_--; }

uint8_t APU::Triangle::output() {
    if (length_counter_ == 0 || linear_counter_ == 0 || timer_ < 2) return 0;
    return (sequencer_ < 16) ? sequencer_ : (31 - sequencer_);
}

// Noise implementation
void APU::Noise::step_timer() {
    if (timer_ == 0) return;
    if (--timer_ == 0) {
        uint8_t feedback = (shift_register_ & 1) ^ ((shift_register_ >> (mode_ ? 6 : 1)) & 1);
        shift_register_ >>= 1;
        shift_register_ |= (feedback << 14);
        timer_ = noise_period_table_[envelope_ & 0x0F];
    }
}

void APU::Noise::step_envelope() {
    if (envelope_start_) { envelope_counter_ = 15; envelope_start_ = false; }
    else if (envelope_counter_ > 0) envelope_counter_--;
}

void APU::Noise::step_length() { if (length_counter_ > 0 && !(length_ & 0x20)) length_counter_--; }

uint8_t APU::Noise::output() {
    if (length_counter_ == 0 || (shift_register_ & 1)) return 0;
    uint8_t vol = (envelope_ & 0x10) ? envelope_ & 0x0F : envelope_counter_;
    return vol;
}

// DMC implementation (simplified)
void APU::DMC::step_timer() {
    if (timer_ == 0) return;
    if (--timer_ == 0) {
        // Simplified: just output direct_load_
        output_ = direct_load_;
        timer_ = dmc_period_table_[0]; // placeholder
    }
}

uint8_t APU::DMC::output() { return output_; }