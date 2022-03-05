#include <lac/parser/ast_adapted.h>
#include <lac/parser/chunk.h>

#include <lac/helper/test_utils.h>

namespace
{
	template <class P, class A>
	bool phrase_parser_elements(std::string_view input, const P& p, A& arg, lac::pos::Elements& elts)
	{
		auto f = input.begin();
		const auto l = input.end();
		lac::pos::Positions positions{f, l};
		const auto parser = boost::spirit::x3::with<lac::pos::position_tag>(std::ref(positions))[p];
		if (boost::spirit::x3::phrase_parse(f, l, parser, boost::spirit::x3::ascii::space, arg) && f == l)
		{
			elts = positions.elements();
			return true;
		}
		return false;
	}
} // namespace

namespace lac
{
#ifdef WITH_TESTS
	using helper::test_phrase_parser;

	TEST_CASE("Positions")
	{
		auto chunk = parser::chunkRule();

		ast::Block block;
		REQUIRE(test_phrase_parser("testVar = 'hello' .. 42", chunk, block));
		REQUIRE(block.statements.size() == 1);
		const auto& statement = block.statements[0];
		REQUIRE(statement.get().type() == typeid(ast::AssignmentStatement));
		const auto& assignment = boost::get<ast::AssignmentStatement>(statement);
		REQUIRE(assignment.variables.size() == 1);
		const auto& var = assignment.variables[0];
		CHECK(var.begin == 0);
		CHECK(var.end == 7);
	}

	TEST_CASE("Elements")
	{
		auto chunk = parser::chunkRule();

		ast::Block block;
		pos::Elements elements;
		REQUIRE(phrase_parser_elements("testVar = 'hello' .. 42", chunk, block, elements));
		REQUIRE(elements.size() == 3);

		const auto var = elements[0];
		CHECK(var.type == ast::ElementType::variable);
		CHECK(var.begin == 0);
		CHECK(var.end == 7);

		const auto str = elements[1];
		CHECK(str.type == ast::ElementType::literal_string);
		CHECK(str.begin == 10);
		CHECK(str.end == 17);

		const auto num = elements[2];
		CHECK(num.type == ast::ElementType::numeral);
		CHECK(num.begin == 21);
		CHECK(num.end == 23);
	}

	TEST_CASE("Elements 2")
	{
		auto chunk = parser::chunkRule();

		ast::Block block;
		pos::Elements elements;
		REQUIRE(phrase_parser_elements("goto toto", chunk, block, elements));
		REQUIRE(elements.size() == 1);

		const auto var = elements[0];
		CHECK(var.type == ast::ElementType::keyword);
		CHECK(var.begin == 0);
		CHECK(var.end == 4);
	}
#endif
} // namespace lac
