#include "mgos.h"
#include "mgos_rpc.h"

#include "orchestrator.h"

#define RPC_MBUF_SIZE 50

static void rpc_clock_handler(
    struct mg_rpc_request_info *ri, void *cb_arg,
    struct mg_rpc_frame_info *fi, struct mg_str args) {
  struct mbuf fb;
  struct json_out out = JSON_OUT_MBUF(&fb);

  mbuf_init(&fb, RPC_MBUF_SIZE);

  int clock_style = 0;
  if (json_scanf(args.p, args.len, ri->args_fmt, &clock_style) != 1) {
    json_printf(&out, "{error: %Q}", "Style is required");
  } else {
    if (!orchestrator_activate_clock(clock_style)) {
      json_printf(&out, "{error: %Q}", "Invalid clock style");
    } else {
      json_printf(&out, "{style: %d}", clock_style);
    }
  }

  mg_rpc_send_responsef(ri, "%.*s", fb.len, fb.buf);
  mbuf_free(&fb);
}

static void rpc_notification_handler(
    struct mg_rpc_request_info *ri, void *cb_arg,
    struct mg_rpc_frame_info *fi, struct mg_str args) {
  struct mbuf fb;
  struct json_out out = JSON_OUT_MBUF(&fb);

  mbuf_init(&fb, RPC_MBUF_SIZE);

  char* icon = NULL;
  if (json_scanf(args.p, args.len, ri->args_fmt, &icon) != 1) {
    json_printf(&out, "{error: %Q}", "Icon is required");
  } else {
    if (!orchestrator_activate_notification(icon)) {
      json_printf(&out, "{error: %Q}", "Invalid notification icon");
    } else {
      json_printf(&out, "{icon: %Q}", icon);
    }
  }

  mg_rpc_send_responsef(ri, "%.*s", fb.len, fb.buf);
  mbuf_free(&fb);
}

void rpc_setup() {
  mg_rpc_add_handler(mgos_rpc_get_global(),
      "FlipDots.Clock", "{style: %d}",
      rpc_clock_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(),
      "FlipDots.Notification", "{icon: %Q}",
      rpc_notification_handler, NULL);
}
