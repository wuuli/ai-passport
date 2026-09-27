#pragma once
#include "freertos/FreeRTOS.h"

typedef struct QueueDefinition *QueueHandle_t;

QueueHandle_t xQueueCreate(size_t uxQueueLength, size_t uxItemSize);
BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait);
BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait);
void xQueueReset(QueueHandle_t xQueue);
void vQueueDelete(QueueHandle_t xQueue);
