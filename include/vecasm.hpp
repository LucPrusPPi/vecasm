#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "vecasm.h"

namespace vecasm {

inline std::uint32_t caps() { return vecasm_caps(); }
inline int set_backend(int mode) { return vecasm_set_backend(mode); }
inline int best_backend() { return vecasm_best_backend(); }
inline int active_backend() { return vecasm_active_backend(); }
inline int active_backend_n(std::size_t n) { return vecasm_active_backend_n(n); }
inline void calibrate() { vecasm_calibrate(); }
inline const char *backend_name(int mode) { return vecasm_backend_name(mode); }

inline float dot(std::span<const float> a, std::span<const float> b) {
    const std::size_t n = a.size() < b.size() ? a.size() : b.size();
    return vecasm_dot_f32(a.data(), b.data(), n);
}

inline double dot(std::span<const double> a, std::span<const double> b) {
    const std::size_t n = a.size() < b.size() ? a.size() : b.size();
    return vecasm_dot_f64(a.data(), b.data(), n);
}

inline void axpy(float alpha, std::span<const float> x, std::span<float> y) {
    const std::size_t n = x.size() < y.size() ? x.size() : y.size();
    vecasm_axpy_f32(alpha, x.data(), y.data(), n);
}

inline float sum(std::span<const float> a) { return vecasm_sum_f32(a.data(), a.size()); }
inline float norm2(std::span<const float> a) { return vecasm_norm2_f32(a.data(), a.size()); }

inline float dot3(const float a[3], const float b[3]) { return vecasm_dot3_f32(a, b); }
inline void cross3(const float a[3], const float b[3], float out[3]) {
    vecasm_cross3_f32(a, b, out);
}
inline void normalize3(const float in[3], float out[3]) { vecasm_normalize3_f32(in, out); }

} // namespace vecasm
