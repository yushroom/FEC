from mako.template import Template

tpl_fields = r'''
% for field in fields:
% if field.getter is not None:
// ${field.type} ${field.name} getter
static JSValue js_fe_${T}_${field.name}_getter(JSContext *ctx, JSValueConst this_val)
{
    JSValue ret;
    ${field.type} value;
    ${T} *self = (${T} *)JS_GetOpaque2(ctx, this_val, js_fe_${T}_class_id);
    if (!self)
        return JS_EXCEPTION;
%if field.getter == '':
    value = self->${field.name};
%else:
    ${field.getter}
%endif
    ret = JSValueFrom<${field.type}>(ctx, value);
    return ret;
}
%endif
%if field.setter is not None:
// ${field.type} ${field.name} setter
static JSValue js_fe_${T}_${field.name}_setter(JSContext *ctx, JSValueConst this_val, JSValue val)
{
    ${T} *self = (${T} *)JS_GetOpaque2(ctx, this_val, js_fe_${T}_class_id);
    if (!self)
        return JS_EXCEPTION;
    ${field.type} value;
    if (JSValueTo<${field.type}>(ctx, &value, val))
        return JS_EXCEPTION;
%if field.setter == '':
    self->${field.name} = value;
%else:
    ${field.setter}
%endif
    return JS_UNDEFINED;
}
% endif
% endfor
'''
tpl_fields = Template(tpl_fields)

tpl_function = r'''
% for f in functions:
static JSValue js_fe_${T}_${f['name']}(JSContext *ctx, JSValueConst this_value, int argc, JSValueConst *argv)
{
%if not f['is_static']:
    ${T} *self = (${T} *)JS_GetOpaque2(ctx, this_value, js_fe_${T}_class_id);
    if (!self)
        return JS_EXCEPTION;
% endif

    if (argc != ${len(f['params'])})
        return JS_EXCEPTION;

    // TODO: goto fail and clear values if exception
% for i, p in enumerate(f['params']):
    ${p['type']} ${p['name']};
    if (JSValueTo<${p['type']}>(ctx, &${p['name']}, argv[${i}]))
        return JS_EXCEPTION;
% endfor
%if f['ret'] != 'void':
    ${f['ret']} ret;
%endif

    ${f['code']}

% for i, p in enumerate(f['params']):
    JSValueFree<${p['type']}>(ctx, ${p['name']});
% endfor

%if f['ret'] == 'void':
    return JS_UNDEFINED;
%else:
    return JSValueFrom<${f['ret']}>(ctx, ret);
%endif
}
% endfor
'''
tpl_function = Template(tpl_function)

tpl = r'''
extern "C" {
JSClassID js_fe_${T}_class_id = 0;
}

template<>
inline JSClassID JSGetClassID<${T}>() {
    return js_fe_${T}_class_id;
}

%if dtor:
static void js_fe_${T}_finalizer(JSRuntime *rt, JSValue val) {
    ${T} *self = (${T} *)JS_GetOpaque(val, js_fe_${T}_class_id);
    ${dtor}
}
%endif

static JSClassDef js_fe_${T}_class = {
    "${T}",
% if dtor:
    .finalizer = js_fe_${T}_finalizer,
% else:
    .finalizer = NULL,
% endif
};

% if ctor:
static JSValue js_fe_${T}_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    ${T} *self = NULL;
    JSValue proto;
    JSValue obj = JS_UNDEFINED;
    
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        return proto;
    obj = JS_NewObjectProtoClass(ctx, proto, js_fe_${T}_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj)) {
        return obj;
    }
    
    ${ctor}
    
    JS_SetOpaque(obj, self);
    return obj;
}
% endif

${getter_setter}

${function_defines}

static const JSCFunctionListEntry js_fe_${T}_proto_funcs[] = {
% for f in fields:
    JS_CGETSET_DEF("${f.name}",
%if f.getter is not None:
        js_fe_${T}_${f.name}_getter,
%else:
        NULL,
%endif
%if f.setter is not None:
        js_fe_${T}_${f.name}_setter),
%else:
        NULL),
%endif
% endfor
% for f in functions:
    JS_CFUNC_DEF("${f['name']}", ${len(f['params'])}, js_fe_${T}_${f['name']}),
% endfor
};
'''
tpl = Template(tpl)

tpl_export = r'''
int js_fe_init_extra(JSContext *ctx, JSModuleDef *m) {
%for c in classes:
%if c['ctor']:
    {
		JSValue proto, _class;
		JSClassID class_id = 0;
		JS_NewClassID(&class_id);
		js_fe_${c['name']}_class_id = class_id;
		JS_NewClass(JS_GetRuntime(ctx), class_id, &js_fe_${c['name']}_class);
		proto = JS_NewObject(ctx);
		JS_SetPropertyFunctionList(ctx, proto, js_fe_${c['name']}_proto_funcs, countof(js_fe_${c['name']}_proto_funcs));
		JS_SetClassProto(ctx, class_id, proto);
		const char *class_name = "${c['name']}";
		_class = JS_NewCFunction2(ctx, js_fe_${c['name']}_ctor, class_name, 0, JS_CFUNC_constructor, 0);
		JS_SetConstructor(ctx, _class, proto);
%for f in c['static_functions']:
        JS_SetPropertyStr(ctx, _class, "${f['name']}", JS_NewCFunction(ctx, js_fe_${c['name']}_${f['name']}, "${f['name']}", ${len(f['params'])}));
%endfor
		JS_SetModuleExport(ctx, m, class_name, _class);
	}
%else:
    CreateClass(${c['name']});
%endif
%endfor
    return 0;
}

int js_init_module_fishengine_extra(JSContext *ctx, JSModuleDef *m) {
%for c in classes:
    JS_AddModuleExport(ctx, m, "${c['name']}");
%endfor
    return 0;
}
'''
tpl_export = Template(tpl_export)

def get_jsvalue(type_:str, var:str, js_value:str):
    print(type_, var, js_value)
    if type_ == 'float':
        return f'''
${type_} ${var};
if (JSValueToFloat32(ctx, &${var}, ${js_value}
    return JS_EXCEPTION;
        '''.strip()
    if type_ == 'int':
        return f'''
${type_} ${var};
if (JSValueToInt32(ctx, &${var}, ${js_value}
    return JS_EXCEPTION;
        '''.strip()
    
    print('unknown type', type_)
    # exit()



class Field:
    def __init__(self, name:str, type_:str, getter:str, setter:str):
        self.name = name
        self.type = type_
        self.getter = getter
        self.setter = setter

import json
with open('classes.json') as f:
    classes = json.load(f)

output = r''' /* auto generated by codegen tools, do NOT modified this file directly. see engine/source/codegen*/
#include "jsbinding.hpp"
extern "C" {
extern World *defaultWorld;
}

'''
for c in classes:
    T:str = c['name']
    fields = [Field(x['name'], x['type'], x['getter'], x['setter']) for x in c['properties']]
    # for f in fields:
    #     assert(f.type in 'float')
    getter_setter = ''
    if fields.count != 0:
        getter_setter = tpl_fields.render(T=T, fields=fields)
    function_defines = ''
    functions = c['functions']
    static_functions = c['static_functions']
    if len(functions) + len(static_functions) > 0:
        for f in functions:
            f['is_static'] = False
        for f in static_functions:
            f['is_static'] = True
        # print(functions+static_functions)
        function_defines = tpl_function.render(T=T, functions=functions+static_functions)
    output += tpl.render(T=T, fields=fields, ctor=c['ctor'], dtor=c['dtor'], functions=functions,
    getter_setter=getter_setter, function_defines=function_defines)

output += r'''
extern "C" {
int js_init_module_fishengine_extra(JSContext *ctx, JSModuleDef *m);
int js_fe_init_extra(JSContext *ctx, JSModuleDef *m);
JSClassID js_def_class(JSContext *ctx, JSClassDef *class_def,
                       const JSCFunctionListEntry proto_funcs[],
                       int proto_func_count);
}

#define CreateClass(name)                                                    \
    js_fe_##name##_class_id =                                                \
        js_def_class(ctx, &js_fe_##name##_class, js_fe_##name##_proto_funcs, \
                     countof(js_fe_##name##_proto_funcs))
'''

output += tpl_export.render(classes=classes)

output_path = 'jsbinding.gen.cpp'
with open(output_path, 'w') as f:
    f.write(output)

print("done. write to", output_path)

# import os
# os.system('cd .. && clang-format -i jsbinding.gen.inl -style=file')