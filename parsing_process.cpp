#include <iostream>

#include <api.hpp>
#include <io.hpp>

#include <boost/program_options.hpp>

class call {
public:
  call() { }

  template <typename Index>
  void invoke(Index &idx)
  {
    auto parsing = idx.get_parsing();
    for (auto &i : parsing) {
      std::int64_t offset;
      size_t copy_len;
      size_t junk_len;
      std::tie(offset, copy_len, junk_len) = i;
      std::cout << offset << "\t" << copy_len << "\t" << junk_len << std::endl;
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
          "Reference filename");

    po::positional_options_description pd;
    pd.add("index-file", 1).add("reference-file", 1);

    try {
      po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);
      po::notify(vm);
    } catch (boost::program_options::error &e) {
      throw std::runtime_error(e.what());
    }


    string index      = vm["index-file"].as<string>();
    string reference  = vm["reference-file"].as<string>();

    call c;
    rlz::serialize::load_stream(index.c_str(), reference.c_str(), c);

  } catch (std::exception &e) {
    std::cerr << e.what() << "\n"
              << "Command-line options:"  << "\n"
              << desc << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}