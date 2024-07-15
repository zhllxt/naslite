cmake_minimum_required(VERSION 3.20)

project(naslite)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set_property (GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
endif()
message("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

# remove last end of "/"
string(REGEX REPLACE "/$" "" CMAKELISTS_DIR_PATH ${CMAKE_CURRENT_SOURCE_DIR})
# get current relative dir name and set target name
string(REGEX REPLACE ".*/(.*)" "\\1" CMAKELISTS_DIR_NAME ${CMAKELISTS_DIR_PATH})
# set root directory
set(PROJECT_ROOT_DIR ${CMAKELISTS_DIR_PATH})
message("PROJECT_ROOT_DIR = ${PROJECT_ROOT_DIR}")

#-------------------------------------------------------------------------------
function (DoGroupSources curdir rootdir foldername folder)
    file (GLOB children RELATIVE ${PROJECT_ROOT_DIR}/${curdir} ${PROJECT_ROOT_DIR}/${curdir}/*)
    foreach (child ${children})
        if (IS_DIRECTORY ${PROJECT_ROOT_DIR}/${curdir}/${child})
            DoGroupSources (${curdir}/${child} ${rootdir} ${foldername} ${folder})
        elseif (${child} STREQUAL "CMakeLists.txt")
            source_group("" FILES ${PROJECT_ROOT_DIR}/${curdir}/${child})
        else()
            string (REGEX REPLACE ^${rootdir} ${folder} groupname ${curdir})
            string (REPLACE "/" "\\" groupname ${groupname})
            source_group (${foldername}${groupname} FILES ${PROJECT_ROOT_DIR}/${curdir}/${child})
        endif()
    endforeach()
endfunction()

function (GroupSources curdir folder)
    string (FIND ${curdir} "/" Separator)
    if(${Separator} EQUAL -1)
        set(Separator 0)
    else ()
        math(EXPR Separator "${Separator} + 1")
    endif()
    string (SUBSTRING ${curdir} ${Separator} -1 foldername)
    DoGroupSources (${curdir} ${curdir} ${foldername} ${folder})
endfunction()
#-------------------------------------------------------------------------------

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include_directories (${CMAKE_SOURCE_DIR}/3rd)
include_directories (${CMAKE_SOURCE_DIR}/3rd/openssl/include)
include_directories (${CMAKE_SOURCE_DIR}/3rd/mimalloc/include)
include_directories (${CMAKE_SOURCE_DIR}/include)

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
	set(OPENSSL_LIBS libssl.a libcrypto.a)
	set(GENERAL_LIBS -lpthread -lrt -ldl stdc++fs)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(LINK_LIBS_DIR ${CMAKE_SOURCE_DIR}/3rd/openssl/prebuilt/linux/x64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(LINK_LIBS_DIR ${CMAKE_SOURCE_DIR}/3rd/openssl/prebuilt/linux/x86)
    endif()
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
	set(OPENSSL_LIBS "libssl.lib;libcrypto.lib;Crypt32.lib;")
	set(GENERAL_LIBS "ws2_32;mswsock;")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(LINK_LIBS_DIR ${CMAKE_SOURCE_DIR}/3rd/openssl/prebuilt/windows/x64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(LINK_LIBS_DIR ${CMAKE_SOURCE_DIR}/3rd/openssl/prebuilt/windows/x86)
    endif()
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
	set(OPENSSL_LIBS libssl.a libcrypto.a)
	set(GENERAL_LIBS -lpthread -lrt -ldl stdc++fs)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
	set(OPENSSL_LIBS libssl.a libcrypto.a)
	set(GENERAL_LIBS -lpthread -ldl)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(LINK_LIBS_DIR ${CMAKE_SOURCE_DIR}/3rd/openssl/prebuilt/mac)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(LINK_LIBS_DIR ${CMAKE_SOURCE_DIR}/3rd/openssl/prebuilt/mac)
    endif()
ELSE ()
	set(OPENSSL_LIBS libssl.a libcrypto.a)
	set(GENERAL_LIBS -lpthread -lrt -ldl stdc++fs)
ENDIF (CMAKE_SYSTEM_NAME MATCHES "Linux")

link_directories(${LINK_LIBS_DIR})

add_subdirectory(3rd/boost)

file (GLOB_RECURSE SRC_ALL_FILES ${PROJECT_ROOT_DIR}/naslite/*.*)

GroupSources (naslite "/")

set(MainAppName naslite)

add_executable(
    ${MainAppName}
    ${SRC_ALL_FILES}
    ${CMAKE_SOURCE_DIR}/3rd/mimalloc/src/static.c
)

target_link_libraries(${MainAppName} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(${MainAppName} ${GENERAL_LIBS})
target_link_libraries(${MainAppName} ${OPENSSL_LIBS})
target_link_libraries(${MainAppName} ${BOOST_LIBRARIES})

if (MSVC)
    target_compile_definitions (${MainAppName} PRIVATE
        #-D_WIN32_WINNT=0x0601
        #-D_SCL_SECURE_NO_WARNINGS=1
        #-D_CRT_SECURE_NO_WARNINGS=1
        -D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
        -D_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
    )

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
      target_compile_options(${MainAppName} PRIVATE /bigobj /JMC)
    else()
      target_compile_options(${MainAppName} PRIVATE /bigobj /Zc:__cplusplus /MP /JMC /GS /permissive-)
    endif ()

    target_compile_options(${MainAppName} PRIVATE
        $<$<CONFIG:Release>: /Ob1>
        # Other compile options for all configurations
    )
endif()

set(PreposeAppName prepose)

add_executable(
    ${PreposeAppName}
    ${PROJECT_ROOT_DIR}/prepose/main.cpp
)

target_link_libraries(${PreposeAppName} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(${PreposeAppName} ${GENERAL_LIBS})

if (MSVC)
    target_compile_definitions (${PreposeAppName} PRIVATE
        #-D_WIN32_WINNT=0x0601
        #-D_SCL_SECURE_NO_WARNINGS=1
        #-D_CRT_SECURE_NO_WARNINGS=1
        -D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
        -D_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
    )

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
      target_compile_options(${PreposeAppName} PRIVATE /bigobj /JMC)
    else()
      target_compile_options(${PreposeAppName} PRIVATE /bigobj /Zc:__cplusplus /MP /JMC)
    endif ()
endif()

if(WIN32)
    add_library(windows-kill-library SHARED
        ${PROJECT_ROOT_DIR}/3rd/windows-kill-master/windows-kill-library/ctrl-routine.cpp
        ${PROJECT_ROOT_DIR}/3rd/windows-kill-master/windows-kill-library/ctrl-routine.h
        ${PROJECT_ROOT_DIR}/3rd/windows-kill-master/windows-kill-library/remote-process.cpp
        ${PROJECT_ROOT_DIR}/3rd/windows-kill-master/windows-kill-library/remote-process.h
        ${PROJECT_ROOT_DIR}/3rd/windows-kill-master/windows-kill-library/sender.cpp
        ${PROJECT_ROOT_DIR}/3rd/windows-kill-master/windows-kill-library/sender.h
        ${PROJECT_ROOT_DIR}/3rd/windows-kill-master/windows-kill-library/signal.cpp
        ${PROJECT_ROOT_DIR}/3rd/windows-kill-master/windows-kill-library/signal.h
        ${PROJECT_ROOT_DIR}/3rd/windows-kill-master/windows-kill-library/stdafx.cpp
        ${PROJECT_ROOT_DIR}/3rd/windows-kill-master/windows-kill-library/stdafx.h
        ${PROJECT_ROOT_DIR}/3rd/windows-kill-master/windows-kill-library/targetver.h
        ${PROJECT_ROOT_DIR}/3rd/windows-kill-master/windows-kill-library/windows-kill-library.cpp
        ${PROJECT_ROOT_DIR}/3rd/windows-kill-master/windows-kill-library/windows-kill-library.h
    )
    target_compile_definitions (windows-kill-library PRIVATE
        -D_WINDOWS
        -D_USRDLL
        -DWINDOWSKILLLIBRARY_EXPORTS
    )
    target_link_libraries(windows-kill-library Dbghelp.lib)
    set_target_properties(windows-kill-library PROPERTIES IMPORTED_IMPLIB windows-kill-library.lib)

    #target_include_directories(${MainAppName} PRIVATE
    #    ${PROJECT_ROOT_DIR}/3rd/windows-kill-master/windows-kill-library
    #)
    target_link_libraries(${MainAppName} windows-kill-library)
endif()
