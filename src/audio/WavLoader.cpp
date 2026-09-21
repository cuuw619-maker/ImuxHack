#include "WavLoader.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <algorithm>
namespace {
std::uint32_t u32(std::uint8_t const* p) { return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24); }
std::uint16_t u16(std::uint8_t const* p) { return p[0] | (p[1]<<8); }
}
namespace imux::audio {
bool loadWav(std::string const& path, PCMBuffer& out, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "Cannot open WAV file."; return false; }
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(f)), {});
    if (data.size() < 44 || std::memcmp(data.data(), "RIFF", 4) || std::memcmp(data.data()+8, "WAVE", 4)) {
        error = "Not a RIFF/WAVE file."; return false;
    }
    std::uint16_t channels=0, bits=0, format=0;
    std::uint32_t rate=0, dataSize=0, dataOffset=0;
    std::size_t p=12;
    while (p + 8 <= data.size()) {
        auto id = data.data()+p;
        auto size = u32(data.data()+p+4);
        p += 8;
        if (p + size > data.size()) break;
        if (!std::memcmp(id, "fmt ", 4) && size >= 16) {
            format=u16(data.data()+p); channels=u16(data.data()+p+2);
            rate=u32(data.data()+p+4); bits=u16(data.data()+p+14);
        } else if (!std::memcmp(id, "data", 4)) {
            dataOffset=static_cast<std::uint32_t>(p); dataSize=size; break;
        }
        p += size + (size & 1u);
    }
    if (!channels || !rate || !dataOffset || !dataSize || (format != 1 && format != 3) || (bits != 16 && bits != 24 && bits != 32)) {
        error = "Unsupported WAV format. Use PCM 16/24/32-bit or IEEE float 32-bit."; return false;
    }
    std::size_t bytesPerSample=bits/8, frameBytes=bytesPerSample*channels;
    if (!frameBytes) return false;
    std::size_t frames=dataSize/frameBytes;
    out.sampleRate=static_cast<float>(rate);
    out.mono.resize(frames);
    auto* raw=data.data()+dataOffset;
    for (std::size_t i=0;i<frames;++i) {
        double sum=0.0;
        for (std::size_t c=0;c<channels;++c) {
            auto* q=raw+i*frameBytes+c*bytesPerSample;
            float v=0.f;
            if (format==3 && bits==32) std::memcpy(&v,q,4);
            else if (bits==16) v=static_cast<float>(static_cast<std::int16_t>(u16(q)))/32768.f;
            else if (bits==24) {
                std::int32_t x=(q[0]|(q[1]<<8)|(q[2]<<16)); if (x&0x800000) x|=~0xffffff;
                v=static_cast<float>(x)/8388608.f;
            } else v=static_cast<float>(static_cast<std::int32_t>(u32(q)))/2147483648.f;
            sum += v;
        }
        out.mono[i]=static_cast<float>(sum/channels);
    }
    return true;
}
}
