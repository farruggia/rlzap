#include <iostream>
#include <iterator>
#include <string>
#include <tuple>

#include <alphabet.hpp>
#include <dumper.hpp>
#include <io.hpp>
#include <get_matchings.hpp>
#include <match_serialize.hpp>
#include <type_listing.hpp>

#include <boost/program_options.hpp>

REGISTER(DNA, rlz::alphabet::dna<>, "dna");
REGISTER(Int16, rlz::alphabet::Integer<16UL>, "int16");
REGISTER(Int32, rlz::alphabet::Integer<32UL>, "int32");
REGISTER(Lcp16, rlz::alphabet::lcp_16, "lcp16");
REGISTER(Lcp32, rlz::alphabet::lcp_32, "lcp32");
REGISTER(Dlcp16, rlz::alphabet::dlcp_16, "dlcp16");
REGISTER(Dlcp32, rlz::alphabet::dlcp_32, "dlcp32");
LIST(Alphabets, DNA, Lcp32, Dlcp32); // First is default
CALLER(Alphabets);

class invoke {
  std::string input;
  std::string reference;
  std::string output;
public:

  invoke(std::string input, std::string reference, std::string output)
    : input(input), reference(reference), output(output)
  { }

  template <typename Alphabet>
  void call()
  {
    std::ifstream input_stream { input, std::ifstream::in},
                  ref_stream { reference, std::ifstream::in};
    
    rlz::utils::stream_dumper ref_dump(ref_stream), input_dump(input_stream);
    auto matches = std::get<0>(rlz::get_relative_matches<Alphabet>(ref_dump, input_dump));

    std::ofstream output_stream { output, std::ofstream::out};
    rlz::serialize::matches::store(output_stream, matches.begin(), matches.end());
  }
};

int main(int argc, char **argv)
{
  using std::string;
  using rlz::utils::options_string;
  using rlz::utils::options;
  namespace po = boost::program_options;
  po::options_description desc;
  po::variables_map vm;
  try {
    auto alphabets  = options<Alphabets>();
    auto default_ab = alphabets.front();
    desc.add_options()
        ("input-file,i", po::value<string>()->required(),
         "Input file, that is, file to be indexed.")
        ("reference-file,r", po::value<string>()->required(),
         "Reference file.")
        ("alphabet,a", po::value<string>()->default_value(default_ab.c_str()),
         ("Alphabet. Choices: " + options_string<Alphabets>()).c_str())
        ("output-file,o", po::value<string>()->required(),
         "Output file.");
    po::positional_options_description pd;
    pd.add("input-file", 1).add("reference-file", 1).add("output-file", 1);
    try {
      po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);
      po::notify(vm);
    } catch (boost::program_options::error &e) {
      throw std::runtime_error(e.what());
    }


    auto infile     = vm["input-file"].as<string>();
    auto reference  = vm["reference-file"].as<string>();
    auto alphabet   = vm["alphabet"].as<string>();
    auto outfile    = vm["output-file"].as<string>();

    invoke ivk{infile, reference, outfile};
    rlz::utils::call<Caller>(alphabet, ivk);

  } catch (std::exception &e) {
    std::cerr << e.what() << "\n"
              << "Command-line options:"  << "\n"
              << desc << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
