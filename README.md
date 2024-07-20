# RTOS_Communicating_Tasks
A FreeRTOS project with three sender tasks and one receiver task on an Eclipse CDT Embedded emulation board. Senders send timed messages to a queue, incrementing transmitted and blocked message counters. The receiver reads these messages. All tasks use semaphores for wake/sleep control
