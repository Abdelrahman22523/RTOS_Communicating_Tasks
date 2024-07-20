#include <stdio.h>
#include <stdlib.h>
#include "diag/trace.h"
#include <time.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"

// Define the CCM_RAM attribute for specific section placement
#define CCM_RAM __attribute__((section(".ccmram")))

// Queue handle for inter-task communication
QueueHandle_t testQueue;

// Queue size definitions
#define QUEUE_SIZE_3 3
#define QUEUE_SIZE_10 10

// Task periods and initial sender periods
#define Treceiver 100 // Receiver task period in ms
#define initialTsenderLower 50
#define initialTsenderUpper 150
#define TEST_TIMER_PERIOD pdMS_TO_TICKS(1000)

// Global variables for tracking statistics
uint32_t successfulTransmissions[3] = {0};
uint32_t blockedMessages[3] = {0};
uint32_t receivedMessages;
uint32_t Sum_Successful[3] = {0};
uint32_t Sum_Blocked[3] = {0};
uint32_t total_period1 = 0;
uint32_t total_period2 = 0;
uint32_t total_period3 = 0;
uint32_t sum_send = 0;
uint32_t sum_block = 0;

// Task and timer creation status indicators
BaseType_t xTaskCreateStatus;
BaseType_t timerStarted;

// Timer periods for sender tasks
const TickType_t timerPeriodsLower[] = {50, 80, 110, 140, 170, 200};
const TickType_t timerPeriodsUpper[] = {150, 200, 250, 300, 350, 400};

// Index to track the current period set
static int periodIndex = 0;

// Message buffers
char message[20];
char receivedMessage[20];

// Timer handles for sender and receiver tasks
TimerHandle_t xSenderTimers[3];
TimerHandle_t xReceiverTimer;

// Semaphore handles for synchronization
SemaphoreHandle_t xSenderSemaphores[3];
SemaphoreHandle_t xReceiverSemaphore;

// Time tracking variables
TickType_t totalSendingTime[3] = {0};
TickType_t totalBlockingTime[3] = {0};
TickType_t startTime = 0;
TickType_t currentTime = 0;
TickType_t avgSendingTime = 0;
TickType_t avgBlockingTime = 0;
TickType_t newPeriod = 0;
TickType_t timerPeriod = 0;

/////////////////////////////////////////////////////////
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"
/////////////////////////////////////////////////////////

// Function prototypes
void SenderTask(void *pvParameters);
void ReceiverTask(void *pvParameters);
void vSenderTimerCallback(TimerHandle_t xTimer);
void vReceiverTimerCallback(TimerHandle_t xTimer);
void vResetFunction(void);
void createQueue(void);
void createsemaphore(void);
void creat_sender_timer(void);
void create_reciver_timer(void);
void create_sender_task(void);
void create_reciver_task(void);
void start_timers(void);

// Sender Timer Callback Function
void vSenderTimerCallback(TimerHandle_t xTimer)
{
	//store the number of task in taskId
    int taskId = (int)(intptr_t)pvTimerGetTimerID(xTimer);
    // Release the semaphore associated with this timer
    xSemaphoreGive(xSenderSemaphores[taskId]);
}

// Receiver Timer Callback Function
void vReceiverTimerCallback(TimerHandle_t xTimer)
{
    // Release the receiver semaphore
    xSemaphoreGive(xReceiverSemaphore);
}

// Sender Task Function
void SenderTask(void *pvParameters)
{
	//store the number of task in taskId
    int taskId = (int)(intptr_t)pvParameters;

    while (1)
    {
        startTime = xTaskGetTickCount();
        // Wait for the semaphore to be released by the timer
        xSemaphoreTake(xSenderSemaphores[taskId], portMAX_DELAY);

        // Prepare the message
        snprintf(message, sizeof(message), "Time is %lu", startTime);
        //the following line print the message of the sender task.

        trace_printf("%s\n", message);

        // Attempt to send the message to the queue
        if (xQueueSend(testQueue, message, 0) == pdPASS)
        {
        	//if  true increment the namber of successful sent messages
            successfulTransmissions[taskId]++;
        }
        else
        {
        	//if  false increment the namber of blocked sent messages
            blockedMessages[taskId]++;
        }

        // Update the timer period
        newPeriod = pdMS_TO_TICKS(timerPeriodsLower[periodIndex] + (rand() % (timerPeriodsUpper[periodIndex] - timerPeriodsLower[periodIndex] + 1)));
        //increment the total period comparing to the taskId
        switch(taskId)
        {
            case 0:
                total_period1 += newPeriod;
                break;
            case 1:
                total_period2 += newPeriod;
                break;
            case 2:
                total_period3 += newPeriod;
                break;
        }
        //check for changing the period
        if (xTimerChangePeriod(xSenderTimers[taskId], newPeriod, 0) != pdPASS)
        {
            trace_printf("Failed to change timer period for task %d\n", taskId);
        }
    }
}

// Receiver Task Function
void ReceiverTask(void *parameters)
{
    while (1)
    {
        // Wait for the semaphore to be released by the timer
        xSemaphoreTake(xReceiverSemaphore, portMAX_DELAY);
        // Check the queue for messages
        if (xQueueReceive(testQueue, receivedMessage, 0) == pdPASS)
        {
            receivedMessages++;
            // print the received message
            trace_printf("Received: %s\n", receivedMessage);
            // If 1000 messages received, call the reset function
            if (receivedMessages >= 1000)
            {
                vResetFunction();
            }
        }
    }
}

// Reset Function to reset statistics and update periods
void vResetFunction(void)
{
    // Print statistics
    trace_printf("\n\n");
    trace_printf("Period: %d\n", periodIndex + 1); //the number of the iteration
    trace_printf("Total Received Messages = %lu\n", receivedMessages); //the total recived message

    trace_printf("Calculating the average time \n");

    // Calculate average sending time for each sender task
    int avgSendingTime1 = total_period1 / (successfulTransmissions[0] + blockedMessages[0]);
    int avgSendingTime2 = total_period2 / (successfulTransmissions[1] + blockedMessages[1]);
    int avgSendingTime3 = total_period3 / (successfulTransmissions[2] + blockedMessages[2]);
    //store the values of the average in array of size 3
    int arr[3] = {avgSendingTime1, avgSendingTime2, avgSendingTime3};

    for (int i = 0; i < 3; i++)
    {
        trace_printf("Sender Task %d\n", i + 1);
        //print the total number of successful dent messages and blocked messages
        trace_printf("Successful Transmissions = %lu\nBlocked Messages = %lu\n", successfulTransmissions[i], blockedMessages[i]);
        trace_printf("Total sent messages = %lu\n",(successfulTransmissions[i]+blockedMessages[i]));
        //print the average for each task
        trace_printf("Average Sending Time = %lu\n", arr[i]);
        //sum the total successful and blocked  message for each task
        sum_send += successfulTransmissions[i];
        sum_block += blockedMessages[i];
    }

    for (int i = 0; i < 3; i++)
    {
    	//sum the total successful and blocked  message for each task for all periods
        Sum_Successful[i] += successfulTransmissions[i];
        Sum_Blocked[i] += blockedMessages[i];
    }

    trace_printf("For this iteration: \n");
    trace_printf("Total Successful Transmissions = %lu\ntotal Blocked Messages = %lu\n", sum_send, sum_block);
    trace_printf("\n\n");

    // Reset counters
    for (int i = 0; i < 3; i++)
    {
        successfulTransmissions[i] = 0;
        blockedMessages[i] = 0;
        totalSendingTime[i] = 0;
        totalBlockingTime[i] = 0;
    }

    total_period1 = 0;
    total_period2 = 0;
    total_period3 = 0;
    receivedMessages = 0;
    sum_send = 0;
    sum_block = 0;

    // Clear the queue
    xQueueReset(testQueue);

    // Update sender timer periods
    periodIndex++;
    if ((size_t)periodIndex >= sizeof(timerPeriodsLower) / sizeof(timerPeriodsLower[0]))
    {
    	// if the period index greater than the number of periods "6"
        // Destroy timers and stop execution
        for (int i = 0; i < 3; i++)
        {
        	// print the total seccsseful and blockeed messages for each task for all periods
            trace_printf("Sender Task %d: Sum Successful Transmissions = %lu, Sum Blocked Messages = %lu\n", i + 1, Sum_Successful[i], Sum_Blocked[i]);
            // delete the senders timers
            xTimerDelete(xSenderTimers[i], 0);
        }
        // delete the receiver timers
        xTimerDelete(xReceiverTimer, 0);
        trace_printf("Game Over\n");
        exit(0);
    }
}

// Create Queue
void createQueue(void)
{
	// creating queue and store true or false in testQueue variable
	// now we use the queue of size 10 >>> if we want of size 3 just remove 10 and write 3 as we define the
	// 2 queues in the above
    testQueue = xQueueCreate(QUEUE_SIZE_10, sizeof(char[20]));
    if (testQueue == NULL)
    {
    	// if testQueue is false then failed to create queue
        trace_printf("Failed to create queue\n");
        exit(0);
    }
}

// Create semaphores
void createsemaphore(void)
{
    for (int i = 0; i < 3; i++)
    {
    	// create semaphore for each sender task
        xSenderSemaphores[i] = xSemaphoreCreateBinary();
        if (xSenderSemaphores[i] == NULL)
        {
        	//check expreation
            trace_printf("Failed to create sender semaphore %d\n", i);
            exit(0);
        }
    }
    // create semaphore for receiver
    xReceiverSemaphore = xSemaphoreCreateBinary();
    if (xReceiverSemaphore == NULL)
    {
        trace_printf("Failed to create receiver semaphore\n");
        exit(0);
    }
}

// Create sender timers with random periods
void creat_sender_timer(void)
{
    for (int i = 0; i < 3; i++)
    {
    	// the timer period between lower and upper
        timerPeriod = pdMS_TO_TICKS(initialTsenderLower + (rand() % (initialTsenderUpper - initialTsenderLower)));
        //create timer for each sender task
        xSenderTimers[i] = xTimerCreate("SenderTimer", timerPeriod, pdTRUE, (void *)(intptr_t)i, vSenderTimerCallback);
        if (xSenderTimers[i] == NULL)
        {
        	//check condition
            trace_printf("Handle sender_timer creation failure\n");
            exit(0);
        }
    }
}

// Create receiver timer with fixed period
void create_reciver_timer(void)
{
    xReceiverTimer = xTimerCreate("ReceiverTimer", pdMS_TO_TICKS(Treceiver), pdTRUE, NULL, vReceiverTimerCallback);
    if (xReceiverTimer == NULL)
    {
    	//check condition
        trace_printf("Handle reciver_timer creation failure\n");
        exit(0);
    }
}

// Create sender tasks
void create_sender_task(void)
{
    for (int i = 0; i < 3; i++)
    {
    	//creating tasks for each sender
        switch (i)
        {
            case 0:
                xTaskCreateStatus = xTaskCreate(SenderTask, "SenderTasksLow1", configMINIMAL_STACK_SIZE, (void *)(intptr_t)i, tskIDLE_PRIORITY + 1, NULL);
                break;
            case 1:
                xTaskCreateStatus = xTaskCreate(SenderTask, "SenderTasksLow2", configMINIMAL_STACK_SIZE, (void *)(intptr_t)i, tskIDLE_PRIORITY + 1, NULL);
                break;
            case 2:
                xTaskCreateStatus = xTaskCreate(SenderTask, "SenderTaskHigh", configMINIMAL_STACK_SIZE, (void *)(intptr_t)i, tskIDLE_PRIORITY + 2, NULL);
                break;
        }

        //checking if the task is created
        if (xTaskCreateStatus != pdPASS)
        {
            trace_printf("Failed to create sender task %d\n", i);
            exit(0);
        }
    }
}

// Create receiver task
void create_reciver_task(void)
{
	//create receiver task
    xTaskCreateStatus = xTaskCreate(ReceiverTask, "ReceiverTask", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
    //checking if the task is created
    if (xTaskCreateStatus != pdPASS)
    {
        trace_printf("Failed to create receiver task\n");
        exit(0);
    }
}

// Start timers
void start_timers(void)
{
    for (int i = 0; i < 3; i++)
    {
    	//start timer of each sender with the random period
        timerStarted = xTimerStart(xSenderTimers[i], 0);
        if (timerStarted != pdPASS)
        {
        	//check if the timer started successful
            trace_printf("Handle timer start failure\n");
            exit(0);
        }
    }
    //start timer of receiver task with fixed period
    timerStarted = xTimerStart(xReceiverTimer, 0);
    if (timerStarted != pdPASS)
    {
    	//check if the timer started successful
        trace_printf("Handle timer start failure\n");
        exit(0);
    }
}

// Main function
int main(int argc, char* argv[])
{
    srand(time(NULL)); // Seed the random number generator with the current time

    trace_printf("RTOS_projrect\n");

    // Initialize components
    createQueue();
    createsemaphore();

    //create timers
    creat_sender_timer();
    create_reciver_timer();

    //create tasks
    create_sender_task();
    create_reciver_task();

    // Start all timers
    start_timers();
    // Start the FreeRTOS scheduler
    vTaskStartScheduler();

    return 0;
}

#pragma GCC diagnostic pop

// FreeRTOS hook functions for handling errors and special events
void vApplicationMallocFailedHook(void)
{
    /* Called if a call to pvPortMalloc() fails because there is insufficient
    free memory available in the FreeRTOS heap.  pvPortMalloc() is called
    internally by FreeRTOS API functions that create tasks, queues, software
    timers, and semaphores.  The size of the FreeRTOS heap is set by the
    configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
    for (;;);
}

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
    (void)pcTaskName;
    (void)pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */
    for (;;);
}

void vApplicationIdleHook(void)
{
    volatile size_t xFreeStackSpace;

    /* This function is called on each cycle of the idle task.  In this case it
    does nothing useful, other than report the amount of FreeRTOS heap that
    remains unallocated. */
    xFreeStackSpace = xPortGetFreeHeapSize();

    if (xFreeStackSpace > 100)
    {
        /* By now, the kernel has allocated everything it is going to, so
        if there is a lot of heap remaining unallocated then
        the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
        reduced accordingly. */
    }
}

void vApplicationTickHook(void)
{
}

// Memory allocation for idle and timer tasks
StaticTask_t xIdleTaskTCB CCM_RAM;
StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] CCM_RAM;

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB CCM_RAM;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] CCM_RAM;

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
