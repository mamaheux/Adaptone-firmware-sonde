#include "sound.h"
#include "config.h"
#include "network/communication.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <driver/i2s.h>

#define US_IN_MS_COUNT 1000

#define I2S_READ_DATA_SIZE 8

#define UDP_MESSAGE_FULL_HEADER_SIZE 17
#define UDP_MESSAGE_PAYLOAD_HEADER_SIZE 9
#define UDP_MESSAGE_SIZE (UDP_MESSAGE_FULL_HEADER_SIZE + CONFIG_SOUND_MESSAGE_SAMPLE_COUNT * sizeof(int32_t))
#define UDP_MESSAGE_ID 7

static const ledc_timer_config_t LEDC_TIMER_CONFIG = 
{
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .timer_num  = LEDC_TIMER_0,
    .duty_resolution    = 2,
    .freq_hz    = 11289600
};
 
static const ledc_channel_config_t LEDC_CHANNEL_CONFIG = 
{
    .channel    = LEDC_CHANNEL_0,
    .gpio_num   = CONFIG_SOUND_CLOCK_IO,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .timer_sel  = LEDC_TIMER_0,
    .duty       = 2
};

static const gpio_config_t ADC_IO_CONFIG = 
{
    .intr_type = GPIO_PIN_INTR_DISABLE,
    .mode = GPIO_MODE_OUTPUT,
    .pin_bit_mask = CONFIG_SOUND_GPIO_OUTPUT_PIN_SEL,
    .pull_down_en = 0,
    .pull_up_en = 0
};

static const i2s_config_t I2S_CONFIG = 
{
     .mode = I2S_MODE_SLAVE | I2S_MODE_RX,
     .sample_rate = CONFIG_SOUND_SAMPLE_FREQUENCY,
     .bits_per_sample = 24,
     .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
     .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
     .intr_alloc_flags = 0,
     .dma_buf_count = 8,
     .dma_buf_len = 64,
     .use_apll = 0
};

static const i2s_pin_config_t I2S_PIN_CONFIG = 
{
    .bck_io_num = 23,
    .ws_io_num = 39,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = 36
};

static uint8_t udpMessageData[UDP_MESSAGE_SIZE];
static int32_t* udpSampleData;

static void initAdc()
{
    gpio_config(&ADC_IO_CONFIG);

    gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_PDWN, 0);

    // Set MODE to master 256 fs
    gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_MODE0, 1);
    gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_MODE1, 1);

    // Set format to I2S
    gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_FMT0, 1);
    gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_FMT1, 0);
    
    gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_OSR, 0); // SET OSR to x64    
    gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_BYPASS, 0); //SET BYPAS to normal mode (HPF activated)
}

static void initializeUdpMessageHeader()
{
    *(uint32_t*)udpMessageData = htonl(UDP_MESSAGE_ID);
    *(uint32_t*)(udpMessageData + 4) = 
        htonl(UDP_MESSAGE_PAYLOAD_HEADER_SIZE + CONFIG_SOUND_MESSAGE_SAMPLE_COUNT * sizeof(int32_t));
    
    udpSampleData = (int32_t*)(udpMessageData + UDP_MESSAGE_FULL_HEADER_SIZE);
}

static void updateUdpMessageIdAndTimestamp()
{
    static uint16_t currentId = 0;
    
    *(uint16_t*)(udpMessageData + 8) = htons(currentId);

    struct timeval tv;
    struct tm timeinfo;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &timeinfo);

    udpMessageData[10] = (uint8_t)timeinfo.tm_hour;
    udpMessageData[11] = (uint8_t)timeinfo.tm_min;
    udpMessageData[12] = (uint8_t)timeinfo.tm_sec;
    *(uint16_t*)(udpMessageData + 13) = htons(tv.tv_usec / US_IN_MS_COUNT);
    *(uint16_t*)(udpMessageData + 15) = htons(tv.tv_usec % US_IN_MS_COUNT);

    currentId++;
}

static void soundTask(void* parameters)
{
    uint8_t data[I2S_READ_DATA_SIZE];
    size_t readSize;

    while (1)
    {
        updateUdpMessageIdAndTimestamp();
        for (size_t i = 0; i < CONFIG_SOUND_MESSAGE_SAMPLE_COUNT; i++)
        {
            i2s_read(CONFIG_SOUND_I2S_PORT_NUMBER, data, I2S_READ_DATA_SIZE, &readSize, portMAX_DELAY);
            udpSampleData[i] = *(int32_t*)data;
        }

        sendUdp(udpMessageData, UDP_MESSAGE_SIZE);
    }
    vTaskDelete(NULL);
}

void initializeSound()
{
    ESP_LOGI(SOUND_LOGGER_TAG, "Sound initialization");
    ledc_timer_config(&LEDC_TIMER_CONFIG);
    ledc_channel_config(&LEDC_CHANNEL_CONFIG);
    initAdc();

    i2s_driver_install(CONFIG_SOUND_I2S_PORT_NUMBER, &I2S_CONFIG, 0, NULL);
    i2s_set_pin(CONFIG_SOUND_I2S_PORT_NUMBER, &I2S_PIN_CONFIG);

    gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_PDWN, 1);

    initializeUdpMessageHeader();
}

void startSound()
{
    xTaskCreate(soundTask, 
        "sound",
        CONFIG_SOUND_TASK_STACK_SIZE,
        NULL,
        CONFIG_SOUND_TASK_PRIORITY,
        NULL);
}
