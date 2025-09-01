#pragma once

#include "common.h"

namespace io {

using load_func_t = std::function<std::shared_ptr<cyHairFile>(const std::string &filename)>;
using save_func_t = std::function<void(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile)>;

std::shared_ptr<cyHairFile> load_bin(const std::string &filename);
std::shared_ptr<cyHairFile> load_hair(const std::string &filename);
std::shared_ptr<cyHairFile> load_data(const std::string &filename);
std::shared_ptr<cyHairFile> load_ply(const std::string &filename);
std::shared_ptr<cyHairFile> load_ma(const std::string &filename);
std::shared_ptr<cyHairFile> load_abc(const std::string &filename);
std::shared_ptr<cyHairFile> load_npy(const std::string &filename);

void save_bin(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile);
void save_hair(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile);
void save_data(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile);
void save_ply(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile);
void save_ma(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile);
void save_abc(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile);
void save_npy(const std::string &filename, const std::shared_ptr<cyHairFile> &hairfile);

}

namespace globals {
    extern const std::unordered_map<std::string, std::pair<::io::load_func_t, ::io::save_func_t>> supported_ext;
    extern ::io::load_func_t load_func;
    extern ::io::save_func_t save_func;
}
