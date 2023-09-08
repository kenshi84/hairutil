#include "common.h"

namespace cmd {

namespace parse {
void convert(args::Subparser &parser);
}

namespace exec {
std::shared_ptr<cyHairFile> convert(std::shared_ptr<cyHairFile> hairfile_in);
}

}
