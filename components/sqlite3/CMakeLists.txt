set(COMPONENT_ADD_INCLUDEDIRS "include")

idf_component_register(SRCS "sqlite3.c" "esp32.c" "shox96_0_2.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "private_include"
                       REQUIRES mbedtls spi_flash
                       PRIV_REQUIRES console spiffs esp_system)

set_source_files_properties("sqlite3.c" PROPERTIES
    COMPILE_OPTIONS "-Wno-error;-Wno-implicit-fallthrough;-Wno-unused-function;-Wno-unused-variable;-Wno-maybe-uninitialized;-Wno-char-subscripts;-Wno-unused-but-set-variable;-Wno-old-style-declaration;-Wno-sign-compare;-Wno-unused-parameter;-Wno-unused-value;-Wno-unused-label;-Wno-deprecated-declarations")

set_source_files_properties("shox96_0_2.c" PROPERTIES
    COMPILE_OPTIONS "-Wno-error;-Wno-char-subscripts;-Wno-maybe-uninitialized")

set_source_files_properties("esp32.c" PROPERTIES
    COMPILE_OPTIONS "-Wno-error;-Wno-implicit-function-declaration;-Wno-int-conversion")

target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu99 -DSQLITE_OS_OTHER -DSQLITE_OMIT_LOAD_EXTENSION -DNDEBUG -DF_CPU=240000000L -DESP32 -DCORE_DEBUG_LEVEL=0)
