#ifndef SENDER_H
#define SENDER_H

/* Initialize ESP-NOW subsystem */
void espnow_init(void);

/* ESP-NOW sender task - pulls from audio_queue and broadcasts */
void espnow_sender_task(void *arg);

#endif /* SENDER_H */
