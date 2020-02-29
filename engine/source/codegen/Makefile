jsbinding.gen.cpp: classes.json codegen.py
	python3 codegen.py
	cd ../fishengine && clang-format --style=file ../codegen/jsbinding.gen.cpp > jsbinding.gen.cpp
	# cp jsbinding.gen.cpp ..
classes.json: template.cpp parser.py Makefile
	python3 parser.py
all: classes.json jsbinding.gen.cpp