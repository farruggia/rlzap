#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>

#include "include/sla.hpp"

#include <lcp/api.hpp>

#include <boost/program_options.hpp>
#include <boost/function_output_iterator.hpp>

void build(const std::string &input_file, const std::string &output_file)
{
  std::ifstream input(input_file);
  std::vector<unsigned int> vals;
  unsigned int val;
  while (input >> val) {
    vals.push_back(val);
  }

  std::ofstream out(output_file, std::ios_base::binary | std::ios_base::trunc);
  relative::SLArray built(vals);
  built.serialize(out);
}

int main(int argc, char **argv)
{
  namespace po = boost::program_options;
  po::options_description general_options("Options options", 120U);
  po::variables_map vm;
  try {

    general_options.add_options()
      ("input,i", po::value<std::string>()->required(), "Input file.")
      ("output,o", po::value<std::string>()->required(), "Output file.");
    po::positional_options_description pd;
    pd.add("input", 1).add("output", 1);
    po::store(po::command_line_parser(argc, argv).options(general_options).positional(pd).run(), vm);
    po::notify(vm);

    auto input_file  = vm["input"].as<std::string>();
    auto output_file = vm["output"].as<std::string>();
    build(input_file, output_file);
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