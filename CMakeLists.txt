cmake_minimum_required(VERSION 3.10)
project(pjwebrtc)

set(CMAKE_CXX_STANDARD 14)

#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework AudioUnit")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreAudio")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework AudioToolbox")

set(MAIN_DIR .)
set(LIB_DIR ${MAIN_DIR}/lib)

aux_source_directory(${LIB_DIR}/promise/src PROMISE_SRC_LIST)
include_directories(${LIB_DIR}/promise/include)

aux_source_directory(${LIB_DIR}/wsxx/src WSXX_SRC_LIST)
include_directories(${LIB_DIR}/wsxx/include)

include_directories(${LIB_DIR})

aux_source_directory(${MAIN_DIR}/src SRC_FILE_LIST)

set(SOURCES main.cpp ${SRC_FILE_LIST} ${PROMISE_SRC_LIST} ${WSXX_SRC_LIST} linux/terminal.cpp)

add_executable(pjwebrtc ${SOURCES})

set(PJ_SUFIX unknown-linux-gnu)

target_link_libraries(pjwebrtc
        stdc++
        opus openh264
        pjnath-x86_64-${PJ_SUFIX}
        pjmedia-videodev-x86_64-${PJ_SUFIX}
        pjmedia-audiodev-x86_64-${PJ_SUFIX}
        pjmedia-codec-x86_64-${PJ_SUFIX}
        pjmedia-x86_64-${PJ_SUFIX}
        pjsip-simple-x86_64-${PJ_SUFIX}
        pjsip-ua-x86_64-${PJ_SUFIX}
        pjsip-x86_64-${PJ_SUFIX}
        pjsua-x86_64-${PJ_SUFIX}
        pjsua2-x86_64-${PJ_SUFIX}
        resample-x86_64-${PJ_SUFIX}
        gsmcodec-x86_64-${PJ_SUFIX}
        speex-x86_64-${PJ_SUFIX}
        ilbccodec-x86_64-${PJ_SUFIX}
        g7221codec-x86_64-${PJ_SUFIX}
        yuv-x86_64-${PJ_SUFIX}
        webrtc-x86_64-${PJ_SUFIX}
        srtp-x86_64-${PJ_SUFIX}
        pj-x86_64-${PJ_SUFIX}
        pjlib-util-x86_64-${PJ_SUFIX}
        )

target_link_libraries(pjwebrtc ssl crypto pthread asound uuid v4l2 SDL2)

#target_link_libraries(pjwebrtc
#        /usr/local/opt/openssl@1.1/lib/libssl.1.1.dylib
#        /usr/local/opt/openssl@1.1/lib/libcrypto.1.1.dylib
#        )
