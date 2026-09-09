/* reader.h — Text reader state machine */
#ifndef READER_H
#define READER_H

void reader_init(void);
void reader_handle_key(char key);
void reader_render(void);

#endif /* READER_H */
