// How a packed vertex array stores one component and the readers that expand one back to floats
#include "engine/formats/nif_internal.h"

namespace KnC::nif {

namespace {

float half_to_float(uint16_t half) {
    const uint32_t sign = static_cast<uint32_t>(half & 0x8000u) << 16;
    const uint32_t exponent = (half >> 10) & 0x1Fu;
    uint32_t mantissa = half & 0x3FFu;
    uint32_t bits = sign;
    if (exponent == 0x1Fu) {
        bits |= 0x7F800000u | (mantissa << 13);            // infinity or NaN
    } else if (exponent != 0) {
        bits |= ((exponent + 112u) << 23) | (mantissa << 13);
    } else if (mantissa != 0) {
        uint32_t shift = 0;                                // subnormal
        while ((mantissa & 0x400u) == 0) { mantissa <<= 1; ++shift; }
        bits |= ((113u - shift) << 23) | ((mantissa & 0x3FFu) << 13);
    }
    float value = 0.f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool take_half_array(Cursor& cursor, size_t count, std::vector<float>& out) {
    if (count > cursor.remaining() / sizeof(uint16_t)) return false;
    out.resize(count);
    for (float& value : out) {
        uint16_t half = 0;
        if (!cursor.take_u16(half)) return false;
        value = half_to_float(half);
    }
    return true;
}

bool take_signed_byte_array(Cursor& cursor, size_t count, std::vector<float>& out) {
    if (count > cursor.remaining()) return false;
    out.resize(count);
    for (float& value : out) {
        uint8_t raw_component = 0;
        if (!cursor.take_u8(raw_component)) return false;
        value = static_cast<int8_t>(raw_component) / 127.f;
    }
    return true;
}

// The exact inverse of half to float above which is what a lossless encoder needs
uint16_t float_to_half(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const uint16_t sign = static_cast<uint16_t>((bits >> 16) & 0x8000u);
    const int32_t  exponent = static_cast<int32_t>((bits >> 23) & 0xFFu) - 112;
    const uint32_t mantissa = bits & 0x7FFFFFu;
    if (exponent >= 0x1F) {
        if (((bits >> 23) & 0xFFu) != 0xFFu || mantissa == 0)
            return static_cast<uint16_t>(sign | 0x7C00u);
        // half to float widened the half mantissa by thirteen bits so this is its inverse
        const uint32_t payload = (mantissa >> 13) != 0 ? (mantissa >> 13) : 0x200u;
        return static_cast<uint16_t>(sign | 0x7C00u | payload);
    }
    if (exponent > 0)
        return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exponent) << 10) |
                                     (mantissa >> 13));
    if (exponent < -10) return sign;                       // underflows to zero
    const uint32_t subnormal = (mantissa | 0x800000u) >> static_cast<uint32_t>(1 - exponent);
    return static_cast<uint16_t>(sign | (subnormal >> 13));
}

// Rounding to nearest integer which is all the inverse of two byte packings needs
uint8_t nearest_byte(float scaled) {
    const float rounded = scaled < 0.f ? scaled - 0.5f : scaled + 0.5f;
    const int32_t whole = static_cast<int32_t>(rounded);
    if (whole < -128) return static_cast<uint8_t>(0x80);
    if (whole > 255) return 0xFF;
    return static_cast<uint8_t>(whole & 0xFF);
}

bool take_unsigned_byte_array(Cursor& cursor, size_t count, std::vector<float>& out) {
    if (count > cursor.remaining()) return false;
    out.resize(count);
    for (float& value : out) {
        uint8_t raw_component = 0;
        if (!cursor.take_u8(raw_component)) return false;
        value = raw_component / 255.f;
    }
    return true;
}

} // namespace

size_t component_width(NifComponentFormat format) {
    switch (format) {
        case NifComponentFormat::Float32:      return 4;
        case NifComponentFormat::Float16:      return 2;
        case NifComponentFormat::SignedByte:   return 1;
        case NifComponentFormat::UnsignedByte: return 1;
        case NifComponentFormat::Absent:       break;
    }
    return 0;
}

bool take_component_format(Cursor& cursor, const NifHeader& header,
                           NifComponentFormat& format) {
    uint8_t raw_format = 0;
    return take_component_format(cursor, header, format, raw_format);
}

bool take_component_format(Cursor& cursor, const NifHeader& header,
                           NifComponentFormat& format, uint8_t& raw_format) {
    raw_format = 0;
    if (!cursor.take_u8(raw_format)) return false;
    // nif xml warns that a writer may put 0xFF in one of these bools rather than 1
    if (header.version < kComponentFormatFrom) {
        format = raw_format != 0 ? NifComponentFormat::Float32 : NifComponentFormat::Absent;
        return true;
    }
    switch (raw_format) {
        case 0x00: case 0x01: case 0x06: case 0x07: case 0x0F:
            format = static_cast<NifComponentFormat>(raw_format);
            return true;
        default:
            return false;
    }
}

bool read_component_array(Cursor& cursor, NifComponentFormat format, size_t count,
                          std::vector<float>& out) {
    switch (format) {
        case NifComponentFormat::Float32:      return take_f32_array(cursor, count, out);
        case NifComponentFormat::Float16:      return take_half_array(cursor, count, out);
        case NifComponentFormat::SignedByte:   return take_signed_byte_array(cursor, count, out);
        case NifComponentFormat::UnsignedByte: return take_unsigned_byte_array(cursor, count, out);
        case NifComponentFormat::Absent:       break;
    }
    out.clear();
    return true;
}

void write_component_array(NifComponentFormat format, const std::vector<float>& values,
                           std::string& out) {
    for (const float value : values) {
        if (format == NifComponentFormat::Float32) {
            char bytes[sizeof(float)];
            std::memcpy(bytes, &value, sizeof(value));
            out.append(bytes, sizeof(bytes));
        } else if (format == NifComponentFormat::Float16) {
            const uint16_t half = float_to_half(value);
            out.push_back(static_cast<char>(half & 0xFFu));
            out.push_back(static_cast<char>(half >> 8));
        } else if (format == NifComponentFormat::SignedByte) {
            out.push_back(static_cast<char>(nearest_byte(value * 127.f)));
        } else if (format == NifComponentFormat::UnsignedByte) {
            out.push_back(static_cast<char>(nearest_byte(value * 255.f)));
        }
    }
}

bool take_component(Cursor& cursor, NifComponentFormat format, float& out) {
    if (format == NifComponentFormat::Float32) return cursor.take_f32(out);
    if (format == NifComponentFormat::Float16) {
        uint16_t half = 0;
        if (!cursor.take_u16(half)) return false;
        out = half_to_float(half);
        return true;
    }
    uint8_t raw_component = 0;
    if (format == NifComponentFormat::Absent || !cursor.take_u8(raw_component)) return false;
    out = format == NifComponentFormat::SignedByte
              ? static_cast<int8_t>(raw_component) / 127.f
              : raw_component / 255.f;
    return true;
}

bool skip_components(Cursor& cursor, NifComponentFormat format, size_t count) {
    return cursor.skip(count * component_width(format));
}

}
