cmake_minimum_required(VERSION 3.10)
project(pjwebrtc)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework AudioUnit")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreAudio")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework AudioToolbox")

set(MAIN_DIR .)
set(LIB_DIR ${MAIN_DIR}/lib)

aux_source_directory(${LIB_DIR}/promise/src PROMISE_SRC_LIST)
include_directories(${LIB_DIR}/promise/include)

aux_source_directory(${LIB_DIR}/wsxx/src WSXX_SRC_LIST)
include_directories(${LIB_DIR}/wsxx/include)

include_directories(${LIB_DIR})

aux_source_directory(${MAIN_DIR}/src SRC_FILE_LIST)

set(SOURCES main.cpp ${SRC_FILE_LIST} ${PROMISE_SRC_LIST} ${WSXX_SRC_LIST})

add_executable(pjwebrtc ${SOURCES})

target_link_libraries(pjwebrtc
        stdc++
        opus
        pj-x86_64-apple-darwin15.6.0
        pjlib-util-x86_64-apple-darwin15.6.0
        pjnath-x86_64-apple-darwin15.6.0
        pjmedia-x86_64-apple-darwin15.6.0.a
        pjmedia-videodev-x86_64-apple-darwin15.6.0
        pjmedia-audiodev-x86_64-apple-darwin15.6.0
        pjmedia-codec-x86_64-apple-darwin15.6.0
        pjsip-simple-x86_64-apple-darwin15.6.0
        pjsip-ua-x86_64-apple-darwin15.6.0
        pjsip-x86_64-apple-darwin15.6.0
        pjsua-x86_64-apple-darwin15.6.0
        pjsua2-x86_64-apple-darwin15.6.0
        resample-x86_64-apple-darwin15.6.0
        gsmcodec-x86_64-apple-darwin15.6.0
        speex-x86_64-apple-darwin15.6.0
        ilbccodec-x86_64-apple-darwin15.6.0
        g7221codec-x86_64-apple-darwin15.6.0
        yuv-x86_64-apple-darwin15.6.0
        webrtc-x86_64-apple-darwin15.6.0
        srtp-x86_64-apple-darwin15.6.0
        /usr/local/opt/openssl@1.1/lib/libssl.1.1.dylib
        /usr/local/opt/openssl@1.1/lib/libcrypto.1.1.dylib
        )
