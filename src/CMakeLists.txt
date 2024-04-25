add_library(RelayLib STATIC  Utils.cpp Server/Server.cpp Logger.cpp)

list(APPEND ELF_DIR Server Generator)
set(MAIN_SRC Utils.cpp Logger.cpp)
foreach(ELF ${ELF_DIR})
    aux_source_directory(${ELF} ${ELF})
    add_executable(${ELF} ${MAIN_SRC} ${${ELF}})
    # target_link_libraries(${ELF} ${CMAKE_THREAD_LIBS_INIT})
endforeach(ELF ${ELF_DIR})