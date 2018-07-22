/*
 * ERMInterpreter.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "ERMInterpreter.h"

#include <cctype>
#include "../../lib/mapObjects/CObjectHandler.h"
#include "../../lib/mapObjects/MapObjects.h"
#include "../../lib/NetPacks.h"
#include "../../lib/CHeroHandler.h"
#include "../../lib/CCreatureHandler.h"
#include "../../lib/VCMIDirs.h"
#include "../../lib/IGameCallback.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/MiscObjects.h"

namespace spirit = boost::spirit;
using ::scripting::ContextBase;
using namespace ::VERMInterpreter;

typedef int TUnusedType;

namespace ERMConverter
{
	//console printer
	using namespace ERM;

	enum class EDir{GET, SET};

	struct LVL2Iexp : boost::static_visitor<std::string>
	{
		EDir dir;

		LVL2Iexp(EDir dir_)
			: dir(dir_)
		{}

		std::string processNotMacro(TVarExpNotMacro const & val) const
		{
			if(val.questionMark.is_initialized())
				throw EIexpProblem("Question marks ('?') are not allowed in getter i-expressions");

			//TODO:

			if(val.val.is_initialized())
			{
				return boost::to_string(boost::format("%s[%d]") % val.varsym % val.val.get());
			}
			else
			{
				return val.varsym;
			}
		}

		std::string operator()(TVarExpNotMacro const & val) const
		{
			return processNotMacro(val);
		}

		std::string operator()(TMacroUsage const & val) const
		{
			return val.macro;
		}
	};

	struct LVL1Iexp : boost::static_visitor<std::string>
	{
		EDir dir;

		LVL1Iexp(EDir dir_)
			: dir(dir_)
		{}

		LVL1Iexp()
			: dir(EDir::GET)
		{}

		std::string operator()(int const & constant) const
		{
			if(dir == EDir::GET)
			{
				return boost::lexical_cast<std::string>(constant);
			}
			else
			{
				throw EIexpProblem("Cannot set a constant!");
			}
		}

		std::string operator()(TVarExp const & var) const
		{
			return boost::apply_visitor(LVL2Iexp(dir), var);
		}
	};

	struct Condition : public boost::static_visitor<std::string>
	{
		Condition()
		{}

		std::string operator()(TComparison const & cmp) const
		{
			std::string lhs = boost::apply_visitor(LVL1Iexp(), cmp.lhs);
			std::string rhs = boost::apply_visitor(LVL1Iexp(), cmp.rhs);

			static const std::map<std::string, std::string> OPERATION =
			{
				{"<", "<"},
				{">", ">"},
				{">=", ">="},
				{"=>", ">="},
				{"<=", "<="},
				{"=<", "<="},
				{"==", "=="},
				{"<>", "~="},
				{"><", "~="},
			};

			auto sign = OPERATION.find(cmp.compSign);
			if(sign == std::end(OPERATION))
				throw EScriptExecError(std::string("Wrong comparison sign: ") + cmp.compSign);

			boost::format fmt("(%s %s %s)");
			fmt % lhs % sign->second % rhs;
			return fmt.str();
		}
		std::string operator()(int const & flag) const
		{
			return boost::to_string(boost::format("ERM.flag[%d]") % flag);
		}
	};

	struct ParamIO
	{
		std::string name;
		bool isInput;
	};

	struct Converter : public boost::static_visitor<>
	{
		mutable std::ostream * out;
		Converter(std::ostream * out_)
			: out(out_)
		{}
	};

	struct GetBodyOption : public boost::static_visitor<std::string>
	{
		GetBodyOption()
		{}

		virtual std::string operator()(TVarConcatString const & cmp) const
		{
			throw EScriptExecError("String concatenation not allowed in this receiver");
		}
		virtual std::string operator()(TStringConstant const & cmp) const
		{
			throw EScriptExecError("String constant not allowed in this receiver");
		}
		virtual std::string operator()(TCurriedString const & cmp) const
		{
			throw EScriptExecError("Curried string not allowed in this receiver");
		}
		virtual std::string operator()(TSemiCompare const & cmp) const
		{
			throw EScriptExecError("Semi comparison not allowed in this receiver");
		}
	// 	virtual void operator()(TMacroUsage const& cmp) const
	// 	{
	// 		throw EScriptExecError("Macro usage not allowed in this receiver");
	// 	}
		virtual std::string operator()(TMacroDef const & cmp) const
		{
			throw EScriptExecError("Macro definition not allowed in this receiver");
		}
		virtual std::string operator()(TIexp const & cmp) const
		{
			throw EScriptExecError("i-expression not allowed in this receiver");
		}
		virtual std::string operator()(TVarpExp const & cmp) const
		{
			throw EScriptExecError("Varp expression not allowed in this receiver");
		}
		virtual std::string operator()(spirit::unused_type const & cmp) const
		{
			throw EScriptExecError("\'Nothing\' not allowed in this receiver");
		}
	};

	struct BodyOption : public boost::static_visitor<ParamIO>
	{
		ParamIO operator()(TVarConcatString const & cmp) const
		{
			throw EScriptExecError("String concatenation not allowed in this receiver");
		}

		ParamIO operator()(TStringConstant const & cmp) const
		{
			boost::format fmt("[===[%s]===]");
			fmt % cmp.str;

			ParamIO ret;
			ret.isInput = true;
			ret.name = fmt.str();
			return ret;
		}

		ParamIO operator()(TCurriedString const & cmp) const
		{
			throw EScriptExecError("Curried string not allowed in this receiver");
		}

		ParamIO operator()(TSemiCompare const & cmp) const
		{
			throw EScriptExecError("Semi comparison not allowed in this receiver");
		}

		ParamIO operator()(TMacroDef const & cmp) const
		{
			throw EScriptExecError("Macro definition not allowed in this receiver");
		}

		ParamIO operator()(TIexp const & cmp) const
		{
			ParamIO ret;
			ret.isInput = true;
			ret.name = boost::apply_visitor(LVL1Iexp(), cmp);
			return ret;
		}

		ParamIO operator()(TVarpExp const & cmp) const
		{
//			ParamIO ret;
//			ret.isInput = false;
//
//			ret.name = "";
//			return ret;
			throw EScriptExecError("Not implemented");
		}

		ParamIO operator()(spirit::unused_type const & cmp) const
		{
			throw EScriptExecError("\'Nothing\' not allowed in this receiver");
		}
	};

	struct VR_S : public GetBodyOption
	{
		VR_S()
		{}

		using GetBodyOption::operator();

		std::string operator()(TIexp const & cmp) const override
		{
			return boost::apply_visitor(LVL1Iexp(), cmp);
		}
		std::string operator()(TStringConstant const & cmp) const override
		{
			boost::format fmt("[===[%s]===]");
			fmt % cmp.str;
			return fmt.str();
		}
	};

	struct Receiver : public Converter
	{
		std::string name;
		std::vector<std::string> identifiers;

		Receiver(std::ostream * out_, std::string name_, std::vector<std::string> identifiers_)
			: Converter(out_),
			name(name_),
			identifiers(identifiers_)
		{}

		void operator()(TVRLogic const & trig) const
		{
			throw EInterpreterError("VR logic is not allowed in this receiver!");
		}

		void operator()(TVRArithmetic const & trig) const
		{
			throw EInterpreterError("VR arithmetic is not allowed in this receiver!");
		}

		void operator()(TNormalBodyOption const & trig) const
		{
			std::string params;
			std::string outParams;
			std::string inParams;

			for(auto iter = std::begin(identifiers); iter != std::end(identifiers); ++iter)
			{
				params += ", ";
				params += *iter;
			}

			{
				std::vector<ParamIO> optionParams;

				for(auto & p : trig.params)
					optionParams.push_back(boost::apply_visitor(BodyOption(), p));

				for(auto & p : optionParams)
				{
					if(p.isInput)
					{
						if(outParams.empty())
							outParams = "_";
						else
							outParams += ", _";

						inParams += ", ";
						inParams += p.name;
					}
					else
					{
						if(outParams.empty())
						{
							outParams = p.name;
						}
						else
						{
							outParams += ", ";
							outParams += p.name;
						}

						inParams += ", nil";
					}
				}
			}

			boost::format callFormat("%s = ERM.%s(x%s):%s(x%s)");

			callFormat % outParams;
			callFormat % name;
			callFormat % params;
			callFormat % trig.optionCode;
			callFormat % inParams;

			(*out) << callFormat.str() << std::endl;
		}
	};

	struct VR : public Converter
	{
		std::string var;

		VR(std::ostream * out_, std::string var_)
			: Converter(out_),
			var(var_)
		{}

		void operator()(TVRLogic const & trig) const
		{
			std::string rhs = boost::apply_visitor(LVL1Iexp(), trig.var);

			std::string opcode;

			switch (trig.opcode)
			{
			case '&':
				opcode = "bit.band";
				break;
			case '|':
				opcode = "bit.bor";
				break;
			case 'X':
				opcode = "bit.bxor";
				break;
			default:
				throw EInterpreterError("Wrong opcode in VR logic expression!");
				break;
			}

			boost::format fmt("%s = %s %s(%s, %s)");
			fmt % var % opcode % var % rhs;

			(*out) << fmt.str() << std::endl;
		}

		void operator()(TVRArithmetic const & trig) const
		{
			std::string rhs = boost::apply_visitor(LVL1Iexp(), trig.rhs);

			std::string opcode;

			switch (trig.opcode)
			{
			case '+':
			case '-':
			case '*':
			case '%':
				opcode = trig.opcode;
				break;
			case ':':
				opcode = "/";
			default:
				throw EInterpreterError("Wrong opcode in VR arithmetic!");
				break;
			}

			boost::format fmt("%s = %s %s %s");
			fmt % var %  var % opcode % rhs;
			(*out) << fmt.str() << std::endl;
		}

		void operator()(TNormalBodyOption const & trig) const
		{
			switch(trig.optionCode)
			{
			case 'C': //setting/checking v vars
				{
					//TODO
				}
				break;
			case 'H': //checking if string is empty
				{
					//TODO
				}
				break;
			case 'M': //string operations
				{
					//TODO
				}
				break;
			case 'R': //random variables
				{
					//TODO
				}
				break;
			case 'S': //setting variable
				{
					if(trig.params.size() != 1)
						throw EScriptExecError("VR:S option takes exactly 1 parameter!");

					std::string opt = boost::apply_visitor(VR_S(), trig.params[0]);

					(*out) << var  << " = " << opt << std::endl;
				}
				break;
			case 'T': //random variables
				{
					//TODO
				}
				break;
			case 'U': //search for a substring
				{
					//TODO
				}
				break;
			case 'V': //convert string to value
				{
					//TODO
				}
				break;
			default:
				throw EScriptExecError("Wrong VR receiver option!");
				break;
			}
		}
	};


	struct ERMExp : public Converter
	{
		ERMExp(std::ostream * out_)
			: Converter(out_)
		{}

		template <typename Visitor>
		void performBody(const boost::optional<ERM::Tbody> & body, const Visitor & visitor) const
		{
			if(body.is_initialized())
			{
				ERM::Tbody bo = body.get();
				for(int g=0; g<bo.size(); ++g)
				{
					boost::apply_visitor(visitor, bo[g]);
				}
			}
		}

		void convert(const std::string & name, boost::optional<Tidentifier> identifier, boost::optional<Tbody> body) const
		{
			if(name == "VR")
			{
				if(!identifier.is_initialized())
					throw EScriptExecError("VR receiver requires arguments");

				ERM::Tidentifier tid = identifier.get();
				if(tid.size() != 1)
					throw EScriptExecError("VR receiver takes exactly 1 argument");

				auto var = boost::apply_visitor(LVL1Iexp(), tid[0]);

				performBody(body, VR(out, var));

			}
			else if(name == "FU")
			{
				//TODO:
			}
			else if(name == "DO")
			{
				//TODO: use P body option
				//TODO: pass|return parameters
				if(!identifier.is_initialized())
					throw EScriptExecError("DO receiver requires arguments");

				ERM::Tidentifier tid = identifier.get();
				if(tid.size() != 4)
					throw EScriptExecError("DO receiver takes exactly 4 arguments");

				auto funNum = boost::apply_visitor(LVL1Iexp(), tid[0]);
				auto startVal = boost::apply_visitor(LVL1Iexp(), tid[1]);
				auto stopVal = boost::apply_visitor(LVL1Iexp(), tid[2]);
				auto increment = boost::apply_visitor(LVL1Iexp(), tid[3]);

				(*out) << "\t" << "for __iter = " << startVal <<", " << stopVal << "-1, " << increment << " do " << std::endl;
				(*out) << "\t\t" << "local x = x or {}" << std::endl;
				(*out) << "\t\t" << "x[16] = __iter" << std::endl;
				(*out) << "\t\t" << "FU" << funNum << "(x)" << std::endl;
				(*out) << "\t\t" << "__iter = x[16]" << std::endl;
				(*out) << "\t" << "end" << std::endl;
			}
			else
			{
				std::vector<std::string> identifiers;

				if(identifier.is_initialized())
				{
					for(const auto & id : identifier.get())
						identifiers.push_back(boost::apply_visitor(LVL1Iexp(), id));
				}

				performBody(body, Receiver(out, name, identifiers));
			}
		}

		void convertConditionInner(Tcondition const & cond, char op) const
		{
			std::string lhs = boost::apply_visitor(Condition(), cond.cond);

			if(cond.ctype != '/')
				op = cond.ctype;

			switch (op)
			{
			case '&':
				(*out) << " and ";
				break;
			case '|':
				(*out) << " or ";
				break;
			default:
				throw EInterpreterProblem(std::string("Wrong condition connection (") + cond.ctype + ") !");
				break;
			}

			(*out) << lhs;

			if(cond.rhs.is_initialized())
			{
				switch (op)
				{
				case '&':
				case '|':
					break;
				default:
					throw EInterpreterProblem(std::string("Wrong condition connection (") + cond.ctype + ") !");
					break;
				}

				convertConditionInner(cond.rhs.get().get(), op);
			}
		}

		void convertConditionOuter(Tcondition const & cond) const
		{
			//&c1/c2/c3|c4/c5/c6 -> (c1  & c2  & c3)  | c4  |  c5  | c6
			std::string lhs = boost::apply_visitor(Condition(), cond.cond);

			(*out) << lhs;

			if(cond.rhs.is_initialized())
			{
				switch (cond.ctype)
				{
				case '&':
				case '|':
					break;
				default:
					throw EInterpreterProblem(std::string("Wrong condition connection (") + cond.ctype + ") !");
					break;
				}

				convertConditionInner(cond.rhs.get().get(), cond.ctype);
			}
		}

		void convertCondition(Tcondition const & cond) const
		{
			(*out) << " if ";
			convertConditionOuter(cond);
			(*out) << " then " << std::endl;
		}

		void operator()(Ttrigger const & trig) const
		{
			throw EInterpreterError("Triggers cannot be executed!");
		}

		void operator()(TPostTrigger const & trig) const
		{
			throw EInterpreterError("Post-triggers cannot be executed!");
		}

		void operator()(Tinstruction const & trig) const
		{
			if(trig.condition.is_initialized())
			{
				convertCondition(trig.condition.get());

				convert(trig.name, trig.identifier, boost::make_optional(trig.body));

				(*out) << "end" << std::endl;

			}
			else
			{
				convert(trig.name, trig.identifier, boost::make_optional(trig.body));
			}
		}

		void operator()(Treceiver const & trig) const
		{
			if(trig.condition.is_initialized())
			{
				convertCondition(trig.condition.get());

				convert(trig.name, trig.identifier, trig.body);

				(*out) << "end" << std::endl;
			}
			else
			{
				convert(trig.name, trig.identifier, trig.body);
			}
		}
	};

	struct Command : public Converter
	{
		Command(std::ostream * out_)
			: Converter(out_)
		{}

		void operator()(Tcommand const & cmd) const
		{
			boost::apply_visitor(ERMExp(out), cmd.cmd);
		}
		void operator()(std::string const & comment) const
		{
			(*out) << "-- " << comment << std::endl;
		}

		void operator()(spirit::unused_type const &) const
		{
		}
	};

	struct TLiteralEval : public boost::static_visitor<std::string>
	{

		std::string operator()(char const & val)
		{
			return boost::to_string(val);
		}
		std::string operator()(double const & val)
		{
			return boost::lexical_cast<std::string>(val);
		}
		std::string operator()(int const & val)
		{
			return boost::lexical_cast<std::string>(val);
		}
		std::string operator()(std::string const & val)
		{
			return "[===[" + std::string(val) + "]===]";
		}
	};

	struct VOptionEval : public Converter
	{
		bool discardResult;

		VOptionEval(std::ostream * out_, bool discardResult_ = false)
			: Converter(out_),
			discardResult(discardResult_)
		{}

		void operator()(VNIL const & opt) const
		{
			if(!discardResult)
			{
				(*out) << "return ";
			}
			(*out) << "nil";
		}
		void operator()(VNode const & opt) const;

		void operator()(VSymbol const & opt) const
		{
			if(!discardResult)
			{
				(*out) << "return ";
			}
			(*out) << opt.text;
		}
		void operator()(TLiteral const & opt) const
		{
			if(!discardResult)
			{
				(*out) << "return ";
			}

			TLiteralEval tmp;

			(*out) << boost::apply_visitor(tmp, opt);
		}
		void operator()(ERM::Tcommand const & opt) const
		{
			//this is how FP works, evaluation == producing side effects
			//TODO: can we evaluate to smth more useful?
			boost::apply_visitor(ERMExp(out), opt.cmd);
		}
		void operator()(VFunc const & opt) const
		{
//			return opt;
		}
	};

	struct VOptionNodeEval : public Converter
	{
		bool discardResult;

		VNode & exp;

		VOptionNodeEval(std::ostream * out_, VNode & exp_, bool discardResult_ = false)
			: Converter(out_),
			discardResult(discardResult_),
			exp(exp_)
		{}

		void operator()(VNIL const & opt) const
		{
			throw EVermScriptExecError("Nil does not evaluate to a function");
		}

		void operator()(VNode const & opt) const
		{
			VNode tmpn(exp);

			(*out) << "local fu = ";

			VOptionEval tmp(out, true);
			tmp(opt);

			(*out) << std::endl;

			if(!discardResult)
				(*out) << "return ";

			(*out) << "fu(";

			VOptionList args = tmpn.children.cdr().getAsList();

			for(int g=0; g<args.size(); ++g)
			{
				if(g == 0)
				{
					boost::apply_visitor(VOptionEval(out, true), args[g]);
				}
				else
				{
					(*out) << ", ";
					boost::apply_visitor(VOptionEval(out, true), args[g]);
				}
			}

			(*out) << ")";
		}

		void operator()(VSymbol const & opt) const
		{
			std::map<std::string, std::string> symToOperator =
			{
				{"<", "<"},
				{"<=", "<="},
				{">", ">"},
				{">=", ">="},
				{"=", "=="},
				{"+", "+"},
				{"-", "-"},
				{"*", "*"},
				{"/", "/"},
				{"%", "%"}
			};

			if(false)
			{

			}
//			//check keywords
//			else if(opt.text == "quote")
//			{
//				if(exp.children.size() == 2)
//					return exp.children[1];
//				else
//					throw EVermScriptExecError("quote special form takes only one argument");
//			}
//			else if(opt.text == "backquote")
//			{
//				if(exp.children.size() == 2)
//					return boost::apply_visitor(_SbackquoteEval(interp), exp.children[1]);
//				else
//					throw EVermScriptExecError("backquote special form takes only one argument");
//
//			}
//			else if(opt.text == "car")
//			{
//				if(exp.children.size() != 2)
//					throw EVermScriptExecError("car special form takes only one argument");
//
//				auto & arg = exp.children[1];
//				VOption evaluated = interp->eval(arg);
//
//				return boost::apply_visitor(CarEval(interp), evaluated);
//			}
//			else if(opt.text == "cdr")
//			{
//				if(exp.children.size() != 2)
//					throw EVermScriptExecError("cdr special form takes only one argument");
//
//				auto & arg = exp.children[1];
//				VOption evaluated = interp->eval(arg);
//
//				return boost::apply_visitor(CdrEval(interp), evaluated);
//			}
			else if(opt.text == "if")
			{
				if(exp.children.size() > 4 || exp.children.size() < 3)
					throw EVermScriptExecError("if special form takes two or three arguments");

				(*out) << "if ";

				boost::apply_visitor(VOptionEval(out, true),  exp.children[1]);

				(*out) << " then" << std::endl;

				boost::apply_visitor(VOptionEval(out, discardResult),  exp.children[2]);

				if(exp.children.size() == 4)
				{
					(*out) << std::endl << "else" << std::endl;

					boost::apply_visitor(VOptionEval(out, discardResult),  exp.children[3]);
				}

				(*out)<< std::endl << "end" << std::endl;
			}
			else if(opt.text == "lambda")
			{
				if(exp.children.size() <= 2)
				{
					throw EVermScriptExecError("Too few arguments for lambda special form");
				}

				(*out) << " function(";

				VNode arglist = getAs<VNode>(exp.children[1]);

				for(int g=0; g<arglist.children.size(); ++g)
				{
					std::string argName = getAs<VSymbol>(arglist.children[g]).text;

					if(g == 0)
						(*out) << argName;
					else
						(*out) << ", " <<argName;
				}

				(*out) << ")" << std::endl;

				VOptionList body = exp.children.cdr().getAsCDR().getAsList();

				for(int g=0; g<body.size(); ++g)
				{
					if(g < body.size()-1)
						boost::apply_visitor(VOptionEval(out, true), body[g]);
					else
						boost::apply_visitor(VOptionEval(out, false), body[g]);
				}

				(*out)<< std::endl << "end";
			}
			else if(opt.text == "setq")
			{
				if(exp.children.size() != 3 && exp.children.size() != 4)
					throw EVermScriptExecError("setq special form takes 2 or 3 arguments");

				std::string name = getAs<VSymbol>(exp.children[1]).text;

				size_t valIndex = 2;

				if(exp.children.size() == 4)
				{
					TLiteral varIndexLit = getAs<TLiteral>(exp.children[2]);

					int varIndex = getAs<int>(varIndexLit);

					boost::format fmt("%s[%d]");
					fmt % name % varIndex;

					name = fmt.str();

					valIndex = 3;
				}

				(*out) << name << " = ";

				boost::apply_visitor(VOptionEval(out, true), exp.children[valIndex]);

				(*out) << std::endl;

				if(!discardResult)
					(*out) << "return " << name << std::endl;
			}
			else if(opt.text == ERMInterpreter::defunSymbol)
			{
				if(exp.children.size() < 4)
				{
					throw EVermScriptExecError("defun special form takes at least 3 arguments");
				}

				std::string name = getAs<VSymbol>(exp.children[1]).text;

				(*out) << std::endl << "local function " << name << " (";

				VNode arglist = getAs<VNode>(exp.children[2]);

				for(int g=0; g<arglist.children.size(); ++g)
				{
					std::string argName = getAs<VSymbol>(arglist.children[g]).text;

					if(g == 0)
						(*out) << argName;
					else
						(*out) << ", " <<argName;
				}

				(*out) << ")" << std::endl;

				VOptionList body = exp.children.cdr().getAsCDR().getAsCDR().getAsList();

				for(int g=0; g<body.size(); ++g)
				{
					if(g < body.size()-1)
						boost::apply_visitor(VOptionEval(out, true), body[g]);
					else
						boost::apply_visitor(VOptionEval(out, false), body[g]);
				}

				(*out)<< std::endl << "end";

				if(!discardResult)
				{
					(*out)  << std::endl << "return " << name;
				}

			}
			else if(opt.text == "defmacro")
			{
//				if(exp.children.size() < 4)
//				{
//					throw EVermScriptExecError("defmacro special form takes at least 3 arguments");
//				}
//				VFunc f(exp.children.cdr().getAsCDR().getAsCDR().getAsList(), true);
//				VNode arglist = getAs<VNode>(exp.children[2]);
//				for(int g=0; g<arglist.children.size(); ++g)
//				{
//					f.args.push_back(getAs<VSymbol>(arglist.children[g]));
//				}
//				env.localBind(getAs<VSymbol>(exp.children[1]).text, f);
//				return f;
			}
			else if(opt.text == "progn")
			{
				(*out)<< std::endl << "do" << std::endl;

				for(int g=1; g<exp.children.size(); ++g)
				{
					if(g < exp.children.size()-1)
						boost::apply_visitor(VOptionEval(out, true),  exp.children[g]);
					else
						boost::apply_visitor(VOptionEval(out, discardResult),  exp.children[g]);
				}
				(*out) << std::endl << "end" << std::endl;
			}
			else if(opt.text == "do") //evaluates second argument as long first evaluates to non-nil
			{
				if(exp.children.size() != 3)
				{
					throw EVermScriptExecError("do special form takes exactly 2 arguments");
				}

				(*out) << std::endl << "while ";

				boost::apply_visitor(VOptionEval(out, true), exp.children[1]);

				(*out) << " do" << std::endl;

				boost::apply_visitor(VOptionEval(out, true), exp.children[2]);

				(*out) << std::endl << "end" << std::endl;

			}
			//"apply" part of eval, a bit blurred in this implementation but this way it looks good too
			else if(symToOperator.find(opt.text) != symToOperator.end())
			{
				if(!discardResult)
					(*out) << "return ";

				std::string _operator = symToOperator[opt.text];

				VOptionList opts = exp.children.cdr().getAsList();

				for(int g=0; g<opts.size(); ++g)
				{
					if(g == 0)
					{
						boost::apply_visitor(VOptionEval(out, true), opts[g]);
					}
					else
					{
						(*out) << " " << _operator << " ";
						boost::apply_visitor(VOptionEval(out, true), opts[g]);
					}
				}
			}
			else
			{
				//assume callable
				if(!discardResult)
					(*out) << "return ";

				(*out) << opt.text << "(";

				VOptionList opts = exp.children.cdr().getAsList();

				for(int g=0; g<opts.size(); ++g)
				{
					if(g == 0)
					{
						boost::apply_visitor(VOptionEval(out, true), opts[g]);
					}
					else
					{
						(*out) << ", ";
						boost::apply_visitor(VOptionEval(out, true), opts[g]);
					}
				}

				(*out) << ")";
			}

		}
		void operator()(TLiteral const & opt) const
		{
			throw EVermScriptExecError("Literal does not evaluate to a function: "+boost::to_string(opt));
		}
		void operator()(ERM::Tcommand const & opt) const
		{
			throw EVermScriptExecError("ERM command does not evaluate to a function");
		}
		void operator()(VFunc const & opt) const
		{
//			return opt;
		}
	};

	void VOptionEval::operator()(VNode const& opt) const
	{
		if(!opt.children.empty())
		{
			VOption & car = const_cast<VNode&>(opt).children.car().getAsItem();

			boost::apply_visitor(VOptionNodeEval(out, const_cast<VNode&>(opt), discardResult), car);
		}
	}


	struct Line : public Converter
	{
		Line(std::ostream * out_)
			: Converter(out_)
		{}

		void operator()(TVExp const & cmd) const
		{
			//TODO:
			VNode line(cmd);

			VOptionEval eval(out, true);
			eval(line);

			(*out) << std::endl;
		}
		void operator()(TERMline const & cmd) const
		{
			boost::apply_visitor(Command(out), cmd);
		}
	};

	void convertFunctions(std::ostream & out, ERMInterpreter * owner, const std::vector<VERMInterpreter::Trigger> & triggers)
	{
		std::map<std::string, LinePointer> numToBody;

		Line lineConverter(&out);

		for(const VERMInterpreter::Trigger & trigger : triggers)
		{
			ERM::TLine firstLine = owner->retrieveLine(trigger.line);

			const ERM::TTriggerBase & trig = ERMInterpreter::retrieveTrigger(firstLine);

			if(!trig.identifier.is_initialized())
				throw EInterpreterError("Function must have identifier");

			ERM::Tidentifier tid = trig.identifier.get();

			if(tid.size() == 0)
				throw EInterpreterError("Function must have identifier");

			std::string num = boost::apply_visitor(LVL1Iexp(), tid[0]);

			if(vstd::contains(numToBody, num))
				throw EInterpreterError("Function index duplicated: "+num);

			numToBody[num] = trigger.line;
		}

		for(const auto & p : numToBody)
		{
			std::string name = "FU"+p.first;

			out << name << " = function(x)" << std::endl;

			LinePointer lp = p.second;

			++lp;

			out << "local y = ERM.getY('" << name << "')" << std::endl;

			for(; lp.isValid(); ++lp)
			{
				ERM::TLine curLine = owner->retrieveLine(lp);
				if(owner->isATrigger(curLine))
					break;

				boost::apply_visitor(lineConverter, curLine);
			}

			out << "end" << std::endl;
		}
	}
}

struct ScriptScanner : boost::static_visitor<>
{
	ERMInterpreter * interpreter;
	LinePointer lp;

	ScriptScanner(ERMInterpreter * interpr, const LinePointer & _lp) : interpreter(interpr), lp(_lp)
	{}

	void operator()(TVExp const& cmd) const
	{
		//
	}
	void operator()(TERMline const& cmd) const
	{
		if(cmd.which() == 0) //TCommand
		{
			Tcommand tcmd = boost::get<Tcommand>(cmd);
			switch (tcmd.cmd.which())
			{
			case 0: //trigger
				{
					Trigger trig;
					trig.line = lp;
					interpreter->triggers[ TriggerType(boost::get<ERM::Ttrigger>(tcmd.cmd).name) ].push_back(trig);
				}
				break;
			case 3: //post trigger
				{
					Trigger trig;
					trig.line = lp;
					interpreter->postTriggers[ TriggerType(boost::get<ERM::TPostTrigger>(tcmd.cmd).name) ].push_back(trig);
				}
				break;
			default:

				break;
			}
		}

	}
};

void ERMInterpreter::scanScripts()
{
	for(std::map< LinePointer, ERM::TLine >::const_iterator it = scripts.begin(); it != scripts.end(); ++it)
	{
		boost::apply_visitor(ScriptScanner(this, it->first), it->second);
	}
}

ERMInterpreter::ERMInterpreter(vstd::CLoggerBase * logger_)
	: logger(logger_)
{
	curFunc = nullptr;
	curTrigger = nullptr;

	ermGlobalEnv = new ERMEnvironment();
	globalEnv = new Environment();

	topDyn = globalEnv;
}

ERMInterpreter::~ERMInterpreter()
{
	vstd::clear_pointer(globalEnv);
	vstd::clear_pointer(ermGlobalEnv);
}

void ERMInterpreter::executeTrigger(VERMInterpreter::Trigger & trig, int funNum, std::vector<int> funParams)
{
	//function-related logic
	if(funNum != -1)
	{
		curFunc = getFuncVars(funNum);
		for(int g=1; g<=FunctionLocalVars::NUM_PARAMETERS; ++g)
		{
			curFunc->getParam(g) = g-1 < funParams.size() ? funParams[g-1] : 0;
		}
	}
	else
		curFunc = getFuncVars(0);

	//skip the first line
	LinePointer lp = trig.line;
	++lp;
	for(; lp.isValid(); ++lp)
	{
		ERM::TLine curLine = retrieveLine(lp);
		if(isATrigger(curLine))
			break;

		executeLine(lp);
	}

	curFunc = nullptr;
}

bool ERMInterpreter::isATrigger( const ERM::TLine & line )
{
	switch(line.which())
	{
	case 0: //v-exp
		{
			TVExp vexp = boost::get<TVExp>(line);
			if(vexp.children.size() == 0)
				return false;

			switch (getExpType(vexp.children[0]))
			{
			case SYMBOL:
				{
					//TODO: what about sym modifiers?
					//TOOD: macros?
					ERM::TSymbol sym = boost::get<ERM::TSymbol>(vexp.children[0]);
					return sym.sym == triggerSymbol || sym.sym == postTriggerSymbol;
				}
				break;
			case TCMD:
				return isCMDATrigger( boost::get<ERM::Tcommand>(vexp.children[0]) );
				break;
			default:
				return false;
				break;
			}
		}
		break;
	case 1: //erm
		{
			TERMline ermline = boost::get<TERMline>(line);
			switch(ermline.which())
			{
			case 0: //tcmd
				return isCMDATrigger( boost::get<ERM::Tcommand>(ermline) );
				break;
			default:
				return false;
				break;
			}
		}
		break;
	default:
		assert(0); //it should never happen
		break;
	}
	assert(0);
	return false;
}

ERM::EVOtions ERMInterpreter::getExpType( const ERM::TVOption & opt )
{
	//MAINTENANCE: keep it correct!
	return static_cast<ERM::EVOtions>(opt.which());
}

bool ERMInterpreter::isCMDATrigger( const ERM::Tcommand & cmd )
{
	switch (cmd.cmd.which())
	{
	case 0: //trigger
	case 3: //post trigger
		return true;
		break;
	default:
		return false;
		break;
	}
}

ERM::TLine &ERMInterpreter::retrieveLine( LinePointer linePtr )
{
	return scripts.find(linePtr)->second;
}

/////////
//code execution

template<typename OwnerType>
struct StandardBodyOptionItemVisitor : boost::static_visitor<>
{
	typedef OwnerType TReceiverType;
	OwnerType & owner;
	explicit StandardBodyOptionItemVisitor(OwnerType & _owner) : owner(_owner)
	{}
	virtual void operator()(TVarConcatString const& cmp) const
	{
		throw EScriptExecError("String concatenation not allowed in this receiver");
	}
	virtual void operator()(TStringConstant const& cmp) const
	{
		throw EScriptExecError("String constant not allowed in this receiver");
	}
	virtual void operator()(TCurriedString const& cmp) const
	{
		throw EScriptExecError("Curried string not allowed in this receiver");
	}
	virtual void operator()(TSemiCompare const& cmp) const
	{
		throw EScriptExecError("Semi comparison not allowed in this receiver");
	}
// 	virtual void operator()(TMacroUsage const& cmp) const
// 	{
// 		throw EScriptExecError("Macro usage not allowed in this receiver");
// 	}
	virtual void operator()(TMacroDef const& cmp) const
	{
		throw EScriptExecError("Macro definition not allowed in this receiver");
	}
	virtual void operator()(TIexp const& cmp) const
	{
		throw EScriptExecError("i-expression not allowed in this receiver");
	}
	virtual void operator()(TVarpExp const& cmp) const
	{
		throw EScriptExecError("Varp expression not allowed in this receiver");
	}
	virtual void operator()(spirit::unused_type const& cmp) const
	{
		throw EScriptExecError("\'Nothing\' not allowed in this receiver");
	}
};

void ERMInterpreter::executeLine( const LinePointer & lp )
{
	executeLine(scripts[lp]);
}

void ERMInterpreter::executeLine(const ERM::TLine &line)
{
	assert(false);
}

IexpValStr ERMInterpreter::getVar(std::string toFollow, boost::optional<int> initVal) const
{
	IexpValStr ret;
	ret.type = IexpValStr::WRONGVAL;

	int initV=0;
	bool hasInit = false;
	if(initVal.is_initialized())
	{
		initV = initVal.get();
		hasInit = true;
	}

	int endNum = 0;
	if(toFollow[0] == 'd')
	{
		endNum = 1;
		//TODO: support
	}
	if(toFollow.size() == 0)
	{
		if(hasInit)
			ret = IexpValStr(initV);
		else
			throw EIexpProblem("No input to getVar!");

		return ret;
	}
	//now we have at least one element in toFollow
	for(int b=toFollow.size()-1; b>=endNum; --b)
	{
		bool retIt = b == endNum/*+1*/; //if we should return the value are currently at

		char cr = toFollow[b];
		if(cr == 'c')//write number of current day
		{
			//TODO
		}
		else if(cr == 'd') //info for external env - add i/o set
		{
			throw EIexpProblem("d inside i-expression not allowed!");
		}
		else if(cr == 'e')
		{
			if(hasInit)
			{
				if(retIt)
				{
					//these C-style cast is here just to shut up compiler errors
					if(initV > 0 && initV <= FunctionLocalVars::NUM_FLOATINGS)
					{
						if(curFunc)
							ret = IexpValStr(&curFunc->getFloat(initV));
						else
							throw EIexpProblem("Function context not available!");
					}
					else if(initV < 0 && initV >= -TriggerLocalVars::EVAR_NUM)
					{
						if(curTrigger)
							ret = IexpValStr(&curTrigger->ermLocalVars.getEvar(initV));
						else
							throw EIexpProblem("No trigger context available!");
					}
					else
						throw EIexpProblem("index " + boost::lexical_cast<std::string>(initV) + " not allowed for e array");
				}
				else
					throw EIexpProblem("e variables cannot appear in this context");
			}
			else
				throw EIexpProblem("e variables cannot appear in this context");
		}
		else if(cr >= 'f' && cr <= 't')
		{
			if(retIt)
				ret = IexpValStr(&ermGlobalEnv->getQuickVar(cr));
			else
			{
				if(hasInit)
					throw EIexpProblem("quick variables cannot be used in this context");
				else
				{
					initV = ermGlobalEnv->getQuickVar(cr);
					hasInit = true;
				}
			}
		}
		else if(cr == 'v') //standard variables
		{
			if(hasInit)
			{
				if(retIt)
					ret = IexpValStr(&ermGlobalEnv->getStandardVar(initV));
				else
					initV = ermGlobalEnv->getStandardVar(initV);
			}
			else
				throw EIexpProblem("standard variable cannot be used in this context!");
		}
		else if(cr == 'w') //local hero variables
		{
			//TODO
		}
		else if(cr == 'x') //function parameters
		{
			if(hasInit)
			{
				if(curFunc)
				{
					if(retIt)
						ret = IexpValStr(&curFunc->getParam(initV));
					else
						initV = curFunc->getParam(initV);
				}
				else throw EIexpProblem("Function parameters cannot be used outside a function!");
			}
			else
				throw EIexpProblem("Specify which function parameter should be used");
		}
		else if(cr == 'y')
		{
			if(hasInit)
			{
				if(initV > 0 && initV <= FunctionLocalVars::NUM_LOCALS)
				{
					int &valPtr = curFunc ? curFunc->getLocal(initV) : const_cast<ERMInterpreter&>(*this).getFuncVars(0)->getLocal(initV); //retrieve local var if in function or use global set otherwise
					if(retIt)
						ret = IexpValStr(&valPtr);
					else
						initV = curFunc->getLocal(initV);
				}
				else if(initV < 0 && initV >= -TriggerLocalVars::YVAR_NUM)
				{
					if(curTrigger)
					{
						if(retIt)
							ret = IexpValStr(&curTrigger->ermLocalVars.getYvar(initV));
						else
							initV = curTrigger->ermLocalVars.getYvar(initV);
					}
					else
						throw EIexpProblem("Trigger local variables cannot be used outside triggers!");
				}
				else
					throw EIexpProblem("Wrong argument for function local variable!");
			}
			else
				throw EIexpProblem("y variable cannot be used in this context!");
		}
		else if(cr == 'z')
		{
			if(hasInit)
			{
				if(retIt)
				{
					//these C-style casts are here just to shut up compiler errors
					if(initV > 0 )
						ret = IexpValStr(&ermGlobalEnv->getZVar(initV));
					else if(initV < 0)
					{
						if(curFunc)
							ret = IexpValStr(&curFunc->getString(initV));
						else
							throw EIexpProblem("Function local string variables cannot be used outside functions!");
					}
					else
						throw EIexpProblem("Wrong parameter for string variable!");
				}
				else
					throw EIexpProblem("String variables can only be returned!");
			}
			else
				throw EIexpProblem("String variables cannot be used in this context!");
		}
		else
		{
			throw EIexpProblem(std::string("Symbol ") + cr + " is not allowed in this context!");
		}

	}

	ret.name = toFollow;
	if(initVal.is_initialized())
	{
		ret.name += boost::lexical_cast<std::string>(initVal.get());
	}
	return ret;
}

ERM::TTriggerBase & ERMInterpreter::retrieveTrigger( ERM::TLine &line )
{
	if(line.which() == 1)
	{
		ERM::TERMline &tl = boost::get<ERM::TERMline>(line);
		if(tl.which() == 0)
		{
			ERM::Tcommand &tcm = boost::get<ERM::Tcommand>(tl);
			if(tcm.cmd.which() == 0)
			{
				return boost::get<ERM::Ttrigger>(tcm.cmd);
			}
			else if(tcm.cmd.which() == 3)
			{
				return boost::get<ERM::TPostTrigger>(tcm.cmd);
			}
			throw ELineProblem("Given line is not a trigger!");
		}
		throw ELineProblem("Given line is not a command!");
	}
	throw ELineProblem("Given line is not an ERM trigger!");
}

bool ERMInterpreter::checkCondition( ERM::Tcondition cond )
{
	return false;
}

FunctionLocalVars * ERMInterpreter::getFuncVars( int funNum )
{
	if(funNum >= ARRAY_COUNT(funcVars) || funNum < 0)
		throw EScriptExecError("Attempt of accessing variables of function with index out of boundaries!");
	return funcVars + funNum;
}

void ERMInterpreter::executeInstructions()
{
	//TODO implement me
}

int ERMInterpreter::getRealLine(const LinePointer &lp)
{
	for(std::map<VERMInterpreter::LinePointer, ERM::TLine>::const_iterator i = scripts.begin(); i != scripts.end(); i++)
		if(i->first.lineNum == lp.lineNum && i->first.file->filename == lp.file->filename)
			return i->first.realLineNum;

	return -1;
}

const std::string ERMInterpreter::triggerSymbol = "trigger";
const std::string ERMInterpreter::postTriggerSymbol = "postTrigger";
const std::string ERMInterpreter::defunSymbol = "defun";

VERMInterpreter::ERMEnvironment::ERMEnvironment()
{
	for(int g=0; g<NUM_QUICKS; ++g)
		quickVars[g] = 0;
	for(int g=0; g<NUM_STANDARDS; ++g)
		standardVars[g] = 0;
	//string should be automatically initialized to ""
	for(int g=0; g<NUM_FLAGS; ++g)
		flags[g] = false;
}

int & VERMInterpreter::ERMEnvironment::getQuickVar( const char letter )
{
	assert(letter >= 'f' && letter <= 't'); //it should be check by another function, just making sure here
	return quickVars[letter - 'f'];
}

int & VERMInterpreter::ERMEnvironment::getStandardVar( int num )
{
	if(num < 1 || num > NUM_STANDARDS)
		throw EScriptExecError("Number of standard variable out of bounds");

	return standardVars[num-1];
}

std::string & VERMInterpreter::ERMEnvironment::getZVar( int num )
{
	if(num < 1 || num > NUM_STRINGS)
		throw EScriptExecError("Number of string variable out of bounds");

	return strings[num-1];
}

bool & VERMInterpreter::ERMEnvironment::getFlag( int num )
{
	if(num < 1 || num > NUM_FLAGS)
		throw EScriptExecError("Number of flag out of bounds");

	return flags[num-1];
}

VERMInterpreter::TriggerLocalVars::TriggerLocalVars()
{
	for(int g=0; g<EVAR_NUM; ++g)
		evar[g] = 0.0;
	for(int g=0; g<YVAR_NUM; ++g)
		yvar[g] = 0;
}

double & VERMInterpreter::TriggerLocalVars::getEvar( int num )
{
	num = -num;
	if(num < 1 || num > EVAR_NUM)
		throw EScriptExecError("Number of trigger local floating point variable out of bounds");

	return evar[num-1];
}

int & VERMInterpreter::TriggerLocalVars::getYvar( int num )
{
	num = -num; //we handle negative indices
	if(num < 1 || num > YVAR_NUM)
		throw EScriptExecError("Number of trigger local variable out of bounds");

	return yvar[num-1];
}

bool VERMInterpreter::Environment::isBound( const std::string & name, EIsBoundMode mode ) const
{
	std::map<std::string, VOption>::const_iterator it = symbols.find(name);
	if(mode == LOCAL_ONLY)
	{
		return it != symbols.end();
	}

	if(mode == GLOBAL_ONLY && parent)
	{
		return parent->isBound(name, mode);
	}

	//we have it; if globalOnly is true, lexical parent is false here so we are global env
	if(it != symbols.end())
		return true;

	//here, we don;t have it; but parent can have
	if(parent)
		return parent->isBound(name, mode);

	return false;
}

VOption & VERMInterpreter::Environment::retrieveValue( const std::string & name )
{
	std::map<std::string, VOption>::iterator it = symbols.find(name);
	if(it == symbols.end())
	{
		if(parent)
		{
			return parent->retrieveValue(name);
		}

		throw ESymbolNotFound(name);
	}
	return it->second;
}

bool VERMInterpreter::Environment::unbind( const std::string & name, EUnbindMode mode )
{
	if(isBound(name, ANYWHERE))
	{
		if(symbols.find(name) != symbols.end()) //result of isBound could be from higher lexical env
			symbols.erase(symbols.find(name));

		if(mode == FULLY_RECURSIVE && parent)
			parent->unbind(name, mode);

		return true;
	}
	if(parent && (mode == RECURSIVE_UNTIL_HIT || mode == FULLY_RECURSIVE))
		return parent->unbind(name, mode);

	//neither bound nor have lexical parent
	return false;
}

void VERMInterpreter::Environment::localBind( std::string name, const VOption & sym )
{
	symbols[name] = sym;
}

void VERMInterpreter::Environment::setParent( Environment * _parent )
{
	parent = _parent;
}

Environment * VERMInterpreter::Environment::getParent() const
{
	return parent;
}

void VERMInterpreter::Environment::bindAtFirstHit( std::string name, const VOption & sym )
{
	if(isBound(name, Environment::LOCAL_ONLY) || !parent)
		localBind(name, sym);
	else
		parent->bindAtFirstHit(name, sym);
}

int & VERMInterpreter::FunctionLocalVars::getParam( int num )
{
	if(num < 1 || num > NUM_PARAMETERS)
		throw EScriptExecError("Number of parameter out of bounds");

	return params[num-1];
}

int & VERMInterpreter::FunctionLocalVars::getLocal( int num )
{
	if(num < 1 || num > NUM_LOCALS)
		throw EScriptExecError("Number of local variable out of bounds");

	return locals[num-1];
}

std::string & VERMInterpreter::FunctionLocalVars::getString( int num )
{
	num = -num; //we deal with negative indices
	if(num < 1 || num > NUM_PARAMETERS)
		throw EScriptExecError("Number of function local string variable out of bounds");

	return strings[num-1];
}

double & VERMInterpreter::FunctionLocalVars::getFloat( int num )
{
	if(num < 1 || num > NUM_FLOATINGS)
		throw EScriptExecError("Number of float var out of bounds");

	return floats[num-1];
}

void VERMInterpreter::FunctionLocalVars::reset()
{
	for(int g=0; g<ARRAY_COUNT(params); ++g)
		params[g] = 0;
	for(int g=0; g<ARRAY_COUNT(locals); ++g)
		locals[g] = 0;
	for(int g=0; g<ARRAY_COUNT(strings); ++g)
		strings[g] = "";
	for(int g=0; g<ARRAY_COUNT(floats); ++g)
		floats[g] = 0.0;
}

void IexpValStr::setTo( const IexpValStr & second )
{
//	logger->trace("setting %s to %s", getName(), second.getName());
	switch(type)
	{
	case IexpValStr::FLOATVAR:
		*val.flvar = second.getFloat();
		break;
	case IexpValStr::INT:
		throw EScriptExecError("VR S: value not assignable!");
		break;
	case IexpValStr::INTVAR:
		*val.integervar = second.getInt();
		break;
	case IexpValStr::STRINGVAR:
		*val.stringvar = second.getString();
		break;
	default:
		throw EScriptExecError("Wrong type of identifier iexp!");
	}
}

void IexpValStr::setTo( int val )
{
//	logger->trace("setting %s to %d", getName(), val);
	switch(type)
	{
	case INTVAR:
		*this->val.integervar = val;
		break;
	default:
		throw EIexpProblem("Incompatible type!");
		break;
	}
}

void IexpValStr::setTo( double val )
{
//	logger->trace("setting %s to %f", getName(), val);
	switch(type)
	{
	case FLOATVAR:
		*this->val.flvar = val;
		break;
	default:
		throw EIexpProblem("Incompatible type!");
		break;
	}
}

void IexpValStr::setTo( const std::string & val )
{
//	logger->trace("setting %s to %s", getName(), val);
	switch(type)
	{
	case STRINGVAR:
		*this->val.stringvar = val;
		break;
	default:
		throw EIexpProblem("Incompatible type!");
		break;
	}
}

int IexpValStr::getInt() const
{
	switch(type)
	{
	case IexpValStr::INT:
		return val.val;
		break;
	case IexpValStr::INTVAR:
		return *val.integervar;
		break;
	default:
		throw EIexpProblem("Cannot get iexp as int!");
		break;
	}
}

double IexpValStr::getFloat() const
{
	switch(type)
	{
	case IexpValStr::FLOATVAR:
		return *val.flvar;
		break;
	default:
		throw EIexpProblem("Cannot get iexp as float!");
		break;
	}
}

std::string IexpValStr::getString() const
{
	switch(type)
	{
	case IexpValStr::STRINGVAR:
		return *val.stringvar;
		break;
	default:
		throw EScriptExecError("Cannot get iexp as string!");
		break;
	}
}

std::string IexpValStr::getName() const
{
	if(name.size())
	{
		return name;
	}
	else if(type == IexpValStr::INT)
	{
		return "Literal " + boost::lexical_cast<std::string>(getInt());
	}
	else
		return "Unknown variable";
}

void ERMInterpreter::loadScript(const std::string & name, const std::string & source)
{
	ERMParser ep(source);
	FileInfo * finfo = new FileInfo();
	finfo->filename = name;

	std::vector<LineInfo> buf = ep.parseFile();
	finfo->length = buf.size();
	files.push_back(finfo);

	for(int g=0; g<buf.size(); ++g)
	{
		scripts[LinePointer(finfo, g, buf[g].realLineNum)] = buf[g].tl;
	}
}


std::string ERMInterpreter::convert()
{
	std::stringstream out;

	out << "local ERM = require(\"core:erm\")" << std::endl;

	out << "local v = ERM.v" << std::endl;
	out << "local z = ERM.z" << std::endl;
	out << "local flag = ERM.flag" << std::endl;

	for(const auto & p : triggers)
	{
		const VERMInterpreter::TriggerType & tt = p.first;

		if(tt.type == VERMInterpreter::TriggerType::FU)
		{
			ERMConverter::convertFunctions(out, this, p.second);
		}
		else
		{

		}
	}

	for(const auto & p : postTriggers)
		;//TODO

	//TODO: instructions

	//TODO: !?PI

	return out.str();
}

struct VOptionPrinter : boost::static_visitor<>
{
	void operator()(VNIL const& opt) const
	{
		logGlobal->error("VNIL");
	}
	void operator()(VNode const& opt) const
	{
		logGlobal->error("--vnode (will be supported in future versions)--");
	}
	void operator()(VSymbol const& opt) const
	{
		logGlobal->error(opt.text);
	}
	void operator()(TLiteral const& opt) const
	{
		logGlobal->error(boost::to_string(opt));
	}
	void operator()(ERM::Tcommand const& opt) const
	{
		logGlobal->error("--erm command (will be supported in future versions)--");
	}
	void operator()(VFunc const& opt) const
	{
		logGlobal->error("function");
	}
};

struct _SbackquoteEval : boost::static_visitor<VOption>
{
	ERMInterpreter * interp;

	_SbackquoteEval(ERMInterpreter * interp_)
		: interp(interp_)
	{}

	VOption operator()(VNIL const& opt) const
	{
		return opt;
	}
	VOption operator()(VNode const& opt) const
	{
		VNode ret = opt;
		if(opt.children.size() == 2)
		{
			VOption fo = opt.children[0];
			if(isA<VSymbol>(fo))
			{
				if(getAs<VSymbol>(fo).text == "comma")
				{
					return interp->eval(opt.children[1]);
				}
			}
		}
		for(int g=0; g<opt.children.size(); ++g)
		{
			ret.children[g] = boost::apply_visitor(_SbackquoteEval(interp), ret.children[g]);
		}
		return ret;
	}
	VOption operator()(VSymbol const& opt) const
	{
		return opt;
	}
	VOption operator()(TLiteral const& opt) const
	{
		return opt;
	}
	VOption operator()(ERM::Tcommand const& opt) const
	{
		return opt;
	}
	VOption operator()(VFunc const& opt) const
	{
		return opt;
	}
};

class CarEval : public boost::static_visitor<VOption>
{
	ERMInterpreter * interp;
public:
	CarEval(ERMInterpreter * interp_)
		: interp(interp_)
	{}

	VOption operator()(VNIL const& opt) const
	{
		return opt;
	}
	VOption operator()(VNode const& opt) const
	{
		if(opt.children.size() == 0)
		{
			return VNIL();
		}
		else
		{
			return opt.children[0];
		}
	}
	VOption operator()(VSymbol const& opt) const
	{
		return opt; //???
	}
	VOption operator()(TLiteral const& opt) const
	{
		return opt; //???
	}
	VOption operator()(ERM::Tcommand const& opt) const
	{
		return opt; //???
	}
	VOption operator()(VFunc const& opt) const
	{
		return opt; //???
	}
};

class CdrEval : public boost::static_visitor<VOption>
{
	ERMInterpreter * interp;
public:
	CdrEval(ERMInterpreter * interp_)
		: interp(interp_)
	{}

	VOption operator()(VNIL const& opt) const
	{
		return opt;
	}
	VOption operator()(VNode const& opt) const
	{
		if(opt.children.size() < 2)
		{
			return VNIL();
		}
		else
		{
			VNode ret(opt.children);
			ret.children.erase(ret.children.begin());
			return ret;
		}
	}
	VOption operator()(VSymbol const& opt) const
	{
		return VNIL();
	}
	VOption operator()(TLiteral const& opt) const
	{
		return VNIL();
	}
	VOption operator()(ERM::Tcommand const& opt) const
	{
		return VNIL();
	}
	VOption operator()(VFunc const& opt) const
	{
		return VNIL();
	}
};

struct VNodeEvaluator : boost::static_visitor<VOption>
{
	ERMInterpreter * interp;
	Environment & env;
	VNode & exp;
	VNodeEvaluator(ERMInterpreter * _interp, Environment & _env, VNode & _exp)
		: interp(_interp),
		env(_env),
		exp(_exp)
	{}
	VOption operator()(VNIL const& opt) const
	{
		throw EVermScriptExecError("Nil does not evaluate to a function");
	}
	VOption operator()(VNode const& opt) const
	{
		//otherwise...
		VNode tmpn(exp);
		tmpn.children.car() = interp->eval(opt);
		VFunc fun = getAs<VFunc>(tmpn.children.car().getAsItem());
		return fun(interp, tmpn.children.cdr());
	}
	VOption operator()(VSymbol const& opt) const
	{
		std::map<std::string, VFunc::Eopt> symToFunc =
		{
			{"<", VFunc::LT},{"<=", VFunc::LE},{">", VFunc::GT},{">=", VFunc::GE},{"=", VFunc::EQ},{"+", VFunc::ADD},{"-", VFunc::SUB},
			{"*", VFunc::MULT},{"/", VFunc::DIV},{"%", VFunc::MOD}
		};

		//check keywords
		if(opt.text == "quote")
		{
			if(exp.children.size() == 2)
				return exp.children[1];
			else
				throw EVermScriptExecError("quote special form takes only one argument");
		}
		else if(opt.text == "backquote")
		{
			if(exp.children.size() == 2)
				return boost::apply_visitor(_SbackquoteEval(interp), exp.children[1]);
			else
				throw EVermScriptExecError("backquote special form takes only one argument");

		}
		else if(opt.text == "car")
		{
			if(exp.children.size() != 2)
				throw EVermScriptExecError("car special form takes only one argument");

			auto & arg = exp.children[1];
			VOption evaluated = interp->eval(arg);

			return boost::apply_visitor(CarEval(interp), evaluated);
		}
		else if(opt.text == "cdr")
		{
			if(exp.children.size() != 2)
				throw EVermScriptExecError("cdr special form takes only one argument");

			auto & arg = exp.children[1];
			VOption evaluated = interp->eval(arg);

			return boost::apply_visitor(CdrEval(interp), evaluated);
		}
		else if(opt.text == "if")
		{
			if(exp.children.size() > 4)
				throw EVermScriptExecError("if statement takes no more than three arguments");

			if( !isA<VNIL>(interp->eval(exp.children[1]) ) )
			{
				if(exp.children.size() > 2)
					return interp->eval(exp.children[2]);
				else
					throw EVermScriptExecError("this if form needs at least two arguments");
			}
			else
			{
				if(exp.children.size() > 3)
					return interp->eval(exp.children[3]);
				else
					throw EVermScriptExecError("this if form needs at least three arguments");
			}
		}
		else if(opt.text == "lambda")
		{
			if(exp.children.size() <= 2)
			{
				throw EVermScriptExecError("Too few arguments for lambda special form");
			}
			VFunc ret(exp.children.cdr().getAsCDR().getAsList());
			VNode arglist = getAs<VNode>(exp.children[1]);
			for(int g=0; g<arglist.children.size(); ++g)
			{
				ret.args.push_back(getAs<VSymbol>(arglist.children[g]));
			}
			return ret;
		}
		else if(opt.text == "print")
		{
			if(exp.children.size() == 2)
			{
				VOption printed = interp->eval(exp.children[1]);
				boost::apply_visitor(VOptionPrinter(), printed);
				return printed;
			}
			else
				throw EVermScriptExecError("print special form takes only one argument");
		}
		else if(opt.text == "setq")
		{
			if(exp.children.size() != 3)
				throw EVermScriptExecError("setq special form takes exactly 2 arguments");

			env.bindAtFirstHit( getAs<VSymbol>(exp.children[1]).text, interp->eval(exp.children[2]));
			return getAs<VSymbol>(exp.children[1]);
		}
		else if(opt.text == ERMInterpreter::defunSymbol)
		{
			if(exp.children.size() < 4)
			{
				throw EVermScriptExecError("defun special form takes at least 3 arguments");
			}
			VFunc f(exp.children.cdr().getAsCDR().getAsCDR().getAsList());
			VNode arglist = getAs<VNode>(exp.children[2]);
			for(int g=0; g<arglist.children.size(); ++g)
			{
				f.args.push_back(getAs<VSymbol>(arglist.children[g]));
			}
			env.localBind(getAs<VSymbol>(exp.children[1]).text, f);
			return f;
		}
		else if(opt.text == "defmacro")
		{
			if(exp.children.size() < 4)
			{
				throw EVermScriptExecError("defmacro special form takes at least 3 arguments");
			}
			VFunc f(exp.children.cdr().getAsCDR().getAsCDR().getAsList(), true);
			VNode arglist = getAs<VNode>(exp.children[2]);
			for(int g=0; g<arglist.children.size(); ++g)
			{
				f.args.push_back(getAs<VSymbol>(arglist.children[g]));
			}
			env.localBind(getAs<VSymbol>(exp.children[1]).text, f);
			return f;
		}
		else if(opt.text == "progn")
		{
			for(int g=1; g<exp.children.size(); ++g)
			{
				if(g < exp.children.size()-1)
					interp->eval(exp.children[g]);
				else
					return interp->eval(exp.children[g]);
			}
			return VNIL();
		}
		else if(opt.text == "do") //evaluates second argument as long first evaluates to non-nil
		{
			if(exp.children.size() != 3)
			{
				throw EVermScriptExecError("do special form takes exactly 2 arguments");
			}
			while(!isA<VNIL>(interp->eval(exp.children[1])))
			{
				interp->eval(exp.children[2]);
			}
			return VNIL();
		}
		//"apply" part of eval, a bit blurred in this implementation but this way it looks good too
		else if(symToFunc.find(opt.text) != symToFunc.end())
		{
			VFunc f(symToFunc[opt.text]);
			if(f.macro)
			{
				return f(interp, exp.children.cdr());
			}
			else
			{
				VOptionList ls = interp->evalEach(exp.children.cdr());
				return f(interp, VermTreeIterator(ls));
			}
		}
		else if(interp->topDyn->isBound(opt.text, Environment::ANYWHERE))
		{
			VOption & bValue = interp->topDyn->retrieveValue(opt.text);
			if(!isA<VFunc>(bValue))
			{
				throw EVermScriptExecError("This value does not evaluate to a function!");
			}
			VFunc f = getAs<VFunc>(bValue);
			VOptionList ls = f.macro ? exp.children.cdr().getAsList() : interp->evalEach(exp.children.cdr());
			return f(interp, VermTreeIterator(ls));
		}
		interp->logger->error("Cannot evaluate:");
		printVOption(exp);
		throw EVermScriptExecError("Cannot evaluate given expression");
	}
	VOption operator()(TLiteral const& opt) const
	{
		throw EVermScriptExecError("Literal does not evaluate to a function: "+boost::to_string(opt));
	}
	VOption operator()(ERM::Tcommand const& opt) const
	{
		throw EVermScriptExecError("ERM command does not evaluate to a function");
	}
	VOption operator()(VFunc const& opt) const
	{
		return opt;
	}
};

struct VEvaluator : boost::static_visitor<VOption>
{
	ERMInterpreter * interp;
	Environment & env;
	VEvaluator(ERMInterpreter * _interp, Environment & _env)
		: interp(_interp),
		env(_env)
	{}
	VOption operator()(VNIL const& opt) const
	{
		return opt;
	}
	VOption operator()(VNode const& opt) const
	{
		if(opt.children.size() == 0)
			return VNIL();
		else
		{
			VOption & car = const_cast<VNode&>(opt).children.car().getAsItem();
			return boost::apply_visitor(VNodeEvaluator(interp, env, const_cast<VNode&>(opt)), car);
		}
	}
	VOption operator()(VSymbol const& opt) const
	{
		return env.retrieveValue(opt.text);
	}
	VOption operator()(TLiteral const& opt) const
	{
		return opt;
	}
	VOption operator()(ERM::Tcommand const& opt) const
	{
		//this is how FP works, evaluation == producing side effects
		//TODO: can we evaluate to smth more useful?
		return VNIL();
	}
	VOption operator()(VFunc const& opt) const
	{
		return opt;
	}
};

VOption ERMInterpreter::eval(VOption line, Environment * env)
{
	return boost::apply_visitor(VEvaluator(this, env ? *env : *topDyn), line);
}

VOptionList ERMInterpreter::evalEach(VermTreeIterator list, Environment * env)
{
	VOptionList ret;
	for(int g=0; g<list.size(); ++g)
	{
		ret.push_back(eval(list.getIth(g), env));
	}
	return ret;
}

namespace VERMInterpreter
{
	VOption convertToVOption(const ERM::TVOption & tvo)
	{
		return boost::apply_visitor(OptionConverterVisitor(), tvo);
	}

	VNode::VNode( const ERM::TVExp & exp )
	{
		for(int i=0; i<exp.children.size(); ++i)
		{
			children.push_back(convertToVOption(exp.children[i]));
		}
		processModifierList(exp.modifier, false);
	}

	VNode::VNode( const VOption & first, const VOptionList & rest ) /*merges given arguments into [a, rest] */
	{
		setVnode(first, rest);
	}

	VNode::VNode( const VOptionList & cdren ) : children(cdren)
	{}

	VNode::VNode( const ERM::TSymbol & sym )
	{
		children.car() = VSymbol(sym.sym);
		processModifierList(sym.symModifier, true);
	}

	void VNode::setVnode( const VOption & first, const VOptionList & rest )
	{
		children.car() = first;
		children.cdr() = rest;
	}

	void VNode::processModifierList( const std::vector<TVModifier> & modifierList, bool asSymbol )
	{
		for(int g=0; g<modifierList.size(); ++g)
		{
			if(asSymbol)
			{
				children.resize(children.size()+1);
				for(int i=children.size()-1; i >0; i--)
				{
					children[i] = children[i-1];
				}
			}
			else
			{
				children.cdr() = VNode(children);
			}

			if(modifierList[g] == "`")
			{
				children.car() = VSymbol("backquote");
			}
			else if(modifierList[g] == ",!")
			{
				children.car() = VSymbol("comma-unlist");
			}
			else if(modifierList[g] == ",")
			{
				children.car() = VSymbol("comma");
			}
			else if(modifierList[g] == "#'")
			{
				children.car() = VSymbol("get-func");
			}
			else if(modifierList[g] == "'")
			{
				children.car() = VSymbol("quote");
			}
			else
				throw EInterpreterError("Incorrect value of modifier!");
		}
	}

	VermTreeIterator & VermTreeIterator::operator=( const VOption & opt )
	{
		switch (state)
		{
		case CAR:
			if(parent->size() <= basePos)
				parent->push_back(opt);
			else
				(*parent)[basePos] = opt;
			break;
		case NORM:
			parent->resize(basePos+1);
			(*parent)[basePos] = opt;
			break;
		default://should never happen
			break;
		}
		return *this;
	}

	VermTreeIterator & VermTreeIterator::operator=( const std::vector<VOption> & opt )
	{
		switch (state)
		{
		case CAR:
			//TODO: implement me
			break;
		case NORM:
			parent->resize(basePos+1);
			parent->insert(parent->begin()+basePos, opt.begin(), opt.end());
			break;
		default://should never happen
			break;
		}
		return *this;
	}
	VermTreeIterator & VermTreeIterator::operator=( const VOptionList & opt )
	{
		return *this = opt;
	}
	VOption & VermTreeIterator::getAsItem()
	{
		if(state == CAR)
			return (*parent)[basePos];
		else
			throw EInterpreterError("iterator is not in car state, cannot get as list");
	}
	VermTreeIterator VermTreeIterator::getAsCDR()
	{
		VermTreeIterator ret = *this;
		ret.basePos++;
		return ret;
	}
	VOption & VermTreeIterator::getIth( int i )
	{
		return (*parent)[basePos + i];
	}
	size_t VermTreeIterator::size() const
	{
		return parent->size() - basePos;
	}

	VERMInterpreter::VOptionList VermTreeIterator::getAsList()
	{
		VOptionList ret;
		for(int g = basePos; g<parent->size(); ++g)
		{
			ret.push_back((*parent)[g]);
		}
		return ret;
	}

	VOption OptionConverterVisitor::operator()( ERM::TVExp const& cmd ) const
	{
		return VNode(cmd);
	}
	VOption OptionConverterVisitor::operator()( ERM::TSymbol const& cmd ) const
	{
		if(cmd.symModifier.size() == 0)
			return VSymbol(cmd.sym);
		else
			return VNode(cmd);
	}
	VOption OptionConverterVisitor::operator()( char const& cmd ) const
	{
		return TLiteral(cmd);
	}
	VOption OptionConverterVisitor::operator()( double const& cmd ) const
	{
		return TLiteral(cmd);
	}
	VOption OptionConverterVisitor::operator()(int const& cmd) const
	{
		return TLiteral(cmd);
	}
	VOption OptionConverterVisitor::operator()(ERM::Tcommand const& cmd) const
	{
		return cmd;
	}
	VOption OptionConverterVisitor::operator()( ERM::TStringConstant const& cmd ) const
	{
		return TLiteral(cmd.str);
	}

	bool VOptionList::isNil() const
	{
		return size() == 0;
	}

	VermTreeIterator VOptionList::cdr()
	{
		VermTreeIterator ret(*this);
		ret.basePos = 1;
		return ret;
	}

	VermTreeIterator VOptionList::car()
	{
		VermTreeIterator ret(*this);
		ret.state = VermTreeIterator::CAR;
		return ret;
	}


	VERMInterpreter::VOption VFunc::operator()(ERMInterpreter * interp, VermTreeIterator params)
	{
		switch(option)
		{
		case DEFAULT:
			{
				if(params.size() != args.size())
				{
					throw EVermScriptExecError("Expected " + boost::lexical_cast<std::string>(args.size()) + " arguments!");
				}
				IntroduceDynamicEnv dyn(interp);
				for(int i=0; i<args.size(); ++i)
				{
					if(macro)
						interp->topDyn->localBind(args[i].text, params.getIth(i));
					else
						interp->topDyn->localBind(args[i].text, interp->eval(params.getIth(i)));
				}
				//execute
				VOptionList toEval = body;
				if(macro)
				{
					//first evaluation (in place of definition)
					toEval = interp->evalEach(toEval);
				}
				//second evaluation for macros/evaluation of funcs
				VOptionList ret = interp->evalEach(toEval);
				return ret[ret.size()-1];
			}
			break;
		case LT:
			{
				if(params.size() != 2)
					throw EVermScriptExecError("< special function takes exactly 2 arguments");
				TLiteral lhs = getAs<TLiteral>(params.getIth(0)),
					rhs = getAs<TLiteral>(params.getIth(1));
				if(VERMInterpreter::operator<(lhs, rhs))
					return lhs;
				else
					return VNIL();
			}
			break;
		case LE:
			{
				if(params.size() != 2)
					throw EVermScriptExecError("<= special function takes exactly 2 arguments");

				TLiteral lhs = getAs<TLiteral>(params.getIth(0)),
					rhs = getAs<TLiteral>(params.getIth(1));
				if(VERMInterpreter::operator<=(lhs, rhs))
					return lhs;
				else
					return VNIL();
			}
			break;
		case GT:
			{
				if(params.size() != 2)
					throw EVermScriptExecError("> special function takes exactly 2 arguments");

				TLiteral lhs = getAs<TLiteral>(params.getIth(0)),
					rhs = getAs<TLiteral>(params.getIth(1));
				if(VERMInterpreter::operator>(lhs, rhs))
					return lhs;
				else
					return VNIL();
			}
			break;
		case GE:
			{
				if(params.size() != 2)
					throw EVermScriptExecError(">= special function takes exactly 2 arguments");

				TLiteral lhs = getAs<TLiteral>(params.getIth(0)),
					rhs = getAs<TLiteral>(params.getIth(1));
				if(VERMInterpreter::operator>=(lhs, rhs))
					return lhs;
				else
					return VNIL();
			}
			break;
		case EQ:
			{
				if(params.size() != 2)
					throw EVermScriptExecError("= special function takes exactly 2 arguments");
				printVOption(params.getIth(0));
				printVOption(params.getIth(1));
				TLiteral lhs = getAs<TLiteral>(params.getIth(0)),
					rhs = getAs<TLiteral>(params.getIth(1));
				if(lhs.type() == rhs.type())
				{
					if(boost::apply_visitor(_opEQvis(lhs), rhs))
						return lhs;
					else
						return VNIL();
				}
				else
					throw EVermScriptExecError("Incompatible types in = special function");

			}
			break;
		case ADD:
			{
				if(params.size() == 0)
					throw EVermScriptExecError("+ special function takes at least 1 argument");

				TLiteral par1 = getAs<TLiteral>(params.getIth(0));
				int retI = 0;
				double retD = 0.0;
				int used = isA<int>(par1) ? 0 : 1;

				for(int i=0; i<params.size(); ++i)
				{
					if(used == 0)
						retI += getAs<int>(getAs<TLiteral>(params.getIth(i)));
					else
						retD += getAs<double>(getAs<TLiteral>(params.getIth(i)));
				}
				if(used == 0)
					return retI;
				else
					return retD;
			}
			break;
		case SUB:
			{
				if(params.size() != 2)
					throw EVermScriptExecError("- special function takes at least 2 argument");

				TLiteral par1 = getAs<TLiteral>(params.getIth(0));
				int used = isA<int>(par1) ? 0 : 1;

				if(used == 0)
					return getAs<int>(getAs<TLiteral>(params.getIth(0))) - getAs<int>(getAs<TLiteral>(params.getIth(1)));
				else
					return getAs<double>(getAs<TLiteral>(params.getIth(1))) - getAs<double>(getAs<TLiteral>(params.getIth(1)));
			}
			break;
		case MULT:
			{
				if(params.size() == 0)
					throw EVermScriptExecError("* special function takes at least 1 argument");

				TLiteral par1 = getAs<TLiteral>(params.getIth(0));
				int retI = 1;
				double retD = 1.0;
				int used = isA<int>(par1) ? 0 : 1;

				for(int i=0; i<params.size(); ++i)
				{
					if(used == 0)
						retI *= getAs<int>(getAs<TLiteral>(params.getIth(i)));
					else
						retD *= getAs<double>(getAs<TLiteral>(params.getIth(i)));
				}
				if(used == 0)
					return retI;
				else
					return retD;
			}
			break;
		case DIV:
			{
				if(params.size() != 2)
					throw EVermScriptExecError("/ special function takes at least 2 argument");

				TLiteral par1 = getAs<TLiteral>(params.getIth(0));
				int used = isA<int>(par1) ? 0 : 1;

				if(used == 0)
					return getAs<int>(getAs<TLiteral>(params.getIth(0))) / getAs<int>(getAs<TLiteral>(params.getIth(1)));
				else
					return getAs<double>(getAs<TLiteral>(params.getIth(1))) / getAs<double>(getAs<TLiteral>(params.getIth(1)));
			}
			break;
		case MOD:
			{
				if(params.size() != 2)
					throw EVermScriptExecError("% special function takes at least 2 argument");

				return getAs<int>(getAs<TLiteral>(params.getIth(0))) % getAs<int>(getAs<TLiteral>(params.getIth(1)));
			}
			break;
		default:
			throw EInterpreterError("VFunc in forbidden mode!");
			break;
		}
	}


	IntroduceDynamicEnv::IntroduceDynamicEnv(ERMInterpreter * interp_)
		: interp(interp_)
	{
		Environment * nen = new Environment();
		nen->setParent(interp->topDyn);
		interp->topDyn = nen;
	}

	IntroduceDynamicEnv::~IntroduceDynamicEnv()
	{
		Environment * nen = interp->topDyn;
		interp->topDyn = nen->getParent();
		delete nen;
	}

 	bool operator<(const TLiteral & t1, const TLiteral & t2)
 	{
 		if(t1.type() == t2.type())
 		{
 			return boost::apply_visitor(_opLTvis(t1), t2);
 		}
 		throw EVermScriptExecError("These types are incomparable!");
 	}

	bool operator<=(const TLiteral & t1, const TLiteral & t2)
	{
		if(t1.type() == t2.type())
		{
			return boost::apply_visitor(_opLEvis(t1), t2);
		}
		throw EVermScriptExecError("These types are incomparable!");
	}
	bool operator>(const TLiteral & t1, const TLiteral & t2)
	{
		if(t1.type() == t2.type())
		{
			return boost::apply_visitor(_opGTvis(t1), t2);
		}
		throw EVermScriptExecError("These types are incomparable!");
	}
	bool operator>=(const TLiteral & t1, const TLiteral & t2)
	{
		if(t1.type() == t2.type())
		{
			return boost::apply_visitor(_opGEvis(t1), t2);
		}
		throw EVermScriptExecError("These types are incomparable!");
	}

	struct _VLITPrinter : boost::static_visitor<void>
	{
		void operator()(const std::string & par) const
		{
			logGlobal->debug("^%s^", par);
		}
		template<typename T>
		void operator()(const T & par) const
		{
			logGlobal->debug(boost::to_string(par));
		}
	};

	struct _VOPTPrinter : boost::static_visitor<void>
	{
		void operator()(VNIL const& opt) const
		{
			logGlobal->debug("[]");
		}
		void operator()(VNode const& opt) const
		{
			logGlobal->debug("[");
			for(int g=0; g<opt.children.size(); ++g)
			{
				boost::apply_visitor(_VOPTPrinter(), opt.children[g]);
				logGlobal->debug(" ");
			}
			logGlobal->debug("]");
		}
		void operator()(VSymbol const& opt) const
		{
			logGlobal->debug(opt.text);
		}
		void operator()(TLiteral const& opt) const
		{
			boost::apply_visitor(_VLITPrinter(), opt);
		}
		void operator()(ERM::Tcommand const& opt) const
		{
			logGlobal->debug("--erm--");
		}
		void operator()(VFunc const& opt) const
		{
			logGlobal->debug("function");
		}
	};

	void printVOption(const VOption & opt)
	{
		boost::apply_visitor(_VOPTPrinter(), opt);
	}
}
