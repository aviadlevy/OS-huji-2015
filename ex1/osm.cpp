#include "osm.h"
#include <sys/time.h>
#include <math.h>
#include <unistd.h>

#define OSM_DEFAULT_ITER 50000  // The default number of iterations to be done
#define LP_UNRLNG 5  // Loop unrooling - number of operations
#define FAILURE -1
#define END_OF_STR '\0'

/* Initialization function that the user must call
 * before running any other library function.
 * Returns 0 uppon success and -1 on failure.
 */
int osm_init()
{

	return 0;
}

/*
 * An empty function for measurements.
 * Will always succeed.
 */
void someFunction()
{

}

/* Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   Zero iterations number is invalid.
   */
double osm_function_time(unsigned int osm_iterations)
{
	if(osm_iterations <= 0)
	{
		osm_iterations = OSM_DEFAULT_ITER;
	}
	timeval before, after, diff;  // Time stamps
	timezone defalutTimeZone;  // TimeZone
	int remaining = osm_iterations % LP_UNRLNG ? 1 : 0;
	if(gettimeofday(&before,&defalutTimeZone) == FAILURE)
	{
		return FAILURE;
	}
	for(unsigned int i = 0; i < osm_iterations / LP_UNRLNG + remaining; ++i)
	{
		someFunction();
		someFunction();
		someFunction();
		someFunction();
		someFunction();
	}
	if(gettimeofday(&after,&defalutTimeZone) == FAILURE)
	{
		return FAILURE;
	}
	timersub(&after,&before,&diff);
	return (double) ((diff.tv_sec * pow(10,9)) + (diff.tv_usec * pow(10,3))) / osm_iterations;
}

/* Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   Zero iterations number is invalid.

   */
double osm_syscall_time(unsigned int osm_iterations)
{
	if(osm_iterations <= 0)
	{
		osm_iterations = OSM_DEFAULT_ITER;
	}
	timeval before, after, diff;  // Time stamps
	timezone defalutTimeZone;  // TimeZone
	int remaining = osm_iterations % LP_UNRLNG ? 1 : 0;
	if (gettimeofday(&before,&defalutTimeZone) == FAILURE)
	{
		return FAILURE;
	}
	for(unsigned int i = 0; i < osm_iterations / LP_UNRLNG + remaining; ++i)
	{
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		//notice failure, but it's always succeed
	}
	if (gettimeofday(&after,&defalutTimeZone) == FAILURE)
	{
		return FAILURE;
	}
	timersub(&after,&before,&diff);
	return (double) ((diff.tv_sec * pow(10,9)) + (diff.tv_usec * pow(10,3))) / osm_iterations;
}

/* Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   Zero iterations number is invalid.
   */
double osm_operation_time(unsigned int osm_iterations)
{
	if(osm_iterations <= 0)
	{
		osm_iterations = OSM_DEFAULT_ITER;
	}
	int temp = 0;
	timeval before, after, diff;  // Time stamps
	timezone defalutTimeZone;  // TimeZone
	int remaining = osm_iterations % LP_UNRLNG ? 1 : 0;
	if (gettimeofday(&before,&defalutTimeZone) == FAILURE)
	{
		return FAILURE;
	}
	for(unsigned int i = 0; i < osm_iterations / LP_UNRLNG + remaining; ++i)
	{
		temp = 1 & 2;
		temp = 1 & 2;
		temp = 1 & 2;
		temp = 1 & 2;
		temp = 1 & 2;
		//notice failure, but it's always succeed
	}
	if (gettimeofday(&after,&defalutTimeZone) == FAILURE)
	{
		return FAILURE;
	}
	timersub(&after,&before,&diff);
	if(temp){}
	return (double) ((diff.tv_sec * pow(10,9)) + (diff.tv_usec * pow(10,3))) / osm_iterations;
}
/*
 * this function calls the others and returns all the results in a struct
 * containing all the data we are interested in, comparing the various times
 */
timeMeasurmentStructure measureTimes(unsigned int osm_iterations)
{
	timeMeasurmentStructure allData;
	if(gethostname(allData.machineName,HOST_NAME_MAX) == FAILURE)
	{
		allData.machineName[0] = END_OF_STR;
	}
	allData.numberOfIterations = (osm_iterations / LP_UNRLNG + (osm_iterations % LP_UNRLNG ? 1 : 0)) * LP_UNRLNG;
	allData.functionTimeNanoSecond = osm_function_time(osm_iterations);
	allData.instructionTimeNanoSecond = osm_operation_time(osm_iterations);
	allData.trapTimeNanoSecond = osm_syscall_time(osm_iterations);
	//check if any error occurred during time measurement, and if so, the ratio is '-1'
	int funcRatio = FAILURE, trapRatio = FAILURE;
	if(allData.instructionTimeNanoSecond != FAILURE)
	{
		if(allData.functionTimeNanoSecond != FAILURE)
		{
			funcRatio = allData.functionTimeNanoSecond / allData.instructionTimeNanoSecond;
		}
		if(allData.trapTimeNanoSecond != FAILURE)
		{
			trapRatio = allData.trapTimeNanoSecond / allData.instructionTimeNanoSecond;
		}
	}
	allData.functionInstructionRatio = funcRatio;
	allData.trapInstructionRatio = trapRatio;
	return allData;
}
