#include <lac/completion/variable_at_pos.h>
#include <lac/parser/parser.h>
#ifdef WITH_TESTS
#include <doctest/doctest.h>
#endif
#include <cctype>

namespace
{
	bool isName(int ch)
	{
		return std::isalnum(ch) || ch == '_';
	}
} // namespace

namespace lac::comp
{
	std::string_view extractVariableAtPos(std::string_view view, size_t pos)
	{
		if (pos == std::string_view::npos)
			pos = view.size() - 1;

		if (pos >= view.size()
			|| !(isName(view[pos])
				 || view[pos] == ')'
				 || view[pos] == ']'))
			return {};

		auto pe = pos, ps = pos;
		bool prevIsName = false;

		// Go left until we it a chracter that is not alpha numeric nor underscore
		auto goToNameStart = [&ps, &view, &prevIsName] {
			while (ps != 0 && isName(view[ps - 1]))
			{
				--ps;
				prevIsName = true;
			}
		};

		// Go left while the character is a whitespace
		auto ignoreWhiteSpace = [&ps, &view] {
			while (ps != 0 && std::isspace(view[ps - 1]))
				--ps;
		};

		// Go left until we encounter the opening parenthesis
		auto ignoreFunctionCall = [&ps, &view, &prevIsName] {
			int nbParen = 0;
			while (ps != 0)
			{
				--ps;

				// TODO: test if the parenthesis is inside a litteral string
				if (view[ps] == ')')
				{
					++nbParen;
					prevIsName = false;
				}
				else if (view[ps] == '(')
				{
					--nbParen;
					if (!nbParen)
						break;
				}
			}
		};

		// Go left until we encounter the opening bracket
		auto ignoreBrackets = [&ps, &view, &prevIsName] {
			int nbBrackets = 0;
			while (ps != 0)
			{
				--ps;

				// TODO: test if the bracket is inside a litteral string
				if (view[ps] == ']')
				{
					++nbBrackets;
					prevIsName = false;
				}
				else if (view[ps] == '[')
				{
					--nbBrackets;
					if (!nbBrackets)
						break;
				}
			}
		};

		

		if (isName(view[pos]))
		{
			// Go to the end of the name
			while (pe < view.size() && isName(view[pe]))
				++pe;

			// Go to the start of the name
			goToNameStart();

			// Cannot go to the left
			if (ps < 2)
				return view.substr(ps, pe - ps);
		}
		else if (view[pos] == ')')
		{
			ps = pe = pos + 1;
			ignoreFunctionCall();
			ignoreWhiteSpace();
			goToNameStart();
			pos = ps;
		}
		else if (view[pos] == ']')
		{
			ps = pe = pos + 1;
			ignoreBrackets();
			ignoreWhiteSpace();
			goToNameStart();
			pos = ps;
		}

		// Test the next character to the left and see if we must continue
		while (pos > 1)
		{
			pos = ps;
			ignoreWhiteSpace();

			auto c = view[ps - 1];
			if (c == '.' || c == ':')
			{
				--ps;
				ignoreWhiteSpace();

				if (view[ps - 1] == ')')
					ignoreFunctionCall();
				else if (view[ps - 1] == ']')
					ignoreBrackets();

				ignoreWhiteSpace();
				goToNameStart();
				pos = ps;
			}
			else if (!prevIsName && c == ')')
			{
				ignoreFunctionCall();
				ignoreWhiteSpace();
				goToNameStart();
				pos = ps;
			}
			else if (!prevIsName && c == ']')
			{
				ignoreBrackets();
				ignoreWhiteSpace();
				goToNameStart();
				pos = ps;
			}
			else
				break;
		}

		// TODO: support bracketed expressions

		return view.substr(pos, pe - pos);
	}

	boost::optional<ast::VariableOrFunction> parseVariableAtPos(std::string_view view, size_t pos)
	{
		auto extracted = extractVariableAtPos(view, pos);
		if (extracted.empty())
			return {};

		auto ret = parser::parseVariable(extracted);
		if (ret.parsed)
			return ret.variable;
		return {};
	}
#ifdef WITH_TESTS
	TEST_SUITE_BEGIN("Parse current line");

	TEST_CASE("Name only")
	{
		CHECK(extractVariableAtPos("foobar", 0) == "foobar");
		CHECK(extractVariableAtPos("foobar", 3) == "foobar");
		CHECK(extractVariableAtPos("foobar", 5) == "foobar");
		CHECK(extractVariableAtPos("foobar") == "foobar");
		CHECK(extractVariableAtPos("(foobar)", 3) == "foobar");
		CHECK(extractVariableAtPos("test[\"foobar\"]", 8) == "foobar");
		CHECK(extractVariableAtPos("foobar test", 5) == "foobar");
		CHECK(extractVariableAtPos("foobar test", 6) == "");
		CHECK(extractVariableAtPos("foobar test", 7) == "test");
		CHECK(extractVariableAtPos("foobar test") == "test");
		CHECK(extractVariableAtPos("foobar test", 11) == "");
		CHECK(extractVariableAtPos("foo.bar", 2) == "foo");
		CHECK(extractVariableAtPos("foo.bar test", 10) == "test");
		CHECK(extractVariableAtPos("first.second.third", 4) == "first");
	}

	TEST_CASE("Parse name")
	{
		auto var = parseVariableAtPos("foobar test", 5);
		REQUIRE(var.has_value());
		REQUIRE(var->start.get().type() == typeid(ast::Variable));
		const auto& variable = boost::get<ast::Variable>(var->start);
		REQUIRE(variable.start.get().type() == typeid(std::string));
		CHECK(boost::get<std::string>(variable.start) == "foobar");
	}

	TEST_CASE("Member variable")
	{
		CHECK(extractVariableAtPos("foo.bar", 5) == "foo.bar");
		CHECK(extractVariableAtPos("foo.bar") == "foo.bar");
		CHECK(extractVariableAtPos("foo.bar test", 5) == "foo.bar");
		CHECK(extractVariableAtPos("foo . bar test", 7) == "foo . bar");
		CHECK(extractVariableAtPos("test foo.bar", 10) == "foo.bar");
		CHECK(extractVariableAtPos("first.second.third", 8) == "first.second");
		CHECK(extractVariableAtPos("first.second.third", 15) == "first.second.third");
		CHECK(extractVariableAtPos("first.second.third") == "first.second.third");

		CHECK(extractVariableAtPos("foo.bar test", 10) == "test");
	}

	TEST_CASE("Parse member variable")
	{
		auto var = parseVariableAtPos("first.second.third", 15);
		REQUIRE(var.has_value());
		REQUIRE(var->start.get().type() == typeid(ast::Variable));
		const auto& variable = boost::get<ast::Variable>(var->start);
		REQUIRE(variable.start.get().type() == typeid(std::string));
		CHECK(boost::get<std::string>(variable.start) == "first");
		REQUIRE(variable.rest.size() == 2);
		REQUIRE(variable.rest[0].get().type() == typeid(ast::TableIndexName));
		CHECK(boost::get<ast::TableIndexName>(variable.rest[0]).name == "second");
		REQUIRE(variable.rest[1].get().type() == typeid(ast::TableIndexName));
		CHECK(boost::get<ast::TableIndexName>(variable.rest[1]).name == "third");
	}

	TEST_CASE("Member function")
	{
		CHECK(extractVariableAtPos("foo:bar", 5) == "foo:bar");
		CHECK(extractVariableAtPos("foo:bar test", 5) == "foo:bar");
		CHECK(extractVariableAtPos("foo : bar test", 7) == "foo : bar");
		CHECK(extractVariableAtPos("test foo:bar", 10) == "foo:bar");
		CHECK(extractVariableAtPos("first.second:third", 15) == "first.second:third");

		CHECK(extractVariableAtPos("foo:bar test", 10) == "test");
	}

	TEST_CASE("Parse member function")
	{
		auto var = parseVariableAtPos("first.second:third", 15);
		REQUIRE(var.has_value());
		REQUIRE(var->start.get().type() == typeid(ast::Variable));
		const auto& variable = boost::get<ast::Variable>(var->start);
		REQUIRE(variable.start.get().type() == typeid(std::string));
		CHECK(boost::get<std::string>(variable.start) == "first");
		REQUIRE(variable.rest.size() == 1);
		REQUIRE(variable.rest[0].get().type() == typeid(ast::TableIndexName));
		CHECK(boost::get<ast::TableIndexName>(variable.rest[0]).name == "second");
		REQUIRE(var->member.has_value());
		CHECK(var->member->name == "third");
	}

	TEST_CASE("With function call")
	{
		CHECK(extractVariableAtPos("foo()", 4) == "foo()");
		CHECK(extractVariableAtPos("test foo()", 9) == "foo()");
		CHECK(extractVariableAtPos("foo().bar", 8) == "foo().bar");
		CHECK(extractVariableAtPos("test foo().bar", 12) == "foo().bar");
		CHECK(extractVariableAtPos("foo ( ) . bar", 11) == "foo ( ) . bar");
		CHECK(extractVariableAtPos("foo(a, 42, false).bar", 20) == "foo(a, 42, false).bar");
		CHECK(extractVariableAtPos("foo('anything here').bar", 23) == "foo('anything here').bar");
		CHECK(extractVariableAtPos("foo().bar.test", 8) == "foo().bar");
		CHECK(extractVariableAtPos("foo():bar.test", 8) == "foo():bar");
		CHECK(extractVariableAtPos("foo().bar.test", 12) == "foo().bar.test");
		CHECK(extractVariableAtPos("foo():bar.test", 12) == "foo():bar.test");
		CHECK(extractVariableAtPos("foo.bar().test", 12) == "foo.bar().test");
		CHECK(extractVariableAtPos("foo():bar().test", 14) == "foo():bar().test");
		CHECK(extractVariableAtPos("first():second():third():fourth", 12) == "first():second");
		CHECK(extractVariableAtPos("first():second():third():fourth", 20) == "first():second():third");
		CHECK(extractVariableAtPos("first():second():third():fourth", 30) == "first():second():third():fourth");

		CHECK(extractVariableAtPos("f() test", 5) == "test");
	}

	TEST_CASE("With array index")
	{
		CHECK(extractVariableAtPos("foo[x]", 5) == "foo[x]");
		CHECK(extractVariableAtPos("test foo[x]", 10) == "foo[x]");
		CHECK(extractVariableAtPos("foo[x].bar", 9) == "foo[x].bar");
		CHECK(extractVariableAtPos("test foo[x].bar", 13) == "foo[x].bar");
		CHECK(extractVariableAtPos("foo [ x ] . bar", 13) == "foo [ x ] . bar");
		CHECK(extractVariableAtPos("foo['anything here'].bar", 23) == "foo['anything here'].bar");
		CHECK(extractVariableAtPos("foo[x].bar.test", 9) == "foo[x].bar");
		CHECK(extractVariableAtPos("foo[x]:bar.test", 9) == "foo[x]:bar");
		CHECK(extractVariableAtPos("foo[x].bar.test", 13) == "foo[x].bar.test");
		CHECK(extractVariableAtPos("foo[x]:bar.test", 13) == "foo[x]:bar.test");
		CHECK(extractVariableAtPos("foo.bar[x].test", 13) == "foo.bar[x].test");
		CHECK(extractVariableAtPos("foo[x]:bar[42].test", 16) == "foo[x]:bar[42].test");

		CHECK(extractVariableAtPos("foo[x] test", 10) == "test");

	}

	TEST_CASE("Function calls and array index")
	{
		CHECK(extractVariableAtPos("foo()[x]", 7) == "foo()[x]");
		CHECK(extractVariableAtPos("test foo()[x]", 12) == "foo()[x]");
		CHECK(extractVariableAtPos("foo(a,b)[x]", 10) == "foo(a,b)[x]");
		CHECK(extractVariableAtPos("test foo(a,b)[x]", 15) == "foo(a,b)[x]");
		CHECK(extractVariableAtPos("test foo(a).m[x]", 15) == "foo(a).m[x]");
	}

	TEST_SUITE_END();
#endif
} // namespace lac::comp
