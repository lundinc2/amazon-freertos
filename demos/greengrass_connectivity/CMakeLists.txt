# Greengrass demo
afr_demo_module()

afr_set_demo_metadata(ID "GREENGRASS_DISCOVERY_DEMO")
afr_set_demo_metadata(DESCRIPTION "An example on how to discover and connect to AWS Greengrass")
afr_set_demo_metadata(DISPLAY_NAME "AWS Greengrass Discovery")

afr_module_sources(
    ${AFR_CURRENT_MODULE}
    INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/aws_greengrass_discovery_demo.c"
)

afr_module_include_dirs(
    ${AFR_CURRENT_MODULE}
    INTERFACE
        ${AFR_DEMOS_DIR}/common/http_demo_helpers
)

afr_module_dependencies(
    ${AFR_CURRENT_MODULE}
    INTERFACE
        AFR::greengrass
        AFR::mqtt
        AFR::http_demo_helpers
        AFR::core_http
        AFR::backoff_algorithm
        AFR::transport_interface_secure_sockets
        AFR::secure_sockets
        # Add dependency on the core_http_demo_dependencies metadata module 
        # so that FreeRTOS console shows this demo ONLY when the core HTTP library
        # is selected on the console.
        AFR::core_http_demo_dependencies
)
