#include "bluetooth_a2dp_sink.h"
#include "application.h"
#include "audio_codecs/audio_codec.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "board.h"
#include "driver/i2s.h"

#define TAG "BT_A2DP_SINK"

static esp_a2d_audio_state_t s_audio_state = ESP_A2D_AUDIO_STATE_STOPPED;

static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT: {
            if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
            }
            break;
        }
        case ESP_A2D_AUDIO_STATE_EVT: {
            s_audio_state = param->audio_stat.state;
            break;
        }
        case ESP_A2D_AUDIO_CFG_EVT: {
            uint32_t sample_rate = param->audio_cfg.mcc.cie.sbc[0];
            // uint8_t bits_per_sample = param->audio_cfg.mcc.cie.sbc[1]; // bits_per_sample seems not directly usable for i2s_set_clk which expects enum
            ESP_LOGI(TAG, "Configuring I2S clock for sample rate: %d", (int)sample_rate);
            // Assuming I2S port 0 and 16-bit stereo as per standard A2DP SBC
            // You might need to adjust I2S_NUM_0 based on your board configuration
            i2s_set_clk(I2S_NUM_0, sample_rate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
            break;
        }
        default:
            break;
    }
}

static void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len) {
    if (s_audio_state == ESP_A2D_AUDIO_STATE_STARTED) {
        auto *codec = Board::GetInstance().GetAudioCodec();
        const int16_t *pcm_data = reinterpret_cast<const int16_t *>(data);
        int sample_count = len / 2;
        codec->WritePublic(pcm_data, sample_count);
    }
}

static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT:
            break;
        default:
            break;
    }
}

void bluetooth_a2dp_sink_init(void) {
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_bt_gap_register_callback(bt_app_gap_cb));

    ESP_ERROR_CHECK(esp_a2d_register_callback(&bt_app_a2d_cb));
    ESP_ERROR_CHECK(esp_a2d_sink_register_data_callback(bt_app_a2d_data_cb));
    ESP_ERROR_CHECK(esp_a2d_sink_init());

    ESP_ERROR_CHECK(esp_bt_dev_set_device_name("XIAOZHI_SPEAKER"));
    ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE));
}

void bluetooth_a2dp_sink_deinit(void) {
    ESP_ERROR_CHECK(esp_a2d_sink_deinit());
    ESP_ERROR_CHECK(esp_bluedroid_disable());
    ESP_ERROR_CHECK(esp_bluedroid_deinit());
    ESP_ERROR_CHECK(esp_bt_controller_disable());
    ESP_ERROR_CHECK(esp_bt_controller_deinit());
}
