#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <list>
#include <string>
#include <sstream>
#include <vector>

#include <boost/program_options.hpp>

#include <sdsl/io.hpp>

#include <api.hpp>
#include <classic_parse.hpp>
#include <io.hpp>
#include <generic_caller.hpp>
#include <match_serialize.hpp>
#include <type_listing.hpp>

REGISTER(DNA, rlz::alphabet::dna<>, "dna");
REGISTER(Dlcp32, rlz::alphabet::dlcp_32, "dlcp32");

LIST(Alphabets, DNA, Dlcp32);

REGISTER(Value_32, rlz::values::Size<32UL>, "32");
REGISTER(Value_64, rlz::values::Size<32UL>, "64");
LIST(BigLengths, Value_32);

CALLER(Alphabets, BigLengths);

class invoke {
private:
  std::string input;
  std::string reference;
  std::string output;
  std::vector<rlz::match> matching_stats;
  rlz::classic::parser parse;

  template <typename Index>
  void process(Index &idx) {
    using namespace std::chrono;
    std::cout << "--- Index size: " << sdsl::size_in_bytes(idx) << " bytes" << std::endl;
    std::cout << "=== Serializing... " << std::flush;
    auto t_1 = high_resolution_clock::now();
    rlz::serialize::store(idx, output.c_str());
    auto t_2 = high_resolution_clock::now();
    std::cout << duration_cast<milliseconds>(t_2 - t_1).count() << " ms" << std::endl;
  }

public:

  template <typename MS>
  invoke(std::string input, std::string reference, std::string output, MS &&matching_stats)
    : input(input),  reference(reference),  output(output), 
      matching_stats(std::forward<MS>(matching_stats))
  { }

  template <typename Alphabet, typename PtrSize>
  void call()
  {
    using namespace std::chrono;
    using Prefix        = rlz::classic::prefix::trivial;
    using LiteralKeeper = rlz::api::LiteralKeeper<Prefix>;
    using ParseKeeper   = rlz::api::ClassicParseKeeper<PtrSize>;

    auto t_1 = high_resolution_clock::now();
    if (matching_stats.empty()) {
      std::cout << "=== Building index... " << std::endl;
      auto index = rlz::construct_sstream<Alphabet, ParseKeeper, LiteralKeeper>(input.c_str(), reference.c_str(), parse);
      auto t_2 = high_resolution_clock::now();
      std::cout << "=== Build time: " << duration_cast<milliseconds>(t_2 - t_1).count() << " ms" << std::endl;
      process(index);
    } else {
      std::cout << "=== Building index (accelerated)... " << std::endl;
      auto m_begin = matching_stats.begin();
      auto m_end   = matching_stats.end();
      auto index = rlz::construct_sstream<Alphabet, ParseKeeper, LiteralKeeper>(input.c_str(), reference.c_str(), parse, m_begin, m_end);
      auto t_2 = high_resolution_clock::now();
      std::cout << "=== Build time: " << duration_cast<milliseconds>(t_2 - t_1).count() << " ms" << std::endl;
      process(index);
    }
  }
};

int main(int argc, char **argv)
{
  using std::string;
  using rlz::utils::options_string;
  using rlz::utils::allow;
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
        ("alphabet,A", po::value<string>()->default_value(default_ab.c_str()),
         ("Alphabet. Choices: " + options_string<Alphabets>()).c_str())
        ("output-file,o", po::value<string>()->required(),
         "Output file.")
        ("pointer-length,p", po::value<string>()->default_value("32"),
         ("Pointer size, in bits. Choices: " + options_string<BigLengths>() + ".").c_str())
        ("accelerate,a", po::value<string>(),
         "Filename containing matching stats (optional)");

    po::positional_options_description pd;
    pd.add("input-file", 1).add("reference-file", 1).add("output-file", 1).add("alphabet", 1);

    try {
      po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);
      po::notify(vm);
    } catch (boost::program_options::error &e) {
      throw std::runtime_error(e.what());
    }

    // Collect parameters
    string infile     = vm["input-file"].as<string>();
    string reference  = vm["reference-file"].as<string>();
    string alphabet   = vm["alphabet"].as<string>();
    string outfile    = vm["output-file"].as<string>();
    std::vector<std::string> vtypes = {
      alphabet,
      vm["pointer-length"].as<string>()
    };

    // Check
    if (!allow<Alphabets>(alphabet)) {
      throw std::logic_error(alphabet + " is not a valid alphabet.");
    }    
    if (!allow<BigLengths>(vtypes[1])) {
      throw std::logic_error(vtypes[1] + " is not a valid pointer size.");
    }

    std::cout << "--- Input:          " << infile << "\n"
              << "--- Reference:      " << reference << "\n"
              << "--- Alphabet        " << alphabet << "\n"
              << "--- Output:         " << outfile << "\n"
              << "--- Pointer size:   " << vtypes[1] << std::endl;

    // Load matching stats
    std::vector<rlz::match> matches;
    if (vm.count("accelerate") > 0) {
      std::ifstream match_stream(vm["accelerate"].as<string>());
      if (!match_stream.good()) {
        throw std::logic_error("Match file not readable");
      }
      matches = rlz::serialize::matches::load(match_stream);
    }
    // Invoke function
    invoke ivk(infile, reference, outfile, matches);
    rlz::utils::call<Caller>(vtypes.begin(), vtypes.end(), ivk);

  } catch (std::exception &e) {
    std::cerr << e.what() << "\n"
              << "Command-line options:"  << "\n"
              << desc << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
