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


namespace ERMPrinter
{
	//console printer
	using namespace ERM;

	struct VarPrinterVisitor : boost::static_visitor<>
	{
		void operator()(TVarExpNotMacro const& val) const
		{
			logGlobal->debug(val.varsym);
			if(val.val.is_initialized())
			{
				logGlobal->debug("%d", val.val.get());
			}
		}
		void operator()(TMacroUsage const& val) const
		{
			logGlobal->debug("$%s$", val.macro);
		}
	};

	void varPrinter(const TVarExp & var)
	{
		boost::apply_visitor(VarPrinterVisitor(), var);
	}

	struct IExpPrinterVisitor : boost::static_visitor<>
	{
		void operator()(int const & constant) const
		{
			logGlobal->debug("%d", constant);
		}
		void operator()(TVarExp const & var) const
		{
			varPrinter(var);
		}
	};


	void iexpPrinter(const TIexp & exp)
	{
		boost::apply_visitor(IExpPrinterVisitor(), exp);
	}

	struct IdentifierPrinterVisitor : boost::static_visitor<>
	{
		void operator()(TIexp const& iexp) const
		{
			iexpPrinter(iexp);
		}
		void operator()(TArithmeticOp const& arop) const
		{
			iexpPrinter(arop.lhs);
			logGlobal->debug(" %s ", arop.opcode);
			iexpPrinter(arop.rhs);
		}
	};

	void identifierPrinter(const boost::optional<Tidentifier> & id)
	{
		if(id.is_initialized())
		{
			logGlobal->debug("identifier: ");
			for (auto x : id.get())
			{
				logGlobal->debug("#");
				boost::apply_visitor(IdentifierPrinterVisitor(), x);
			}
		}
	}

	struct ConditionCondPrinterVisitor : boost::static_visitor<>
	{
		void operator()(TComparison const& cmp) const
		{
			iexpPrinter(cmp.lhs);
			logGlobal->debug(" %s ", cmp.compSign);
			iexpPrinter(cmp.rhs);
		}
		void operator()(int const& flag) const
		{
			logGlobal->debug("condflag %d", flag);
		}
	};

	void conditionPrinter(const boost::optional<Tcondition> & cond)
	{
		if(cond.is_initialized())
		{
			Tcondition condp = cond.get();
			logGlobal->debug(" condition: ");
			boost::apply_visitor(ConditionCondPrinterVisitor(), condp.cond);
			logGlobal->debug(" cond type: %s", condp.ctype);

			//recursive call
			if(condp.rhs.is_initialized())
			{
				logGlobal->debug("rhs: ");
				boost::optional<Tcondition> rhsc = condp.rhs.get().get();
				conditionPrinter(rhsc);
			}
			else
			{
				logGlobal->debug("no rhs; ");
			}
		}
	}

	struct BodyVarpPrinterVisitor : boost::static_visitor<>
	{
		void operator()(TVarExpNotMacro const& cmp) const
		{
			if(cmp.questionMark.is_initialized())
			{
				logGlobal->debug("%s", cmp.questionMark.get());
			}
			if(cmp.val.is_initialized())
			{
				logGlobal->debug("val:%d",cmp.val.get());
			}
			logGlobal->debug("varsym: |%s|",cmp.varsym);
		}
		void operator()(TMacroUsage const& cmp) const
		{
			logGlobal->debug("???$$%s$$", cmp.macro);
		}
	};

	struct BodyOptionItemPrinterVisitor : boost::static_visitor<>
	{
		void operator()(TVarConcatString const& cmp) const
		{
			logGlobal->debug("+concat\"");
			varPrinter(cmp.var);
			logGlobal->debug(" with %s", cmp.string.str);
		}
		void operator()(TStringConstant const& cmp) const
		{
			logGlobal->debug(" \"%s\" ", cmp.str);
		}
		void operator()(TCurriedString const& cmp) const
		{
			logGlobal->debug("cs: ");
			iexpPrinter(cmp.iexp);
			logGlobal->debug(" '%s' ", cmp.string.str);
		}
		void operator()(TSemiCompare const& cmp) const
		{
			logGlobal->debug("%s; rhs: ", cmp.compSign);
			iexpPrinter(cmp.rhs);
		}
		void operator()(TMacroUsage const& cmp) const
		{
			logGlobal->debug("$$%s$$", cmp.macro);
		}
		void operator()(TMacroDef const& cmp) const
		{
			logGlobal->debug("@@%s@@", cmp.macro);
		}
		void operator()(TIexp const& cmp) const
		{
			iexpPrinter(cmp);
		}
		void operator()(TVarpExp const& cmp) const
		{
			logGlobal->debug("varp");
			boost::apply_visitor(BodyVarpPrinterVisitor(), cmp.var);
		}
		void operator()(spirit::unused_type const& cmp) const
		{
			logGlobal->debug("nothing");
		}
	};

	struct BodyOptionVisitor : boost::static_visitor<>
	{
		void operator()(TVRLogic const& cmp) const
		{
			logGlobal->debug("%s ", cmp.opcode);
			iexpPrinter(cmp.var);
		}
		void operator()(TVRArithmetic const& cmp) const
		{
			logGlobal->debug("%s ", cmp.opcode);
			iexpPrinter(cmp.rhs);
		}
		void operator()(TNormalBodyOption const& cmp) const
		{
			logGlobal->debug("%s~",cmp.optionCode);
			for (auto optList : cmp.params)
			{
				boost::apply_visitor(BodyOptionItemPrinterVisitor(), optList);
			}
		}
	};

	void bodyPrinter(const Tbody & body)
	{
		logGlobal->debug(" body items: ");
		for (auto bi: body)
		{
			logGlobal->debug(" (");
			apply_visitor(BodyOptionVisitor(), bi);
			logGlobal->debug(") ");
		}
	}

	struct CommandPrinterVisitor : boost::static_visitor<>
	{
		void operator()(Ttrigger const& trig) const
		{
			logGlobal->debug("trigger: %s ", trig.name);
			identifierPrinter(trig.identifier);
			conditionPrinter(trig.condition);
		}
		void operator()(Tinstruction const& trig) const
		{
			logGlobal->debug("instruction: %s", trig.name);
			identifierPrinter(trig.identifier);
			conditionPrinter(trig.condition);
			bodyPrinter(trig.body);

		}
		void operator()(Treceiver const& trig) const
		{
			logGlobal->debug("receiver: %s ", trig.name);

			identifierPrinter(trig.identifier);
			conditionPrinter(trig.condition);
			if(trig.body.is_initialized())
				bodyPrinter(trig.body.get());
		}
		void operator()(TPostTrigger const& trig) const
		{
			logGlobal->debug("post trigger: %s ", trig.name);
			identifierPrinter(trig.identifier);
			conditionPrinter(trig.condition);
		}
	};

	struct LinePrinterVisitor : boost::static_visitor<>
	{
		void operator()(Tcommand const& cmd) const
		{
			CommandPrinterVisitor un;
			boost::apply_visitor(un, cmd.cmd);
//			logGlobal->debug("Line comment: %s", cmd.comment);
		}
		void operator()(std::string const& comment) const
		{
		}
		void operator()(spirit::unused_type const& nothing) const
		{
		}
	};

	void printERM(const TERMline & ast)
	{
		logGlobal->debug("");

		boost::apply_visitor(LinePrinterVisitor(), ast);
	}

	void printTVExp(const TVExp & exp);

	struct VOptionPrinterVisitor : boost::static_visitor<>
	{
		void operator()(TVExp const& cmd) const
		{
			printTVExp(cmd);
		}
		void operator()(TSymbol const& cmd) const
		{
			for(auto mod : cmd.symModifier)
			{
				logGlobal->debug(mod);
			}
			logGlobal->debug(cmd.sym);
		}
		void operator()(char const& cmd) const
		{
			logGlobal->debug("'%s'", cmd);
		}
		void operator()(int const& cmd) const
		{
			logGlobal->debug("%d", cmd);
		}
		void operator()(double const& cmd) const
		{
			logGlobal->debug("%f", cmd);
		}
		void operator()(TERMline const& cmd) const
		{
			printERM(cmd);
		}
		void operator()(TStringConstant const& cmd) const
		{
			logGlobal->debug("^%s^", cmd.str);
		}
	};

	void printTVExp(const TVExp & exp)
	{
		for (auto mod: exp.modifier)
		{
			logGlobal->debug("%s ", mod);
		}
		logGlobal->debug("[ ");
		for (auto opt: exp.children)
		{
			boost::apply_visitor(VOptionPrinterVisitor(), opt);
			logGlobal->debug(" ");
		}
		logGlobal->debug("]");
	}

	struct TLPrinterVisitor : boost::static_visitor<>
	{
		void operator()(TVExp const& cmd) const
		{
			printTVExp(cmd);
		}
		void operator()(TERMline const& cmd) const
		{
			printERM(cmd);
		}
	};

	void printAST(const TLine & ast)
	{
		boost::apply_visitor(TLPrinterVisitor(), ast);
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

void ERMInterpreter::printScripts(EPrintMode mode)
{
	std::map< LinePointer, ERM::TLine >::const_iterator prevIt;
	for(std::map< LinePointer, ERM::TLine >::const_iterator it = scripts.begin(); it != scripts.end(); ++it)
	{
		if(it == scripts.begin() || it->first.file != prevIt->first.file)
		{
			logger->debug("----------------- script %s ------------------", it->first.file->filename);
		}

		logger->debug("%d", it->first.realLineNum);
		ERMPrinter::printAST(it->second);
		prevIt = it;
	}
}

void ERMInterpreter::scanScripts()
{
	for(std::map< LinePointer, ERM::TLine >::const_iterator it = scripts.begin(); it != scripts.end(); ++it)
	{
		boost::apply_visitor(ScriptScanner(this, it->first), it->second);
	}
}

ERMInterpreter::ERMInterpreter(vstd::CLoggerBase * logger_)
	: ContextBase(logger_)
{
	curFunc = nullptr;
	curTrigger = nullptr;

	ermGlobalEnv = new ERMEnvironment();
	globalEnv = new Environment();

	topDyn = globalEnv;

	acb = nullptr;
	icb = nullptr;
	bicb = nullptr;
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

template<typename T>
struct StandardReceiverVisitor : boost::static_visitor<>
{
	ERMInterpreter * interp;
	T identifier;
	StandardReceiverVisitor(ERMInterpreter * _interpr, T ident) : interp(_interpr), identifier(ident)
	{}

	virtual void operator()(TVRLogic const& trig) const
	{
		throw EScriptExecError("VR logic not allowed in this receiver!");
	}
	virtual void operator()(TVRArithmetic const& trig) const
	{
		throw EScriptExecError("VR arithmetic not allowed in this receiver!");
	}
	virtual void operator()(TNormalBodyOption const& trig) const = 0;

	template<typename OptionPerformer>
	void performOptionTakingOneParamter(const ERM::TNormalBodyOptionList & params) const
	{
		if(params.size() == 1)
		{
			ERM::TBodyOptionItem boi = params[0];
			boost::apply_visitor(
				OptionPerformer(*const_cast<typename OptionPerformer::TReceiverType*>(static_cast<const typename OptionPerformer::TReceiverType*>(this))), boi);
		}
		else
			throw EScriptExecError("This receiver option takes exactly 1 parameter!");
	}

	template<template <int opcode> class OptionPerformer>
	void performOptionTakingOneParamterWithIntDispatcher(const ERM::TNormalBodyOptionList & params) const
	{
		if(params.size() == 2)
		{
			int optNum = interp->getIexp(params[0]).getInt();

			ERM::TBodyOptionItem boi = params[1];
			switch(optNum)
			{
			case 0:
				boost::apply_visitor(
					OptionPerformer<0>(*const_cast<typename OptionPerformer<0>::TReceiverType*>(static_cast<const typename OptionPerformer<0>::TReceiverType*>(this))), boi);
				break;
			default:
				throw EScriptExecError("Wrong number of option code!");
				break;
			}
		}
		else
			throw EScriptExecError("This receiver option takes exactly 2 parameters!");
	}
};
////HE
struct HEPerformer;

template<int opcode>
struct HE_BPerformer : StandardBodyOptionItemVisitor<HEPerformer>
{
	explicit HE_BPerformer(HEPerformer & _owner) : StandardBodyOptionItemVisitor<HEPerformer>(_owner)
	{}
	using StandardBodyOptionItemVisitor<HEPerformer>::operator();

	void operator()(TIexp const& cmp) const override;
	void operator()(TVarpExp const& cmp) const override;
};

template<int opcode>
void HE_BPerformer<opcode>::operator()( TIexp const& cmp ) const
{
	throw EScriptExecError("Setting hero name is not implemented!");
}

template<int opcode>
struct HE_CPerformer : StandardBodyOptionItemVisitor<HEPerformer>
{
	explicit HE_CPerformer(HEPerformer & _owner) : StandardBodyOptionItemVisitor<HEPerformer>(_owner)
	{}
	using StandardBodyOptionItemVisitor<HEPerformer>::operator();

	void operator()(TIexp const& cmp) const override;
	void operator()(TVarpExp const& cmp) const override;
};

template<int opcode>
void HE_CPerformer<opcode>::operator()( TIexp const& cmp ) const
{
	throw EScriptExecError("Setting hero army is not implemented!");
}

struct HEPerformer : StandardReceiverVisitor<const CGHeroInstance *>
{
	HEPerformer(ERMInterpreter * _interpr, const CGHeroInstance * hero) : StandardReceiverVisitor<const CGHeroInstance *>(_interpr, hero)
	{}
	using StandardReceiverVisitor<const CGHeroInstance *>::operator();

	void operator()(TNormalBodyOption const& trig) const override
	{
		switch(trig.optionCode)
		{
		case 'B':
			{
				performOptionTakingOneParamterWithIntDispatcher<HE_BPerformer>(trig.params);
			}
			break;
		case 'C':
			{
				const ERM::TNormalBodyOptionList & params = trig.params;
				if(params.size() == 4)
				{
					if(interp->getIexp(params[0]).getInt() == 0)
					{
						SlotID slot = SlotID(interp->getIexp(params[1]).getInt());
						const CStackInstance *stack = identifier->getStackPtr(slot);
						if(params[2].which() == 6) //varp
						{
							IexpValStr lhs = interp->getIexp(boost::get<ERM::TVarpExp>(params[2]));
							if(stack)
								lhs.setTo(stack->getCreatureID());
							else
								lhs.setTo(-1);
						}
						else
							throw EScriptExecError("Setting stack creature type is not implemented!");

						if(params[3].which() == 6) //varp
						{
							interp->getIexp(boost::get<ERM::TVarpExp>(params[3])).setTo(identifier->getStackCount(SlotID(slot)));
						}
						else
							throw EScriptExecError("Setting stack count is not implemented!");
					}
					else
						throw EScriptExecError("Slot number must be an evaluable i-exp");
				}
				//todo else if(14 params)
				else
					throw EScriptExecError("Slot number must be an evaluable i-exp");
			}
			break;
		case 'E':
			break;
		case 'N':
			break;
		default:
			break;
		}
	}

};
struct IFPerformer;

struct IF_MPerformer : StandardBodyOptionItemVisitor<IFPerformer>
{
	explicit IF_MPerformer(IFPerformer & _owner) : StandardBodyOptionItemVisitor<IFPerformer>(_owner){}
	using StandardBodyOptionItemVisitor<IFPerformer>::operator();

	void operator()(TStringConstant const& cmp) const override;
};


//according to the ERM help:
//"%%" -> "%"
//"%V#" -> current value of # flag.
//"%Vf"..."%Vt" -> current value of corresponding variable.
//"%W1"..."%W100" -> current value of corresponding hero variable.
//"%X1"..."%X16" -> current value of corresponding function parameter.
//"%Y1"..."%Y100" -> current value of corresponding local variable.
//"%Z1"..."%Z500" -> current value of corresponding string variable.
//"%$macro$" -> macro name of corresponding variable
//"%Dd" -> current day of week
//"%Dw" -> current week
//"%Dm" -> current month
//"%Da" -> current day from beginning of the game
//"%Gc" -> the color of current gamer in text
struct StringFormatter
{
	ERMInterpreter * interpr;
	int pos;
	int tokenLength;
	size_t percentPos;
	int charsToReplace;
	std::string &msg;

	StringFormatter(ERMInterpreter * _interpr, std::string &MSG)
		: interpr(_interpr), pos(0), msg(MSG)
	{}

	static void format(ERMInterpreter * _interpr, std::string &msg)
	{
		StringFormatter sf(_interpr, msg);
		sf.format();
	}

	// startpos is the first digit
	// digits will be converted to number and returned
	// ADDITIVE on digitsUsed
	int getNum()
	{
		int toAdd = 0;
		int numStart = percentPos + 2;
		size_t numEnd = msg.find_first_not_of("1234567890", numStart);

		if(numEnd == std::string::npos)
			toAdd = msg.size() - numStart;
		else
			toAdd = numEnd - numStart;

		charsToReplace += toAdd;
		return boost::lexical_cast<int>(msg.substr(numStart, toAdd));
	}

	void format()
	{
		while(pos < msg.size())
		{
			percentPos = msg.find_first_of('%', pos);
			charsToReplace = 1; //at least the same '%' needs to be replaced

			std::ostringstream replaceWithWhat;

			if(percentPos == std::string::npos) //processing done?
				break;

			if(percentPos + 1 >= msg.size()) //at least one character after % is required
				throw EScriptExecError("Formatting error: % at the end of string!");

			charsToReplace++; //the sign after % is consumed
			switch(msg[percentPos+1])
			{
			case '%':
				replaceWithWhat << '%';
				break;
			case 'F':
				replaceWithWhat << interpr->ermGlobalEnv->getFlag(getNum());
				break;
			case 'V':
				if(std::isdigit(msg[percentPos + 2]))
					replaceWithWhat << interpr->ermGlobalEnv->getStandardVar(getNum());
				else
				{
					charsToReplace++;
					replaceWithWhat << interpr->ermGlobalEnv->getQuickVar(msg[percentPos+2]);
				}
				break;
			case 'X':
				replaceWithWhat << interpr->getVar("x", getNum()).getInt();
				break;
			case 'Z':
				replaceWithWhat << interpr->ermGlobalEnv->getZVar(getNum());
				break;
			default:
				throw EScriptExecError("Formatting error: unrecognized token indicator after %!");
			}
			msg.replace(percentPos, charsToReplace, replaceWithWhat.str());
			pos = percentPos + 1;
		}
	}
};

struct IFPerformer : StandardReceiverVisitor<TUnusedType>
{
	IFPerformer(ERMInterpreter * _interpr) : StandardReceiverVisitor<TUnusedType>(_interpr, 0)
	{}
	using StandardReceiverVisitor<TUnusedType>::operator();


	void operator()(TNormalBodyOption const& trig) const override
	{
		switch(trig.optionCode)
		{
		case 'M': //Show the message (Text) or contents of z$ variable on the screen immediately.
			performOptionTakingOneParamter<IF_MPerformer>(trig.params);
			break;
		default:
			break;
		}
	}

	void showMessage(const std::string &msg)
	{
		std::string msgToFormat = msg;
		StringFormatter::format(interp, msgToFormat);
		interp->showInfoDialog(msgToFormat, interp->icb->getLocalPlayer());
	}
};

void IF_MPerformer::operator()(TStringConstant const& cmp) const
{
	owner.showMessage(cmp.str);
}

template<int opcode>
void HE_BPerformer<opcode>::operator()( TVarpExp const& cmp ) const
{
	owner.interp->getIexp(cmp).setTo(owner.identifier->name);
}

template<int opcode>
void HE_CPerformer<opcode>::operator()( TVarpExp const& cmp ) const
{
	owner.interp->getIexp(cmp).setTo(owner.identifier->name);
}

////MA
struct MAPerformer;
struct MA_PPerformer : StandardBodyOptionItemVisitor<MAPerformer>
{
	explicit MA_PPerformer(MAPerformer & _owner);
	using StandardBodyOptionItemVisitor<MAPerformer>::operator();

	void operator()(TIexp const& cmp) const override;
	void operator()(TVarpExp const& cmp) const override;
};

struct MAPerformer : StandardReceiverVisitor<TUnusedType>
{
	MAPerformer(ERMInterpreter * _interpr) : StandardReceiverVisitor<TUnusedType>(_interpr, 0)
	{}
	using StandardReceiverVisitor<TUnusedType>::operator();

	void operator()(TNormalBodyOption const& trig) const override
	{
		switch(trig.optionCode)
		{
		case 'A': //sgc monster attack
			break;
		case 'B': //spell?
			break;
		case 'P': //hit points
			{
				//TODO
			}
			break;
		default:
			break;
		}
	}

};

void MA_PPerformer::operator()( TIexp const& cmp ) const
{

}

void MA_PPerformer::operator()( TVarpExp const& cmp ) const
{

}

////MO

struct MOPerformer;
struct MO_GPerformer : StandardBodyOptionItemVisitor<MOPerformer>
{
	explicit MO_GPerformer(MOPerformer & _owner) : StandardBodyOptionItemVisitor<MOPerformer>(_owner)
	{}
	using StandardBodyOptionItemVisitor<MOPerformer>::operator();

	void operator()(TVarpExp const& cmp) const override;
	void operator()(TIexp const& cmp) const override;
};

struct MOPerformer: StandardReceiverVisitor<int3>
{
	MOPerformer(ERMInterpreter * _interpr, int3 pos) : StandardReceiverVisitor<int3>(_interpr, pos)
	{}
	using StandardReceiverVisitor<int3>::operator();

	void operator()(TNormalBodyOption const& trig) const override
	{
		switch(trig.optionCode)
		{
		case 'G':
			{
				performOptionTakingOneParamter<MO_GPerformer>(trig.params);
			}
			break;
		default:
			break;
		}
	}
};

void MO_GPerformer::operator()( TIexp const& cmp ) const
{
	throw EScriptExecError("Setting monster count is not implemented yet!");
}

void MO_GPerformer::operator()( TVarpExp const& cmp ) const
{
	const CGCreature *cre = owner.interp->getObjFromAs<CGCreature>(owner.identifier);
	owner.interp->getIexp(cmp).setTo(cre->getStackCount(SlotID(0)));
}


struct ConditionDisemboweler;
//OB
struct OBPerformer;
struct OB_UPerformer : StandardBodyOptionItemVisitor<OBPerformer>
{
	explicit OB_UPerformer(OBPerformer & owner) : StandardBodyOptionItemVisitor<OBPerformer>(owner)
	{}
	using StandardBodyOptionItemVisitor<OBPerformer>::operator();

	virtual void operator()(TIexp const& cmp) const;
	virtual void operator()(TVarpExp const& cmp) const;
};

struct OBPerformer : StandardReceiverVisitor<int3>
{
	OBPerformer(ERMInterpreter * _interpr, int3 objPos) : StandardReceiverVisitor<int3>(_interpr, objPos)
	{}
	using StandardReceiverVisitor<int3>::operator(); //it removes compilation error... not sure why it *must* be here
	void operator()(TNormalBodyOption const& trig) const
	{
		switch(trig.optionCode)
		{
		case 'B': //removes description hint
			{
				//TODO
			}
			break;
		case 'C': //sgc of control word of object
			{
				//TODO
			}
			break;
		case 'D': //disable gamer to use object
			{
				//TODO
			}
			break;
		case 'E': //enable gamer to use object
			{
				//TODO
			}
			break;
		case 'H': //replace hint for object
			{
				//TODO
			}
			break;
		case 'M': //disabling messages and questions
			{
				//TODO
			}
			break;
		case 'R': //enable all gamers to use object
			{
				//TODO
			}
			break;
		case 'S': //disable all gamers to use object
			{
				//TODO
			}
			break;
		case 'T': //sgc of obj type
			{
				//TODO
			}
			break;
		case 'U': //sgc of obj subtype
			{
				performOptionTakingOneParamter<OB_UPerformer>(trig.params);
			}
			break;
		default:
			throw EScriptExecError("Wrong OB receiver option!");
			break;
		}
	}
};

void OB_UPerformer::operator()( TIexp const& cmp ) const
{
	IexpValStr val = owner.interp->getIexp(cmp);
	throw EScriptExecError("Setting subID is not implemented yet!");
}

void OB_UPerformer::operator()( TVarpExp const& cmp ) const
{
	IexpValStr val = owner.interp->getIexp(cmp);
	val.setTo(owner.interp->getObjFrom(owner.identifier)->subID);
}

/////VR
struct VRPerformer;
struct VR_SPerformer : StandardBodyOptionItemVisitor<VRPerformer>
{
	explicit VR_SPerformer(VRPerformer & _owner);
	using StandardBodyOptionItemVisitor<VRPerformer>::operator();

	void operator()(TStringConstant const& cmp) const override;
	void operator()(TIexp const& cmp) const override;
};

struct VRPerformer : StandardReceiverVisitor<IexpValStr>
{
	VRPerformer(ERMInterpreter * _interpr, IexpValStr ident) : StandardReceiverVisitor<IexpValStr>(_interpr, ident)
	{}

	void operator()(TVRLogic const& trig) const override
	{
		int valr = interp->getIexp(trig.var).getInt();
		switch (trig.opcode)
		{
		case '&':
			const_cast<VRPerformer*>(this)->identifier.setTo(identifier.getInt() & valr);
			break;
		case '|':
			const_cast<VRPerformer*>(this)->identifier.setTo(identifier.getInt() | valr);
			break;
		case 'X':
			const_cast<VRPerformer*>(this)->identifier.setTo(identifier.getInt() ^ valr);
			break;
		default:
			throw EInterpreterError("Wrong opcode in VR logic expression!");
			break;
		}
	}
	void operator()(TVRArithmetic const& trig) const override
	{
		IexpValStr rhs = interp->getIexp(trig.rhs);
		switch (trig.opcode)
		{
		case '+':
			const_cast<VRPerformer*>(this)->identifier += rhs;
			break;
		case '-':
			const_cast<VRPerformer*>(this)->identifier -= rhs;
			break;
		case '*':
			const_cast<VRPerformer*>(this)->identifier *= rhs;
			break;
		case ':':
			const_cast<VRPerformer*>(this)->identifier /= rhs;
			break;
		case '%':
			const_cast<VRPerformer*>(this)->identifier %= rhs;
			break;
		default:
			throw EInterpreterError("Wrong opcode in VR arithmetic!");
			break;
		}
	}
	void operator()(TNormalBodyOption const& trig) const override
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
				performOptionTakingOneParamter<VR_SPerformer>(trig.params);
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


VR_SPerformer::VR_SPerformer(VRPerformer & _owner) : StandardBodyOptionItemVisitor<VRPerformer>(_owner)
{}

void VR_SPerformer::operator()(ERM::TIexp const& trig) const
{
	owner.identifier.setTo(owner.interp->getIexp(trig));
}
void VR_SPerformer::operator()(TStringConstant const& cmp) const
{
	owner.identifier.setTo(cmp.str);
}

/////

struct ERMExpDispatch : boost::static_visitor<>
{
	struct HLP
	{
		ERMInterpreter * interp;

		HLP(ERMInterpreter * _interp)
			: interp(_interp)
		{}

		int3 getPosFromIdentifier(ERM::Tidentifier tid, bool allowDummyFourth)
		{
			switch(tid.size())
			{
			case 1:
				{
					int num = interp->getIexp(tid[0]).getInt();
					return int3(interp->ermGlobalEnv->getStandardVar(num),
						interp->ermGlobalEnv->getStandardVar(num+1),
						interp->ermGlobalEnv->getStandardVar(num+2));
				}
				break;
			case 3:
			case 4:
				if(tid.size() == 4 && !allowDummyFourth)
					throw EScriptExecError("4 items in identifier are not allowed for this receiver!");

				return int3(interp->getIexp(tid[0]).getInt(),
					interp->getIexp(tid[1]).getInt(),
					interp->getIexp(tid[2]).getInt());
				break;
			default:
				throw EScriptExecError("This receiver takes 1 or 3 items in identifier!");
				break;
			}
		}
		template <typename Visitor>
		void performBody(const boost::optional<ERM::Tbody> & body, const Visitor& visitor)
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
	};

	ERMInterpreter * interp;

	ERMExpDispatch(ERMInterpreter * interp_)
		: interp(interp_)
	{
	}

	void operator()(Ttrigger const& trig) const
	{
		throw EInterpreterError("Triggers cannot be executed!");
	}
	void operator()(Tinstruction const& trig) const
	{
	}
	void operator()(Treceiver const& trig) const
	{
		HLP helper(interp);
		//check condition
		if(trig.condition.is_initialized())
		{
			if( !interp->checkCondition(trig.condition.get()) )
				return;
		}

		if(trig.name == "VR")
		{
			//perform operations
			if(trig.identifier.is_initialized())
			{
				ERM::Tidentifier ident = trig.identifier.get();
				if(ident.size() == 1)
				{
					IexpValStr ievs = interp->getIexp(ident[0]);

					//see body
					helper.performBody(trig.body, VRPerformer(interp, ievs));
				}
				else
					throw EScriptExecError("VR receiver must be used with exactly one identifier item!");
			}
			else
				throw EScriptExecError("VR receiver must be used with an identifier!");
		}
		else if(trig.name == "DO")
		{
			//perform operations
			if(trig.identifier.is_initialized())
			{
				ERM::Tidentifier tid = trig.identifier.get();
				if(tid.size() != 4)
				{
					throw EScriptExecError("DO receiver takes exactly 4 arguments");
				}
				int funNum = interp->getIexp(tid[0]).getInt(),
					startVal = interp->getIexp(tid[1]).getInt(),
					stopVal = interp->getIexp(tid[2]).getInt(),
					increment = interp->getIexp(tid[3]).getInt();

				for(int it = startVal; it < stopVal; it += increment)
				{
					std::vector<int> params(FunctionLocalVars::NUM_PARAMETERS, 0);
					params.back() = it;
					//owner->getFuncVars(funNum)->getParam(16) = it;

					std::vector<int> v1;
					v1.push_back(funNum);
					ERMInterpreter::TIDPattern tip = {{v1.size(), v1}};
					interp->executeTriggerType(TriggerType("FU"), true, tip, params);
					it = interp->getFuncVars(funNum)->getParam(16);
				}
			}
		}
		else if(trig.name == "MA")
		{
			if(trig.identifier.is_initialized())
			{
				throw EScriptExecError("MA receiver doesn't take the identifier!");
			}
			helper.performBody(trig.body, MAPerformer(interp));
		}
		else if(trig.name == "MO")
		{
			int3 objPos;
			if(trig.identifier.is_initialized())
			{
				ERM::Tidentifier tid = trig.identifier.get();
				objPos = HLP(interp).getPosFromIdentifier(tid, true);

				helper.performBody(trig.body, MOPerformer(interp, objPos));
			}
			else
				throw EScriptExecError("MO receiver must have an identifier!");
		}
		else if(trig.name == "OB")
		{
			int3 objPos;
			if(trig.identifier.is_initialized())
			{
				ERM::Tidentifier tid = trig.identifier.get();
				objPos = HLP(interp).getPosFromIdentifier(tid, false);

				helper.performBody(trig.body, OBPerformer(interp, objPos));
			}
			else
				throw EScriptExecError("OB receiver must have an identifier!");
		}
		else if(trig.name == "HE")
		{
			if(trig.identifier.is_initialized())
			{
				const CGHeroInstance * hero = nullptr;
				ERM::Tidentifier tid = trig.identifier.get();
				switch(tid.size())
				{
				case 1:
					{
						int heroNum = interp->getIexp(tid[0]).getInt();
						if(heroNum < 0)
							throw EScriptExecError("Indirect hero selection is not implemented!");
						else
							hero = interp->icb->getHeroWithSubid(heroNum);

					}
					break;
				case 3:
					{
						int3 pos = helper.getPosFromIdentifier(tid, false);
						hero = interp->getObjFromAs<CGHeroInstance>(pos);
					}
					break;
				default:
					throw EScriptExecError("HE receiver takes 1 or 3 items in identifier");
					break;
				}
				helper.performBody(trig.body, HEPerformer(interp, hero));
			}
			else
				throw EScriptExecError("HE receiver must have an identifier!");
		}
		else if(trig.name == "IF")
		{
			helper.performBody(trig.body, IFPerformer(interp));
		}
		else
		{
			interp->logger->warn("%s receiver is not supported yet, doing nothing...", trig.name);
			//not supported or invalid trigger
		}
	}
	void operator()(TPostTrigger const& trig) const
	{
		throw EInterpreterError("Post-triggers cannot be executed!");
	}
};

struct CommandExec : boost::static_visitor<>
{
	ERMInterpreter * interp;

	CommandExec(ERMInterpreter * interp_)
		:interp(interp_)
	{
	}

	void operator()(Tcommand const& cmd) const
	{
		boost::apply_visitor(ERMExpDispatch(interp), cmd.cmd);
//		logGlobal->debug("Line comment: %s", cmd.comment);
	}
	void operator()(std::string const& comment) const
	{
		//comment - do nothing
	}
	void operator()(spirit::unused_type const& nothing) const
	{
		//nothing - do nothing
	}
};

struct LineExec : boost::static_visitor<>
{
	ERMInterpreter * interp;

	LineExec(ERMInterpreter * interp_)
		:interp(interp_)
	{
	}

	void operator()(TVExp const& cmd) const
	{
		VNode line(cmd);
		interp->eval(line);
	}
	void operator()(TERMline const& cmd) const
	{
		boost::apply_visitor(CommandExec(interp), cmd);
	}
};

/////////

void ERMInterpreter::executeLine( const LinePointer & lp )
{
	logger->trace("Executing line %d (internal %d) from %s", getRealLine(lp), lp.lineNum, lp.file->filename);
	executeLine(scripts[lp]);
}

void ERMInterpreter::executeLine(const ERM::TLine &line)
{
	boost::apply_visitor(LineExec(this), line);
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

namespace IexpDisemboweler
{
	enum EDir{GET, SET};
}

struct LVL2IexpDisemboweler : boost::static_visitor<IexpValStr>
{
	/*const*/ ERMInterpreter * env;
	IexpDisemboweler::EDir dir;

	LVL2IexpDisemboweler(/*const*/ ERMInterpreter * _env, IexpDisemboweler::EDir _dir)
		: env(_env), dir(_dir) //writes value to given var
	{}

	IexpValStr processNotMacro(const TVarExpNotMacro & val) const
	{
		if(val.questionMark.is_initialized())
			throw EIexpProblem("Question marks ('?') are not allowed in getter i-expressions");

		//const-cast just to do some code-reuse...
		return env->getVar(val.varsym, val.val);

	}

	IexpValStr operator()(TVarExpNotMacro const& val) const
	{
		return processNotMacro(val);
	}
	IexpValStr operator()(TMacroUsage const& val) const
	{
		return env->getIexp(val);
	}
};

struct LVL1IexpDisemboweler : boost::static_visitor<IexpValStr>
{
	/*const*/ ERMInterpreter * env;
	IexpDisemboweler::EDir dir;

	LVL1IexpDisemboweler(/*const*/ ERMInterpreter * _env, IexpDisemboweler::EDir _dir)
		: env(_env), dir(_dir) //writes value to given var
	{}
	IexpValStr operator()(int const & constant) const
	{
		if(dir == IexpDisemboweler::GET)
		{
			return IexpValStr(constant);
		}
		else
		{
			throw EIexpProblem("Cannot set a constant!");
		}
	}
	IexpValStr operator()(TVarExp const & var) const
	{
		return boost::apply_visitor(LVL2IexpDisemboweler(env, dir), var);
	}
};

IexpValStr ERMInterpreter::getIexp( const ERM::TIexp & iexp ) const
{
	return boost::apply_visitor(LVL1IexpDisemboweler(const_cast<ERMInterpreter*>(this), IexpDisemboweler::GET), iexp);
}

IexpValStr ERMInterpreter::getIexp( const ERM::TMacroUsage & macro ) const
{
	std::map<std::string, ERM::TVarExpNotMacro>::const_iterator it =
		ermGlobalEnv->macroBindings.find(macro.macro);
	if(it == ermGlobalEnv->macroBindings.end())
		throw EUsageOfUndefinedMacro(macro.macro);

	return getVar(it->second.varsym, it->second.val);
}

IexpValStr ERMInterpreter::getIexp( const ERM::TIdentifierInternal & tid ) const
{
	if(tid.which() == 0)
	{
		return getIexp(boost::get<ERM::TIexp>(tid));
	}
	else
		throw EScriptExecError("Identifier must be a valid i-expression to perform this operation!");
}

IexpValStr ERMInterpreter::getIexp( const ERM::TVarpExp & tid ) const
{
	return boost::apply_visitor(LVL2IexpDisemboweler(const_cast<ERMInterpreter*>(this), IexpDisemboweler::GET), tid.var);
}

struct LVL3BodyOptionItemVisitor : StandardBodyOptionItemVisitor<IexpValStr>
{
	const ERMInterpreter * env;

	explicit LVL3BodyOptionItemVisitor(const ERMInterpreter * env_, IexpValStr & _owner)
		: StandardBodyOptionItemVisitor<IexpValStr>(_owner),
		env(env_)
	{}
	using StandardBodyOptionItemVisitor<IexpValStr>::operator();

	void operator()(TIexp const& cmp) const override
	{
		owner = env->getIexp(cmp);
	}
	void operator()(TVarpExp const& cmp) const override
	{
		owner = env->getIexp(cmp);
	}
};

IexpValStr ERMInterpreter::getIexp( const ERM::TBodyOptionItem & opit ) const
{
	IexpValStr ret;
	boost::apply_visitor(LVL3BodyOptionItemVisitor(this, ret), opit);
	return ret;
}

void ERMInterpreter::executeTriggerType(VERMInterpreter::TriggerType tt, bool pre, const TIDPattern & identifier, const std::vector<int> &funParams)
{
	struct HLP
	{
		static int calcFunNum(VERMInterpreter::TriggerType tt, const TIDPattern & identifier)
		{
			if(tt.type != VERMInterpreter::TriggerType::FU)
				return -1;

			return identifier.begin()->second[0];
		}
	};
	TtriggerListType & triggerList = pre ? triggers : postTriggers;

	TriggerIdentifierMatch tim;
	tim.allowNoIdetifier = true;
	tim.ermEnv = this;
	tim.matchToIt = identifier;
	std::vector<Trigger> & triggersToTry = triggerList[tt];
	for(int g=0; g<triggersToTry.size(); ++g)
	{
		if(tim.tryMatch(&triggersToTry[g]))
		{
			curTrigger = &triggersToTry[g];
			executeTrigger(triggersToTry[g], HLP::calcFunNum(tt, identifier), funParams);
		}
	}
}

void ERMInterpreter::executeTriggerType(const char *trigger, int id)
{
	TIDPattern tip;
	tip[1] = std::vector<int>(1, id);
	executeTriggerType(VERMInterpreter::TriggerType(trigger), true, tip);
}

void ERMInterpreter::executeTriggerType(const char *trigger)
{
	executeTriggerType(VERMInterpreter::TriggerType(trigger), true, TIDPattern());
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

template<typename T>
bool compareExp(const T & lhs, const T & rhs, std::string op)
{
	if(op == "<")
	{
		return lhs < rhs;
	}
	else if(op == ">")
	{
		return lhs > rhs;
	}
	else if(op == ">=" || op == "=>")
	{
		return lhs >= rhs;
	}
	else if(op == "<=" || op == "=<")
	{
		return lhs <= rhs;
	}
	else if(op == "==")
	{
		return lhs == rhs;
	}
	else if(op == "<>" || op == "><")
	{
		return lhs != rhs;
	}
	else
		throw EScriptExecError(std::string("Wrong comparison sign: ") + op);
}

struct ConditionDisemboweler : boost::static_visitor<bool>
{
	ConditionDisemboweler(ERMInterpreter * _ei) : ei(_ei)
	{}

	bool operator()(TComparison const & cmp) const
	{
		IexpValStr lhs = ei->getIexp(cmp.lhs),
			rhs = ei->getIexp(cmp.rhs);
		switch (lhs.type)
		{
		case IexpValStr::FLOATVAR:
			switch (rhs.type)
			{
			case IexpValStr::FLOATVAR:
				return compareExp(lhs.getFloat(), rhs.getFloat(), cmp.compSign);
				break;
			default:
				throw EScriptExecError("Incompatible types for comparison");
			}
			break;
		case IexpValStr::INT:
		case IexpValStr::INTVAR:
			switch (rhs.type)
			{
			case IexpValStr::INT:
			case IexpValStr::INTVAR:
				return compareExp(lhs.getInt(), rhs.getInt(), cmp.compSign);
				break;
			default:
				throw EScriptExecError("Incompatible types for comparison");
			}
			break;
		case IexpValStr::STRINGVAR:
			switch (rhs.type)
			{
			case IexpValStr::STRINGVAR:
				return compareExp(lhs.getString(), rhs.getString(), cmp.compSign);
				break;
			default:
				throw EScriptExecError("Incompatible types for comparison");
			}
			break;
		default:
			throw EScriptExecError("Wrong type of left iexp!");
		}
		return false;//we should never reach this place
	}
	bool operator()(int const & flag) const
	{
		return ei->ermGlobalEnv->getFlag(flag);
	}
private:
	ERMInterpreter * ei;
};

bool ERMInterpreter::checkCondition( ERM::Tcondition cond )
{
	bool ret = boost::apply_visitor(ConditionDisemboweler(this), cond.cond);
	if(cond.rhs.is_initialized())
	{ //taking care of rhs expression
		bool rhs = checkCondition(cond.rhs.get().get());
		switch (cond.ctype)
		{
		case '&':
			ret &= rhs;
			break;
		case '|':
			ret |= rhs;
			break;
		case 'X':
			ret ^= rhs;
			break;
		default:
			throw EInterpreterProblem(std::string("Strange - wrong condition connection (") + cond.ctype + ") !");
			break;
		}
	}

	return ret;
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

void ERMInterpreter::setCurrentlyVisitedObj( int3 pos )
{
	ermGlobalEnv->getStandardVar(998) = pos.x;
	ermGlobalEnv->getStandardVar(999) = pos.y;
	ermGlobalEnv->getStandardVar(1000) = pos.z;
}

const std::string ERMInterpreter::triggerSymbol = "trigger";
const std::string ERMInterpreter::postTriggerSymbol = "postTrigger";
const std::string ERMInterpreter::defunSymbol = "defun";


struct TriggerIdMatchHelper : boost::static_visitor<>
{
	int & ret;
	ERMInterpreter * interpreter;
	Trigger * trig;

	TriggerIdMatchHelper(int & b, ERMInterpreter * ermint, Trigger * _trig)
	: ret(b), interpreter(ermint), trig(_trig)
	{}

	void operator()(TIexp const& iexp) const
	{
		IexpValStr val = interpreter->getIexp(iexp);
		switch(val.type)
		{
		case IexpValStr::INT:
		case IexpValStr::INTVAR:
			ret = val.getInt();
			break;
		default:
			throw EScriptExecError("Incompatible i-exp type!");
			break;
		}
	}
	void operator()(TArithmeticOp const& arop) const
	{
		//error?!?
	}
};

bool TriggerIdentifierMatch::tryMatch( Trigger * interptrig ) const
{
	bool ret = true;

	const ERM::TTriggerBase & trig = ERMInterpreter::retrieveTrigger(ermEnv->retrieveLine(interptrig->line));
	if(trig.identifier.is_initialized())
	{

		ERM::Tidentifier tid = trig.identifier.get();
		std::map< int, std::vector<int> >::const_iterator it = matchToIt.find(tid.size());
		if(it == matchToIt.end())
			ret = false;
		else
		{
			const std::vector<int> & pattern = it->second;
			for(int g=0; g<pattern.size(); ++g)
			{
				int val = -1;
				boost::apply_visitor(TriggerIdMatchHelper(val, ermEnv, interptrig), tid[g]);
				if(pattern[g] != val)
				{
					ret = false;
				}
			}
		}
	}
	else
	{
		ret = allowNoIdetifier;
	}

	//check condition
	if(ret)
	{
		if(trig.condition.is_initialized())
		{
			return ermEnv->checkCondition(trig.condition.get());
		}
		else //no condition
			return true;
	}
	else
		return false;
}

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

class TLiteralToJson : public boost::static_visitor<JsonNode>
{
public:
	JsonNode operator()(char const & val) const
	{
		std::string tmp;
		tmp.assign(1, val);
		return JsonUtils::stringNode(tmp);
	}

	JsonNode operator()(double const & val) const
	{
		return JsonUtils::floatNode(val);
	}

	JsonNode operator()(ERM::ValType const & val) const
	{
		return JsonUtils::intNode(val);
	}

	JsonNode operator()(std::string const & val) const
	{
		return JsonUtils::stringNode(val);
	}
};

class VOptionToJson : public boost::static_visitor<JsonNode>
{
public:
	JsonNode operator()(VNIL const & opt) const
	{
		return JsonNode();
	}
	JsonNode operator()(VNode const & opt) const
	{
		JsonNode ret(JsonNode::JsonType::DATA_VECTOR);

		for(auto & item : opt.children)
			ret.Vector().push_back(boost::apply_visitor(VOptionToJson(), item));

		return ret;
	}
	JsonNode operator()(VSymbol const & opt) const
	{
		return JsonNode();
	}
	JsonNode operator()(TLiteral const & opt) const
	{
		return boost::apply_visitor(TLiteralToJson(), opt);
	}
	JsonNode operator()(ERM::Tcommand const & opt) const
	{
		return JsonNode();
	}
	JsonNode operator()(VFunc const & opt) const
	{
		return JsonNode();
	}
};

class JsonToVOption
{
public:
	bool outer;
	JsonToVOption(bool outer_)
	:outer(outer_)
	{}


	VOption convert(const JsonNode & source)
	{
		switch(source.getType())
		{
		case JsonNode::JsonType::DATA_INTEGER:
			{
				TLiteral vval = static_cast<int>(source.Integer());

				if(outer)
				{
					VOptionList box;
					box.push_back(VSymbol("backquote"));
					box.push_back(vval);
					return VOption(VNode(box));
				}
				else
				{
					return VOption(vval);
				}
			}
			break;
		case JsonNode::JsonType::DATA_STRING:
			{
				TLiteral vval = source.String();
				if(outer)
				{
					VOptionList box;
					box.push_back(VSymbol("backquote"));
					box.push_back(vval);
					return VOption(VNode(box));
				}
				else
				{
					return VOption(vval);
				}
			}
			break;
		case JsonNode::JsonType::DATA_VECTOR:
			{
				if(outer)
				{
					VOptionList box;
					box.emplace_back(VSymbol("backquote"));

					VOptionList vval;

					for(const JsonNode & elem : source.Vector())
					{
						vval.push_back(JsonToVOption(false).convert(elem));
					}

					box.emplace_back(VNode(vval));

					return VOption(VNode(box));
				}
				else
				{
					VOptionList vval;

					for(const JsonNode & elem : source.Vector())
					{
						vval.push_back(JsonToVOption(false).convert(elem));
					}
					return VOption(VNode(vval));
				}
			}
			break;
		default:
			return VNIL();
			break;
		}
	}
};

JsonNode ERMInterpreter::callGlobal(const std::string & name, const JsonNode & parameters)
{
	try
	{
		if(name.size()>4 && name[0] == '!' && name[1] == '!')
		{
			std::string trig = name.substr(2,2);
			std::string numStr = name.substr(4);

			int num = boost::lexical_cast<int>(numStr);

			executeTriggerType(trig.c_str(), num);
			return JsonNode();
		}

		if(!globalEnv->isBound(name, Environment::ANYWHERE))
			throw ESymbolNotFound(name);

		VOption & apiMethod = globalEnv->retrieveValue(name);

		if(!isA<VFunc>(apiMethod))
			throw EVermScriptExecError("API method must be implemented in a function");

		VFunc f = getAs<VFunc>(apiMethod);

		if(f.macro)
			throw EVermScriptExecError("API method can not be macro");

		VOptionList vparams;

		if(parameters.getType() == JsonNode::JsonType::DATA_VECTOR)
		{
			for(const JsonNode & elem : parameters.Vector())
			{
				VOption opt = JsonToVOption(true).convert(elem);

				vparams.push_back(opt);
			}
		}

		VOption vret = f(this, VermTreeIterator(vparams));

		return boost::apply_visitor(VOptionToJson(), vret);
	}
	catch(VERMInterpreter::EInterpreterProblem & ex)
	{
		logMod->error(ex.what());
		return JsonUtils::stringNode(ex.what());
	}
}

JsonNode ERMInterpreter::callGlobal(ServerCb * cb, const std::string & name, const JsonNode & parameters)
{
	acb = cb;

	auto ret = callGlobal(name, parameters);

	acb = nullptr;

	return ret;
}

JsonNode ERMInterpreter::callGlobal(ServerBattleCb * cb, const std::string & name, const JsonNode & parameters)
{
	bacb = cb;

	auto ret = callGlobal(name, parameters);

	bacb = nullptr;

	return ret;
}

void ERMInterpreter::setGlobal(const std::string & name, int value)
{
	globalEnv->localBindLiteral(name, value);
}

void ERMInterpreter::setGlobal(const std::string & name, const std::string & value)
{
	globalEnv->localBindLiteral(name, value);
}

void ERMInterpreter::setGlobal(const std::string & name, double value)
{
	globalEnv->localBindLiteral(name, value);
}

void ERMInterpreter::init(const IGameInfoCallback * cb, const CBattleInfoCallback * battleCb)
{
	curFunc = nullptr;
	curTrigger = nullptr;
	icb = cb;
	bicb = battleCb;

	//TODO: reset?
	for(int g = 0; g < ARRAY_COUNT(funcVars); ++g)
		funcVars[g].reset();

	scanScripts();

	//TODO: execute every time for stateless context, only on new game otherwise

	executeInstructions();
	executeTriggerType("PI");
}

void ERMInterpreter::heroVisit(const CGHeroInstance *visitor, const CGObjectInstance *visitedObj, bool start)
{
	if(!visitedObj)
		return;
	setCurrentlyVisitedObj(visitedObj->pos);
	TIDPattern tip;
	tip[1] = {visitedObj->ID};
	tip[2] = {visitedObj->ID, visitedObj->subID};
	tip[3] = {visitedObj->pos.x, visitedObj->pos.y, visitedObj->pos.z};
	executeTriggerType(VERMInterpreter::TriggerType("OB"), start, tip);
}

void ERMInterpreter::battleStart(const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side)
{
	executeTriggerType("BA", 0);
	executeTriggerType("BR", -1);
	executeTriggerType("BF", 0);
	//TODO tactics or not
}

const CGObjectInstance * ERMInterpreter::getObjFrom( int3 pos )
{
	std::vector<const CGObjectInstance * > objs = icb->getVisitableObjs(pos);
	if(!objs.size())
		throw EScriptExecError("Attempt to obtain access to nonexistent object!");
	return objs.back();
}

void ERMInterpreter::checkActionCallback() const
{
	if(!acb)
		throw EScriptExecError("Game state change is not permitted in this environment");
}

void ERMInterpreter::showInfoDialog(const std::string & msg, PlayerColor player)
{
	//TODO: move to IF_M
	checkActionCallback();

	InfoWindow iw;
	iw.player = player;
	iw.text << msg;
	acb->commitPackage(&iw);
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
		return opt;
		//throw EVermScriptExecError("Literal does not evaluate to a function: "+boost::to_string(opt));
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
		boost::apply_visitor(ERMExpDispatch(interp), opt.cmd);
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
// 	if(line.children.isNil())
// 		return;
//
// 	VOption & car = line.children.car().getAsItem();
//	logger->trace("\tevaluating ");
//	printVOption(line);
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

//void ERMInterpreter::executeUserCommand(const std::string &cmd)
//{
//	logger->trace("ERM here: received command: %s", cmd);
//	if(cmd.size() < 3)
//	{
//		logger->error("That can't be a valid command: %s", cmd);
//		return;
//	}
//	try
//	{
//		if(cmd[0] == '!') //should be a neat (V)ERM command
//		{
//			ERM::TLine line = ERMParser::parseLine(cmd);
//			executeLine(line);
//		}
//	}
//	catch(std::exception &e)
//	{
//		logger->error("Failed executing user command! Exception info: %s", e.what());
//	}
//}

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
