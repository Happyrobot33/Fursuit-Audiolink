#ifndef RECEIVER_H
#define RECEIVER_H

/* UART Serial Receive Task - reads COBS-encoded data from UART */
void serial_rx_task(void *arg);

#endif /* RECEIVER_H */
