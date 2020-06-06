#ifndef _WSS_CMD_HANDLE_H
#define _WSS_CMD_HANDLE_H

#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    wss_respond_code_ok = 0,
    wss_respond_code_invalid_format = -1,
    wss_respond_code_invalid_cmd   = -2,
    wss_respond_code_invalid_token = -3,
    wss_respond_code_invalid_param = -4,
    wss_respond_code_exec_failure  = -5,
} wss_respond_code_t;

enum {
    wss_cmd_r_heartbeat = 0,
    wss_cmd_r_devinfo = 1,
    wss_cmd_r_devtime = 2,
    wss_cmd_w_devtime = 3,
    wss_cmd_w_devnetwork = 4,

    wss_cmd_r_ledctrl_state = 10,
    wss_cmd_r_ledctrl_mode = 11,
    wss_cmd_w_ledctrl_mode = 12,
    wss_cmd_r_ledctrl_current_output = 13,
    wss_cmd_w_ledctrl_demo_output = 14, 
    wss_cmd_r_ledctrl_manual = 15,
    wss_cmd_w_ledctrl_manual = 16,
    wss_cmd_r_ledctrl_schedule = 17,
    wss_cmd_w_ledctrl_schedule = 18,

    wss_cmd_e_ota_begin = 64,
    wss_cmd_e_ota_write = 65,
    wss_cmd_e_ota_done  = 66,

    wss_cmd_e_dev_reboot = 128,
    wss_cmd_e_dev_recovery = 129,
};

wss_respond_code_t read_devinfo_handle(cJSON *request, cJSON *respone);
wss_respond_code_t read_devtime_handle(cJSON *request, cJSON *respone);
wss_respond_code_t write_devtime_handle(cJSON *request, cJSON *respone);
wss_respond_code_t write_devnetwork_handle(cJSON *request, cJSON *respone);

wss_respond_code_t read_ledctrl_state_handle(cJSON *request, cJSON *respone);
wss_respond_code_t read_ledctrl_mode_handle(cJSON *request, cJSON *respone);
wss_respond_code_t read_ledctrl_current_output_handle(cJSON *request, cJSON *respone);
wss_respond_code_t read_ledctrl_manual_handle(cJSON *request, cJSON *respone);
wss_respond_code_t read_ledctrl_schedule_handle(cJSON *request, cJSON *respone);
wss_respond_code_t write_ledctrl_mode_handle(cJSON *request, cJSON *respone);
wss_respond_code_t write_ledctrl_demo_output_handle(cJSON *request, cJSON *respone);
wss_respond_code_t write_ledctrl_manual_handle(cJSON *request, cJSON *respone);
wss_respond_code_t write_ledctrl_schedule_handle(cJSON *request, cJSON *respone);

wss_respond_code_t exec_reboot_handle(cJSON *request, cJSON *respone);
wss_respond_code_t exec_recovery_factory_handle(cJSON *request, cJSON *respone);
wss_respond_code_t exec_ledtemporary_handler(cJSON *request, cJSON *respone);

wss_respond_code_t exec_ota_begin_handle(int client, cJSON *request, cJSON *respone);
wss_respond_code_t exec_ota_write_handle(int client, cJSON *request, cJSON *respone);
wss_respond_code_t exec_ota_done_handle(int client, cJSON *request, cJSON *respone);

#ifdef __cplusplus
}
#endif
#endif