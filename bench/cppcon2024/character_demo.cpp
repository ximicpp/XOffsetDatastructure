#include "character.hpp"

#include <charconv>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {

using Character = xo::CharacterInline;
using Buffer = XOffsetDatastructure::TypedXBuffer<Character>;
const auto capacity = XOffsetDatastructure::XBuffer::maximum_capacity(1 << 24);
constexpr std::array<float, 10> updated_attributes{
    1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f,
};

void write_wire(const char* path, Buffer& buffer) {
    const auto wire = buffer.save_verified();
    std::ofstream file(path, std::ios::binary);
    file.write(wire.data(), static_cast<std::streamsize>(wire.size()));
    file.close();
    if (!file) throw std::runtime_error("could not write Character wire");
    std::cout << cc24::checksum(buffer.plain_root()) << '\n';
}

Buffer read_wire(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("could not open Character wire");
    const std::string wire{std::istreambuf_iterator<char>(file), {}};
    if (file.bad()) throw std::runtime_error("could not read Character wire");
    return Buffer::load(wire, capacity);
}

uint64_t parse_checksum(std::string_view text) {
    uint64_t value = 0;
    const auto end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end)
        throw std::runtime_error("invalid expected checksum");
    return value;
}

void check(Buffer& buffer, uint64_t checksum) {
    if (!buffer.ok() || cc24::checksum(buffer.plain_root()) != checksum)
        throw std::runtime_error("Character checksum mismatch");
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view command = argc > 1 ? argv[1] : "";
        if (command == "write" && argc == 3) {
            auto buffer = Buffer::create(16 * 1024, capacity);
            const auto reference = cc24::make_character();
            if (!buffer.edit([&](Character& r) { xo::fill(r, reference); }))
                throw std::runtime_error("Character creation failed");
            check(buffer, cc24::checksum(reference));
            write_wire(argv[2], buffer);
        } else if (command == "modify" && argc == 5) {
            auto buffer = read_wire(argv[2]);
            check(buffer, parse_checksum(argv[4]));
            if (!buffer.edit([](Character& r) {
                    ++r.level;
                    r.path.push_back(xo::Position{10.0f, 20.0f, 30.0f});
                    r.equips.front().attributes.assign(updated_attributes);
                }))
                throw std::runtime_error("Character edit failed");
            write_wire(argv[3], buffer);
        } else if (command == "verify" && argc == 3) {
            auto buffer = read_wire(argv[2]);
            auto reference = cc24::make_character();
            ++reference.level;
            reference.path.push_back({10.0f, 20.0f, 30.0f});
            reference.equips.front().attributes.assign(updated_attributes.begin(), updated_attributes.end());
            check(buffer, cc24::checksum(reference));
            std::cout << "modified Character verified\n";
        } else {
            std::cerr << "usage: character_demo write OUT | modify IN OUT CHECKSUM | verify IN\n";
            return 2;
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
