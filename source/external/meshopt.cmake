CPMAddPackage(
    NAME meshoptimizer
    GITHUB_REPOSITORY zeux/meshoptimizer
    VERSION 0.20
)
set_target_properties(meshoptimizer PROPERTIES VS_GLOBAL_VcpkgEnabled false)


set_property(TARGET meshoptimizer PROPERTY FOLDER "External/")