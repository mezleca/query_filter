#include <charconv>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

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
#define X(op, str) op,
    QUERY_OP_LIST
#undef X
};

struct my_struct {
	int k;
#define X(type, name, ...) type name;
	QUERY_FIELDS
#undef X
};

using token_comp = std::function<bool(my_struct&, std::string_view)>;

std::map<std::string_view, std::map<QUERY_OP, token_comp>> token_parser;
std::unordered_map<std::string_view, QUERY_OP> op_list = {
#define X(op, str) { str, QUERY_OP::op },
        QUERY_OP_LIST
#undef X
};

// https://stackoverflow.com/questions/48012539/idiomatically-split-a-string-view
std::vector<std::string_view> split(const std::string_view str, const char delim = ' ')
{
    std::vector<std::string_view> result;

    int indexCommaToLeftOfColumn = 0;
    int indexCommaToRightOfColumn = -1;

    for (int i=0; i < static_cast<int>(str.size()); i++)
    {
        if (str[i] == delim)
        {
            indexCommaToLeftOfColumn = indexCommaToRightOfColumn;
            indexCommaToRightOfColumn = i;
            int index = indexCommaToLeftOfColumn + 1;
            int length = indexCommaToRightOfColumn - index;
            std::string_view column(str.data() + index, length);
            result.push_back(column);
        }
    }

    const std::string_view finalColumn(str.data() + indexCommaToRightOfColumn + 1, str.size() - indexCommaToRightOfColumn - 1);
    result.push_back(finalColumn);
    return result;
}

struct QueryToken {
	std::string_view key;
	std::string_view value;
	QUERY_OP op;
};

static std::optional<QueryToken> parse_token(std::string_view s) {
    // find where the operator starts
    size_t op_start = 0;
    while (op_start < s.size() && (std::isalnum(s[op_start]) || s[op_start] == '_')) {
        ++op_start;
    }

    if (op_start == 0 || op_start >= s.size()) {
        return std::nullopt;
    }

    // find where the operator ends
    size_t op_end = op_start;
    while (op_end < s.size() && !std::isalnum(s[op_end]) && s[op_end] != '_') {
        ++op_end;
    }

    if (op_end == s.size()) {
        return std::nullopt;
    }

    auto op = op_list.find(s.substr(op_start, op_end - op_start));

    if (op == op_list.end()) {
    	return std::nullopt;
    }

    return QueryToken{
        .key   = s.substr(0, op_start),
        .value = s.substr(op_end),
        .op    = op->second
    };
}

int main() {
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

	std::string_view content = "hello abc!=5 _cba_=abc    abc=bca num!=5.0 num=222 where's the pairs??? snd=444 | valid key with invalid op: abc:=10";

	// split content by spaces
	auto c_tokens = split(content);
	std::vector<QueryToken> tokens;
	std::string nm_content;

	for (const auto& token : c_tokens) {
		if (token == " ") {
			nm_content += token;
			continue;
		}

		// attempt to parse the token
		auto parsed = parse_token(token);

		// if we cant parse treat as text
		if (!parsed.has_value()) {
			nm_content += std::string(token) + " ";
			continue;
		}

		// uuh
		tokens.emplace_back(parsed.value());
	}

	bool query_result = true;

	for (const auto& token : tokens) {
		auto comp = token_parser[token.key][token.op];

		if (comp == nullptr) {
			std::cout << "ignoring invalid token " << token.key << "\n";
			continue;
		}

		if (!comp(a, token.value)) {
			query_result = false;
			break;
		}
	}

	std::string bleh = query_result ? "true" : "false";
	std::cout << "query result for:\n" << content << " -> " << bleh << "\n";
    return 0;
}
