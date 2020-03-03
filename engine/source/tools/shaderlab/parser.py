import ply.yacc as yacc
from .shaderlab import *
from .lexer import tokens

# start point 'Shader "Name" { ... }'
def p_shader(p):
	'''shader : SHADER STRING LBRACE shader_subsections RBRACE'''
	p[0] = Shader(p[2], p[4])


# Shader subsections

# possible subsections of shader:
#   Properties*, SubShader, Fallback*, CustomEditor*
def p_shader_subsections(p):
	'''shader_subsections : shader_subsections shader_subsection
                          | shader_subsection'''
	if len(p) > 2:
		p[0] = p[1]
		p[1].append(p[2])
	else:
		p[0] = [p[1]]

def p_shader_subsection_hlslinclude(p):
	'''shader_subsection : HLSLINCLUDE'''
	p[0] = { 'HLSLINCLUDE': p[1] }

def p_shader_subsection_properties(p):
	'''shader_subsection : PROPERTIES LBRACE properties RBRACE'''
	p[0] = { 'props': p[3] }


def p_shader_subsection_subshader(p):
	'''shader_subsection : SUBSHADER LBRACE subshader_subsections RBRACE'''
	p[0] = SubShader(p[3])

def p_shader_subsection_fallback(p):
	'''shader_subsection : FALLBACK STRING
	                     | FALLBACK IDENT'''
	p[0] = { 'fallback': FallBack(p[2]) }


def p_shader_subsection_customeditor(p):
	'''shader_subsection : CUSTOMEDITOR STRING'''
	p[0] = { 'customed': CustomEditor(p[2]) }

# SubShader subsections

def p_subshader_subsections(p):
	'''subshader_subsections : subshader_subsections subshader_subsection
	                         | subshader_subsection'''
	if len(p) > 2:
		p[0] = p[1]
		p[1].append(p[2])
	else:
		p[0] = [p[1]]

def p_subshader_subsection_tags(p):
	'''subshader_subsection : tag_block'''
	p[0] = p[1]

def p_subshader_subsection_pass(p):
	'''subshader_subsection : pass_block'''
	p[0] = p[1]

# render states can be define in SubShader or Pass
def p_subshader_render_state(p):
	'''subshader_subsection : render_state'''
	p[0] = p[1]

# Pass sections

def p_pass_block(p):
	'''pass_block : PASS LBRACE pass_sections RBRACE'''
	p[0] = Pass(p[3])

def p_pass_sections(p):
	'''pass_sections : pass_sections pass_section
	                 | pass_section'''
	if len(p) > 2:
		p[0] = p[1]
		p[1].append(p[2])
	else:
		p[0] = [p[1]]

def p_pass_section_tags(p):
	'''pass_section : tag_block'''
	p[0] = { 'tags': p[1] }

def p_pass_section_bind(p):
	'''pass_section : bind_block'''
	p[0] = { 'bind': p[1] }

def p_pass_section_fog(p):
	'''pass_section : fog_block'''
	p[0] = { 'fog': p[1] }

def p_pass_section_name(p):
	'''pass_section : NAME STRING'''
	p[0] = { 'name': p[2] }

def p_pass_section_state(p):
	'''pass_section : render_state'''
	p[0] = p[1]

def p_render_state(p):
	'''render_state : pass_state1
	                | pass_state2
	                | pass_section_cull
	                | pass_section_zteset
	                | pass_section_zwrite
	                | pass_section_blend
	                | pass_section_blendop
	                | pass_section_state_colormask
					| pass_section_state_stencil'''
	p[0] = p[1]

def p_render_state_value(p):
	'''render_state_value : IDENT 
	                      | ATTRIBUTE'''
	p[0] = p[1]

def p_pass_section_state_cull(p):
	'''pass_section_cull : CULL render_state_value'''
	p[0] = Pair(p[1], p[2])

def p_pass_section_state_ztest(p):
	'''pass_section_zteset : ZTEST render_state_value'''
	p[0] = Pair(p[1], p[2])

def p_pass_section_state_zwrite(p):
	'''pass_section_zwrite : ZWRITE render_state_value'''
	p[0] = Pair(p[1], p[2])

def p_pass_section_state_blend(p):
	'''pass_section_blend : BLEND render_state_value
	                      | BLEND render_state_value render_state_value COMMA render_state_value render_state_value
	                      | BLEND render_state_value render_state_value'''
	p[0] = p[1]

def p_pass_section_state_blendop(p):
	'''pass_section_blendop : BLENDOP IDENT COMMA ATTRIBUTE'''
	p[0] = p[1]

def p_pass_section_state_colormask(p):
	'''pass_section_state_colormask : ColorMask NUMBER
	                                | ColorMask ATTRIBUTE NUMBER'''
	print('COLORMASK', p[2])

def p_pass_section_state_stencil(p):
	'''pass_section_state_stencil : Stencil LBRACE WriteMask render_state_value Ref render_state_value Comp render_state_value PASS render_state_value RBRACE'''
	# print('Stencil', p[4], p[6], p[8], p[10])
	p[0] = Pair(p[1], {"WriteMask":p[4], "Ref":p[6], "Comp":p[8], "Pass":p[10]})

# pass can have many render state commands
# full syntax (https://docs.unity3d.com/Manual/SL-Pass.html)
def p_pass_section_state1(p):
	'''pass_state1 : IDENT IDENT
	               | IDENT value
	               | IDENT STRING
	               | IDENT ATTRIBUTE
	               | PASS IDENT'''		# TODO: PASS is a keyword
	p[0] = Pair(p[1], p[2])

def p_pass_section_state2(p):
	'''pass_state2 : IDENT LBRACE pass_states RBRACE'''
	p[0] = Pair(p[1], p[3])

def p_pass_section_states(p):
	'''pass_states : pass_states pass_state1
	               | pass_state1'''
	if len(p) > 2:
		p[0] = p[1]
		p[1].append(p[2])
	else:
		p[0] = [p[1]]

# # for unpacked shaders (would be CGPROGRAM in regular shaderlab)
# def p_pass_section_program(p):
# 	'''pass_section : PROGRAM STRING LBRACE subprograms RBRACE'''
# 	p[0] = Program(p[2], p[4])
def p_pass_section_program(p):
	'''pass_section : HLSLPROGRAM'''
	# print('HLSLPROGRAM')
	p[0] = {'HLSLPROGRAM': p[1]}

# BindChannels

def p_bind_block(p):
	'''bind_block : BINDCHANNELS LBRACE bindings RBRACE'''
	p[0] = p[3]

def p_bindings(p):
	'''bindings : bindings bind_statement
	            | bind_statement'''
	if len(p) > 2:
		p[0] = p[1]
		p[1].append(p[2])
	else:
		p[0] = [p[1]]

def p_bind_statement(p):
	'''bind_statement : BIND STRING COMMA keyword'''
	p[0] = Pair(p[2],p[4])

# Tags

def p_tag_block(p):
	'''tag_block : TAGS LBRACE tag_pairs RBRACE'''
	p[0] = p[3]


def p_tag_pairs(p):
	'''tag_pairs : tag_pairs tag_pair
	             | tag_pair'''
	if len(p) > 2:
		p[0] = p[1]
		p[1].append(p[2])
	else:
		p[0] = [p[1]]

def p_tag_pair(p):
	'''tag_pair : STRING EQUALS STRING'''
	p[0] = Pair(p[1], p[3])

# Fog (Legacy)

def p_fog_block(p):
	'''fog_block : FOG LBRACE fog_commands RBRACE'''
	p[0] = p[3]

def p_fog_commands(p):
	'''fog_commands : fog_commands fog_command
	                | fog_command'''
	if len(p) > 2:
		p[0] = p[1]
		p[1].append(p[2])
	else:
		p[0] = [p[1]]

# Fog is considered Legacy (https://docs.unity3d.com/Manual/SL-Fog.html)
# rule conforms to minimum syntax from samples used
def p_fog_command(p):
	'''fog_command : IDENT value
	               | COLOR vector'''
	p[0] = Pair(p[1], p[2])

# Program / SubProgram

def p_subprograms(p):
	'''subprograms : subprograms subprogram
	               | subprogram'''
	if len(p) > 2:
		p[0] = p[1]
		p[1].append(p[2])
	else:
		p[0] = [p[1]]

def p_subprogram(p):
	'''subprogram : SUBPROGRAM STRING LBRACE subprogram_defs RBRACE'''
	p[0] = SubProgram(p[2], p[4])


def p_subprogram_defs(p):
	'''subprogram_defs : subprogram_defs subprogram_def
	                   | subprogram_def'''
	if len(p) > 2:
		p[0] = p[1]
		p[1].append(p[2])
	else:
		p[0] = [p[1]]

def p_subprogram_def_bind(p):
	'''subprogram_def : BIND STRING keyword'''
	p[0] = Pair(p[2], p[3])


def p_subprogram_def_keywords(p):
	'''subprogram_def : KEYWORDS LBRACE keywords RBRACE'''
	p[0] = p[3]


def p_keywords(p):
	'''keywords : keywords STRING
	            | STRING'''
	if len(p) > 2:
		p[0] = p[1]
		p[1].append(p[2])
	else:
		p[0] = [p[1]]


# TODO number is int really
# TODO SetTexture different
def p_subprogram_def_reg(p):
	'''subprogram_def : keyword NUMBER LBRACKET IDENT RBRACKET
	                  | IDENT NUMBER LBRACKET IDENT RBRACKET 2D NUMBER'''
	p[0] = RegisterEntry(p[4], p[2], p[1])

def p_subprogram_def_asm(p):
	'''subprogram_def : STRING'''
	p[0] = p[1]


# Properties

def p_properties(p):
	'''properties : properties property
                  | property'''
	if len(p) > 2:
		p[0] = p[1]
		p[1].append(p[2])
	else:
		p[0] = [p[1]]


def p_property(p):
	'''property : IDENT pair EQUALS value
	            | property_attribute property'''
	# '''property : IDENT pair EQUALS value'''
	if len(p) == 5:
		p[0] = Property(p[1], p[2], p[4])
	# else:
	# 	p[0] = Property(p[2], p[3], p[5])
	# print("TODO: property with attribute,", p[1])

# # [HideInInspector]
# # [Enum(MinMax, 0, Amplitude, 1)]
def p_property_attribute(p):
	'''property_attribute : ATTRIBUTE'''
	# print('property_attribute', p[1])
	p[0] = p[1]


def p_value(p):
	'''value : texture
			 | vector
			 | NUMBER'''
	p[0] = p[1]


def p_pair(p):
	'''pair : LPAREN STRING COMMA pair_type LPAREN NUMBER COMMA NUMBER RPAREN RPAREN
	        | LPAREN STRING COMMA pair_type RPAREN'''
	# print('pair', p[2], p[4])
	p[0] = [p[2], p[4]]


def p_pair_type(p):
	'''pair_type : FLOAT
	             | COLOR
	             | 2D
	             | INT
	             | VECTOR
	             | RANGE'''
	p[0] = p[1]


# TODO cant be updating this with lexer, should be seperate
def p_keyword(p):
	'''keyword : SHADER
	           | PROPERTIES
	           | SUBSHADER
	           | PASS
	           | TAGS
	           | FOG
	           | PROGRAM
	           | SUBPROGRAM
	           | KEYWORDS
	           | BIND
	           | MATRIX
	           | VECTOR
	           | FLOAT
	           | FALLBACK
	           | RANGE
	           | 2D
	           | COLOR
	           | VERTEX
	           | TEXCOORD
	           | TEXCOORD0
	           | TEXCOORD1'''
	p[0] = p[1]


# (1, 1, 1, 1)
# (1, 1, 1)
def p_vector(p):
	'''vector : LPAREN NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER RPAREN
	          | LPAREN NUMBER COMMA NUMBER COMMA NUMBER RPAREN'''
	if len(p) == 10:
		p[0] = '(' + str(p[2]) + ' ' + str(p[4]) + ' ' + str(p[6]) + ' ' + str(p[8]) + ')'
	else:
		p[0] = '(' + str(p[2]) + ' ' + str(p[4]) + ' ' + str(p[6]) + ')'

def p_texture(p):
	'''texture : STRING LBRACE RBRACE'''
	p[0] = p[1]


# an empty production, makes rules clearer
# def p_empty(p):
# 	'empty :'
# 	pass

def p_error(p):
    print("Syntax error in input! {}".format(p))


# Build the parser
parser = yacc.yacc(debug=False)


def parse(data):
	return parser.parse(data)
