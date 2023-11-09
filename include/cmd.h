#include "common.h"

namespace cmd {

namespace parse {
void convert(args::Subparser &parser);
void decompose(args::Subparser &parser);
void info(args::Subparser &parser);
void resample(args::Subparser &parser);
void subsample(args::Subparser &parser);
}

namespace exec {
std::shared_ptr<cyHairFile> convert(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> decompose(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> info(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> resample(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> subsample(std::shared_ptr<cyHairFile> hairfile_in);
}

}
