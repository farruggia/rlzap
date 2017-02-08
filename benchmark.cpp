#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <tuple>
#include <vector>

#include <boost/program_options.hpp>

#include <sdsl/io.hpp>

#include <papi.h>

#include <api.hpp>
#include <io.hpp>

#define GCC_TURN_OFF_DEAD_STORE_OPT(x) __asm__ volatile("" : "+g"(x));

template <typename T>
struct index_symbol {

};

template <typename Alphabet, typename Reference, typename ParseKeeper, typename LiteralKeeper>
struct index_symbol<rlz::index<Alphabet, Reference, ParseKeeper, LiteralKeeper>> {
  using Type = typename Alphabet::Symbol;
};

class performance_events {
private:
  std::map<int, const char *> event_desc;
  std::vector<long long int> values;

  void check_and_add(int evt, const char *evt_name)
  {
    if (PAPI_query_event(evt) == PAPI_OK) {
      event_desc[evt] = evt_name;
    }
  }
public:
  performance_events()
  {
    // Initialize the library
    PAPI_library_init(PAPI_VER_CURRENT);
    // Fill the events map
    check_and_add(PAPI_L1_DCM, "L1 cache misses");
    check_and_add(PAPI_L2_DCM, "L2 cache misses");
    check_and_add(PAPI_L3_DCM, "L3 cache misses");
    check_and_add(PAPI_L2_LDM, "L1 load misses");
    check_and_add(PAPI_L2_LDM, "L2 load misses");
    check_and_add(PAPI_L3_LDM, "L3 load misses");
    check_and_add(PAPI_TOT_INS, "Instructions");
    // Resize values
    values.resize(event_desc.size());
  
  }

  void start()
  {
    // Start recording
    std::vector<int> record_evts;
    for (auto i : event_desc) {
      record_evts.push_back(i.first);
    }
    PAPI_start_counters(record_evts.data(), record_evts.size());
  }

  void stop()
  {
    PAPI_stop_counters(values.data(), values.size());
  }

  void show(std::function<void(const char *, long long)> func)
  {
    // Collect events
    auto val_it = values.begin();
    for (auto i : event_desc) {
      func(i.second, *val_it++);
    }
  }
};

struct random_gen {
  std::uint32_t x, y, z, w;
  size_t max;
  random_gen(size_t max) : x(0x0F0C0FFE), y(0xDEADBEEF), z(0xEFF0C0F0), w(0xFEEBDAED), max(max) { }

  std::uint32_t operator()() {
    std::uint32_t t = x;
    t ^= t << 11;
    t ^= t >> 8;
    x = y; y = z; z = w;
    w ^= w >> 19;
    w ^= t;
    return w % max;
  }
};

class call {
private:
  size_t length;
  size_t times;
public:
  call(size_t length, size_t times) : length(length), times(times) { }

  template <typename Index>
  void invoke(Index &idx)
  {
    using T = typename index_symbol<Index>::Type;
    using namespace std::chrono;

    random_gen rg(idx.size() - length);
    auto t_1 = high_resolution_clock::now();
    {
      size_t f, s;
      for (size_t i = 0UL; i < times; ++i) {
        f = rg();
        s = f + length;
        GCC_TURN_OFF_DEAD_STORE_OPT(f);
        GCC_TURN_OFF_DEAD_STORE_OPT(s);
      }      
    }
    auto t_2 = high_resolution_clock::now();
    auto rgen_time = t_2 - t_1;

    std::vector<T> buffer(length, '0');
    auto buf_it = buffer.begin();
    performance_events pe;
    pe.start();
    t_1 = high_resolution_clock::now();
    size_t start, end;
    for (size_t i = 0UL; i < times; ++i) {
      start = rg();
      end   = start + length;
      idx(start, end, buf_it);
      GCC_TURN_OFF_DEAD_STORE_OPT(buf_it);
    }
    t_2 = high_resolution_clock::now();
    pe.stop();
    std::cout << "Average time   = "
              << duration_cast<nanoseconds>((t_2 - t_1 - rgen_time) / times).count() << "ns\n"
              << "Average length = "
              << length << "\n"
              << "Repetitions    = "
              << times << "\n"
              << "Index size     = "
              << sdsl::size_in_mega_bytes(idx) << std::endl;
    pe.show([&](const char *desc, long long int val) -> void {
      std::cout << desc << "\t= " << val / times << std::endl;
    });
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
         "Reference file.")
        ("length,l", po::value<size_t>()->required(),
         "Extraction length.")
        ("times,t", po::value<size_t>()->required(),
         "Number of extractions.");

    po::positional_options_description pd;
    pd.add("index-file", 1).add("reference-file", 1).add("length", 1).add("times", 1);

    try {
      po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);
      po::notify(vm);
    } catch (boost::program_options::error &e) {
      throw std::runtime_error(e.what());
    }

    string index      = vm["index-file"].as<string>();
    string reference  = vm["reference-file"].as<string>();
    size_t length     = vm["length"].as<size_t>();
    size_t times      = vm["times"].as<size_t>();

    call c { length, times };
    rlz::serialize::load_stream(index.c_str(), reference.c_str(), c);     
  } catch (std::exception &e) {
    std::cerr << e.what() << "\n"
              << "Command-line options:"  << "\n"
              << desc << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
