#ifndef LIBRARY_H
#define LIBRARY_H
int skip_remaining_tests_requested(void);
void set_skip_remaining_tests(const int val);
void handle_signal(int signum);
#endif //LIBRARY_H
