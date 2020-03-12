keywords = {
    'int': 'INT',
    'uint32_t': 'UINT32',
    'float': 'FLOAT',
    'string': 'STRING',
    'bool': 'BOOL',
    'ArrayBuffer': 'ARRAY_BUFFER',
    'TypedArray': 'TYPED_ARRAY',
    'Array': 'ARRAY',
    'Entity': 'ENTITY',
    'float3': 'FLOAT3',
    'float4': 'FLOAT4',
    'quat': 'QUAT',
    'float4x4': 'FLOAT4X4',
    'class': 'CLASS',
    'void': 'VOID',
    'setter': 'SETTER',
    'getter': 'GETTER',
    'ctor': 'CTOR',
    'dtor': 'DTOR',
    'enum': 'ENUM',
    'include': 'INCLUDE',
    'static': 'STATIC',
}

tokens = (
    'IDENT',
    # 'COMMENT',
    'CODE',
    # 'COMPONENT',
) + tuple(keywords.values())
# print(tokens)

def t_CODE(t):
    r'{{(.|\n)*?}}'
    t.value = t.value[2:-2]
    t.lexer.lineno += t.value.count('\n')
    return t

# t_INCLUDE = r'(\#include(s)\".*\")'

def t_INCLUDE(t):
    r'\#include\s\".*\"'
    # print(t)
    return t

literals = '{}();,*'

def t_semicolon(t):
    r';'
    t.type = ';'
    return t

def t_comma(t):
    r','
    t.type = ','
    return t

def t_lbrace(t):
    r'\{'
    t.type = '{'
    return t

def t_rbrace(t):
    r'\}'
    t.type = '}'
    return t

def t_lparen(t):
    r'\('
    t.type = '('
    return t

def t_rparen(t):
    r'\)'
    t.type = ')'
    return t

def t_star(t):
    r'\*'
    t.type = '*'
    return t

def t_IDENT(t):
    r'[a-zA-Z_][a-zA-Z_0-9]*'
    # t.type = keywords.get(t.value, 'IDENT')
    if t.value in keywords:
        t.type = keywords[t.value]
    # elif t.value in components:
    #     t.type = 'COMPONENT'
    # print(t)
    return t

def t_COMMENT(t):
    r'(/\*(.|\n)*?\*/)|(//.*)'
    t.lexer.lineno += t.value.count("\n")
    pass

def t_newline(t):
    r'\n+'
    t.lexer.lineno += len(t.value)

t_ignore = ' \t'

def t_error(t):
    print("Illegal character '%s'" % t.value[0])
    t.lexer.skip(1)
    exit()

import ply.lex as lex
lexer = lex.lex()

def make_empty_class():
    return {
        'name': '',
        'properties': [],
        'functions': [],
        'static_functions': [],
        'ctor': None,
        'dtor': None,
    }

klasses = []
klass = make_empty_class()

def p_start(p):
    '''start : includes classes
        | classes
    '''
    # print(p[1])

def p_includes(p):
    '''includes : INCLUDE
        | includes INCLUDE
    '''
    pass

def p_classes(p):
    '''classes : class
            | classes class
    '''
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[0] = p[1]
        p[1].append(p[2])

def p_class(p):
    '''class : CLASS class_name '{' class_statements '}' ';'
    '''
    p[0] = (p[2], p[4])
    # print('class', p[2])
    global klass
    klass['name'] = p[2]
    klasses.append(klass)
    klass = make_empty_class()

def p_empty_class(p):
    '''class : CLASS class_name '{' '}' ';'
    '''
    p[0] = (p[2], [])
    # print('class', p[2])
    global klass
    klass['name'] = p[2]
    klasses.append(klass)
    klass = make_empty_class()

def p_class_name(p):
    '''class_name : IDENT
    '''
    p[0] = p[1]

def p_class_statements(p):
    '''class_statements : class_statement
        | class_statements class_statement
    '''
    # print(p[1])
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[0] = p[1]
        p[1].append(p[2])

def p_class_statement(p):
    '''class_statement : property_stmt
        | constructor
        | deconstructor
        | static_function
    '''
    p[0] = p[1]

def p_class_statement2(p):
    '''class_statement : function
    '''
    p[0] = p[1]
    global klass
    klass['functions'].append(p[1])

def p_property_stmt(p):
    '''property_stmt : decl_stmt ';'
        | decl_stmt property_getter_setter
    '''
    global klass
    # o = {'type':p[1][0], 'name':p[1][1]}
    o = p[1]
    o['getter'] = None
    o['setter'] = None
    # o = {'getter': None, 'setter': None}
    getter_setters = p[2]
    if p[2] == ';':
        getter_setters = [('getter', ''), ('setter', '')]
    for k, v in getter_setters:
        o[k] = v
    p[0] = o
    klass['properties'].append(p[0])

def p_property_getter_setter(p):
    '''property_getter_setter : '{' property_getter property_setter '}'
        | '{' property_setter property_getter '}'
        | '{' property_getter '}'
        | '{' property_setter '}'
    '''
    if len(p) == 5:
        p[0] = [p[2], p[3]]
    else:
        p[0] = [p[2]]

def p_property_getter(p):
    '''property_getter : GETTER CODE
        | GETTER ';'
    '''
    getter = p[2]
    if getter == ';':
        getter = ''
    p[0] = ('getter', getter)

def p_property_setter(p):
    '''property_setter : SETTER CODE
        | SETTER ';'
    '''
    setter = p[2]
    if setter == ';':
        setter = ''
    p[0] = ('setter', setter)

def p_decl_stmt(p):
    '''decl_stmt : type_dec IDENT
    '''
    p[0] = {'type':p[1], 'name':p[2]}

def p_constructor(p):
    '''constructor : CTOR '(' ')' CODE
    '''
    p[0] = 'ctor'
    # print('ctor')
    global klass
    klass['ctor'] = p[4]

def p_deconstructor(p):
    '''deconstructor : DTOR '(' ')' CODE
    '''
    p[0] = 'dtor'
    global klass
    klass['dtor'] = p[4]

def p_static_function(p):
    '''static_function : STATIC function
    '''
    global klass
    p[0] = p[2]
    klass['static_functions'].append( p[2] )

def p_function(p):
    '''function : type_dec IDENT '(' ')' CODE
                | VOID IDENT '(' ')' CODE
    '''
    p[0] = {'name': p[2], 'code': p[5], 'params': [], 'ret': p[1]}

def p_function_with_params(p):
    '''function : type_dec IDENT '(' function_params ')' CODE
                | VOID IDENT '(' function_params ')' CODE
    '''
    p[0] = {'name': p[2], 'code': p[6], 'params': p[4], 'ret': p[1]}

def p_function_params(p):
    '''function_params : function_param
        | function_params ',' function_param
    '''
    if len(p) > 2:
        p[0] = p[1]
        p[1].append(p[3])
    else:
        p[0] = [p[1]]

def p_function_param(p):
    '''function_param : type_dec IDENT'''
    # p[0] = (p[1], p[2])
    p[0] = {'type':p[1], 'name':p[2]}


def p_type_dec(p):
    '''type_dec : IDENT
        | type_pointer_dec
        | type_enum
        | type_internal_dec
    '''
    p[0] = p[1]

def p_type_internal_dec(p):
    '''type_internal_dec : INT
        | FLOAT
        | UINT32
        | ENTITY
        | ARRAY
        | ARRAY_BUFFER
        | TYPED_ARRAY
        | FLOAT4
        | FLOAT3
        | QUAT
        | FLOAT4X4
        | STRING
        | BOOL
    '''
    p[0] = p[1]

def p_type_enum(p):
    '''type_enum : ENUM IDENT
    '''
    p[0] = p[1] + ' ' + p[2]

def p_type_pointer_dec(p):
    '''type_pointer_dec : IDENT '*'
    '''
    p[0] = p[1] + ' *'

def p_error(p):
    print("Syntax error at {}".format(p))
    exit()


with open('template.cpp') as f:
    data = f.read()
# print(data)

# lexer.input(data)
# old_lineno = lexer.lineno
# # Tokenize
# while True:
#     tok = lexer.token()
#     if not tok: 
#         break      # No more input
#     print(tok)
# lexer.lineno = old_lineno

# lexer.klasses = []
# lexer.klass = make_empty_class()

import ply.yacc as yacc
parser = yacc.yacc()
parser.parse(data, debug=False)

# print(klasses)
import json

with open('classes.json', 'w') as f:
    f.write(json.dumps(klasses, indent=2))