#include "scheduler.h"

TaskHandle_t xHandle1 = NULL;
TaskHandle_t xHandle2 = NULL;
TaskHandle_t xHandle3 = NULL;
TaskHandle_t xHandle4 = NULL;

// the loop function runs over and over again forever
void loop() {}


static void testFunc1( void *pvParameters )
{
  (void) pvParameters;
  int i,a;
  Serial.println("Task 1 is executing");
  delay(90);
  Serial.println("Task 1 is completed");
 
}

static void testFunc2( void *pvParameters )
{ 
  (void) pvParameters;  
  int i, a; 
  Serial.println("Task 2 is executing");  
  delay(190);
  Serial.println("Task 2 is completed");

}

static void testFunc3( void *pvParameters )
{ 
  (void) pvParameters;  
  int i, a; 
  Serial.println("Task 3 is executing");  
  delay(140);
  Serial.println("Task 3 is completed");
 
}

static void testFunc4( void *pvParameters )
{ 
  (void) pvParameters;  
  int i, a; 
  Serial.println("Task 4 is executing");
  delay(300);
  
  Serial.println("Task 4 is completed");
 
}

int main( void )
{
  init();
  Serial.begin(9600);
  
  char c1 = 'a';
  char c2 = 'b';    
  char c3 = 'c';
  char c4 = 'd';    

  vSchedulerInit(); 
  //TaskSet 1.1
//  vSchedulerPeriodicTaskCreate(testFunc1, "t1", configMINIMAL_STACK_SIZE, &c1, NULL, &xHandle1, pdMS_TO_TICKS(0), pdMS_TO_TICKS(150), pdMS_TO_TICKS(50), pdMS_TO_TICKS(150));
//  vSchedulerPeriodicTaskCreate(testFunc2, "t2", configMINIMAL_STACK_SIZE, &c2, NULL, &xHandle2, pdMS_TO_TICKS(0), pdMS_TO_TICKS(200), pdMS_TO_TICKS(150), pdMS_TO_TICKS(100));
  /* Args:                     FunctionName, Name, StackDepth, *pvParameters, Priority, TaskHandle, Phase,         Period,             MaxExecTime,       Deadline */
//TaskSet 1
  vSchedulerPeriodicTaskCreate(testFunc1, "t1", configMINIMAL_STACK_SIZE, &c1, NULL, &xHandle1, pdMS_TO_TICKS(0), pdMS_TO_TICKS(400), pdMS_TO_TICKS(100), pdMS_TO_TICKS(400));
  vSchedulerPeriodicTaskCreate(testFunc2, "t2", configMINIMAL_STACK_SIZE, &c2, NULL, &xHandle2, pdMS_TO_TICKS(0), pdMS_TO_TICKS(800), pdMS_TO_TICKS(200), pdMS_TO_TICKS(700));
  vSchedulerPeriodicTaskCreate(testFunc3, "t3", configMINIMAL_STACK_SIZE, &c3, NULL, &xHandle3, pdMS_TO_TICKS(0), pdMS_TO_TICKS(1000), pdMS_TO_TICKS(150), pdMS_TO_TICKS(1000));
  vSchedulerPeriodicTaskCreate(testFunc4, "t4", configMINIMAL_STACK_SIZE, &c4, NULL, &xHandle4, pdMS_TO_TICKS(0), pdMS_TO_TICKS(5000), pdMS_TO_TICKS(300), pdMS_TO_TICKS(5000));

//TaskSet 2
//  vSchedulerPeriodicTaskCreate(testFunc1, "t1", configMINIMAL_STACK_SIZE, &c1, NULL, &xHandle1, pdMS_TO_TICKS(0), pdMS_TO_TICKS(400), pdMS_TO_TICKS(100), pdMS_TO_TICKS(400));
//  vSchedulerPeriodicTaskCreate(testFunc2, "t2", configMINIMAL_STACK_SIZE, &c2, NULL, &xHandle2, pdMS_TO_TICKS(0), pdMS_TO_TICKS(500), pdMS_TO_TICKS(150), pdMS_TO_TICKS(200));
//  vSchedulerPeriodicTaskCreate(testFunc3, "t3", configMINIMAL_STACK_SIZE, &c3, NULL, &xHandle3, pdMS_TO_TICKS(0), pdMS_TO_TICKS(800), pdMS_TO_TICKS(200), pdMS_TO_TICKS(700));
//  vSchedulerPeriodicTaskCreate(testFunc4, "t4", configMINIMAL_STACK_SIZE, &c4, NULL, &xHandle4, pdMS_TO_TICKS(0), pdMS_TO_TICKS(1000), pdMS_TO_TICKS(150), pdMS_TO_TICKS(1000));


  vSchedulerStart();

  /* If all is well, the scheduler will now be running, and the following line
  will never be reached. */
  
  for( ;; );
}
