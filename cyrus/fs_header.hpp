#pragma once

#include <cstdint>

#pragma pack(push)

struct Fs_header {
  uint16_t num_word_bytes{};
  uint16_t num_sample_bits{};
  uint32_t num_samples{};
};

#pragma pack(pop)