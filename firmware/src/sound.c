#include "sound.h"
#include "config.h"
#include "network/communication.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <driver/i2s.h>

#define MS_IN_S_COUNT 1000
#define US_IN_MS_COUNT 1000

#define I2S_READ_DATA_SIZE 8

#define SOUND_DATA_MESSAGE_FULL_HEADER_SIZE 17
#define SOUND_DATA_MESSAGE_PAYLOAD_HEADER_SIZE 9
#define SOUND_DATA_MESSAGE_SIZE (SOUND_DATA_MESSAGE_FULL_HEADER_SIZE + CONFIG_SOUND_MESSAGE_SAMPLE_COUNT * sizeof(int32_t))
#define SOUND_DATA_MESSAGE_ID 7
#define SOUND_DATA_MESSAGE_CURRENT_PAYLOAD_SIZE_OFFSET 4
#define SOUND_DATA_MESSAGE_CURRENT_ID_OFFSET 8
#define SOUND_DATA_MESSAGE_CURRENT_HOUR_OFFSET 10
#define SOUND_DATA_MESSAGE_CURRENT_MINUTE_OFFSET 11
#define SOUND_DATA_MESSAGE_CURRENT_SECOND_OFFSET 12
#define SOUND_DATA_MESSAGE_CURRENT_MS_OFFSET 13
#define SOUND_DATA_MESSAGE_CURRENT_US_OFFSET 15

#define RECORD_HEADER_SIZE 9
#define RECORD_PAYLOAD_SIZE_OFFSET 4

static const ledc_timer_config_t LEDC_TIMER_CONFIG =
{
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .timer_num = LEDC_TIMER_0,
    .duty_resolution = 2,
    .freq_hz = 11289600
};
 
static const ledc_channel_config_t LEDC_CHANNEL_CONFIG =
{
    .channel = LEDC_CHANNEL_0,
    .gpio_num = CONFIG_SOUND_CLOCK_IO,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .timer_sel = LEDC_TIMER_0,
    .duty = 2
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

static uint8_t soundDataMessageData[SOUND_DATA_MESSAGE_SIZE];
static int32_t* soundDataSampleData;
static size_t currentSoundDataSampleDataIndex = 0;

static volatile int isRecordEnabled = 0;
static volatile int isRecordPending = 0;
static volatile int recordHour = 0;
static volatile int recordMinute = 0;
static volatile int recordSecond = 0;
static volatile int recordMs = 0;
static volatile uint8_t recordId = 0;

static int32_t recordedSampleData[CONFIG_SOUND_MESSAGE_SAMPLE_COUNT];
static size_t currentRecordSampleDataIndex = 0;
static size_t recordedSampleCount = 0;
static size_t sampleCountToBeRecorded = 0;


static void initAdc()
{
    ESP_ERROR_CHECK(gpio_config(&ADC_IO_CONFIG));

    ESP_ERROR_CHECK(gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_PDWN, 0));

    // Set MODE to master 256 fs
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_MODE0, 1));
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_MODE1, 1));

    // Set format to I2S
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_FMT0, 1));
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_FMT1, 0));
    
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_OSR, 0)); // Set OSR to x64
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_BYPASS, 0)); // Set BYPAS to normal mode (HPF activated)
}

static void initializeSoundDataMessageHeader()
{
    *(uint32_t*)soundDataMessageData = htonl(SOUND_DATA_MESSAGE_ID);
    *(uint32_t*)(soundDataMessageData + SOUND_DATA_MESSAGE_CURRENT_PAYLOAD_SIZE_OFFSET) =
        htonl(SOUND_DATA_MESSAGE_PAYLOAD_HEADER_SIZE + CONFIG_SOUND_MESSAGE_SAMPLE_COUNT * sizeof(int32_t));
    
    soundDataSampleData = (int32_t*)(soundDataMessageData + SOUND_DATA_MESSAGE_FULL_HEADER_SIZE);
}

static void updateSoundDataMessageIdAndTimestamp()
{
    static uint16_t currentId = 0;
    
    *(uint16_t*)(soundDataMessageData + SOUND_DATA_MESSAGE_CURRENT_ID_OFFSET) = htons(currentId);

    struct timeval tv;
    struct tm timeinfo;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &timeinfo);

    soundDataMessageData[SOUND_DATA_MESSAGE_CURRENT_HOUR_OFFSET] = (uint8_t)timeinfo.tm_hour;
    soundDataMessageData[SOUND_DATA_MESSAGE_CURRENT_MINUTE_OFFSET] = (uint8_t)timeinfo.tm_min;
    soundDataMessageData[SOUND_DATA_MESSAGE_CURRENT_SECOND_OFFSET] = (uint8_t)timeinfo.tm_sec;
    *(uint16_t*)(soundDataMessageData + SOUND_DATA_MESSAGE_CURRENT_MS_OFFSET) = htons(tv.tv_usec / US_IN_MS_COUNT);
    *(uint16_t*)(soundDataMessageData + SOUND_DATA_MESSAGE_CURRENT_US_OFFSET) = htons(tv.tv_usec % US_IN_MS_COUNT);

    currentId++;
}

static void updateSoundDataMessage(int32_t sampleValue)
{
    soundDataSampleData[currentSoundDataSampleDataIndex] = sampleValue;

    currentSoundDataSampleDataIndex++;

    if (currentSoundDataSampleDataIndex == CONFIG_SOUND_MESSAGE_SAMPLE_COUNT)
    {
        sendUdp(soundDataMessageData, SOUND_DATA_MESSAGE_SIZE);
        updateSoundDataMessageIdAndTimestamp();
        currentSoundDataSampleDataIndex = 0;
    }
}

static void sendRecordHeader()
{
    uint8_t buffer[RECORD_HEADER_SIZE] =
    {
        0, 0, 0, 6,
        0, 0, 0, 0,
        recordId
    };
    *(uint32_t*)(buffer + RECORD_PAYLOAD_SIZE_OFFSET) =
        htonl(sampleCountToBeRecorded * sizeof(int32_t) + 1);
    sendTcp(buffer, RECORD_HEADER_SIZE);
}

static void updateRecordPending()
{
    if (isRecordPending)
    {
        struct timeval tv;
        struct tm timeinfo;
        gettimeofday(&tv, NULL);
        localtime_r(&tv.tv_sec, &timeinfo);

        if (timeinfo.tm_hour >= recordHour &&
            timeinfo.tm_min >= recordMinute &&
            timeinfo.tm_sec >= recordSecond &&
            (tv.tv_usec / US_IN_MS_COUNT) >= recordMs)
        {
            isRecordPending = 0;
            isRecordEnabled = 1;
            currentRecordSampleDataIndex = 0;
            recordedSampleCount = 0;
            sendRecordHeader();
            ESP_LOGI(SOUND_LOGGER_TAG, "Record started");
        }
    }
}

static void updateRecordEnabled(int32_t sampleValue)
{
    if (isRecordEnabled)
    {
        recordedSampleData[currentRecordSampleDataIndex] = sampleValue;

        currentRecordSampleDataIndex++;
        recordedSampleCount++;

        if (currentRecordSampleDataIndex == CONFIG_SOUND_MESSAGE_SAMPLE_COUNT)
        {
            sendTcp((uint8_t*)recordedSampleData, CONFIG_SOUND_MESSAGE_SAMPLE_COUNT * sizeof(int32_t));
            currentRecordSampleDataIndex = 0;
        }

        if (recordedSampleCount == sampleCountToBeRecorded)
        {
            sendTcp((uint8_t*)recordedSampleData, currentRecordSampleDataIndex * sizeof(int32_t));
            isRecordEnabled = 0;
        }
    }
}

static void updateRecordMessage(int32_t sampleValue)
{
    updateRecordPending();
    updateRecordEnabled(sampleValue);
}

static void soundTask(void* parameters)
{
    uint8_t data[I2S_READ_DATA_SIZE];
    size_t readSize;

    currentSoundDataSampleDataIndex = 0;
    updateSoundDataMessageIdAndTimestamp();
    while (1)
    {
        i2s_read(CONFIG_SOUND_I2S_PORT_NUMBER, data, I2S_READ_DATA_SIZE, &readSize, portMAX_DELAY);
        int32_t sampleValue = *(int32_t*)data;

        updateSoundDataMessage(sampleValue);
        updateRecordMessage(sampleValue);
    }
    vTaskDelete(NULL);
}

void initializeSound()
{
    ESP_LOGI(SOUND_LOGGER_TAG, "Sound initialization");
    ESP_ERROR_CHECK(ledc_timer_config(&LEDC_TIMER_CONFIG));
    ESP_ERROR_CHECK(ledc_channel_config(&LEDC_CHANNEL_CONFIG));
    initAdc();

    ESP_ERROR_CHECK(i2s_driver_install(CONFIG_SOUND_I2S_PORT_NUMBER, &I2S_CONFIG, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(CONFIG_SOUND_I2S_PORT_NUMBER, &I2S_PIN_CONFIG));

    ESP_ERROR_CHECK(gpio_set_level(CONFIG_SOUND_GPIO_OUTPUT_IO_PDWN, 1));

    initializeSoundDataMessageHeader();
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

void recordSound(uint8_t requestedRecordHour,
    uint8_t requestedRecordMinute,
    uint8_t requestedRecordSecond,
    uint16_t requestedRecordMs,
    uint16_t requestedRecordDurationMs,
    uint8_t requestedRecordRecordId)
{
    recordHour = requestedRecordHour;
    recordMinute = requestedRecordMinute;
    recordSecond = requestedRecordSecond;
    recordMs = requestedRecordMs;
    sampleCountToBeRecorded = (size_t)(CONFIG_SOUND_SAMPLE_FREQUENCY) * requestedRecordDurationMs / MS_IN_S_COUNT;
    recordId = requestedRecordRecordId;
    isRecordPending = 1;

    ESP_LOGI(SOUND_LOGGER_TAG, "Record requested");
}
