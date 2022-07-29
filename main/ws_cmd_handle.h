#ifndef _WSS_CMD_HANDLE_H
#define _WSS_CMD_HANDLE_H

#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ws_respond_code_ok = 0,
    ws_respond_code_invalid_format = -1,
    ws_respond_code_invalid_cmd   = -2,
    ws_respond_code_invalid_token = -3,
    ws_respond_code_invalid_param = -4,
    ws_respond_code_exec_failure  = -5,
} ws_respond_code_t;

enum {
    ws_cmd_r_heartbeat = 0,
    ws_cmd_r_dev_info = 1,
    ws_cmd_r_dev_time = 2,
    ws_cmd_w_dev_time = 3,
    ws_cmd_w_dev_network = 4,
    ws_cmd_e_wifi_ap_scan= 5,
    ws_cmd_b_wifi_ap_list = 6,
    ws_cmd_r_fan_duty = 7,
    ws_cmd_w_fan_duty = 8,

    ws_cmd_r_ledctrl_state = 10,
    ws_cmd_r_ledctrl_mode = 11,
    ws_cmd_w_ledctrl_mode = 12,
    ws_cmd_r_ledctrl_current_output = 13,
    ws_cmd_w_ledctrl_demo_output = 14, 
    ws_cmd_r_ledctrl_manual = 15,
    ws_cmd_w_ledctrl_manual = 16,
    ws_cmd_r_ledctrl_schedule = 17,
    ws_cmd_w_ledctrl_schedule = 18,

    ws_cmd_e_ota_begin = 64,
    ws_cmd_e_ota_write = 65,
    ws_cmd_e_ota_done  = 66,

    ws_cmd_e_dev_reboot = 128,
    ws_cmd_e_dev_recovery = 129,
};

ws_respond_code_t read_dev_info_handle(cJSON *request, cJSON *respone);
ws_respond_code_t read_dev_time_handle(cJSON *request, cJSON *respone);
ws_respond_code_t write_dev_time_handle(cJSON *request, cJSON *respone);
ws_respond_code_t write_dev_network_handle(cJSON *request, cJSON *respone);
ws_respond_code_t exec_wifi_ap_scan_handle(int client, cJSON *request, cJSON *respone);
ws_respond_code_t read_fan_duty_handle(cJSON *request, cJSON *respone);
ws_respond_code_t write_fan_duty_handle(cJSON *request, cJSON *respone);

ws_respond_code_t read_ledctrl_state_handle(cJSON *request, cJSON *respone);
ws_respond_code_t read_ledctrl_mode_handle(cJSON *request, cJSON *respone);
ws_respond_code_t read_ledctrl_current_output_handle(cJSON *request, cJSON *respone);
ws_respond_code_t read_ledctrl_manual_handle(cJSON *request, cJSON *respone);
ws_respond_code_t read_ledctrl_schedule_handle(cJSON *request, cJSON *respone);
ws_respond_code_t write_ledctrl_mode_handle(cJSON *request, cJSON *respone);
ws_respond_code_t write_ledctrl_demo_output_handle(cJSON *request, cJSON *respone);
ws_respond_code_t write_ledctrl_manual_handle(cJSON *request, cJSON *respone);
ws_respond_code_t write_ledctrl_schedule_handle(cJSON *request, cJSON *respone);

ws_respond_code_t exec_reboot_handle(cJSON *request, cJSON *respone);
ws_respond_code_t exec_recovery_factory_handle(cJSON *request, cJSON *respone);
ws_respond_code_t exec_ledtemporary_handler(cJSON *request, cJSON *respone);

ws_respond_code_t exec_ota_begin_handle(int client, cJSON *request, cJSON *respone);
ws_respond_code_t exec_ota_done_handle(int client, cJSON *request, cJSON *respone);

#ifdef __cplusplus
}
#endif
#endif