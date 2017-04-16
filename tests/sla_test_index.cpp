#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>

#include "include/sla.hpp"

#include <lcp/api.hpp>

#include <boost/program_options.hpp>
#include <boost/function_output_iterator.hpp>

using Iterator    = relative::SLArray::iterator;
using Value       = relative::SLArray::value_type;
using Index       = rlz::lcp::LcpIndex<Iterator, Value>;
using Source      = rlz::lcp::detail::index_get_types<Index>::Source;

template <typename It>
class checker {
  It begin, it, end;
public:
  checker(It begin_, It end_)
    : begin(begin_), it(begin_), end(end_)
  { }

  void operator()(const Value &v)
  {
    auto exp = *it++;
    if (v != exp) {
      std::stringstream ss;
      ss << "Index and reference differ at position "
         << (std::distance(begin, it) - 1)
         << ": " << exp << " vs " << v;
      throw std::logic_error(ss.str());
    }
  }
};

void check_correctness(Index &idx, relative::SLArray &input_vec)
{
  checker<Iterator> check(input_vec.begin(), input_vec.end());
  idx(0, idx.size(), boost::make_function_output_iterator(check));
}

int main(int argc, char **argv)
{
  namespace po = boost::program_options;
  po::options_description general_options("Options options", 120U);
  po::variables_map vm;
  try {

    general_options.add_options()
      ("input,i", po::value<std::string>()->required(), "Input file.")
      ("reference,r", po::value<std::string>()->required(), "Reference file.")
      ("index,I", po::value<std::string>()->required(), "Index file.");
    po::positional_options_description pd;
    pd.add("input", 1).add("reference", 1).add("index", 1);
    po::store(po::command_line_parser(argc, argv).options(general_options).positional(pd).run(), vm);
    po::notify(vm);

    auto input_file     = vm["input"].as<std::string>();
    auto index_file     = vm["index"].as<std::string>();
    auto reference_file = vm["reference"].as<std::string>();

    std::cout << ">>> Loading input/reference... " << std::flush;
    auto t_1 = std::chrono::high_resolution_clock::now();
    relative::SLArray input_vec, ref_vec;
    std::ifstream input_stream(input_file, std::ios_base::binary),
                  reference_stream(reference_file, std::ios_base::binary);
    input_vec.load(input_stream);
    ref_vec.load(reference_stream);
    auto t_2 = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::seconds>(t_2 - t_1).count()
              << " secs." << std::endl;

    std::cout << ">>> Loading index... " << std::flush;
    t_1 = std::chrono::high_resolution_clock::now();
    Index idx;
    std::ifstream index_stream(index_file, std::ios_base::binary);
    idx.load(index_stream);
    Source source(ref_vec.begin(), ref_vec.end());
    idx.set_source(source);
    t_2 = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::seconds>(t_2 - t_1).count()
              << " secs." << std::endl;

    std::cout << ">>> Checking correctness... " << std::flush;
    t_1 = std::chrono::high_resolution_clock::now();
    check_correctness(idx, input_vec);
    t_2 = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::seconds>(t_2 - t_1).count()
              << " secs." << std::endl;
  } catch (boost::program_options::error &e) {
      std::cerr << "ERROR: "
                << e.what() << ".\n\n"
                << general_options << std::endl;    
  } catch (std::exception &e) {
    std::cerr << "ERROR: "
              << e.what() << "."
              << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}