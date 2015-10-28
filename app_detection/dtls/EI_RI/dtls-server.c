#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <string.h>

#include "tinydtls.h"

#ifndef DEBUG
#define DEBUG DEBUG_PRINT
#endif
#include "net/ip/uip-debug.h"
#include "net/ip/udp-socket.h"
#include "net/rpl/rpl.h"

#include "debug.h"
#include "dtls.h"
#include "servreg-hack.h"


#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_UDP_BUF  ((struct uip_udp_hdr *)&uip_buf[UIP_LLIPH_LEN])
#define INVALID_COMMAND "INVALID COMMAND\r\n"
#define MAX_PAYLOAD_LEN 120
#define MAX_BUF 128
#define SPACE 20
#define DTLS_SERVICE_ID 217

static struct udp_socket server_conn;
static dtls_context_t *dtls_context = NULL;

static void
create_rpl_dag(uip_ipaddr_t *ipaddr)
{
#ifdef SERVER_RPL_DAG
  struct uip_ds6_addr *root_if;

  root_if = uip_ds6_addr_lookup(ipaddr);
  if(root_if != NULL) {
    rpl_dag_t *dag;
    uip_ipaddr_t prefix;
    
    rpl_set_root(RPL_DEFAULT_INSTANCE, ipaddr);
    dag = rpl_get_any_dag();
    uip_ip6addr(&prefix, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    rpl_set_prefix(dag, &prefix, 64);
    PRINTF("created a new RPL dag\n");
  } else {
    PRINTF("failed to create a new RPL DAG\n");
  }
#endif
  return;
}

void DtlsServerCb (struct udp_socket *c, void *ptr,
                   const uip_ipaddr_t * source_addr, uint16_t source_port,
                   const uip_ipaddr_t * dest_addr, uint16_t dest_port,
                   const uint8_t * data, uint16_t datalen);

static uip_ipaddr_t *
set_global_address (void)
{
    static uip_ipaddr_t ipaddr;
    int i;
    uint8_t state;

    uip_ip6addr (&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    uip_ds6_set_addr_iid (&ipaddr, &uip_lladdr);
    uip_ds6_addr_add (&ipaddr, 0, ADDR_AUTOCONF);

    printf ("IPv6 addresses: ");
    for (i = 0; i < UIP_DS6_ADDR_NB; i++)
      {
          state = uip_ds6_if.addr_list[i].state;
          if (uip_ds6_if.addr_list[i].isused &&
              (state == ADDR_TENTATIVE || state == ADDR_PREFERRED))
            {
                uip_debug_ipaddr_print (&uip_ds6_if.addr_list[i].ipaddr);
                printf ("\n");
            }
      }

    return &ipaddr;
}

//This function is the "key store" for tinyDTLS. It is called to
//retrieve a key for the given identity within this particular
//session.
int
get_psk_info (struct dtls_context_t *ctx, const session_t * session,
              dtls_credentials_type_t type,
              const unsigned char *id, size_t id_len,
              unsigned char *result, size_t result_length)
{

    struct keymap_t
    {
        unsigned char *id;
        size_t id_length;
        unsigned char *key;
        size_t key_length;
    } psk[3] =
    {
        {
        (unsigned char *) "Client_identity", 15,
                (unsigned char *) "secretPSK", 9},
        {
        (unsigned char *) "default identity", 16,
                (unsigned char *) "\x11\x22\x33", 3},
        {
        (unsigned char *) "\0", 2, (unsigned char *) "", 1}
    };

    if (type != DTLS_PSK_KEY)
      {
          return 0;
      }

    if (id)
      {
          int i;
          for (i = 0; i < sizeof (psk) / sizeof (struct keymap_t); i++)
            {
                if (id_len == psk[i].id_length
                    && memcmp (id, psk[i].id, id_len) == 0)
                  {
                      if (result_length < psk[i].key_length)
                        {
                            dtls_warn ("buffer too small for PSK");
                            return
                                dtls_alert_fatal_create
                                (DTLS_ALERT_INTERNAL_ERROR);
                        }
                      memcpy (result, psk[i].key, psk[i].key_length);
                      return psk[i].key_length;
                  }
            }
      }

    return dtls_alert_fatal_create (DTLS_ALERT_DECRYPT_ERROR);
}


//ReadSensors() generates content in the reply.
int
ReadSensors (uint8 * data, size_t * plen)
{
    int count, value, size;

    for (count = 0, value = 0; count < SPACE; count++)
      {
          value += random_rand () % 100;
      }
    value /= SPACE;
    size = snprintf ((char *) data, *plen, "READ: %d\r\n", value);
    *plen = size;

    return size;
}

//DtlsReadCb() is called by DTLS module to handle the decrypted data.
int
DtlsReadCb (struct dtls_context_t *ctx,
            session_t * session, uint8 * data, size_t len)
{
    uint8 buf[MAX_BUF];
    size_t buflen = MAX_BUF;

    if (!strncasecmp ("GET", (char *) data, 3))
      {
          printf ("Received GET\n");
          memset (buf, 0, MAX_BUF);
          ReadSensors (buf, &buflen);
          printf ("Sending: %s", buf);
          dtls_write (ctx, session, buf, buflen);
      }
    else
      {
          dtls_write (ctx, session, (unsigned char *) INVALID_COMMAND,
                      strlen (INVALID_COMMAND));
      }
    return 0;
}

int
DtlsSendCb (struct dtls_context_t *ctx,
            session_t * session, uint8 * data, size_t len)
{
    struct udp_socket *conn = (struct udp_socket *) dtls_get_app_data (ctx);

    PRINTF ("send to ");
    PRINT6ADDR (&session->addr);
    PRINTF (":%u\n", uip_ntohs (session->port));

    //Sends DTLS payload
    return udp_socket_sendto (conn, data, len, &session->addr, session->port);
}

void
Init ()
{
    static dtls_handler_t cb = {
        .write = DtlsSendCb,
        .read = DtlsReadCb,
        .event = NULL,
#ifdef DTLS_PSK
        .get_psk_info = get_psk_info,
#endif /* DTLS_PSK */
    };

    PRINTF ("DTLS server started\n");
    dtls_context = dtls_new_context (&server_conn);

    //Register a DTLS port.
    udp_socket_register (&server_conn, dtls_context, DtlsServerCb);
    udp_socket_bind (&server_conn, 20220);

    dtls_set_log_level (DTLS_LOG_DEBUG);

    if (dtls_context)
        dtls_set_handler (dtls_context, &cb);
}

//DtlsServerCb() bridges UDP API to DTLS API.
void
DtlsServerCb (struct udp_socket *sock,
              void *apparg,
              const uip_ipaddr_t * remoteaddr,
              uint16_t remoteport,
              const uip_ipaddr_t * localaddr,
              uint16_t localport, const uint8_t * data, uint16_t datalen)
{
    session_t session;
    uint8_t databuf[MAX_BUF] = { 0 };

    PRINTF ("%u bytes received from ", datalen);
    PRINT6ADDR (remoteaddr);
    PRINTF (":%u.\n", remoteport);

    //Record remote endpoint.
    uip_ipaddr_copy (&session.addr, remoteaddr);
    session.port = remoteport;
    session.size = sizeof (session.addr) + sizeof (session.port);
    //Move data around...(FIXME)
    memcpy (databuf, data, datalen);
    printf ("apparg:%d\n", (int) apparg);
    //Pass the received encrypted packet to DTLS module.
    dtls_handle_message ((dtls_context_t *) apparg, &session, databuf,
                         datalen);

    return;
}

/*---------------------------------------------------------------------------*/
PROCESS (udp_server_process, "UDP server process");
AUTOSTART_PROCESSES (&udp_server_process);
PROCESS_THREAD (udp_server_process, ev, data)
{
    static uip_ipaddr_t *ipaddr;

    PROCESS_BEGIN ();
    
    dtls_init ();
    Init ();

    servreg_hack_init ();

    ipaddr = set_global_address ();
    create_rpl_dag(ipaddr);
    servreg_hack_register (DTLS_SERVICE_ID, ipaddr);

    if (!dtls_context)
      {
          dtls_emerg ("cannot create context\n");
          PROCESS_EXIT ();
      }

    while (1)
      {
          PROCESS_YIELD ();
      }

    PROCESS_END ();
}

/*---------------------------------------------------------------------------*/
