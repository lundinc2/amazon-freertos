afr_module(NAME pkcs11_implementation)

set(inc_dir "${CMAKE_CURRENT_LIST_DIR}/include")
set(test_dir "${CMAKE_CURRENT_LIST_DIR}/test")
set(ecc608a_dir "${CMAKE_CURRENT_LIST_DIR}/ecc608a")

afr_module_include_dirs(
    ${AFR_CURRENT_MODULE}
    PUBLIC "${inc_dir}"
    # Requires standard/common/include/private/aws_default_root_certificates.h
    PRIVATE "${AFR_MODULES_C_SDK_DIR}/standard/common/include/private"
    # Requires aws_ota_codesigner_certificate.h
    "$<IF:${AFR_IS_TESTING},${AFR_MODULES_FREERTOS_PLUS_DIR}/aws/ota/test,${AFR_DEMOS_DIR}/include>"
)


if(ECC608_IN_USE)
    afr_module_dependencies(
        ${AFR_CURRENT_MODULE}
        PUBLIC
            3rdparty::pkcs11 
            AFR::pkcs11
            AFR::crypto
            AFR::common
            AFR::utils
        PRIVATE
            AFR::${AFR_CURRENT_MODULE}::mcu_port
    )

    afr_module_sources(
        ${AFR_CURRENT_MODULE}
        PUBLIC
            "${ecc608a_dir}/atca_cert_chain.h"
            "${ecc608a_dir}/atca_cert_chain.c"
            "${ecc608a_dir}/iot_pkcs11_secure_element.c"
    )

    afr_module_include_dirs(
        ${AFR_CURRENT_MODULE}
        PUBLIC
            "${inc_dir}"
    )
else()
    set(src_dir "${CMAKE_CURRENT_LIST_DIR}/mbedtls")
    afr_module_dependencies(
        ${AFR_CURRENT_MODULE}
        PUBLIC
            3rdparty::pkcs11 
            AFR::pkcs11
            AFR::crypto
            3rdparty::mbedtls
            AFR::common
            AFR::utils
        PRIVATE
            AFR::${AFR_CURRENT_MODULE}::mcu_port
    )


    afr_module_sources(
        ${AFR_CURRENT_MODULE}
        PUBLIC
            "${src_dir}/iot_pkcs11_mbedtls.c"
            "${src_dir}/threading_alt.h"
            "${inc_dir}/iot_pkcs11_pal.h"
    )

    afr_module_include_dirs(
        ${AFR_CURRENT_MODULE}
        PUBLIC
            "${inc_dir}"
    )

endif()


# PKCS11 test
afr_test_module()
afr_module_sources(
    ${AFR_CURRENT_MODULE}
    INTERFACE
        "${test_dir}/iot_test_pkcs11.c"
)
afr_module_dependencies(
    ${AFR_CURRENT_MODULE}
    INTERFACE
        AFR::pkcs11
        AFR::crypto
)
