SeaBIOS (version 1.17.0-debian-1.17.0-1ubuntu1)


iPXE (https://ipxe.org) 00:03.0 CA00 PCI2.10 PnP PMM+1EFC8E00+1EF08E00 CA00



Booting from Hard Disk..xv6...
cpu0: starting 0
sb: size 1000 nblocks 941 ninodes 200 nlog 30 logstart 2 inodestart 32 bmap start 58
init: starting sh
$ peterson
Process 0 in CS, counter = 1
Process 1 in CS, counter = 2
Process 0 in CS, counter = 3
Process 1 in CS, counter = 4
Process 0 in CS, counter = 5
Process 1 in CS, counter = 6
Process 0 in CS, counter = 7
Process 1 in CS, counter = 8
Process 0 in CS, counter = 9
Process 1 in CS, counter = 10
Process 0 in CS, counter = 11
Process 1 in CS, counter = 12
Process 0 in CS, counter = 13
Process 1 in CS, counter = 14
Process 0 in CS, counter = 15
Process 1 in CS, counter = 16
Process 0 in CS, counter = 17
Process 1 in CS, counter = 18
Process 0 in CS, counter = 19
Process 1 in CS, counter = 20
Final counter = 20 (expected 20)
$ prodcons
Producer: produced 1 (buffer count = 1)
Producer: produced 2 (buffer count = 2)
Producer: produced 3 (buffer count = 3)
Producer: produced 4 (buffer count = 4)
Producer: produced 5 (buffer count = 5)
Consumer: consumed 1 (buffer count = 4)
Consumer: consumed 2 (buffer count = 3)
Consumer: consumed 3 (buffer count = 2)
Consumer: consumed 4 (buffer count = 1)
Consumer: consumed 5 (buffer count = 0)
Producer: produced 6 (buffer count = 1)
Producer: produced 7 (buffer count = 2)
Producer: produced 8 (buffer count = 3)
Producer: produced 9 (buffer count = 4)
Producer: produced 10 (buffer count = 5)
Consumer: consumed 6 (buffer count = 4)
Consumer: consumed 7 (buffer count = 3)
Consumer: consumed 8 (buffer count = 2)
Consumer: consumed 9 (buffer count = 1)
Consumer: consumed 10 (buffer count = 0)
Producer: produced 11 (buffer count = 1)
Producer: produced 12 (buffer count = 2)
Producer: produced 13 (buffer count = 3)
Producer: produced 14 (buffer count = 4)
Producer: produced 15 (buffer count = 5)
Consumer: consumed 11 (buffer count = 4)
Consumer: consumed 12 (buffer count = 3)
Consumer: consumed 13 (buffer count = 2)
Consumer: consumed 14 (buffer count = 1)
Consumer: consumed 15 (buffer count = 0)
Producer: produced 16 (buffer count = 1)
Producer: produced 17 (buffer count = 2)
Producer: produced 18 (buffer count = 3)
Producer: produced 19 (buffer count = 4)
Producer: produced 20 (buffer count = 5)
Consumer: consumed 16 (buffer count = 4)
Consumer: consumed 17 (buffer count = 3)
Consumer: consumed 18 (buffer count = 2)
Consumer: consumed 19 (buffer count = 1)
Consumer: consumed 20 (buffer count = 0)
Producer-Consumer completed successfully
$ readwrite
Reader 1 ENTER, data = 0, readers = 1
Reader 1 EXIT
Reader 1 ENTER, data = 0, readers = 1
Reader 1 EXIT
Reader 2 ENTER, data = 0, readers = 1
Reader 2 EXIT
Reader 3 ENTER, data = 0, readers = 1
Reader 3 EXIT
Writer 1 ENTER, data = 1
Writer 1 EXIT
Writer 1 ENTER, data = 2
Writer 1 EXIT
Writer 1 ENTER, data = 3
Writer 1 EXIT
Writer 1 ENTER, data = 4
Writer 1 EXIT
Writer 1 ENTER, data = 5
Writer 1 EXIT
Writer 2 ENTER, data = 6
Writer 2 EXIT
Writer 2 ENTER, data = 7
Writer 2 EXIT
Writer 2 ENTER, data = 8
Writer 2 EXIT
Writer 2 ENTER, data = 9
Writer 2 EXIT
Writer 2 ENTER, data = 10
Writer 2 EXIT
Reader 1 ENTER, data = 10, readers = 1
Reader 1 EXIT
Reader 2 ENTER, data = 10, readers = 1
Reader 3 ENTER, data = 10, readers = 2
Reader 3 EXIT
Reader 3 ENTER, data = 10, readers = 2
Reader 3 EXIT
Reader 2 EXIT
Reader 2 ENTER, data = 10, readers = 1
Reader 2 EXIT
Readers-Writers completed successfully
Final shared_data = 10
$ dining
Philosopher 0: THINKING
Philosopher 0: HUNGRY
Philosopher 0: picked fork Philosopher 1: THINKING
Philosopher 1: HUNGRY
PhPhilosopher 2: THINKING
Philosopher 2: HUNGRY
Philosopher 3: THINKING
Philosopher Philosopher 4: THINKING
Philosopher 4: HUNGRY
Philosopher 2: picked fork 2
Philosopher 2: picked fork 3
3: HUNGRY
0
ilosopher 1: picked fPhilosopher 2: EATING
Philosopher 2: finished eating
Philosopher 3: picked fork 3
ork 1
Philosopher 2: THINKPhilosopher 3: picked fork 4
Philosopher 3: EATING
Philosopher 1: picked fork 2
Philosopher 1: EAING
Philosopher 2: HUNGRY
Philosopher 3: finiTING
Philosopher 1: finished eating
PhilPhilosopher 2: picked fork 2
shed eating
Philosopher 0: picked fork 1
Philosopher 0: EATING
osopher 1: THINKING
Philosopher 1: HUNGRY
PhilosophePhilosopher 0: finished eating
Philosopher 0: THINKING
Philosopher 1: picked fork 1
Philosopher 2: picked fork r 3: THINKING
Philosopher 3: HUNGRY
PhilosPhilosopher 0: HUNGRY
3
Philosopher 2: EATING
Philosopher 2: finished eating
Philosopher 2:Philosopher 3: picked fork 3
Philosopher 3: picked fork 4
opher 4: picked fork 0
Philosopher 1: picked fork 2
Philosopher 1: EATING
Philosopher 1: fin THINKING
Philosopher 2: HUNGRY
Philosopher 3ished eating
Philosopher 1: THINKING
Philosopher 1: HUNGRY
Philosopher 1: picked fork 1
Philoso: EATING
Philosopher 3: finished eating
PhilosoPhilosopher 4: picked fork 4
Philosopher 4: EpATING
Philosopher 4: finished eating
Philosopher 4: THINKING
Philosopher 4: HUNGRY
pher 2: picked fork 2
Philosopher 2: picked fork 3
Philosopher 2: EATING
Philosopher 2:her 3: THINKING
Philosopher 3: HUNGRY
Philosopher 0: picked fork 0
 finished eating
Philosopher 2: THINKING
Philosopher 2: HUNGRY
Philosopher 3: picked fork 3
Philosopher 3: picked fork 4
Philosopher 3: EATING
PhilosopherPhilosopher 1: picked fork 2
Philosopher 1: EATING
 3: finished eating
Philosopher 3: THINKING
Philosopher 3: HUNGRY
Philosopher 3: picked fork 3
Philosopher 1: finished eating
Philosopher 1: THINKING
Philosopher 2: picked fork 2
Philosopher 3: pickPhilosopher 0: picked fork 1
Philosopher 0: EATING
PPhilosopher 1: HUNGRY
ed fork 4
Philosophhilosopher 0: finished eating
Philosopher 0: THINKINGPhilosopher 1: picked fork 1
er 3: EATING
PhiPhilosopher 4: picked fork 0

Philosopher 0: HUNGRY
losopher 3: finished eating
Philosopher 3: THINKING
Philosopher 3: HUNGRY
Philosopher 4: picked fork 4
Philosopher 4: EATINPhilosopher 2: picked fork 3
Philosopher 2: EATING
G
Philosopher 4: finished eating
PhilosPhilosopher 0: picked fork 0
Philosopher 2: finished eating
Philosopher 2: THINKING
Philosopher 2: Philosopher 3: picked fork 3
Philosopheopher 4: THINKING
Philosopher 4: HUNGRY
PhilosoHUNGRY
r 3: picked fork 4
Philosopher 3: EATING
pher 1: picked fork 2
Philosopher 1: EATING
Philosopher 3: finished eating
Philosopher 3: completed all 5 cycles
Philosopher 1: finished eating
Philosopher 1: THINKING
Philosopher 1: HUNGRY
Philosopher 2: picked fork 2
Philosopher 2: picPhilosopher 0: picked fork 1
Philosopher 0: EATING
ked fork 3
Philosopher 2: EATING
Philosopher 2: fiPhilosopher 0: finished eating
Philosopher 0: THINKPhilosopher 1: picked fork 1
nished eating
PhilosoPhilosopher 4: picked fork 0
Philosopher 4: picked fork 4
ING
Philosopher 0: HUNGRY
Philosopher 1: picked fork 2
Philosopher 1: EATING
Philosopher 1: finpher 2: completed all 5 cycles
Philosopherished eating
Philosopher 1: completed all 5 cycle 4: EATING
Philosopher 4: finished eating
PhilosophPhilosopher 0: picked fork 0
Philosopher 0: picked fors
er 4: THINKING
Philk 1
Philosopher 0: EATING
Philosopher 0: finished eating
Philosopher 0: THINKING
Philosopher 0: HUNGRY
Philosopher 0: picked foosopher 4: HUNGRY
rk 0
Philosopher 0: picked fork 1
Philosopher 0: EATING
Philosopher 0: finished eating
Philosopher 0: completed all 5 cycles
Philosopher 4: picked fork 0
Philosopher 4: picked fork 4
Philosopher 4: EATING
Philosopher 4: finished eating
Philosopher 4: THINKING
Philosopher 4: HUNGRY
Philosopher 4: picked fork 0
Philosopher 4: picked fork 4
Philosopher 4: EATING
Philosopher 4: finished eating
Philosopher 4: completed all 5 cycles
Dining Philosophers completed successfully
Philosopher 0 completed 5 cycles
Philosopher 1 completed 5 cycles
Philosopher 2 completed 5 cycles
Philosopher 3 completed 5 cycles
Philosopher 4 completed 5 cycles
