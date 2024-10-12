#!/usr/bin/env python
import os

if not os.path.isfile("src/ogg/include/ogg/config_types.h"):
    os.system("cd src/ogg && ./autogen.sh && ./configure")

libname = "mp3andvorbistowav"

env = SConscript("godot-cpp/SConstruct")

env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp")

env.Append(CPPPATH=["src/vorbis/include/", "src/vorbis/lib/", "src/ogg/include/"])
sources += [
    "src/ogg/src/bitwise.c",
    "src/ogg/src/framing.c",

    "src/vorbis/lib/mdct.c",
    "src/vorbis/lib/smallft.c",
    "src/vorbis/lib/block.c",
    "src/vorbis/lib/envelope.c",
    "src/vorbis/lib/window.c",
    "src/vorbis/lib/lsp.c",
    "src/vorbis/lib/lpc.c",
    "src/vorbis/lib/analysis.c",
    "src/vorbis/lib/synthesis.c",
    "src/vorbis/lib/psy.c",
    "src/vorbis/lib/info.c",
    "src/vorbis/lib/floor1.c",
    "src/vorbis/lib/floor0.c",
    "src/vorbis/lib/res0.c",
    "src/vorbis/lib/mapping0.c",
    "src/vorbis/lib/registry.c",
    "src/vorbis/lib/codebook.c",
    "src/vorbis/lib/sharedbook.c",
    "src/vorbis/lib/lookup.c",
    "src/vorbis/lib/bitrate.c",
]

if env["target"] in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Not including class reference as we're targeting a pre-4.3 baseline.")

file = "{}{}{}".format(libname, env["suffix"], env["SHLIBSUFFIX"])

if env["platform"] == "macos" or env["platform"] == "ios":
    platlibname = "{}.{}.{}".format(libname, env["platform"], env["target"])
    file = "{}.framework/{}".format(env["platform"], platlibname, platlibname)

libraryfile = "bin/{}/{}".format(env["platform"], file)
library = env.SharedLibrary(
    libraryfile,
    source=sources,
)

Default(library)
