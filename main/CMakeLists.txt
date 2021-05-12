set(COMPONENT_SRCDIRS   ./
                        ./app)
set(COMPONENT_SRCS *.c)
set(COMPONENT_ADD_INCLUDEDIRS ./app/include)
set(COMPONENT_REQUIRES  app_update
                        audio_board
                        audio_hal
                        esp32
                        esp_http_server
                        esp-tls
                        heap
                        json
                        mbedtls
                        nvs_flash
                        partition_table
                        sound_player
                        sound_recorder
                        spiffs
                        uuid)
register_component()