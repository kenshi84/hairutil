#include "output_file.h"
#include "util.h"

std::string OutputFile::operator()() const {
    const std::string suffix = globals::extra_suffix.empty() ? "" : "_" + globals::extra_suffix;
    return util::path_under_optional_dir(func() + suffix, dir);
}
