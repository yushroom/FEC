import ply.lex as lex
from .shaderlab import keywords


# define the lexical tokens, plus keywords
tokens = (
	"NUMBER",
	"LBRACE",
	"RBRACE",
	"STRING",
	"IDENT",
	"LPAREN",
	"RPAREN",
	"COMMA",
	"EQUALS",
	"LBRACKET",
	"RBRACKET",
	'COMMENT1',
	'COMMENT2',
	'PRAGMA',
	'HLSLPROGRAM',
	'HLSLINCLUDE',
	'ATTRIBUTE',
) + tuple(keywords.values())


# Begin token regex definitions (order matters)

def t_STRING(t):
	r'\"([^\\\n]|(\\(.|\n)))*?\"'
	t.value = t.value[1:-1]
	# print(t)
	return t

# eg.
#	[HideInInspector]
#	[ToggleUI]
#	[HDR]
#	[Enum(Add, 0, Multiply, 1)]
#	[Enum(DetailMap ANySNx (Builtin), 0, Peeled DetailMap (Forest4), 4)]
def t_ATTRIBUTE(t):
	r'\[.*?\]'
	t.value = t.value[1:-1]
	# print('ATTRIBUTE1', t)
	return t


# Single character tokens
t_LBRACE    = r'{'
t_RBRACE    = r'}'
t_LBRACKET  = r'\['
t_RBRACKET  = r'\]'
t_LPAREN    = r'\('
t_RPAREN    = r'\)'
t_COMMA     = r','
t_EQUALS    = r'='


# 2D, is a special case to avoid number/ident conflicts
def t_2D(t):
	r'2D'
	return t

def t_HLSLPROGRAM(t):
	r'(HLSLPROGRAM(.|\n)*?ENDHLSL)'
	n = t.value.count("\n")
	t.value = t.value[len('HLSLPROGRAM'):-len('ENDHLSL')]
	t.value = "#line " + str(t.lexer.lineno + 1) + t.value
	t.lexer.lineno += n
	return t

def t_HLSLINCLUDE(t):
	r'(HLSLINCLUDE(.|\n)*?ENDHLSL)'
	# t.lexer.lineno += t.value.count("\n")
	# t.value = t.value[len('HLSLINCLUDE'):-len('ENDHLSL')]
	n = t.value.count("\n")
	t.value = t.value[len('HLSLPROGRAM'):-len('ENDHLSL')]
	t.value = "#line " + str(t.lexer.lineno + 1) + t.value
	t.lexer.lineno += n
	return t


# IDENT, identifiers not starting with a number, assign to keyword terminals
def t_IDENT(t):
	r'[A-Za-z_][A-Za-z_0-9]*'
	t.type = keywords.get(t.value, 'IDENT')
	# print(t)
	return t


# NUMBER, ints and floats (without exp), convert to float type
def t_NUMBER(t):
	r'(-?(\d+(\.\d*)?|\.\d+))'
	t.value = float(t.value)
	# print(t)
	return t


# NewLine, track line breaks to allow storing of line numbers
# TODO not used at the moment
def t_newline(t):
	r'\n+'
	t.lexer.lineno += len(t.value)

# c-style multiline comment
def t_COMMENT1(t):
	r'(/\*(.|\n)*?\*/)'
	ncr = t.value.count("\n")
	t.lexer.lineno += ncr
	# print('COMMENT1', t)
	# t.type = 'ignore'; t.value = '\n' * ncr if ncr else ' '
	# return t

# single line comment starts with //
def t_COMMENT2(t):
	r'(//.*?(\n|$))'
	t.lexer.lineno += t.value.count("\n")
	# print('COMMENT2', t)
	# return t

def t_PRAGMA(t):
	r'\#pragma'
	# print(t)
	return t

# ignored characters (spaces and tabs)
t_ignore  = ' \t'


# lex error
# TODO could skip token with 't.lexer.skip(1)'
def t_error(t):
	# raise lex.LexError("Illegal character '%s'" % t.value[0])
	print("Illegal character '%s'" % t.value[0])
	t.lexer.skip(1)


# Build the lexer
lexer = lex.lex(debug=False)


# Tokenize a string, mostly to allow testing
def tokenize(data):
	'''Use the defined lexer to return a list of tokens from data'''
	tokenized = []
	lexer.input(data)
	while True:
		tok = lexer.token()
		if not tok:
			break
		tokenized.append(tok)
	return tokenized
