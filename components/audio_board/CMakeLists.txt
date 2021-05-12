set(COMPONENT_SRCS  ./board/board.c
                    ./board/board_pins_config.c)
set(COMPONENT_ADD_INCLUDEDIRS   ./board
                                ./include)
set(COMPONENT_REQUIRES  audio_hal
                        button)
register_component()