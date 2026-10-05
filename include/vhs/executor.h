#ifndef VHS_EXECUTOR_H
#define VHS_EXECUTOR_H

#include <iosfwd>
#include <string>

namespace vhs {

int executeTapeFile(const std::string& path, std::ostream& output, std::ostream& error);

} // namespace vhs

#endif

