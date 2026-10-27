# The Commodore 64 on VICE's x64sc (third_party/vice), driven one frame at a time, for the Android
# app and for host tests.
cmake_minimum_required(VERSION 3.22.1)
project(kairo64_core LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# The emulator runs in real time only optimized, so the core is optimized in debug builds too.
add_compile_options(-O3)

include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckTypeSize)

get_filename_component(THIRD_PARTY "${CMAKE_CURRENT_LIST_DIR}/../third_party" ABSOLUTE)
set(VICE "${THIRD_PARTY}/vice/src")
set(KAIRO "${CMAKE_CURRENT_LIST_DIR}/kairo")
set(GENERATED "${CMAKE_CURRENT_BINARY_DIR}/vice-config")

# zlib, which VICE uses for compressed images.
set(ZLIB_DIR "${THIRD_PARTY}/zlib")
add_library(kairo_zlib STATIC
    "${ZLIB_DIR}/adler32.c" "${ZLIB_DIR}/compress.c" "${ZLIB_DIR}/crc32.c" "${ZLIB_DIR}/deflate.c"
    "${ZLIB_DIR}/gzclose.c" "${ZLIB_DIR}/gzlib.c" "${ZLIB_DIR}/gzread.c" "${ZLIB_DIR}/gzwrite.c"
    "${ZLIB_DIR}/infback.c" "${ZLIB_DIR}/inffast.c" "${ZLIB_DIR}/inflate.c" "${ZLIB_DIR}/inftrees.c"
    "${ZLIB_DIR}/trees.c" "${ZLIB_DIR}/uncompr.c" "${ZLIB_DIR}/zutil.c")
target_include_directories(kairo_zlib PUBLIC "${ZLIB_DIR}")
if(NOT WIN32)
    target_compile_definitions(kairo_zlib PRIVATE HAVE_UNISTD_H)
endif()

# VICE's config.h, from its own template (src/config.h.in): each "#undef NAME" becomes defined when
# the variable NAME is set below, as configure would for a headless build of x64sc.
set(PACKAGE "\"vice\"")
set(PACKAGE_NAME "\"vice\"")
set(PACKAGE_TARNAME "\"vice\"")
set(PACKAGE_VERSION "\"3.10\"")
set(PACKAGE_STRING "\"vice 3.10\"")
set(PACKAGE_BUGREPORT "\"\"")
set(PACKAGE_URL "\"\"")
set(VERSION "\"3.10\"")
set(CONFIGURE_FLAGS "\"Kairo64 (core/CMakeLists.txt)\"")
set(PREFIX "\"/\"")
set(STDC_HEADERS 1)
set(RETSIGTYPE void)
set(USE_GCC 1)
set(HAVE_CXX11 1)
set(USE_HEADLESSUI 1)
set(HAVE_RESID 1)
set(HAVE_ZLIB 1)
set(HAVE_MOUSE 1)
set(HAVE_LIGHTPEN 1)
set(HAVE_OFF_T 1)
set(HAVE_OFF_T_IN_SYS_TYPES 1)
set(HAVE_TIME_T_IN_TIME_H 1)
set(HAVE_TIME_T_IN_TYPES_H 1)
# Sockets, which configure finds on every platform VICE builds for; VICE's monitor and netplay
# code builds only with them, though Kairo64 opens none.
set(HAVE_NETWORK 1)
if(WIN32)
    set(WINDOWS_COMPILE 1)
    set(HAVE_HTONL 1)
    set(HAVE_HTONS 1)
    check_include_files("windows.h;winsock.h" HAVE_WINSOCK_H)
else()
    set(UNIX_COMPILE 1)
    set(LINUX_COMPILE 1)
    foreach(header sys/socket.h netinet/in.h netinet/tcp.h arpa/inet.h netdb.h)
        string(TOUPPER "HAVE_${header}" variable)
        string(REGEX REPLACE "[/.]" "_" variable "${variable}")
        check_include_files("${header}" ${variable})
    endforeach()
    foreach(function socket send bind listen gethostbyname connect recv accept htonl htons)
        string(TOUPPER "HAVE_${function}" variable)
        check_symbol_exists(${function} "sys/types.h;sys/socket.h;netdb.h;arpa/inet.h" ${variable})
    endforeach()
    set(CMAKE_EXTRA_INCLUDE_FILES sys/types.h sys/socket.h netinet/in.h)
    check_type_size(socklen_t HAVE_SOCKLEN_T_SIZE LANGUAGE C)
    check_type_size(in_addr_t HAVE_IN_ADDR_T_SIZE LANGUAGE C)
    unset(CMAKE_EXTRA_INCLUDE_FILES)
    if(HAVE_SOCKLEN_T_SIZE)
        set(HAVE_SOCKLEN_T 1)
    endif()
    if(HAVE_IN_ADDR_T_SIZE)
        set(HAVE_IN_ADDR_T 1)
    endif()
endif()
foreach(header dirent.h direct.h errno.h fcntl.h inttypes.h io.h libgen.h limits.h math.h process.h
        signal.h stdint.h stdio.h stdlib.h string.h strings.h sys/stat.h sys/time.h sys/types.h
        sys/ioctl.h sys/select.h unistd.h wchar.h alloca.h execinfo.h regex.h)
    string(TOUPPER "HAVE_${header}" variable)
    string(REGEX REPLACE "[/.]" "_" variable "${variable}")
    check_include_files("${header}" ${variable})
endforeach()
# Each function with the header that declares it.
foreach(check atexit:stdlib.h dirname:libgen.h getcwd:unistd.h gettimeofday:sys/time.h
        memmove:string.h mkstemp:stdlib.h nanosleep:time.h random:stdlib.h rewinddir:dirent.h
        snprintf:stdio.h stpcpy:string.h strcasecmp:strings.h strdup:string.h strerror:string.h
        strlcpy:string.h strncasecmp:strings.h strtok_r:string.h strtoul:stdlib.h vsnprintf:stdio.h
        fseeko:stdio.h ftello:stdio.h getpwuid:pwd.h)
    string(REPLACE ":" ";" check "${check}")
    list(GET check 0 function)
    list(GET check 1 header)
    string(TOUPPER "HAVE_${function}" variable)
    check_symbol_exists(${function} "${header}" ${variable})
endforeach()
check_type_size("unsigned int" SIZEOF_UNSIGNED_INT)
check_type_size("unsigned long" SIZEOF_UNSIGNED_LONG)
check_type_size("unsigned short" SIZEOF_UNSIGNED_SHORT)
check_type_size("time_t" SIZEOF_TIME_T)
if(SIZEOF_TIME_T EQUAL 8)
    set(TIME_T_IS_64BIT 1)
else()
    set(TIME_T_IS_32BIT 1)
endif()
file(READ "${VICE}/config.h.in" config_template)
string(REGEX REPLACE "#undef ([A-Za-z0-9_]+)" "#cmakedefine \\1 @\\1@" config_template "${config_template}")
file(WRITE "${GENERATED}/config.h.cmake" "${config_template}")
configure_file("${GENERATED}/config.h.cmake" "${GENERATED}/config.h")
# debug.h without VICE's debugging build, and reSID's settings as configure gives them.
set(DEBUGBUILD 0)
configure_file("${VICE}/debug.h.in" "${GENERATED}/debug.h" @ONLY)
set(RESID_INLINING 1)
set(RESID_INLINE inline)
set(RESID_BRANCH_HINTS 1)
set(NEW_8580_FILTER 0)
set(HAVE_BOOL 1)
set(HAVE_BUILTIN_EXPECT 1)
set(HAVE_LOG1P 1)
configure_file("${VICE}/resid/siddefs.h.in" "${GENERATED}/siddefs.h" @ONLY)

include("${CMAKE_CURRENT_LIST_DIR}/vice_sources.cmake")

# Kairo64's replacements (kairo/), in the libraries of the files they replace:
# - vsync.c: the end of each frame hands the machine to the host, which paces it; VICE's own
#   real-time pacing is not built.
# - arch/headless/main.c: VICE starts on Kairo64's thread (KairoC64_Run); main_exit stays.
# - arch/headless/video.c: frames are rendered to RGBA for the host.
# - arch/headless/mousedrv.c: the host's mouse buttons reach VICE's mouse emulation.
# - arch/headless/uistatusbar.c: the datasette's motor, button and tape reports are kept, so the
#   host can tell when a tape is moving (KairoC64_TapeMoving).
# - arch/shared/archdep_exit.c: an exit ends the machine with an error instead of the process.
# - arch/shared/archdep_default_logger.c: VICE's log goes to the host.
# - arch/shared/archdep_user_{cache,config,state}_path.c: VICE's per-user folders are the
#   machine's home folder (archdep_user_paths.c), not the XDG folders an Android app cannot write.
list(REMOVE_ITEM VICE_BASE_SOURCES "${VICE}/vsync.c")
list(APPEND VICE_BASE_SOURCES "${KAIRO}/vsync.c" "${KAIRO}/control.c" "${KAIRO}/tapedisk.c")
list(REMOVE_ITEM VICE_LIBRARY_arch "${VICE}/arch/headless/main.c" "${VICE}/arch/headless/video.c"
    "${VICE}/arch/headless/mousedrv.c" "${VICE}/arch/headless/uistatusbar.c")
list(APPEND VICE_LIBRARY_arch "${KAIRO}/main.c" "${KAIRO}/video.c" "${KAIRO}/mousedrv.c" "${KAIRO}/uistatusbar.c")
list(REMOVE_ITEM VICE_LIBRARY_archdep "${VICE}/arch/shared/archdep_exit.c"
    "${VICE}/arch/shared/archdep_default_logger.c" "${VICE}/arch/shared/archdep_user_cache_path.c"
    "${VICE}/arch/shared/archdep_user_config_path.c" "${VICE}/arch/shared/archdep_user_state_path.c")
list(APPEND VICE_LIBRARY_archdep "${KAIRO}/archdep_exit.c" "${KAIRO}/archdep_default_logger.c"
    "${KAIRO}/archdep_user_paths.c")
# The VIC-II's state the host reads (KairoC64_RasterIrqLine), with the C64 library's VIC-II headers.
list(APPEND VICE_LIBRARY_c64sc "${KAIRO}/vicii.c")
# The sound device that hands VICE's sound to the host (the vice-kairo-sound local change lists it).
list(APPEND VICE_LIBRARY_sounddrv "${KAIRO}/soundkairo.c")
# sound.c lists Windows' waveout device on Windows, which configure's SOUND_DRIVERS adds there (the
# host tests).
if(WIN32)
    list(APPEND VICE_LIBRARY_sounddrv "${VICE}/arch/shared/sounddrv/soundwmm.c")
endif()

# VICE's flags: it is C99 with GNU extensions, and its code assumes signed char, as on the x86
# hosts it is mostly developed on (ARM's is unsigned).
set(VICE_OPTIONS -fsigned-char -Wno-unused-variable -Wno-unused-function -Wno-unused-label
    -Wno-sign-compare -Wno-missing-braces -Wno-unused-but-set-variable -Wno-implicit-fallthrough
    -Wno-deprecated-declarations -Wno-format-truncation -Wno-unknown-warning-option)
# VICE's install paths (arch/shared): its data directory is the machine's home, which
# KairoC64_Run's caller fills.
set(VICE_DEFINITION_VICE_DATADIR "VICE_DATADIR=\"/\"")
set(VICE_DEFINITION_VICE_DOCDIR "VICE_DOCDIR=\"/\"")

function(vice_library name sources includes defines)
    add_library(vice_${name} STATIC ${sources})
    set_target_properties(vice_${name} PROPERTIES C_STANDARD 99 C_EXTENSIONS ON)
    target_include_directories(vice_${name} PRIVATE "${GENERATED}" "${KAIRO}" ${includes})
    target_compile_options(vice_${name} PRIVATE ${VICE_OPTIONS})
    foreach(define ${defines})
        target_compile_definitions(vice_${name} PRIVATE "${VICE_DEFINITION_${define}}")
    endforeach()
endfunction()

vice_library(base "${VICE_BASE_SOURCES}" "${VICE_BASE_INCLUDES}" "")
# Generated once from doc/vice.texi with VICE's own script (tools/generate_infocontrib.sh).
target_include_directories(vice_base PRIVATE "${CMAKE_CURRENT_LIST_DIR}/generated")
target_link_libraries(vice_base PUBLIC kairo_zlib)
set(VICE_TARGETS vice_base)
foreach(library ${VICE_LIBRARIES})
    # A library whose sources are all optional drivers this build leaves out (hwsiddrv) has none.
    if(NOT VICE_LIBRARY_${library})
        continue()
    endif()
    vice_library(${library} "${VICE_LIBRARY_${library}}" "${VICE_INCLUDES_${library}}" "${VICE_DEFINES_${library}}")
    target_link_libraries(vice_${library} PUBLIC kairo_zlib)
    list(APPEND VICE_TARGETS vice_${library})
endforeach()
# reSID is C++ (its own .cc files) and links into the C libraries. Its version is its own
# configure's (src/resid/configure.in).
set_target_properties(vice_resid PROPERTIES CXX_STANDARD 11)
target_compile_definitions(vice_resid PRIVATE VERSION="1.0-pre2")

# The frame-driven machine (c64_machine.h). VICE's libraries reference one another in both
# directions, as automake's link of x64sc allows; lld resolves archives in any order.
add_library(kairo64_core STATIC c64_machine.cpp)
target_include_directories(kairo64_core PUBLIC "${CMAKE_CURRENT_LIST_DIR}" PRIVATE "${KAIRO}")
target_link_libraries(kairo64_core PUBLIC ${VICE_TARGETS})
if(ANDROID)
    target_link_libraries(kairo64_core PUBLIC log)
elseif(WIN32)
    target_link_libraries(kairo64_core PUBLIC ws2_32 shlwapi winmm)
endif()

if(NOT ANDROID)
    add_executable(boot_test test/boot_test.cpp)
    target_link_libraries(boot_test PRIVATE kairo64_core)
    if(WIN32)
        target_link_options(boot_test PRIVATE -static)
    endif()
endif()
