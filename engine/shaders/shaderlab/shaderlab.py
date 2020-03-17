from enum import IntEnum

keywords = {
	"Shader": "SHADER",
	"Properties": "PROPERTIES",
	"SubShader": "SUBSHADER",
	"Pass": "PASS",
	"Tags": "TAGS",
	"Fog": "FOG",
	"Program": "PROGRAM",
	"SubProgram": "SUBPROGRAM",
	"Keywords": "KEYWORDS",
	"Bind": "BIND",
	"Matrix": "MATRIX",
	"Vector": "VECTOR",
	"Float": "FLOAT",
	"float": "FLOAT",
	"Int": "INT",
	"Fallback": "FALLBACK",
	"CustomEditor": "CUSTOMEDITOR",
	"2D": "2D",
	"Cube": "CUBE",
	"Color": "COLOR",
	# "Off": "OFF",
	"BindChannels": "BINDCHANNELS",
	"Vertex": "VERTEX",
	"TexCoord": "TEXCOORD",
	"TexCoord0": "TEXCOORD0",
	"TexCoord1": "TEXCOORD1",
	"Name": "NAME",
	"Range": "RANGE",
	"Enum": "ENUM",

	# render states
	"Cull": "CULL",
	"ZTest": "ZTEST",
	"ZWrite": "ZWRITE",
	"Offset": "OFFSET",
	"Blend": "BLEND",
	"BlendOp": "BLENDOP",
	"ColorMask": "ColorMask",
	"Stencil": "Stencil",
	"WriteMask": "WriteMask",
	"Ref": "Ref",
	"Comp": "Comp",
}


class ProgramType(IntEnum):
	VERTEX = 0
	FRAGMENT = 1


class ProgramFormat(IntEnum):
	OPENGL = 0
	D3D9 = 1
	D3D11 = 2


class Pair:
	def __init__(self, key, value):
		self.key = key
		self.value = value

	def __str__(self):
		return "\"%s\"=\"%s\"" % (self.key, self.value)

	def __repr__(self):
		return self.__str__()


class RegisterEntry:
	def __init__(self, name, position, type):
		self.name = name
		self.position = int(position)
		self.type = type

	def __repr__(self):
		return '[{}] {} {}'.format(self.position, self.type, self.name)


class Shader:
	def __init__(self, name, sections):
		self.name = name
		self.sections = sections

		self.subshaders = []

		# merge dicts
		merged = {}
		for s in sections:
			if isinstance(s, dict):
				merged.update(s)

		self.properties = merged.get('props')
		if self.properties is None:
			self.properties = []
		self.fallback = merged.get('fallback')
		self.custom_editor = merged.get('customed')
		# self.subshaders = merged.get('subshaders')
		self.hlslinclude = merged.get('HLSLINCLUDE')

		for s in sections:
			if isinstance(s, SubShader):
				self.subshaders.append(s)
				for ps in s.passes:
					if self.hlslinclude:
						ps.hlslprogram = self.hlslinclude + '\n' + ps.hlslprogram
					ps.ParsePragmas()

	@property
	def vertex_programs(self):
		return self._shader_programs(ProgramType.VERTEX)

	@property
	def fragment_programs(self):
		return self._shader_programs(ProgramType.FRAGMENT)

	def _shader_programs(self, ptype):
		v = []
		# TODO must be a better way than this
		for ss in self.subshaders:
			for ps in ss.passes:
				for pg in ps.programs:
					if pg.type == ptype:
						v = pg.subprograms
		return v

	def __str__(self):
		pretty = []
		pretty.append("Shader \"%s\" {\n" % (self.name))
		pretty.append("Properties {\n")
		for p in self.properties:
			pretty.append("\t%s\n" % p)
		pretty.append("}\n") # end props
		for s in self.subshaders:
			pretty.append("%s\n" % s)
		pretty.append("%s\n" % (self.fallback))
		pretty.append("%s\n" % (self.custom_editor))
		pretty.append("}") # end shader
		return "".join(pretty)


# TODO types should be acutal types/classes
class Property:
	def __init__(self, name, pair, value):
		self.name = name
		self.description = pair[0]
		self.type = pair[1]
		self.value = value

	def __str__(self):
		return "%s (\"%s\", %s) = %s" % (self.name, self.description, self.type, self.value)

	def __repr__(self):
		return self.__str__()


class SubShader:
	def __init__(self, sections):
		self.sections = sections
		self.tags = []
		self.passes = []

		for s in sections:
			if isinstance(s, list) and len(s) > 0 and isinstance(s[0], Pair):
				self.tags += s
			elif isinstance(s, Pass):
				self.passes.append(s)

	def __str__(self):
		pretty = []
		pretty.append("SubShader {\n")
		pretty.append("\tTags {")
		for t in self.tags:
			pretty.append(" %s" % t)
		pretty.append(" }\n") # end tags
		for p in self.passes:
			pretty.append("%s" % p)
		pretty.append("}\n") # end subshader
		return "".join(pretty)


class Pass:
	def __init__(self, sections):
		# print('new pass', sections)
		self.sections = sections
		self.programs = []
		self.state = []
		# merge dicts
		merged = {}
		for s in sections:
			if isinstance(s, dict):
				merged.update(s)
			elif isinstance(s, Program):
				self.programs.append(s)
			elif isinstance(s, Pair):
				self.state.append(s)

		self.name = merged.get('name')
		self.tags = merged.get('tags')
		self.fog = merged.get('fog')
		self.bind = merged.get('bind')
		self.hlslprogram = merged.get('HLSLPROGRAM')

		self.vertex_entry = 'vertex'
		self.fragment_entry = 'fragment'
		self.shader_features = []
		self.multi_compiles = []

	def __str__(self):
		pretty = []
		pretty.append("Pass {\n")
		pretty.append("\tName \"%s\"\n" % (self.name))
		pretty.append("\tTags {")
		for t in self.tags:
			pretty.append(" %s" % t)
		pretty.append(" }\n") # end tags
		if self.fog:
			pretty.append("\tFog {")
			for f in self.fog:
				pretty.append(" %s" % f)
			pretty.append(" }\n") # end fog
		pretty.append(str(self.state))
		pretty.append("}\n") # end pass
		return "".join(pretty)

	def ParsePragmas(self):
		lines = self.hlslprogram.split('\n')
		for l in lines:
			ll = l.strip()
			if ll.startswith('#pragma vertex '):
				self.vertex_entry = ll[len('#pragma vertex '):]
			elif ll.startswith('#pragma fragment '):
				self.fragment_entry = ll[len('#pragma fragment '):]
			elif ll.startswith('#pragma shader_feature '):
				sf = ll[len('#pragma shader_feature '):].split(' ')
				self.shader_features.append(sf)
			elif ll.startswith('#pragma multi_compile '):
				mc = ll[len('#pragma multi_compile '):].split(' ')
				self.multi_compiles.append(mc)
		# print('shader_feature', self.shader_features)
		# print('multi_compile', self.multi_compiles)
			

class Program:
	def __init__(self, type, subprograms):
		self.subprograms = subprograms
		if type == 'vp':
			self.type = ProgramType.VERTEX
		elif type == 'fp':
			self.type = ProgramType.FRAGMENT
		else:
			raise ValueError("{} must be either 'vp' or 'fp'".format(type))

	def __repr__(self):
		return 'Program({})'.format(self.type)


class SubProgram:
	def __init__(self, format, sections):
		self.sections = sections
		fmt = format.strip().lower()
		if fmt == "d3d9":
			self.format = ProgramFormat.D3D9
		elif fmt == "d3d11":
			self.format = ProgramFormat.D3D11
		elif fmt == "opengl":
			self.format = ProgramFormat.OPENGL
		else:
			raise NotImplementedError("{} is an unknown format".format(format))

		self.code = None
		self.constants = []
		self.defs = []
		self.keywords = []
		for s in sections:
			if isinstance(s, Pair):
				self.defs.append(s)
			elif isinstance(s, RegisterEntry):
				self.constants.append(s)
			elif isinstance(s, list):
				self.keywords = s
			elif isinstance(s, str):
				self.code = s

	def __repr__(self):
		return 'SubProgram({})'.format(self.format)


class FallBack:
	def __init__(self, value):
		self.value = value

	def __str__(self):
		return "Fallback \"%s\"" % self.value


class CustomEditor:
	def __init__(self, value):
		self.value = value

	def __str__(self):
		return "CustomEditor \"%s\"" % self.value
