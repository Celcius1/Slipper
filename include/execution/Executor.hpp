#pragma once
#include "string"
#include "vector"
#include "memory"
#include "utility"

namespace Slipper {
namespace Execution {

class Executor {
public:
    Executor(const std::vector< std::string >& repo_paths);

    bool slipQueue(const std::vector< std::string >& merge_queue, bool stream_output);

    bool slipOut(const std::string& cpv);

private:
    std::vector< std::string > repos;

    std::string findEbuildPath(const std::string& cpv) const;

    std::vector< std::pair< std::string, std::string > > extractSrcUri(const std::string& ebuild_path) const;
    
    bool fetchSources(const std::vector< std::pair< std::string, std::string > >& uris) const;

    bool executeEbuildPhases(const std::string& ebuild_path, const std::string& cpv, bool stream_output) const;
    
    bool mergeImage(const std::string& cpv, const std::string& ebuild_path) const;

    bool clean(const std::string& cpv) const;

    bool filterEnvironment(const std::string& workdir) const;
};

} // namespace Execution
} // namespace Slipper