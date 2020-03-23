/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *    Ian Craggs - MQTT 3.1.1 support
 *    Ian Craggs - test8 - failure callbacks
 *    Ian Craggs - MQTT V5
 *******************************************************************************/


/**
 * @file
 * Tests for the Paho Asynchronous MQTT C client
 */


#include "MQTTAsync.h"
#include <string.h>
#include <stdlib.h>

#if !defined(_WINDOWS)
	#include <sys/time.h>
  #include <sys/socket.h>
	#include <unistd.h>
  #include <errno.h>
#else
	#include <windows.h>
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

void usage(void)
{
	printf("help!!\n");
	exit(EXIT_FAILURE);
}

struct Options
{
	char* connection;         /**< connection to system under test. */
	int verbose;
	int test_no;
	int size;									/**< size of big message */
	int MQTTVersion;
	int iterations;
} options =
{
	"localhost:1883",
	0,
	-1,
	10000,
	MQTTVERSION_5,
	1,
};

void getopts(int argc, char** argv)
{
	int count = 1;

	while (count < argc)
	{
		if (strcmp(argv[count], "--test_no") == 0)
		{
			if (++count < argc)
				options.test_no = atoi(argv[count]);
			else
				usage();
		}
		else if (strcmp(argv[count], "--size") == 0)
		{
			if (++count < argc)
				options.size = atoi(argv[count]);
			else
				usage();
		}
		else if (strcmp(argv[count], "--connection") == 0)
		{
			if (++count < argc)
				options.connection = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--MQTTversion") == 0)
		{
			if (++count < argc)
			{
				options.MQTTVersion = atoi(argv[count]);
				printf("setting MQTT version to %d\n", options.MQTTVersion);
			}
			else
				usage();
		}
		else if (strcmp(argv[count], "--iterations") == 0)
		{
			if (++count < argc)
				options.iterations = atoi(argv[count]);
			else
				usage();
		}
		else if (strcmp(argv[count], "--verbose") == 0)
			options.verbose = 1;
		count++;
	}
}

#if 0
#include <logaX.h>   /* For general log messages                      */
#define MyLog logaLine
#else
#define LOGA_DEBUG 0
#define LOGA_INFO 1
#include <stdarg.h>
#include <time.h>
#include <sys/timeb.h>
void MyLog(int LOGA_level, char* format, ...)
{
	static char msg_buf[256];
	va_list args;
	struct timeb ts;

	struct tm *timeinfo;

	if (LOGA_level == LOGA_DEBUG && options.verbose == 0)
	  return;

	ftime(&ts);
	timeinfo = localtime(&ts.time);
	strftime(msg_buf, 80, "%Y%m%d %H%M%S", timeinfo);

	sprintf(&msg_buf[strlen(msg_buf)], ".%.3hu ", ts.millitm);

	va_start(args, format);
	vsnprintf(&msg_buf[strlen(msg_buf)], sizeof(msg_buf) - strlen(msg_buf), format, args);
	va_end(args);

	printf("%s\n", msg_buf);
	fflush(stdout);
}
#endif


#if defined(WIN32) || defined(_WINDOWS)
#define mqsleep(A) Sleep(1000*A)
#define START_TIME_TYPE DWORD
static DWORD start_time = 0;
START_TIME_TYPE start_clock(void)
{
	return GetTickCount();
}
#elif defined(AIX)
#define mqsleep sleep
#define START_TIME_TYPE struct timespec
START_TIME_TYPE start_clock(void)
{
	static struct timespec start;
	clock_gettime(CLOCK_REALTIME, &start);
	return start;
}
#else
#define mqsleep sleep
#define START_TIME_TYPE struct timeval
/* TODO - unused - remove? static struct timeval start_time; */
START_TIME_TYPE start_clock(void)
{
	struct timeval start_time;
	gettimeofday(&start_time, NULL);
	return start_time;
}
#endif


#if defined(WIN32)
long elapsed(START_TIME_TYPE start_time)
{
	return GetTickCount() - start_time;
}
#elif defined(AIX)
#define assert(a)
long elapsed(struct timespec start)
{
	struct timespec now, res;

	clock_gettime(CLOCK_REALTIME, &now);
	ntimersub(now, start, res);
	return (res.tv_sec)*1000L + (res.tv_nsec)/1000000L;
}
#else
long elapsed(START_TIME_TYPE start_time)
{
	struct timeval now, res;

	gettimeofday(&now, NULL);
	timersub(&now, &start_time, &res);
	return (res.tv_sec)*1000 + (res.tv_usec)/1000;
}
#endif

#define assert(a, b, c, d) myassert(__FILE__, __LINE__, a, b, c, d)
#define assert1(a, b, c, d, e) myassert(__FILE__, __LINE__, a, b, c, d, e)

int tests = 0;
int failures = 0;
FILE* xml;
START_TIME_TYPE global_start_time;
char output[3000];
char* cur_output = output;

void write_test_result(void)
{
	long duration = elapsed(global_start_time);

	fprintf(xml, " time=\"%ld.%.3ld\" >\n", duration / 1000, duration % 1000);
	if (cur_output != output)
	{
		fprintf(xml, "%s", output);
		cur_output = output;
	}
	fprintf(xml, "</testcase>\n");
}

void myassert(char* filename, int lineno, char* description, int value, char* format, ...)
{
	++tests;
	if (!value)
	{
		va_list args;

		++failures;
		printf("Assertion failed, file %s, line %d, description: %s\n", filename, lineno, description);

		va_start(args, format);
		vprintf(format, args);
		va_end(args);

		cur_output += sprintf(cur_output, "<failure type=\"%s\">file %s, line %d </failure>\n",
                        description, filename, lineno);
	}
    else
    	MyLog(LOGA_DEBUG, "Assertion succeeded, file %s, line %d, description: %s", filename, lineno, description);
}


void logProperties(MQTTProperties *props)
{
	int i = 0;

	for (i = 0; i < props->count; ++i)
	{
		int id = props->array[i].identifier;
		const char* name = MQTTPropertyName(id);
		char* intformat = "Property name %s value %d";

		switch (MQTTProperty_getType(id))
		{
		case PROPERTY_TYPE_BYTE:
		  MyLog(LOGA_INFO, intformat, name, props->array[i].value.byte);
		  break;
		case TWO_BYTE_INTEGER:
		  MyLog(LOGA_INFO, intformat, name, props->array[i].value.integer2);
		  break;
		case FOUR_BYTE_INTEGER:
		  MyLog(LOGA_INFO, intformat, name, props->array[i].value.integer4);
		  break;
		case VARIABLE_BYTE_INTEGER:
		  MyLog(LOGA_INFO, intformat, name, props->array[i].value.integer4);
		  break;
		case BINARY_DATA:
		case UTF_8_ENCODED_STRING:
		  MyLog(LOGA_INFO, "Property name %s value len %.*s", name,
				  props->array[i].value.data.len, props->array[i].value.data.data);
		  break;
		case UTF_8_STRING_PAIR:
		  MyLog(LOGA_INFO, "Property name %s key %.*s value %.*s", name,
			  props->array[i].value.data.len, props->array[i].value.data.data,
		  	  props->array[i].value.value.len, props->array[i].value.value.data);
		  break;
		}
	}
}


struct
{
	int disconnected;
	char* test_topic;
	int test_finished;
} test_client_topic_aliases_globals =
{
	0, "client topic aliases topic", 0
};


void test1_onDisconnect(void* context, MQTTAsync_successData* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MyLog(LOGA_DEBUG, "In onDisconnect callback %p", c);
	test_client_topic_aliases_globals.test_finished = 1;
}


int test_client_topic_aliases_messageArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message)
{
	MQTTAsync c = (MQTTAsync)context;
	static int received = 0;
	int rc;

	received++;
	assert("Message structure version should be 1", message->struct_version == 1,
				"message->struct_version was %d", message->struct_version);
	if (message->struct_version == 1)
	{
		const int props_count = 0;

		assert("No topic alias", MQTTProperties_hasProperty(&message->properties, TOPIC_ALIAS) == 0,
				"topic alias is %d\n", MQTTProperties_hasProperty(&message->properties, TOPIC_ALIAS));

		assert("Topic should be name", strcmp(topicName, test_client_topic_aliases_globals.test_topic) == 0,
					"Topic name was %s\n", topicName);

		logProperties(&message->properties);
	}

	if (received == 2)
		test_client_topic_aliases_globals.test_finished = 1;

	MQTTAsync_freeMessage(&message);
	MQTTAsync_free(topicName);

	return 1;
}


void test_client_topic_aliases_onSubscribe(void* context, MQTTAsync_successData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTProperty property;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int rc;

	MyLog(LOGA_INFO, "Suback properties:");
	logProperties(&response->properties);

	pubmsg.payload = "a much longer message that we can shorten to the extent that we need to payload up to 11";
	pubmsg.payloadlen = 11;
	pubmsg.retained = 0;
	pubmsg.qos = 1;

	/* first set the topic alias */
	property.identifier = TOPIC_ALIAS;
	property.value.integer2 = 1;
	MQTTProperties_add(&pubmsg.properties, &property);

	rc = MQTTAsync_sendMessage(c, test_client_topic_aliases_globals.test_topic, &pubmsg, &opts);
	assert("Good rc from send", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != MQTTASYNC_SUCCESS)
		test_client_topic_aliases_globals.test_finished = 1;
	else
	{
		/* now send the publish with topic alias only */
		rc = MQTTAsync_sendMessage(c, "", &pubmsg, &opts);
		assert("Good rc from send", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
		if (rc != MQTTASYNC_SUCCESS)
			test_client_topic_aliases_globals.test_finished = 1;
	}
	MQTTProperties_free(&pubmsg.properties);
}


void test_client_topic_aliases_disconnected(void* context, MQTTProperties* props, enum MQTTReasonCodes rc)
{
	MQTTAsync c = (MQTTAsync)context;
	MyLog(LOGA_INFO, "Callback: disconnected, reason code \"%s\"", MQTTReasonCodeString(rc));
	logProperties(props);
	test_client_topic_aliases_globals.disconnected = 1;
}


void test_client_topic_aliases_onConnect(void* context, MQTTAsync_successData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTProperty property;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int rc;
	static int first = 1;

	MyLog(LOGA_DEBUG, "In connect onSuccess callback, context %p", context);

	assert("Reason code should be 0", response->reasonCode == SUCCESS,
		   "Reason code was %d\n", response->reasonCode);

	MyLog(LOGA_INFO, "Connack properties:");
	logProperties(&response->properties);

	if (first)
	{
		first = 0;
		pubmsg.payload = "a much longer message that we can shorten to the extent that we need to payload up to 11";
		pubmsg.payloadlen = 11;
		pubmsg.qos = 1;
		pubmsg.retained = 0;

		/* a Topic Alias of 0 is not allowed, so we should be disconnected */
		property.identifier = TOPIC_ALIAS;
		property.value.integer2 = 0;
		MQTTProperties_add(&pubmsg.properties, &property);

		rc = MQTTAsync_sendMessage(c, test_client_topic_aliases_globals.test_topic, &pubmsg, &opts);
		assert("Good rc from connect", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
		if (rc != MQTTASYNC_SUCCESS)
			test_client_topic_aliases_globals.test_finished = 1;
		MQTTProperties_free(&pubmsg.properties);
	}
	else
	{
		opts.onSuccess5 = test_client_topic_aliases_onSubscribe;
		opts.context = c;
		rc = MQTTAsync_subscribe(c, test_client_topic_aliases_globals.test_topic, 2, &opts);
		assert("Good rc from subscribe", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
		if (rc != MQTTASYNC_SUCCESS)
			test_client_topic_aliases_globals.test_finished = 1;

	}
}


/*********************************************************************

Test1: client topic aliases

*********************************************************************/
int test_client_topic_aliases(struct Options options)
{
	int subsqos = 2;
	MQTTAsync c;
	MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer5;
	MQTTAsync_willOptions wopts = MQTTAsync_willOptions_initializer;
	MQTTProperties props = MQTTProperties_initializer;
	MQTTProperties willProps = MQTTProperties_initializer;
	MQTTProperty property;
	int rc = 0;
	char* test_topic = "V5 C client test client topic aliases";

	MyLog(LOGA_INFO, "Starting V5 test 1 - client topic aliases");
	fprintf(xml, "<testcase classname=\"test11\" name=\"client topic aliases\"");
	global_start_time = start_clock();

	rc = MQTTAsync_create(&c, options.connection, "async_test",
			MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
	assert("good rc from create",  rc == MQTTASYNC_SUCCESS, "rc was %d\n", rc);
	if (rc != MQTTASYNC_SUCCESS)
	{
		MQTTAsync_destroy(&c);
		goto exit;
	}

	rc = MQTTAsync_setCallbacks(c, c, NULL, test_client_topic_aliases_messageArrived, NULL);
	assert("Good rc from setCallbacks", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);

	rc = MQTTAsync_setDisconnected(c, c, test_client_topic_aliases_disconnected);

	opts.keepAliveInterval = 20;
	opts.username = "testuser";
	opts.password = "testpassword";
	opts.MQTTVersion = options.MQTTVersion;
	opts.cleanstart = 1;

	opts.will = &wopts;
	opts.will->message = "will message";
	opts.will->qos = 1;
	opts.will->retained = 0;
	opts.will->topicName = "will topic";
	opts.will = NULL;
	opts.onSuccess5 = test_client_topic_aliases_onConnect;
	opts.onFailure5 = NULL;
	opts.context = c;

	property.identifier = SESSION_EXPIRY_INTERVAL;
	property.value.integer4 = 30;
	MQTTProperties_add(&props, &property);

	property.identifier = USER_PROPERTY;
	property.value.data.data = "test user property";
	property.value.data.len = strlen(property.value.data.data);
	property.value.value.data = "test user property value";
	property.value.value.len = strlen(property.value.value.data);
	MQTTProperties_add(&props, &property);

	opts.connectProperties = &props;
	opts.willProperties = &willProps;

	MyLog(LOGA_DEBUG, "Connecting");
	rc = MQTTAsync_connect(c, &opts);
	assert("Good rc from connect", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != MQTTASYNC_SUCCESS)
		goto exit;

	while (test_client_topic_aliases_globals.disconnected == 0)
		#if defined(WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

	/* Now try a valid topic alias */
	rc = MQTTAsync_connect(c, &opts);
	assert("Good rc from connect", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != MQTTASYNC_SUCCESS)
		goto exit;

	while (test_client_topic_aliases_globals.test_finished == 0)
		#if defined(WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

	MQTTProperties_free(&props);
	MQTTProperties_free(&willProps);
	MQTTAsync_destroy(&c);

exit:
	MyLog(LOGA_INFO, "TEST1: test %s. %d tests run, %d failures.",
			(failures == 0) ? "passed" : "failed", tests, failures);
	write_test_result();
	return failures;
}


struct
{
	char* test_topic;
	int test_finished;
	int messages_arrived;
	int msg_count;
} test_server_topic_aliases_globals =
{
	"server topic aliases topic", 0, 0, 3
};


int test_server_topic_aliases_messageArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message)
{
	MQTTAsync c = (MQTTAsync)context;
	static int received = 0;
	static int first_topic_alias = 0;
	int topicAlias = 0;
	int rc;

	received++;
	assert("Message structure version should be 1", message->struct_version == 1,
				"message->struct_version was %d", message->struct_version);
	if (message->struct_version == 1)
	{
		const int props_count = 0;

		if (MQTTProperties_hasProperty(&message->properties, TOPIC_ALIAS))
			topicAlias = MQTTProperties_getNumericValue(&message->properties, TOPIC_ALIAS);

		if (received == 1)
		{
			first_topic_alias = topicAlias;
			assert("Topic should be name", strcmp(topicName, test_server_topic_aliases_globals.test_topic) == 0,
					"Topic name was %s\n", topicName);
		}
		else
		{
			assert("All topic aliases should be the same", topicAlias == first_topic_alias,
					"Topic alias was %d\n", topicAlias);
			assert("Topic should be 0 length", strcmp(topicName, "") == 0,
					"Topic name was %s\n", topicName);
		}

		assert("topicAlias should not be 0", topicAlias > 0, "Topic alias was %d\n", topicAlias);
		logProperties(&message->properties);
	}
	test_server_topic_aliases_globals.messages_arrived++;

	if (test_server_topic_aliases_globals.messages_arrived == test_server_topic_aliases_globals.msg_count)
		test_server_topic_aliases_globals.test_finished = 1;

	MQTTAsync_freeMessage(&message);
	MQTTAsync_free(topicName);

	return 1;
}


void test_server_topic_aliases_onSubscribe(void* context, MQTTAsync_successData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTProperty property;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int qos = 0, rc;

	MyLog(LOGA_INFO, "Suback properties:");
	logProperties(&response->properties);

	test_server_topic_aliases_globals.messages_arrived = 0;
	pubmsg.payload = "a much longer message that we can shorten to the extent that we need to payload up to 11";
	pubmsg.payloadlen = 11;
	pubmsg.retained = 0;
	for (qos = 0; qos < test_server_topic_aliases_globals.msg_count; ++qos)
	{
		pubmsg.qos = qos;
		rc = MQTTAsync_sendMessage(c, test_server_topic_aliases_globals.test_topic, &pubmsg, &opts);
		assert("Good rc from send", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
		if (rc != MQTTASYNC_SUCCESS)
		{
			test_server_topic_aliases_globals.test_finished = 1;
			break;
		}
	}
}


void test_server_topic_aliases_onConnect(void* context, MQTTAsync_successData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTProperty property;
	int rc;

	MyLog(LOGA_DEBUG, "In connect onSuccess callback, context %p", context);

	assert("Reason code should be 0", response->reasonCode == SUCCESS,
		   "Reason code was %d\n", response->reasonCode);

	MyLog(LOGA_INFO, "Connack properties:");
	logProperties(&response->properties);

	opts.onSuccess5 = test_server_topic_aliases_onSubscribe;
	opts.context = c;
	rc = MQTTAsync_subscribe(c, test_server_topic_aliases_globals.test_topic, 2, &opts);
	assert("Good rc from subscribe", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != MQTTASYNC_SUCCESS)
		test_server_topic_aliases_globals.test_finished = 1;
}


/*********************************************************************

Test2: server topic aliases

*********************************************************************/
int test_server_topic_aliases(struct Options options)
{
	int subsqos = 2;
	MQTTAsync c;
	MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer5;
	MQTTAsync_willOptions wopts = MQTTAsync_willOptions_initializer;
	MQTTProperties connect_props = MQTTProperties_initializer;
	MQTTProperty property;
	int rc = 0;
	char* test_topic = "V5 C client test server topic aliases";

	MyLog(LOGA_INFO, "Starting V5 test 2 - server topic aliases");
	fprintf(xml, "<testcase classname=\"test11\" name=\"server topic aliases\"");
	global_start_time = start_clock();

	rc = MQTTAsync_create(&c, options.connection, "server_topic_aliases",
			MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
	assert("good rc from create",  rc == MQTTASYNC_SUCCESS, "rc was %d\n", rc);
	if (rc != MQTTASYNC_SUCCESS)
	{
		MQTTAsync_destroy(&c);
		goto exit;
	}

	rc = MQTTAsync_setCallbacks(c, c, NULL, test_server_topic_aliases_messageArrived, NULL);
	assert("Good rc from setCallbacks", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);

	/* Allow at least one server topic alias */
	property.identifier = TOPIC_ALIAS_MAXIMUM;
	property.value.integer2 = 1;
	MQTTProperties_add(&connect_props, &property);

	opts.MQTTVersion = options.MQTTVersion;
	opts.onSuccess5 = test_server_topic_aliases_onConnect;
	opts.context = c;
	opts.connectProperties = &connect_props;

	MyLog(LOGA_DEBUG, "Connecting");
	rc = MQTTAsync_connect(c, &opts);
	assert("Good rc from connect", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != MQTTASYNC_SUCCESS)
		goto exit;

	while (test_server_topic_aliases_globals.test_finished == 0)
		#if defined(WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

	MQTTProperties_free(&connect_props);
	MQTTAsync_destroy(&c);

exit:
	MyLog(LOGA_INFO, "TEST2: test %s. %d tests run, %d failures.",
			(failures == 0) ? "passed" : "failed", tests, failures);
	write_test_result();
	return failures;
}



/*********************************************************************

Test3: subscription ids

*********************************************************************/

struct
{
	char* test_topic;
	int test_finished;
	int messages_arrived;
} test_subscription_ids_globals =
{
	"server subscription ids topic", 0, 0
};


int test_subscription_ids_messageArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message)
{
	MQTTAsync c = (MQTTAsync)context;
	int rc;

	test_subscription_ids_globals.messages_arrived++;

	assert("Message structure version should be 1", message->struct_version == 1,
				"message->struct_version was %d", message->struct_version);
	MyLog(LOGA_DEBUG, "Callback: message received on topic %s is %.*s.",
				 topicName, message->payloadlen, (char*)(message->payload));

	if (message->struct_version == 1)
	{
		int subsidcount = 0, i = 0;

		subsidcount = MQTTProperties_propertyCount(&message->properties, SUBSCRIPTION_IDENTIFIER);

		for (i = 0; i < subsidcount; ++i)
		{
			int subsid = MQTTProperties_getNumericValueAt(&message->properties, SUBSCRIPTION_IDENTIFIER, i);
			assert("Subsid is i+1", subsid == i+1, "subsid is not correct %d\n", subsid);
		}
		logProperties(&message->properties);
	}

	test_subscription_ids_globals.test_finished = 1;

	MQTTAsync_freeMessage(&message);
	MQTTAsync_free(topicName);

	return 1;
}


void test_subscription_ids_onSubscribe(void* context, MQTTAsync_successData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTProperty property;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int rc;
	static int subs_count = 0;

	MyLog(LOGA_INFO, "Suback properties:");
	logProperties(&response->properties);

	if (++subs_count == 1)
	{
		/* subscribe to a wildcard */
		property.identifier = SUBSCRIPTION_IDENTIFIER;
		property.value.integer4 = 2;
		MQTTProperties_add(&opts.properties, &property);

		opts.onSuccess5 = test_subscription_ids_onSubscribe;
		opts.context = c;

		rc = MQTTAsync_subscribe(c, "+", 2, &opts);
		assert("Good rc from subscribe", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
		if (rc != MQTTASYNC_SUCCESS)
			test_subscription_ids_globals.test_finished = 1;

		MQTTProperties_free(&opts.properties);
	}
	else
	{
		test_subscription_ids_globals.messages_arrived = 0;
		pubmsg.payload = "a much longer message that we can shorten to the extent that we need to payload up to 11";
		pubmsg.payloadlen = 11;
		pubmsg.retained = 0;
		pubmsg.qos = 1;
		rc = MQTTAsync_sendMessage(c, test_subscription_ids_globals.test_topic, &pubmsg, &opts);
		assert("Good rc from send", rc == MQTTASYNC_SUCCESS, "rc was %d\n", rc);
		if (rc != MQTTASYNC_SUCCESS)
			test_subscription_ids_globals.test_finished = 1;
	}
}


void test_subscription_ids_onConnect(void* context, MQTTAsync_successData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTProperty property;
	int rc;

	MyLog(LOGA_DEBUG, "In connect onSuccess callback, context %p", context);

	assert("Reason code should be 0", response->reasonCode == SUCCESS,
		   "Reason code was %d\n", response->reasonCode);

	MyLog(LOGA_INFO, "Connack properties:");
	logProperties(&response->properties);

	opts.onSuccess5 = test_subscription_ids_onSubscribe;
	opts.context = c;
	/* subscribe to the test topic */
	property.identifier = SUBSCRIPTION_IDENTIFIER;
	property.value.integer4 = 1;
	MQTTProperties_add(&opts.properties, &property);

	rc = MQTTAsync_subscribe(c, test_subscription_ids_globals.test_topic, 2, &opts);
	assert("Good rc from subscribe", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != MQTTASYNC_SUCCESS)
		test_subscription_ids_globals.test_finished = 1;

	MQTTProperties_free(&opts.properties);
}


int test_subscription_ids(struct Options options)
{
	MQTTAsync c;
	MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer5;
	int rc = 0;

	MyLog(LOGA_INFO, "Starting V5 test 3 - subscription ids");
	fprintf(xml, "<testcase classname=\"test11\" name=\"subscription ids\"");
	global_start_time = start_clock();

	rc = MQTTAsync_create(&c, options.connection, "subscription_ids",
			MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
	assert("good rc from create",  rc == MQTTASYNC_SUCCESS, "rc was %d\n", rc);
	if (rc != MQTTASYNC_SUCCESS)
	{
		MQTTAsync_destroy(&c);
		goto exit;
	}

	rc = MQTTAsync_setCallbacks(c, c, NULL, test_subscription_ids_messageArrived, NULL);
	assert("Good rc from setCallbacks", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);

	opts.MQTTVersion = options.MQTTVersion;
	opts.onSuccess5 = test_subscription_ids_onConnect;
	opts.context = c;
	opts.cleanstart = 1;

	MyLog(LOGA_DEBUG, "Connecting");
	rc = MQTTAsync_connect(c, &opts);
	assert("Good rc from connect", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != MQTTASYNC_SUCCESS)
		goto exit;

	while (test_subscription_ids_globals.test_finished == 0)
		#if defined(WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

	MQTTAsync_destroy(&c);

exit:
	MyLog(LOGA_INFO, "TEST3: test %s. %d tests run, %d failures.",
			(failures == 0) ? "passed" : "failed", tests, failures);
	write_test_result();
	return failures;
}

/*********************************************************************

Test: flow control

*********************************************************************/

struct
{
	char * test_topic;
	int test_finished;
	int messages_arrived;
	int receive_maximum;
	int blocking_found;
} test_flow_control_globals =
{
	"flow control topic", 0, 0, 65535, 0
};


int test_flow_control_messageArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message)
{
	MQTTAsync c = (MQTTAsync)context;

	test_flow_control_globals.messages_arrived++;

	assert("Message structure version should be 1", message->struct_version == 1,
				"message->struct_version was %d", message->struct_version);
	MyLog(LOGA_DEBUG, "Callback: message received on topic %s is %.*s.",
				 topicName, message->payloadlen, (char*)(message->payload));

	if (message->struct_version == 1)
		logProperties(&message->properties);

	if (test_flow_control_globals.messages_arrived == test_flow_control_globals.receive_maximum + 2)
		test_flow_control_globals.test_finished = 1;

	MQTTAsync_freeMessage(&message);
	MQTTAsync_free(topicName);

	return 1;
}


void test_flow_control_onSubscribe(void* context, MQTTAsync_successData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int rc;
	int i = 0;

	MyLog(LOGA_INFO, "Suback properties:");
	logProperties(&response->properties);

	test_flow_control_globals.messages_arrived = 0;
	pubmsg.payload = "a much longer message that we can shorten to the extent that we need to payload up to 11";
	pubmsg.payloadlen = 11;
	pubmsg.retained = 0;
	pubmsg.qos = 2;

	for (i = 0; i < test_flow_control_globals.receive_maximum + 2; ++i)
	{
		rc = MQTTAsync_sendMessage(c, test_flow_control_globals.test_topic, &pubmsg, &opts);
		assert("Good rc from send", rc == MQTTASYNC_SUCCESS, "rc was %d\n", rc);
		if (rc != MQTTASYNC_SUCCESS)
			test_flow_control_globals.test_finished = 1;
	}
}


void test_flow_control_onConnect(void* context, MQTTAsync_successData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	int rc;

	MyLog(LOGA_DEBUG, "In connect onSuccess callback, context %p", context);

	assert("Reason code should be 0", response->reasonCode == SUCCESS,
		   "Reason code was %d\n", response->reasonCode);

	MyLog(LOGA_INFO, "Connack properties:");
	logProperties(&response->properties);

	if (MQTTProperties_hasProperty(&response->properties, RECEIVE_MAXIMUM))
		test_flow_control_globals.receive_maximum = MQTTProperties_getNumericValue(&response->properties, RECEIVE_MAXIMUM);

	opts.onSuccess5 = test_flow_control_onSubscribe;
	opts.context = c;

	rc = MQTTAsync_subscribe(c, test_flow_control_globals.test_topic, 2, &opts);
	assert("Good rc from subscribe", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != MQTTASYNC_SUCCESS)
		test_flow_control_globals.test_finished = 1;

	MQTTProperties_free(&opts.properties);
}


void flow_control_trace_callback(enum MQTTASYNC_TRACE_LEVELS level, char* message)
{
	static char* msg = "Blocking on server receive maximum";

	if (strstr(message, msg) != NULL)
		test_flow_control_globals.blocking_found = 1;
}


int test_flow_control(struct Options options)
{
	MQTTAsync c;
	MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer5;
	int rc = 0;

	MyLog(LOGA_INFO, "Starting V5 test - flow control");
	fprintf(xml, "<testcase classname=\"test11\" name=\"flow control\"");
	global_start_time = start_clock();

	rc = MQTTAsync_create(&c, options.connection, "flow_control",
			MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
	assert("good rc from create",  rc == MQTTASYNC_SUCCESS, "rc was %d\n", rc);
	if (rc != MQTTASYNC_SUCCESS)
	{
		MQTTAsync_destroy(&c);
		goto exit;
	}

	MQTTAsync_setTraceCallback(flow_control_trace_callback);
	MQTTAsync_setTraceLevel(MQTTASYNC_TRACE_MINIMUM);

	rc = MQTTAsync_setCallbacks(c, c, NULL, test_flow_control_messageArrived, NULL);
	assert("Good rc from setCallbacks", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);

	opts.MQTTVersion = options.MQTTVersion;
	opts.onSuccess5 = test_flow_control_onConnect;
	opts.context = c;
	opts.cleanstart = 1;

	MyLog(LOGA_DEBUG, "Connecting");
	rc = MQTTAsync_connect(c, &opts);
	assert("Good rc from connect", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != MQTTASYNC_SUCCESS)
		goto exit;

	while (test_flow_control_globals.test_finished == 0)
		#if defined(WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

	assert("should have blocked", test_flow_control_globals.blocking_found == 1, "was %d\n",
			test_flow_control_globals.blocking_found);

	MQTTAsync_destroy(&c);

exit:
	MyLog(LOGA_INFO, "TEST4: test %s. %d tests run, %d failures.",
			(failures == 0) ? "passed" : "failed", tests, failures);
	write_test_result();
	return failures;
}

struct
{
	char* test_topic;
	int test_finished;
	int messages_arrived;
} test_error_reporting_globals =
{
	"error reporting topic", 0, 0
};


void test_error_reporting_onUnsubscribe(void* context, MQTTAsync_successData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	int rc;
	int i = 0;

	MyLog(LOGA_INFO, "Unsuback properties:");
	logProperties(&response->properties);

	assert("Reason code count should be 2", response->alt.unsub.reasonCodeCount == 2,
		   "Reason code count was %d\n", response->alt.unsub.reasonCodeCount);

	if (response->alt.unsub.reasonCodeCount == 1)
		MyLog(LOGA_INFO, "reason code %d", response->reasonCode);
	else if (response->alt.unsub.reasonCodeCount > 1)
	{
		for (i = 0; i < response->alt.unsub.reasonCodeCount; ++i)
		{
			MyLog(LOGA_INFO, "Unsubscribe reason code %d", response->alt.unsub.reasonCodes[i]);
		}
	}

	test_error_reporting_globals.test_finished = 1;
}


void test_error_reporting_onSubscribe(void* context, MQTTAsync_successData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTProperty property;
	char* topics[2] = {test_error_reporting_globals.test_topic, "+"};
	int rc;
	int i = 0;

	MyLog(LOGA_INFO, "Suback properties:");
	logProperties(&response->properties);

	assert("Reason code count should be 2", response->alt.sub.reasonCodeCount == 2,
		   "Reason code count was %d\n", response->alt.sub.reasonCodeCount);

	if (response->alt.sub.reasonCodeCount == 1)
		MyLog(LOGA_INFO, "reason code %d", response->reasonCode);
	else if (response->alt.sub.reasonCodeCount > 1)
	{
		for (i = 0; i < response->alt.sub.reasonCodeCount; ++i)
		{
			MyLog(LOGA_INFO, "Subscribe reason code %d", response->alt.sub.reasonCodes[i]);
		}
	}

	opts.onSuccess5 = test_error_reporting_onUnsubscribe;
	opts.context = c;

	property.identifier = USER_PROPERTY;
	property.value.data.data = "test user property";
	property.value.data.len = strlen(property.value.data.data);
	property.value.value.data = "test user property value";
	property.value.value.len = strlen(property.value.value.data);
	MQTTProperties_add(&opts.properties, &property);

	rc = MQTTAsync_unsubscribeMany(c, 2, topics, &opts);
	assert("Good rc from unsubscribe", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != MQTTASYNC_SUCCESS)
		test_flow_control_globals.test_finished = 1;

	MQTTProperties_free(&opts.properties);
}

void test_error_reporting_onConnect(void* context, MQTTAsync_successData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	int rc;
	char* topics[2] = {test_error_reporting_globals.test_topic, "+"};
	int qoss[2] = {2, 2};
	MQTTSubscribe_options subopts[2] = {MQTTSubscribe_options_initializer, MQTTSubscribe_options_initializer};
	MQTTProperty property;

	MyLog(LOGA_DEBUG, "In connect onSuccess callback, context %p", context);

	assert("Reason code should be 0", response->reasonCode == SUCCESS,
		   "Reason code was %d\n", response->reasonCode);

	MyLog(LOGA_INFO, "Connack properties:");
	logProperties(&response->properties);

	opts.onSuccess5 = test_error_reporting_onSubscribe;
	opts.context = c;

	property.identifier = USER_PROPERTY;
	property.value.data.data = "test user property";
	property.value.data.len = strlen(property.value.data.data);
	property.value.value.data = "test user property value";
	property.value.value.len = strlen(property.value.value.data);
	MQTTProperties_add(&opts.properties, &property);

	opts.subscribe_options_count = 2;
	opts.subscribe_options_list = subopts;

	rc = MQTTAsync_subscribeMany(c, 2, topics, qoss, &opts);
	assert("Good rc from subscribe", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != MQTTASYNC_SUCCESS)
		test_flow_control_globals.test_finished = 1;

	MQTTProperties_free(&opts.properties);
}


int test_error_reporting(struct Options options)
{
	MQTTAsync c;
	MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer5;
	int rc = 0;

	MyLog(LOGA_INFO, "Starting V5 test - error reporting");
	fprintf(xml, "<testcase classname=\"test11\" name=\"error reporting\"");
	global_start_time = start_clock();

	rc = MQTTAsync_create(&c, options.connection, "error reporting",
			MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
	assert("good rc from create",  rc == MQTTASYNC_SUCCESS, "rc was %d\n", rc);
	if (rc != MQTTASYNC_SUCCESS)
	{
		MQTTAsync_destroy(&c);
		goto exit;
	}

	rc = MQTTAsync_setCallbacks(c, c, NULL, test_flow_control_messageArrived, NULL);
	assert("Good rc from setCallbacks", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);

	opts.MQTTVersion = options.MQTTVersion;
	opts.onSuccess5 = test_error_reporting_onConnect;
	opts.context = c;
	opts.cleanstart = 1;

	MyLog(LOGA_DEBUG, "Connecting");
	rc = MQTTAsync_connect(c, &opts);
	assert("Good rc from connect", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != MQTTASYNC_SUCCESS)
		goto exit;

	while (test_error_reporting_globals.test_finished == 0)
		#if defined(WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

	MQTTAsync_destroy(&c);

exit:
	MyLog(LOGA_INFO, "TEST5: test %s. %d tests run, %d failures.",
			(failures == 0) ? "passed" : "failed", tests, failures);
	write_test_result();
	return failures;
}


struct
{
	int test_finished;
	char* test_topic;
} test_qos_1_2_errors_globals =
{
	0, "test_qos_1_2_errors"
};


void test_qos_1_2_errors_onPublishSuccess(void* context, MQTTAsync_successData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MyLog(LOGA_INFO, "Callback: publish success, reason code \"%s\" msgid: %d packet type: ",
			MQTTReasonCodeString(response->reasonCode), response->token);

	logProperties(&response->properties);

	test_qos_1_2_errors_globals.test_finished = 1;
}


void test_qos_1_2_errors_onPublishFailure3(void* context, MQTTAsync_failureData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTProperty property;
	int rc;

	MyLog(LOGA_INFO, "Callback: publish failure, reason code \"%s\" msgid: %d packet type: ",
			MQTTReasonCodeString(response->reasonCode), response->token, response->packet_type);

	logProperties(&response->properties);

	test_qos_1_2_errors_globals.test_finished = 1;
}


void test_qos_1_2_errors_onPublishFailure2(void* context, MQTTAsync_failureData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	MQTTProperty property;
	int rc;

	MyLog(LOGA_INFO, "Callback: publish failure, reason code \"%s\" msgid: %d packet type: ",
			MQTTReasonCodeString(response->reasonCode), response->token, response->packet_type);

	logProperties(&response->properties);

	opts.onSuccess5 = test_qos_1_2_errors_onPublishSuccess;
	opts.onFailure5 = test_qos_1_2_errors_onPublishFailure3;
	opts.context = c;

	pubmsg.payload = "a much longer message that we can shorten to the extent that we need to payload up to 11";
	pubmsg.payloadlen = 11;
	pubmsg.qos = 2;
	pubmsg.retained = 0;

	property.identifier = USER_PROPERTY;
	property.value.data.data = "pub user property";
	property.value.data.len = strlen(property.value.data.data);
	property.value.value.data = "pub user property value";
	property.value.value.len = strlen(property.value.value.data);
	MQTTProperties_add(&pubmsg.properties, &property);

	rc = MQTTAsync_sendMessage(c, "test_qos_1_2_errors_pubcomp", &pubmsg, &opts);
	assert("Good rc from publish", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != SUCCESS)
		test_qos_1_2_errors_globals.test_finished = 1;

	MQTTProperties_free(&pubmsg.properties);
}


void test_qos_1_2_errors_onPublishFailure(void* context, MQTTAsync_failureData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	MQTTProperty property;
	int rc;

	MyLog(LOGA_INFO, "Callback: publish failure, reason code \"%s\" msgid: %d packet type: ",
			MQTTReasonCodeString(response->reasonCode), response->token, response->packet_type);

	logProperties(&response->properties);

	opts.onSuccess5 = test_qos_1_2_errors_onPublishSuccess;
	opts.onFailure5 = test_qos_1_2_errors_onPublishFailure2;
	opts.context = c;

	pubmsg.payload = "a much longer message that we can shorten to the extent that we need to payload up to 11";
	pubmsg.payloadlen = 11;
	pubmsg.qos = 2;
	pubmsg.retained = 0;

	property.identifier = USER_PROPERTY;
	property.value.data.data = "pub user property";
	property.value.data.len = strlen(property.value.data.data);
	property.value.value.data = "pub user property value";
	property.value.value.len = strlen(property.value.value.data);
	MQTTProperties_add(&pubmsg.properties, &property);

	rc = MQTTAsync_sendMessage(c, test_qos_1_2_errors_globals.test_topic, &pubmsg, &opts);
	assert("Good rc from publish", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != SUCCESS)
		test_qos_1_2_errors_globals.test_finished = 1;

	MQTTProperties_free(&pubmsg.properties);
}


void test_qos_1_2_errors_onConnect(void* context, MQTTAsync_successData5* response)
{
	MQTTAsync c = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTProperty property;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int rc;

	MyLog(LOGA_DEBUG, "In connect onSuccess callback, context %p", context);

	assert("Reason code should be 0", response->reasonCode == SUCCESS,
		   "Reason code was %d\n", response->reasonCode);

	MyLog(LOGA_INFO, "Connack properties:");
	logProperties(&response->properties);

	opts.onSuccess5 = test_qos_1_2_errors_onPublishSuccess;
	opts.onFailure5 = test_qos_1_2_errors_onPublishFailure;
	opts.context = c;

	pubmsg.payload = "a much longer message that we can shorten to the extent that we need to payload up to 11";
	pubmsg.payloadlen = 11;
	pubmsg.qos = 1;
	pubmsg.retained = 0;

	property.identifier = USER_PROPERTY;
	property.value.data.data = "pub user property";
	property.value.data.len = strlen(property.value.data.data);
	property.value.value.data = "pub user property value";
	property.value.value.len = strlen(property.value.value.data);
	MQTTProperties_add(&pubmsg.properties, &property);

	rc = MQTTAsync_sendMessage(c, test_qos_1_2_errors_globals.test_topic, &pubmsg, &opts);
	assert("Good rc from publish", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != SUCCESS)
		test_qos_1_2_errors_globals.test_finished = 1;

	MQTTProperties_free(&pubmsg.properties);
}


int test_qos_1_2_errors(struct Options options)
{
	MQTTAsync c;
	MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer5;
	int rc = 0;

	MyLog(LOGA_INFO, "Starting V5 test - qos 1 and 2 errors");
	fprintf(xml, "<testcase classname=\"test11\" name=\"qos 1 and 2 errors\"");
	global_start_time = start_clock();

	rc = MQTTAsync_create(&c, options.connection, "qos 1 and 2 errors",
			MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
	assert("good rc from create",  rc == MQTTASYNC_SUCCESS, "rc was %d\n", rc);
	if (rc != MQTTASYNC_SUCCESS)
	{
		MQTTAsync_destroy(&c);
		goto exit;
	}

	rc = MQTTAsync_setCallbacks(c, c, NULL, test_flow_control_messageArrived, NULL);
	assert("Good rc from setCallbacks", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);

	opts.MQTTVersion = options.MQTTVersion;
	opts.onSuccess5 = test_qos_1_2_errors_onConnect;
	opts.context = c;
	opts.cleanstart = 1;

	MyLog(LOGA_DEBUG, "Connecting");
	rc = MQTTAsync_connect(c, &opts);
	assert("Good rc from connect", rc == MQTTASYNC_SUCCESS, "rc was %d", rc);
	if (rc != MQTTASYNC_SUCCESS)
		goto exit;

	while (test_qos_1_2_errors_globals.test_finished == 0)
		#if defined(WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

	MQTTAsync_destroy(&c);

exit:
	MyLog(LOGA_INFO, "TEST6: test %s. %d tests run, %d failures.",
			(failures == 0) ? "passed" : "failed", tests, failures);
	write_test_result();
	return failures;
}


void trace_callback(enum MQTTASYNC_TRACE_LEVELS level, char* message)
{
	printf("Trace : %d, %s\n", level, message);
}


int main(int argc, char** argv)
{
	int rc = -1;
 	int (*tests[])() = {NULL,
 		test_client_topic_aliases,
		test_server_topic_aliases,
		test_subscription_ids,
		test_flow_control,
		test_error_reporting,
		test_qos_1_2_errors,
 	}; /* indexed starting from 1 */
	MQTTAsync_nameValue* info;
	int i;

	xml = fopen("TEST-test4.xml", "w");
	fprintf(xml, "<testsuite name=\"test4\" tests=\"%d\">\n", (int)(ARRAY_SIZE(tests)) - 1);

	getopts(argc, argv);

	MQTTAsync_setTraceCallback(trace_callback);

	info = MQTTAsync_getVersionInfo();
	while (info->name)
	{
	  MyLog(LOGA_INFO, "%s: %s", info->name, info->value);
	  info++;
	}

	for (i = 0; i < options.iterations; ++i)
	{
	 	if (options.test_no == -1)
		{ /* run all the tests */
			for (options.test_no = 1; options.test_no < ARRAY_SIZE(tests); ++options.test_no)
			{
				failures = rc = 0;
				MQTTAsync_setTraceLevel(MQTTASYNC_TRACE_ERROR);
				rc += tests[options.test_no](options); /* return number of failures.  0 = test succeeded */
			}
		}
		else
		{
			if (options.test_no >= ARRAY_SIZE(tests))
				MyLog(LOGA_INFO, "No test number %d", options.test_no);
			else
			{
				MQTTAsync_setTraceLevel(MQTTASYNC_TRACE_ERROR);
				rc = tests[options.test_no](options); /* run just the selected test */
			}
		}
	}

	if (rc == 0)
		MyLog(LOGA_INFO, "verdict pass");
	else
		MyLog(LOGA_INFO, "verdict fail");

	fprintf(xml, "</testsuite>\n");
	fclose(xml);

	return rc;
}
