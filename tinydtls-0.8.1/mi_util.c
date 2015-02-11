#include "mi_util.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <error.h>

#define HAS_RESPOND 1
#define NO_RESPOND 0

unsigned int
mi_rand ()
{
  static int fd = 0;
  unsigned int r;

  if (!fd)
    {
      if (-1 == (fd = open ("/dev/urandom", O_RDONLY)))
	{
	  perror ("open");
	  fd = 0;
	  return 0;
	}
    }
  if (-1 == (read (fd, &r, sizeof (r))))
    perror ("read");
  return r;
}

void SingleOrEven_C(void* buf, size_t *len)
{
	unsigned int r;

	*len = sizeof(r);
	r = mi_rand();
	memcpy(buf,&r,*len);

	return;
}

int SingleOrEven_S(void *inbuf, size_t inlen, void* outbuf, size_t *outlen)
{
	unsigned int *r = inbuf;
	printf("Number:\t%u\n", *r);
	
	if(*r % 2)
	{
		printf("Single\n");
		strncpy(outbuf, "Single\n", *outlen);
	}
	else
	{
		printf("Even\n");
		strncpy(outbuf, "Even\n", *outlen);
	}
	*outlen = strlen(outbuf);

	return HAS_RESPOND;
}
