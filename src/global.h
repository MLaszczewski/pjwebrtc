//
// Created by Michał Łaszczewski on 02/02/18.
//

#ifndef PJWEBRTC_GLOBAL_H
#define PJWEBRTC_GLOBAL_H

#include <pjmedia.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <pjmedia-codec.h>

namespace webrtc {

  extern pj_caching_pool cachingPool;

  void init();
  void destroy();

}

#endif //PJWEBRTC_GLOBAL_H
