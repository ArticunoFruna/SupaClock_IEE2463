#ifdef ENV_TEST_DISPLAY

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "st7789.h"
#include "esp_spiffs.h"
#include "JPEGDEC.h"

static const char *TAG = "Test_ST7789_JPEG";

JPEGDEC jpeg;

// Callbacks POSIX estandar para que JPEGDEC sepa leer desde el disco SPIFFS (ESP-IDF VFS)
void * myOpen(const char *filename, int32_t *size) {
    FILE *f = fopen(filename, "rb");
    if(f) {
        fseek(f, 0, SEEK_END);
        *size = ftell(f);
        fseek(f, 0, SEEK_SET);
    }
    return (void*)f;
}
void myClose(void *handle) {
    if(handle) fclose((FILE*)handle);
}
int32_t myRead(JPEGFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    return fread(pBuf, 1, iLen, (FILE*)pFile->fHandle);
}
int32_t mySeek(JPEGFILE *pFile, int32_t iPosition) {
    if (fseek((FILE*)pFile->fHandle, iPosition, SEEK_SET) == 0) return iPosition;
    return -1;
}

// Callback asincrono para mandar "bloques" (MCUs) 16x16 de la foto decodificada
int JPEGDraw(JPEGDRAW *pDraw) {
    // La libreria nos va mandando cuadritos. Los envíamos directo al ST7789 a 80MHz sin guardar en RAM la imagen entera
    st7789_draw_bitmap(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
    return 1; // 1 = seguir decodificando
}

static void init_spiffs() {
    ESP_LOGI(TAG, "Montando particion SPIFFS virtual...");
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error montando SPIFFS: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS montado genial.");
    }
}

extern "C" void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "Test JPEG + SPIFFS - START");
    ESP_LOGI(TAG, "====================================");

    init_spiffs();
    st7789_init();

    while (1) {
        ESP_LOGI(TAG, "Buscando /spiffs/foto.jpg...");
        
        // El framework abre el archivo en C Estandar (FILE*)
        uint32_t t_start = xTaskGetTickCount();
        
        if (jpeg.open("/spiffs/foto.jpg", myOpen, myClose, myRead, mySeek, JPEGDraw)) {
            ESP_LOGI(TAG, "Foto abierta. Stats: %dx%d", jpeg.getWidth(), jpeg.getHeight());
            // Para el ST7789, a veces los colores endian deben invertirse. JPEGDEC tiene esto:
            jpeg.setPixelType(RGB565_BIG_ENDIAN); // Big Endian le gusta al driver SPI directo
            
            // Comenzamos el flasheo de bytes a la CPU!
            if (jpeg.decode(0, 0, 0)) { // (Alinear x=0, y=0, sin escalar)
                uint32_t t_end = xTaskGetTickCount();
                ESP_LOGI(TAG, "OK! Decodificada y pintada en %ld ms", (t_end - t_start) * portTICK_PERIOD_MS);
            } else {
                ESP_LOGE(TAG, "Error interno falló la decodificacion del JPEG");
            }
            jpeg.close();
        } else {
            ESP_LOGE(TAG, "No se encontro la imagen. (Ejecutaste 'pio run -e test_display --target uploadfs'?)");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
#endif
