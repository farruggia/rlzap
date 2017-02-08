#include <cstdlib>
#include <cstdint>
#include <limits>

#include <boost/program_options.hpp>

#include <api.hpp>
#include <io.hpp>

template <typename T>
struct delta_bits { };

template <typename Ab, typename Source, typename PK, typename LK>
struct delta_bits<rlz::index<Ab, Source, PK, LK>> {
  static const constexpr size_t value = PK::delta_bits;
};

struct call {

  template <typename Index>
  void invoke(Index &idx)
  {
    auto db = delta_bits<Index>::value;

    std::int64_t diff;
    std::int64_t delta_lo   = db > 0 ? -(1 << (db - 1)) : 0;
    std::int64_t delta_hi   = db > 0 ? -delta_lo - 1 : 0;
    std::int64_t prev_abs   = 0;
    std::int64_t prev_delta = 0;

    auto is_new_phrase = [&] (std::int64_t ptr) -> bool {
      diff = ptr - prev_abs;
      return (diff < delta_lo) or (diff > delta_hi);
    };

    size_t absolutes         = 0UL;
    size_t deltas            = 0UL;
    size_t lit_total_len     = 0UL;
    size_t sum_diff_delta    = 0UL;
    size_t sum_insertions    = 0UL;
    size_t sum_deletions     = 0UL;
    size_t sum_substitutions = 0UL;
    auto process_phrase = [&](std::int64_t ptr, size_t phrase_len, size_t lit_len) -> void {
      if (is_new_phrase(ptr)) {
        prev_abs    = ptr;
        prev_delta  = 0UL;
        ++absolutes;
      } else {
        sum_diff_delta += std::abs(diff - prev_delta);
        prev_delta = diff;
        if (diff >= 0) {
          sum_substitutions += lit_len;
          sum_deletions     += diff;
        } else {
          sum_substitutions += std::max<std::int64_t>(lit_len + diff, 0L);
          sum_insertions    += -diff;
        }
        ++deltas;
      }
      lit_total_len += lit_len;
    };
    idx.process_parsing(process_phrase);

    std::cout << "Length           = " << idx.size() << "\n"
              << "Absolute Phrases = " << absolutes << "\n"
              << "Delta phrases    = " << deltas << "\n"
              << "Total phrases    = " << absolutes + deltas << "\n"
              << "Literals         = " << lit_total_len << "\n"
              << "Literals/phrase  = " << 1.0 * lit_total_len / (absolutes + deltas) << "\n"
              << "Average diff     = " << 1.0 * sum_diff_delta / deltas << "\n"
              << "Average ins      = " << 1.0 * sum_insertions / deltas << "\n"
              << "Average dels     = " << 1.0 * sum_deletions / deltas << "\n"
              << "Average subs     = " << 1.0 * sum_substitutions / deltas << std::endl;
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
         "Reference file.");

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
