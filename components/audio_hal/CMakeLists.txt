set(COMPONENT_SRCS  ./driver/mp45dt02/mp45dt02.c
                    ./driver/vs1053b/vs1053b.c)
set(COMPONENT_ADD_INCLUDEDIRS   ./driver/mp45dt02
                                ./driver/vs1053b)
set(COMPONENT_REQUIRES audio_board)
register_component()