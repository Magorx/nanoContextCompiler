#ifndef RECURSIVE_PARSER
#define RECURSIVE_PARSER

#include "general/c/announcement.h"
#include "general/cpp/stringview.hpp"
#include "general/cpp/vector.hpp"

#include "general/c/debug.h"

#include <cmath>

#include "code_node.h"
#include "lex_token.h"

typedef CodeNode ParseNode;

enum PARSER_ERROR {
	OK = 0,

	ERROR_SYNTAX = 100,
};

enum OP_CODES {
	OP_VAR_DEF  = 20,
	OP_CONDITION   = 30,
	OP_COND_DEPENDENT = 31,
};

//=============================================================================
// RecursiveParser ============================================================

class RecursiveParser {
private:
// data =======================================================================
	Vector<Token> *expr;
	int            cur_index;
	Token         *cur;

	int            ERROR;
	const Token   *ERRPOS;
//=============================================================================
	#define NEXT() ++cur_index; cur = &(*expr)[cur_index]
	#define PREV() --cur_index; cur = &(*expr)[cur_index]
	#define SETI(ind) cur_index = ind; cur = &(*expr)[cur_index]
	#define RESET() SETI(enter_index)

	#define NEW_NODE(type, data, l, r) ParseNode::NEW(type, data, l, r, cur->line, cur->pos)

	#define REQUIRE_OP(op)                                  \
		do {                                                \
			if (cur->type != T_OP || cur->data.op != op) {  \
				ERROR  = ERROR_SYNTAX;                      \
				ERRPOS = cur;                               \
				return nullptr;                             \
			} else {                                        \
				NEXT();                                     \
			}                                               \
		} while (0)

	#define IF_PARSED(index, ret_name, code)          \
		ParseNode *ret_name = (code);                 \
		if (ERROR) {                                  \
			cur_index = index;                        \
			cur = &(*expr)[cur_index];                \
			SET_ERR(0, &(*expr)[0]);                  \
		} else

	#define SET_ERR(errcode, errpos) do {ERROR = errcode; ERRPOS = errpos;} while (0)

	bool is_id_char(const char c) {
		return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
	}

	bool is_digit(const char c) {
		return ('0' <= c && c <= '9');
	}

	bool is_sign(const char c) {
		return c == '+' || c == '-';
	}

	bool is_sign(const Token *t) {
		return t->is_op('+') || t->is_op('-');
	}

	bool is_multiplicative(const char c) {
		return c == '*' || c == '/';
	}

	bool is_multiplicative(const Token *t) {
		return t->is_op('*') || t->is_op('/');
	}

	ParseNode *parse_ID() {
		if (!cur->is_id()) {
			SET_ERR(ERROR_SYNTAX, cur);
			return nullptr;
		}

		CodeNode *ret = NEW_NODE(ID, cur->data.id, nullptr, nullptr);
		NEXT();
		return ret;
	}

	ParseNode *parse_NUMB() {
		if (!cur->is_number()) {
			SET_ERR(ERROR_SYNTAX, cur);
			return nullptr;
		}
		CodeNode *ret = NEW_NODE(VALUE, cur->data.num, nullptr, nullptr);
		NEXT();
		return ret;
	}

	ParseNode *parse_UNIT() {
		IF_PARSED (cur_index, unit_id, parse_ID()) {
			if (cur->is_op('(')) {
				NEXT();
				IF_PARSED (cur_index, bracket_expr, parse_MATH_EXPR()) {
					if (cur->is_op(')')) {
						NEXT();
						unit_id->set_R(bracket_expr);
						return unit_id;
					}
				}
				NEXT();
				SET_ERR(ERROR_SYNTAX, cur);
				ParseNode::DELETE(unit_id);
				return nullptr;
			} else {
				return unit_id;
			}
		}

		if (cur->is_op('(')) {
			NEXT();
			IF_PARSED (cur_index, expr_in_brackets, parse_EXPR()) {
				if (cur->is_op(')')) {
					NEXT();
					return expr_in_brackets;
				} else {
					ParseNode::DELETE(expr_in_brackets);
				}
			}
			NEXT();
			SET_ERR(ERROR_SYNTAX, cur);
			return nullptr;
		}

		IF_PARSED (cur_index, number, parse_NUMB()) {
			return number;
		}

		SET_ERR(ERROR_SYNTAX, cur);
		return nullptr;
	}

	ParseNode *parse_FACT() {
		if (is_sign(cur)) {
			int sign = cur->get_op();
			NEXT();
			IF_PARSED (cur_index, fact, parse_FACT()) {
				return NEW_NODE(OPERATION, sign, nullptr, fact);
			}
			SET_ERR(ERROR_SYNTAX, cur);
			return nullptr;
		}

		IF_PARSED (cur_index, unit, parse_UNIT()) {
			if (cur->is_op('^')) {
				NEXT();
				IF_PARSED (cur_index, fact, parse_FACT()) {
					return NEW_NODE(OPERATION, '^', unit, fact);
				}
				ParseNode::DELETE(unit);
				SET_ERR(ERROR_SYNTAX, cur);
				return nullptr;
			} else {
				return unit;
			}
		}
		
		SET_ERR(ERROR_SYNTAX, cur);
		return nullptr;
	}

	ParseNode *parse_TERM() {
		IF_PARSED (cur_index, fact, parse_FACT()) {
			while (is_multiplicative(cur)) {
				int op = cur->get_op();
				NEXT();

				IF_PARSED (cur_index, next_fact, parse_FACT()) {
					fact = NEW_NODE(OPERATION, op, fact, next_fact);
					continue;
				}

				ParseNode::DELETE(fact, true);
				SET_ERR(ERROR_SYNTAX, cur);
				return nullptr;
			}

			return fact;
		}

		SET_ERR(ERROR_SYNTAX, cur);
		return nullptr;
	}

	ParseNode *parse_DEF_VAR() {
		if (!cur->is_id() || !cur->get_id()->equal("var")) {
			SET_ERR(ERROR_SYNTAX, cur);
			return nullptr;
		}

		NEXT();
		IF_PARSED (cur_index, id, parse_ID()) {
			ParseNode *var_definition = NEW_NODE(OPERATION, OP_VAR_DEF, id, nullptr);
			if (cur->is_op('=')) {
				NEXT();

				IF_PARSED (cur_index, expr_node, parse_EXPR()) {
					var_definition->R = expr_node;
					return var_definition;
				}

				PREV();
				PREV();
				ParseNode::DELETE(var_definition, true);
				SET_ERR(ERROR_SYNTAX, cur);
				return nullptr;
			} else {
				return var_definition;
			}
		}

		PREV();
		SET_ERR(ERROR_SYNTAX, cur);
		return nullptr;
	}

	ParseNode *parse_MATH_EXPR() {
		IF_PARSED (cur_index, term, parse_TERM()) {
			while (is_sign(cur)) {
				int op = cur->get_op();
				NEXT();

				IF_PARSED (cur_index, next_term, parse_TERM()) {
					term = NEW_NODE(OPERATION, op, term, next_term);
					continue;
				}

				ParseNode::DELETE(term, true);
				SET_ERR(ERROR_SYNTAX, cur);
				return nullptr;
			}

			return term;
		}

		SET_ERR(ERROR_SYNTAX, cur);
		return nullptr;
	} 

	ParseNode *parse_COND() {
		IF_PARSED (cur_index, cur_expr, parse_MATH_EXPR()) {
			while (cur->is_op('>') || cur->is_op('<') ||
				   cur->is_op(OPCODE_LE) || cur->is_op(OPCODE_GE ) ||
				   cur->is_op(OPCODE_EQ) || cur->is_op(OPCODE_NEQ)) {
				int op = cur->get_op();
				NEXT();

				IF_PARSED (cur_index, next_expr, parse_MATH_EXPR()) {
					cur_expr = NEW_NODE(OPERATION, op, cur_expr, next_expr);
					continue;
				}

				ParseNode::DELETE(cur_expr, true);
				SET_ERR(ERROR_SYNTAX, cur);
				return nullptr;
			}

			return cur_expr;
		}

		SET_ERR(ERROR_SYNTAX, cur);
		return nullptr;
	}

	ParseNode *parse_AND_EXPR() {
		IF_PARSED (cur_index, cur_cond, parse_COND()) {
			while (cur->is_op(OPCODE_AND)) {
				int op = cur->get_op();
				NEXT();

				IF_PARSED (cur_index, next_cond, parse_COND()) {
					cur_cond = NEW_NODE(OPERATION, op, cur_cond, next_cond);
					continue;
				}

				ParseNode::DELETE(cur_cond, true);
				SET_ERR(ERROR_SYNTAX, cur);
				return nullptr;
			}

			return cur_cond;
		}

		SET_ERR(ERROR_SYNTAX, cur);
		return nullptr;
	}

	ParseNode *parse_EXPR() {
		IF_PARSED (cur_index, cur_and_node, parse_AND_EXPR()) {
			while (cur->is_op(OPCODE_OR)) {
				int op = cur->get_op();
				NEXT();

				IF_PARSED (cur_index, next_and_node, parse_AND_EXPR()) {
					cur_and_node = NEW_NODE(OPERATION, op, cur_and_node, next_and_node);
					continue;
				}

				ParseNode::DELETE(cur_and_node, true);
				SET_ERR(ERROR_SYNTAX, cur);
				return nullptr;
			}

			return cur_and_node;
		}

		SET_ERR(ERROR_SYNTAX, cur);
		return nullptr;
	}

	ParseNode *parse_ASGN() {
		IF_PARSED (cur_index, id, parse_ID()) {
			if (cur->is_op('=')) {
				int op = cur->get_op();
				NEXT();

				IF_PARSED (cur_index, expr_node, parse_EXPR()) {
					return NEW_NODE(OPERATION, op, id, expr_node);
				}

				ParseNode::DELETE(id, true);
				SET_ERR(ERROR_SYNTAX, cur);
				return nullptr;
			} else {
				PREV();
				ParseNode::DELETE(id, true);
				SET_ERR(ERROR_SYNTAX, cur);
				return nullptr;
			}
		}

		SET_ERR(ERROR_SYNTAX, cur);
		return nullptr;
	}

	ParseNode *parse_IF() {
		int enter_index = cur_index;

		if (!cur->is_op('?')) {
			SET_ERR(ERROR_SYNTAX, cur);
			return nullptr;
		}
		NEXT();

		if (!cur->is_op('(')) {
			RESET();
			SET_ERR(ERROR_SYNTAX, cur);
			return nullptr;
		}
		NEXT();

		IF_PARSED (cur_index, cond_block, parse_EXPR()) {
			if (!cur->is_op(')')) {
				RESET();
				SET_ERR(ERROR_SYNTAX, cur);
				return nullptr;
			}
			NEXT();

			IF_PARSED (cur_index, true_block, parse_BLOCK_STATEMENT()) {
				ParseNode *dep  = NEW_NODE(OPERATION, OP_COND_DEPENDENT, nullptr,    true_block);
				ParseNode *cond = NEW_NODE(OPERATION, OP_CONDITION,      cond_block, dep       );
				if (!cur->is_op(':')) {
					return cond;
				}

				NEXT();
				IF_PARSED (cur_index, false_block, parse_BLOCK_STATEMENT()) {
					dep->L = false_block;
					return cond;
				}

				RESET();
				ParseNode::DELETE(cond, true);
				SET_ERR(ERROR_SYNTAX, cur);
				return nullptr;
			}

			RESET();
			ParseNode::DELETE(cond_block, true);
			SET_ERR(ERROR_SYNTAX, cur);
			return nullptr;
		}

		RESET();
		SET_ERR(ERROR_SYNTAX, cur);
		return nullptr;
	}

	ParseNode *parse_STATEMENT() {
		IF_PARSED (cur_index, var_def, parse_DEF_VAR()) {
			if (!cur->is_op(';')) {
				ParseNode::DELETE(var_def, true);
				SET_ERR(ERROR_SYNTAX, cur);
				return nullptr;
			} else {
				NEXT();
				return NEW_NODE(OPERATION, ';', var_def, nullptr);
			}
		}

		IF_PARSED (cur_index, assign, parse_ASGN()) {
			if (!cur->is_op(';')) {
				ParseNode::DELETE(assign, true);
				SET_ERR(ERROR_SYNTAX, cur);
				return nullptr;
			} else {
				NEXT();
				return NEW_NODE(OPERATION, ';', assign, nullptr);
			}
		}

		IF_PARSED (cur_index, expr_node, parse_EXPR()) {
			if (!cur->is_op(';')) {
				ParseNode::DELETE(expr_node, true);
				SET_ERR(ERROR_SYNTAX, cur);
				return nullptr;
			} else {
				NEXT();
				return NEW_NODE(OPERATION, ';', expr_node, nullptr);
			}
		}

		IF_PARSED (cur_index, if_node, parse_IF()) {
			return NEW_NODE(OPERATION, ';', if_node, nullptr);
		}

		SET_ERR(ERROR_SYNTAX, cur);
		return nullptr;
	}

	ParseNode *parse_BLOCK_STATEMENT() {
		if (cur->is_op('{')) {
			NEXT();
			ParseNode *block_node = NEW_NODE(OPERATION, '{', nullptr, nullptr);
			IF_PARSED (cur_index, block_stmt, parse_BLOCK_STATEMENT()) {
				block_node->L = block_stmt;
				while (!cur->is_op('}')) {
					IF_PARSED(cur_index, next_block, parse_BLOCK_STATEMENT()) {
						block_stmt->R = next_block;
						block_stmt = next_block;
						continue;
					}

					ParseNode::DELETE(block_node, true);
					SET_ERR(ERROR_SYNTAX, cur);
					return nullptr;
				}

				NEXT();
				return block_node;
			}

			ParseNode::DELETE(block_node, true);
			SET_ERR(ERROR_SYNTAX, cur);
			return nullptr;
		}

		IF_PARSED (cur_index, statement, parse_STATEMENT()) {
			return statement;
		}

		SET_ERR(ERROR_SYNTAX, cur);
		return nullptr;
	}

	ParseNode *parse_PROG() {
		return parse_BLOCK_STATEMENT();
	}      

	ParseNode *parse_G() {
		IF_PARSED (cur_index, prog, parse_PROG()) {
			if (cur->type == T_END) {
				return prog;
			} else {
				ParseNode::DELETE(prog, true);
			}
		}

		SET_ERR(ERROR_SYNTAX, cur);

		return nullptr;
	}

public:
	RecursiveParser            (const RecursiveParser&) = delete;
	RecursiveParser &operator= (const RecursiveParser&) = delete;

	RecursiveParser():
	expr(nullptr),
	cur_index(0),
	cur(nullptr),
	ERROR(0),
	ERRPOS(nullptr)
	{}

	~RecursiveParser() {}

	void ctor() {
		expr = nullptr;
		cur_index = 0;
		cur = nullptr;
		ERROR = 0;
		ERRPOS = nullptr;
	}

	static RecursiveParser *NEW() {
		RecursiveParser *cake = (RecursiveParser*) calloc(1, sizeof(RecursiveParser));
		if (!cake) {
			return nullptr;
		}

		cake->ctor();
		return cake;
	}

	void dtor() {
		expr = nullptr;
		cur_index = 0;
		cur = nullptr;

		ERROR  = 0;
		ERRPOS = 0;
	}

	static void DELETE(RecursiveParser *classname) {
		if (!classname) {
			return;
		}

		classname->dtor();
		free(classname);
	}

//=============================================================================

	ParseNode *parse(Vector<Token> *expression) {
		expr      = expression;
		cur_index = 0;
		cur       = &(*expr)[cur_index];

		ParseNode *res = parse_G();
		if (!ERROR) {
			return res;
		} else {
			ANNOUNCE("ERR", "parser", "an error occured during grammar parsing");
			ANNOUNCE("ERR", "parser", "line [%d] | pos [%d]", ERRPOS->line, ERRPOS->pos);
			//ERRPOS->dump();
			return nullptr;
		}
	}

};

#endif // RECURSIVE_PARSER