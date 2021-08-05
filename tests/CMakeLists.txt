
FUNCTION(addtest t)
    if(NOT t MATCHES "^[-_.0-9a-zA-Z]+$")
        message(FATAL_ERROR "invalid test name: ${t}")
    endif()

    add_test(
        NAME ${t}
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/${t}.sh
    )
    set(env BINDIR=${CMAKE_BINARY_DIR}
            SRCDIR=${CMAKE_SOURCE_DIR}
            TNAME=${t}
            PATH=${CMAKE_BINARY_DIR}:${CMAKE_BINARY_DIR}/tests:$ENV{PATH}
            SEXPECT_SOCKFILE=${CMAKE_BINARY_DIR}/tests/TEST_${t}.sock
    )
    set_tests_properties(
        ${t} PROPERTIES
        ENVIRONMENT "${env}"
    )
ENDFUNCTION()

add_executable(sleep sleep.c)
add_executable(eof_before_exit eof_before_exit.c)
add_executable(still_open_after_exit still_open_after_exit.c)

#
# glob2re
#
include_directories(${CMAKE_SOURCE_DIR})
add_executable(glob2re glob2re.c ${CMAKE_SOURCE_DIR}/common.c ${CMAKE_SOURCE_DIR}/proto.c)
if (HAVE_LIBRT)
    target_link_libraries(glob2re rt)
endif()

add_test(
    NAME glob2re
    COMMAND ${CMAKE_BINARY_DIR}/tests/glob2re
)

foreach(t
        version
        spawn-ttl
       ) 
    addtest(${t})
endforeach()

foreach(t
        chkerr
        cstring
        chdir-after-exec
        chdir-after-logfile
        cloexit
        close
        eof-before-exit
        expbuf-overflow
        expect_out
        expect-eof
        expect-nocase
        expect-pattern
        interact-re-helper
        kill
        spawn-discard
        spawn-nohup
        spawn-zombie-idle
        spawn-zombie-idle_02
        still-data-after-exit
       ) 
    addtest(${t})
    set_tests_properties(
        ${t} PROPERTIES
        DEPENDS spawn-ttl
    )
endforeach()

if (CMAKE_SYSTEM MATCHES "Linux.*")
    foreach(t
            spawn-nowait
            still-open-after-exit
           )
        addtest(${t})
        set_tests_properties(
            ${t} PROPERTIES
            DEPENDS spawn-ttl
        )
    endforeach()
endif()
