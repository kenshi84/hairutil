#include "common.h"

namespace cmd {

namespace parse {
void autofix(args::Subparser &parser);
void convert(args::Subparser &parser);
void decompose(args::Subparser &parser);
void filter(args::Subparser &parser);
void findpenet(args::Subparser &parser);
void getcurvature(args::Subparser &parser);
void info(args::Subparser &parser);
void resample(args::Subparser &parser);
void stats(args::Subparser &parser);
void subsample(args::Subparser &parser);
void transform(args::Subparser &parser);
}

namespace exec {
std::shared_ptr<cyHairFile> autofix(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> convert(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> decompose(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> filter(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> findpenet(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> getcurvature(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> info(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> resample(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> stats(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> subsample(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> transform(std::shared_ptr<cyHairFile> hairfile_in);
}

}

namespace globals {
    extern const std::set<std::shared_ptr<cyHairFile> (*)(std::shared_ptr<cyHairFile>)> cmd_wo_output;
}
