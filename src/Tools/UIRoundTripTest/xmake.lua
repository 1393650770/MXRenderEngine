-- UIRoundTripTest — round-trip idempotence regression for UIDocumentModel.
-- Compiles the editor's document-model sources directly (pure stdlib, no
-- Editor/Runtime linkage) + CssToRcssLib for the RCSS parser.

target("UIRoundTripTest")
    set_kind("binary")
    set_languages("clatest", "cxx20")
    set_group("Tools")
    add_deps("CssToRcssLib")
    add_files("main.cpp")
    add_files("../../Editor/UI/UIDocumentModel/UIDocumentModel.cpp")
    add_files("../../Editor/UI/UIDocumentModel/RmlParser.cpp")
    add_files("../../Editor/UI/UIDocumentModel/UIRcssParser.cpp")
    add_files("../../Editor/UI/UIDocumentModel/UIDocumentSerializer.cpp")
    add_packages("boost")
    add_includedirs("$(projectdir)/src/Editor", {public = true})
    add_includedirs("$(projectdir)/src/Runtime", {public = true})
    add_includedirs("$(projectdir)/src/Tools/CssToRcss/Core", {public = true})
    set_targetdir("$(projectdir)/src/Tools/UIRoundTripTest")
