# Task: Ant Colony Dispatch

Deep under the forest there is an ant colony built from chambers connected by one-way tunnels. The queen lives in one selected chamber and keeps sending scout ants into the maze. Somewhere else in the colony there is a food chamber. Every scout tries to reach it by moving from chamber to chamber. Some scouts get lost, some find food, and from time to time an overloaded chamber collapses and disappears from the colony.

Write a program called `sop-ants` that simulates this colony using processes, unnamed pipes, a named FIFO, and signals.

## Program Arguments

The program should be started as:

```bash
./sop-ants graph start dest
```

where:

- `graph` is a path to a text file describing the colony,
- `start` is the index of the chamber where new ants are spawned,
- `dest` is the index of the chamber that contains food.

Assume:

- `1 <= n <= 32`,
- `0 <= start, dest < n`.

## Colony Description File

The graph file describes a directed graph.

- The first line contains the number of chambers `n`.
- Each next line contains two integers: `from to`.
- A line `a b` means there is a one-way tunnel from chamber `a` to chamber `b`.

Example:

```text
4
0 1
0 2
1 0
1 2
1 3
3 1
3 2
```

## Story Rules

Each chamber of the colony is handled by a separate process. The parent process plays the role of the queen. Ants are small messages carrying:

- a unique ant ID,
- the path visited so far.

The maximum remembered path length is `64`.

## Required Functionality

### 1. Colony Setup

After startup, the queen process should:

- read the graph from the input file,
- create one unnamed pipe for each chamber,
- create a named FIFO `/tmp/colony_fifo`,
- fork one child process per chamber.

Each chamber process should immediately print its local map in the form:

```text
{node}: neighbour1 neighbour2 ...
```

Example:

```text
{3}: 1 2
```

### 2. Chamber Behavior

Each child process represents exactly one chamber and waits for ants on its private unnamed pipe.

When a chamber receives an ant, it should:

1. wait `100` ms,
2. append its own chamber index to the ant path.

Then:

- If the chamber has no outgoing tunnels, the ant is lost:

```text
Ant {ID}: got lost
```

- If the path length reaches `64`, the ant is also treated as lost:

```text
Ant {ID}: got lost
```

- If the current chamber is the destination chamber `dest`, the ant found food. The chamber prints:

```text
Ant {ID}: found food
```

  and sends the whole ant record to the queen through the named FIFO.

- Otherwise, the chamber chooses one outgoing tunnel at random and forwards the ant to that neighbour using the neighbour's unnamed pipe.

If forwarding fails because the target chamber is already gone, the ant is lost and the process prints:

```text
Ant {ID}: got lost
```

After forwarding an ant, a chamber may collapse with probability `1/50`. In that case it prints:

```text
Node {node}: collapsed
```

and terminates.

### 3. Queen Behavior

The parent process acts as the queen and generates a new ant every second.

Ants should:

- receive consecutive IDs starting from `0`,
- always enter the colony through chamber `start`.

After sending one ant into the colony, the queen should perform a non-blocking read from `/tmp/colony_fifo`.

If a successful ant arrives from the destination chamber, the queen prints its route:

```text
Ant {ID} path: v1 v2 v3 ...
```

### 4. End of Simulation

The simulation continues until the starting chamber is no longer reachable, meaning the queen cannot inject another ant into chamber `start` because its pipe is broken.

At that point the queen should:

- send `SIGINT` to the whole process group,
- wait for all chamber processes to finish,
- close all remaining file descriptors,
- remove `/tmp/colony_fifo`,
- exit.

Each chamber process should handle `SIGINT` so it can stop waiting on its pipe and terminate cleanly.

## Required IPC and Synchronization

The solution should use:

- `fork()` to create chamber processes,
- unnamed pipes for communication between chambers,
- one named FIFO for sending successful ants back to the parent,
- `sigaction()` and `SIGINT` for shutdown,
- `wait()` or `waitpid()` for cleanup.

## Output Notes

The program is nondeterministic because:

- the next tunnel is chosen randomly,
- chambers may collapse randomly,
- the timing of processes may differ between runs.

Because of that, exact output will vary. The important part is to preserve the message format and the simulation rules.

## Example Messages

```text
{0}: 1 2
{1}: 0 2 3
Ant {0}: found food
Ant {1}: got lost
Node {3}: collapsed
Ant {0} path: 0 1 3
```
