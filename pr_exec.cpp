#include "quakedef.h"


/*

*/

struct prstack_t
{
	int s;
	dfunction_t* f;
};

#define	MAX_STACK_DEPTH		32
prstack_t pr_stack[MAX_STACK_DEPTH];
int pr_depth;

#define	LOCALSTACK_SIZE		2048
int localstack[LOCALSTACK_SIZE];
int localstack_used;


qboolean pr_trace;
dfunction_t* pr_xfunction;
int pr_xstatement;


int pr_argc;

char* pr_opnames[] =
{
	"DONE",

	"MUL_F",
	"MUL_V",
	"MUL_FV",
	"MUL_VF",

	"DIV",

	"ADD_F",
	"ADD_V",

	"SUB_F",
	"SUB_V",

	"EQ_F",
	"EQ_V",
	"EQ_S",
	"EQ_E",
	"EQ_FNC",

	"NE_F",
	"NE_V",
	"NE_S",
	"NE_E",
	"NE_FNC",

	"LE",
	"GE",
	"LT",
	"GT",

	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",

	"ADDRESS",

	"STORE_F",
	"STORE_V",
	"STORE_S",
	"STORE_ENT",
	"STORE_FLD",
	"STORE_FNC",

	"STOREP_F",
	"STOREP_V",
	"STOREP_S",
	"STOREP_ENT",
	"STOREP_FLD",
	"STOREP_FNC",

	"RETURN",

	"NOT_F",
	"NOT_V",
	"NOT_S",
	"NOT_ENT",
	"NOT_FNC",

	"IF",
	"IFNOT",

	"CALL0",
	"CALL1",
	"CALL2",
	"CALL3",
	"CALL4",
	"CALL5",
	"CALL6",
	"CALL7",
	"CALL8",

	"STATE",

	"GOTO",

	"AND",
	"OR",

	"BITAND",
	"BITOR"
};

char* PR_GlobalString(int ofs);
char* PR_GlobalStringNoContents(int ofs);


//=============================================================================

/*
=================
PR_PrintStatement
=================
*/
void PR_PrintStatement(dstatement_t* s)
{
	if (static_cast<unsigned>(s->op) < sizeof pr_opnames / sizeof pr_opnames[0])
	{
		Con_Printf("%s ", pr_opnames[static_cast<uint16_t>(s->op)]);
		int i = strlen(pr_opnames[static_cast<uint16_t>(s->op)]);
		for (; i < 10; i++)
			Con_Printf(" ");
	}

	if (s->op == op_t::OP_IF || s->op == op_t::OP_IFNOT)
		Con_Printf("%sbranch %i", PR_GlobalString(s->a), s->b);
	else if (s->op == op_t::OP_GOTO)
	{
		Con_Printf("branch %i", s->a);
	}
	else if (static_cast<uint16_t>(s->op) - static_cast<uint16_t>(op_t::OP_STORE_F) < 6)
	{
		Con_Printf("%s", PR_GlobalString(s->a));
		Con_Printf("%s", PR_GlobalStringNoContents(s->b));
	}
	else
	{
		if (s->a)
			Con_Printf("%s", PR_GlobalString(s->a));
		if (s->b)
			Con_Printf("%s", PR_GlobalString(s->b));
		if (s->c)
			Con_Printf("%s", PR_GlobalStringNoContents(s->c));
	}
	Con_Printf("\n");
}

/*
============
PR_StackTrace
============
*/
void PR_StackTrace(void)
{
	if (pr_depth == 0)
	{
		Con_Printf("<NO STACK>\n");
		return;
	}

	pr_stack[pr_depth].f = pr_xfunction;
	for (auto i = pr_depth; i >= 0; i--)
	{
		auto f = pr_stack[i].f;

		if (!f)
		{
			Con_Printf("<NO FUNCTION>\n");
		}
		else
			Con_Printf("%12s : %s\n", pr_strings + f->s_file, pr_strings + f->s_name);
	}
}


/*
============
PR_Profile_f

============
*/
void PR_Profile_f(void)
{
	dfunction_t* best;

	auto num = 0;
	do
	{
		auto max = 0;
		best = nullptr;
		for (auto i = 0; i < progs->numfunctions; i++)
		{
			auto f = &pr_functions[i];
			if (f->profile > max)
			{
				max = f->profile;
				best = f;
			}
		}
		if (best)
		{
			if (num < 10)
				Con_Printf("%7i %s\n", best->profile, pr_strings + best->s_name);
			num++;
			best->profile = 0;
		}
	}
	while (best);
}


/*
============
PR_RunError

Aborts the currently executing function
============
*/
void PR_RunError(char* error, ...)
{
	va_list argptr;
	char string[1024];

	va_start (argptr,error);
	vsprintf(string, error, argptr);
	va_end (argptr);

	PR_PrintStatement(pr_statements + pr_xstatement);
	PR_StackTrace();
	Con_Printf("%s\n", string);

	pr_depth = 0; // dump the stack so host_error can shutdown functions

	Host_Error("Program error");
}

/*
============================================================================
PR_ExecuteProgram

The interpretation main loop
============================================================================
*/

/*
====================
PR_EnterFunction

Returns the new program statement counter
====================
*/
int PR_EnterFunction(dfunction_t* f)
{
	int i;

	pr_stack[pr_depth].s = pr_xstatement;
	pr_stack[pr_depth].f = pr_xfunction;
	pr_depth++;
	if (pr_depth >= MAX_STACK_DEPTH)
		PR_RunError("stack overflow");

	// save off any locals that the new function steps on
	auto c = f->locals;
	if (localstack_used + c > LOCALSTACK_SIZE)
		PR_RunError("PR_ExecuteProgram: locals stack overflow\n");

	for (i = 0; i < c; i++)
		localstack[localstack_used + i] = reinterpret_cast<int *>(pr_globals)[f->parm_start + i];
	localstack_used += c;

	// copy parameters
	auto o = f->parm_start;
	for (i = 0; i < f->numparms; i++)
	{
		for (auto j = 0; j < f->parm_size[i]; j++)
		{
			reinterpret_cast<int *>(pr_globals)[o] = reinterpret_cast<int *>(pr_globals)[OFS_PARM0 + i * 3 + j];
			o++;
		}
	}

	pr_xfunction = f;
	return f->first_statement - 1; // offset the s++
}

/*
====================
PR_LeaveFunction
====================
*/
int PR_LeaveFunction(void)
{
	if (pr_depth <= 0)
		Sys_Error("prog stack underflow");

	// restore locals from the stack
	auto c = pr_xfunction->locals;
	localstack_used -= c;
	if (localstack_used < 0)
		PR_RunError("PR_ExecuteProgram: locals stack underflow\n");

	for (auto i = 0; i < c; i++)
		reinterpret_cast<int *>(pr_globals)[pr_xfunction->parm_start + i] = localstack[localstack_used + i];

	// up stack
	pr_depth--;
	pr_xfunction = pr_stack[pr_depth].f;
	return pr_stack[pr_depth].s;
}


/*
====================
PR_ExecuteProgram
====================
*/
void PR_ExecuteProgram(func_t fnum)
{
	eval_t* a;
	edict_t* ed;
	eval_t* ptr;

	if (!fnum || fnum >= progs->numfunctions)
	{
		if (pr_global_struct->self)
			ED_Print(PROG_TO_EDICT(pr_global_struct->self));
		Host_Error("PR_ExecuteProgram: nullptr function");
	}

	auto f = &pr_functions[fnum];

	auto runaway = 100000;
	pr_trace = qfalse;

	// make a stack frame
	auto exitdepth = pr_depth;

	auto s = PR_EnterFunction(f);

	while (true)
	{
		s++; // next statement

		auto st = &pr_statements[s];
		a = reinterpret_cast<eval_t *>(&pr_globals[st->a]);
		auto b = reinterpret_cast<eval_t *>(&pr_globals[st->b]);
		auto c = reinterpret_cast<eval_t *>(&pr_globals[st->c]);

		if (!--runaway)
			PR_RunError("runaway loop error");

		pr_xfunction->profile++;
		pr_xstatement = s;

		if (pr_trace)
			PR_PrintStatement(st);

		switch (st->op)
		{
		case op_t::OP_ADD_F:
			c->_float = a->_float + b->_float;
			break;
		case op_t::OP_ADD_V:
			c->vector[0] = a->vector[0] + b->vector[0];
			c->vector[1] = a->vector[1] + b->vector[1];
			c->vector[2] = a->vector[2] + b->vector[2];
			break;

		case op_t::OP_SUB_F:
			c->_float = a->_float - b->_float;
			break;
		case op_t::OP_SUB_V:
			c->vector[0] = a->vector[0] - b->vector[0];
			c->vector[1] = a->vector[1] - b->vector[1];
			c->vector[2] = a->vector[2] - b->vector[2];
			break;

		case op_t::OP_MUL_F:
			c->_float = a->_float * b->_float;
			break;
		case op_t::OP_MUL_V:
			c->_float = a->vector[0] * b->vector[0]
				+ a->vector[1] * b->vector[1]
				+ a->vector[2] * b->vector[2];
			break;
		case op_t::OP_MUL_FV:
			c->vector[0] = a->_float * b->vector[0];
			c->vector[1] = a->_float * b->vector[1];
			c->vector[2] = a->_float * b->vector[2];
			break;
		case op_t::OP_MUL_VF:
			c->vector[0] = b->_float * a->vector[0];
			c->vector[1] = b->_float * a->vector[1];
			c->vector[2] = b->_float * a->vector[2];
			break;

		case op_t::OP_DIV_F:
			c->_float = a->_float / b->_float;
			break;

		case op_t::OP_BITAND:
			c->_float = static_cast<int>(a->_float) & static_cast<int>(b->_float);
			break;

		case op_t::OP_BITOR:
			c->_float = static_cast<int>(a->_float) | static_cast<int>(b->_float);
			break;


		case op_t::OP_GE:
			c->_float = a->_float >= b->_float;
			break;
		case op_t::OP_LE:
			c->_float = a->_float <= b->_float;
			break;
		case op_t::OP_GT:
			c->_float = a->_float > b->_float;
			break;
		case op_t::OP_LT:
			c->_float = a->_float < b->_float;
			break;
		case op_t::OP_AND:
			c->_float = a->_float && b->_float;
			break;
		case op_t::OP_OR:
			c->_float = a->_float || b->_float;
			break;

		case op_t::OP_NOT_F:
			c->_float = !a->_float;
			break;
		case op_t::OP_NOT_V:
			c->_float = !a->vector[0] && !a->vector[1] && !a->vector[2];
			break;
		case op_t::OP_NOT_S:
			c->_float = !a->string || !pr_strings[a->string];
			break;
		case op_t::OP_NOT_FNC:
			c->_float = !a->function;
			break;
		case op_t::OP_NOT_ENT:
			c->_float = PROG_TO_EDICT(a->edict) == sv.edicts;
			break;

		case op_t::OP_EQ_F:
			c->_float = a->_float == b->_float;
			break;
		case op_t::OP_EQ_V:
			c->_float = a->vector[0] == b->vector[0] &&
				a->vector[1] == b->vector[1] &&
				a->vector[2] == b->vector[2];
			break;
		case op_t::OP_EQ_S:
			c->_float = !strcmp(pr_strings + a->string, pr_strings + b->string);
			break;
		case op_t::OP_EQ_E:
			c->_float = a->_int == b->_int;
			break;
		case op_t::OP_EQ_FNC:
			c->_float = a->function == b->function;
			break;


		case op_t::OP_NE_F:
			c->_float = a->_float != b->_float;
			break;
		case op_t::OP_NE_V:
			c->_float = a->vector[0] != b->vector[0] ||
				a->vector[1] != b->vector[1] ||
				a->vector[2] != b->vector[2];
			break;
		case op_t::OP_NE_S:
			c->_float = strcmp(pr_strings + a->string, pr_strings + b->string);
			break;
		case op_t::OP_NE_E:
			c->_float = a->_int != b->_int;
			break;
		case op_t::OP_NE_FNC:
			c->_float = a->function != b->function;
			break;

			//==================
		case op_t::OP_STORE_F:
		case op_t::OP_STORE_ENT:
		case op_t::OP_STORE_FLD: // integers
		case op_t::OP_STORE_S:
		case op_t::OP_STORE_FNC: // pointers
			b->_int = a->_int;
			break;
		case op_t::OP_STORE_V:
			b->vector[0] = a->vector[0];
			b->vector[1] = a->vector[1];
			b->vector[2] = a->vector[2];
			break;

		case op_t::OP_STOREP_F:
		case op_t::OP_STOREP_ENT:
		case op_t::OP_STOREP_FLD: // integers
		case op_t::OP_STOREP_S:
		case op_t::OP_STOREP_FNC: // pointers
			ptr = reinterpret_cast<eval_t *>(reinterpret_cast<byte *>(sv.edicts) + b->_int);
			ptr->_int = a->_int;
			break;
		case op_t::OP_STOREP_V:
			ptr = reinterpret_cast<eval_t *>(reinterpret_cast<byte *>(sv.edicts) + b->_int);
			ptr->vector[0] = a->vector[0];
			ptr->vector[1] = a->vector[1];
			ptr->vector[2] = a->vector[2];
			break;

		case op_t::OP_ADDRESS:
			ed = PROG_TO_EDICT(a->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
			if (ed == static_cast<edict_t *>(sv.edicts) && sv.state == server_state_t::ss_active)
				PR_RunError("assignment to world entity");
			c->_int = reinterpret_cast<byte *>(reinterpret_cast<int *>(&ed->v) + b->_int) - reinterpret_cast<byte *>(sv.edicts);
			break;

		case op_t::OP_LOAD_F:
		case op_t::OP_LOAD_FLD:
		case op_t::OP_LOAD_ENT:
		case op_t::OP_LOAD_S:
		case op_t::OP_LOAD_FNC:
			ed = PROG_TO_EDICT(a->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
			a = reinterpret_cast<eval_t *>(reinterpret_cast<int *>(&ed->v) + b->_int);
			c->_int = a->_int;
			break;

		case op_t::OP_LOAD_V:
			ed = PROG_TO_EDICT(a->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
			a = reinterpret_cast<eval_t *>(reinterpret_cast<int *>(&ed->v) + b->_int);
			c->vector[0] = a->vector[0];
			c->vector[1] = a->vector[1];
			c->vector[2] = a->vector[2];
			break;

			//==================

		case op_t::OP_IFNOT:
			if (!a->_int)
				s += st->b - 1; // offset the s++
			break;

		case op_t::OP_IF:
			if (a->_int)
				s += st->b - 1; // offset the s++
			break;

		case op_t::OP_GOTO:
			s += st->a - 1; // offset the s++
			break;

		case op_t::OP_CALL0:
		case op_t::OP_CALL1:
		case op_t::OP_CALL2:
		case op_t::OP_CALL3:
		case op_t::OP_CALL4:
		case op_t::OP_CALL5:
		case op_t::OP_CALL6:
		case op_t::OP_CALL7:
		case op_t::OP_CALL8:
			{
				pr_argc = static_cast<uint16_t>(st->op) - static_cast<uint16_t>(op_t::OP_CALL0);
				if (!a->function)
					PR_RunError("nullptr function");

				auto newf = &pr_functions[a->function];

				if (newf->first_statement < 0)
				{ // negative statements are built in functions
					auto i = -newf->first_statement;
					if (i >= pr_numbuiltins)
						PR_RunError("Bad builtin call number");
					pr_builtins[i]();
					break;
				}

				s = PR_EnterFunction(newf);
			}
			break;

		case op_t::OP_DONE:
		case op_t::OP_RETURN:
			pr_globals[OFS_RETURN] = pr_globals[st->a];
			pr_globals[OFS_RETURN + 1] = pr_globals[st->a + 1];
			pr_globals[OFS_RETURN + 2] = pr_globals[st->a + 2];

			s = PR_LeaveFunction();
			if (pr_depth == exitdepth)
				return; // all done
			break;

		case op_t::OP_STATE:
			ed = PROG_TO_EDICT(pr_global_struct->self);
#ifdef FPS_20
		ed->v.nextthink = pr_global_struct->time + 0.05;
#else
			ed->v.nextthink = pr_global_struct->time + 0.1;
#endif
			if (a->_float != ed->v.frame)
			{
				ed->v.frame = a->_float;
			}
			ed->v.think = b->function;
			break;

		default:
			PR_RunError("Bad opcode %i", st->op);
		}
	}
}
