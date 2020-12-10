#ifndef COMPILER_OPTIONS
#define COMPILER_OPTIONS

#define OPDEF(name, code) name = code,

enum OPERATION_CODE {
	#include "opcodes.h"
};

#undef OPDEF
#define OPDEF(name, code) case code : return #name + 7;

const char *OPERATION_NAME(const int opcode) {
	switch(opcode) {
		#include "opcodes.h"

		default : return nullptr;
	}
}

#undef OPDEF

bool is_normal_op(const int op) {
	return op == '+' || op == '-' ||
		   op == '*' || op == '/' ||
		   op == '^' || op == '=' ||
		   op == '<' || op == '>' ||
		   op == '?' || op == ':';
}

bool is_bracket(const int op) {
	return op == '(' || op == ')' ||
		   op == '{' || op == '}' ||
		   op == '[' || op == ']';
}

bool is_splitting_op(const int op) {
	return op == '{' || op == ';';
}

bool is_printable_op(const int op) {
	return isalpha(op) || isdigit(op) || is_normal_op(op) || is_bracket(op) || is_splitting_op(op);
}

bool is_loggable_op(const int op) {
	return !is_splitting_op(op) && is_printable_op(op);
}

bool is_compiling_loggable_op(const int op) {
	return !is_splitting_op(op);
}

#endif // COMPILER_OPTIONS