#include "ble_telemetry.h"
#include "esp_log.h"

static const char *TAG = "BLE_TELEMETRY";

esp_err_t ble_telemetry_init(void) {
    ESP_LOGI(TAG, "Inicializando stack BLE (Mock)...");
    
    // Aquí iría el setup de NimBLE (nimble_port_init, ble_svc_gap_init, etc.)
    // Se requiere habilitar CONFIG_BT_ENABLED en modo menuconfig/sdkconfig
#ifdef CONFIG_BT_ENABLED
    ESP_LOGI(TAG, "Bluetooth Nativo Habilitado por SDK");
#else
    ESP_LOGW(TAG, "Bluetooth no activado en sdkconfig, simulando conexión GATT.");
#endif

    return ESP_OK;
}

esp_err_t ble_telemetry_send(uint32_t *data, size_t count) {
    if (data == NULL || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Simular el envío por GATT Characteristic Notification
    ESP_LOGD(TAG, "Notificando [HR: %lu bpm, Pasos: %lu] vía BLE (Dummy)", data[0], data[1]);
    
    return ESP_OK;
}
