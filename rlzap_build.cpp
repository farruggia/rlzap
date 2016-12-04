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
#include <io.hpp>
#include <generic_caller.hpp>
#include <match_serialize.hpp>
#include <parse_rlzap.hpp>
#include <type_listing.hpp>

struct small_prefix {
  template <typename Rep, typename LengthWidth, typename SampleInterval>
  using type = rlz::prefix::cumulative<Rep, LengthWidth>;
};

struct sample_prefix {
  template <typename Rep, typename LengthWidth, typename SampleInterval>
  using type = rlz::prefix::sampling_cumulative<LengthWidth, SampleInterval>;
};

REGISTER(SmallPrefix, small_prefix, "bv");
REGISTER(SamplePrefix, sample_prefix, "sampling");
LIST(Prefix, SamplePrefix, /*FastPrefix,*/ SmallPrefix);

REGISTER(DNA, rlz::alphabet::dna<>, "dna");
REGISTER(Dlcp32, rlz::alphabet::dlcp_32, "dlcp32");

LIST(Alphabets, DNA, Dlcp32);

REGISTER(Value_2 , rlz::values::Size<2UL>,  "2");
REGISTER(Value_4 , rlz::values::Size<4UL>,  "4");
REGISTER(Value_6 , rlz::values::Size<6UL>,  "6");
REGISTER(Value_8 , rlz::values::Size<8UL>,  "8");
REGISTER(Value_16, rlz::values::Size<16UL>, "16");
REGISTER(Value_32, rlz::values::Size<32UL>, "32");
REGISTER(Value_48, rlz::values::Size<48UL>, "48");
REGISTER(Value_64, rlz::values::Size<64UL>, "64");
LIST(LiteralLengths, Value_2, Value_4, Value_8);
LIST(DiffLengths, Value_2, Value_4, Value_6, Value_8);
LIST(BigLengths, Value_32);
LIST(SampleLengths, Value_16, Value_32, Value_48, Value_64);

CALLER(Alphabets, Prefix, LiteralLengths, DiffLengths, BigLengths, SampleLengths);

size_t ab_bits(const std::string &s)
{
  if (s == "dlcp32") {
    return 32UL;
  } else if (s == "dna") {
    return 2UL;
  } else {
    std::stringstream ss;
    ss << "Unknown alphabet " << s;
    throw std::logic_error(ss.str());
  }
}

template <typename Parser>
class invoke {
private:
  std::string input;
  std::string reference;
  std::string output;
  std::vector<rlz::match> matching_stats;
  Parser parse;

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
  invoke(std::string input, std::string reference, std::string output, Parser parse, MS &&matching_stats)
    : input(input),  reference(reference),  output(output), 
      matching_stats(std::forward<MS>(matching_stats)),
      parse(parse)
  { }

  template <typename Alphabet, typename GPrefix, typename LitLength, typename DiffSize, typename PtrSize, typename SampleLengths>
  void call()
  {
    using namespace std::chrono;
    using Prefix        = typename GPrefix::template type<rlz::vectors::dense, LitLength, SampleLengths>;
    using LiteralKeeper = rlz::api::LiteralKeeper<Prefix>;
    using ParseKeeper   = rlz::api::ParseKeeper<PtrSize, DiffSize>;

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

enum struct Parser { classic, rlzap_automatic, rlzap_parameter };

#define MAP_NAME(var, name, field) if (var == "name") { return Parser::field; }

enum Parser name_to_parser(std::string name)
{
  if (name == "classic") { return Parser::classic; }
  if (name == "automatic") { return Parser::rlzap_automatic; }
  if (name == "parametric") { return Parser::rlzap_parameter; }
  throw std::logic_error(name + " is not a valid parsing strategy");
}

std::string parser_to_name(enum Parser kind) {
  switch (kind) {
  case Parser::classic:
    return "classic";
  case Parser::rlzap_parameter:
    return "parametric";
  case Parser::rlzap_automatic:
    return "automatic";
  default:
    throw std::logic_error("parser_to_name: unhandled case");
  }
}

std::string parser_choices()
{
  std::stringstream ss;
  ss << parser_to_name(Parser::classic) << ", "
     << parser_to_name(Parser::rlzap_automatic) << ", "
     << parser_to_name(Parser::rlzap_parameter) << ", ";
  return ss.str();
}

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
        ("parser,k", po::value<string>()->default_value(parser_to_name(Parser::rlzap_parameter).c_str()),
          ("Select a parsing strategy. Choices: " + parser_choices()).c_str())
        ("look-ahead,L", po::value<size_t>()->default_value(30UL),
         "Amount of look-ahead symbols.")
        ("explicit-len,E", po::value<size_t>()->default_value(15UL),
         "Minimum length of an explicit phrase, in symbols.")
        ("literal-strategy,l", po::value<string>()->default_value("bv"),
         ("Literal lengths datastructure implementation. Choices: " + options_string<Prefix>() + ".").c_str())
        ("max-lit,M", po::value<string>()->default_value("4"),
         ("Length of end-of-phrase literal length, in bits. Choices: " + options_string<LiteralLengths>() + ".").c_str())
        ("sample-int,S", po::value<string>()->default_value("32"),
         ("Literal sampling interval size, in elements. Choices: " + options_string<SampleLengths>() + ".").c_str())
        ("delta-bits,d", po::value<string>()->default_value("8"),
         ("Adaptive pointer length, in bits. Choices: " + options_string<DiffLengths>() + ".").c_str())
        ("explicit-bits,e", po::value<string>()->default_value("32"),
         ("Explicit pointer length, in bits. Choices: " + options_string<BigLengths>() + ".").c_str())
        ("accelerate,a", po::value<string>(),
         "File containing matching stats (optional)");

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
    Parser parser     = name_to_parser(vm["parser"].as<string>());
    size_t E_L        = vm["look-ahead"].as<size_t>();
    size_t P_T        = vm["explicit-len"].as<size_t>();
    std::vector<std::string> vtypes {{
      alphabet,
      vm["literal-strategy"].as<string>(),
      vm["max-lit"].as<string>(),
      vm["delta-bits"].as<string>(),
      vm["explicit-bits"].as<string>(),
      vm["sample-int"].as<string>()
    }};

    // Check
    if (!allow<Alphabets>(alphabet)) {
      throw std::logic_error(alphabet + " is not a valid alphabet.");
    }    
    if (!allow<Prefix>(vtypes[1])) {
      throw std::logic_error(vtypes[1] + " is not a valid literal-keeper implementation.");
    }
    if (!allow<LiteralLengths>(vtypes[2])) {
      throw std::logic_error(vtypes[2] + " is not a valid literal length.");
    }
    if (!allow<DiffLengths>(vtypes[3])) {
      throw std::logic_error(vtypes[3] + " is not a valid subphrase pointer size.");
    }
    if (!allow<BigLengths>(vtypes[4])) {
      throw std::logic_error(vtypes[4] + " is not a valid pointer size.");
    }
    if (!allow<SampleLengths>(vtypes[5])) {
      throw std::logic_error(vtypes[5] + " is not a valid sampling interval.");
    }

    std::cout << "--- Input:          " << infile << "\n"
              << "--- Reference:      " << reference << "\n"
              << "--- Alphabet        " << alphabet << "\n"
              << "--- Output:         " << outfile << "\n"
              << "--- Parser:         " << parser_to_name(parser) << "\n"
              << "--- Edit distance:  " << E_L << "\n"
              << "--- Phrase thsld:   " << P_T << "\n"
              << "--- Literal impl:   " << vtypes[1] << "\n"
              << "--- Literal size:   " << vtypes[2] << "\n"
              << "--- Literal sample: " << vtypes[5] << "\n"
              << "--- Subphrase size: " << vtypes[3] << "\n"
              << "--- Pointer size:   " << vtypes[4] << "\n"
              << std::endl;

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
    if (parser == Parser::classic) {
      invoke<rlz::Parser> ivk(infile, reference, outfile, rlz::Parser{E_L, P_T}, matches);
      rlz::utils::call<Caller>(vtypes.begin(), vtypes.end(), ivk);
    } else {
      size_t rlt_bits = std::stoi(vtypes[3]);
      size_t abs_bits = std::stoi(vtypes[4]);
      size_t sym_bits = ab_bits(alphabet);
      rlz::parser_rlzap parse;
      if (parser == Parser::rlzap_automatic) {
        rlz::parser_rlzap parse{rlt_bits, abs_bits, sym_bits};
        invoke<rlz::parser_rlzap> ivk(infile, reference, outfile, parse, matches);
        rlz::utils::call<Caller>(vtypes.begin(), vtypes.end(), ivk);            
      } else {
        assert(parser == Parser::rlzap_parameter);
        rlz::parser_rlzap parse{rlt_bits, abs_bits, sym_bits, E_L, P_T};
        invoke<rlz::parser_rlzap> ivk(infile, reference, outfile, parse, matches);
        rlz::utils::call<Caller>(vtypes.begin(), vtypes.end(), ivk);            
      }
    }

  } catch (std::exception &e) {
    std::cerr << e.what() << "\n"
              << "Command-line options:"  << "\n"
              << desc << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
