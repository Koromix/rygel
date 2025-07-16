/* !checksrc! disable COPYRIGHT all */

#include "first.h"

#include "../../lib/curlx/multibyte.c"
#include "../../lib/curlx/timediff.c"
#include "../../lib/curlx/wait.c"
#include "h2_pausing.c"
#include "h2_serverpush.c"
#include "h2_upgrade_extreme.c"
#include "hx_download.c"
#include "hx_upload.c"
#include "tls_session_reuse.c"
#include "upload_pausing.c"
#include "ws_data.c"
#include "ws_pingpong.c"

const struct entry_s s_entries[] = {
  {"h2_pausing", test_h2_pausing},
  {"h2_serverpush", test_h2_serverpush},
  {"h2_upgrade_extreme", test_h2_upgrade_extreme},
  {"hx_download", test_hx_download},
  {"hx_upload", test_hx_upload},
  {"tls_session_reuse", test_tls_session_reuse},
  {"upload_pausing", test_upload_pausing},
  {"ws_data", test_ws_data},
  {"ws_pingpong", test_ws_pingpong},
  {NULL, NULL}
};

#include "first.c"
