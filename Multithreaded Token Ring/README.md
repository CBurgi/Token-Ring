Cole Burgi 1174454

# Thread Methodology
The adaptation process from A2 was fairly straightforward. The main adaptations involved creating threads instead of process and converting the shared memory to block memory.

## pthread
Pthread functions were used in place of the process fork, exit, and wait functions. To facilitate this, a new array of `pthread_t`'s were created to keep track of the created threads' IDs. In addition the `token_node` function was converted to a `void *`, and a new struct was created to handle its inputs as is required for teh `pthread_create` function.

## Block memory
When initializing memory, a `shared_data` struct was simply `malloc`'d to `shared_ptr` rather than setting up shared memory at that location. Because of this, the `shmid` variable was no longer used. The semaphores to protect this memory were largely unchanged as protecting this memory is largely the same problem as shared memory. The remaining methodology is left over from A2 and can be read below.

# Semaphore Methodology
The program uses 22 semaphores
- 1 `EMPTY` for each node (7) to state if that node's shared byte is empty
- 1 `FILLED` for each node (7) to state if that node's shared byte is filled
- 1 `TO_SEND` for each node (7) to state if that node is open to receiving a new packet
- 1 `CRIT` for critical sections

The empty and filled semaphores create a producer-consumer pair at each node's shared byte, and since the program expects to consume every byte produced and only the producer (previous node) and consumer (this node) expect to access the shared byte there is no need to create a critical section when accessing the shared byte.

The critical semaphore is used to create a critical section whenever a node (or fork) attempts to access a piece of shared memory that the parent process is also able to access, which happens in `token_node` to get a packet's length and in `send_pkt` to get all of the packet's data.

# Packet sending Methodology
The node opens a critical section to read the length of the packet in its shared memory, as if it is greater than 0 the node knows it has data to send.

The node uses a For loop to send the packet, calling `send_pkt` then `rcv_byte` each iteration, then calls `send_byte` to send the 'available' `TOKEN`. This for loop runs for the length of the data + 4 (for the packet's headers). 

After sending, the node opens a critical section to set the length in its shared memory to 0, then signals its `TO_SEND` to tell the parent process it is open to receiving a new packet.

If the node doesn't have data to send, it simply forwards the `TOKEN` to the next node.

If the `TOKEN` is the 'available' `TOKEN` and the node has been set to terminate (which it checks by opening a critical section and accessing its `terminate` shared variable), the node ends its while loop and exits.

## snd_pkt
This function needs to open a critical section every time it gets a byte from the node's shared packet memory to ensure it doesn't come into conflict with the parent, even though there is a very small window after the parent fills `len` where the node may try to access memory it is filling.

## send_byte
This function needs to access the shared byte of the next node to the one calling it, which it determines by the below calculation. It waits on the `EMPTY` semaphore of the next node before accessing its shared byte, then singals its `FILLED` semaphore.

`next = (num + 1) % N_NODES`

## rvy_byte
This function waits on the `FILLED` semaphore of the node calling it before accessing its shared byte, then signals its `EMPTY` semaphore.