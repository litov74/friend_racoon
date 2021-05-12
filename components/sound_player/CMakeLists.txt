set(COMPONENT_SRCS sound_player.c)
set(COMPONENT_ADD_INCLUDEDIRS ./include)
set(COMPONENT_REQUIRES  esp_http_client
                        uuid)
register_component()