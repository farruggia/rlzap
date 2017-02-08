#include <cstdlib>
#include <cstdint>
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

class call {
private:
  size_t start;
  size_t end;
public:
  call(size_t start, size_t end) : start(start), end(end) { }

  template <typename Index>
  void invoke(Index &idx)
  {
		start = std::min(start, idx.size());
		end 	= std::min(end, idx.size());
    std::cout << idx(start, end);
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
        ("start,s", po::value<size_t>()->default_value(0),
         "Extraction start.")
        ("end,e", po::value<size_t>()->default_value(std::numeric_limits<size_t>::max()),
         "Extraction end.");

    po::positional_options_description pd;
    pd.add("index-file", 1).add("reference-file", 1).add("start", 1).add("end", 1);

    try {
      po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);
      po::notify(vm);
    } catch (boost::program_options::error &e) {
      throw std::runtime_error(e.what());
    }

    string index      = vm["index-file"].as<string>();
    string reference  = vm["reference-file"].as<string>();
    size_t start      = vm["start"].as<size_t>();
    size_t end        = vm["end"].as<size_t>();

    call c { start, end };
    rlz::serialize::load_stream(index.c_str(), reference.c_str(), c);
     
  } catch (std::exception &e) {
    std::cerr << e.what() << "\n"
              << "Command-line options:"  << "\n"
              << desc << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
