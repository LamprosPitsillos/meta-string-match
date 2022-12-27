#pragma once

#include <utility>
#include <type_traits>

// IMPORTANT: string matcher matches the longest match it finds and pays no attention to the order in which the strings are given in the spec

/*
TODO: Although my textbook about the topic doesn't mention this possibility, I figure that this table DFA system could be improved
with a bit of effort. Right now, we've got elements with next states and callback pointers. Basically, the table interpreter has to
always be on the lookout for hitting an invalid element (one where none of the strings continue to match). When it does it has to call
the callback function of the last match that it detected. This is easily done by storing the position in the input stream and the callback
pointer everytime a final state for a string is hit while jumping through the table. When you hit an invalid element, all you have to do
is call the stored callback pointer and pass in the stored input stream position as the argument.
THE POSSIBLE IMPROVEMENT IS AS SUCH: The job of keeping track of the last match can be done for free by the table itself, given enough
processing at compile-time. Obviously, this means longer compile times, but that probably won't be a big deal. Basically,
whenever a string ends and the last element (the one that calls the callback) is put into the table. You now know that every
invalid element from there on out must call it's callback, because from their POV, this is the latest match.
I don't exactly know what the algorithm is going to look like, but you can figure one out to efficiently make all the invalid
elements from there on out point to the last match. Each element is going to need something like an age property, so that,
if there is a match after this match because one of the other strings has already put a match further up the chain, we can
tell that the invalid elements that point to another callback have precedence over our proposed callback invalids.
I've called it age, but that might not be an appropraite name. You could do it like this: the age stores how far away in the chain
the actual end element that calls the callback is. You can only overwrite an callback invalid if the age is greater than the age
you would put into that element. Something along those lines. You're also gonna need an extra column in the state table so that
end of input event can trigger a callback in the table, then the interpreter doesn't really have to think at all.
Also, each invalid element would also have to store the offset to the input stream position that should be applied before
calling the callback. That way, the interpreter can read the offset, read the callback, subtract the offset from the current
stream position, and then call the callback with that new position.
*/

// TODO: I'm not sure exactly if the above TODO makes this optimization unnecessary, but you could add an option to find the shortest match
// instead of the longest. That would remove the overhead that you're complaining about in the above comment.
// Like I said, I don't know if it actually would do anything if we do the above, but if the system remains as it is right now,
// it would certainly make things faster to interpret.

namespace meta {

	using size_t = std::size_t;

	// NOTE: See srcembed for a detailed explanation of why this works. After reading that, you'll
	// also understand that [[noreturn]] probably isn't useless here.
	[[noreturn]] consteval void static_fail_with_msg(const char * const);

	struct const_string {
		const char * const data;
		const size_t length;

		template <size_t size>
		consteval const_string(const char (&string)[size]) : data(string), length(size - 1) { }

		consteval const_string(const char* const data, size_t length) : data(data), length(length) { }

		consteval const char& operator[](size_t index) const { return data[index]; }

		consteval const_string offset_by(size_t offset) const { return const_string(data + offset, length - offset); }
	};

	consteval size_t calculate_table_length(const const_string& meta_matcher_spec) {
		size_t result = 0;

		/*
		   We could use pointers for this loop stuff instead of indices in the hopes of making compile times shorter,
		   but who knows how much the compiler optimizes these consteval functions before interpreting them anyway.
		   I'm gonna simply rely on optimizations like I would if I were writing this for runtime, for the sake of expressiveness.
		   If the compile times get too bad, I'll do something about it then, when I know for sure that it's necessary.
		   One also has to remember, compilers will get better in the future and optimize these types of functions more and more,
		   so this code will compile faster and faster.
		*/
		for (size_t i = 0; i < meta_matcher_spec.length; i++) {
			unsigned char character = meta_matcher_spec[i];
			// NOTE: This if could be done away with if we used a table, but it's not a big deal, see below.
			if (character > 127) { static_fail_with_msg("invalid character (not in ASCII range) present in matcher specification"); }
			// NOTE: UTF8 contains ASCII, so even if that encoding is used, this check will still only filter out unwanted stuff.
			// As for systems that don't use ASCII or UTF8 for their character encoding in C++,
			// we're just gonna ignore those for now. I don't even know of any, very uncommon I guess.

			switch (character) {
			case '|': continue;
			case '\\':
				i++;
				if (i == meta_matcher_spec.length) {
					static_fail_with_msg("backslash ('\\') character cannot appear at end of matcher specification");
				}

				character = meta_matcher_spec[i];
				if (character > 127) { static_fail_with_msg("invalid character (not in ASCII range) present in matcher specification"); }

				// NOTE: Nested switches are technically less efficient than state tables,
				// but it's not that big of a deal for now. The expressiveness of this outweighs
				// it's cost, at least in compile-time.
				switch (character) {
				case '|': case '\\': result++; continue;
				default: static_fail_with_msg("invalid character following backslash ('\\') character in matcher specification");
				}
			default: result++; continue;
			}
		}

		return result;
	}

	struct string_matcher_with_indices_table_element_t {
		size_t next_state;
		void (*callback)(size_t);	// NOTE: callbacks get called with the position in the input character stream as the only parameter
	};

	inline constexpr size_t string_matcher_table_width = (size_t)(unsigned char)-1 + 1;
	// NOTE: We make the table as wide as the the num of values representable by char (which we elsewhere make sure is not bigger than 1 byte),
	// so that non-ASCII characters can be correctly identified as invalid without extra overhead when following the table.

	template <size_t template_length>
	class string_matcher_with_indices_t {
	public:
		static constexpr size_t length = template_length;

		string_matcher_with_indices_table_element_t data[length][string_matcher_table_width];
		// NOTE: Doesn't have to be a 1D array because this table uses indices and not pointers. See below.
	};

	struct string_matcher_table_element_t {
		string_matcher_table_element_t *next_state_ptr;
		void (*callback)(size_t);
	};

	template <size_t template_length>
	class string_matcher_t {
	public:
		static constexpr size_t length = template_length;

		string_matcher_table_element_t data[length][string_matcher_table_width];

		// NOTE: The below comment is from when we we're interfacing with this class from compile-time.
		// Now, we're doing that from runtime but basically expecting the compiler to optimize, which is a different situation.
		// In that situation, it is most likely still UB to do the pointer stuff with a 2D array, but the compiler won't
		// complain because it isn't a compile-time function, so we do it anyway.
		/*
		   I wanted to use a 2D array, but C++ wouldn't let me because of the way compile-time functions works:
		   Basically, we've got a pointer down there somewhere that points to the first element of some row, the row is subject to change
		   as part of the algorithm. The way we initialized the pointer before was:
		   type* ptr = string_matcher.data[0]
		   --> which is the pointer to the first element of the first row
		   We then proceeded to add row-sized sizes to the pointer one after another, but that is what messes up interpretation.
		   I always thought the compiler tracked all the regions of the objects that were currently allocated at compile-time
		   and made sure that every pointer access was always in one of those regions, which would have been pretty cool.
		   Instead, probably because of the C++ spec rule that basically says a pointer to an object must remain inside that object, unless
		   it's only one past the end of the object (I think they weren't thinking hard enough when they formulated that rule,
		   because as it stands, it makes a couple of useful things UB), every pointer in compile-time functions is
		   associated with it's object and can't leave the region for that object.
		   Now, of course the compiler considers the sub-array 0 of string_matcher.data to be the pointers object, so it essentially
		   can't leave the first row of the string_matcher.
		   That's why we had to use a 1D array instead, because now the whole array is the object of the pointer and it can move around
		   freely.
		   NOTE: Technically, the exact same rule applies to runtime as well, it just can't get enforced like it can at compile-time.
		   While it technically is UB, like I said, it's very useful sometimes, and I assume basically all implementations
		   implement it correctly rather than keeping it UB, which is why at runtime, I will continue to use it.
		   NOTE: This could easily be standardized and implemented differently (like I said above with the regions and the tracking),
		   but apparently logic is something the language designers don't have too good a grasp on at times.
		*/

		const string_matcher_table_element_t* state = data[0];

		size_t input_stream_position_at_last_match;
		void (*callback_ptr_at_last_match)(size_t) = nullptr;

		size_t input_stream_position = 0;

		bool match_character(char character) noexcept {
			const unsigned char unsigned_character = character;
			if (state[unsigned_character].callback) {
				callback_ptr_at_last_match = state[unsigned_character].callback;
				input_stream_position_at_last_match = input_stream_position;
			}
			input_stream_position++;
			if (!state[unsigned_character].next_state_ptr) {
				state = data[0];
				if (!callback_ptr_at_last_match) { return false; }
				callback_ptr_at_last_match(input_stream_position_at_last_match);
				callback_ptr_at_last_match = nullptr;
				return true;
			}
			state = state[unsigned_character].next_state_ptr;
			return false;
		}

		void full_reset() noexcept {
			state = data[0];
			callback_ptr_at_last_match = nullptr;
			input_stream_position = 0;
		}
	};

	// NOTE: The below function doesn't work because compile-time functions can't change variables outside of their scope.
	// They're pure functional as far as I can tell, or rather every thing a compile-time function can do, a pure functional function can do,
	// maybe not the other way around.
	// NOTE: The notes about strict aliasing are interesting though, so I'm leaving the whole thing here.
	/*
	consteval void static_memcpy(void* destination, void* source, size_t size) {
		// NOTE: Accessing through char* doesn't break strict aliasing because char (+ signed/unsigned counterparts) is always allowed to alias any type.
		char* char_destination = (char*)destination;
		char* char_source = (char*)source;
		// NOTE: Technically, since there are now two char ptrs (when they were void ptrs, you couldn't access through them, so this wasn't considered then),
		// it is expected that they might alias each other. Normally, this causes slower code because values need to get written to RAM way more often,
		// to make sure that the other pointer can see them if they are accessed through it.
		// IMPORTANT: This should affect us greatly in this function, since if you think about it, it prevents anything more than byte-wise copies at a time while transferring,
		// which definitely affects speed.
		// BUT: First of all, there's not much we can do about that, unless we write this function in assembly, which isn't even allowed in a consteval function.
		// Second of all, it's compile-time, the speed here isn't as important as the speed at run-time. This function almost definitely won't slow down compilation times significantly anyway, even if it's inefficient,
		// since it's only used once per string matcher creation and the extra overhead of the inefficiency is minimal compared to the other stuff we're doing in this header.
		// Third of all, who knows how clang does consteval functions. I have a feeling it doesn't JIT compile them and run them natively, it probably interprets them, in either case who knows
		// how many optimizations it does before interpreting/jitting and how many it simply drops because the time it takes to do them would be longer than the time to simply interpret/jit the unoptimized version.
		// What I'm getting at is somehow I don't think it's likely that clang would even do more than byte-wise transfers here, even if the pointers somehow couldn't alias one another.
		// All this is to say that I think this function is okay.

		for (size_t i = 0; i < size; i++) { char_destination[i] = char_source[i]; }
	}
	*/

	template <size_t length>
	struct func_ptr_array_wrapper_t {
		using func_ptr_t = void(*)(size_t);

		func_ptr_t data[length];

		consteval func_ptr_t& operator[](size_t index) { return data[index]; }
	};

	template <typename... func_ret_types>
	consteval auto convert_func_ptr_pack_to_array(void (*first_func_ptr)(size_t), func_ret_types (*...func_ptrs)(size_t)) {
		func_ptr_array_wrapper_t<sizeof...(func_ptrs) + 1> result;
		result[0] = first_func_ptr;

		if constexpr (sizeof...(func_ptrs) != 0) {
			//func_ptr_array_wrapper_t<sizeof...(func_ptrs)> rest_of_result = convert_func_ptr_pack_to_array(func_ptrs...);
			// TODO: THE ABOVE DOESN'T WORK FOR SOME REASON, SEEMS TO BE A CLANG BUG!!! REPORT!!!!!
			func_ptr_array_wrapper_t<sizeof...(func_ptrs)> rest_of_result;
			rest_of_result = convert_func_ptr_pack_to_array(func_ptrs...);
			for (size_t i = 0; i < sizeof...(func_ptrs); i++) { result[i + 1] = rest_of_result[i]; }
		}

		return result;
	}

	template <typename A_t, typename B_t>
	struct are_types_same {
		consteval operator bool() const { return false; }
	};

	template <typename A_t>
	struct are_types_same<A_t, A_t> {
		consteval operator bool() const { return true; }
	};

	// NOTE: Empty structs/classes are well defined in the C++ spec (although being UB in the C spec).
	// The following line is totally valid, there is one thing you have to watch out for though: sizeof(zero_out_t) is never equal to zero.
	// It must at minimum be 1, but the implementation can define exactly how big, so it could be 50 or 1000, although that practically
	// never happens (so don't worry about taking up space, AFAIK it's 1 most of the time).
	// REASONING: A struct with size 0 could technically be located at the exact same spot as another struct, because it doesn't
	// take up any memory, and that doesn't make any sense. Think about it this way: what would an array of these structs look like?
	// The more I think about it, the more I think one could make everything work with 0-sized empty structs, but I guess the
	// designers chose not to go down that route to avoid causing unnecessary confusion.
	// TODO: Research more about this.
	struct zero_out_t { } zero_out;

	// NOTE: I know the stdlib has one of these, but I wanted to make my own.
	// NOTE: Interestingly, C++ has the facility to deduce class template args from a constructor call, but I think
	// that would require me to forgoe the forwarding and create two separate contructors for rvalue refs and lvalue refs.
	// Maybe next time.
	template <typename first_t, typename second_t>
	struct pair_t {
		first_t first;
		second_t second;

		consteval pair_t(zero_out_t zero_out_flag) : first { }, second { } { }

		/*
IMPORTANT: Remember that typename std::enable_if<condition, bool>::type = true/false is the correct way to use enable_if.
You would think you could do this instead: typename = typename std::enable_if<condition, bool>::type, BUT THAT IS BAD!
default arguments are not considered when checking for template function equivalence (basically checking for redefinition),
because they usually (when SFINAE is not there) don't matter for whether a function is a redefinition or not, since the same set of
args will always match both functions, regardless of the default template arg. Obviously this is different with SFINAE,
but that gets considered when instantiating the templates, which comes sometime after the stage where we check if it's a redefinition.
The way to get around this is with the first version above. Again, nothing is instantiated, so the compiler doesn't know if the enable_if
needs to be SFINAE'd away yet, but it does know that it's a type (because of typename) that isn't directly comparable to
it's counterpart in the other instance of the function definition (because it hasn't been instantiated yet). Because these are now type args,
which are integral to the process of checking for redefinition, and not simply default type values, the compiler will say that these
functions are not redefinitions, because it has to assume that, since they could very well be completely different types which could very well
not even rely on SFINAE.
		*/

		template <typename constructor_first_t, typename constructor_second_t, 
			typename std::enable_if<are_types_same<first_t, typename std::remove_reference<constructor_first_t>::type> { } && 
			are_types_same<second_t, typename std::remove_reference<constructor_second_t>::type> { }, bool>::type = true>
		consteval pair_t(constructor_first_t&& first, constructor_second_t&& second) : 
			first(std::forward<constructor_first_t>(first)), 
			second(std::forward<constructor_second_t>(second))
		{ }
	};

	template <typename first_ref_t, typename second_ref_t>
	consteval auto create_pair_t(first_ref_t&& first, second_ref_t&& second) {
		return pair_t<std::remove_reference_t<first_ref_t>, std::remove_reference_t<second_ref_t>>(std::forward<first_ref_t>(first), std::forward<second_ref_t>(second));
	}

	template <typename first_t, typename second_t, second_t second_value>
	struct pair_with_compile_time_second_t {
		first_t first;
		static constexpr second_t second = second_value;

		template <typename constructor_first_t, 
			 typename std::enable_if<are_types_same<first_t, typename std::remove_reference<constructor_first_t>::type> { }, bool>::type = true>
		consteval pair_with_compile_time_second_t(constructor_first_t&& first) : first(std::forward<constructor_first_t>(first)) { }
	};

	template <auto second_value, typename first_ref_t>
	consteval auto create_pair_with_compile_time_second_t(first_ref_t&& first) {
		return pair_with_compile_time_second_t<std::remove_reference_t<first_ref_t>, decltype(second_value), second_value>(std::forward<first_ref_t>(first));
	}

	template <size_t table_length, typename... func_ret_types>
	consteval auto compile_to_table(const const_string& meta_matcher_spec, func_ret_types (*...func_ptrs)(size_t)) {
		static_assert(sizeof...(func_ptrs) != 0, "failed to create string matcher, no callbacks provided");
		//func_ptr_array_wrapper_t<sizeof...(func_ptrs)> func_ptr_array = convert_func_ptr_pack_to_array(func_ptrs...);
		// TODO: The above causes clang to bug out. Presumably same bug as the other spots where this happens. REPORT!!!!!
		// TODO: This and the other bugs in this file might be totally gone now that I've updated my clang version, test that out!
		func_ptr_array_wrapper_t<sizeof...(func_ptrs)> func_ptr_array;
		func_ptr_array = convert_func_ptr_pack_to_array(func_ptrs...);

		// NOTE: We zero the table out because the error state of an element is when the next_state size_t is 0.
		// Probably the fastest way to do it because the interpreter can do it in a couple operations instead of interpreting a memset or
		// something.
		// NOTE: The classic { } doesn't work here because it's not a POD type. It's got a custom constructor and no
		// default constructor, meaning { } can't even default to default initialization.
		// Instead, I've added another constructor that does the zeroing out by value initializing the
		// member variables. If those aren't zero initializable that's a problem, but they are in this case so it's fine.
		pair_t<string_matcher_with_indices_t<table_length>, size_t> result_pair(zero_out);
		auto& string_matcher_with_indices = result_pair.first;

		/*
		   IMPORTANT: Setting the next_state_ptr's from this function doesn't really work because when you copy the object
		   to the outside, those pointers don't refer to the correct spots anymore. I've thought about it, and the only way to
		   counter-act this is to construct the state table with indices in this function, return that, and then
		   convert the index table into a ptr table outside of this function, which is also (by necessity) outside of compile-time.
		   This isn't a big deal though because the compiler will almost definitely optimize.
		   This does however require that you compile with optimization enabled, which is kind of a shame, but not too big an issue.
		   IMPORTANT: Also, you're not even allowed to return addresses to local variables from compile-time functions,
		   because that would stop them from being deterministic, so that's important to remember as well.
		   IMPORTANT: You might think that you could counter-act this by using static variables inside the compile-time function,
		   which would then also be in the scope of the caller and thereby not violate this rule, but that's incorrect.
		   YOU CAN'T USE STATIC VARIABLES IN COMPILE-TIME FUNCTIONS! For obvious reasons, the way static variables work
		   inherently breaks determinism, so their not allowed.
		*/

		size_t table_row = 0;
		size_t last_table_row;
		size_t merge_row_current = 0;
		size_t last_merge_row;
		size_t i = 0;			// index into the matcher spec
		size_t string_index = 0;	// used to get the correct function pointer for the end elements

		unsigned char character = -1;
		unsigned char last_character;

		while (true) {
			for (; i < meta_matcher_spec.length; i++) {
				// NOTE: This is due to the same bug as below, see comment below.
				//unsigned char character;
				//character = meta_matcher_spec[i];
				// That's how it was.

				last_character = character;
				character = meta_matcher_spec[i];
				if (character > 127) {
					static_fail_with_msg("invalid character (not in ASCII range) present in matcher specification");
				}

				switch (character) {
				case '|':
					if (last_character != -1) {
						// NOTE: We don't set last_table_row on purpose here, don't worry.
						string_matcher_with_indices.data[last_table_row][last_character] = { 0, func_ptr_array[string_index] };
						character = -1;		// to make sure that empty strings don't mess up the system
					}
					string_index++;
					i++;
					break;
				case '\\':
					i++;
					if (i == meta_matcher_spec.length) {
						static_fail_with_msg("backslash ('\\') character cannot appear at end of matcher specification");
					}

					last_character = character;
					character = meta_matcher_spec[i];
					if (character > 127) {
						static_fail_with_msg("invalid character (not in ASCII range) present in matcher specification");
					}
					// fallthrough
				default:
					last_table_row = table_row;
					// IMPORTANT: The right side of this gets evaluated first and then the assignment is done.
					// This is deadly if your doing a[var] = ++var and expecting it to
					// basically be a[4] = 5;      BE VERY CAREFUL WITH THIS STUFF!
					string_matcher_with_indices.data[last_table_row][character] = { ++table_row, nullptr };
					continue;
				}
				break;		// NOTE: We're forced into doing this because consteval's don't allow goto.
			}

			for (; i < meta_matcher_spec.length; i++) {
				//char character = meta_matcher_spec[i]; <-- THIS DOESN'T WORK!!! PROBS CLANG BUG!!! TODO: RESEARCH AND REPORT!!!!
				last_character = character;
				character = meta_matcher_spec[i];
				if (character > 127) {
					static_fail_with_msg("invalid character (not in ASCII range) present in matcher specification");
				}

				switch (character) {
				case '|':
					if (last_character != -1) {
						// We only set callbacks here on purpose, this is the correct behavior for longest match.
						string_matcher_with_indices.data[last_merge_row][last_character].callback = func_ptr_array[string_index];
						character = -1;		// to make sure that empty strings don't mess up the system
					}
					string_index++;
					merge_row_current = 0;
					continue;
				case '\\':
					i++;
					if (i == meta_matcher_spec.length) {
						static_fail_with_msg("backslash ('\\') character cannot appear at end of matcher specification");
					}

					last_character = character;
					character = meta_matcher_spec[i];
					if (character > 127) {
						static_fail_with_msg("invalid character (not in ASCII range) present in matcher specification");
					}
					// fallthrough
				default:
					// NOTE: Using chars as array subscript shouldn't be an issue since, while the character encoding
					// is implementation defined, I believe the basic string literal character set is guaranteed to always be
					// 0 or greater for every character. The user will probably be able to put in negative chars
					// if they use some weird unicode strings for meta_matcher_spec, which is bad
					// since negative subscripts are UB, so we check that each character is in the correct range and throw an
					// error if it isn't.
					// IMPORTANT: In doing so, we've converted char to unsigned char, making the negativity issue irrelevant.
					// Still, I'm not allowing anything that isn't ASCII because I don't want to deal with unicode right now,
					// although it could potentially be easy in this case.
					// SIDE-NOTE: The reason negative subscripts are UB is because pointer overflow/underflow is UB. If the array
					// happens to be at the start of memory (it won't be on most machines, but the spec needs to be
					// compatible with that situation), then negative subscript will underflow the resulting pointer, so not allowed.
					// This is also why moving a pointer outside of the bounds of an object (like an array or a class or something)
					// is undefined (UNLESS you've only moved it one past the end of the object, the spec defines this
					// because one-past-the-end pointers are useful, this also means that no object will be right at the end
					// of memory because there still has to be space for the possible one-past-the-end pointer).
					// Note that you don't even have to dereference it, simply moving it past the one-past-the-end or before the
					// start of the object will result in UB.
					// Thankfully (although I didn't explicitly find the following in the standard, it makes sense and is necessary
					// for many applications), pointing a pointer to a specific position by value and moving it around there,
					// even though there might not exist an object there, is probably fine, since you're avoiding pointer overflow.
					// TODO: Research and find out what is different about pointers from unsigned integers.
					// I always thought pointers were basically integers that you dereference, so why is overflow undefined for them?
					string_matcher_with_indices_table_element_t& element = string_matcher_with_indices.data[merge_row_current][character];
					if (element.next_state == 0) {
						last_table_row = merge_row_current;	// in order to ensure the ending if's change the right row
						element.next_state = table_row;
						merge_row_current = 0;		// in order to make sure that the ending if's get triggered right
						i++;
						break;
					}
					last_merge_row = merge_row_current;
					merge_row_current = element.next_state;
					continue;
				}
				break;
			}
			if (i == meta_matcher_spec.length) { break; }		// NOTE: Again, necessary because consteval's don't allow goto.
		}

		if (character != -1) {
			if (merge_row_current != 0) {
				// We only set callbacks here on purpose, this is the correct behavior for longest match.
				string_matcher_with_indices.data[last_merge_row][character].callback = func_ptr_array[string_index];
			} else {
				string_matcher_with_indices.data[last_table_row][character] = { 0, func_ptr_array[string_index] };
			}
			string_index++;
		}

		if (string_index != sizeof...(func_ptrs)) { static_fail_with_msg("failed to create string matcher, too many callbacks specified"); }

		// NOTE: I wanted to trim the unused section of the string matcher table here, but that doesn't work because
		// the length of the used part of the table isn't a constant expression and we have no way of getting it into a template
		// parameter. (This makes sense given that the length of the used section is solely dependant on the func arg matcher_spec).
		// No matter, we'll just do it outside of compile-time and rely on optimizations, which should almost definitely come.

		result_pair.second = table_row;

		return result_pair;
	}

	template <const const_string& meta_matcher_spec, typename... func_ret_types>
	// NOTE: Rule of thumb for where the ... goes: before the variable name.
	consteval auto inner_create_string_matcher_with_indices(func_ret_types (*...func_ptrs)(size_t)) {
		constexpr size_t table_length = calculate_table_length(meta_matcher_spec);
		static_assert(table_length != 0, "failed to create string matcher, the resulting table length would be 0 (it would not match anything)");
		return compile_to_table<table_length>(meta_matcher_spec, func_ptrs...);
	}

	template <const const_string& meta_matcher_spec, typename... callback_types>
	consteval auto create_string_matcher_with_indices(callback_types... callbacks) {
		static_assert(sizeof(char) == 1, "meta_string_match.h cannot be used on systems where char size is bigger than 1");
		return inner_create_string_matcher_with_indices<meta_matcher_spec>(((void(*)(size_t))callbacks)...);
	}

// NOTE: ## concatinates the two C++ tokens to either side into one token. If the resulting token isn't valid, the whole expression is simply emitted.
// Since most things cause the current token to end and a new one to begin, like the transition from a parenthesis to a name or
// the transition from a name to a dot or from a name to a parenthesis, etc... (simply think: how would the lexer tokenize this?),
// you don't have to worry about leaving spaces around the target tokens to make sure that the surrounding text isn't used as well.
// At least in the vast majority of cases you shouldn't have to worry. Just to be sure, I'm gonna leave spaces everywhere I can,
// to make absolutely sure that nothing gets misunderstood.
// Isn't it cool though that the preprocessor contains a lexer? In practice the preprocessor and lexer (as well as almost everything else)
// are simply combined.
#define CREATE_META_STRING_MATCHER(matcher_name, matcher_spec, ...) const auto matcher_name ## _INDICES_PLUS_LENGTH_VERSION_DO_NOT_TOUCH = []() { \
static constexpr meta::const_string meta_matcher_spec = meta::const_string(matcher_spec); \
/* static constexpr meta::const_string meta_matcher_spec(matcher_spec); <-- CLANG BUG!!!! CRASHES CLANG!!!! TODO: REPORT!!!!! */ \
\
constexpr auto string_matcher_with_indices_pair = meta::create_string_matcher_with_indices<meta_matcher_spec>(__VA_ARGS__); \
\
const auto result_pair = meta::create_pair_with_compile_time_second_t<string_matcher_with_indices_pair.second>(string_matcher_with_indices_pair.first); \
\
return result_pair; \
}(); \
\
meta::string_matcher_t<decltype( matcher_name ## _INDICES_PLUS_LENGTH_VERSION_DO_NOT_TOUCH )::second> matcher_name; \
\
const void * const matcher_name ## _DUMMY_VARIABLE_DO_NOT_TOUCH = [](auto& matcher_name, auto& matcher_name ## _INDICES_PLUS_LENGTH_VERSION_DO_NOT_TOUCH ) { \
for (std::size_t i = 0; i < std::remove_reference_t<decltype(matcher_name)>::length * meta::string_matcher_table_width; i++) { \
if (( matcher_name ## _INDICES_PLUS_LENGTH_VERSION_DO_NOT_TOUCH ).first.data[0][i].next_state == 0) { matcher_name.data[0][i].next_state_ptr = nullptr; } \
else { matcher_name.data[0][i].next_state_ptr = matcher_name.data[0] + ( matcher_name ## _INDICES_PLUS_LENGTH_VERSION_DO_NOT_TOUCH ).first.data[0][i].next_state * meta::string_matcher_table_width; } \
matcher_name.data[0][i].callback = ( matcher_name ## _INDICES_PLUS_LENGTH_VERSION_DO_NOT_TOUCH ).first.data[0][i].callback; \
} \
return nullptr; \
}(matcher_name, ( matcher_name ## _INDICES_PLUS_LENGTH_VERSION_DO_NOT_TOUCH )) \

}
