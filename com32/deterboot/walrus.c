/******************************************************************************
 *
 * Walrus Test Framework
 * Copyright Ryan Goodfellow 2017, all rights reserved
 * Apache 2.0
 *
 *****************************************************************************/

#include <stdlib.h>
#include <hiredis/hiredis.h>
#include "walrus.h"

int __WTFxxx(struct WTFTest *t, const char *level, const char *fmt, ...)
{
  //connect to redis
  
  redisContext *c = redisConnect(t->collector, REDIS_PORT);
  if(c == NULL || c->err)
  {
    printf("redis error: %s\n", c->errstr);
    return FAILURE;
  }

  //prepare elipsis arguments for forwarding

  va_list vl1, vl2;
  va_start(vl1, fmt);
  va_copy(vl2, vl1);
  size_t sz = vsnprintf(NULL, 0, fmt, vl1);
  va_end(vl1);
  char *buf = malloc(sz+1);
  vsnprintf(buf, sz+1, fmt, vl2);
  va_end(vl2);
  buf[sz] = 0;

  //get the time

  redisReply *r = (redisReply*)redisCommand(c, "TIME");
  if(c->err)
  {
    printf("redis error: %s\n", c->errstr);
    return FAILURE;
  }
  if(r->type != REDIS_REPLY_ARRAY)
  {
    printf("redis error: TIME did not return array\n");
    return FAILURE;
  }
  redisReply *seconds = r->element[0];
  redisReply *microseconds= r->element[1];

  //push the diagnostic

  r = (redisReply*)redisCommand(c, "SET %s:%s:%s:%s %s:::%s", 
      t->test, 
      t->participant,
      seconds->str,
      microseconds->str,
      level,
      buf
  );
  if(c->err)
  {
    printf("redis error: %s\n", c->errstr);
    exit(FAILURE);
  }
  if(r->type == REDIS_REPLY_ERROR)
  {
    printf("redis reply error: %s\n", r->str);
    exit(FAILURE);
  }

  free(buf);
  redisFree(c);

  return SUCCESS;
}

