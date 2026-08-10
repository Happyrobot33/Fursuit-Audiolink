#ifndef SENDER_H
#define SENDER_H

void setup_radio(void);

/* ESP-NOW sender task - pulls from audio_queue and broadcasts */
void sender_task(void *arg);

#endif /* SENDER_H */
