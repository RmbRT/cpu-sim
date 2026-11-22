#include "instruction.h"
#include <range.h>

extern struct String const k_operation_mnemonics[OP_END];

enum TokenType {
	IDENT,
	NUMBER,
	EQUALS,
	WHITESPACE,
	NEWLINE,
	COMMA,
	EOF,
	ERROR
};

struct Token { enum TokenType type; u32 size; u64 num; };
struct Token parse_token(char const * text, u32 len)
{
	if(!len) return (struct Token) { .type=EOF };

	if(text[0] == ' ')
	{
		unsigned i = 1;
		for(; i < len; i++)
			if(text[i] != ' ') break;
		return (struct Token) { WHITESPACE, i, 0 };
	} else if(text[0] == '\n')
	{
		return (struct Token) { NEWLINE, 1, 0 };
	} else if((text[0] >= 'a' && text[0] <= 'z')
		|| (text[0] >= 'A' && text[0] <= 'Z'))
	{
		unsigned i = 0;
		for(; i < len; i++)
			if(!((text[0] >= 'a' && text[0] <= 'z')
			|| (text[0] >= 'A' && text[0] <= 'Z')
			|| (text[0] >= '0' && text[0] <= '9')
			|| text[0] == '_'))
				break;
		return (struct Token) { IDENT, i, 0 };
	} else if(text[0] == '=')
	{
		return (struct Token) { EQUALS, 1, 0 };
	} else if(text[0] == ',')
	{
		return (struct Token) { COMMA, 1, 0 };
	}

	unsigned start = 0;
	bool negate = false;
	if(text[0] == '-')
	{
		start = 1;
		negate = true;
	}

	if(text[start] >= '0')
	{
		u64 parsed = 0;
		if(text[start+1] == 'x')
		{
			if(len < start+3)
				return (struct Token) { .type=ERROR };
			unsigned i = start+2;
			for(; i < len; i++)
			{
				u8 nibble;
				// TODO: overflow check.
				if(text[i] >= 'A' && text[i] <= 'F')
					nibble = text[i] - 'A' + 10;
				else if(text[i] >= 'a' && text[i] <= 'f')
					nibble = text[i] - 'a' + 10;
				else if(text[i] >= '0' && text[i] <= '9')
					nibble = text[i] - '0';
				else break;
				parsed <<= 4;
				parsed |= nibble;
			}
			if(negate) parsed = -parsed;
			return (struct Token) { NUMBER, i, parsed };
		} else
		{
			return (struct Token) { NUMBER, start+1, 0 };
		}
	} else if(text[start] >= '0' && text[start] <= '9')
	{
		u64 parsed = text[start] - '0';

		unsigned i = start+1;
		for(; i < len; i++)
		{
			if(text[i] >= '0' && text[i] <= '9')
				parsed = 10 * parsed + text[i]-'0';
			else break;
		}
		if(negate) parsed = -parsed;
		return (struct Token) { NUMBER, i, parsed };
	} else
		return (struct Token) { .type=ERROR };
}

enum ParseResult
{
	PR_INSTRUCTION,
	PR_ERROR,
	PR_BLANK_LINE,
	PR_EOF
};


enum Operation get_op(struct String cmp_str)
{
	unsigned op = 0;
	for(; op < OP_END; op++)
	{
		auto op_str = k_operation_mnemonics[op];

		if(streq(op_str, cmp_str))
			break;
	}
	return op;
}


enum ParseResult parse_line(char const * text, u32 len, struct Instruction * out)
{
	struct Token tok = {};

	do { text += tok.size; len -= tok.size; tok = parse_token(text, len); }
		while(tok.type == WHITESPACE);

	if(tok.type == EOF) return PR_EOF;
	if(tok.type == NEWLINE) return PR_BLANK_LINE;

	out->dst = ARG_NONE;
	out->src[0] = ARG_NONE;
	out->src[1] = ARG_NONE;
	out->immediate = 0;

	if(tok.type == IDENT)
	{
		struct String cmp_str = { text, tok.size };

		out->op = get_op(cmp_str);

		if(out->op == OP_END)
		{
			if(cmp_str.length != 1)
				return PR_ERROR;

			if(cmp_str.ptr[0] >= 'A' && cmp_str.ptr[0] <= 'A' + ARG_END - ARG_REG0)
				out->dst = cmp_str.ptr[0]-'A' + ARG_REG0;
			else return PR_ERROR;

			do { text += tok.size; len -= tok.size; tok = parse_token(text, len); }
				while(tok.type == WHITESPACE);

			if(tok.type != EQUALS)
				return PR_ERROR;

			do { text += tok.size; len -= tok.size; tok = parse_token(text, len); }
				while(tok.type == WHITESPACE);

			if(tok.type == IDENT)
			{
				cmp_str = (struct String) { text, tok.size };
				out->op = get_op(cmp_str);

				if(out->op == OP_END)
				{
					if(cmp_str.ptr[0] >= 'A' && cmp_str.ptr[0] <= 'A' + ARG_END - ARG_REG0)
						out->src[0] = cmp_str.ptr[0]-'A' + ARG_REG0;
					else return PR_ERROR;

					// TODO: check that we are at the end of line.
					out->op = OP_COPY;
					goto end;
				}
			} else if(tok.type == NUMBER)
			{
				out->src[0] = ARG_IMMEDIATE;
				out->immediate = tok.num;
				out->op = OP_COPY;
				goto end;
			} else return PR_ERROR;
		}

		do { text += tok.size; len -= tok.size; tok = parse_token(text, len); }
			while(tok.type == WHITESPACE);

		if(tok.type == EOF || tok.type == NEWLINE)
			goto end;

		bool has_immediate = false;
		if(tok.type == NUMBER)
		{
			out->src[0] = ARG_IMMEDIATE;
			has_immediate = true;
			out->immediate = tok.num;
		} else if(tok.type == IDENT)
		{
			cmp_str = (struct String) { text, tok.size };
			if(cmp_str.ptr[0] >= 'A' && cmp_str.ptr[0] <= 'A' + ARG_END - ARG_REG0)
				out->src[0] = cmp_str.ptr[0]-'A' + ARG_REG0;
			else return PR_ERROR;
		}

		do { text += tok.size; len -= tok.size; tok = parse_token(text, len); }
			while(tok.type == WHITESPACE);

		if(tok.type == EOF || tok.type == NEWLINE)
			goto end;

		if(tok.type != COMMA) return PR_ERROR;

		do { text += tok.size; len -= tok.size; tok = parse_token(text, len); }
			while(tok.type == WHITESPACE);

		if(tok.type == NUMBER)
		{
			if(has_immediate) return PR_ERROR;
			out->src[1] = ARG_IMMEDIATE;
			has_immediate = true;
			out->immediate = tok.num;
		} else if(tok.type == IDENT)
		{
			cmp_str = (struct String) { text, tok.size };
			if(cmp_str.ptr[0] >= 'A' && cmp_str.ptr[0] <= 'A' + ARG_END - ARG_REG0)
				out->src[1] = cmp_str.ptr[0]-'A' + ARG_REG0;
			else return PR_ERROR;
		}
	} else return PR_ERROR;

end:
	do { text += tok.size; len -= tok.size; tok = parse_token(text, len); }
		while(tok.type == WHITESPACE);

	if((tok.type != EOF) && (tok.type != NEWLINE))
		return PR_ERROR;

	return PR_INSTRUCTION;
}