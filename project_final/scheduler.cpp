#include "scheduler.h"

#define schedUSE_TCB_ARRAY 1

/* Extended Task control block for managing periodic tasks within this library. */
typedef struct xExtended_TCB
{
	TaskFunction_t pvTaskCode;	  /* Function pointer to the code that will be run periodically. */
	const char *pcName;			  /* Name of the task. */
	UBaseType_t uxStackDepth;	  /* Stack size of the task. */
	void *pvParameters;			  /* Parameters to the task function. */
	UBaseType_t uxPriority;		  /* Priority of the task. */
	UBaseType_t uxBasePriority;	  /* Base Priority of the task. */
	TaskHandle_t *pxTaskHandle;	  /* Task handle for the task. */
	TickType_t xReleaseTime;	  /* Release time of the task. */
	TickType_t xRelativeDeadline; /* Relative deadline of the task. */
	TickType_t xAbsoluteDeadline; /* Absolute deadline of the task. */
	TickType_t xPeriod;			  /* Task period. */
	TickType_t xLastWakeTime;	  /* Last time stamp when the task was running. */
	TickType_t xMaxExecTime;	  /* Worst-case execution time of the task. */
	TickType_t xExecTime;		  /* Current execution time of the task. */

	BaseType_t xWorkIsDone; /* pdFALSE if the job is not finished, pdTRUE if the job is finished. */

#if (schedUSE_TCB_ARRAY == 1)
	BaseType_t xPriorityIsSet; /* pdTRUE if the priority is assigned. */
	BaseType_t xInUse;		   /* pdFALSE if this extended TCB is empty. */
#endif

#if (schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1)
	BaseType_t xExecutedOnce; /* pdTRUE if the task has executed once. */
#endif						  /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

#if (schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 || schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1)
	TickType_t xAbsoluteUnblockTime; /* The task will be unblocked at this time if it is blocked by the scheduler task. */
#endif								 /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME || schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

#if (schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1)
	BaseType_t xSuspended;			 /* pdTRUE if the task is suspended. */
	BaseType_t xMaxExecTimeExceeded; /* pdTRUE when execTime exceeds maxExecTime. */
#endif								 /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

	/* add if you need anything else */
	BaseType_t xExecStart;
	BaseType_t xBlocked;
	BaseType_t xResourceIndex;
	BaseType_t xResourceAccessed;
	TickType_t xRTickArray[schedMAX_NUMBER_OF_SHARED_RESOURCES];
} SchedTCB_t;

/* Resource Control Block to manage resource sharing */
typedef struct xExtended_RCB
{
	BaseType_t priorityCeiling;
	BaseType_t xInUse;
	SemaphoreHandle_t xMutexSem;
	TaskHandle_t xMutexHolder;
} SchedRCB_t;

#if (schedUSE_TCB_ARRAY == 1)
static BaseType_t prvGetTCBIndexFromHandle(TaskHandle_t xTaskHandle);
static void prvInitTCBArray(void);
/* Find index for an empty entry in xTCBArray. Return -1 if there is no empty entry. */
static BaseType_t prvFindEmptyElementIndexTCB(void);
/* Remove a pointer to extended TCB from xTCBArray. */
static void prvDeleteTCBFromArray(BaseType_t xIndex);
#endif /* schedUSE_TCB_ARRAY */

static void prvInitRCBArray(void);

static TickType_t xSystemStartTime = 0;

static void prvPeriodicTaskCode(void *pvParameters);
static void prvCreateAllTasks(void);

#if ((schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS) || (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DM))
static void prvSetFixedPriorities(void);
#endif /* schedSCHEDULING_POLICY */

#if ((schedSUB_SCHEDULING_POLICY == schedSUB_SCHEDULING_POLICY_OPCP) || (schedSUB_SCHEDULING_POLICY == schedSUB_SCHEDULING_POLICY_IPCP))
static void prvSetPriorityCeilings(void);
#endif /* schedSUB_SCHEDULING_POLICY */

#if (schedUSE_SCHEDULER_TASK == 1)
static void prvSchedulerCheckTimingError(TickType_t xTickCount, SchedTCB_t *pxTCB);
static void prvSchedulerFunction(void);
static void prvCreateSchedulerTask(void);
static void prvWakeScheduler(void);

#if (schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1)
static void prvPeriodicTaskRecreate(SchedTCB_t *pxTCB);
static void prvDeadlineMissedHook(SchedTCB_t *pxTCB, TickType_t xTickCount);
static void prvCheckDeadline(SchedTCB_t *pxTCB, TickType_t xTickCount);
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

#if (schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1)
static void prvExecTimeExceedHook(TickType_t xTickCount, SchedTCB_t *pxCurrentTask);
#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

#endif /* schedUSE_SCHEDULER_TASK */

#if (schedUSE_TCB_ARRAY == 1)
/* Array for extended TCBs. */
static SchedTCB_t xTCBArray[schedMAX_NUMBER_OF_PERIODIC_TASKS] = {0};
/* Counter for number of periodic tasks. */
static BaseType_t xTaskCounter = 0;
#endif /* schedUSE_TCB_ARRAY */

static SchedRCB_t xRCBArray[schedMAX_NUMBER_OF_SHARED_RESOURCES] = {0};

#if (schedUSE_SCHEDULER_TASK)
static TickType_t xSchedulerWakeCounter = 0; /* useful. why? */
static TaskHandle_t xSchedulerHandle = NULL; /* useful. why? */
#endif										 /* schedUSE_SCHEDULER_TASK */

#if (schedUSE_TCB_ARRAY == 1)
/* Returns index position in xTCBArray of TCB with same task handle as parameter. */
static BaseType_t prvGetTCBIndexFromHandle(TaskHandle_t xTaskHandle)
{
	static BaseType_t xIndex = 0;
	BaseType_t xIterator;

	for (xIterator = 0; xIterator < schedMAX_NUMBER_OF_PERIODIC_TASKS; xIterator++)
	{

		if (pdTRUE == xTCBArray[xIndex].xInUse && *xTCBArray[xIndex].pxTaskHandle == xTaskHandle)
		{
			return xIndex;
		}

		xIndex++;
		if (schedMAX_NUMBER_OF_PERIODIC_TASKS == xIndex)
		{
			xIndex = 0;
		}
	}
	return -1;
}

/* Initializes xTCBArray. */
static void prvInitTCBArray(void)
{
	UBaseType_t uxIndex;
	for (uxIndex = 0; uxIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; uxIndex++)
	{
		xTCBArray[uxIndex].xInUse = pdFALSE;
	}
}

/* Initialize xRCBArray. */
static void prvInitRCBArray(void)
{
	BaseType_t xIter;
	SchedRCB_t *pxRCB;

	for (xIter = 0; xIter < schedMAX_NUMBER_OF_SHARED_RESOURCES; xIter++)
	{
		pxRCB = &xRCBArray[xIter];

		pxRCB->priorityCeiling = 0;
		pxRCB->xInUse = pdFALSE;
		pxRCB->xMutexSem = xSemaphoreCreateMutex();
	}
}

/* Find index for an empty entry in xTCBArray. Returns -1 if there is no empty entry. */
static BaseType_t prvFindEmptyElementIndexTCB(void)
{
	/* your implementation goes here */
	static BaseType_t xIndex = 0;
	for (xIndex = 0; xIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; xIndex++)
	{
		if (xTCBArray[xIndex].xInUse == pdFALSE)
		{
			return xIndex;
		}
	}
	return -1;
}

/* Remove a pointer to extended TCB from xTCBArray. */
static void prvDeleteTCBFromArray(BaseType_t xIndex)
{
	/* your implementation goes here */
	xTCBArray[xIndex].xInUse = pdFALSE;
	xTaskCounter--;
}

#endif /* schedUSE_TCB_ARRAY */

/* The whole function code that is executed by every periodic task.
 * This function wraps the task code specified by the user. */
static void prvPeriodicTaskCode(void *pvParameters)
{
	static BaseType_t xIndex = 0;
	SchedTCB_t *pxThisTask;
	TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();

	/* your implementation goes here */
	xIndex = prvGetTCBIndexFromHandle(xCurrentTaskHandle);
	pxThisTask = &xTCBArray[xIndex];

	/* Check the handle is not NULL. */
	configASSERT(pxThisTask != NULL);

	/* If required, use the handle to obtain further information about the task. */
	/* You may find the following code helpful...
	BaseType_t xIndex;
	for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
	{

	}
	*/

#if (schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1)
	/* your implementation goes here */
	pxThisTask->xExecutedOnce = pdTRUE;
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

	if (0 == pxThisTask->xReleaseTime)
	{
		pxThisTask->xLastWakeTime = xSystemStartTime;
	}
	else
	{
		xTaskDelayUntil(&pxThisTask->xLastWakeTime, pxThisTask->xReleaseTime);
	}

	for (;;)
	{
		pxThisTask->xExecStart = pdTRUE;

		// for (BaseType_t xIter = 0; xIter < xTaskCounter; xIter++)
		// {
		// 	Serial.println(xTCBArray[xIter].uxPriority);
		// }

		/* Execute the task function specified by the user. */
		pxThisTask->pvTaskCode(pvParameters);

		pxThisTask->xWorkIsDone = pdTRUE;
		pxThisTask->xExecStart = pdFALSE;
		pxThisTask->xExecTime = 0;

#if (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF)
		pxThisTask->xAbsoluteDeadline = pxThisTask->xLastWakeTime + pxThisTask->xPeriod + pxThisTask->xRelativeDeadline;
		prvWakeScheduler();
#endif /* schedSCHEDULING_POLICY */

		xTaskDelayUntil(&pxThisTask->xLastWakeTime, pxThisTask->xPeriod);
	}
}

/* Creates a periodic task. */
void vSchedulerPeriodicTaskCreate(TaskFunction_t pvTaskCode, const char *pcName, UBaseType_t uxStackDepth, void *pvParameters, UBaseType_t uxPriority,
								  TaskHandle_t *pxCreatedTask, TickType_t xPhaseTick, TickType_t xPeriodTick, TickType_t xMaxExecTimeTick, TickType_t xDeadlineTick, TickType_t xRTickArray[schedMAX_NUMBER_OF_SHARED_RESOURCES])
{
	taskENTER_CRITICAL();

	BaseType_t xIter = 0;
	SchedTCB_t *pxNewTCB;

#if (schedUSE_TCB_ARRAY == 1)
	BaseType_t xIndex = prvFindEmptyElementIndexTCB();
	configASSERT(xTaskCounter < schedMAX_NUMBER_OF_PERIODIC_TASKS);
	configASSERT(xIndex != -1);
	pxNewTCB = &xTCBArray[xIndex];
#endif /* schedUSE_TCB_ARRAY */

	/* Intialize item. */

	pxNewTCB->pvTaskCode = pvTaskCode;
	pxNewTCB->pcName = pcName;
	pxNewTCB->uxStackDepth = uxStackDepth;
	pxNewTCB->pvParameters = pvParameters;
	pxNewTCB->uxPriority = uxPriority;
	pxNewTCB->uxBasePriority = uxPriority;
	pxNewTCB->pxTaskHandle = pxCreatedTask;
	pxNewTCB->xReleaseTime = xPhaseTick;
	pxNewTCB->xPeriod = xPeriodTick;

	/* Populate the rest */
	/* your implementation goes here */
	pxNewTCB->xAbsoluteDeadline = xPhaseTick + xDeadlineTick;
	pxNewTCB->xRelativeDeadline = xDeadlineTick;
	pxNewTCB->xWorkIsDone = pdFALSE;
	pxNewTCB->xExecTime = 0;
	pxNewTCB->xMaxExecTime = xMaxExecTimeTick;

#if (schedUSE_TCB_ARRAY == 1)
	pxNewTCB->xInUse = pdTRUE;
#endif /* schedUSE_TCB_ARRAY */

#if ((schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS) || (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DM))
	/* member initialization */
	/* your implementation goes here */
	pxNewTCB->xPriorityIsSet = pdFALSE;
#endif /* schedSCHEDULING_POLICY */

#if (schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1)
	/* member initialization */
	/* your implementation goes here */
	pxNewTCB->xExecutedOnce = pdFALSE;
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

#if (schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1)
	pxNewTCB->xSuspended = pdFALSE;
	pxNewTCB->xMaxExecTimeExceeded = pdFALSE;
#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

	/* DEBUG - SUNIL */
	for (xIter = 0; xIter < schedMAX_NUMBER_OF_SHARED_RESOURCES; xIter++)
	{
		pxNewTCB->xRTickArray[xIter] = xRTickArray[xIter];
	}

	pxNewTCB->xExecStart = pdFALSE;
	pxNewTCB->xBlocked = pdFALSE;
	pxNewTCB->xResourceAccessed = pdFALSE;

#if (schedUSE_TCB_ARRAY == 1)
	xTaskCounter++;
#endif /* schedUSE_TCB_SORTED_LIST */
	taskEXIT_CRITICAL();
	// Serial.println(pxNewTCB->xMaxExecTime);
}

/* Deletes a periodic task. */
void vSchedulerPeriodicTaskDelete(TaskHandle_t xTaskHandle)
{
	/* your implementation goes here */
	static BaseType_t xIndex = 0;

	xIndex = prvGetTCBIndexFromHandle(xTaskHandle);
	prvDeleteTCBFromArray(xIndex);
	vTaskDelete(xTaskHandle);
}

/* Creates all periodic tasks stored in TCB array, or TCB list. */
static void prvCreateAllTasks(void)
{
	SchedTCB_t *pxTCB;

#if (schedUSE_TCB_ARRAY == 1)
	BaseType_t xIndex;
	for (xIndex = 0; xIndex < xTaskCounter; xIndex++)
	{
		configASSERT(pdTRUE == xTCBArray[xIndex].xInUse);
		pxTCB = &xTCBArray[xIndex];

		BaseType_t xReturnValue = xTaskCreate((TaskFunction_t)prvPeriodicTaskCode,
											  pxTCB->pcName,
											  pxTCB->uxStackDepth,
											  pxTCB->pvParameters, pxTCB->uxPriority,
											  pxTCB->pxTaskHandle);
	}
#endif /* schedUSE_TCB_ARRAY */
}

#if ((schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS) || (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DM))
/* Initiazes fixed priorities of all periodic tasks with respect to RMS or DM policy. */
static void prvSetFixedPriorities(void)
{
	BaseType_t xIter, xIndex;
	TickType_t xShortest, xPreviousShortest = 0;
	SchedTCB_t *pxShortestTaskPointer;

#if (schedUSE_SCHEDULER_TASK == 1)
	BaseType_t xHighestPriority = schedSCHEDULER_PRIORITY;
#else
	BaseType_t xHighestPriority = configMAX_PRIORITIES;
#endif /* schedUSE_SCHEDULER_TASK */

	for (xIter = 0; xIter < xTaskCounter; xIter++)
	{
		xShortest = portMAX_DELAY;

		/* search for shortest period */
		for (xIndex = 0; xIndex < xTaskCounter; xIndex++)
		{
			/* your implementation goes here */
			if (xTCBArray[xIndex].xInUse == pdFALSE)
				continue;
			if (xTCBArray[xIndex].xPriorityIsSet == pdTRUE)
				continue;

#if (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS)
			/* your implementation goes here */
			if (xShortest > xTCBArray[xIndex].xPeriod)
			{
				xShortest = xTCBArray[xIndex].xPeriod;
				pxShortestTaskPointer = &xTCBArray[xIndex];
			}
#endif /* schedSCHEDULING_POLICY */
#if (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
			if (xShortest > xTCBArray[xIndex].xRelativeDeadline)
			{
				xShortest = xTCBArray[xIndex].xRelativeDeadline;
				pxShortestTaskPointer = &xTCBArray[xIndex];
			}
#endif /* schedSCHEDULING_POLICY */
		}

		/* set highest priority to task with xShortest period (the highest priority is configMAX_PRIORITIES-1) */

		/* your implementation goes here */

		if (xHighestPriority > 0)
		{
			xHighestPriority--;
		}
		else
		{
			xHighestPriority = 0;
		}

		configASSERT(0 <= xHighestPriority);
		pxShortestTaskPointer->uxPriority = xHighestPriority;
		pxShortestTaskPointer->uxBasePriority = pxShortestTaskPointer->uxPriority;
		pxShortestTaskPointer->xPriorityIsSet = pdTRUE;
		xPreviousShortest = xShortest;

		Serial.print(pxShortestTaskPointer->pcName);
		Serial.print(" has priority ");
		Serial.println(pxShortestTaskPointer->uxPriority);
		// Serial.flush();
	}
}
#endif /* schedSCHEDULING_POLICY */

#if (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF)
static void prvSetEDFInitialPriorities(void)
{
	BaseType_t xIter, xIndex;
	TickType_t xShortest, xPreviousShortest = 0;
	SchedTCB_t *pxShortestTaskPointer;

#if (schedUSE_SCHEDULER_TASK == 1)
	BaseType_t xHighestPriority = schedSCHEDULER_PRIORITY;
#else
	BaseType_t xHighestPriority = configMAX_PRIORITIES;
#endif /* schedUSE_SCHEDULER_TASK */

	for (xIter = 0; xIter < xTaskCounter; xIter++)
	{
		xShortest = portMAX_DELAY;

		/* search for shortest period */
		for (xIndex = 0; xIndex < xTaskCounter; xIndex++)
		{
			/* your implementation goes here */
			if (xTCBArray[xIndex].xInUse == pdFALSE)
				continue;
			if (xTCBArray[xIndex].xPriorityIsSet == pdTRUE)
				continue;

			if (xShortest > xTCBArray[xIndex].xRelativeDeadline)
			{
				xShortest = xTCBArray[xIndex].xRelativeDeadline;
				pxShortestTaskPointer = &xTCBArray[xIndex];
			}
		}

		/* set highest priority to task with xShortest period (the highest priority is configMAX_PRIORITIES-1) */

		/* your implementation goes here */

		if (xHighestPriority > 0)
		{
			xHighestPriority--;
		}
		else
		{
			xHighestPriority = 0;
		}

		configASSERT(0 <= xHighestPriority);
		pxShortestTaskPointer->uxPriority = xHighestPriority;
		pxShortestTaskPointer->uxBasePriority = pxShortestTaskPointer->uxPriority;
		pxShortestTaskPointer->xPriorityIsSet = pdTRUE;
		xPreviousShortest = xShortest;

		Serial.print(pxShortestTaskPointer->pcName);
		Serial.print(" has priority ");
		Serial.println(pxShortestTaskPointer->uxPriority);
		// Serial.flush();
	}
}

static void prvUpdateEDFPriorities(TickType_t xTickCount)
{
	BaseType_t xIter, xIndex;
	TickType_t xShortestTimeLeft, xPreviousShortest = 0;
	SchedTCB_t *pxShortestTaskPointer;

#if (schedUSE_SCHEDULER_TASK == 1)
	BaseType_t xHighestPriority = schedSCHEDULER_PRIORITY;
#else
	BaseType_t xHighestPriority = configMAX_PRIORITIES;
#endif /* schedUSE_SCHEDULER_TASK */

	for (xIter = 0; xIter < xTaskCounter; xIter++)
	{
		xTCBArray[xIter].xPriorityIsSet = pdFALSE;
	}

	for (xIter = 0; xIter < xTaskCounter; xIter++)
	{
		xShortestTimeLeft = portMAX_DELAY;

		/* search for shortest period */
		for (xIndex = 0; xIndex < xTaskCounter; xIndex++)
		{
			/* your implementation goes here */
			if (xTCBArray[xIndex].xInUse == pdFALSE)
				continue;
			if (xTCBArray[xIndex].xPriorityIsSet == pdTRUE)
				continue;

			if (xShortestTimeLeft > xTCBArray[xIndex].xAbsoluteDeadline)
			{
				xShortestTimeLeft = xTCBArray[xIndex].xAbsoluteDeadline;
				pxShortestTaskPointer = &xTCBArray[xIndex];
			}
		}

		/* set highest priority to task with xShortest period (the highest priority is configMAX_PRIORITIES-1) */

		/* your implementation goes here */
		if (xHighestPriority > 0)
		{
			xHighestPriority--;
		}
		else
		{
			xHighestPriority = 0;
		}

		configASSERT(0 <= xHighestPriority);
		pxShortestTaskPointer->uxPriority = xHighestPriority;
		pxShortestTaskPointer->uxBasePriority = pxShortestTaskPointer->uxPriority;
		pxShortestTaskPointer->xPriorityIsSet = pdTRUE;
		xPreviousShortest = xShortestTimeLeft;

		vTaskPrioritySet(*pxShortestTaskPointer->pxTaskHandle, pxShortestTaskPointer->uxPriority);

		// Serial.print(pxShortestTaskPointer->pcName);
		// Serial.print(" ");
		// Serial.print(xShortestTimeLeft);
		// Serial.print(" ");
		// Serial.println(pxShortestTaskPointer->uxPriority);
	}
}
#endif /* schedSCHEDULING_POLICY */

#if ((schedSUB_SCHEDULING_POLICY == schedSUB_SCHEDULING_POLICY_OPCP) || (schedSUB_SCHEDULING_POLICY == schedSUB_SCHEDULING_POLICY_IPCP))
static void prvSetPriorityCeilings(void)
{
	BaseType_t xIter, xIndex;
	SchedTCB_t *pxTCB;
	SchedRCB_t *pxRCB;

	prvInitRCBArray();

	for (xIndex = 0; xIndex < xTaskCounter; xIndex++)
	{
		pxTCB = &xTCBArray[xIndex];
		configASSERT(pdTRUE == pxTCB->xInUse);

		for (xIter = 0; xIter < schedMAX_NUMBER_OF_SHARED_RESOURCES; xIter++)
		{
			if (pxTCB->xRTickArray[xIter] > 0)
			{
				pxRCB = &xRCBArray[xIter];

				if (pxRCB->priorityCeiling < pxTCB->uxPriority)
				{
					pxRCB->priorityCeiling = pxTCB->uxPriority;
				}
			}
		}
	}

	Serial.print("R1 priority ceiling - ");
	Serial.println(xRCBArray[0].priorityCeiling);
	Serial.print("R2 priority ceiling - ");
	Serial.println(xRCBArray[1].priorityCeiling);
	// Serial.flush();
}
#endif /* schedSUB_SCHEDULING_POLICY */

#if (schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1)
/* Recreates a deleted task that still has its information left in the task array (or list). */
static void prvPeriodicTaskRecreate(SchedTCB_t *pxTCB)
{
	BaseType_t xReturnValue = xTaskCreate((TaskFunction_t)prvPeriodicTaskCode,
										  pxTCB->pcName,
										  pxTCB->uxStackDepth,
										  pxTCB->pvParameters, pxTCB->uxPriority,
										  pxTCB->pxTaskHandle);

	if (pdPASS == xReturnValue)
	{
		/* your implementation goes here */
		pxTCB->xExecutedOnce = pdFALSE;
		pxTCB->xSuspended = pdFALSE;
		pxTCB->xWorkIsDone = pdFALSE;
		pxTCB->xMaxExecTimeExceeded = pdFALSE;

		Serial.print(pxTCB->pcName);
		Serial.println(" task recreated");
		// Serial.flush();
	}
	else
	{
		/* if task creation failed */
		Serial.print(pxTCB->pcName);
		Serial.println(" task creation failed");
		Serial.flush();

		configASSERT(pdPASS == xReturnValue);
	}
}

/* Called when a deadline of a periodic task is missed.
 * Deletes the periodic task that has missed it's deadline and recreate it.
 * The periodic task is released during next period. */
static void prvDeadlineMissedHook(SchedTCB_t *pxTCB, TickType_t xTickCount)
{
	Serial.print(pxTCB->pcName);
	Serial.println(" task deadline missed");
	// Serial.flush();

	/* Delete the pxTask and recreate it. */
	vTaskDelete(*pxTCB->pxTaskHandle);
	pxTCB->xExecTime = 0;
	prvPeriodicTaskRecreate(pxTCB);

	/* Need to reset next WakeTime for correct release. */
	/* your implementation goes here */
	pxTCB->xReleaseTime = pxTCB->xLastWakeTime + pxTCB->xPeriod;
	pxTCB->xLastWakeTime = 0;
	pxTCB->xAbsoluteDeadline = pxTCB->xRelativeDeadline + pxTCB->xReleaseTime;
}

/* Checks whether given task has missed deadline or not. */
static void prvCheckDeadline(SchedTCB_t *pxTCB, TickType_t xTickCount)
{
	/* check whether deadline is missed. */
	/* your implementation goes here */
	if ((pxTCB->xExecutedOnce == pdTRUE) && (pxTCB->xWorkIsDone == pdFALSE))
	{
		pxTCB->xAbsoluteDeadline = pxTCB->xLastWakeTime + pxTCB->xRelativeDeadline;
		if (pxTCB->xAbsoluteDeadline < xTickCount)
		{
			prvDeadlineMissedHook(pxTCB, xTickCount);
		}
	}
}
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

#if (schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1)

/* Called if a periodic task has exceeded its worst-case execution time.
 * The periodic task is blocked until next period. A context switch to
 * the scheduler task occur to block the periodic task. */
static void prvExecTimeExceedHook(TickType_t xTickCount, SchedTCB_t *pxCurrentTask)
{
	Serial.print(pxCurrentTask->pcName);
	Serial.println(" exec time exceeded ");
	Serial.flush();

	// Release any accessed resources
	if (pxCurrentTask->xResourceAccessed == pdTRUE)
	{
		SchedRCB_t *pxRCB = &xRCBArray[pxCurrentTask->xResourceIndex];
		BaseType_t status = xSemaphoreGive(pxRCB->xMutexSem);

		if (status == pdTRUE)
		{
			pxRCB->xInUse = pdFALSE;
			pxCurrentTask->xBlocked = pdFALSE;
			pxCurrentTask->xResourceAccessed = pdFALSE;
		}
	}

	pxCurrentTask->xMaxExecTimeExceeded = pdTRUE;
	/* Is not suspended yet, but will be suspended by the scheduler later. */
	pxCurrentTask->xSuspended = pdTRUE;
	pxCurrentTask->xAbsoluteUnblockTime = pxCurrentTask->xLastWakeTime + pxCurrentTask->xPeriod;
	pxCurrentTask->xExecTime = 0;

	BaseType_t xHigherPriorityTaskWoken;
	vTaskNotifyGiveFromISR(xSchedulerHandle, &xHigherPriorityTaskWoken);
	xTaskResumeFromISR(xSchedulerHandle);
}
#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

#if (schedUSE_SCHEDULER_TASK == 1)
/* Called by the scheduler task. Checks all tasks for any enabled
 * Timing Error Detection feature. */
static void prvSchedulerCheckTimingError(TickType_t xTickCount, SchedTCB_t *pxTCB)
{
	/* your implementation goes here */

#if (schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1)
	/* check if task missed deadline */
	/* your implementation goes here */
	prvCheckDeadline(pxTCB, xTickCount);
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

#if (schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1)
	if (pdTRUE == pxTCB->xMaxExecTimeExceeded)
	{
		pxTCB->xMaxExecTimeExceeded = pdFALSE;
		vTaskSuspend(*pxTCB->pxTaskHandle);
	}
	if (pdTRUE == pxTCB->xSuspended)
	{
		if ((signed)(pxTCB->xAbsoluteUnblockTime - xTickCount) <= 0)
		{
			pxTCB->xSuspended = pdFALSE;
			pxTCB->xLastWakeTime = xTickCount;
			vTaskResume(*pxTCB->pxTaskHandle);
		}
	}
#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

	return;
}

/* Function code for the scheduler task. */
static void prvSchedulerFunction(void *pvParameters)
{
	BaseType_t xIndex = 0;

	for (;;)
	{
		taskENTER_CRITICAL();

		TickType_t xTickCount = xTaskGetTickCount();

#if (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF)
		{
			prvUpdateEDFPriorities(xTickCount);
		}
#endif /* schedSCHEDULING_POLICY */

		taskEXIT_CRITICAL();

#if (schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1)
		// TickType_t xTickCount = xTaskGetTickCount();
		SchedTCB_t *pxTCB;

		/* your implementation goes here. */
		for (xIndex = 0; xIndex < xTaskCounter; xIndex++)
		{
			pxTCB = &xTCBArray[xIndex];

			if (pxTCB->xInUse == pdTRUE)
			{
				prvSchedulerCheckTimingError(xTickCount, pxTCB);
			}
		}

#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
	}
}

/* Creates the scheduler task. */
static void prvCreateSchedulerTask(void)
{
	xTaskCreate((TaskFunction_t)prvSchedulerFunction, "Scheduler", schedSCHEDULER_TASK_STACK_SIZE, NULL, schedSCHEDULER_PRIORITY, &xSchedulerHandle);
}
#endif /* schedUSE_SCHEDULER_TASK */

#if (schedUSE_SCHEDULER_TASK == 1)
/* Wakes up (context switches to) the scheduler task. */
static void prvWakeScheduler(void)
{
	BaseType_t xHigherPriorityTaskWoken;
	vTaskNotifyGiveFromISR(xSchedulerHandle, &xHigherPriorityTaskWoken);
	xTaskResumeFromISR(xSchedulerHandle);
}

/* Called every software tick. */
// In FreeRTOSConfig.h,
// Enable configUSE_TICK_HOOK
// Enable INCLUDE_xTaskGetIdleTaskHandle
// Enable INCLUDE_xTaskGetCurrentTaskHandle
void vApplicationTickHook(void)
{
	SchedTCB_t *pxCurrentTask;
	TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();
	UBaseType_t flag = 0;
	BaseType_t xIndex;
	BaseType_t prioCurrentTask = uxTaskPriorityGet(xCurrentTaskHandle);

	for (xIndex = 0; xIndex < xTaskCounter; xIndex++)
	{
		pxCurrentTask = &xTCBArray[xIndex];
		if ((pxCurrentTask->uxPriority == prioCurrentTask) && (pxCurrentTask->xBlocked == pdFALSE) && (pxCurrentTask->xExecStart == pdTRUE))
		{
			flag = 1;
			break;
		}
	}

	if (xCurrentTaskHandle != xSchedulerHandle && xCurrentTaskHandle != xTaskGetIdleTaskHandle() && flag == 1)
	{
		pxCurrentTask->xExecTime++;

#if (schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1)
		if (pxCurrentTask->xMaxExecTime < pxCurrentTask->xExecTime) // DEBUG - SUNIL Changed from <= to <
		{
			if (pdFALSE == pxCurrentTask->xMaxExecTimeExceeded)
			{
				if (pdFALSE == pxCurrentTask->xSuspended)
				{
					prvExecTimeExceedHook(xTaskGetTickCountFromISR(), pxCurrentTask);
				}
			}
		}
#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
	}

#if (schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1)
	xSchedulerWakeCounter++;
	if (xSchedulerWakeCounter == schedSCHEDULER_TASK_PERIOD)
	{
		xSchedulerWakeCounter = 0;
		prvWakeScheduler();
	}
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
}
#endif /* schedUSE_SCHEDULER_TASK */

void vRequestResource(TaskHandle_t xTaskHandle, BaseType_t xResourceIndex)
{
	BaseType_t xIter, status, flag = 1;
	BaseType_t xCurrentTaskIndex = prvGetTCBIndexFromHandle(xTaskHandle);
	BaseType_t prioCurrentTask = uxTaskPriorityGet(xTaskHandle);

	SchedRCB_t *pxRCB = &xRCBArray[xResourceIndex];
	SchedTCB_t *pxTCB = &xTCBArray[xCurrentTaskIndex];

	SchedRCB_t *xBlockingResource;

#if (schedSUB_SCHEDULING_POLICY == schedSUB_SCHEDULING_POLICY_OPCP)
	// Check if requested resource is not already blocked
	if (pxRCB->xInUse == pdFALSE)
	{
		// Check if priority of current task is greater than priority ceiling of all blocked resources
		for (xIter = 0; xIter < schedMAX_NUMBER_OF_SHARED_RESOURCES; xIter++)
		{
			pxRCB = &xRCBArray[xIter];

			// Do not block nested resource access if other resource is held by the same task handle
			if ((pxRCB->xInUse == pdTRUE) && (prioCurrentTask <= pxRCB->priorityCeiling) && (pxRCB->xMutexHolder != xTaskHandle))
			{
				xBlockingResource = pxRCB;
				flag = 0;
				break;
			}
		}
	}

	pxRCB = &xRCBArray[xResourceIndex];

	if (flag)
	{
		// Grant resource access or put in blocking list
		if (uxSemaphoreGetCount(pxRCB->xMutexSem) == 0)
		{
			Serial.print(pxTCB->pcName);
			Serial.println(" blocked");

			pxTCB->xBlocked = pdTRUE;

			// Get task handle of mutex holder
			BaseType_t xMutexHolderTaskIndex = prvGetTCBIndexFromHandle(pxRCB->xMutexHolder);
			SchedTCB_t *pxMutexHolderTCB = &xTCBArray[xMutexHolderTaskIndex];

			// Set priority of mutex holder to priority of blocked task
			if (pxMutexHolderTCB->uxPriority < pxTCB->uxPriority)
			{
				vTaskPrioritySet(pxRCB->xMutexHolder, pxTCB->uxPriority);
			}
		}
		status = xSemaphoreTake(pxRCB->xMutexSem, portMAX_DELAY);

		if (status == pdTRUE)
		{
			pxRCB->xInUse = pdTRUE;
			pxTCB->xBlocked = pdFALSE;
			pxTCB->xResourceAccessed = pdTRUE;
			pxTCB->xResourceIndex = xResourceIndex;
			pxRCB->xMutexHolder = xTaskHandle;

			Serial.print(pxTCB->pcName);
			Serial.print(" acquire R");
			Serial.println(xResourceIndex + 1);
		}
	}
	else
	{
		Serial.print(pxTCB->pcName);
		Serial.println(" blocked");

		pxTCB->xBlocked = pdTRUE;

		// Wait on blocking resource
		status = xSemaphoreTake(xBlockingResource->xMutexSem, portMAX_DELAY);

		if (status == pdTRUE)
		{
			xSemaphoreTake(pxRCB->xMutexSem, portMAX_DELAY);

			// Release blocking resource
			xSemaphoreGive(xBlockingResource->xMutexSem);

			pxRCB->xInUse = pdTRUE;
			pxTCB->xBlocked = pdFALSE;
			pxTCB->xResourceAccessed = pdTRUE;
			pxTCB->xResourceIndex = xResourceIndex;
			pxRCB->xMutexHolder = xTaskHandle;

			Serial.print(pxTCB->pcName);
			Serial.print(" acquire R");
			Serial.println(xResourceIndex + 1);
		}
	}
#elif (schedSUB_SCHEDULING_POLICY == schedSUB_SCHEDULING_POLICY_IPCP)
	// Grant resource and immediately set the task's priority to ceiling priority
	status = xSemaphoreTake(pxRCB->xMutexSem, portMAX_DELAY);

	if (status == pdTRUE)
	{
		vTaskPrioritySet(xTaskHandle, pxRCB->priorityCeiling);

		pxRCB->xInUse = pdTRUE;
		pxTCB->xBlocked = pdFALSE;
		pxTCB->xResourceAccessed = pdTRUE;
		pxTCB->xResourceIndex = xResourceIndex;
		pxRCB->xMutexHolder = xTaskHandle;

		Serial.print(pxTCB->pcName);
		Serial.print(" acquire R");
		Serial.println(xResourceIndex + 1);
	}
#endif /* schedSUB_SCHEDULING_POLICY */
}

void vReleaseResource(TaskHandle_t xTaskHandle, BaseType_t xResourceIndex)
{
	BaseType_t xIter, status;
	BaseType_t xCurrentTaskIndex = prvGetTCBIndexFromHandle(xTaskHandle);
	SchedRCB_t *pxRCB = &xRCBArray[xResourceIndex];
	SchedTCB_t *pxTCB = &xTCBArray[xCurrentTaskIndex];

	taskENTER_CRITICAL();

	Serial.print(pxTCB->pcName);
	Serial.print(" release R");
	Serial.println(xResourceIndex + 1);

	status = xSemaphoreGive(pxRCB->xMutexSem);

	if (status == pdTRUE)
	{
		pxRCB->xInUse = pdFALSE;
		pxTCB->xBlocked = pdFALSE;
		pxTCB->xResourceAccessed = pdFALSE;

		vTaskPrioritySet(xTaskHandle, pxTCB->uxBasePriority);
	}

	taskEXIT_CRITICAL();
}

/* This function must be called before any other function call from this module. */
void vSchedulerInit(void)
{
#if (schedUSE_TCB_ARRAY == 1)
	prvInitTCBArray();
#endif /* schedUSE_TCB_ARRAY */
}

/* Starts scheduling tasks. All periodic tasks (including polling server) must
 * have been created with API function before calling this function. */
void vSchedulerStart(void)
{
#if ((schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS) || (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DM))
	prvSetFixedPriorities();
#if ((schedSUB_SCHEDULING_POLICY == schedSUB_SCHEDULING_POLICY_OPCP) || (schedSUB_SCHEDULING_POLICY == schedSUB_SCHEDULING_POLICY_IPCP))
	prvSetPriorityCeilings();
#endif /* schedSUB_SCHEDULING_POLICY */
#elif (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF)
	prvSetEDFInitialPriorities();
#endif /* schedSCHEDULING_POLICY */

#if (schedUSE_SCHEDULER_TASK == 1)
	prvCreateSchedulerTask();
#endif /* schedUSE_SCHEDULER_TASK */

	xSystemStartTime = xTaskGetTickCount();

	prvCreateAllTasks();

	vTaskStartScheduler();
}
