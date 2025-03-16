#include <lac/analysis/analyze_block.h>
#include <lac/analysis/scope.h>
#include <lac/analysis/get_type.h>
#include <lac/analysis/user_defined.h>

#include <lac/parser/ast.h>

namespace lac::an
{
	class AnalysisVisitor : public boost::static_visitor<void>
	{
	public:
		AnalysisVisitor(Scope& scope)
			: m_scope(scope)
		{
		}

		void operator()(const ast::ExpressionConstant&) const
		{
			// Nothing to do here
		}

		void operator()(const ast::Numeral&) const
		{
			// Nothing to do here
		}

		void operator()(const ast::LiteralString&) const
		{
			// Nothing to do here
		}

		void operator()(const std::string&) const
		{
			// Nothing to do here
		}

		void operator()(const ast::UnaryOperation& uo) const
		{
			(*this)(uo.expression);
		}

		void operator()(const ast::BinaryOperation& bo) const
		{
			(*this)(bo.expression);
		}

		void operator()(const ast::FieldByExpression& f) const
		{
			(*this)(f.key);
			(*this)(f.value);
		}

		void operator()(const ast::FieldByAssignment& f) const
		{
			(*this)(f.value);
		}

		void operator()(const ast::Field& f) const
		{
			boost::apply_visitor(*this, f);
		}

		void operator()(const ast::TableConstructor& tc) const
		{
			if (tc.fields)
			{
				for (const auto& f : *tc.fields)
					(*this)(f);
			}
		}

		void operator()(const ast::FunctionBody& fb, const std::string& functionName = {}) const
		{
			Scope scope{fb.block, &m_scope};
			bool isScriptInput = false;
			if (fb.parameters)
			{
				// Test if the function has a defined signature
				if (!functionName.empty())
				{
					const auto userDefined = m_scope.getUserDefined();
					if (userDefined)
					{
						const auto funcType = userDefined->getScriptInput(functionName);
						if (funcType)
						{
							isScriptInput = true;

							// We want to use the given parameter names with the types previously defined
							const auto& inputParams = funcType->function.parameters;
							const auto nbInParams = inputParams.size();
							const auto& funcParams = fb.parameters->parameters;
							const auto nbFuncParams = funcParams.size();
							for (size_t i = 0; i < nbFuncParams; ++i)
							{
								scope.addVariable(funcParams[i],
												  i >= nbInParams
													  ? Type::unknown
													  : inputParams[i].type());
							}
						}
					}
				}

				// We do not know the parameter types
				if (!isScriptInput)
				{
					for (const auto& p : fb.parameters->parameters)
						scope.addVariable(p, Type::unknown);

					if (!functionName.empty())
						scope.addVariable(functionName, m_scope.getVariableType(functionName)); // The function can be called recursively
				}
			}

			analyseBlock(scope, fb.block);
			m_scope.addChildScope(std::move(scope));
		}

		void operator()(const ast::Operand& op) const
		{
			boost::apply_visitor(*this, op);
		}

		void operator()(const ast::Expression& ex) const
		{
			(*this)(ex.operand);
			if (ex.binaryOperation)
				(*this)(*ex.binaryOperation);
		}

		void operator()(const ast::ExpressionsList& el) const
		{
			for (const auto& ex : el)
				(*this)(ex);
		}

		void operator()(const ast::BracketedExpression be) const
		{
			(*this)(be.expression);
		}

		void operator()(const ast::TableIndexExpression tie) const
		{
			(*this)(tie.expression);
		}

		void operator()(const ast::TableIndexName&) const
		{
			// Nothing to do here
		}

		void operator()(const ast::EmptyArguments&) const
		{
			// Nothing to do here
		}

		void operator()(const ast::FunctionCallEnd& fce) const
		{
			boost::apply_visitor(*this, fce.arguments);
		}

		void operator()(const ast::PrefixExpression& pe) const
		{
			boost::apply_visitor(*this, pe.start);
			for (const auto& pp : pe.rest)
				boost::apply_visitor(*this, pp);
		}

		void operator()(const ast::VariableFunctionCall& vfc) const
		{
			(*this)(vfc.functionCall);
			boost::apply_visitor(*this, vfc.postVariable);
		}

		void operator()(const ast::Variable& v) const
		{
			boost::apply_visitor(*this, v.start);
			for (const auto& vp : v.rest)
				boost::apply_visitor(*this, vp);
		}

		void operator()(const ast::FunctionCallPostfix& fcp) const
		{
			for (const auto& ti : fcp.tableIndex)
				boost::apply_visitor(*this, ti);
			(*this)(fcp.functionCall);
		}

		void operator()(const ast::FunctionCall& fc) const
		{
			boost::apply_visitor(*this, fc.start);
			for (const auto& r : fc.rest)
				(*this)(r);
		}

		void operator()(const ast::ReturnStatement& rs) const
		{
			for (const auto& ex : rs.expressions)
				(*this)(ex);
		}

		void operator()(const ast::EmptyStatement&) const
		{
			// Nothing to do here
		}

		void operator()(const ast::AssignmentStatement& as) const
		{
			size_t nbV = as.variables.size(), nbE = as.expressions.size();
			auto varIt = as.variables.begin();
			for (size_t i = 0; i < nbV; ++i)
			{
				TypeInfo type;
				// TODO: support expressions with multiple returns
				if (i < nbE)
					type = getType(m_scope, as.expressions[i]);

				auto& var = *varIt++;

				if (var.start.get().type() == typeid(std::string))
				{
					const auto& varName = boost::get<std::string>(var.start.get());
					// Named variables
					if (var.rest.empty())
						m_scope.addVariable(varName, type);
					else
					{
						auto& tableType = m_scope.modifyTable(varName);
						auto* memberType = &tableType;

						// Table member
						for (const auto& restIt : var.rest)
						{
							const auto& memberExp = restIt.get();
							const auto& memberExpType = memberExp.type();
							if (memberExpType == typeid(ast::TableIndexName))
							{
								const auto& memberName = boost::get<ast::TableIndexName>(memberExp).name;
								memberType = &memberType->members[memberName];
							}
							else // TODO: TableIndexExpression & VariableFunctionCall
							{
								memberType = nullptr;
								break;
							}
						}

						if (memberType)
							*memberType = type;
					}
				}

				// TODO: BracketedExpression
			}

			// Visit expressions, add child scopes
			for (const auto& e : as.expressions)
				(*this)(e);
		}

		void operator()(const ast::LabelStatement& ls) const
		{
			m_scope.addLabel(ls.name);
		}

		void operator()(const ast::GotoStatement&) const
		{
		}

		void operator()(const ast::BreakStatement&) const
		{
			// Nothing to do here
		}

		void operator()(const ast::DoStatement& ds) const
		{
			m_scope.addChildScope(analyseBlock(ds.block, &m_scope));
		}

		void operator()(const ast::WhileStatement& ws) const
		{
			m_scope.addChildScope(analyseBlock(ws.block, &m_scope));
		}

		void operator()(const ast::RepeatStatement& rs) const
		{
			m_scope.addChildScope(analyseBlock(rs.block, &m_scope));
		}

		void operator()(const ast::IfThenElseStatement& s) const
		{
			m_scope.addChildScope(analyseBlock(s.first.block, &m_scope));
			for (const auto& es : s.rest)
				m_scope.addChildScope(analyseBlock(es.block, &m_scope));
			if (s.elseBlock)
				m_scope.addChildScope(analyseBlock(*s.elseBlock, &m_scope));
		}

		void operator()(const ast::NumericalForStatement& s) const
		{
			Scope scope{s.block, &m_scope};
			scope.addVariable(s.variable, Type::number);
			analyseBlock(scope, s.block);
			m_scope.addChildScope(std::move(scope));
		}

		void operator()(const ast::GenericForStatement& s) const
		{
			Scope scope{s.block, &m_scope};
			if (s.expressions.empty())
			{
				for (const auto& var : s.variables)
					m_scope.addVariable(var, {});
			}
			else
			{
				const auto iterType = getType(m_scope, s.expressions.front());
				size_t nbV = s.variables.size();
				if (iterType.type == Type::function)
				{
					const auto& res = iterType.function.results; 
					size_t nbR = res.size();
					for (size_t i = 0; i < nbV; ++i)
					{
						TypeInfo type;
						if (i < nbR)
							type = res[i];
						m_scope.addVariable(s.variables[i], type);
					}
				}
				else
				{
					for (const auto& var : s.variables)
						m_scope.addVariable(var, {});
				}
			}

			analyseBlock(scope, s.block);
			m_scope.addChildScope(std::move(scope));
		}

		void operator()(const ast::FunctionDeclarationStatement& s) const
		{
			auto funcType = getType(m_scope, s.body);

			// Global function
			if (s.name.rest.empty() && !s.name.member)
			{
				m_scope.addVariable(s.name.start, std::move(funcType));
				(*this)(s.body, s.name.start);
				return;
			}

			// Declaration of a table function or method
			auto& tableType = m_scope.modifyTable(s.name.start);
			auto* memberType = &tableType;

			// Table member
			for (const auto& r : s.name.rest)
				memberType = &memberType->members[r];
			
			if (memberType)
			{
				if (s.name.member)
				{
					funcType.function.isMethod = true;
					memberType = &memberType->members[s.name.member->name];
				}

				*memberType = funcType;
			}
			
			(*this)(s.body); // Visit the body scope
		}

		void operator()(const ast::LocalFunctionDeclarationStatement& s) const
		{
			auto funcType = getType(m_scope, s.body);
			m_scope.addVariable(s.name, std::move(funcType));
			(*this)(s.body, s.name);
		}

		void operator()(const ast::LocalAssignmentStatement& s) const
		{
			if (!s.expressions)
			{
				for (const auto& v : s.variables)
					m_scope.addVariable(v, {Type::unknown});
			}
			else
			{
				size_t nbV = s.variables.size(), nbE = s.expressions->size();
				const auto& expressions = *s.expressions;
				for (size_t i = 0; i < nbV; ++i)
				{
					TypeInfo type;
					// TODO: support expressions with multiple returns
					if (i < nbE)
						type = getType(m_scope, expressions[i]);
					m_scope.addVariable(s.variables[i], type);
				}

				// Visit expressions, add child scopes
				for (const auto& e : *s.expressions)
					(*this)(e);
			}
		}

		void operator()(const ast::Block& b) const
		{
			for (const auto& s : b.statements)
				boost::apply_visitor(*this, s);
			if (b.returnStatement)
				(*this)(*b.returnStatement);
		}

	private:
		Scope& m_scope;
	};

	void analyseBlock(Scope& scope, const ast::Block& block)
	{
		AnalysisVisitor{scope}(block);
	}

	Scope analyseBlock(const ast::Block& block, Scope* parentScope)
	{
		Scope scope(block, parentScope);
		analyseBlock(scope, block);
		return scope;
	}
} // namespace lac::an
