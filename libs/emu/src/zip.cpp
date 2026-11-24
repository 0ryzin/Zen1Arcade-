// SPDX-License-Identifier: GPL-3.0-or-later
// ZIP layout per PKWARE APPNOTE: end-of-central-directory record, central
// directory entries, local headers. Zip64 is not needed for ROM sets.
#include "zen1/emu/zip.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>

#include <zlib.h>

namespace zen1::emu {
namespace {

std::uint16_t le16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
std::uint32_t le32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::string lowerBase(const std::string& n) {
    const auto slash = n.find_last_of("/\\");
    std::string s = slash == std::string::npos ? n : n.substr(slash + 1);
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool readAt(std::ifstream& f, std::uint64_t off, void* dst, size_t n) {
    f.clear();
    f.seekg(static_cast<std::streamoff>(off));
    f.read(static_cast<char*>(dst), static_cast<std::streamsize>(n));
    return static_cast<size_t>(f.gcount()) == n;
}

}  // namespace

std::uint32_t crc32(const std::uint8_t* data, size_t size) {
    return static_cast<std::uint32_t>(::crc32(0L, data, static_cast<uInt>(size)));
}

bool ZipArchive::open(const std::filesystem::path& file, std::string* error) {
    path_ = file;
    entries_.clear();
    std::ifstream f(file, std::ios::binary);
    if (!f) {
        if (error) *error = "cannot open " + file.string();
        return false;
    }
    f.seekg(0, std::ios::end);
    const std::uint64_t size = static_cast<std::uint64_t>(f.tellg());
    const std::uint64_t tail = std::min<std::uint64_t>(size, 65536 + 22);
    std::vector<std::uint8_t> buf(tail);
    if (!readAt(f, size - tail, buf.data(), tail)) {
        if (error) *error = "cannot read " + file.string();
        return false;
    }
    long eocd = -1;
    for (long i = static_cast<long>(tail) - 22; i >= 0; --i) {
        if (le32(&buf[static_cast<size_t>(i)]) == 0x06054b50) { eocd = i; break; }
    }
    if (eocd < 0) {
        if (error) *error = "not a zip file: " + file.string();
        return false;
    }
    const std::uint8_t* e = &buf[static_cast<size_t>(eocd)];
    const std::uint16_t count = le16(e + 10);
    const std::uint32_t cdSize = le32(e + 12);
    const std::uint32_t cdOff = le32(e + 16);
    std::vector<std::uint8_t> cd(cdSize);
    if (!readAt(f, cdOff, cd.data(), cdSize)) {
        if (error) *error = "damaged zip directory: " + file.string();
        return false;
    }
    size_t p = 0;
    for (std::uint16_t i = 0; i < count; ++i) {
        if (p + 46 > cd.size() || le32(&cd[p]) != 0x02014b50) {
            if (error) *error = "damaged zip directory: " + file.string();
            return false;
        }
        ZipEntry z;
        z.method = le16(&cd[p + 10]);
        z.crc = le32(&cd[p + 16]);
        z.compressedSize = le32(&cd[p + 20]);
        z.size = le32(&cd[p + 24]);
        const std::uint16_t nameLen = le16(&cd[p + 28]);
        const std::uint16_t extraLen = le16(&cd[p + 30]);
        const std::uint16_t commentLen = le16(&cd[p + 32]);
        z.localHeaderOffset = le32(&cd[p + 42]);
        if (p + 46 + nameLen > cd.size()) break;
        z.name.assign(reinterpret_cast<const char*>(&cd[p + 46]), nameLen);
        if (!z.name.empty() && z.name.back() != '/') entries_.push_back(std::move(z));
        p += 46u + nameLen + extraLen + commentLen;
    }
    return true;
}

const ZipEntry* ZipArchive::find(std::uint32_t crc, const std::string& name) const {
    for (const auto& e : entries_)
        if (e.crc == crc) return &e;
    const std::string want = lowerBase(name);
    for (const auto& e : entries_)
        if (lowerBase(e.name) == want) return &e;
    return nullptr;
}

std::optional<std::vector<std::uint8_t>> ZipArchive::read(const ZipEntry& z, std::string* error) const {
    std::ifstream f(path_, std::ios::binary);
    std::uint8_t lh[30];
    if (!f || !readAt(f, z.localHeaderOffset, lh, 30) || le32(lh) != 0x04034b50) {
        if (error) *error = "damaged entry " + z.name;
        return std::nullopt;
    }
    const std::uint64_t dataOff = z.localHeaderOffset + 30ull + le16(lh + 26) + le16(lh + 28);
    std::vector<std::uint8_t> packed(z.compressedSize);
    if (!readAt(f, dataOff, packed.data(), packed.size())) {
        if (error) *error = "truncated entry " + z.name;
        return std::nullopt;
    }
    std::vector<std::uint8_t> out;
    if (z.method == 0) {
        out = std::move(packed);
    } else if (z.method == 8) {
        out.resize(z.size);
        z_stream s{};
        if (inflateInit2(&s, -MAX_WBITS) != Z_OK) return std::nullopt;
        s.next_in = packed.data();
        s.avail_in = static_cast<uInt>(packed.size());
        s.next_out = out.data();
        s.avail_out = static_cast<uInt>(out.size());
        const int rc = inflate(&s, Z_FINISH);
        inflateEnd(&s);
        if (rc != Z_STREAM_END || s.total_out != z.size) {
            if (error) *error = "cannot decompress " + z.name;
            return std::nullopt;
        }
    } else {
        if (error) *error = "unsupported compression in " + z.name;
        return std::nullopt;
    }
    if (crc32(out.data(), out.size()) != z.crc) {
        if (error) *error = "checksum mismatch in " + z.name;
        return std::nullopt;
    }
    return out;
}

}  // namespace zen1::emu
