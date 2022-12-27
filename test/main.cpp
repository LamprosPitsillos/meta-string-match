#include <iostream>

#include "meta_string_match.h"

using size_t = std::size_t;

void test_func(size_t) noexcept { }

const char* matched_string;
CREATE_META_STRING_MATCHER(string_matcher, "hi|hi2|hi3|hi33|hi44|\\||\\|| ||||||\\\\|", 
		[](size_t) { matched_string = "hi"; }, 
		[](size_t) { matched_string = "hi2"; }, 
		[](size_t) { matched_string = "hi3"; }, 
		[](size_t) { matched_string = "hi33"; }, 
		[](size_t) { matched_string = "hi44"; }, 
		[](size_t) { matched_string = "|"; }, 
		[](size_t) { matched_string = "| instance 2"; }, 
		[](size_t) { matched_string = "__space__"; }, 
		test_func, 
		test_func, 
		test_func, 
		test_func, 
		test_func, 
		[](size_t) { matched_string = "\\"; }, 
		[](size_t) { matched_string = "absolutely nothing"; }
		);

void output_table() noexcept {
	for (size_t y = 0; y < decltype(string_matcher)::length; y++) {
		for (size_t x = 0; x < (unsigned char)-1; x++) {
			size_t thing = ((string_matcher.data[y][x].next_state_ptr - string_matcher.data[0]) / ((size_t)(unsigned char)-1 + 1));
			if (string_matcher.data[y][x].next_state_ptr == nullptr) { thing = 0; }
			std::cout << " { " << thing << ", " << string_matcher.data[y][x].callback << " } ";
		}
		std::cout << "\n";
	}
}

int main() {
	output_table();

	int character;
	while ((character = std::cin.get()) != EOF) {
		if (string_matcher.match_character(character)) {
			std::cout << "match detected => " << matched_string << '\n'; 
		}
	}
}
