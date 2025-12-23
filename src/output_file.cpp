#include "output_file.h"
#include "util.h"

std::string OutputFile::operator()() const {
    return util::path_under_optional_dir(func(), dir);
}
