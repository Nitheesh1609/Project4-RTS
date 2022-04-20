/*
	ESFree V1.0 - Copyright (C) 2016 Robin Kase
	All rights reserved

	This file is part of ESFree.

	ESFree is free software; you can redistribute it and/or modify it under
	the terms of the GNU General Public licence (version 2) as published by the
	Free Software Foundation AND MODIFIED BY one exception.

	***************************************************************************
	>>!   NOTE: The modification to the GPL is included to allow you to     !<<
	>>!   distribute a combined work that includes ESFree without being     !<<
	>>!   obliged to provide the source code for proprietary components     !<<
	>>!   outside of ESFree.                                                !<<
	***************************************************************************

	ESFree is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE. Full license text can be found on license.txt.

    Modified by Dhwan Wanjara for ECE 5550G Spring 2021
*/

#include "scheduler.h"


#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF  || schedSCHEDULING_POLICY_HVDF) // changed for HVDF
#include "list.h"
#endif /* schedSCHEDULING_POLICY_EDF */

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF  || schedSCHEDULING_POLICY_HVDF) // changed for HVDF
#define schedUSE_TCB_SORTED_LIST 1	
#endif /* schedSCHEDULING_POLICY_EDF */

/* Extended Task control block for managing periodic tasks within this library. */
typedef struct xExtended_TCB
{
	TaskFunction_t pvTaskCode; 		/* Function pointer to the code that will be run periodically. */
	const char *pcName; 			/* Name of the task. */
	UBaseType_t uxStackDepth; 			/* Stack size of the task. */
	void *pvParameters; 			/* Parameters to the task function. */
	UBaseType_t uxPriority; 		/* Priority of the task. */
	TaskHandle_t *pxTaskHandle;		/* Task handle for the task. */
	TickType_t xReleaseTime;		/* Release time of the task. */
	TickType_t xRelativeDeadline;	/* Relative deadline of the task. */
	TickType_t xAbsoluteDeadline;	/* Absolute deadline of the task. */
	TickType_t xPeriod;				/* Task period. */
	TickType_t xLastWakeTime; 		/* Last time stamp when the task was running. */
	TickType_t xMaxExecTime;		/* Worst-case execution time of the task. */
	TickType_t xExecTime;			/* Current execution time of the task. */

	BaseType_t xWorkIsDone; 		/* pdFALSE if the job is not finished, pdTRUE if the job is finished. */

#if( schedUSE_TCB_SORTED_LIST == 1 )
	ListItem_t xTCBListItem; 	/* Used to reference TCB from the TCB list. */
#endif /* schedUSE_TCB_SORTED_LIST */

#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
	BaseType_t xExecutedOnce;	/* pdTRUE if the task has executed once. */
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 || schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
	TickType_t xAbsoluteUnblockTime; /* The task will be unblocked at this time if it is blocked by the scheduler task. */
#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME || schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
	BaseType_t xSuspended; 		/* pdTRUE if the task is suspended. */
	BaseType_t xMaxExecTimeExceeded; /* pdTRUE when execTime exceeds maxExecTime. */
#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */ 

#if (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_HVDF)
	TickType_t uxValue;
	TickType_t uxValueDensity; // what type should this be?
#endif // added for HVDF
} SchedTCB_t;

#if( schedUSE_SPORADIC_JOBS == 1 )
/* Control block for managing sporadic jobs. */
typedef struct xSporadicJobControlBlock
{
	TaskFunction_t pvTaskCode;		/* Function pointer to the code of the sporadic job. */
	const char *pcName;				/* Name of the job. */
	void *pvParameters;				/* Pararmeters to the job function. */
	TickType_t xRelativeDeadline;	/* Relative deadline of the sporadic job. */
	TickType_t xMaxExecTime;		/* Worst-case execution time of the sporadic job. */
	TickType_t xExecTime;			/* Current execution time of the sporadic job. */

#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
	TickType_t xAbsoluteDeadline;	/* Absolute deadline of the sporadic job. */
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
} SJCB_t;
#endif /* schedUSE_SPORADIC_JOBS */

#if( schedUSE_TCB_ARRAY == 1 )
static BaseType_t prvGetTCBIndexFromHandle(TaskHandle_t xTaskHandle);
static void prvInitTCBArray(void);
/* Find index for an empty entry in xTCBArray. Return -1 if there is no empty entry. */
static BaseType_t prvFindEmptyElementIndexTCB(void);
/* Remove a pointer to extended TCB from xTCBArray. */
static void prvDeleteTCBFromArray(BaseType_t xIndex);
#endif /* schedUSE_TCB_ARRAY */

#if( schedUSE_TCB_SORTED_LIST == 1 )
static void prvAddTCBToList(SchedTCB_t *pxTCB);
static void prvDeleteTCBFromList(SchedTCB_t *pxTCB);
#endif /* schedUSE_TCB_LIST */

static TickType_t xSystemStartTime = 0;

static void prvPeriodicTaskCode(void *pvParameters);
static void prvCreateAllTasks(void);

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
static void prvSetFixedPriorities(void);
#endif /* schedSCHEDULING_POLICY_RM/DMS */
#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF || schedSCHEDULING_POLICY_HVDF)

static void prvInitEDF(void);
static void prvUpdatePrioritiesEDF(void);

#if( schedUSE_TCB_SORTED_LIST == 1 )
static void prvSwapList(List_t **ppxList1, List_t **ppxList2);
#endif /* schedUSE_TCB_SORTED_LIST */

#endif /* schedSCHEDULING_POLICY_EDF */

#if( schedUSE_SCHEDULER_TASK == 1 )

	static void prvSchedulerCheckTimingError(TickType_t xTickCount, SchedTCB_t *pxTCB);
	static void prvSchedulerFunction(void);
	static void prvCreateSchedulerTask(void);
	static void prvWakeScheduler(void);

	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
	static void prvPeriodicTaskRecreate(SchedTCB_t *pxTCB);
	static void prvDeadlineMissedHook(SchedTCB_t *pxTCB, TickType_t xTickCount);
	static void prvCheckDeadline(SchedTCB_t *pxTCB, TickType_t xTickCount);
		#if( schedUSE_SPORADIC_JOBS == 1 )
		static void prvDeadlineMissedHookSporadicJob(BaseType_t xIndex);
		static void prvCheckSporadicJobDeadline(TickType_t xTickCount);
		#endif /* schedUSE_SPORADIC_JOBS */
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
	static void prvExecTimeExceedHook(TickType_t xTickCount, SchedTCB_t *pxCurrentTask);
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

#endif /* schedUSE_SCHEDULER_TASK */

#if( schedUSE_SPORADIC_JOBS == 1 )
	static SJCB_t *prvGetNextSporadicJob(void);
	static BaseType_t prvFindEmptyElementIndexSJCB(void);
	static BaseType_t prvAnalyzeSporadicJobSchedulability(SJCB_t *pxSporadicJob, TickType_t xTickCount);
#endif /* schedUSE_SPORADIC_JOBS */


#if( schedUSE_TCB_ARRAY == 1 )
	/* Array for extended TCBs. */
	static SchedTCB_t xTCBArray[schedMAX_NUMBER_OF_PERIODIC_TASKS] = { 0 };
	/* Counter for number of periodic tasks. */
	static BaseType_t xTaskCounter = 0;
#endif /* schedUSE_TCB_ARRAY */

#if( schedUSE_TCB_SORTED_LIST == 1 )

static List_t xTCBList;				/* Sorted linked list for all periodic tasks. */
static List_t xTCBTempList;			/* A temporary list used for switching lists. */
static List_t xTCBOverflowedList; 	/* Sorted linked list for periodic tasks that have overflowed deadline. */
static List_t *pxTCBList = NULL;  			/* Pointer to xTCBList. */
static List_t *pxTCBTempList = NULL;		/* Pointer to xTCBTempList. */
static List_t *pxTCBOverflowedList = NULL;	/* Pointer to xTCBOverflowedList. */

#endif /* schedUSE_TCB_LIST */

#if( schedUSE_SCHEDULER_TASK )
static TickType_t xSchedulerWakeCounter = 0;
static TaskHandle_t xSchedulerHandle = NULL;
#endif /* schedUSE_SCHEDULER_TASK */


#if( schedUSE_SPORADIC_JOBS == 1 )
/* Array for extended SJCBs (Sporadic Job Control Block). */
static SJCB_t xSJCBFifo[schedMAX_NUMBER_OF_SPORADIC_JOBS] = { 0 };
static BaseType_t xSJCBFifoHead = 0;
static BaseType_t xSJCBFifoTail = 0;

static UBaseType_t uxSporadicJobCounter = 0;
static TickType_t xAbsolutePreviousMaxResponseTime = 0;
#endif /* schedUSE_SPORADIC_JOBS */

#if( schedUSE_TCB_ARRAY == 1 )
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

/* Find index for an empty entry in xTCBArray. Returns -1 if there is no empty entry. */
static BaseType_t prvFindEmptyElementIndexTCB(void)
{
	BaseType_t xIndex;
	for (xIndex = 0; xIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; xIndex++)
	{
		if (pdFALSE == xTCBArray[xIndex].xInUse)
		{
			return xIndex;
		}
	}

	return -1;
}

/* Remove a pointer to extended TCB from xTCBArray. */
static void prvDeleteTCBFromArray(BaseType_t xIndex)
{
	configASSERT(xIndex >= 0 && xIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS);
	configASSERT(pdTRUE == xTCBArray[xIndex].xInUse);

	if (xTCBArray[pdTRUE == xIndex].xInUse)
	{
		xTCBArray[xIndex].xInUse = pdFALSE;
		xTaskCounter--;
	}
}
#endif /* schedUSE_TCB_SORTED_ARRAY */

#if( schedUSE_TCB_SORTED_LIST == 1 )
/* Add an extended TCB to sorted linked list. */
static void prvAddTCBToList(SchedTCB_t *pxTCB)
{
	/* Initialise TCB list item. */
	vListInitialiseItem(&pxTCB->xTCBListItem);
	/* Set owner of list item to the TCB. */
	listSET_LIST_ITEM_OWNER(&pxTCB->xTCBListItem, pxTCB);
#if (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF)
	/* List is sorted by absolute deadline value. */
	listSET_LIST_ITEM_VALUE(&pxTCB->xTCBListItem, pxTCB->xAbsoluteDeadline);
#endif
#if (schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_HVDF)
	/* List is sorted by calculated value. */
	listSET_LIST_ITEM_VALUE(&pxTCB->xTCBListItem, pxTCB->uxValue); // if called agian changed to valueDensity
#endif
/* Insert TCB into list. */
	vListInsert(pxTCBList, &pxTCB->xTCBListItem);
}

/* Delete an extended TCB from sorted linked list. */
static void prvDeleteTCBFromList(SchedTCB_t *pxTCB)
{
	uxListRemove(&pxTCB->xTCBListItem); // /* your implementation goes here */); DONE
	vPortFree(pxTCB);
}
#endif /* schedUSE_TCB_SORTED_LIST */


#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF  || schedSCHEDULING_POLICY_HVDF)
#if( schedUSE_TCB_SORTED_LIST == 1 )
/* Swap content of two lists. */
static void prvSwapList(List_t **ppxList1, List_t **ppxList2)
{
	/* your implementation goes here */
	List_t *pxTemp;
	pxTemp = *ppxList1;
	*ppxList1 = *ppxList2;
	*ppxList2 = pxTemp;
	/* Done*/
}
#endif /* schedUSE_TCB_SORTED_LIST */

/* Update priorities of all periodic tasks with respect to EDF policy. */
static void prvUpdatePrioritiesEDF(void)
{
	//Serial.flush();
	//Serial.println("called update prio");
	SchedTCB_t *pxTCB;

#if( schedUSE_TCB_SORTED_LIST == 1 )
	ListItem_t *pxTCBListItem;
	ListItem_t *pxTCBListItemTemp;

	if (listLIST_IS_EMPTY(pxTCBList) && !listLIST_IS_EMPTY(pxTCBOverflowedList))
	{
		prvSwapList(&pxTCBList, &pxTCBOverflowedList);
	}

	const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER(pxTCBList);
	pxTCBListItem = listGET_HEAD_ENTRY(pxTCBList);

	while (pxTCBListItem != pxTCBListEndMarker)
	{
		pxTCB = listGET_LIST_ITEM_OWNER(pxTCBListItem);

		/* Update priority in the SchedTCB list. */
		/* your implementation goes here. */
#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF)
		listSET_LIST_ITEM_VALUE(pxTCBListItem, pxTCB->xAbsoluteDeadline);
		/*Done*/
#endif	

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_HVDF)
		pxTCB->uxValueDensity = (pxTCB->uxValue) / (pxTCB->xMaxExecTime - pxTCB->xExecTime); // update VD
		listSET_LIST_ITEM_VALUE(pxTCBListItem, pxTCB->uxValueDensity);
		/*Done*/
#endif // added for HVDf

		pxTCBListItemTemp = pxTCBListItem;
		pxTCBListItem = listGET_NEXT(pxTCBListItem);
		uxListRemove(pxTCBListItem->pxPrevious);

		/* If absolute deadline overflowed, insert TCB to overflowed list. */
		/* your implementation goes here. */
		/* If absolute deadline overflowed, insert TCB to overflowed list. */
		if (pxTCB->xAbsoluteDeadline < pxTCB->xLastWakeTime)
		{
			vListInsert(pxTCBOverflowedList, pxTCBListItemTemp);
		}
		else /* Else Insert TCB into temp list in usual case. */
		{
			vListInsert(pxTCBTempList, pxTCBListItemTemp);
		}
		/*Done*/
		/* else Insert TCB into temp list in usual case. */
	}

	/* Swap list with temp list. */
	prvSwapList(&pxTCBList, &pxTCBTempList);

#if( schedUSE_SCHEDULER_TASK == 1 )
	BaseType_t xHighestPriority = schedSCHEDULER_PRIORITY - 1;
#else
	BaseType_t xHighestPriority = configMAX_PRIORITIES - 1;
#endif /* schedUSE_SCHEDULER_TASK */

	/* assign priorities to tasks */
	/* your implementation goes here. */
	const ListItem_t *pxTCBListEndMarkerAfterSwap = listGET_END_MARKER(pxTCBList);
	pxTCBListItem = listGET_HEAD_ENTRY(pxTCBList);
	while (pxTCBListItem != pxTCBListEndMarkerAfterSwap)
	{
		pxTCB = listGET_LIST_ITEM_OWNER(pxTCBListItem);
		configASSERT(-1 <= xHighestPriority);
		pxTCB->uxPriority = xHighestPriority;
		vTaskPrioritySet(*pxTCB->pxTaskHandle, pxTCB->uxPriority);
		//Serial.println(pxTCB->uxPriority);
		//Serial.println(pxTCB->pcName);
		//Serial.flush()
		xHighestPriority--;
		pxTCBListItem = listGET_NEXT(pxTCBListItem);
	}
	/*Done*/
#endif /* schedUSE_TCB_SORTED_LIST */
}
#endif /* schedSCHEDULING_POLICY_EDF */


/* The whole function code that is executed by every periodic task.
 * This function wraps the task code specified by the user. */
static void prvPeriodicTaskCode(void *pvParameters)
{
	SchedTCB_t *pxThisTask;
	TaskHandle_t xHandle = xTaskGetCurrentTaskHandle();

	const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER(pxTCBList);
	const ListItem_t *pxTCBListItem = listGET_HEAD_ENTRY(pxTCBList);

	while (pxTCBListItem != pxTCBListEndMarker)
	{
		pxThisTask = listGET_LIST_ITEM_OWNER(pxTCBListItem);
		/* your implementation goes here. */	  //TODO
		if (xHandle == *pxThisTask->pxTaskHandle) {
			break;
		}
		//Serial.flush();
		//Serial.println("handle");
		//Serial.flush();
		/*Done*/
		pxTCBListItem = listGET_NEXT(pxTCBListItem);
	}

	if (0 != pxThisTask->xReleaseTime)
	{
		xTaskDelayUntil(&pxThisTask->xLastWakeTime, pxThisTask->xReleaseTime);
	}
#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
	pxThisTask->xExecutedOnce = pdTRUE;
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */	

	if (0 != pxThisTask->xReleaseTime)
	{
		xTaskDelayUntil(&pxThisTask->xLastWakeTime, pxThisTask->xReleaseTime);
	}
	for (; ; )
	{
#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF   || schedSCHEDULING_POLICY_HVDF) // changed for HVDF
		/* Wake up the scheduler task to update priorities of all periodic tasks. */
	// update valueDensity
		prvWakeScheduler();
#endif /* schedSCHEDULING_POLICY_EDF */

		pxThisTask->xWorkIsDone = pdFALSE;
		//Serial.flush();
		Serial.print("Currently executing ");
		Serial.print(pxThisTask->pcName);
		Serial.print(" - ");
		Serial.print(xTaskGetTickCount());
		Serial.print("\n");
		Serial.flush();

		/* Execute the task function specified by the user. */
		pxThisTask->pvTaskCode(pvParameters);

		pxThisTask->xWorkIsDone = pdTRUE;

		pxThisTask->xExecTime = 0;

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF   || schedSCHEDULING_POLICY_HVDF) // changed for HVDF

		/* your implementation goes here. */
		pxThisTask->xAbsoluteDeadline = pxThisTask->xLastWakeTime + pxThisTask->xPeriod + pxThisTask->xRelativeDeadline;
		/*Done*/

		/* Wake up the scheduler task to update priorities of all periodic tasks. */
		/*Serial.print("Wake");
		Serial.flush();*/
		prvWakeScheduler();

#endif /* schedSCHEDULING_POLICY_EDF */

		// added to test the scheduling algotithms against deadline misses.
		//TickType_t makeDelay = 1000;
		//xTaskDelayUntil(&pxThisTask->xLastWakeTime, pxThisTask->xPeriod + makeDelay);

		xTaskDelayUntil(&pxThisTask->xLastWakeTime, pxThisTask->xPeriod);
	}
}

/* Creates a periodic task. */
void vSchedulerPeriodicTaskCreate(TaskFunction_t pvTaskCode, const char *pcName, UBaseType_t uxStackDepth, void *pvParameters, UBaseType_t uxPriority,
	TaskHandle_t *pxCreatedTask, TickType_t xPhaseTick, TickType_t xPeriodTick, TickType_t xMaxExecTimeTick, TickType_t xDeadlineTick, TickType_t uxValue) // aded for HVDF
{
	taskENTER_CRITICAL();

	SchedTCB_t *pxNewTCB;
	pxNewTCB = pvPortMalloc(sizeof(SchedTCB_t));

	/* Intialize item. */
	pxNewTCB->pvTaskCode = pvTaskCode;
	pxNewTCB->pcName = pcName;
	pxNewTCB->uxStackDepth = uxStackDepth;
	pxNewTCB->pvParameters = pvParameters;
	pxNewTCB->uxPriority = uxPriority;
	pxNewTCB->pxTaskHandle = pxCreatedTask;
	pxNewTCB->xReleaseTime = xPhaseTick;
	pxNewTCB->xPeriod = xPeriodTick;
	pxNewTCB->xMaxExecTime = xMaxExecTimeTick;
	pxNewTCB->xRelativeDeadline = xDeadlineTick;
	pxNewTCB->xWorkIsDone = pdFALSE; // ???
	pxNewTCB->xExecTime = 0;
	/* populate the rest */

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
	pxNewTCB->xPriorityIsSet = pdFALSE;
#endif /* schedSCHEDULING_POLICY */

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF   || schedSCHEDULING_POLICY_HVDF) // changed for HVDF
	pxNewTCB->xAbsoluteDeadline = pxNewTCB->xRelativeDeadline + pxNewTCB->xReleaseTime + xSystemStartTime;
	pxNewTCB->uxPriority = -1;
#endif /* schedSCHEDULING_POLICY */

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_HVDF)
	pxNewTCB->uxValue = uxValue;
	pxNewTCB->uxValueDensity = (pxNewTCB->uxValue) / (pxNewTCB->xMaxExecTime - pxNewTCB->xExecTime); // just WCET not remaining WCET
#endif // added for HVDF

#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
	/* member initialization */
	pxNewTCB->xExecutedOnce = pdFALSE;
	/*Done*/
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
	/* member initialization */
	pxNewTCB->xSuspended = pdFALSE;
	pxNewTCB->xMaxExecTimeExceeded = pdFALSE;
	/*Done*/
#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

#if( schedUSE_TCB_SORTED_LIST == 1 )		
	prvAddTCBToList(pxNewTCB);
#endif /* schedUSE_TCB_SORTED_LIST */

	taskEXIT_CRITICAL();
}

/* Deletes a periodic task. */
void vSchedulerPeriodicTaskDelete(TaskHandle_t xTaskHandle)
{
	if (xTaskHandle != NULL)
	{
#if( schedUSE_TCB_SORTED_LIST == 1 )

		SchedTCB_t *pxThisTask;
		const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER(pxTCBList);
		const ListItem_t *pxTCBListItem = listGET_HEAD_ENTRY(pxTCBList);

		while (pxTCBListItem != pxTCBListEndMarker)
		{
			pxThisTask = listGET_LIST_ITEM_OWNER(pxTCBListItem);
			/* your implementation goes here. */	  //TODO
			Serial.print("Delete");
			Serial.flush();
			if (xTaskHandle == *pxThisTask->pxTaskHandle) {
				break;
			}
			pxTCBListItem = listGET_NEXT(pxTCBListItem);
			// DONE
		}

		prvDeleteTCBFromList(pxThisTask);

#endif /* schedUSE_TCB_ARRAY */
	}

	vTaskDelete(xTaskHandle);
}

/* Creates all periodic tasks stored in TCB array, or TCB list. */
static void prvCreateAllTasks(void)
{
	SchedTCB_t *pxTCB;

#if( schedUSE_TCB_SORTED_LIST == 1 )

	const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER(pxTCBList);
	ListItem_t *pxTCBListItem = listGET_HEAD_ENTRY(pxTCBList);

	while (pxTCBListItem != pxTCBListEndMarker)
	{
		pxTCB = listGET_LIST_ITEM_OWNER(pxTCBListItem);
		configASSERT(NULL != pxTCB);
		BaseType_t xReturnValue = xTaskCreate(prvPeriodicTaskCode, pxTCB->pcName, pxTCB->uxStackDepth, pxTCB->pvParameters, pxTCB->uxPriority, pxTCB->pxTaskHandle); // /* your implementation goes here. */ ); Done
		if (xReturnValue == pdPASS) {
			Serial.print(pxTCB->pcName);
			Serial.print(", Period- ");
			Serial.print(pxTCB->xPeriod);
			Serial.print(", Released at- ");
			Serial.print(pxTCB->xReleaseTime);
			Serial.print(", Priority- ");
			Serial.print(pxTCB->uxPriority);
			Serial.print(", WCET- ");
			Serial.print(pxTCB->xMaxExecTime);
			Serial.print(", Deadline- ");
			Serial.print(pxTCB->xRelativeDeadline);
			Serial.println();
			Serial.flush();
		}
		else
		{
			Serial.println("Task creation failed\n");
			Serial.flush();
		}
		pxTCBListItem = listGET_NEXT(pxTCBListItem);
	}
#endif /* schedUSE_TCB_SORTED_LIST */
}

/* Initiazes fixed priorities of all periodic tasks with respect to RMS or
DMS policy. */
#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS) // changed
static void prvSetFixedPriorities(void)
{
	BaseType_t xIter, xIndex;
	TickType_t xShortest, xPreviousShortest = 0;
	SchedTCB_t *pxShortestTaskPointer, *pxTCB;
#if( schedUSE_SCHEDULER_TASK == 1 )
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
			pxTCB = &xTCBArray[xIndex];
			//Serial.println(pxTCB->pcName);
			if ((pdTRUE == pxTCB->xPriorityIsSet) || (pdFALSE == pxTCB -
		> xInUse))
			{
				//Serial.println(pxTCB->pcName);
				//Serial.println("in use");
				continue;
			}
#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS )
			if (pxTCB->xPeriod <= xShortest)
			{
				//Serial.println("short");
				//Serial.println(pxTCB->pcName);
				xShortest = pxTCB->xPeriod;
				pxShortestTaskPointer = pxTCB;
			}
#elif( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
			if (pxTCB->xRelativeDeadline <= xShortest)
			{
				xShortest = pxTCB->xRelativeDeadline;
				pxShortestTaskPointer = pxTCB;
			}
#endif /* schedSCHEDULING_POLICY */
		}
		/* set highest priority to task with xShortest period (the highest
		priority is configMAX_PRIORITIES-1) */
		/* your implementation goes here */ // done
		configASSERT(-1 <= xHighestPriority);
		if (xPreviousShortest != xShortest)
		{
			xHighestPriority--;
		}
		/* set highest priority to task with xShortest period (the highest
		priority is configMAX_PRIORITIES-1) */
		pxShortestTaskPointer->uxPriority = xHighestPriority;
		pxShortestTaskPointer->xPriorityIsSet = pdTRUE;
		Serial.print("Priority of ");
		Serial.print(pxShortestTaskPointer->pcName);
		Serial.print(" is ");
		Serial.println(pxShortestTaskPointer->uxPriority);
		//Serial.println("Period");
		//Serial.println(pxShortestTaskPointer->xPeriod);
		xPreviousShortest = xShortest;
	}
}
#endif /* schedSCHEDULING_POLICY */

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF   || schedSCHEDULING_POLICY_HVDF) // changed for HVDF
/* Initializes priorities of all periodic tasks with respect to EDF policy. */
static void prvInitEDF(void)
{
	SchedTCB_t *pxTCB;

#if( schedUSE_SCHEDULER_TASK == 1 )
	UBaseType_t uxHighestPriority = schedSCHEDULER_PRIORITY - 1;
#else
	UBaseType_t uxHighestPriority = configMAX_PRIORITIES - 1;
#endif /* schedUSE_SCHEDULER_TASK */

	const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER(pxTCBList);
	ListItem_t *pxTCBListItem = listGET_HEAD_ENTRY(pxTCBList);

	while (pxTCBListItem != pxTCBListEndMarker)
	{
		/* assigning priorities to sorted tasks */
		/* your implementation goes here. */
		pxTCB = listGET_LIST_ITEM_OWNER(pxTCBListItem);
		pxTCB->uxPriority = uxHighestPriority;
		uxHighestPriority--;
		/*Done*/

		pxTCBListItem = listGET_NEXT(pxTCBListItem);
	}

}
#endif /* schedSCHEDULING_POLICY */


#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )

/* Recreates a deleted task that still has its information left in the task array (or list). */
static void prvPeriodicTaskRecreate(SchedTCB_t *pxTCB)
{
	BaseType_t xReturnValue = xTaskCreate(prvPeriodicTaskCode, pxTCB->pcName, pxTCB->uxStackDepth, pxTCB->pvParameters, pxTCB->uxPriority, pxTCB->pxTaskHandle); //DONE /* your implementation goes here. */ );
	if (pdPASS == xReturnValue)
	{
		/* This must be set to false so that the task does not miss the deadline immediately when it is created. */
		pxTCB->xExecutedOnce = pdFALSE;
#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
		/* your implementation goes here. */
		pxTCB->xSuspended = pdFALSE;
		pxTCB->xMaxExecTimeExceeded = pdFALSE;
		/*Done*/
#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
	}
	else
	{
		Serial.println("Task Recreation failed");
		Serial.flush();
		/* if task creation failed */
	}
}

/* Called when a deadline of a periodic task is missed.
 * Deletes the periodic task that has missed it's deadline and recreate it.
 * The periodic task is released during next period. */
static void prvDeadlineMissedHook(SchedTCB_t *pxTCB, TickType_t xTickCount)
{
	/* Delete the pxTask and recreate it. Hint: vTaskDelete()*/

	/* your implementation goes here. */
	Serial.print("Deadline missed - ");
	Serial.print(pxTCB->pcName);
	Serial.print(" - ");
	Serial.println(xTaskGetTickCount());
	Serial.flush();

	/* Delete the pxTask and recreate it. */
	vTaskDelete(*pxTCB->pxTaskHandle);
	pxTCB->xExecTime = 0;
	prvPeriodicTaskRecreate(pxTCB);

	pxTCB->xReleaseTime = pxTCB->xLastWakeTime + pxTCB->xPeriod;
	/* Need to reset lastWakeTime for correct release. */
	pxTCB->xLastWakeTime = 0;
	pxTCB->xAbsoluteDeadline = pxTCB->xRelativeDeadline + pxTCB->xReleaseTime;
	/* Need to reset lastWakeTime for correct release. */
	/* your implementation goes here. */
	/*Done*/
}

/* Checks whether given task has missed deadline or not. */
static void prvCheckDeadline(SchedTCB_t *pxTCB, TickType_t xTickCount)
{
	if ((NULL != pxTCB) && (pdFALSE == pxTCB->xWorkIsDone) && (pdTRUE == pxTCB->xExecutedOnce))
	{
		/* check whether deadline is missed. */
		/* your implementation goes here. */
		/* Using ICTOH method proposed by Carlini and Buttazzo, to check whether deadline is missed. */
		if ((signed)(pxTCB->xAbsoluteDeadline - xTickCount) < 0)
		{
			/* Deadline is missed. */
			prvDeadlineMissedHook(pxTCB, xTickCount);
		}
		/*Done*/
	}
}

#if( schedUSE_SPORADIC_JOBS == 1 )
/* Called when a deadline of a sporadic job is missed. */
static void prvDeadlineMissedHookSporadicJob(BaseType_t xIndex)
{
	printf("\r\ndeadline missed sporadic job! %s\r\n\r\n", xSJCBFifo[xIndex].pcName);
}

/* Checks if any sporadic job has missed it's deadline. */
static void prvCheckSporadicJobDeadline(TickType_t xTickCount)
{
	if (uxSporadicJobCounter > 0)
	{
		BaseType_t xIndex;
		for (xIndex = xSJCBFifoHead - 1; xIndex < uxSporadicJobCounter; xIndex++)
		{
			if (-1 == xIndex)
			{
				xIndex = schedMAX_NUMBER_OF_SPORADIC_JOBS - 1;
			}
			if (schedMAX_NUMBER_OF_SPORADIC_JOBS == xIndex)
			{
				xIndex = 0;
			}

			/* Using ICTOH method proposed by Carlini and Buttazzo, to check whether deadline is missed. */
			if ((signed)(xSJCBFifo[xIndex].xAbsoluteDeadline - xTickCount) < 0)
			{
				prvDeadlineMissedHookSporadicJob(xIndex);
			}
		}
	}
}
#endif /* schedUSE_SPORADIC_JOBS */

#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */


#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )

/* Called if a periodic task has exceeded it's worst-case execution time.
 * The periodic task is blocked until next period. A context switch to
 * the scheduler task occur to block the periodic task. */
static void prvExecTimeExceedHook(TickType_t xTickCount, SchedTCB_t *pxCurrentTask)
{
	/* your implementation goes here. */	 // DOUBLE CHECK
	Serial.print(pxCurrentTask->pcName);
	Serial.print(" Exceeded - ");
	Serial.println(xTaskGetTickCount());
	Serial.flush();
	pxCurrentTask->xMaxExecTimeExceeded = pdTRUE;
	/* Is not suspended yet, but will be suspended by the scheduler later. */
	pxCurrentTask->xSuspended = pdTRUE;
	pxCurrentTask->xAbsoluteUnblockTime = pxCurrentTask->xLastWakeTime + pxCurrentTask->xPeriod;
	pxCurrentTask->xExecTime = 0;
	/*Done*/
	BaseType_t xHigherPriorityTaskWoken;
	vTaskNotifyGiveFromISR(xSchedulerHandle, &xHigherPriorityTaskWoken);
}

#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */


#if( schedUSE_SCHEDULER_TASK == 1 )
/* Called by the scheduler task. Checks all tasks for any enabled
 * Timing Error Detection feature. */
static void prvSchedulerCheckTimingError(TickType_t xTickCount, SchedTCB_t *pxTCB)
{
#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )

	/* Since lastWakeTime is updated to next wake time when the task is delayed, tickCount > lastWakeTime implies that
	 * the task has not finished it's job this period. */

	 /* check if task missed deadline */
	 /* your implementation goes here. */
	if ((signed)(xTickCount - pxTCB->xLastWakeTime) > 0)
	{
		pxTCB->xWorkIsDone = pdFALSE;
	}
	/*Done*/
	prvCheckDeadline(pxTCB, xTickCount);
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */



#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
	/* check if task exceeded WCET */
	/* your implementation goes here. Hint: use vTaskSuspend() */
	if (pdTRUE == pxTCB->xMaxExecTimeExceeded)
	{
		pxTCB->xMaxExecTimeExceeded = pdFALSE;
		Serial.print(pxTCB->pcName);
		Serial.print(" suspended - ");
		Serial.print(xTaskGetTickCount());
		Serial.print("\n");
		Serial.flush();
		vTaskSuspend(*pxTCB->pxTaskHandle);
	}
	if (pdTRUE == pxTCB->xSuspended)
	{
		/* Using ICTOH method proposed by Carlini and Buttazzo, to check whether absolute unblock time is reached. */
		if ((signed)(pxTCB->xAbsoluteUnblockTime - xTickCount) <= 0)
		{
			pxTCB->xSuspended = pdFALSE;
			pxTCB->xLastWakeTime = xTickCount;
			Serial.print(pxTCB->pcName);
			Serial.print(" resumed - ");
			Serial.print(xTaskGetTickCount());
			Serial.print("\n");
			Serial.flush();
			vTaskResume(*pxTCB->pxTaskHandle);
		}
	}
	/*Done*/
#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */


	return;
}

/* Function code for the scheduler task. */
static void prvSchedulerFunction(void)
{
	//Serial.println("Scheduler");
	//vTaskDelay(100); // Added to augment the scheduler overhead
	for (; ; )
	{
		//vTaskDelay(100) // Added to augment the scheduler overhead
#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF   || schedSCHEDULING_POLICY_HVDF) // changed for HVDF
		prvUpdatePrioritiesEDF();
#endif /* schedSCHEDULING_POLICY_EDF */

#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
		TickType_t xTickCount = xTaskGetTickCount();
		SchedTCB_t *pxTCB;

#if( schedUSE_TCB_SORTED_LIST == 1 )
		const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER(pxTCBList);
		ListItem_t *pxTCBListItem = listGET_HEAD_ENTRY(pxTCBList);

		while (pxTCBListItem != pxTCBListEndMarker)
		{
			/* your implementation goes here */
			pxTCB = listGET_LIST_ITEM_OWNER(pxTCBListItem);

			prvSchedulerCheckTimingError(xTickCount, pxTCB);

			pxTCBListItem = listGET_NEXT(pxTCBListItem);
			/*Done*/
		}
#endif /* schedUSE_TCB_SORTED_LIST */

#if( schedUSE_TCB_ARRAY == 1 )
		BaseType_t xIndex;
		for (xIndex = 0; xIndex <
			schedMAX_NUMBER_OF_PERIODIC_TASKS; xIndex++)
		{
			pxTCB = &xTCBArray[xIndex];
			prvSchedulerCheckTimingError(xTickCount, pxTCB
			);
		}
#endif /* schedUSE_TCB_Array */


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


#if( schedUSE_SPORADIC_JOBS == 1 )
/* Returns SJCB of first sporadic job stored in FIFO. Returns NULL if
 * the FIFO is empty. */
static SJCB_t *prvGetNextSporadicJob(void)
{
	/* If FIFO is empty. */
	if (0 == uxSporadicJobCounter)
	{
		return NULL;
	}

	SJCB_t *pxReturnValue = &xSJCBFifo[xSJCBFifoHead];

	/* Move FIFO head to next element in the queue. */
	xSJCBFifoHead++;
	if (schedMAX_NUMBER_OF_SPORADIC_JOBS == xSJCBFifoHead)
	{
		xSJCBFifoHead = 0;
	}

	return pxReturnValue;
}

/* Find index for an empty entry in xSJCBArray. Returns -1 if there is
 * no empty entry. */
static BaseType_t prvFindEmptyElementIndexSJCB(void)
{
	/* If the FIFO is full. */
	if (schedMAX_NUMBER_OF_SPORADIC_JOBS == uxSporadicJobCounter)
	{
		return -1;
	}

	BaseType_t xReturnValue = xSJCBFifoTail;

	/* Extend the FIFO tail. */
	xSJCBFifoTail++;
	if (schedMAX_NUMBER_OF_SPORADIC_JOBS == xSJCBFifoTail)
	{
		xSJCBFifoTail = 0;
	}

	return xReturnValue;
}

/* Called from xSchedulerSporadicJobCreate. Analyzes if the given sporadic
 * job is (guaranteed) schedulable. Return pdTRUE if the sporadic job can
 * meet it's deadline (with guarantee), otherwise pdFALSE. */
static BaseType_t prvAnalyzeSporadicJobSchedulability(SJCB_t *pxSporadicJob, TickType_t xTickCount)
{
	TickType_t xRelativeMaxResponseTime;
	if (xAbsolutePreviousMaxResponseTime > xTickCount)
	{
		/* Using ICTOH method to calculate relative max response time. */
		xRelativeMaxResponseTime = xAbsolutePreviousMaxResponseTime - xTickCount;
	}
	else
	{
		xRelativeMaxResponseTime = 0;
		xAbsolutePreviousMaxResponseTime = 0;
	}

	if (schedPOLLING_SERVER_MAX_EXECUTION_TIME >= pxSporadicJob->xMaxExecTime)
	{
		xRelativeMaxResponseTime += schedPOLLING_SERVER_PERIOD * 2;
	}
	else
	{
		xRelativeMaxResponseTime += schedPOLLING_SERVER_PERIOD + (pxSporadicJob->xMaxExecTime / schedPOLLING_SERVER_MAX_EXECUTION_TIME + 1)
			* schedPOLLING_SERVER_PERIOD;
	}

	if (xRelativeMaxResponseTime < pxSporadicJob->xRelativeDeadline)
	{
		/* Accept job. */
		xAbsolutePreviousMaxResponseTime = xRelativeMaxResponseTime + xTickCount;
		return pdTRUE;
	}
	else
	{
		/* Do not accept job. */
		return pdFALSE;
	}
}

/* Creates a sporadic job if it is schedulable. Returns pdTRUE if the
 * sporadic job can meet it's deadline (with guarantee) and created,
 * otherwise pdFALSE. */
BaseType_t xSchedulerSporadicJobCreate(TaskFunction_t pvTaskCode, const char *pcName, void *pvParameters, TickType_t xMaxExecTimeTick, TickType_t xDeadlineTick)
{
	taskENTER_CRITICAL();
	BaseType_t xAccept = pdFALSE;
	BaseType_t xIndex = prvFindEmptyElementIndexSJCB();
	TickType_t xTickCount = xTaskGetTickCount();
	if (-1 == xIndex)
	{
		/* The SJCBFifo is full. */
		taskEXIT_CRITICAL();
		return xAccept;
	}
	configASSERT(uxSporadicJobCounter < schedMAX_NUMBER_OF_SPORADIC_JOBS);
	SJCB_t *pxNewSJCB = &xSJCBFifo[xIndex];

	/* Add item to SJCBList. */
	*pxNewSJCB = (SJCB_t) {
		.pvTaskCode = pvTaskCode, .pcName = pcName, .pvParameters = pvParameters, .xRelativeDeadline = xDeadlineTick,
			.xMaxExecTime = xMaxExecTimeTick, .xExecTime = 0
	};

#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
	pxNewSJCB->xAbsoluteDeadline = pxNewSJCB->xRelativeDeadline + xTickCount;
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

	xAccept = prvAnalyzeSporadicJobSchedulability(pxNewSJCB, xTickCount);
	if (pdTRUE == xAccept)
	{
		uxSporadicJobCounter++;
	}
	else
	{
		if (xSJCBFifoTail == 0)
		{
			xSJCBFifoTail = schedMAX_NUMBER_OF_SPORADIC_JOBS - 1;
		}
		else
		{
			xSJCBFifoTail--;
		}
	}
	taskEXIT_CRITICAL();
	return xAccept;
}
#endif /* schedUSE_SPORADIC_JOBS */

#if( schedUSE_SCHEDULER_TASK == 1 )

/* Wakes up (context switches to) the scheduler task. */
static void prvWakeScheduler(void)
{
	BaseType_t xHigherPriorityTaskWoken;
	vTaskNotifyGiveFromISR(xSchedulerHandle, &xHigherPriorityTaskWoken);
}

/* Called every software tick. */
void vApplicationTickHook(void)
{
	SchedTCB_t *pxCurrentTask;
	TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();

	const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER(pxTCBList);
	const ListItem_t *pxTCBListItem = listGET_HEAD_ENTRY(pxTCBList);

	while (pxTCBListItem != pxTCBListEndMarker)
	{
		/* your implementation goes here */ // TODO???
		// Check id list is empty
		// This maybe casuing the code to run the same task again and again // NO
		//pxCurrentTask = listGET_LIST_ITEM_OWNER(pxTCBListItem);
		///* your implementation goes here. */	  //TODO
		////Serial.print("Delete");
		////Serial.flush();
		//if (xCurrentTaskHandle == *pxCurrentTask->pxTaskHandle) {
		//	break;
		//}
		//pxTCBListItem = listGET_NEXT(pxTCBListItem);
		//// DONE
	}



	if (NULL != pxCurrentTask && xCurrentTaskHandle != xSchedulerHandle && xCurrentTaskHandle != xTaskGetIdleTaskHandle())
	{
		pxCurrentTask->xExecTime++;
#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
		/* your implementation goes here */
		if (pxCurrentTask->xMaxExecTime <= pxCurrentTask->xExecTime)
		{
			if (pdFALSE == pxCurrentTask->xMaxExecTimeExceeded)
			{
				if (pdFALSE == pxCurrentTask->xSuspended)
				{
					prvExecTimeExceedHook(xTaskGetTickCountFromISR(), pxCurrentTask);
				}
			}
		}
		/*Done*/
#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
	}

#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
	xSchedulerWakeCounter++;
	if (xSchedulerWakeCounter == schedSCHEDULER_TASK_PERIOD)
	{
		xSchedulerWakeCounter = 0;
		prvWakeScheduler();
	}
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
}
#endif /* schedUSE_SCHEDULER_TASK */

/* This function must be called before any other function call from this module. */
void vSchedulerInit(void)
{
#if( schedUSE_TCB_ARRAY == 1 )
	prvInitTCBArray();
#elif( schedUSE_TCB_SORTED_LIST == 1 )
	vListInitialise(&xTCBList);
	vListInitialise(&xTCBTempList);
	vListInitialise(&xTCBOverflowedList);
	pxTCBList = &xTCBList;
	pxTCBTempList = &xTCBTempList;
	pxTCBOverflowedList = &xTCBOverflowedList;
#endif 
}

/* Starts scheduling tasks. All periodic tasks (including polling server) must
 * have been created with API function before calling this function. */
void vSchedulerStart(void)
{
	Serial.println("Scheduler Started");
	Serial.flush();
#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
	prvSetFixedPriorities();
#elif( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )
	prvInitEDF();
#endif /* schedSCHEDULING_POLICY */

#if( schedUSE_SCHEDULER_TASK == 1 )
	prvCreateSchedulerTask();
#endif /* schedUSE_SCHEDULER_TASK */

	prvCreateAllTasks();

	xSystemStartTime = xTaskGetTickCount();
	vTaskStartScheduler();
}
