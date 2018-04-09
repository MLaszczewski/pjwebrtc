//
// Created by Michał Łaszczewski on 02/02/18.
//

#include "global.h"

namespace webrtc {

  pj_caching_pool cachingPool;

  void init() {
    pj_status_t status;
    status = pj_init();
    assert(status == PJ_SUCCESS);

    status = pjlib_util_init();
    assert(status == PJ_SUCCESS);

    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(&cachingPool, &pj_pool_factory_default_policy, 0);

  }

  void destroy() {
    /* Destroy pool factory */
    pj_caching_pool_destroy( &cachingPool );

    /* Shutdown PJLIB */
    pj_shutdown();
  }

}