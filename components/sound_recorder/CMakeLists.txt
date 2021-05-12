set(COMPONENT_SRCS sound_recorder.c)
set(COMPONENT_ADD_INCLUDEDIRS ./include)
set(COMPONENT_REQUIRES  esp_http_client
                        spi_flash)
register_component()