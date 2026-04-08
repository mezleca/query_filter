#include <bitset>
#include <charconv>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

template <typename T>
T str_to(std::string_view sv) {
	T value = {};
	auto result = std::from_chars<T>(sv.begin(), sv.end(), value);

	if (result.ec == std::errc::invalid_argument || result.ptr != sv.data() + sv.size()) {
		std::cout << "str_to<" << typeid(T).name() << ">: failed to convert " << sv << "\n";
		return T();
	}

	return value;
}

template <typename T>
static T convert(std::string_view value) {
	if constexpr (std::is_same_v<T, int>) {
		return str_to<int>(value);
	} else if constexpr (std::is_same_v<T, float>) {
		return str_to<float>(value);
	} else if constexpr (std::is_same_v<T, double>) {
		return str_to<double>(value);
	} else if constexpr (std::is_same_v<T, const char*>) {
		return value;
	} else if constexpr (std::is_same_v<T, std::string>) {
	    return std::string(value);
	} else {
		throw std::runtime_error("invalid type for " + std::string(value));
	}
}

#define QUERY_OP_LIST \
    X(EQ,  "=")  \
    X(NEQ, "!=") \
    X(GT,  ">")  \
    X(LT,  "<")  \
    X(GTE, ">=") \
    X(LTE, "<=")

// queryable fields with variations
#define QUERY_FIELDS_V \
	X(std::string, abc, "abc_", "_abc") \
	X(std::string, cba, "_cba_")

// queryable fields with no variations
#define QUERY_FIELDS_NV \
	X(int, num)

#define QUERY_FIELDS \
    QUERY_FIELDS_V \
    QUERY_FIELDS_NV

enum QUERY_OP : int {
	INVALID = -1,
#define X(op, str) op,
    QUERY_OP_LIST
#undef X
};

enum PARSE_STATE : int {
	KEY,
	VALUE
};

/*
 "abc a a>5 a>=5"
  ^^^^^^^^^
  IKKCKKKOVK
  		 .
     	 .
  		 -> changed state due to char being a op duh ->
  	 .	        check if temp buffer is valid "a" if so -> proceed
     .
     .
     encounter a space then dump the temp buffer into the normalized query
 */

struct my_struct {
	int k;
#define X(type, name, ...) type name;
	QUERY_FIELDS
#undef X
};

using token_comp = std::function<bool(my_struct&, std::string_view)>;
std::unordered_map<std::string_view, std::map<QUERY_OP, token_comp>> token_parser;

static constexpr std::bitset<256> make_op_start_table() {
    std::bitset<256> table{};

    table.set('>');
    table.set('<');
    table.set('=');
    table.set('!');

    return table;
}

static auto OP_START_TABLE = make_op_start_table();

struct QueryToken {
	std::string_view key;
	std::string_view value;
	QUERY_OP op;
};

std::pair<QUERY_OP, size_t> parse_op(std::string_view sv, size_t pos) {
	char c1 = sv[pos];
	char c2 = sv[pos + 1];

	switch (c1) {
		case '>': {
            if (c2 == '=') {
                return { QUERY_OP::GTE, 2 };
            }

            return { QUERY_OP::GT, 1 };
        }

        case '<': {
            if (c2 == '=') {
                return { QUERY_OP::LTE, 2 };
            }

            return { QUERY_OP::LT, 1 };
        }

        case '=': {
            if (c2 == '=') { // variation
                return { QUERY_OP::EQ, 2 };
            }

            return { QUERY_OP::EQ, 1 };
        }

        case '!': {
        	if (c2 == '=') {
         		return { QUERY_OP::NEQ, 2 };
         	}

         	std::cout << "invalid op: " << c1 << "\n";

            return { QUERY_OP::INVALID, 1 };
        }
	}

	std::cout << "invalid op: " << c1 << "\n";

	return { QUERY_OP::INVALID, 0 };
}

int main() {
	std::cout << "\n\n\n";
	my_struct a { .abc = "bca", .cba = "abc", .num = 222 };

	// create dispatch table for variable shit
	#define X(type, name, ...) \
		for (auto var : { #name, ##__VA_ARGS__}) { \
			token_parser[var][QUERY_OP::EQ]  = [](const my_struct& s, std::string_view v) { return s.name == convert<type>(v); }; \
			token_parser[var][QUERY_OP::NEQ] = [](const my_struct& s, std::string_view v) { return s.name != convert<type>(v); }; \
			token_parser[var][QUERY_OP::GTE] = [](const my_struct& s, std::string_view v) { return s.name >= convert<type>(v); }; \
			token_parser[var][QUERY_OP::GT]  = [](const my_struct& s, std::string_view v) { return s.name >  convert<type>(v); }; \
			token_parser[var][QUERY_OP::LTE] = [](const my_struct& s, std::string_view v) { return s.name <= convert<type>(v); }; \
			token_parser[var][QUERY_OP::LT]  = [](const my_struct& s, std::string_view v) { return s.name <  convert<type>(v); }; \
		}
	QUERY_FIELDS_V
	#undef X

	// create for shit that is not variable
	#define X(type, name, ...) \
		token_parser[#name][QUERY_OP::EQ]  = [](const my_struct& s, std::string_view v) { return s.name == convert<type>(v); }; \
		token_parser[#name][QUERY_OP::NEQ] = [](const my_struct& s, std::string_view v) { return s.name != convert<type>(v); }; \
		token_parser[#name][QUERY_OP::GTE] = [](const my_struct& s, std::string_view v) { return s.name >= convert<type>(v); }; \
		token_parser[#name][QUERY_OP::GT]  = [](const my_struct& s, std::string_view v) { return s.name >  convert<type>(v); }; \
		token_parser[#name][QUERY_OP::LTE] = [](const my_struct& s, std::string_view v) { return s.name <= convert<type>(v); }; \
		token_parser[#name][QUERY_OP::LT]  = [](const my_struct& s, std::string_view v) { return s.name <  convert<type>(v); };
	QUERY_FIELDS_NV
	#undef X

	std::string_view content = "hello abc=5 _cba_=abc    abc==bca num!=5.0 num!=222 where's the pairs??? snd=444 | valid key with invalid op: abc:=10";

	PARSE_STATE p_state = PARSE_STATE::KEY;

	// split content by spaces
	std::string nm_content;

	// state
	bool hit = false;

	size_t key_start = 0;
	size_t op_start = 0, op_end = 0;
	size_t value_end = 0;

	auto reset_fn = [&]() {
		p_state = PARSE_STATE::KEY;

		key_start = 0;
		op_start = 0;
		op_end = 0;
		value_end = 0;
	};

	QUERY_OP cur_op = QUERY_OP::INVALID;
	QueryToken cur_token = {};

	// evaluate tokens and normalize content
	for (size_t i = 0; i < content.length(); i++) {
		if (hit) {
			break;
		}

		bool is_last = content.length() - 1 == i;
		char c = content[i];

		std::cout << c << "\n";

		switch (p_state) {
			case PARSE_STATE::KEY: {
				if (c == ' ' && !is_last) {
					key_start = i + 1;
				}

				// is a operator?
				if (key_start < i && OP_START_TABLE[static_cast<unsigned char>(c)]) {
					auto [op, size] = parse_op(content, i);
					cur_op = op;

					// invalidate duplicated op's
					if (!is_last && c != '=' && content[i + 1] == c) {
						nm_content += content.substr(key_start, i - 1);
					} else {
						p_state = PARSE_STATE::VALUE;
						op_start = i;
						op_end = op_start + size;
					}

					continue;
				}

				if (key_start == 0) {
					key_start = i;
				}
			} break;
			case PARSE_STATE::VALUE: {
				if (c == ' ' || is_last) {
					cur_token = {
						.key = content.substr(key_start, op_start - key_start),
						.value = content.substr(op_end, value_end - op_end + 1),
						.op = cur_op,
					};

					auto eval = token_parser[cur_token.key][cur_token.op];

					if (eval == nullptr) {
						reset_fn();
						continue;
					}

					// early exit if the eval hits false (no need to continue)
					if (!eval(a, cur_token.value)) {
						hit = true;
						break;
					}

					reset_fn();
					break;
				}

				value_end = i;
			} break;
			default: break;
		}
	}

	if (hit) {
		std::cout << "early exited :)\n";
		return 0;
	}

    return 0;
}
