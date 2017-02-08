#include <rlz/api.hpp> 					// Main API
#include <rlz/containers.hpp>		// For wrapping reference
#include <sdsl/int_vector.hpp>

#include <algorithm>
#include <iostream>
#include <sstream>

const char *reference = "ACGTTTTTTTCGCGCTTCGNNNCCCC";
const char *input     = "TNGCGCTTCANGTTTTTTGNNNCCCG";

template <typename T>
std::ostream &operator<<(std::ostream &s, const std::vector<T> &v)
{
	for (auto &i : v) {
		s << i << " ";
	}
	return s;
}

struct Callback {
	template <typename Index>
	void invoke(Index &idx)
	{
		// Source set automatically by the library
		std::cout << "idx callback content: "
							<< idx(0, idx.size())
							<< std::endl;
	}
};

int main()
{
	// Construct an index, DNA alphabet, from const char *pointers
	using DNA 				 = rlz::alphabet::dna<>; // Using the DNA alphabet	
	auto reference_len = std::string(reference).size();
	auto input_len 		 = std::string(input).size();
	auto index_iter = rlz::construct_iterator<DNA>(input, input + input_len, std::string(reference));

	std::cout << "index_ptr content: "
						<< index_iter(0, index_iter.size())
						<< std::endl; 

	// Construct an index, integers-of-length-7 alphabet, from SDSL vectors
	using Int7 = rlz::alphabet::Integer<7UL>;
	sdsl::int_vector<> ref_vec(reference_len, 0UL), input_vec(input_len, 0UL);
	std::copy(reference, reference + reference_len, ref_vec.begin());
	std::copy(input, input + input_len, input_vec.begin());
	auto index_sdsl = rlz::construct_iterator<Int7>(input_vec.begin(), input_vec.end(), std::move(ref_vec)); 

	std::cout << "index_sdsl content: "
						<< index_sdsl(0, index_sdsl.size())
						<< std::endl;

	// Construct an index, DNA, from streams
	std::stringstream ref_ss{std::string(reference)}, input_ss{std::string(input)};
	auto index_stream = rlz::construct_sstream<DNA>(input_ss, ref_ss);
	std::cout << "index_stream content: "
						<< index_stream(0, index_stream.size())
						<< std::endl;
	// Serialize and unloads an index, reference available as a container.
	// Invokes a callback for processing the index.
	std::stringstream index_dump_2;
	rlz::serialize::store(index_stream, index_dump_2);
	Callback c; // Careful: load_reference takes the callback as a reference, so it cannot be passed as a rvalue to load_reference
	rlz::text_wrap<DNA> ref_container { reference, reference_len}; // Container holding reference
	rlz::serialize::load_reference<DNA>(index_dump_2, ref_container, c); 

	// Serialize and unloads an index, reference serialized in a stream.
	// Invokes a callback for processing the index.
	std::stringstream index_dump_3;
	rlz::serialize::store(index_stream, index_dump_3);
	std::stringstream reference_ss{std::string(reference)};
	rlz::serialize::load_stream(index_dump_3, reference_ss, c);

	// Serialize and unloads an index. This method requires to know the index type, so it's not recommended.
	std::stringstream index_dump;
	sdsl::serialize(index_stream, index_dump);
	std::stringstream read_index(index_dump.str());
	rlz::index<DNA, rlz::text_wrap<DNA>> idx_load; // Alphabet and Source container type
	idx_load.load(read_index);
	idx_load.set_source(ref_container);	// Since reference is not serialized, it must be set manually
	std::cout << "idx_load content: "
						<< idx_load(0, idx_load.size())
						<< std::endl;
}
