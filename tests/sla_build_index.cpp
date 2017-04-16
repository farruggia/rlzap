#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "include/sla.hpp"

#include <lcp/api.hpp>

#include <boost/program_options.hpp>

using Iterator = relative::SLArray::iterator;
using Value    = relative::SLArray::value_type;
using Index    = rlz::lcp::LcpIndex<Iterator, Value>;

Index build_idx(const std::string &input_name, const std::string &reference_name, unsigned int limit)
{
  // Load SLArrays from disk
  std::ifstream input_file(input_name, std::ios_base::binary),
                reference_file(reference_name, std::ios_base::binary);

  relative::SLArray input_vec, reference_vec;
  input_vec.load(input_file);
  reference_vec.load(reference_file);

  // Build and return index
  return rlz::lcp::index_build<Value>(
    input_vec.begin(), input_vec.end(),
    reference_vec.begin(), reference_vec.end(),
    limit
  );
}

template <typename Idx>
void write_idx(Idx &idx, const std::string &output_file)
{
  std::ofstream out_file(output_file, std::ios_base::binary | std::ios_base::trunc);
  idx.serialize(out_file);
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
      ("output,o", po::value<std::string>()->required(), "Output file.")
      ("limit,l", po::value<unsigned int>()->default_value(1024), "Phrase limit");
    po::positional_options_description pd;
    pd.add("input", 1).add("reference", 1).add("output", 1);
    po::store(po::command_line_parser(argc, argv).options(general_options).positional(pd).run(), vm);
    po::notify(vm);

    auto input_file      = vm["input"].as<std::string>();
    auto reference_file  = vm["reference"].as<std::string>();
    auto output_file     = vm["output"].as<std::string>();
    auto limit           = vm["limit"].as<unsigned int>();

    std::cout << ">>> Building index... " << std::flush;
    auto t_1 = std::chrono::high_resolution_clock::now();
    auto idx = build_idx(input_file, reference_file, limit);
    auto t_2 = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::seconds>(t_2 - t_1).count()
              << " secs." << std::endl;

    std::cout << ">>> Writing it on disk... " << std::flush;
    t_1 = std::chrono::high_resolution_clock::now();
    write_idx(idx, output_file);
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