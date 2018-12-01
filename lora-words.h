/*
 *
 * 
 * 
 */
  

#define PACKET_HOST_SIZE        (8)
#define PACKET_PAYLOAD_SIZE     (128)
#define PACKET_SEQ_SIZE         (64)
#define PACKET_DIV_SIZE         (1)

#define PACKET_DIV              ('|')
#define PACKET_DIV_STR          ("|")

#define PACKET_BROADCAST        ("ALL")

#define PACKET_STRING_SIZE      (PACKET_HOST_SIZE + PACKET_DIV_SIZE + \
                                PACKET_HOST_SIZE + PACKET_DIV_SIZE + \
                                PACKET_PAYLOAD_SIZE + PACKET_DIV_SIZE + \
                                PACKET_SEQ_SIZE + PACKET_DIV_SIZE + 1   \
                                + 4 + 1) // Add space for NULLs.

 typedef struct {
     char from[PACKET_HOST_SIZE+1];
     char to[PACKET_HOST_SIZE+1];
     char payload[PACKET_PAYLOAD_SIZE+1];
     char sequence[PACKET_SEQ_SIZE+1];
 } pp_packet;

