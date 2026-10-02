#ifndef TIMEOUT_H_
#define TIMEOUT_H_

/**
 * @brief Default inactivity timeout before returning to idle.
 */
#define BACK_IDLE_TMO SECS(30)

/**
 * @brief Standard timeout callback that returns the state machine to
 * STATE_IDLE.
 *
 * @param arg Unused.
 */
void back2idle(void* arg);

#endif