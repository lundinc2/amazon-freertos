afr_module(credentials)

set(src_dir "${CMAKE_CURRENT_LIST_DIR}/src")
set(inc_dir "${CMAKE_CURRENT_LIST_DIR}/include")

afr_module_sources(
    ${AFR_CURRENT_MODULE}
    PRIVATE
        "${src_dir}/aws_clientcredential_keys.c"
        "${inc_dir}/aws_clientcredential_keys.h"
        "${inc_dir}/aws_clientcredential.h"
)
afr_module_include_dirs(
    ${AFR_CURRENT_MODULE}
    PUBLIC "${inc_dir}"
)
afr_module_dependencies(
    ${AFR_CURRENT_MODULE}
)

