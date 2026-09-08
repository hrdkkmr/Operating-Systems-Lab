#include "types.h"
#include "stat.h"
#include "user.h"

/*
 * These are the semaphore system calls added in the
 * previous synchronization assignment.
 */
int sem_init(int sem_id, int value);
int sem_wait(int sem_id);
int sem_signal(int sem_id);

#define LOCK1 0
#define LOCK2 1

/*
 * Busy-wait delay.
 *
 * We deliberately make this long enough so that the
 * other process gets a chance to acquire its lock.
 */
void delay(void)
{
    volatile int i;

    for (i = 0; i < 50000000; i++)
        ;
}

/*
 * --------------------------------------------------
 * BAD VERSION
 * --------------------------------------------------
 *
 * Process A:
 *      Lock1 -> Lock2
 *
 * Process B:
 *      Lock2 -> Lock1
 *
 * This can produce:
 *
 *      A holds Lock1
 *      B holds Lock2
 *      A waits for Lock2
 *      B waits for Lock1
 *
 *      DEADLOCK
 */

/*
 * Process A in bad version.
 */
void processA_bad(void)
{
    printf(1, "[A] Starting...\n");

    printf(1, "[A] Waiting for Lock1\n");
    sem_wait(LOCK1);

    printf(1, "[A] Acquired Lock1\n");

    /*
     * Give Process B time to acquire Lock2.
     */
    delay();

    printf(1, "[A] Waiting for Lock2\n");
    sem_wait(LOCK2);

    /*
     * This line should NOT be reached
     * when the deadlock occurs.
     */
    printf(1, "[A] Acquired Lock2\n");

    printf(1, "[A] Critical section completed\n");

    sem_signal(LOCK2);
    sem_signal(LOCK1);

    printf(1, "[A] Released Lock2 and Lock1\n");

    exit();
}

/*
 * Process B in bad version.
 */
void processB_bad(void)
{
    printf(1, "[B] Starting...\n");

    printf(1, "[B] Waiting for Lock2\n");
    sem_wait(LOCK2);

    printf(1, "[B] Acquired Lock2\n");

    /*
     * Give Process A time to acquire Lock1.
     */
    delay();

    printf(1, "[B] Waiting for Lock1\n");
    sem_wait(LOCK1);

    /*
     * This line should NOT be reached
     * when the deadlock occurs.
     */
    printf(1, "[B] Acquired Lock1\n");

    printf(1, "[B] Critical section completed\n");

    sem_signal(LOCK1);
    sem_signal(LOCK2);

    printf(1, "[B] Released Lock1 and Lock2\n");

    exit();
}

/*
 * Run the intentionally deadlocking version.
 */
void runBadVersion(void)
{
    int pidA;
    int pidB;

    printf(1, "\n========================================\n");
    printf(1, "       BAD VERSION: DEADLOCK\n");
    printf(1, "========================================\n");

    /*
     * Initialize both locks to 1.
     *
     * 1 means the resource is available.
     */
    sem_init(LOCK1, 1);
    sem_init(LOCK2, 1);

    printf(1, "\nTwo processes will acquire locks in opposite order.\n");
    printf(1, "A: Lock1 -> Lock2\n");
    printf(1, "B: Lock2 -> Lock1\n\n");

    pidA = fork();

    if (pidA == 0) {
        processA_bad();
    }

    pidB = fork();

    if (pidB == 0) {
        processB_bad();
    }

    /*
     * Parent waits for both processes.
     *
     * In the deadlock case, the children never
     * finish, so the parent also remains waiting.
     */
    wait();
    wait();

    printf(1, "\nBad version completed.\n");
}

/*
 * --------------------------------------------------
 * FIXED VERSION
 * --------------------------------------------------
 *
 * Both processes follow the same global ordering:
 *
 *      Lock1 -> Lock2
 *
 * Therefore circular wait cannot occur.
 */

/*
 * Process A in fixed version.
 */
void processA_fixed(void)
{
    printf(1, "[A] Starting...\n");

    printf(1, "[A] Waiting for Lock1\n");
    sem_wait(LOCK1);

    printf(1, "[A] Acquired Lock1\n");

    delay();

    printf(1, "[A] Waiting for Lock2\n");
    sem_wait(LOCK2);

    printf(1, "[A] Acquired Lock2\n");

    printf(1, "[A] Critical section completed\n");

    sem_signal(LOCK2);
    sem_signal(LOCK1);

    printf(1, "[A] Released Lock2 and Lock1\n");

    exit();
}

/*
 * Process B in fixed version.
 *
 * IMPORTANT:
 *
 * B also acquires Lock1 first.
 */
void processB_fixed(void)
{
    printf(1, "[B] Starting...\n");

    printf(1, "[B] Waiting for Lock1\n");
    sem_wait(LOCK1);

    printf(1, "[B] Acquired Lock1\n");

    delay();

    printf(1, "[B] Waiting for Lock2\n");
    sem_wait(LOCK2);

    printf(1, "[B] Acquired Lock2\n");

    printf(1, "[B] Critical section completed\n");

    sem_signal(LOCK2);
    sem_signal(LOCK1);

    printf(1, "[B] Released Lock2 and Lock1\n");

    exit();
}

/*
 * Run the fixed version.
 */
void runFixedVersion(void)
{
    int pidA;
    int pidB;

    printf(1, "\n========================================\n");
    printf(1, "       FIXED VERSION: NO DEADLOCK\n");
    printf(1, "========================================\n");

    sem_init(LOCK1, 1);
    sem_init(LOCK2, 1);

    printf(1, "\nBoth processes follow the same order:\n");
    printf(1, "A: Lock1 -> Lock2\n");
    printf(1, "B: Lock1 -> Lock2\n\n");

    pidA = fork();

    if (pidA == 0) {
        processA_fixed();
    }

    pidB = fork();

    if (pidB == 0) {
        processB_fixed();
    }

    /*
     * Both processes should finish.
     */
    wait();
    wait();

    printf(1, "\nBoth processes completed successfully.\n");
    printf(1, "No deadlock occurred.\n");
}

/*
 * Main
 *
 * Usage:
 *
 *      resourceorder bad
 *      resourceorder fixed
 */
int main(int argc, char *argv[])
{
    if (argc < 2) {

        printf(1, "\nUsage:\n");
        printf(1, "  resourceorder bad\n");
        printf(1, "  resourceorder fixed\n");

        exit();
    }

    /*
     * Run intentionally deadlocking version.
     */
    if (argv[1][0] == 'b') {
        runBadVersion();
    }

    /*
     * Run resource-ordering solution.
     */
    else if (argv[1][0] == 'f') {
        runFixedVersion();
    }

    else {
        printf(1, "Invalid argument.\n");
        printf(1, "Use 'bad' or 'fixed'.\n");
    }

    exit();
}