#include <array>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <limits>

#include <boost/program_options.hpp>

#include <api.hpp>
#include <io.hpp>

std::ostream &operator<<(std::ostream &s, const std::vector<char> &v)
{
  s << std::string(v.data(), v.size());
  return s;
}

template <typename T>
std::ostream &operator<<(std::ostream &s, const std::vector<T> &v)
{
  for (auto i : v) {
    s << i << " ";
  }
  return s;
}

template <typename T>
struct GetSymbol { };

template <typename Alphabet, typename Source, typename ParseKeeper, typename LiteralKeeper>
struct GetSymbol<rlz::index<Alphabet, Source, ParseKeeper, LiteralKeeper>>
{
  using Symbol = typename Alphabet::Symbol;
};

class call {
private:
  std::string output;
public:
  call(std::string output) : output(output) { }

  template <typename Index>
  void invoke(Index &idx)
  {
    using Symbol = typename GetSymbol<Index>::Symbol;
    const constexpr size_t buffer_length = 4194304;
    std::array<Symbol, buffer_length> buffer;
    const char *buf_ptr = reinterpret_cast<const char*>(buffer.data());

    std::ofstream out(output, std::ios_base::binary);

    auto idx_length = idx.size();
    for (auto start = 0UL; start < idx_length; start += buffer_length) {
      auto end = std::min<size_t>(start + buffer_length, idx_length);
      idx(start, end, buffer.begin());
      out.write(buf_ptr, sizeof(Symbol) * (end - start));
    }
  }
};

int main(int argc, char **argv)
{
  using std::string;
  namespace po = boost::program_options;
  po::options_description desc;
  po::variables_map vm;
  try {
    desc.add_options()
        ("index-file,i", po::value<string>()->required(),
         "Index filename.")
        ("reference-file,r", po::value<string>()->required(),
         "Reference file.")
        ("output-file,o", po::value<string>()->required(),
         "Output file.");

    po::positional_options_description pd;
    pd.add("index-file", 1).add("reference-file", 1).add("output-file", 1);

    try {
      po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);
      po::notify(vm);
    } catch (boost::program_options::error &e) {
      throw std::runtime_error(e.what());
    }

    string index      = vm["index-file"].as<string>();
    string reference  = vm["reference-file"].as<string>();
    string output     = vm["output-file"].as<string>();

    call c { output };
    rlz::serialize::load_stream(index.c_str(), reference.c_str(), c);
     
  } catch (std::exception &e) {
    std::cerr << e.what() << "\n"
              << "Command-line options:"  << "\n"
              << desc << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
