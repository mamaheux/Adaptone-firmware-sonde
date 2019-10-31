#include "config.h"
#include "event.h"
#include "network/ethernet.h"
#include "network/sntp.h"
#include "network/discovery.h"
#include "network/communication.h"
#include "network/communication.h"
#include "gpio.h"
#include "ledc.h"
#include "i2s.h"


#include <time.h>

#define STRFTIME_BUFFER_SIZE 64

//GPIO pins to drive the ADC
#define GPIO_OUTPUT_IO_FMT0   18 //
#define GPIO_OUTPUT_IO_FMT1   13 //
#define GPIO_OUTPUT_IO_PDWN   14 //
#define GPIO_OUTPUT_IO_BYPASS  2 //
#define GPIO_OUTPUT_IO_MODE1  15 
#define GPIO_OUTPUT_IO_MODE0   4 //
#define GPIO_OUTPUT_IO_OSR    12 //

#define REC_SIZE 400

#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_BYPASS) | (1ULL<<GPIO_OUTPUT_IO_MODE0) | (1ULL<<GPIO_OUTPUT_IO_OSR) | (1ULL<<GPIO_OUTPUT_IO_FMT1) | (1ULL<<GPIO_OUTPUT_IO_PDWN) | (1ULL<<GPIO_OUTPUT_IO_MODE1) | (1ULL<<GPIO_OUTPUT_IO_FMT0))

#define GPIO_INPUT_IO_0     23
#define GPIO_INPUT_IO_1     36
#define GPIO_INPUT_IO_2     39
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1) | (1ULL<<GPIO_INPUT_IO_2))
#define ESP_INTR_FLAG_DEFAULT 0

ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .timer_num  = LEDC_TIMER_0,
    .duty_resolution    = 2,
    .freq_hz    = 11289600
};
 
ledc_channel_config_t ledc_channel = {
    .channel    = LEDC_CHANNEL_0,
    .gpio_num   = 5,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .timer_sel  = LEDC_TIMER_0,
    .duty       = 2
};



static const int i2s_num = 0; // i2s port number

static const i2s_config_t i2s_config = {
     .mode = I2S_MODE_SLAVE | I2S_MODE_RX,
     .sample_rate = 44100,
     .bits_per_sample = 24,
     .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
     .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
     .intr_alloc_flags = 0, // default interrupt priority
     .dma_buf_count = 8,
     .dma_buf_len = 64,
     .use_apll = 0
};

static const i2s_pin_config_t pin_config = {
    .bck_io_num = 23,
    .ws_io_num = 39,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = 36
};



static void initializeLogger()
{
    esp_log_level_set(MAIN_LOGGER_TAG, MAIN_LOGGER_LEVEL);
    esp_log_level_set(EVENT_LOGGER_TAG, EVENT_LOGGER_LEVEL);
    esp_log_level_set(NETWORK_LOGGER_TAG, NETWORK_LOGGER_LEVEL);
}

static void messageHandler(uint8_t* buffer, size_t size)
{
    ESP_LOGI(MAIN_LOGGER_TAG, "Message received");
}

static void logCurrentUtc()
{
    char buffer[STRFTIME_BUFFER_SIZE];
    struct timeval tv;
    struct tm timeinfo;

    if (gettimeofday(&tv, NULL) == 0)
    {
        localtime_r(&tv.tv_sec, &timeinfo);
        strftime(buffer, sizeof(buffer), "%c", &timeinfo);
        ESP_LOGI(MAIN_LOGGER_TAG, "The current UTC time is: %s (us = %ld)", buffer, tv.tv_usec);
    }
}




static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

void app_main()
{
    ledc_timer_config(&ledc_timer);
    ledc_channel_config(&ledc_channel);

    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    // PWDN low
    gpio_set_level(14, 0); 

    // Set MODE to master 256 fs
    gpio_set_level(4, 1); 
    gpio_set_level(15, 1); 

    //set format to I2S

    gpio_set_level(13, 0); 
    gpio_set_level(18, 1); 

    // SET OSR to x64
    gpio_set_level(12, 0); 

    //SET BYPAS to normal mode (HPF activated)
    gpio_set_level(2, 0); 


    i2s_driver_install(0, &i2s_config, 0, NULL);
    i2s_set_pin(0, &pin_config);

    /*
    //interrupt of rising edge
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;

    //set as input mode    
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    

    //change gpio intrrupt type for one pin
    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);

    //remove isr handler for gpio number.
    gpio_isr_handler_remove(GPIO_INPUT_IO_0);
    //hook isr handler for specific gpio pin again
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);

    */

    initializeLogger();
    vTaskDelay(CONFIG_STARTUP_DELAY_MS / portTICK_PERIOD_MS);

    ESP_LOGI(MAIN_LOGGER_TAG, "Initialization");
    initializeEvent();
    initializeEthernet();
    initializeStnp();
    initializeDiscovery();
    initializeCommunication(messageHandler);

    ESP_LOGI(MAIN_LOGGER_TAG, "Task start");
    startDiscovery();
    startCommunication();

    gpio_set_level(14, 1); 
    size_t mysize;
    int ctn = 0;
    uint8_t * ptr;
    int32_t myarray[REC_SIZE] = {0};
    int32_t mydest = 0;
    //ptr = myarray;

    
    while(1)
    {    
        printf("HELLO\n");    
        for (ctn = 0; ctn < REC_SIZE; ctn++)
        {
            i2s_read(0, &myarray[ctn]+1, 3, &mysize, 1000);
        }
        
        for (ctn = 0; ctn < REC_SIZE; ctn++)
        {
            if(ctn%2 == 0)
            {
                printf("%d   \t", myarray[ctn]);
            }
            else
            {
                printf("%d   \n", myarray[ctn]);
            }
            
        }
        printf("\n");
        
    /*
        if (!ctn)
        {   
            printf("%d \t", mydest);
            ctn = 1;
        }
        else 
        {
            printf("%d   \n", mydest);
            ctn = 0;
        }
        */
        //logCurrentUtc();
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        
    }
}
