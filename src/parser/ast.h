#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4521) // multiple copy constructors specified
#endif

#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/optional.hpp>

#include <iostream>
#include <string>

namespace lac
{
	namespace ast
	{
		enum class Operation
		{
			add,    // Addition (+)
			sub,    // Substraction (-)
			mul,    // Multiplication (*)
			div,    // Division (/)
			idiv,   // Floor division (//)
			mod,    // Modulo (%)
			pow,    // Exponentiation (^)
			unm,    // Unary negation (-)
			band,   // Bitwise AND (&)
			bor,    // Bitwise OR (|)
			bxor,   // Bitwise exclusive OR (~)
			bnot,   // Bitwise NOT (~)
			shl,    // Bitwise left shift (<<)
			shr,    // Bitwise right shift (>>)
			concat, // Concatenation (..)
			len,    // Length (#)
			lt,     // Less than (<)
			le,     // Less equal (<=)
			gt,     // Greater than (>)
			ge,     // Greater equal (>=)
			eq,     // Equal (==)
			ineq    // Inequal (~=)
		};

		enum class ExpressionConstant
		{
			nil,
			dots,
			False,
			True
		};

		struct UnaryOperation;
		using f_UnaryOperation = boost::spirit::x3::forward_ast<UnaryOperation>;

		struct Operand
			: boost::spirit::x3::variant<
				  ExpressionConstant,
				  double,
				  std::string,
				  f_UnaryOperation>
		{
			using base_type::base_type;
			using base_type::operator=;
		};

		using NamesList = std::vector<std::string>;

		struct BinaryOperation;
		struct Expression
		{
			Operand first;
			std::list<BinaryOperation> rest;
		};

		using ExpressionsList = std::list<Expression>;

		struct UnaryOperation
		{
			Operation operation;
			Expression expression;
		};

		struct BinaryOperation
		{
			Operation operation;
			Expression expression;
		};

		struct FieldByExpression
		{
			Expression key, value;
		};

		struct FieldByAssignement
		{
			std::string name;
			Expression value;
		};

		struct Field
			: boost::spirit::x3::variant<
				  FieldByExpression,
				  FieldByAssignement,
				  Expression>
		{
			using base_type::base_type;
			using base_type::operator=;
		};

		using FieldsList = std::list<Field>;

		struct TableConstructor
		{
			boost::optional<FieldsList> fields;
		};

		struct BracketedExpression
		{
			Expression expression;
		};

		struct TableIndexExpression
		{
			Expression expression;
		};

		struct TableIndexName
		{
			std::string name;
		};

		struct ParametersList
		{
			NamesList parameters;
			bool varargs = false;
		};

		struct Statement
		{
		};

		struct Block
		{
			std::string tmp;
			//	std::list<Statement> statements;
			//	std::list<Expression> returnStatement;
		};

		struct FunctionBody
		{
			boost::optional<ParametersList> parameters;
			Block block;
		};
	} // namespace ast
} // namespace lac

#ifdef _MSC_VER
#pragma warning(pop)
#endif
