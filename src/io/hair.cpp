#include "io.h"

std::shared_ptr<cyHairFile> io::load_hair(const std::string &filename) {
    std::shared_ptr<cyHairFile> hairfile = std::make_shared<cyHairFile>();

    int res = hairfile->LoadFromFile(filename.c_str());

    const std::unordered_map<int, std::string> error_messages = {
        {CY_HAIR_FILE_ERROR_CANT_OPEN_FILE, "cannot open file"},
        {CY_HAIR_FILE_ERROR_CANT_READ_HEADER, "cannot read header"},
        {CY_HAIR_FILE_ERROR_WRONG_SIGNATURE, "wrong signature"},
        {CY_HAIR_FILE_ERROR_READING_SEGMENTS, "failed reading segments"},
        {CY_HAIR_FILE_ERROR_READING_POINTS, "failed reading points"},
        {CY_HAIR_FILE_ERROR_READING_THICKNESS, "failed reading thickness"},
        {CY_HAIR_FILE_ERROR_READING_TRANSPARENCY, "failed reading transparency"},
        {CY_HAIR_FILE_ERROR_READING_COLORS, "failed reading colors"},
    };

    if (res < 0) {
        throw std::runtime_error(fmt::format("Error while loading {}: {}", filename, error_messages.at(res)));
    }

    return hairfile;
}

void io::save_hair(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile) {
    hairfile->SaveToFile(filename.c_str());
}
