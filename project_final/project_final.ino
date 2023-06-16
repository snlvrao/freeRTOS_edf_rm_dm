#include "scheduler.h"

#define TASK_SET_1 0
#define TASK_SET_2 1

TaskHandle_t xHandle1 = NULL;
TaskHandle_t xHandle2 = NULL;
TaskHandle_t xHandle3 = NULL;
TaskHandle_t xHandle4 = NULL;

// the loop function runs over and over again forever
void loop() {}

static void testFunc1(void *pvParameters) {

  Serial.println("t1 start");
  // Serial.flush();

#if TASK_SET_1
  {
    // 100 ms
    for (volatile int i = 0; i < 75; i++) {
      for (volatile int j = 0; j < 1000; j++) {
      }
    }
  }
#elif TASK_SET_2
  {
    // 100 ms
    for (volatile int i = 0; i < 75; i++) {
      for (volatile int j = 0; j < 1000; j++) {
      }
    }
  }
#endif

  Serial.println("t1 stop");
}

static void testFunc2(void *pvParameters) {

  Serial.println("t2 start");
  // Serial.flush();

#if TASK_SET_1
  {
    // 200 ms
    for (volatile int i = 0; i < 150; i++) {
      for (volatile int j = 0; j < 1000; j++) {
      }
    }
  }
#elif TASK_SET_2
  {
    // 150 ms
    for (volatile int i = 0; i < 120; i++) {
      for (volatile int j = 0; j < 1000; j++) {
      }
    }
  }
#endif

  Serial.println("t2 stop");
}

static void testFunc3(void *pvParameters) {

  Serial.println("t3 start");
  // Serial.flush();

#if TASK_SET_1
  {
    // 150 ms
    for (volatile int i = 0; i < 120; i++) {
      for (volatile int j = 0; j < 1000; j++) {
      }
    }
  }
#elif TASK_SET_2
  {
    // 200 ms
    for (volatile int i = 0; i < 160; i++) {
      for (volatile int j = 0; j < 1000; j++) {
      }
    }
  }
#endif
  Serial.println("t3 stop");
}

static void testFunc4(void *pvParameters) {

  Serial.println("t4 start");
  // Serial.flush();

#if TASK_SET_1
  {
    // 300 ms
    for (volatile int i = 0; i < 240; i++) {
      for (volatile int j = 0; j < 1000; j++) {
      }
    }
  }
#elif TASK_SET_2
  {
    // 150 ms
    for (volatile int i = 0; i < 120; i++) {
      for (volatile int j = 0; j < 1000; j++) {
      }
    }
  }
#endif
  Serial.println("t4 stop");
}

int main(void) {
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB, on LEONARDO, MICRO, YUN, and other 32u4 based boards.
  }

#if TASK_SET_1
  
    int c1 = 100;
    int c2 = 200;
    int c3 = 150;
    int c4 = 300;

    int d1 = 400;
    int d2 = 700;
    int d3 = 1000;
    int d4 = 5000;

    int t1 = 400;
    int t2 = 800;
    int t3 = 1000;
    int t4 = 5000;
  
#elif TASK_SET_2
  
    int c1 = 100;
    int c2 = 150;
    int c3 = 200;
    int c4 = 150;

    int d1 = 400;
    int d2 = 200;
    int d3 = 700;
    int d4 = 1000;

    int t1 = 400;
    int t2 = 500;
    int t3 = 800;
    int t4 = 1000;
  
#endif

  vSchedulerInit();

  vSchedulerPeriodicTaskCreate(testFunc1, "t1", configMINIMAL_STACK_SIZE, &c1, 1, &xHandle1, pdMS_TO_TICKS(0), pdMS_TO_TICKS(t1), pdMS_TO_TICKS(c1), pdMS_TO_TICKS(d1), NULL);
  vSchedulerPeriodicTaskCreate(testFunc2, "t2", configMINIMAL_STACK_SIZE, &c2, 2, &xHandle2, pdMS_TO_TICKS(0), pdMS_TO_TICKS(t2), pdMS_TO_TICKS(c2), pdMS_TO_TICKS(d2), NULL);
  vSchedulerPeriodicTaskCreate(testFunc3, "t3", configMINIMAL_STACK_SIZE, &c3, 3, &xHandle3, pdMS_TO_TICKS(0), pdMS_TO_TICKS(t3), pdMS_TO_TICKS(c3), pdMS_TO_TICKS(d3), NULL);
  vSchedulerPeriodicTaskCreate(testFunc4, "t4", configMINIMAL_STACK_SIZE, &c4, 4, &xHandle4, pdMS_TO_TICKS(0), pdMS_TO_TICKS(t4), pdMS_TO_TICKS(c4), pdMS_TO_TICKS(d4), NULL);

  vSchedulerStart();

  /* If all is well, the scheduler will now be running, and the following line
	will never be reached. */

  for (;;)
    ;
}
