#include <algorithm>
#include <iostream>

#include <api.hpp>
#include <containers.hpp>
#include <io.hpp>

#include <boost/program_options.hpp>

template <typename Index>
struct index_alphabet_trait {

};

template <typename Alphabet, typename Source, typename PK, typename LK>
struct index_alphabet_trait<rlz::index<Alphabet, Source, PK, LK>> {
  using alphabet = Alphabet;
  using symbol   = typename Alphabet::Symbol;
};

class checker {
private:
  std::string input;
  bool *correctness;

  template <typename Alphabet>
  rlz::managed_wrap<Alphabet> get_input()
  {
    using Symbol = typename Alphabet::Symbol;
    size_t size;
    auto u_ptr = rlz::io::read_file<Symbol>(input.c_str(), &size);
    return rlz::managed_wrap<Alphabet>(
      std::shared_ptr<Symbol>(u_ptr.release(), std::default_delete<Symbol[]>{}), 
      size
    );
  }

public:
  checker(std::string input, bool *correctness)
    : input(input), correctness(correctness)
  { }

  template <typename Index>
  void invoke(Index &index)
  {
    using Alphabet = typename index_alphabet_trait<Index>::alphabet;

    auto input_data = get_input<Alphabet>();
    auto index_data = index(0, index.size());

    *correctness = input_data.size() == index_data.size() && 
                   std::equal(input_data.begin(), input_data.end(), index_data.begin());
  }
};

int main(int argc, char **argv)
{
  using std::string;
  namespace po = boost::program_options;
  po::options_description desc;
  po::variables_map vm;
  bool equal;
  try {
    desc.add_options()
        ("index,i", po::value<string>()->required(),
         "Index file, to be checked against reference and input.")
        ("input,s", po::value<string>()->required(),
         "Input file, that is, the indexed file.")
        ("reference,r", po::value<string>()->required(),
         "Reference file.");

    po::positional_options_description pd;
    pd.add("input", 1).add("reference", 1).add("index", 1);

    try {
      po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);
      po::notify(vm);
    } catch (boost::program_options::error &e) {
      throw std::runtime_error(e.what());
    }

    // Collect parameters
    string index      = vm["index"].as<string>();
    string input      = vm["input"].as<string>();
    string reference  = vm["reference"].as<string>();

    // Invoke function
    checker c(input, &equal);
    rlz::serialize::load_stream(index.c_str(), reference.c_str(), c);
  } catch (std::exception &e) {
    std::cerr << e.what() << "\n"
              << "Command-line options:"  << "\n"
              << desc << std::endl;
    return EXIT_FAILURE;
  }
  return equal ? EXIT_SUCCESS : EXIT_FAILURE;
}