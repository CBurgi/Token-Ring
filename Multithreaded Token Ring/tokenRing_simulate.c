//Cole Burgi 1174454

/*
 * The program simulates a Token Ring LAN by forking off a process
 * for each LAN node, that communicate via shared memory, instead
 * of network cables. To keep the implementation simple, it jiggles
 * out bytes instead of bits.
 *
 * It keeps a count of packets sent and received for each node.
 */
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "tokenRing.h"

#define TOKEN '1'
#define NO_TOKEN '0'

/*
 * This function is the body of a child process emulating a node.
 */
void *
token_node(void *input)
{
	struct TokenRingData *control = ((struct args*)input)->control;
	int num = ((struct args*)input)->num;

	int rcv_state = TOKEN_FLAG, not_done = 1, sending = 0, len;
	unsigned char byte;
	/*
	 * If this is node #0, start the ball rolling by creating the
	 * token.
	 */
	if(num == 0){
        //put token in its shared memory
        WAIT_SEM(control, EMPTY(0));
        control->shared_ptr->node[num].data_xfer = TOKEN; 
        SIGNAL_SEM(control, FILLED(0));
    }

	/*
	 * Loop around processing data, until done.
	 */
	while (not_done) {

        byte = rcv_byte(control, num);
		/*
		 * Handle the byte, based upon current state.
		 */
		switch (rcv_state) {
		case TOKEN_FLAG:
            WAIT_SEM(control, CRIT);
            //check if this node has a data_pkt to send
            unsigned char send_len = control->shared_ptr->node[num].to_send.length;
            SIGNAL_SEM(control, CRIT);

			if(byte == TOKEN && send_len > 0){
                control->shared_ptr->node[num].sent ++;
                control->snd_state = TOKEN_FLAG;
                
                for(int i=0;i<send_len+4;i++){
                    send_pkt(control, num);
                    rcv_byte(control, num);
                }
                send_byte(control, num, TOKEN);
                
                WAIT_SEM(control, CRIT);
                //say that this node no longer has a data_pkt to send
                control->shared_ptr->node[num].to_send.length = 0;
                SIGNAL_SEM(control, CRIT);
                //ask for a new data_pkt for this node
                SIGNAL_SEM(control, TO_SEND(num));
            }else{
                if(byte != TOKEN){
                    send_byte(control, num, byte);
                    rcv_state ++;
                }else{
                    WAIT_SEM(control, CRIT);
                    send_byte(control, num, byte);
                    if(control->shared_ptr->node[num].terminate == 1){
                        not_done = 0;
                    }
                    SIGNAL_SEM(control, CRIT);
                }
            }
			break;
            
        case TO:
			if(byte == (char)num){
                control->shared_ptr->node[num].received ++;
            }
            
            send_byte(control, num, byte);
            rcv_state ++;
			break;
            
        case FROM:
            send_byte(control, num, byte);
            rcv_state ++;
			break;
            
        case LEN:
			len = byte;
            
            send_byte(control, num, byte);
            rcv_state ++;
			break;
            
        case DATA:
            send_byte(control, num, byte);
            sending ++;
            
            if(sending == len){
                sending = 0;
                rcv_state = TOKEN_FLAG;
            }
			break;
		};
	}
    
    pthread_exit(NULL);
}

/*
 * This function sends a data packet followed by the token, one byte each
 * time it is called.
 */
void
send_pkt(control, num)
	struct TokenRingData *control;
	int num;
{
	static int sndpos, sndlen;
    char byte;

	switch (control->snd_state) {
	case TOKEN_FLAG:
        WAIT_SEM(control, CRIT);
        byte = control->shared_ptr->node[num].to_send.token_flag;
        SIGNAL_SEM(control, CRIT);

        control->snd_state ++;
		break;
        
    case TO:
		WAIT_SEM(control, CRIT);
        byte = control->shared_ptr->node[num].to_send.to;
        SIGNAL_SEM(control, CRIT);

        control->snd_state ++;
		break;
        
    case FROM:
        WAIT_SEM(control, CRIT);
        byte = control->shared_ptr->node[num].to_send.from;
        SIGNAL_SEM(control, CRIT);

        control->snd_state ++;
		break;
        
    case LEN:
        WAIT_SEM(control, CRIT);
        byte = control->shared_ptr->node[num].to_send.length;
        SIGNAL_SEM(control, CRIT);
        sndlen = byte;
        sndpos = 0;

        control->snd_state ++;
		break;
        
    case DATA:
        WAIT_SEM(control, CRIT);
        byte = control->shared_ptr->node[num].to_send.data[sndpos];
        SIGNAL_SEM(control, CRIT);
        
        sndpos ++;
        if(sndpos == sndlen){
            control->snd_state ++;
        }
		break;
        
    case DONE:
		break;
	};

    send_byte(control, num, byte);
}

/*
 * Send a byte to the next node on the ring.
 */
void
send_byte(control, num, byte)
	struct TokenRingData *control;
	int num;
	unsigned byte;
{
    int next = (num+1) % N_NODES;

	WAIT_SEM(control, EMPTY(next));
    control->shared_ptr->node[next].data_xfer = byte;
    SIGNAL_SEM(control, FILLED(next));
}

/*
 * Receive a byte for this node.
 */
unsigned char
rcv_byte(control, num)
	struct TokenRingData *control;
	int num;
{
	unsigned char byte;

	WAIT_SEM(control, FILLED(num));
    byte = control->shared_ptr->node[num].data_xfer;
    SIGNAL_SEM(control, EMPTY(num));
    return byte;
}

