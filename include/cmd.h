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
void smooth(args::Subparser &parser);
void stats(args::Subparser &parser);
void subsample(args::Subparser &parser);
void transform(args::Subparser &parser);
void tubify(args::Subparser &parser);
}

namespace param {
    inline bool& b(const std::string& cmd_, const std::string& param_) { static std::unordered_map<std::string, std::unordered_map<std::string, bool>> m; return m[cmd_][param_]; }
    inline unsigned int& ui(const std::string& cmd_, const std::string& param_) { static std::unordered_map<std::string, std::unordered_map<std::string, unsigned int>> m; return m[cmd_][param_]; }
    inline float& f(const std::string& cmd_, const std::string& param_) { static std::unordered_map<std::string, std::unordered_map<std::string, float>> m; return m[cmd_][param_]; }
    inline std::string& s(const std::string& cmd_, const std::string& param_) { static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m; return m[cmd_][param_]; }
    inline std::set<int>& set_i(const std::string& cmd_, const std::string& param_) { static std::unordered_map<std::string, std::unordered_map<std::string, std::set<int>>> m; return m[cmd_][param_]; }
    inline std::optional<float>& opt_f(const std::string& cmd_, const std::string& param_) { static std::unordered_map<std::string, std::unordered_map<std::string, std::optional<float>>> m; return m[cmd_][param_]; }
    inline Eigen::Matrix4f& mat4f(const std::string& cmd_, const std::string& param_) { static std::unordered_map<std::string, std::unordered_map<std::string, Eigen::Matrix4f>> m; return m[cmd_][param_]; }
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
std::shared_ptr<cyHairFile> smooth(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> stats(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> subsample(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> transform(std::shared_ptr<cyHairFile> hairfile_in);
std::shared_ptr<cyHairFile> tubify(std::shared_ptr<cyHairFile> hairfile_in);
}

}

namespace globals {
    extern const std::set<std::shared_ptr<cyHairFile> (*)(std::shared_ptr<cyHairFile>)> cmd_wo_output;
}
