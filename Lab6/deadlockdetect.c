#include "types.h"
#include "stat.h"
#include "user.h"

#define N 4
#define M 3

int allocation[N][M];
int request[N][M];
int waitFor[N][N];

/*
 * Build the wait-for graph.
 *
 * If Pi requests resource Rj, and Rj is currently
 * allocated to Pk, then:
 *
 *              Pi -> Pk
 *
 * This means Pi is waiting for Pk.
 */
void buildWaitForGraph(void)
{
    int i, j, k;

    /* Clear graph */
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            waitFor[i][j] = 0;
        }
    }

    /*
     * Check every process and every resource.
     */
    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {

            /* Pi is requesting resource Rj */
            if (request[i][j] > 0) {

                /*
                 * Find which process currently holds Rj.
                 */
                for (k = 0; k < N; k++) {

                    if (allocation[k][j] > 0) {

                        /*
                         * Pi waits for Pk.
                         */
                        if (i != k)
                            waitFor[i][k] = 1;
                    }
                }
            }
        }
    }
}

/*
 * Print the allocation matrix.
 */
void printAllocation(void)
{
    int i, j;

    printf(1, "\nAllocation Matrix:\n");
    printf(1, "       R0 R1 R2\n");

    for (i = 0; i < N; i++) {
        printf(1, "P%d     ", i);

        for (j = 0; j < M; j++) {
            printf(1, "%d  ", allocation[i][j]);
        }

        printf(1, "\n");
    }
}

/*
 * Print the request matrix.
 */
void printRequest(void)
{
    int i, j;

    printf(1, "\nRequest Matrix:\n");
    printf(1, "       R0 R1 R2\n");

    for (i = 0; i < N; i++) {
        printf(1, "P%d     ", i);

        for (j = 0; j < M; j++) {
            printf(1, "%d  ", request[i][j]);
        }

        printf(1, "\n");
    }
}

/*
 * Print the wait-for graph.
 */
void printWaitForGraph(void)
{
    int i, j;
    int edgeFound;

    printf(1, "\nWait-for Graph:\n");

    edgeFound = 0;

    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {

            if (waitFor[i][j]) {
                printf(1, "P%d -> P%d\n", i, j);
                edgeFound = 1;
            }
        }
    }

    if (!edgeFound) {
        printf(1, "No edges\n");
    }
}

/*
 * DFS for detecting a cycle.
 *
 * state:
 * 0 = not visited
 * 1 = currently in DFS path
 * 2 = completely processed
 */
int state[N];

/*
 * Stores the current DFS path.
 * Used to print the exact cycle.
 */
int path[N];
int pathIndex;

/*
 * Start and end positions of detected cycle.
 */
int cycleStart;
int cycleEnd;

/*
 * DFS cycle detection.
 */
int dfs(int node)
{
    int i;

    state[node] = 1;

    path[pathIndex] = node;
    pathIndex++;

    for (i = 0; i < N; i++) {

        if (waitFor[node][i]) {

            /*
             * If i is currently in the DFS path,
             * we found a cycle.
             */
            if (state[i] == 1) {

                cycleStart = i;
                cycleEnd = pathIndex - 1;

                return 1;
            }

            /*
             * Visit an unvisited process.
             */
            if (state[i] == 0) {

                if (dfs(i))
                    return 1;
            }
        }
    }

    /*
     * Remove node from current DFS path.
     */
    pathIndex--;
    state[node] = 2;

    return 0;
}

/*
 * Detect and print a cycle.
 */
int detectDeadlock(void)
{
    int i;
    int j;
    int found;

    for (i = 0; i < N; i++) {
        state[i] = 0;
    }

    pathIndex = 0;
    cycleStart = -1;
    cycleEnd = -1;

    found = 0;

    /*
     * Run DFS from every process.
     */
    for (i = 0; i < N; i++) {

        if (state[i] == 0) {

            if (dfs(i)) {
                found = 1;
                break;
            }
        }
    }

    if (found) {

        printf(1, "\nDEADLOCK DETECTED!\n");
        printf(1, "Cycle: ");

        /*
         * Print the cycle starting from cycleStart.
         */
        for (j = cycleStart; j <= cycleEnd; j++) {
            printf(1, "P%d -> ", path[j]);
        }

        printf(1, "P%d\n", cycleStart);

        return 1;
    }

    printf(1, "\nNo deadlock detected.\n");

    return 0;
}

/*
 * Scenario 1:
 * No deadlock.
 */
void scenarioNoDeadlock(void)
{
    int i, j;

    int alloc[N][M] = {
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, 1},
        {1, 0, 0}
    };

    int req[N][M] = {
        {0, 1, 0},
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 1}
    };

    /*
     * Copy scenario into global matrices.
     */
    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            allocation[i][j] = alloc[i][j];
            request[i][j] = req[i][j];
        }
    }

    printf(1, "\n========================================\n");
    printf(1, "       SCENARIO 1: NO DEADLOCK\n");
    printf(1, "========================================\n");

    printAllocation();
    printRequest();

    buildWaitForGraph();
    printWaitForGraph();

    detectDeadlock();
}

/*
 * Scenario 2:
 * Three-process circular wait.
 */
void scenarioDeadlock(void)
{
    int i, j;

    int alloc[N][M] = {
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, 1},
        {1, 0, 0}
    };

    int req[N][M] = {
        {0, 1, 0},
        {0, 0, 1},
        {1, 0, 0},
        {0, 0, 0}
    };

    /*
     * Copy scenario into global matrices.
     */
    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            allocation[i][j] = alloc[i][j];
            request[i][j] = req[i][j];
        }
    }

    printf(1, "\n========================================\n");
    printf(1, "    SCENARIO 2: THREE-PROCESS DEADLOCK\n");
    printf(1, "========================================\n");

    printAllocation();
    printRequest();

    buildWaitForGraph();
    printWaitForGraph();

    detectDeadlock();
}

int main(void)
{
    printf(1, "\n========================================\n");
    printf(1, "     DEADLOCK DETECTION - XV6\n");
    printf(1, "========================================\n");

    /*
     * Scenario 1: No deadlock.
     */
    scenarioNoDeadlock();

    /*
     * Scenario 2: Deadlock involving P0, P1 and P2.
     */
    scenarioDeadlock();

    printf(1, "\n========================================\n");
    printf(1, "      Deadlock detection completed.\n");
    printf(1, "========================================\n");

    exit();
}