-- CssToRcss — CSS → RCSS converter tool (design-tool import path).
-- Self-contained (stdlib only); the lib is also linked by the Editor for
-- property-typed editing and CSS import. Picked up by the root
-- `includes("src/**/xmake.lua")` — no root changes needed.

target("CssToRcssLib")
    set_kind("static")
    set_languages("clatest", "cxx20")
    set_group("Tools")
    add_files("Core/**.cpp")
    add_headerfiles("Core/**.h")
    add_includedirs("Core", {public = true})

target("CssToRcss")
    set_kind("binary")
    set_languages("clatest", "cxx20")
    set_group("Tools")
    add_deps("CssToRcssLib")
    add_files("Cli/**.cpp")
    add_includedirs("Core", {public = true})
    set_targetdir("$(projectdir)/src/Tools/CssToRcss")
