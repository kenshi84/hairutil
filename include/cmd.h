#include "common.h"

namespace cmd {

namespace parse {
void convert(args::Subparser &parser);
void subsample(args::Subparser &parser);
}

namespace exec {
std::shared_ptr<cyHairFile> convert(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> subsample(std::shared_ptr<cyHairFile> hairfile_in);
}

}
