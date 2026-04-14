idf_component_register(
    SRCS
        "coinbase_decoder.c"
        "segwit_addr.c"
        "base58.c"

    INCLUDE_DIRS
        "include"

    REQUIRES
        "mbedtls"
        "bm1397"
)
