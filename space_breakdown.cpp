#include <iostream>
#include <sstream>

#include <boost/program_options.hpp>

#include <api.hpp>
#include <containers.hpp>
#include <io.hpp>

class output_stats {
  std::string out_file;
  bool html;
public:
  output_stats(std::string out_file, bool html) : out_file(out_file), html(html)
  { }

  template <typename Index>
  void invoke(Index &idx)
  {
    std::stringstream output;
    if (html) {
      sdsl::write_structure<sdsl::HTML_FORMAT>(idx, output);
    } else {
      sdsl::write_structure<sdsl::JSON_FORMAT>(idx, output);
    }
    auto data = output.str();
    rlz::io::write_file(out_file.c_str(), data.c_str(), data.size());
  }
};

struct empty_wrap_factory {
  template <typename Alphabet>
  rlz::text_wrap<Alphabet> get() const
  {
    return rlz::text_wrap<Alphabet>{};
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
        ("index,i", po::value<string>()->required(),
         "Index filename.")
        ("output,o", po::value<string>()->required(),
         "Visualization output.")
        ("html,h", "Produce output in HTML format");

    po::positional_options_description pd;
    pd.add("index", 1).add("output", 1);

    try {
      po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);
      po::notify(vm);
    } catch (boost::program_options::error &e) {
      throw std::runtime_error(e.what());
    }

    string index  = vm["index"].as<string>();
    string output = vm["output"].as<string>();
    bool   html   = vm.count("html") > 0U;

    output_stats c { output, html };
    empty_wrap_factory factory;
    rlz::serialize::load_factory(index.c_str(), factory, c);
  } catch (std::exception &e) {
    std::cerr << e.what() << "\n"
              << "Command-line options:"  << "\n"
              << desc << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}