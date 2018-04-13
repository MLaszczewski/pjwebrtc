//
// Created by Michał Łaszczewski on 02/02/18.
//

#ifndef PJWEBRTC_GLOBAL_H
#define PJWEBRTC_GLOBAL_H

#ifdef __ARM_EABI__
#define PJ_IS_LITTLE_ENDIAN 1
#define PJ_IS_BIG_ENDIAN 0
#endif

#include <pjmedia.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <pjmedia-codec.h>
#include <pjnath/stun_auth.h>

namespace webrtc {

  extern pj_caching_pool cachingPool;

  void init();
  void destroy();

}

#endif //PJWEBRTC_GLOBAL_H
