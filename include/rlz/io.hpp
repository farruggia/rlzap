#pragma once

#include <iterator>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>

#include <boost/iterator/iterator_facade.hpp>

namespace rlz {
namespace io {
class ioexception : public std::runtime_error {
public:
  explicit ioexception(const char *what) : std::runtime_error(what) { }
};

void open_file(std::ifstream &file, const char *name);

void open_file(std::ofstream &file, const char *name);

std::streamoff stream_length(std::istream &f) throw (ioexception);

template <typename T>
void read_some(std::istream &f, T *data, std::streamsize length) throw (ioexception)
{
  f.read(reinterpret_cast<char*>(data), sizeof(T) * length);
}

template <typename T>
void read_data(std::istream &f, T *data, std::streamsize length) throw (ioexception)
{
  f.read(reinterpret_cast<char*>(data), sizeof(T) * length);
  if (f.bad() or f.fail())
    throw ioexception("Failed to read file");
}

template <typename T>
void read_file(std::ifstream &f, T *data, std::streamsize length) throw (ioexception)
{
  f.seekg(0, std::ios_base::beg);
  read_data(f, data, length);
}

template <typename T>
void write_file(std::ofstream &f, T *data, std::streamsize length) throw (ioexception)
{
  f.write(reinterpret_cast<const char*>(data), sizeof(T) * length);
  if (f.bad() or f.fail())
    throw ioexception("Failed to write on file");
}

template <typename T>
std::unique_ptr<T[]> read_stream(std::istream &stream, size_t *size = nullptr, unsigned int extra = 0)
{
  auto len = stream_length(stream) / sizeof(T);
  if (size != nullptr) { *size = len; }
  std::unique_ptr<T[]> to_ret(new T[len + extra]);
  read_data(stream, to_ret.get(), len);
  return std::move(to_ret);
}


template <typename T>
std::unique_ptr<T[]> read_file(const char *name, size_t *size = nullptr, unsigned int extra = 0)
{
  std::ifstream file;
  open_file(file, name);
  size_t l_size = stream_length(file);
  l_size = l_size / sizeof(T);
  if (size != nullptr) {
    *size = l_size;
  }
  std::unique_ptr<T[]> to_ret(new T[l_size + extra]);
  std::fill(to_ret.get(), to_ret.get() + l_size + extra, T());
  read_file(file, to_ret.get(), static_cast<std::streamsize>(l_size));
  return std::move(to_ret);
}

template <typename T>
void write_file(const char *name, T *data, std::streamsize length) throw (ioexception)
{
  std::ofstream f;
  open_file(f, name);
  write_file(f, data, length);
}

}
}
