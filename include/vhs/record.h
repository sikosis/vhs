#ifndef VHS_RECORD_H
#define VHS_RECORD_H

#include <iosfwd>

namespace vhs {

int recordTape(std::ostream& tape, std::ostream& error);

} // namespace vhs

#endif

