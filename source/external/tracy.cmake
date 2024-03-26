CPMAddPackage(
    NAME tracy
    GITHUB_REPOSITORY wolfpld/tracy
    VERSION 0.10
)
set_target_properties(TracyClient PROPERTIES VS_GLOBAL_VcpkgEnabled false)

set_property(TARGET TracyClient PROPERTY FOLDER "External/")