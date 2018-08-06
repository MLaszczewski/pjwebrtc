//
// Created by Michał Łaszczewski on 02/02/18.
//

#include "global.h"

namespace webrtc {

  pj_caching_pool cachingPool;
  pj_ioqueue_t* ioqueue;
  pj_timer_heap_t* timerHeap;
  pj_pool_t* globalPool;
  pjmedia_endpt *mediaEndpoint;


  void init() {
    pj_status_t status;
    status = pj_init();
    assert(status == PJ_SUCCESS);

    status = pjlib_util_init();
    assert(status == PJ_SUCCESS);

    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(&cachingPool, &pj_pool_factory_default_policy, 0);

    globalPool = pj_pool_create(&cachingPool.factory,"Global.pool", 4096, 4096, NULL);

    assert( pj_timer_heap_create(globalPool, 100, &timerHeap) == PJ_SUCCESS );

    assert( pj_ioqueue_create(globalPool, 16, &ioqueue) == PJ_SUCCESS );

    /* Create the endpoint: */
    status = pjmedia_endpt_create(&cachingPool.factory, NULL, 1, &mediaEndpoint);

    //pj_bool_t telephony = false;
    //pjmedia_endpt_set_flag(mediaEndpoint, PJMEDIA_ENDPT_HAS_TELEPHONE_EVENT_FLAG, &telephony);

    assert(status == PJ_SUCCESS);
    status = pjmedia_codec_g711_init(mediaEndpoint);
    assert(status == PJ_SUCCESS);
    status = pjmedia_codec_g722_init(mediaEndpoint);
    assert(status == PJ_SUCCESS);
    status = pjmedia_codec_ilbc_init(mediaEndpoint, 30);
    assert(status == PJ_SUCCESS);
//    status = pjmedia_codec_opus_init(mediaEndpoint);
//    assert(status == PJ_SUCCESS);

    /// VIDEO INIT:

    status = pjmedia_video_format_mgr_create(globalPool, 64, 0, NULL);
    assert(status == PJ_SUCCESS);
    status = pjmedia_converter_mgr_create(globalPool, NULL);
    assert(status == PJ_SUCCESS);
    status = pjmedia_vid_codec_mgr_create(globalPool, NULL);
    assert(status == PJ_SUCCESS);
    status = pjmedia_vid_dev_subsys_init(&cachingPool.factory);
    assert(status == PJ_SUCCESS);

    status = pjmedia_codec_openh264_vid_init(NULL, &cachingPool.factory);
    assert(status == PJ_SUCCESS);
    /* Init ffmpeg video codecs */
    /*status = pjmedia_codec_ffmpeg_vid_init(NULL, &cachingPool.factory);
    assert(status == PJ_SUCCESS);*/
  }

  void destroy() {
    pj_timer_heap_destroy(timerHeap);
    pj_ioqueue_destroy(ioqueue);
    pjmedia_endpt_destroy2(mediaEndpoint);

    /* Destroy pool factory */
    pj_caching_pool_destroy( &cachingPool );

    /* Shutdown PJLIB */
    pj_shutdown();
  }

}