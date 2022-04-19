// Copyright 2018-2022 René Ferdinand Rivera Morell
// Copyright 2021 Max Ferger
// Copyright 2017 Two Blue Cubes Ltd. All rights reserved.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef LYRA_LYRA_HPP
#define LYRA_LYRA_HPP


#ifndef LYRA_VERSION_HPP
#define LYRA_VERSION_HPP

#define LYRA_VERSION_MAJOR 1
#define LYRA_VERSION_MINOR 6
#define LYRA_VERSION_PATCH 0

#define LYRA_VERSION \
	(((LYRA_VERSION_MAJOR)*10000000) + ((LYRA_VERSION_MINOR)*100000) \
		+ (LYRA_VERSION_PATCH))

#endif // LYRA_VERSION_HPP


#ifndef LYRA_ARG_HPP
#define LYRA_ARG_HPP


#ifndef LYRA_DETAIL_PRINT_HPP
#define LYRA_DETAIL_PRINT_HPP

#include <iostream>
#include <string>

#ifndef LYRA_DEBUG
#	define LYRA_DEBUG 0
#endif

namespace lyra { namespace detail {

constexpr bool is_debug = LYRA_DEBUG;

template <typename T>
std::string to_string(T && t)
{
	return std::string(std::move(t));
}

using std::to_string;

struct print
{
	print(const char * scope = nullptr)
		: scope(scope)
	{
		if (is_debug) print::depth() += 1;
		if (scope) debug(scope, "...");
	}

	~print()
	{
		if (scope) debug("...", scope);
		if (is_debug) print::depth() -= 1;
	}

	template <typename... A>
	void debug(A... arg)
	{
		if (is_debug)
		{
			std::cerr << "[DEBUG]"
					  << std::string((print::depth() - 1) * 2, ' ');
			std::string args[] = { to_string(arg)... };
			for (auto & arg_string : args)
			{
				std::cerr << " " << arg_string;
			}
			std::cerr << "\n";
		}
	}

	private:
	const char * scope;

	static std::size_t & depth()
	{
		static std::size_t d = 0;
		return d;
	}
};

}} // namespace lyra::detail

#if LYRA_DEBUG
#	define LYRA_PRINT_SCOPE ::lyra::detail::print lyra_print_scope
#	define LYRA_PRINT_DEBUG lyra_print_scope.debug
#else
#	define LYRA_PRINT_SCOPE(...) while (false)
#	define LYRA_PRINT_DEBUG(...) while (false)
#endif

#endif

#ifndef LYRA_PARSER_HPP
#define LYRA_PARSER_HPP


#ifndef LYRA_ARGS_HPP
#define LYRA_ARGS_HPP

#include <initializer_list>
#include <string>
#include <vector>

namespace lyra {

/* tag::reference[]

[#lyra_args]
= `lyra::args`

Transport for raw args (copied from main args, supplied via init list, or from
a pair of iterators).

*/ // end::reference[]
class args
{
	public:
	args(int argc, char const * const * argv)
		: m_exeName(argv[0])
		, m_args(argv + 1, argv + argc)
	{}

	args(std::initializer_list<std::string> args)
		: m_exeName(*args.begin())
		, m_args(args.begin() + 1, args.end())
	{}

	template <typename It>
	args(const It & start, const It & end)
		: m_exeName(*start)
		, m_args(start + 1, end)
	{}

	std::string exe_name() const { return m_exeName; }

	std::vector<std::string>::const_iterator begin() const
	{
		return m_args.begin();
	}

	std::vector<std::string>::const_iterator end() const
	{
		return m_args.end();
	}

	private:
	std::string m_exeName;
	std::vector<std::string> m_args;
};

} // namespace lyra

#endif

#ifndef LYRA_DETAIL_BOUND_HPP
#define LYRA_DETAIL_BOUND_HPP


#ifndef LYRA_DETAIL_FROM_STRING_HPP
#define LYRA_DETAIL_FROM_STRING_HPP


#ifndef LYRA_DETAIL_TRAIT_UTILS_HPP
#define LYRA_DETAIL_TRAIT_UTILS_HPP

#include <type_traits>
#include <utility>

namespace lyra { namespace detail {

template <class F, class... Args>
struct is_callable
{
	template <class U>
	static auto test(U * p)
		-> decltype((*p)(std::declval<Args>()...), void(), std::true_type());

	template <class U>
	static auto test(...) -> decltype(std::false_type());

	static constexpr bool value = decltype(test<F>(0))::value;
};

template <class T>
struct remove_cvref
{
	typedef
		typename std::remove_cv<typename std::remove_reference<T>::type>::type
			type;
};

template <class F>
struct is_invocable
{
	template <class U>
	static auto test(U * p)
		-> decltype((&U::operator()), void(), std::true_type());

	template <class U>
	static auto test(...) -> decltype(std::false_type());

	static constexpr bool value
		= decltype(test<typename remove_cvref<F>::type>(0))::value;
};

template <typename... Ts>
struct make_void
{
	typedef void type;
};
template <typename... Ts>
using valid_t = typename make_void<Ts...>::type;

template <class T, template <class...> class Primary>
struct is_specialization_of : std::false_type
{};
template <template <class...> class Primary, class... Args>
struct is_specialization_of<Primary<Args...>, Primary> : std::true_type
{};

}} // namespace lyra::detail

#endif

#include <algorithm>
#include <sstream>
#include <string>
#include <type_traits>

#ifndef LYRA_CONFIG_OPTIONAL_TYPE
#	ifdef __has_include
#		if __has_include(<optional>) && __cplusplus >= 201703L
#			include <optional>
#			define LYRA_CONFIG_OPTIONAL_TYPE std::optional
#		endif
#	endif
#endif

namespace lyra { namespace detail {

template <typename T>
bool to_string(const T & source, std::string & target)
{
	std::stringstream ss;
	ss << source;
	ss >> target;
	return !ss.fail();
}

inline bool to_string(const std::string & source, std::string & target)
{
	target = source;
	return true;
}

inline bool to_string(const char * source, std::string & target)
{
	target = source;
	return true;
}

inline bool to_string(bool source, std::string & target)
{
	target = source ? "true" : "false";
	return true;
}

#ifdef LYRA_CONFIG_OPTIONAL_TYPE
template <typename T>
inline bool to_string(
	LYRA_CONFIG_OPTIONAL_TYPE<T> & source, std::string & target)
{
	if (source)
		return to_string(*source, target);
	else
		target = "<nullopt>";
	return true;
}
#endif // LYRA_CONFIG_OPTIONAL_TYPE

template <typename, typename = void>
struct is_convertible_from_string : std::false_type
{};

template <typename T>
struct is_convertible_from_string<T,
	typename std::enable_if<std::is_arithmetic<T>::value>::type>
	: std::true_type
{};

template <typename, typename = void>
struct validate_from_string
{
	static bool validate(const std::string &) { return true; }
};

template <typename T>
struct validate_from_string<T,
	typename std::enable_if<
		std::is_unsigned<typename detail::remove_cvref<T>::type>::value>::type>
{
	static bool validate(const std::string & s)
	{
		return s.find_first_not_of("0123456789") == std::string::npos;
	}
};

template <typename T>
struct validate_from_string<T,
	typename std::enable_if<
		std::is_integral<typename detail::remove_cvref<T>::type>::value
		&& std::is_signed<typename detail::remove_cvref<T>::type>::value>::type>
{
	static bool validate(const std::string & s)
	{
		return s.find_first_not_of("-0123456789") == std::string::npos;
	}
};

template <typename S, typename T>
inline bool from_string(S const & source, T & target)
{
	std::stringstream ss;
	ss << source;
	if (!validate_from_string<T>::validate(ss.str())) return false;
	T temp {};
	ss >> temp;
	if (!ss.fail() && ss.eof())
	{
		target = temp;
		return true;
	}
	return false;
}

template <typename S, typename... C>
inline bool from_string(S const & source, std::basic_string<C...> & target)
{
	to_string(source, target);
	return true;
}

template <typename T>
struct is_convertible_from_string<T,
	typename std::enable_if<std::is_same<T, bool>::value>::type>
	: std::true_type
{};

template <typename S>
inline bool from_string(S const & source, bool & target)
{
	std::string srcLC;
	to_string(source, srcLC);
	std::transform(srcLC.begin(), srcLC.end(), srcLC.begin(),
		[](char c) { return static_cast<char>(::tolower(c)); });
	if (srcLC == "y" || srcLC == "1" || srcLC == "true" || srcLC == "yes"
		|| srcLC == "on")
		target = true;
	else if (srcLC == "n" || srcLC == "0" || srcLC == "false" || srcLC == "no"
		|| srcLC == "off")
		target = false;
	else
		return false;
	return true;
}

#ifdef LYRA_CONFIG_OPTIONAL_TYPE
template <typename T>
struct is_convertible_from_string<T,
	typename std::enable_if<
		is_specialization_of<T, LYRA_CONFIG_OPTIONAL_TYPE>::value>::type>
	: std::true_type
{};

template <typename S, typename T>
inline bool from_string(S const & source, LYRA_CONFIG_OPTIONAL_TYPE<T> & target)
{
	std::string srcLC;
	to_string(source, srcLC);
	std::transform(srcLC.begin(), srcLC.end(), srcLC.begin(),
		[](char c) { return static_cast<char>(::tolower(c)); });
	if (srcLC == "<nullopt>")
	{
		target.reset();
		return true;
	}
	else
	{
		T temp;
		auto result = from_string(source, temp);
		if (result) target = std::move(temp);
		return result;
	}
}
#endif // LYRA_CONFIG_OPTIONAL_TYPE

}} // namespace lyra::detail

#endif

#ifndef LYRA_DETAIL_INVOKE_LAMBDA_HPP
#define LYRA_DETAIL_INVOKE_LAMBDA_HPP


#ifndef LYRA_DETAIL_PARSE_HPP
#define LYRA_DETAIL_PARSE_HPP


#ifndef LYRA_PARSER_RESULT_HPP
#define LYRA_PARSER_RESULT_HPP


#ifndef LYRA_DETAIL_RESULT_HPP
#define LYRA_DETAIL_RESULT_HPP

#include <memory>
#include <string>

namespace lyra { namespace detail {

class result_base
{
	public:
	result_base const & base() const { return *this; }
	explicit operator bool() const { return is_ok(); }
	bool is_ok() const { return kind_ == result_kind::ok; }
	std::string message() const { return message_; }
	[[deprecated]] std::string errorMessage() const { return message(); }

	protected:
	enum class result_kind
	{
		ok,
		error
	};

	explicit result_base(result_kind kind, const std::string & message = "")
		: kind_(kind)
		, message_(message)
	{}

	explicit result_base(const result_base & other)
		: kind_(other.kind_)
		, message_(other.message_)
	{}

	virtual ~result_base() = default;

	result_base & operator=(const result_base &) = default;

	private:
	result_kind kind_;
	std::string message_;
};

template <typename T>
class result_value_base : public result_base
{
	public:
	using value_type = T;

	value_type const & value() const { return *value_; }
	bool has_value() const { return bool(value_); }

	protected:
	std::unique_ptr<value_type> value_;

	explicit result_value_base(
		result_kind kind, const std::string & message = "")
		: result_base(kind, message)
	{}

	explicit result_value_base(result_kind kind,
		const value_type & val,
		const std::string & message = "")
		: result_base(kind, message)
	{
		value_.reset(new value_type(val));
	}

	explicit result_value_base(result_value_base const & other)
		: result_base(other)
	{
		if (other.value_) value_.reset(new value_type(*other.value_));
	}

	explicit result_value_base(const result_base & other)
		: result_base(other)
	{}

	result_value_base & operator=(result_value_base const & other)
	{
		result_base::operator=(other);
		if (other.value_) value_.reset(new T(*other.value_));
		return *this;
	}
};

template <>
class result_value_base<void> : public result_base
{
	public:
	using value_type = void;

	protected:
	explicit result_value_base(const result_base & other)
		: result_base(other)
	{}
	explicit result_value_base(
		result_kind kind, const std::string & message = "")
		: result_base(kind, message)
	{}
};

template <typename T>
class basic_result : public result_value_base<T>
{
	public:
	using value_type = typename result_value_base<T>::value_type;

	explicit basic_result(result_base const & other)
		: result_value_base<T>(other)
	{}


	static basic_result ok(value_type const & val)
	{
		return basic_result(result_base::result_kind::ok, val);
	}

	static basic_result error(
		value_type const & val, std::string const & message)
	{
		return basic_result(result_base::result_kind::error, val, message);
	}

	protected:
	using result_value_base<T>::result_value_base;
};

template <>
class basic_result<void> : public result_value_base<void>
{
	public:
	using value_type = typename result_value_base<void>::value_type;

	explicit basic_result(result_base const & other)
		: result_value_base<void>(other)
	{}


	static basic_result ok()
	{
		return basic_result(result_base::result_kind::ok);
	}

	static basic_result error(std::string const & message)
	{
		return basic_result(result_base::result_kind::error, message);
	}

	protected:
	using result_value_base<void>::result_value_base;
};

}} // namespace lyra::detail

#endif
#include <string>

namespace lyra {

enum class parser_result_type
{
	matched,
	no_match,
	short_circuit_all
};

inline std::string to_string(parser_result_type v)
{
	switch (v)
	{
		case parser_result_type::matched: return "matched";
		case parser_result_type::no_match: return "no_match";
		case parser_result_type::short_circuit_all: return "short_circuit_all";
	}
	return "?";
}

using result = detail::basic_result<void>;

using parser_result = detail::basic_result<parser_result_type>;

} // namespace lyra

#endif

#include <string>

namespace lyra { namespace detail {

template <typename S, typename T>
parser_result parse_string(S const & source, T & target)
{
	if (from_string(source, target))
		return parser_result::ok(parser_result_type::matched);
	else
		return parser_result::error(parser_result_type::no_match,
			"Unable to convert '" + source + "' to destination type");
}

}} // namespace lyra::detail

#endif

#ifndef LYRA_DETAIL_UNARY_LAMBDA_TRAITS_HPP
#define LYRA_DETAIL_UNARY_LAMBDA_TRAITS_HPP

#include <type_traits>

namespace lyra { namespace detail {

template <typename L>
struct unary_lambda_traits : unary_lambda_traits<decltype(&L::operator())>
{};

template <typename ClassT, typename ReturnT, typename... Args>
struct unary_lambda_traits<ReturnT (ClassT::*)(Args...) const>
{
	static const bool isValid = false;
};

template <typename ClassT, typename ReturnT, typename ArgT>
struct unary_lambda_traits<ReturnT (ClassT::*)(ArgT) const>
{
	static const bool isValid = true;
	using ArgType = typename std::remove_const<
		typename std::remove_reference<ArgT>::type>::type;
	using ReturnType = ReturnT;
};

}} // namespace lyra::detail

#endif

namespace lyra { namespace detail {

template <typename ReturnType>
struct LambdaInvoker
{
	template <typename L, typename ArgType>
	static parser_result invoke(L const & lambda, ArgType const & arg)
	{
		return lambda(arg);
	}
};

template <>
struct LambdaInvoker<void>
{
	template <typename L, typename ArgType>
	static parser_result invoke(L const & lambda, ArgType const & arg)
	{
		lambda(arg);
		return parser_result::ok(parser_result_type::matched);
	}
};

template <typename ArgType, typename L>
inline parser_result invokeLambda(L const & lambda, std::string const & arg)
{
	ArgType temp {};
	auto result = parse_string(arg, temp);
	return !result
		? result
		: LambdaInvoker<typename unary_lambda_traits<L>::ReturnType>::invoke(
			lambda, temp);
}

}} // namespace lyra::detail

#endif
#include <string>

namespace lyra { namespace detail {

struct NonCopyable
{
	NonCopyable() = default;
	NonCopyable(NonCopyable const &) = delete;
	NonCopyable(NonCopyable &&) = delete;
	NonCopyable & operator=(NonCopyable const &) = delete;
	NonCopyable & operator=(NonCopyable &&) = delete;
};

struct BoundRef : NonCopyable
{
	virtual ~BoundRef() = default;
	virtual auto isContainer() const -> bool { return false; }
	virtual auto isFlag() const -> bool { return false; }

	virtual size_t get_value_count() const { return 0; }
	virtual std::string get_value(size_t) const { return ""; }
};

struct BoundValueRefBase : BoundRef
{
	virtual auto setValue(std::string const & arg) -> parser_result = 0;
};

struct BoundFlagRefBase : BoundRef
{
	virtual auto setFlag(bool flag) -> parser_result = 0;
	virtual auto isFlag() const -> bool { return true; }
};

template <typename T>
struct BoundValueRef : BoundValueRefBase
{
	T & m_ref;

	explicit BoundValueRef(T & ref)
		: m_ref(ref)
	{}

	auto setValue(std::string const & arg) -> parser_result override
	{
		return parse_string(arg, m_ref);
	}

	virtual size_t get_value_count() const override { return 1; }
	virtual std::string get_value(size_t i) const override
	{
		if (i == 0)
		{
			std::string result;
			detail::to_string(m_ref, result);
			return result;
		}
		return "";
	}
};

template <typename T>
struct BoundValueRef<std::vector<T>> : BoundValueRefBase
{
	std::vector<T> & m_ref;

	explicit BoundValueRef(std::vector<T> & ref)
		: m_ref(ref)
	{}

	auto isContainer() const -> bool override { return true; }

	auto setValue(std::string const & arg) -> parser_result override
	{
		T temp;
		auto result = parse_string(arg, temp);
		if (result) m_ref.push_back(temp);
		return result;
	}

	virtual size_t get_value_count() const override { return m_ref.size(); }
	virtual std::string get_value(size_t i) const override
	{
		if (i < m_ref.size())
		{
			std::string result;
			detail::to_string(m_ref[i], result);
			return result;
		}
		return "";
	}
};

struct BoundFlagRef : BoundFlagRefBase
{
	bool & m_ref;

	explicit BoundFlagRef(bool & ref)
		: m_ref(ref)
	{}

	auto setFlag(bool flag) -> parser_result override
	{
		m_ref = flag;
		return parser_result::ok(parser_result_type::matched);
	}

	virtual size_t get_value_count() const override { return 1; }
	virtual std::string get_value(size_t i) const override
	{
		if (i == 0) return m_ref ? "true" : "false";
		return "";
	}
};

template <typename L>
struct BoundLambda : BoundValueRefBase
{
	L m_lambda;

	static_assert(unary_lambda_traits<L>::isValid,
		"Supplied lambda must take exactly one argument");
	explicit BoundLambda(L const & lambda)
		: m_lambda(lambda)
	{}

	auto setValue(std::string const & arg) -> parser_result override
	{
		return invokeLambda<typename unary_lambda_traits<L>::ArgType>(
			m_lambda, arg);
	}
};

template <typename L>
struct BoundFlagLambda : BoundFlagRefBase
{
	L m_lambda;

	static_assert(unary_lambda_traits<L>::isValid,
		"Supplied lambda must take exactly one argument");
	static_assert(
		std::is_same<typename unary_lambda_traits<L>::ArgType, bool>::value,
		"flags must be boolean");

	explicit BoundFlagLambda(L const & lambda)
		: m_lambda(lambda)
	{}

	auto setFlag(bool flag) -> parser_result override
	{
		return LambdaInvoker<
			typename unary_lambda_traits<L>::ReturnType>::invoke(m_lambda,
			flag);
	}
};

template <typename T>
struct BoundVal : BoundValueRef<T>
{
	T value;

	BoundVal(T && v)
		: BoundValueRef<T>(value)
		, value(v)
	{}

	BoundVal(BoundVal && other)
		: BoundValueRef<T>(value)
		, value(std::move(other.value))
	{}

	std::shared_ptr<BoundRef> move_to_shared()
	{
		return std::shared_ptr<BoundRef>(new BoundVal<T>(std::move(*this)));
	}
};

}} // namespace lyra::detail

#endif

#ifndef LYRA_DETAIL_CHOICES_HPP
#define LYRA_DETAIL_CHOICES_HPP

#include <algorithm>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <vector>

namespace lyra { namespace detail {

/*
Type erased base for set of choices. I.e. it's an "interface".
*/
struct choices_base
{
	virtual ~choices_base() = default;
	virtual parser_result contains_value(std::string const & val) const = 0;
};

/*
Stores a set of choice values and provides checking if a given parsed
string value is one of the choices.
*/
template <typename T>
struct choices_set : choices_base
{
	std::vector<T> values;

	template <typename... Vals>
	explicit choices_set(Vals... vals)
		: choices_set({ vals... })
	{}

	explicit choices_set(const std::vector<T> & vals)
		: values(vals)
	{}

	parser_result contains_value(std::string const & val) const override
	{
		T value;
		auto parse = parse_string(val, value);
		if (!parse)
		{
			return parser_result::error(
				parser_result_type::no_match, parse.message());
		}
		bool result = std::count(values.begin(), values.end(), value) > 0;
		if (result)
		{
			return parser_result::ok(parser_result_type::matched);
		}
		return parser_result::error(parser_result_type::no_match,
			"Value '" + val
				+ "' not expected. Allowed values are: " + this->to_string());
	}

	std::string to_string() const
	{
		std::string result;
		for (const T & val : values)
		{
			if (!result.empty()) result += ", ";
			std::string val_string;
			if (detail::to_string(val, val_string))
			{
				result += val_string;
			}
			else
			{
				result += "<value error>";
			}
		}
		return result;
	}

	protected:
	explicit choices_set(std::initializer_list<T> const & vals)
		: values(vals)
	{}
};

template <>
struct choices_set<const char *> : choices_set<std::string>
{
	template <typename... Vals>
	explicit choices_set(Vals... vals)
		: choices_set<std::string>(vals...)
	{}
};

/*
Calls a designated function to check if the choice is valid.
*/
template <typename Lambda>
struct choices_check : choices_base
{
	static_assert(unary_lambda_traits<Lambda>::isValid,
		"Supplied lambda must take exactly one argument");
	static_assert(std::is_same<bool,
					  typename unary_lambda_traits<Lambda>::ReturnType>::value,
		"Supplied lambda must return bool");

	Lambda checker;
	using value_type = typename unary_lambda_traits<Lambda>::ArgType;

	explicit choices_check(Lambda const & checker)
		: checker(checker)
	{}

	parser_result contains_value(std::string const & val) const override
	{
		value_type value;
		auto parse = parse_string(val, value);
		if (!parse)
		{
			return parser_result::error(
				parser_result_type::no_match, parse.message());
		}
		if (checker(value))
		{
			return parser_result::ok(parser_result_type::matched);
		}
		return parser_result::error(
			parser_result_type::no_match, "Value '" + val + "' not expected.");
	}
};

}} // namespace lyra::detail

#endif

#ifndef LYRA_DETAIL_TOKENS_HPP
#define LYRA_DETAIL_TOKENS_HPP


#ifndef LYRA_OPTION_STYLE_HPP
#define LYRA_OPTION_STYLE_HPP

#include <string>

namespace lyra {

/* tag::reference[]

[#lyra_option_style]
= `lyra::option_style`

Specify the syntax style for options to the parser.

[source]
----
std::string value_delimiters;
std::string long_option_prefix;
std::size_t long_option_size = 0;
std::string short_option_prefix;
std::size_t short_option_size = 0;
----

* `value_delimiters` -- Specifies a set of characters that are accepted as a
	delimiter/separator between an option name and an option value when a
	single argument is used for the option+value (i.e. "--option=value").
* `long_option_prefix` -- Specifies a set of characters that are accepted as a
	prefix for long options (i.e. multi-char single option name).
* `long_option_size` -- The number of prefix characters that indicates a long
	option. A value of zero (0) indicates that long options are not accepted.
* `short_option_prefix` -- Specifies a set of characters that are accepted as a
	prefix for short options (i.e. single-char multi-options).
* `short_option_size` -- The number of prefix characters that indicates a short
	option. A value of zero (0) indicates that short options are not accepted.

end::reference[] */
struct option_style
{
	std::string value_delimiters;
	std::string long_option_prefix;
	std::size_t long_option_size = 0;
	std::string short_option_prefix;
	std::size_t short_option_size = 0;


	option_style(std::string && value_delimiters,
		std::string && long_option_prefix = {},
		std::size_t long_option_size = 0,
		std::string && short_option_prefix = {},
		std::size_t short_option_size = 0)
		: value_delimiters(std::move(value_delimiters))
		, long_option_prefix(std::move(long_option_prefix))
		, long_option_size(long_option_size)
		, short_option_prefix(std::move(short_option_prefix))
		, short_option_size(short_option_size)
	{}


	std::string long_option_string() const;
	std::string short_option_string() const;


	static const option_style & posix();
	static const option_style & posix_brief();
	static const option_style & windows();
};

/* tag::reference[]

[#lyra_option_style_ctor]
== Construction

[source]
----
lyra::option_style::option_style(
	std::string && value_delimiters,
	std::string && long_option_prefix = {},
	std::size_t long_option_size = 0,
	std::string && short_option_prefix = {},
	std::size_t short_option_size = 0)
----

Utility constructor that defines all the settings.

end::reference[] */

/* tag::reference[]

[#lyra_option_style_def]
== Definitions

[source]
----
std::string lyra::option_style::long_option_string() const
std::string lyra::option_style::short_option_string() const
----

Gives the default long or short option string, or prefix, for this option
style. If the type of option is not available, i.e. size is zero, an empty
string is returned.

end::reference[] */

inline std::string option_style::long_option_string() const
{
	return long_option_size > 0
		? std::string(long_option_size, long_option_prefix[0])
		: "";
}

inline std::string option_style::short_option_string() const
{
	return short_option_size > 0
		? std::string(short_option_size, short_option_prefix[0])
		: "";
}

/* tag::reference[]

[#lyra_option_style_styles]
== Styles

[source]
----
static const option_style & lyra::option_style::posix();
static const option_style & lyra::option_style::posix_brief();
static const option_style & lyra::option_style::windows();
----

These provide definitions for common syntax of option styles:

`posix`:: The overall _default_ that is two dashes (`--`) for long option
	names and one dash (`-`) for short option names. Values for long options
	use equal (`=`) between the option and value.
`posix_brief`:: Variant that only allows for long option names with a single
	dash (`-`).
`windows`:: The common option style on Windows `CMD.EXE` shell. It only allows
	long name options that start with slash (`/`) where the value is
	specified after a colon (`:`). Single character flag style options are
	only available as individual long options, for example `/A`.

end::reference[] */

inline const option_style & option_style::posix()
{
	static const option_style style("= ", "-", 2, "-", 1);
	return style;
}

inline const option_style & option_style::posix_brief()
{
	static const option_style style("= ", "-", 1);
	return style;
}

inline const option_style & option_style::windows()
{
	static const option_style style(":", "/", 1);
	return style;
}

} // namespace lyra

#endif

#include <string>
#include <vector>

namespace lyra { namespace detail {

enum class token_type
{
	unknown,
	option,
	argument
};

template <typename Char, class Traits = std::char_traits<Char>>
class basic_token_name
{
	public:
	using traits_type = Traits;
	using value_type = Char;
	using pointer = value_type *;
	using const_pointer = const value_type *;
	using reference = value_type &;
	using const_reference = const value_type &;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using const_iterator = const_pointer;
	using iterator = const_iterator;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	using reverse_iterator = const_reverse_iterator;
	using string_type = std::basic_string<value_type, traits_type>;

	basic_token_name() noexcept
		: str { nullptr }
		, len { 0 }
	{}

	basic_token_name(const basic_token_name &) noexcept = default;

	basic_token_name(const_pointer s) noexcept
		: str { s }
		, len { traits_type::length(s) }
	{}

	basic_token_name(const_pointer s, size_type count) noexcept
		: str { s }
		, len { count }
	{}

	basic_token_name & operator=(const basic_token_name &) noexcept = default;

	void swap(basic_token_name & other) noexcept
	{
		auto tmp = *this;
		*this = other;
		other = tmp;
	}

	const_iterator begin() const noexcept { return this->str; }
	const_iterator end() const noexcept { return this->str + this->len; }
	const_iterator cbegin() const noexcept { return this->str; }
	const_iterator cend() const noexcept { return this->str + this->len; }

	size_type size() const noexcept { return this->len; }
	size_type length() const noexcept { return this->len; }
	bool empty() const noexcept { return this->len == 0; }

	friend string_type to_string(const basic_token_name & t)
	{
		return { t.str, t.len };
	}

	friend string_type operator+(
		const_pointer lhs, const basic_token_name & rhs)
	{
		return lhs + to_string(rhs);
	}

	private:
	const_pointer str;
	size_type len;
};

using token_name = std::string;

struct token
{
	token_type type;
	token_name name;

	token()
		: type(token_type::unknown)
	{}
	token(const token & other) = default;
	token(token_type t, const token_name & n)
		: type(t)
		, name(n)
	{}

	explicit operator bool() const { return type != token_type::unknown; }
};

class token_iterator
{
	public:
	template <typename Span>
	explicit token_iterator(Span const & args, const option_style & opt_style)
		: style(opt_style)
		, args_i(args.begin())
		, args_e(args.end())
		, args_i_sub(opt_style.short_option_size)
	{}

	explicit operator bool() const noexcept { return args_i != args_e; }

	token_iterator & pop(const token & arg_or_opt)
	{
		if (arg_or_opt.type == token_type::option && has_short_option_prefix())
		{
			if (++args_i_sub >= args_i->size())
			{
				++args_i;
				args_i_sub = style.short_option_size;
			}
		}
		else
		{
			++args_i;
			args_i_sub = style.short_option_size;
		}
		return *this;
	}

	token_iterator & pop(const token & /* opt */, const token & /* val */)
	{
		if (has_short_option_prefix() && args_i->size() > 2)
			++args_i;
		else if (!has_value_delimiter())
			args_i += 2;
		else
			++args_i;
		args_i_sub = style.short_option_size;
		return *this;
	}

	bool has_option_prefix() const noexcept
	{
		return has_long_option_prefix() || has_short_option_prefix();
	}

	bool has_short_option_prefix() const noexcept
	{
		return (args_i != args_e)
			&& is_prefixed(
				style.short_option_prefix, style.short_option_size, *args_i);
	}

	bool has_long_option_prefix() const noexcept
	{
		return (args_i != args_e)
			&& is_prefixed(
				style.long_option_prefix, style.long_option_size, *args_i);
	}

	bool has_value_delimiter() const noexcept
	{
		return (args_i != args_e)
			&& (args_i->find_first_of(style.value_delimiters)
				!= std::string::npos);
	}

	token option() const
	{
		if (has_long_option_prefix())
		{
			if (has_value_delimiter())
				return token(token_type::option,
					args_i->substr(
						0, args_i->find_first_of(style.value_delimiters)));
			else
				return token(token_type::option, *args_i);
		}
		else if (has_short_option_prefix())
		{
			return { token_type::option,
				prefix_value(style.short_option_prefix, style.short_option_size)
					+ (*args_i)[args_i_sub] };
		}
		return token();
	}

	token value() const
	{
		if (has_option_prefix() && has_value_delimiter())
			return token(token_type::argument,
				args_i->substr(
					args_i->find_first_of(style.value_delimiters) + 1));
		else if (has_long_option_prefix())
		{
			if (args_i + 1 != args_e)
				return token(token_type::argument, *(args_i + 1));
		}
		else if (has_short_option_prefix())
		{
			if (args_i_sub + 1 < args_i->size())
				return token(
					token_type::argument, args_i->substr(args_i_sub + 1));
			else if (args_i + 1 != args_e)
				return token(token_type::argument, *(args_i + 1));
		}
		return token();
	}

	token argument() const { return token(token_type::argument, *args_i); }

	static bool is_prefixed(
		const std::string & prefix, std::size_t size, const std::string & s)
	{
		if (!prefix.empty() && size > 0 && s.size() > size)
		{
			for (auto c : prefix)
			{
				if (s[size] != c
					&& s.find_last_not_of(c, size - 1) == std::string::npos)
					return true;
			}
		}
		return false;
	}

	private:
	const option_style & style;
	std::vector<std::string>::const_iterator args_i;
	std::vector<std::string>::const_iterator args_e;
	std::string::size_type args_i_sub;

	inline bool is_opt_prefix(char c) const noexcept
	{
		return is_prefix_char(
				   style.long_option_prefix, style.long_option_size, c)
			|| is_prefix_char(
				style.short_option_prefix, style.short_option_size, c);
	}

	inline bool is_prefix_char(const std::string & prefix,
		std::size_t size,
		std::string::value_type c) const noexcept
	{
		return !prefix.empty() && size > 0
			&& prefix.find(c) != std::string::npos;
	}

	inline std::string prefix_value(
		const std::string & prefix, std::size_t size) const
	{
		return std::string(
			static_cast<typename std::string::size_type>(size), prefix[0]);
	}
};

}} // namespace lyra::detail

#endif

#ifndef LYRA_VAL_HPP
#define LYRA_VAL_HPP

#include <memory>

namespace lyra {

/* tag::reference[]

[#lyra_val]
= `lyra::val`

[source]
----
auto val(T && v);
auto val(const char * v);
----

Makes a bound self-contained value of the type of the given r-value. The created
bound values can be used in place of the value references for arguments. And can
be retrieved with the
<<lyra_cli_array_ref>> call.

*/ // end::reference[]
template <typename T>
detail::BoundVal<T> val(T && v)
{
	return detail::BoundVal<T>(std::forward<T>(v));
}

inline detail::BoundVal<std::string> val(const char * v)
{
	return detail::BoundVal<std::string>(v);
}

} // namespace lyra

#endif

#include <memory>
#include <string>
#include <type_traits>

namespace lyra {

namespace detail {

class parse_state
{
	public:
	parse_state(parser_result_type type,
		token_iterator const & remaining_tokens,
		size_t parsed_tokens = 0)
		: result_type(type)
		, tokens(remaining_tokens)
	{
		(void)parsed_tokens;
	}

	parser_result_type type() const { return result_type; }
	token_iterator remainingTokens() const { return tokens; }
	bool have_tokens() const { return bool(tokens); }

	private:
	parser_result_type result_type;
	token_iterator tokens;
};

struct parser_cardinality
{
	size_t minimum = 0;
	size_t maximum = 0;

	parser_cardinality() = default;

	parser_cardinality(size_t a, size_t b)
		: minimum(a)
		, maximum(b)
	{}

	bool is_optional() const { return (minimum == 0); }

	bool is_unbounded() const { return (maximum == 0); }

	bool is_bounded() const { return !is_unbounded(); }

	bool is_required() const { return (minimum > 0); }

	void optional()
	{
		minimum = 0;
		maximum = 1;
	}
	void required(size_t n = 1)
	{
		minimum = n;
		maximum = n;
	}
	void counted(size_t n)
	{
		minimum = n;
		maximum = n;
	}
	void bounded(size_t n, size_t m)
	{
		minimum = n;
		maximum = m;
	}
};

} // namespace detail

/* tag::reference[]

[#lyra_parser_result]
= `lyra::parser_result`

The result of parsing arguments.

end::reference[] */
class parse_result : public detail::basic_result<detail::parse_state>
{
	public:
	using base = detail::basic_result<detail::parse_state>;
	using base::basic_result;
	using base::error;
	using base::ok;

	parse_result(const base & other)
		: base(other)
	{}
};

/* tag::reference[]

[#lyra_parser]
= `lyra::parser`

Base for all argument parser types.

end::reference[] */
class parser
{
	public:
	struct help_text_item
	{
		std::string option;
		std::string description;
	};

	using help_text = std::vector<help_text_item>;

	[[deprecated]] help_text get_help_text() const { return {}; }
	[[deprecated]] std::string get_usage_text() const { return ""; }
	[[deprecated]] std::string get_description_text() const { return ""; }

	virtual help_text get_help_text(const option_style &) const { return {}; }
	virtual std::string get_usage_text(const option_style &) const
	{
		return "";
	}
	virtual std::string get_description_text(const option_style &) const
	{
		return "";
	}

	virtual ~parser() = default;

	virtual detail::parser_cardinality cardinality() const { return { 0, 1 }; }
	bool is_optional() const { return cardinality().is_optional(); }
	virtual bool is_group() const { return false; }
	virtual result validate() const { return result::ok(); }
	virtual std::unique_ptr<parser> clone() const { return nullptr; }
	virtual bool is_named(const std::string & n) const
	{
		(void)n;
		return false;
	}
	virtual const parser * get_named(const std::string & n) const
	{
		if (is_named(n)) return this;
		return nullptr;
	}
	virtual size_t get_value_count() const { return 0; }
	virtual std::string get_value(size_t i) const
	{
		(void)i;
		return "";
	}

	virtual parse_result parse(detail::token_iterator const & tokens,
		const option_style & style) const = 0;

	protected:
	void print_help_text(std::ostream & os, const option_style & style) const
	{
		std::string usage_test = get_usage_text(style);
		if (!usage_test.empty())
			os << "USAGE:\n"
			   << "  " << get_usage_text(style) << "\n\n";

		std::string description_test = get_description_text(style);
		if (!description_test.empty())
			os << get_description_text(style) << "\n";

		os << "OPTIONS, ARGUMENTS:\n";
		const std::string::size_type left_col_size = 26 - 3;
		const std::string left_pad(left_col_size, ' ');
		for (auto const & cols : get_help_text(style))
		{
			if (cols.option.size() > left_pad.size())
				os << "  " << cols.option << "\n  " << left_pad << " "
				   << cols.description << "\n";
			else
				os << "  " << cols.option
				   << left_pad.substr(0, left_pad.size() - cols.option.size())
				   << " " << cols.description << "\n";
		}
	}
};

/* tag::reference[]

[#lyra_parser_specification]
== Specification

[#lyra_parser_help_text_item]
=== `lyra::parser::help_text_item`

[source]
----
struct lyra::parser::help_text_item
{
	std::string option;
	std::string description;
};
----

Holds the help information for a single argument option. The `option` member is
the long name of the option. And the `description` is the text describing the
option. A list of them is returned from the `lyra::parser::get_help_text`
method.

[#lyra_parser_help_text]
=== `lyra::parser::help_text`

[source]
----
using help_text = std::vector<help_text_item>;
----

The set of help texts for any options in the sub-parsers to this one, if any.

[#lyra_parser_get_help_text]
=== `lyra::parser::get_help_text`

[source]
----
virtual help_text get_help_text(const option_style &) const;
----

Collects, and returns, the set of help items for the sub-parser arguments in
this parser, if any. The default is to return an empty set. Which is what most
parsers will return. Parsers like `arguments`, `group`, and `cli` will return a
set for the arguments defined. This is called to print out the help text from
the stream operator.

[#lyra_parser_get_usage_text]
=== `lyra::parser::get_usage_text`

[source]
----
virtual std::string get_usage_text(const option_style &) const;
----

Returns the formatted `USAGE` text for this parser, and any contained
sub-parsers. This is called to print out the help text from the stream operator.

[#lyra_parser_get_description_text]
=== `lyra::parser::get_description_text`

[source]
----
virtual std::string get_description_text(const option_style &) const;
----

Returns the description text for this, and any contained sub-parsers. This is
called to print out the help text from the stream operator.

end::reference[] */

template <typename T, typename U>
std::unique_ptr<parser> make_clone(const U * source)
{
	return std::unique_ptr<parser>(new T(*static_cast<const T *>(source)));
}

/* tag::reference[]

[#lyra_composable_parser]
= `lyra::composable_parser`

A parser that can be composed with other parsers using `operator|`. Composing
two `composable_parser` instances generates a `cli` parser.

end::reference[] */
template <typename Derived>
class composable_parser : public parser
{};

/* tag::reference[]

[#lyra_bound_parser]
= `lyra::bound_parser`

Parser that binds a variable reference or callback to the value of an argument.

end::reference[] */
template <typename Derived>
class bound_parser : public composable_parser<Derived>
{
	protected:
	std::shared_ptr<detail::BoundRef> m_ref;
	std::string m_hint;
	std::string m_description;
	detail::parser_cardinality m_cardinality;
	std::shared_ptr<detail::choices_base> value_choices;

	explicit bound_parser(std::shared_ptr<detail::BoundRef> const & ref)
		: m_ref(ref)
	{
		if (m_ref->isContainer())
			m_cardinality = { 0, 0 };
		else
			m_cardinality = { 0, 1 };
	}
	bound_parser(
		std::shared_ptr<detail::BoundRef> const & ref, std::string const & hint)
		: m_ref(ref)
		, m_hint(hint)
	{
		if (m_ref->isContainer())
			m_cardinality = { 0, 0 };
		else
			m_cardinality = { 0, 1 };
	}

	public:
	enum class ctor_lambda_e
	{
		val
	};

	template <typename Reference>
	bound_parser(Reference & ref, std::string const & hint);

	template <typename Lambda>
	bound_parser(Lambda const & ref,
		std::string const & hint,
		typename std::enable_if<detail::is_invocable<Lambda>::value,
			ctor_lambda_e>::type
		= ctor_lambda_e::val);

	template <typename T>
	explicit bound_parser(detail::BoundVal<T> && val)
		: bound_parser(val.move_to_shared())
	{}
	template <typename T>
	bound_parser(detail::BoundVal<T> && val, std::string const & hint)
		: bound_parser(val.move_to_shared(), hint)
	{}

	Derived & help(const std::string & text);
	Derived & operator()(std::string const & description);
	Derived & optional();
	Derived & required(size_t n = 1);
	Derived & cardinality(size_t n);
	Derived & cardinality(size_t n, size_t m);
	detail::parser_cardinality cardinality() const override
	{
		return m_cardinality;
	}
	std::string hint() const;
	Derived & hint(std::string const & hint);

	template <typename T,
		typename... Rest,
		typename std::enable_if<!detail::is_invocable<T>::value, int>::type = 0>
	Derived & choices(T val0, Rest... rest);
	template <typename Lambda,
		typename std::enable_if<detail::is_invocable<Lambda>::value, int>::type
		= 1>
	Derived & choices(Lambda const & check_choice);
	template <typename T, std::size_t N>
	Derived & choices(const T (&choice_values)[N]);

	virtual std::unique_ptr<parser> clone() const override
	{
		return make_clone<Derived>(this);
	}

	virtual bool is_named(const std::string & n) const override
	{
		return n == m_hint;
	}
	virtual size_t get_value_count() const override
	{
		return m_ref->get_value_count();
	}
	virtual std::string get_value(size_t i) const override
	{
		return m_ref->get_value(i);
	}
};

/* tag::reference[]

[#lyra_bound_parser_ctor]
== Construction

end::reference[] */

/* tag::reference[]
[source]
----
template <typename Derived>
template <typename Reference>
bound_parser<Derived>::bound_parser(Reference& ref, std::string const& hint);

template <typename Derived>
template <typename Lambda>
bound_parser<Derived>::bound_parser(Lambda const& ref, std::string const& hint);
----

Constructs a value option with a target typed variable or callback. These are
options that take a value as in `--opt=value`. In the first form the given
`ref` receives the value of the option after parsing. The second form the
callback is called during the parse with the given value. Both take a
`hint` that is used in the help text. When the option can be specified
multiple times the callback will be called consecutively for each option value
given. And if a container is given as a reference on the first form it will
contain all the specified values.

end::reference[] */
template <typename Derived>
template <typename Reference>
bound_parser<Derived>::bound_parser(Reference & ref, std::string const & hint)
	: bound_parser(
		std::make_shared<detail::BoundValueRef<Reference>>(ref), hint)
{}

template <typename Derived>
template <typename Lambda>
bound_parser<Derived>::bound_parser(Lambda const & ref,
	std::string const & hint,
	typename std::enable_if<detail::is_invocable<Lambda>::value,
		ctor_lambda_e>::type)
	: bound_parser(std::make_shared<detail::BoundLambda<Lambda>>(ref), hint)
{}

/* tag::reference[]

[#lyra_bound_parser_specification]
== Specification

end::reference[] */

/* tag::reference[]

[#lyra_bound_parser_help]
=== `lyra::bound_parser::help`, `lyra::bound_parser::operator(help)`

[source]
----
template <typename Derived>
Derived& bound_parser<Derived>::help(std::string const& text);
template <typename Derived>
Derived& bound_parser<Derived>::operator()(std::string const& help);
----

Defines the help description of an argument.

end::reference[] */
template <typename Derived>
Derived & bound_parser<Derived>::help(const std::string & text)
{
	m_description = text;
	return static_cast<Derived &>(*this);
}
template <typename Derived>
Derived & bound_parser<Derived>::operator()(std::string const & help_text)
{
	return this->help(help_text);
}

/* tag::reference[]

[#lyra_bound_parser_optional]
=== `lyra::bound_parser::optional`

[source]
----
template <typename Derived>
Derived& bound_parser<Derived>::optional();
----

Indicates that the argument is optional. This is equivalent to specifying
`cardinality(0, 1)`.

end::reference[] */
template <typename Derived>
Derived & bound_parser<Derived>::optional()
{
	return this->cardinality(0, 1);
}

/* tag::reference[]

[#lyra_bound_parser_required]
=== `lyra::bound_parser::required(n)`

[source]
----
template <typename Derived>
Derived& bound_parser<Derived>::required(size_t n);
----

Specifies that the argument needs to given the number of `n` times
(defaults to *1*).

end::reference[] */
template <typename Derived>
Derived & bound_parser<Derived>::required(size_t n)
{
	if (m_ref->isContainer())
		return this->cardinality(1, 0);
	else
		return this->cardinality(n);
}

/* tag::reference[]

[#lyra_bound_parser_cardinality]
=== `lyra::bound_parser::cardinality(n, m)`

[source]
----
template <typename Derived>
Derived& bound_parser<Derived>::cardinality(size_t n);

template <typename Derived>
Derived& bound_parser<Derived>::cardinality(size_t n, size_t m);
----

Specifies the number of times the argument can and needs to appear in the list
of arguments. In the first form the argument can appear exactly `n` times. In
the second form it specifies that the argument can appear from `n` to `m` times
inclusive.

end::reference[] */
template <typename Derived>
Derived & bound_parser<Derived>::cardinality(size_t n)
{
	m_cardinality = { n, n };
	return static_cast<Derived &>(*this);
}

template <typename Derived>
Derived & bound_parser<Derived>::cardinality(size_t n, size_t m)
{
	m_cardinality = { n, m };
	return static_cast<Derived &>(*this);
}

/* tag::reference[]

[#lyra_bound_parser_choices]
=== `lyra::bound_parser::choices`

[source]
----
template <typename Derived>
template <typename T, typename... Rest>
lyra::opt& lyra::bound_parser<Derived>::choices(T val0, Rest... rest)

template <typename Derived>
template <typename Lambda>
lyra::opt& lyra::bound_parser<Derived>::choices(Lambda const &check_choice)
----

Limit the allowed values of an argument. In the first form the value is
limited to the ones listed in the call (two or more values). In the second
form the `check_choice` function is called with the parsed value and returns
`true` if it's an allowed value.

end::reference[] */
template <typename Derived>
template <typename T,
	typename... Rest,
	typename std::enable_if<!detail::is_invocable<T>::value, int>::type>
Derived & bound_parser<Derived>::choices(T val0, Rest... rest)
{
	value_choices = std::make_shared<detail::choices_set<T>>(val0, rest...);
	return static_cast<Derived &>(*this);
}

template <typename Derived>
template <typename Lambda,
	typename std::enable_if<detail::is_invocable<Lambda>::value, int>::type>
Derived & bound_parser<Derived>::choices(Lambda const & check_choice)
{
	value_choices
		= std::make_shared<detail::choices_check<Lambda>>(check_choice);
	return static_cast<Derived &>(*this);
}

template <typename Derived>
template <typename T, std::size_t N>
Derived & bound_parser<Derived>::choices(const T (&choice_values)[N])
{
	value_choices = std::make_shared<detail::choices_set<T>>(
		std::vector<T> { choice_values, choice_values + N });
	return static_cast<Derived &>(*this);
}

/* tag::reference[]

[#lyra_bound_parser_hint]
=== `lyra::bound_parser::hint`

[source]
----
template <typename Derived>
std::string lyra::bound_parser<Derived>::hint() const

template <typename Derived>
Derived & lyra::bound_parser<Derived>::hint(std::string const & hint)
----

Selectors to read and write the hint of a variable-bound parser.

The hint should not be modified anymore, once the parser is applied to arguments
or used in a `lyra::composable_parser`.

end::reference[] */
template <typename Derived>
std::string bound_parser<Derived>::hint() const
{
	return m_hint;
}

template <typename Derived>
Derived & bound_parser<Derived>::hint(std::string const & hint)
{
	m_hint = hint;
	return static_cast<Derived &>(*this);
}

} // namespace lyra

#endif

namespace lyra {

/* tag::reference[]

[#lyra_arg]
= `lyra::arg`

A parser for regular arguments, i.e. not `--` or `-` prefixed. This is simply
a way to get values of arguments directly specified in the cli.

Is-a <<lyra_bound_parser>>.

*/ // end::reference[]
class arg : public bound_parser<arg>
{
	public:
	using bound_parser::bound_parser;

	virtual std::string get_usage_text(const option_style &) const override
	{
		std::ostringstream oss;
		if (!m_hint.empty())
		{
			auto c = cardinality();
			if (c.is_required())
			{
				for (size_t i = 0; i < c.minimum; ++i)
					oss << (i > 0 ? " " : "") << "<" << m_hint << ">";
				if (c.is_unbounded())
					oss << (c.is_required() ? " " : "") << "[<" << m_hint
						<< ">...]";
			}
			else if (c.is_unbounded())
			{
				oss << "[<" << m_hint << ">...]";
			}
			else
			{
				oss << "<" << m_hint << ">";
			}
		}
		return oss.str();
	}

	virtual help_text get_help_text(const option_style & style) const override
	{
		return { { get_usage_text(style), m_description } };
	}

	using parser::parse;

	parse_result parse(detail::token_iterator const & tokens,
		const option_style & style) const override
	{
		(void)style;
		LYRA_PRINT_SCOPE("arg::parse");
		auto validationResult = validate();
		if (!validationResult) return parse_result(validationResult);

		if (!tokens)
		{
			return parse_result::ok(
				detail::parse_state(parser_result_type::no_match, tokens));
		}

		auto const & token = tokens.argument();

		auto valueRef = static_cast<detail::BoundValueRefBase *>(m_ref.get());

		if (value_choices)
		{
			auto choice_result = value_choices->contains_value(token.name);
			if (!choice_result)
			{
				LYRA_PRINT_DEBUG(
					"(!)", get_usage_text(style), "!=", token.name);
				return parse_result(choice_result);
			}
		}

		auto result = valueRef->setValue(token.name);
		if (!result)
		{
			LYRA_PRINT_DEBUG("(!)", get_usage_text(style), "!=", token.name);
			return parse_result(result);
		}
		else
		{
			LYRA_PRINT_DEBUG("(=)", get_usage_text(style), "==", token.name);
			auto remainingTokens = tokens;
			remainingTokens.pop(token);
			return parse_result::ok(detail::parse_state(
				parser_result_type::matched, remainingTokens));
		}
	}
};

} // namespace lyra

#endif

#ifndef LYRA_ARGUMENTS_HPP
#define LYRA_ARGUMENTS_HPP


#ifndef LYRA_EXE_NAME_HPP
#define LYRA_EXE_NAME_HPP


namespace lyra {

/* tag::reference[]

[#lyra_exe_name]
= `lyra::exe_name`

Specifies the name of the executable.

Is-a <<lyra_composable_parser>>.

end::reference[] */
class exe_name : public composable_parser<exe_name>
{
	public:
	exe_name()
		: m_name(std::make_shared<std::string>("<executable>"))
	{}

	explicit exe_name(std::string & ref);

	template <typename LambdaT>
	explicit exe_name(LambdaT const & lambda);

	std::string name() const;
	parser_result set(std::string const & newName);

	virtual parse_result parse(detail::token_iterator const & tokens,
		const option_style &) const override
	{
		return parse_result::ok(
			detail::parse_state(parser_result_type::no_match, tokens));
	}

	virtual std::unique_ptr<parser> clone() const override
	{
		return make_clone<exe_name>(this);
	}

	private:
	std::shared_ptr<std::string> m_name;
	std::shared_ptr<detail::BoundValueRefBase> m_ref;
};

/* tag::reference[]

[#lyra_exe_name_ctor]
== Construction

end::reference[] */

/* tag::reference[]
[source]
----
exe_name::exe_name(std::string& ref)
----

Constructs with a target string to receive the name of the executable. When
the `cli` is run the target string will contain the exec name.

end::reference[] */
inline exe_name::exe_name(std::string & ref)
	: exe_name()
{
	m_ref = std::make_shared<detail::BoundValueRef<std::string>>(ref);
}

/* tag::reference[]
[source]
----
template <typename LambdaT>
exe_name::exe_name(LambdaT const& lambda)
----

Construct with a callback that is called with the value of the executable name
when the `cli` runs.

end::reference[] */
template <typename LambdaT>
exe_name::exe_name(LambdaT const & lambda)
	: exe_name()
{
	m_ref = std::make_shared<detail::BoundLambda<LambdaT>>(lambda);
}

/* tag::reference[]

[#lyra_exe_name_accessors]
== Accessors

end::reference[] */

/* tag::reference[]

[#lyra_exe_name_name]
=== `lyra::exe_name::name`

[source]
----
std::string exe_name::name() const
----

Returns the executable name when available. Otherwise it returns a default
value.

end::reference[] */
inline std::string exe_name::name() const { return *m_name; }

/* tag::reference[]

[#lyra_exe_name_set]
=== `lyra::exe_name::set`

[source]
----
parser_result exe_name::set(std::string const& newName)
----

Sets the executable name with the `newName` value. The new value is reflected
in the bound string reference or callback.

end::reference[] */
inline parser_result exe_name::set(std::string const & newName)
{
	auto lastSlash = newName.find_last_of("\\/");
	auto filename = (lastSlash == std::string::npos)
		? newName
		: newName.substr(lastSlash + 1);

	*m_name = filename;
	if (m_ref)
		return m_ref->setValue(filename);
	else
		return parser_result::ok(parser_result_type::matched);
}

} // namespace lyra

#endif

#include <functional>
#include <sstream>

namespace lyra {

/* tag::reference[]

[#lyra_arguments]
= `lyra::arguments`

A Combined parser made up of any number of parsers. Creating and using
one of these as a basis one can incrementally compose other parsers into this
one. For example:

[source]
----
auto p = lyra::arguments();
std::string what;
float when = 0;
std::string where;
p |= lyra::opt(what, "what")["--make-it-so"]("Make it so.").required();
p |= lyra::opt(when. "when")["--time"]("When to do <what>.").optional();
p.add_argument(lyra::opt(where, "where").name("--where")
	.help("There you are.").optional());
----

*/ // end::reference[]
class arguments : public parser
{
	public:
	enum evaluation
	{
		any = 0,
		sequence = 1
	};

	arguments() = default;

	arguments(evaluation e)
		: eval_mode(e)
	{}

	arguments(const arguments & other);

	arguments & add_argument(parser const & p);
	arguments & operator|=(parser const & p);

	arguments & add_argument(arguments const & other);
	arguments & operator|=(arguments const & other);

	template <typename T>
	arguments operator|(T const & other) const
	{
		return arguments(*this) |= other;
	}

	arguments & sequential();
	arguments & inclusive();

	template <typename T>
	T & get(size_t i);


	virtual std::string get_usage_text(
		const option_style & style) const override
	{
		std::ostringstream os;
		for (auto const & p : parsers)
		{
			std::string usage_text = p->get_usage_text(style);
			if (usage_text.size() > 0)
			{
				if (os.tellp() != std::ostringstream::pos_type(0)) os << " ";
				if (p->is_group() && p->is_optional())
					os << "[ " << usage_text << " ]";
				else if (p->is_group())
					os << "{ " << usage_text << " }";
				else if (p->is_optional())
					os << "[" << usage_text << "]";
				else
					os << usage_text;
			}
		}
		return os.str();
	}

	virtual std::string get_description_text(
		const option_style & style) const override
	{
		std::ostringstream os;
		for (auto const & p : parsers)
		{
			if (p->is_group()) continue;
			auto child_description = p->get_description_text(style);
			if (!child_description.empty()) os << child_description << "\n";
		}
		return os.str();
	}

	virtual help_text get_help_text(const option_style & style) const override
	{
		help_text text;
		for (auto const & p : parsers)
		{
			if (p->is_group()) text.push_back({ "", "" });
			auto child_help = p->get_help_text(style);
			text.insert(text.end(), child_help.begin(), child_help.end());
		}
		return text;
	}

	virtual detail::parser_cardinality cardinality() const override
	{
		return { 0, 0 };
	}

	virtual result validate() const override
	{
		for (auto const & p : parsers)
		{
			auto result = p->validate();
			if (!result) return result;
		}
		return result::ok();
	}

	parse_result parse(detail::token_iterator const & tokens,
		const option_style & style) const override
	{
		switch (eval_mode)
		{
			case any: return parse_any(tokens, style);
			case sequence: return parse_sequence(tokens, style);
		}
		return parse_result::error(
			detail::parse_state(parser_result_type::no_match, tokens),
			"Unknown evaluation mode; not one of 'any', or 'sequence'.");
	}

	parse_result parse_any(
		detail::token_iterator const & tokens, const option_style & style) const
	{
		LYRA_PRINT_SCOPE("arguments::parse_any");
		LYRA_PRINT_DEBUG("(?)", get_usage_text(style),
			"?=", tokens ? tokens.argument().name : "", "..");

		struct ParserInfo
		{
			parser const * parser_p = nullptr;
			size_t count = 0;
		};
		std::vector<ParserInfo> parser_info(parsers.size());
		{
			size_t i = 0;
			for (auto const & p : parsers) parser_info[i++].parser_p = p.get();
		}

		auto result = parse_result::ok(
			detail::parse_state(parser_result_type::matched, tokens));
		auto error_result = parse_result::ok(
			detail::parse_state(parser_result_type::no_match, tokens));
		while (result.value().remainingTokens())
		{
			bool token_parsed = false;

			for (auto & parse_info : parser_info)
			{
				auto parser_cardinality = parse_info.parser_p->cardinality();
				if (parser_cardinality.is_unbounded()
					|| parse_info.count < parser_cardinality.maximum)
				{
					auto subparse_result = parse_info.parser_p->parse(
						result.value().remainingTokens(), style);
					if (!subparse_result)
					{
						LYRA_PRINT_DEBUG("(!)", get_usage_text(style), "!=",
							result.value().remainingTokens().argument().name);
						if (subparse_result.has_value()
							&& subparse_result.value().type()
								== parser_result_type::short_circuit_all)
							return subparse_result;
						if (error_result)
							error_result = parse_result(subparse_result);
					}
					else if (subparse_result
						&& subparse_result.value().type()
							!= parser_result_type::no_match)
					{
						LYRA_PRINT_DEBUG("(=)", get_usage_text(style), "==",
							result.value().remainingTokens().argument().name,
							"==>", subparse_result.value().type());
						result = parse_result(subparse_result);
						token_parsed = true;
						parse_info.count += 1;
						break;
					}
				}
			}

			if (result.value().type() == parser_result_type::short_circuit_all)
				return result;
			if (!token_parsed && !error_result) return error_result;
			if (!token_parsed) break;
		}
		for (auto & parseInfo : parser_info)
		{
			auto parser_cardinality = parseInfo.parser_p->cardinality();
			if ((parser_cardinality.is_bounded()
					&& (parseInfo.count < parser_cardinality.minimum
						|| parser_cardinality.maximum < parseInfo.count))
				|| (parser_cardinality.is_required()
					&& (parseInfo.count < parser_cardinality.minimum)))
			{
				return parse_result::error(result.value(),
					"Expected: " + parseInfo.parser_p->get_usage_text(style));
			}
		}
		return result;
	}

	parse_result parse_sequence(
		detail::token_iterator const & tokens, const option_style & style) const
	{
		LYRA_PRINT_SCOPE("arguments::parse_sequence");
		LYRA_PRINT_DEBUG("(?)", get_usage_text(style),
			"?=", tokens ? tokens.argument().name : "", "..");

		struct ParserInfo
		{
			parser const * parser_p = nullptr;
			size_t count = 0;
		};
		std::vector<ParserInfo> parser_info(parsers.size());
		{
			size_t i = 0;
			for (auto const & p : parsers) parser_info[i++].parser_p = p.get();
		}

		auto result = parse_result::ok(
			detail::parse_state(parser_result_type::matched, tokens));

		for (std::size_t parser_i = 0; parser_i < parsers.size(); ++parser_i)
		{
			auto & parse_info = parser_info[parser_i];
			auto parser_cardinality = parse_info.parser_p->cardinality();
			do
			{
				auto subresult = parse_info.parser_p->parse(
					result.value().remainingTokens(), style);
				if (!subresult)
				{
					break;
				}
				if (subresult.value().type()
					== parser_result_type::short_circuit_all)
				{
					return subresult;
				}
				if (subresult.value().type() != parser_result_type::no_match)
				{
					LYRA_PRINT_DEBUG("(=)", get_usage_text(style), "==",
						result.value().remainingTokens()
							? result.value().remainingTokens().argument().name
							: "",
						"==>", subresult.value().type());
					result = subresult;
					parse_info.count += 1;
				}
			}
			while (result.value().have_tokens()
				&& (parser_cardinality.is_unbounded()
					|| parse_info.count < parser_cardinality.maximum));
			if ((parser_cardinality.is_bounded()
					&& (parse_info.count < parser_cardinality.minimum
						|| parser_cardinality.maximum < parse_info.count))
				|| (parser_cardinality.is_required()
					&& (parse_info.count < parser_cardinality.minimum)))
			{
				return parse_result::error(result.value(),
					"Expected: " + parse_info.parser_p->get_usage_text(style));
			}
		}
		return result;
	}

	virtual std::unique_ptr<parser> clone() const override
	{
		return make_clone<arguments>(this);
	}

	friend std::ostream & operator<<(
		std::ostream & os, arguments const & parser)
	{
		const option_style & s
			= parser.opt_style ? *parser.opt_style : option_style::posix();
		parser.print_help_text(os, s);
		return os;
	}

	virtual const parser * get_named(const std::string & n) const override
	{
		for (auto & p : parsers)
		{
			const parser * result = p->get_named(n);
			if (result) return result;
		}
		return nullptr;
	}

	protected:
	std::shared_ptr<option_style> opt_style;

	private:
	std::vector<std::unique_ptr<parser>> parsers;
	evaluation eval_mode = any;
};

/* tag::reference[]

[#lyra_arguments_ctor]
== Construction

end::reference[] */

/* tag::reference[]

[#lyra_arguments_ctor_default]
=== Default

[source]
----
arguments() = default;
----

Default constructing a `arguments` is the starting point to adding arguments
and options for parsing a arguments line.

end::reference[] */

/* tag::reference[]

[#lyra_arguments_ctor_copy]
=== Copy

[source]
----
arguments::arguments(const arguments& other);
----

end::reference[] */
inline arguments::arguments(const arguments & other)
	: parser(other)
	, opt_style(other.opt_style)
	, eval_mode(other.eval_mode)
{
	for (auto & other_parser : other.parsers)
	{
		parsers.push_back(other_parser->clone());
	}
}

/* tag::reference[]

[#lyra_arguments_specification]
== Specification

end::reference[] */


/* tag::reference[]
[#lyra_arguments_add_argument]
=== `lyra::arguments::add_argument`

[source]
----
arguments& arguments::add_argument(parser const& p);
arguments& arguments::operator|=(parser const& p);
arguments& arguments::add_argument(arguments const& other);
arguments& arguments::operator|=(arguments const& other);
----

Adds the given argument parser to the considered arguments for this
`arguments`. Depending on the parser given it will be: directly added as an
argument (for `parser`), or add the parsers from another `arguments` to
this one.

end::reference[] */
inline arguments & arguments::add_argument(parser const & p)
{
	parsers.push_back(p.clone());
	return *this;
}
inline arguments & arguments::operator|=(parser const & p)
{
	return this->add_argument(p);
}
inline arguments & arguments::add_argument(arguments const & other)
{
	if (other.is_group())
	{
		parsers.push_back(other.clone());
	}
	else
	{
		for (auto & p : other.parsers)
		{
			parsers.push_back(p->clone());
		}
	}
	return *this;
}
inline arguments & arguments::operator|=(arguments const & other)
{
	return this->add_argument(other);
}

/* tag::reference[]
=== `lyra::arguments::sequential`

[source]
----
arguments & arguments::sequential();
----

Sets the parsing mode for the arguments to "sequential". When parsing the
arguments they will be, greedily, consumed in the order they where added.
This is useful for sub-commands and structured command lines.

end::reference[] */
inline arguments & arguments::sequential()
{
	eval_mode = sequence;
	return *this;
}

/* tag::reference[]
=== `lyra::arguments::inclusive`

[source]
----
arguments & arguments::inclusive();
----

Sets the parsing mode for the arguments to "inclusively any". This is the
default that attempts to match each parsed argument with all the available
parsers. This means that there is no ordering enforced.

end::reference[] */
inline arguments & arguments::inclusive()
{
	eval_mode = any;
	return *this;
}

/* tag::reference[]
=== `lyra::arguments::get`

[source]
----
template <typename T>
T & arguments::get(size_t i);
----

Get a modifyable reference to one of the parsers specified.

end::reference[] */
template <typename T>
T & arguments::get(size_t i)
{
	return static_cast<T &>(*parsers.at(i));
}

} // namespace lyra

#endif

#ifndef LYRA_CLI_HPP
#define LYRA_CLI_HPP


#ifndef LYRA_DETAIL_DEPRECATED_PARSER_CUSTOMIZATION_HPP
#define LYRA_DETAIL_DEPRECATED_PARSER_CUSTOMIZATION_HPP

#include <string>

namespace lyra {

/* tag::reference[]

[#lyra_parser_customization]
= `lyra::parser_customization`

Customization interface for parsing of options.

[source]
----
virtual std::string token_delimiters() const = 0;
----

Specifies the characters to use for splitting a cli argument into the option
and its value (if any).

[source]
----
virtual std::string option_prefix() const = 0;
----

Specifies the characters to use as possible prefix, either single or double,
for all options.

end::reference[] */
struct parser_customization
{
	virtual std::string token_delimiters() const = 0;
	virtual std::string option_prefix() const = 0;
};

/* tag::reference[]

[#lyra_default_parser_customization]
= `lyra::default_parser_customization`

Is-a `lyra::parser_customization` that defines token delimiters as space (" ")
or equal (`=`). And specifies the option prefix character as dash (`-`)
resulting in long options with `--` and short options with `-`.

This customization is used as the default if none is given.

end::reference[] */
struct default_parser_customization : parser_customization
{
	std::string token_delimiters() const override { return " ="; }
	std::string option_prefix() const override { return "-"; }
};

} // namespace lyra

#endif

#ifndef LYRA_GROUP_HPP
#define LYRA_GROUP_HPP

#include <functional>

namespace lyra {

/* tag::reference[]

[#lyra_group]
= `lyra::group`

A group of arguments provides for parsing, optionally, a set of arguments
together. The group itself is considered successfully parsed only when the
arguments in the group are parsed without errors. A common use case for this
are sub-commands. This implementation is recursive. And hence allows groups
within groups for describing branching argument parsing.

Is-a <<lyra_arguments>>.

end::reference[] */
class group : public arguments
{
	public:
	group();
	group(const group & other);
	explicit group(const std::function<void(const group &)> & f);

	virtual bool is_group() const override { return true; }

	parse_result parse(detail::token_iterator const & tokens,
		const option_style & style) const override
	{
		LYRA_PRINT_SCOPE("group::parse");
		LYRA_PRINT_DEBUG("(?)", get_usage_text(style),
			"?=", tokens ? tokens.argument().name : "");
		parse_result result = arguments::parse(tokens, style);
		if (result && result.value().type() != parser_result_type::no_match
			&& success_signal)
		{
			this->success_signal(*this);
		}
		if (!result)
		{
			LYRA_PRINT_DEBUG("(!)", get_usage_text(style),
				"!=", tokens ? tokens.argument().name : "");
		}
		else
		{
			LYRA_PRINT_DEBUG("(=)", get_usage_text(style),
				"==", tokens ? tokens.argument().name : "", "==>",
				result.value().type());
		}
		return result;
	}

	group & optional();
	group & required(size_t n = 1);
	group & cardinality(size_t n);
	group & cardinality(size_t n, size_t m);
	detail::parser_cardinality cardinality() const override
	{
		return m_cardinality;
	}

	virtual std::unique_ptr<parser> clone() const override
	{
		return make_clone<group>(this);
	}

	private:
	std::function<void(const group &)> success_signal;
	detail::parser_cardinality m_cardinality = { 0, 1 };
};

/* tag::reference[]

[#lyra_group_ctor]
== Construction

end::reference[] */

/* tag::reference[]

[#lyra_group_ctor_default]
=== Default

[source]
----
group();
----

Default constructing a `group` does not register the success callback.

end::reference[] */
inline group::group()
	: m_cardinality(0, 1)
{}

/* tag::reference[]

[#lyra_group_ctor_copy]
=== Copy

[source]
----
group::group(const group & other);
----

end::reference[] */
inline group::group(const group & other)
	: arguments(other)
	, success_signal(other.success_signal)
	, m_cardinality(other.m_cardinality)
{}

/* tag::reference[]

[#lyra_group_ctor_success]
=== Success Handler

[source]
----
group::group(const std::function<void(const group &)> & f)
----

Registers a function to call when the group is successfully parsed. The
function is called with the group to facilitate customization.

end::reference[] */
inline group::group(const std::function<void(const group &)> & f)
	: success_signal(f)
{}

/* tag::reference[]

[#lyra_group_optional]
=== `lyra::group::optional`

[source]
----
group & group::optional();
----

Indicates that the argument is optional. This is equivalent to specifying
`cardinality(0, 1)`.

end::reference[] */
inline group & group::optional()
{
	m_cardinality.optional();
	return *this;
}

/* tag::reference[]

[#lyra_group_required]
=== `lyra::group::required(n)`

[source]
----
group & group::required(size_t n);
----

Specifies that the argument needs to given the number of `n` times
(defaults to *1*).

end::reference[] */
inline group & group::required(size_t n)
{
	m_cardinality.required(n);
	return *this;
}

/* tag::reference[]

[#lyra_group_cardinality]
=== `lyra::group::cardinality(n)`

[source]
----
group & group::cardinality(size_t n);
group & group::cardinality(size_t n, size_t m);
----

Specifies the number of times the argument can and needs to appear in the list
of arguments. In the first form the argument can appear exactly `n` times. In
the second form it specifies that the argument can appear from `n` to `m` times
inclusive.

end::reference[] */
inline group & group::cardinality(size_t n)
{
	m_cardinality.counted(n);
	return *this;
}
inline group & group::cardinality(size_t n, size_t m)
{
	m_cardinality.bounded(n, m);
	return *this;
}

} // namespace lyra

#endif

#include <type_traits>

namespace lyra {

/* tag::reference[]

[#lyra_cli]
= `lyra::cli`

A Combined parser made up of any two or more other parsers. Creating and using
one of these as a basis one can incrementally compose other parsers into this
one. For example:

[source]
----
auto cli = lyra::cli();
std::string what;
float when = 0;
std::string where;
cli |= lyra::opt(what, "what")["--make-it-so"]("Make it so.").required();
cli |= lyra::opt(when. "when")["--time"]("When to do <what>.").optional();
cli.add_argument(lyra::opt(where, "where").name("--where")
	.help("There you are.").optional());
----

*/ // end::reference[]
class cli : protected arguments
{
	public:
	cli() = default;

	cli(const cli & other);

	cli & add_argument(exe_name const & exe_name);
	cli & operator|=(exe_name const & exe_name);

	cli & add_argument(parser const & p);
	cli & operator|=(parser const & p);

	cli & add_argument(group const & p);
	cli & operator|=(group const & p);

	cli & add_argument(cli const & other);
	cli & operator|=(cli const & other);

	template <typename T>
	cli operator|(T const & other) const;

	struct value_result
	{
		public:
		explicit value_result(const parser * p)
			: parser_ref(p)
		{}

		template <typename T,
			typename std::enable_if<detail::is_convertible_from_string<
				typename detail::remove_cvref<T>::type>::value>::
				type * = nullptr>
		operator T() const
		{
			typename detail::remove_cvref<T>::type result {};
			if (parser_ref)
				detail::from_string<std::string,
					typename detail::remove_cvref<T>::type>(
					parser_ref->get_value(0), result);
			return result;
		}

		template <typename T>
		operator std::vector<T>() const
		{
			std::vector<T> result;
			if (parser_ref)
			{
				for (size_t i = 0; i < parser_ref->get_value_count(); ++i)
				{
					T v;
					if (detail::from_string(parser_ref->get_value(i), v))
						result.push_back(v);
				}
			}
			return result;
		}

		operator std::string() const
		{
			if (parser_ref) return parser_ref->get_value(0);
			return "";
		}

		private:
		const parser * parser_ref = nullptr;
	};

	value_result operator[](const std::string & n);

	cli & style(const option_style & style);
	cli & style(option_style && style);

	friend std::ostream & operator<<(std::ostream & os, cli const & parser)
	{
		return os << static_cast<const arguments &>(parser);
	}

	parse_result parse(args const & args) const
	{
		if (opt_style)
			return parse(args, *opt_style);
		else
			return parse(args, option_style::posix());
	}
	parse_result parse(args const & args, const option_style & style) const;

	[[deprecated]] parse_result parse(
		args const & args, const parser_customization & customize) const
	{
		return this->parse(args,
			option_style(customize.token_delimiters(),
				customize.option_prefix(), 2, customize.option_prefix(), 1));
	}


	using arguments::parse;
	using arguments::get_named;

	virtual std::unique_ptr<parser> clone() const override
	{
		return std::unique_ptr<parser>(new cli(*this));
	}

	protected:
	mutable exe_name m_exeName;

	virtual std::string get_usage_text(
		const option_style & style) const override
	{
		if (!m_exeName.name().empty())
			return m_exeName.name() + " " + arguments::get_usage_text(style);
		else
			return "";
	}
};

/* tag::reference[]

[#lyra_cli_ctor]
== Construction

end::reference[] */

/* tag::reference[]

[#lyra_cli_ctor_default]
=== Default

[source]
----
cli() = default;
----

Default constructing a `cli` is the starting point to adding arguments
and options for parsing a command line.

end::reference[] */

/* tag::reference[]

[#lyra_cli_ctor_copy]
=== Copy

[source]
----
cli::cli(const cli& other);
----

end::reference[] */
inline cli::cli(const cli & other)
	: arguments(other)
	, m_exeName(other.m_exeName)
{}

/* tag::reference[]

[#lyra_cli_specification]
== Specification

end::reference[] */


/* tag::reference[]
[#lyra_cli_add_argument]
=== `lyra::cli::add_argument`

[source]
----
cli& cli::add_argument(exe_name const& exe_name);
cli& cli::operator|=(exe_name const& exe_name);
cli& cli::add_argument(parser const& p);
cli& cli::operator|=(parser const& p);
cli& cli::add_argument(group const& p);
cli& cli::operator|=(group const& p);
cli& cli::add_argument(cli const& other);
cli& cli::operator|=(cli const& other);
----

Adds the given argument parser to the considered arguments for this
`cli`. Depending on the parser given it will be: recorded as the exe
name (for `exe_name` parser), directly added as an argument (for
`parser`), or add the parsers from another `cli` to this one.

end::reference[] */
inline cli & cli::add_argument(exe_name const & exe_name)
{
	m_exeName = exe_name;
	return *this;
}
inline cli & cli::operator|=(exe_name const & exe_name)
{
	return this->add_argument(exe_name);
}
inline cli & cli::add_argument(parser const & p)
{
	arguments::add_argument(p);
	return *this;
}
inline cli & cli::operator|=(parser const & p)
{
	arguments::add_argument(p);
	return *this;
}
inline cli & cli::add_argument(group const & other)
{
	arguments::add_argument(static_cast<parser const &>(other));
	return *this;
}
inline cli & cli::operator|=(group const & other)
{
	return this->add_argument(other);
}
inline cli & cli::add_argument(cli const & other)
{
	arguments::add_argument(static_cast<arguments const &>(other));
	return *this;
}
inline cli & cli::operator|=(cli const & other)
{
	return this->add_argument(other);
}

template <typename T>
inline cli cli::operator|(T const & other) const
{
	return cli(*this).add_argument(other);
}

template <typename DerivedT, typename T>
cli operator|(composable_parser<DerivedT> const & thing, T const & other)
{
	return cli() | static_cast<DerivedT const &>(thing) | other;
}

/* tag::reference[]
[#lyra_cli_array_ref]
=== `lyra::cli::operator[]`

[source]
----
cli::value_result cli::operator[](const std::string & n)
----

Finds the given argument by either option name or hint name and returns a
convertible reference to the value, either the one provided by the user or the
default.

end::reference[] */
inline cli::value_result cli::operator[](const std::string & n)
{
	return value_result(this->get_named(n));
}

/* tag::reference[]
[#lyra_cli_parse]
=== `lyra::cli::parse`

[source]
----
parse_result cli::parse(
	args const& args, const option_style& customize) const;
----

Parses given arguments `args` and optional option style.
The result indicates success or failure, and if failure what kind of failure
it was. The state of variables bound to options is unspecified and any bound
callbacks may have been called.

end::reference[] */
inline parse_result cli::parse(
	args const & args, const option_style & style) const
{
	LYRA_PRINT_SCOPE("cli::parse");
	m_exeName.set(args.exe_name());
	detail::token_iterator args_tokens(args, style);
	parse_result result = parse(args_tokens, style);
	if (result
		&& (result.value().type() == parser_result_type::no_match
			|| result.value().type() == parser_result_type::matched))
	{
		if (result.value().have_tokens())
		{
			return parse_result::error(result.value(),
				"Unrecognized token: "
					+ result.value().remainingTokens().argument().name);
		}
	}
	return result;
}

/* tag::reference[]
[#lyra_cli_style]
=== `lyra::cli::style`

[source]
----
lyra::cli & lyra::cli::style(const lyra::option_style & style)
lyra::cli & lyra::cli::style(lyra::option_style && style)
----

Specifies the <<lyra_option_style>> to accept for this instance.

end::reference[] */
inline cli & cli::style(const option_style & style)
{
	opt_style = std::make_shared<option_style>(style);
	return *this;
}
inline cli & cli::style(option_style && style)
{
	opt_style = std::make_shared<option_style>(std::move(style));
	return *this;
}

} // namespace lyra

#endif

#ifndef LYRA_CLI_PARSER_HPP
#define LYRA_CLI_PARSER_HPP


namespace lyra {

using cli_parser = cli;

} // namespace lyra

#endif

#ifndef LYRA_COMMAND_HPP
#define LYRA_COMMAND_HPP


#ifndef LYRA_LITERAL_HPP
#define LYRA_LITERAL_HPP

#include <string>

namespace lyra {

/* tag::reference[]

[#lyra_literal]
= `lyra::literal`

A parser that matches a single fixed value.

Is-a <<lyra_parser>>.

end::reference[] */
class literal : public parser
{
	public:
	literal(std::string const & n);

	literal & help(const std::string & text);
	literal & operator()(std::string const & description);

	virtual detail::parser_cardinality cardinality() const override
	{
		return { 1, 1 };
	}


	virtual std::string get_usage_text(const option_style &) const override
	{
		return name;
	}

	virtual std::string get_description_text(
		const option_style &) const override
	{
		return description;
	}

	virtual help_text get_help_text(const option_style &) const override
	{
		return { { name, description } };
	}

	using parser::parse;

	virtual parse_result parse(detail::token_iterator const & tokens,
		const option_style &) const override
	{
		auto validationResult = validate();
		if (!validationResult) return parse_result(validationResult);

		auto const & token = tokens.argument();
		if (name == token.name)
		{
			auto remainingTokens = tokens;
			remainingTokens.pop(token);
			return parse_result::ok(detail::parse_state(
				parser_result_type::matched, remainingTokens));
		}
		else
		{
			return parse_result(parser_result::error(
				parser_result_type::no_match, "Expected '" + name + "'."));
		}
	}

	virtual std::unique_ptr<parser> clone() const override
	{
		return make_clone<literal>(this);
	}

	protected:
	std::string name;
	std::string description;
};

/* tag::reference[]

[#lyra_literal_ctor]
== Construction

end::reference[] */

/* tag::reference[]

=== Token

[#lyra_literal_ctor_token]

[source]
----
inline literal::literal(std::string const& n)
----

Constructs the literal with the name of the token to match.

end::reference[] */
inline literal::literal(std::string const & n)
	: name(n)
{}

/* tag::reference[]

[#lyra_literal_specification]
== Specification

end::reference[] */

/* tag::reference[]

[#lyra_literal_help]
=== `lyra:literal::help`

[source]
----
literal& literal::help(const std::string& text)
literal& literal::operator()(std::string const& description)
----

Specify a help description for the literal.

end::reference[] */
inline literal & literal::help(const std::string & text)
{
	description = text;
	return *this;
}
inline literal & literal::operator()(std::string const & description)
{
	return this->help(description);
}

} // namespace lyra

#endif
#include <functional>
#include <string>

namespace lyra {

/* tag::reference[]

[#lyra_command]
= `lyra::command`

A parser that encapsulates the pattern of parsing sub-commands. It provides a
quick wrapper for the equivalent arrangement of `group` and `literal` parsers.
For example:

[source]
----
lyra::command c = lyra::command("sub");
----

Is equivalent to:

[source]
----
lyra::command c = lyra::group()
	.sequential()
	.add_argument(literal("sub"))
	.add_argument(group());
lyra::group & g = c.get<lyra::group>(1);
----

I.e. it's conposed of a `literal` followed by the rest of the command arguments.

Is-a <<lyra_group>>.

*/ // end::reference[]
class command : public group
{
	public:
	explicit command(const std::string & n);
	command(
		const std::string & n, const std::function<void(const group &)> & f);

	command & help(const std::string & text);
	command & operator()(std::string const & description);

	template <typename P>
	command & add_argument(P const & p);
	template <typename P>
	command & operator|=(parser const & p);

	virtual std::unique_ptr<parser> clone() const override
	{
		return make_clone<command>(this);
	}
};

/* tag::reference[]

[#lyra_command_ctor]
== Construction

[source]
----
command::command(const std::string & n);
command::command(
	const std::string & n, const std::function<void(const group &)>& f);
----

To construct an `command` we need a name (`n`) that matches, and triggers, that
command.


end::reference[] */
inline command::command(const std::string & n)
{
	this->sequential()
		.add_argument(literal(n))
		.add_argument(group().required());
}
inline command::command(
	const std::string & n, const std::function<void(const group &)> & f)
	: group(f)
{
	this->sequential()
		.add_argument(literal(n))
		.add_argument(group().required());
}

/* tag::reference[]

[#lyra_command_specification]
== Specification

end::reference[] */

/* tag::reference[]

[#lyra_command_help]
=== `lyra:command::help`

[source]
----
command & command::help(const std::string& text)
command & command::operator()(std::string const& description)
----

Specify a help description for the command. This sets the help for the
underlying literal of the command.

end::reference[] */
inline command & command::help(const std::string & text)
{
	this->get<literal>(0).help(text);
	return *this;
}
inline command & command::operator()(std::string const & description)
{
	return this->help(description);
}

/* tag::reference[]
[#lyra_command_add_argument]
=== `lyra::command::add_argument`

[source]
----
template <typename P>
command & command::add_argument(P const & p);
template <typename P>
command & command::operator|=(parser const & p);
----

Adds the given argument parser to the considered arguments for this `comand`.
The argument is added to the sub-group argument instead of this one. Hence it
has the effect of adding arguments *after* the command name.

end::reference[] */
template <typename P>
command & command::add_argument(P const & p)
{
	this->get<group>(1).add_argument(p);
	return *this;
}
template <typename P>
command & command::operator|=(parser const & p)
{
	return this->add_argument(p);
}

} // namespace lyra

#endif

#ifndef LYRA_HELP_HPP
#define LYRA_HELP_HPP


#ifndef LYRA_OPT_HPP
#define LYRA_OPT_HPP

#include <memory>

namespace lyra {

/* tag::reference[]

[#lyra_opt]
= `lyra::opt`

A parser for one option with multiple possible names. The option value(s) are
communicated through a reference to a variable, a container, or a callback.

Is-a <<lyra_bound_parser>>.

end::reference[] */
class opt : public bound_parser<opt>
{
	public:
	enum class ctor_lambda_e
	{
		val
	};
	enum class ctor_ref_e
	{
		val
	};


	explicit opt(bool & ref);

	template <typename L>
	explicit opt(L const & ref,
		typename std::enable_if<detail::is_invocable<L>::value,
			ctor_lambda_e>::type
		= ctor_lambda_e::val);


	template <typename T>
	opt(T & ref,
		std::string const & hint,
		typename std::enable_if<!detail::is_invocable<T>::value,
			ctor_ref_e>::type
		= ctor_ref_e::val);

	template <typename L>
	opt(L const & ref,
		std::string const & hint,
		typename std::enable_if<detail::is_invocable<L>::value,
			ctor_lambda_e>::type
		= ctor_lambda_e::val);

	template <typename T>
	explicit opt(detail::BoundVal<T> && val)
		: bound_parser(val.move_to_shared())
	{}
	template <typename T>
	explicit opt(detail::BoundVal<T> && val, std::string const & hint)
		: bound_parser(val.move_to_shared(), hint)
	{}


	opt & name(const std::string & opt_name);
	opt & operator[](std::string const & opt_name);


	virtual std::string get_usage_text(
		const option_style & style) const override
	{
		std::string result;
		for (std::size_t o = 0; o < opt_names.size(); ++o)
		{
			if (o > 0) result += "|";
			result += format_opt(opt_names[o], style);
		}
		if (!m_hint.empty()) result += " <" + m_hint + ">";
		return result;
	}

	virtual help_text get_help_text(const option_style & style) const override
	{
		std::ostringstream oss;
		bool first = true;
		for (auto const & opt : opt_names)
		{
			if (first)
				first = false;
			else
				oss << ", ";
			oss << format_opt(opt, style);
		}
		if (!m_hint.empty()) oss << " <" << m_hint << ">";
		return { { oss.str(), m_description } };
	}

	virtual bool is_named(const std::string & n) const override
	{
		return bound_parser::is_named(n)
			|| (std::find(opt_names.begin(), opt_names.end(), n)
				!= opt_names.end());
	}

	using parser::parse;

	parse_result parse(detail::token_iterator const & tokens,
		const option_style & style) const override
	{
		LYRA_PRINT_SCOPE("opt::parse");
		auto validationResult = validate();
		if (!validationResult) return parse_result(validationResult);

		auto remainingTokens = tokens;
		if (remainingTokens && remainingTokens.has_option_prefix())
		{
			auto const & token = remainingTokens.option();
			if (is_match(token.name, style))
			{
				if (m_ref->isFlag())
				{
					if (remainingTokens.has_value_delimiter())
					{
						return parse_result::error(
							{ parser_result_type::short_circuit_all,
								remainingTokens },
							"Flag option '" + token.name + "' contains value '"
								+ remainingTokens.value().name + "'.");
					}
					remainingTokens.pop(token);
					auto flagRef
						= static_cast<detail::BoundFlagRefBase *>(m_ref.get());
					auto result = flagRef->setFlag(true);
					if (!result) return parse_result(result);
					LYRA_PRINT_DEBUG(
						"(=)", get_usage_text(style), "==", token.name);
					if (result.value() == parser_result_type::short_circuit_all)
						return parse_result::ok(detail::parse_state(
							result.value(), remainingTokens));
				}
				else
				{
					auto const & argToken = remainingTokens.value();
					if (argToken.type == detail::token_type::unknown)
						return parse_result::error(
							{ parser_result_type::no_match, remainingTokens },
							"Expected argument following " + token.name);
					remainingTokens.pop(token, argToken);
					auto valueRef
						= static_cast<detail::BoundValueRefBase *>(m_ref.get());
					if (value_choices)
					{
						auto choice_result
							= value_choices->contains_value(argToken.name);
						if (!choice_result) return parse_result(choice_result);
					}
					auto result = valueRef->setValue(argToken.name);
					if (!result)
					{
						return parse_result::error(
							{ parser_result_type::short_circuit_all,
								remainingTokens },
							result.message());
					}
					LYRA_PRINT_DEBUG("(=)", get_usage_text(style),
						"==", token.name, argToken.name);
					if (result.value() == parser_result_type::short_circuit_all)
						return parse_result::ok(detail::parse_state(
							result.value(), remainingTokens));
				}
				return parse_result::ok(detail::parse_state(
					parser_result_type::matched, remainingTokens));
			}
			LYRA_PRINT_DEBUG("(!)", get_usage_text(style), "!= ", token.name);
		}
		else
		{
			LYRA_PRINT_DEBUG("(!)", get_usage_text(style),
				"!=", remainingTokens.argument().name);
		}

		return parse_result::ok(
			detail::parse_state(parser_result_type::no_match, remainingTokens));
	}

	result validate() const override
	{
		if (opt_names.empty())
			return result::error("No options supplied to opt");
		if (m_ref->isFlag() && value_choices)
			return result::error("Flag options cannot contain choices.");
		for (auto const & name : opt_names)
		{
			if (name.empty())
				return result::error("Option name cannot be empty");
			if (name[0] != '-')
				return result::error("Option name must begin with '-'");
		}
		return bound_parser::validate();
	}

	virtual std::unique_ptr<parser> clone() const override
	{
		return make_clone<opt>(this);
	}

	protected:
	std::vector<std::string> opt_names;

	bool is_match(
		std::string const & opt_name, const option_style & style) const
	{
		auto opt_normalized = normalise_opt(opt_name, style);
		for (auto const & name : opt_names)
		{
			if (normalise_opt(name, style) == opt_normalized) return true;
		}
		return false;
	}

	std::string normalise_opt(
		std::string const & opt_name, const option_style & style) const
	{
		if (detail::token_iterator::is_prefixed(
				style.short_option_prefix, style.short_option_size, opt_name))
			return "-" + opt_name.substr(style.short_option_size);

		if (detail::token_iterator::is_prefixed(
				style.long_option_prefix, style.long_option_size, opt_name))
			return "--" + opt_name.substr(style.long_option_size);

		return opt_name;
	}

	std::string format_opt(
		std::string const & opt_name, const option_style & style) const
	{
		if (opt_name[0] == '-' && opt_name[1] == '-')
			return style.long_option_string() + opt_name.substr(2);
		else if (opt_name[0] == '-')
			return style.short_option_string() + opt_name.substr(1);
		else
			return opt_name;
	}
};

/* tag::reference[]

[#lyra_opt_ctor]
== Construction

end::reference[] */

/* tag::reference[]

[#lyra_opt_ctor_flags]
=== Flags

[source]
----
lyra::opt::opt(bool& ref);

template <typename L>
lyra::opt::opt(L const& ref);
----

Constructs a flag option with a target `bool` to indicate if the flag is
present. The first form takes a reference to a variable to receive the
`bool`. The second takes a callback that is called with `true` when the
option is present.

end::reference[] */
inline opt::opt(bool & ref)
	: bound_parser(std::make_shared<detail::BoundFlagRef>(ref))
{}
template <typename L>
opt::opt(L const & ref,
	typename std::enable_if<detail::is_invocable<L>::value,
		opt::ctor_lambda_e>::type)
	: bound_parser(std::make_shared<detail::BoundFlagLambda<L>>(ref))
{}

/* tag::reference[]

[#lyra_opt_ctor_values]
=== Values

[source]
----
template <typename T>
lyra::opt::opt(T& ref, std::string const& hint);

template <typename L>
lyra::opt::opt(L const& ref, std::string const& hint)
----

Constructs a value option with a target `ref`. The first form takes a reference
to a variable to receive the value. The second takes a callback that is called
with the value when the option is present.

end::reference[] */
template <typename T>
opt::opt(T & ref,
	std::string const & hint,
	typename std::enable_if<!detail::is_invocable<T>::value,
		opt::ctor_ref_e>::type)
	: bound_parser(ref, hint)
{}
template <typename L>
opt::opt(L const & ref,
	std::string const & hint,
	typename std::enable_if<detail::is_invocable<L>::value,
		opt::ctor_lambda_e>::type)
	: bound_parser(ref, hint)
{}

/* tag::reference[]

[#lyra_opt_specification]
== Specification

end::reference[] */

/* tag::reference[]

[#lyra_opt_name]
=== `lyra::opt::name`

[source]
----
lyra::opt& lyra::opt::name(const std::string &opt_name)
lyra::opt& lyra::opt::operator[](const std::string &opt_name)
----

Add a spelling for the option of the form `--<name>` or `-n`.
One can add multiple short spellings at once with `-abc`.

end::reference[] */
inline opt & opt::name(const std::string & opt_name)
{
	if (opt_name.size() > 2 && opt_name[0] == '-' && opt_name[1] != '-')
		for (auto o : opt_name.substr(1))
			opt_names.push_back(std::string(1, opt_name[0]) + o);
	else
		opt_names.push_back(opt_name);
	return *this;
}
inline opt & opt::operator[](const std::string & opt_name)
{
	return this->name(opt_name);
}

} // namespace lyra

#endif

namespace lyra {

/* tag::reference[]

[#lyra_help]
= `lyra::help`

Utility function that defines a default `--help` option. You can specify a
`bool` flag to indicate if the help option was specified and that you could
display a help message.

The option accepts `-?`, `-h`, and `--help` as allowed option names.

*/ // end::reference[]
class help : public opt
{
	public:
	help(bool & showHelpFlag)
		: opt([&](bool flag) {
			showHelpFlag = flag;
			return parser_result::ok(parser_result_type::short_circuit_all);
		})
	{
		this->description("Display usage information.")
			.optional()
			.name("-?")
			.name("-h")
			.name("--help");
	}

	help & description(const std::string & text);

	virtual std::string get_description_text(
		const option_style &) const override
	{
		return description_text;
	}

	virtual std::unique_ptr<parser> clone() const override
	{
		return make_clone<help>(this);
	}

	private:
	std::string description_text;
};

/* tag::reference[]
[source]
----
help & help::description(const std::string &text)
----

Sets the given `text` as the general description to show with the help and
usage output for CLI parser. This text is displayed between the "Usage"
and "Options, arguments" sections.

end::reference[] */
inline help & help::description(const std::string & text)
{
	description_text = text;
	return *this;
}

} // namespace lyra

#endif

#ifndef LYRA_MAIN_HPP
#define LYRA_MAIN_HPP


#include <iostream>

namespace lyra {

/* tag::reference[]

[#lyra_main]
= `lyra::main`

Encapsulates the common use case of a main program that has a help option and
has a minimal way to specify and parse options. This provides for a way to
specify options and arguments in a simple function form. It handles checking for
errors and reporting problems.

*/ // end::reference[]
class main final : protected cli
{
	bool show_help = false;

	public:
	explicit main(const std::string & text = "");

	template <typename T>
	main & operator()(const T & parser);
	template <typename T>
	main & add_argument(const T & parser);
	template <typename T>
	main & operator|=(const T & parser);

	template <typename V>
	main & operator()(
		std::initializer_list<std::string> arg_names, V && default_value);
	template <typename V>
	main & operator()(const std::string & arg_name, V && default_value);

	template <typename L>
	int operator()(const args & argv, L action);
	template <typename L>
	int operator()(int argc, const char ** argv, L action);

	using cli::operator[];

	main & style(const option_style & style)
	{
		cli::style(style);
		return *this;
	}
	main & style(option_style && style)
	{
		cli::style(style);
		return *this;
	}
};

/* tag::reference[]

[#lyra_main_ctor]
== Construction

[source]
----
main::main(const std::string & text);
----

Construct with text for description, which defaults to an empty string. The
description is specified for the help option that is added to the command line.

end::reference[] */
inline main::main(const std::string & text)
{
	this->add_argument(help(show_help).description(text));
}

/* tag::reference[]

[#lyra_main_add_argument]
== Add Argument

[source]
----
template <typename T> main & main::operator()(const T & parser)
template <typename T> main & main::add_argument(const T & parser)
template <typename T> main & main::operator|=(const T & parser)
----

Adds a parser as an argument to the command line. These forward directly to the
`lyra::cli` equivalents. The added parser can be any of the regular Lyra parsers
like `lyra::opt` or `lyra::arg`.

end::reference[] */
template <typename T>
main & main::operator()(const T & parser)
{
	cli::add_argument(parser);
	return *this;
}
template <typename T>
main & main::add_argument(const T & parser)
{
	cli::add_argument(parser);
	return *this;
}
template <typename T>
main & main::operator|=(const T & parser)
{
	cli::operator|=(parser);
	return *this;
}

/* tag::reference[]

[#lyra_main_simple_args]
== Simple Args

[source]
----
template <typename V>
main & main::operator()(
	std::initializer_list<std::string> arg_names, V && default_value)
template <typename V>
main & main::operator()(const std::string & arg_name, V && default_value)
----

Specifies, and adds, a new argument. Depending on the `arg_names` it can be
either a `lyra::opt` or `lyra::arg`. The first item in `arg_names` indicates
the type of argument created and added:

Specify either `-<name>` or `--<name>` to add a `lyra::opt`. You can specify as
many option names following the first name. A name that doesn't follow the
option syntax is considered the as the help text for the option.

Specify a non `-` prefixed name as the first item to signify a positional
`lyra::arg`.

The single `std::string` call is equivalent to specifying just the one option or
argument.

Example specifications:

|===
| `("-o", 0)` | Short `-o` option as `int` value.
| `("--opt", 0)` | Long `--opt` option as `int` value.
| `({"-o", "--opt"}, 1.0f)` | Short and long option as `float` value.
| `({"-o", "The option."}, 1.0f)` | Short option and help description as `float`
value.
| `("opt", 2)` | Positional, i.e. `lyra::arg`, argument as `int` value.
| `({"opt", "The option."}, 2)` | Positional argument and help description as
`int` value.
| `("--opt", std::vector<float>())` | Long option with as multiple
float values.
|===

end::reference[] */
template <typename V>
main & main::operator()(
	std::initializer_list<std::string> arg_names, V && default_value)
{
	auto bound_val = val(std::forward<V>(default_value));
	if ((*arg_names.begin())[0] == '-')
	{
		std::string hint = arg_names.begin()->substr(1);
		if (hint[0] == '-') hint = hint.substr(1);
		opt o(std::move(bound_val), hint);
		for (auto arg_name : arg_names)
		{
			if (arg_name[0] == '-')
				o.name(arg_name);
			else
				o.help(arg_name);
		}
		cli::add_argument(o);
	}
	else
	{
		arg a(std::move(bound_val), *arg_names.begin());
		a.optional();
		if (arg_names.size() > 2) a.help(*(arg_names.begin() + 1));
		cli::add_argument(a);
	}
	return *this;
}
template <typename V>
main & main::operator()(const std::string & arg_name, V && default_value)
{
	return (*this)({ arg_name }, std::forward<V>(default_value));
}

/* tag::reference[]

[#lyra_main_execute]
== Execute

[source]
----
template <typename L>
int main::operator()(const args & argv, L action)
template <typename L>
int main::operator()(int argc, const char ** argv, L action)
----

Executes the given action after parsing of the program input arguments. It
returns either `0` or `1` if the execution was successful or failed
respectively. The `action` is called with the `lyra::main` instance to provide
access to the parsed argument values.

end::reference[] */
template <typename L>
int main::operator()(const args & argv, L action)
{
	auto result = cli::parse(argv);
	if (!result) std::cerr << result.message() << "\n\n";
	if (show_help || !result)
		std::cout << *this << "\n";
	else
		return action(*this);
	return result ? 0 : 1;
}
template <typename L>
int main::operator()(int argc, const char ** argv, L action)
{
	return (*this)({ argc, argv }, action);
}

} // namespace lyra

#endif

#endif // LYRA_HPP_INCLUDED
