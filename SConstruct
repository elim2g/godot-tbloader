#!/usr/bin/env python
import os
import sys

env = SConscript("godot-cpp/SConstruct")

# For the reference:
# - CCFLAGS are compilation flags shared between C and C++
# - CFLAGS are for C-specific compilation flags
# - CXXFLAGS are for C++-specific compilation flags
# - CPPFLAGS are for pre-processor flags
# - CPPDEFINES are for pre-processor defines
# - LINKFLAGS are for linking flags

# tweak this if you want to use different folders, or more folders, to store your source code in.
env.Append(CPPPATH=["src/", "src/map/", "src/secure_store/", "src/serialization/"])
sources = Glob("src/*.cpp")
sources += Glob("src/builders/*.cpp")
sources += Glob("src/map/*.cpp")
sources += Glob("src/secure_store/*.cpp")
sources += Glob("src/serialization/*.cpp")

try:
	doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
	sources.append(doc_data)
except AttributeError:
	print("Not including class reference as we're targeting a pre-4.3 baseline.")

plat = env["platform"]

if plat == "windows":
	env.Append(LIBS=["Advapi32"])

elif plat == "osx":
	env.Append(LINKFLAGS=["-framework", "Security", "-framework", "CoreFoundation"])

# libsecret-1-dev libsecret-devel
# eg
# sudo apt install libsecret-1-dev
# sudo dnf install libsecret-devel
# sudo pacman -S libsecret
elif plat == "linux":
	env.ParseConfig("pkg-config --cflags --libs libsecret-1")

if env["platform"] == "windows" and env["target"] == "template_debug":
	env.Append(LINKFLAGS=["/DEBUG"])

if env["platform"] == "osx":
	library = env.SharedLibrary(
		"addons/libturnt/bin/libturnt.{}.framework/libturnt.{}".format(
			env["platform"], env["platform"]
		),
		source=sources,
	)
else:
	library = env.SharedLibrary(
		"addons/libturnt/bin/libturnt.{}.{}{}".format(
			env["platform"], env["arch"], env["SHLIBSUFFIX"]
		),
		source=sources,
	)

Default(library)
