#include "io.hpp"

namespace rlz {
namespace io {

void open_file(std::ifstream &file, const char *name)
{
    file.open(name, std::ios::binary | std::ios::in);
}

void open_file(std::ofstream &file, const char *name)
{
    file.open(name, std::ios::binary | std::ios::out | std::ios::trunc);
}

std::streamoff stream_length(std::istream &f) throw (ioexception)
{
    auto cur_pos = f.tellg();
    f.seekg(0, std::ios_base::end);
    auto end = f.tellg();
    f.seekg(cur_pos);
    if (f.bad() or f.fail())
        throw ioexception("Failed to open file");
    return end;
}
    
}
}

