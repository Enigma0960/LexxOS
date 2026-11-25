#pragma once

using SemaphoreHandle_t = void*;

SemaphoreHandle_t xSemaphoreCreateRecursiveMutex();
SemaphoreHandle_t xSemaphoreCreateBinary();
SemaphoreHandle_t xSemaphoreCreateMutex();
SemaphoreHandle_t xSemaphoreCreateCounting(unsigned maximum, unsigned initial);

int xSemaphoreTake(SemaphoreHandle_t, unsigned ticks);
int xSemaphoreGive(SemaphoreHandle_t);
int xSemaphoreGiveFromISR(SemaphoreHandle_t, int* higherPriorityWoken);

void vSemaphoreDelete(SemaphoreHandle_t);
