# Computer Networks Course Projects

This repository contains my solutions for two major projects from the "Computer Networks" course I took at the University of Warsaw in 2025.

Each project resides in its own directory, complete with its source code and a `Makefile`.

---

## 1. Peer-to-Peer Clock Synchronization

*   **Directory:** `peer-time-sync/`
*   **Language:** C

### Project Description

This project is a C implementation of a peer-to-peer clock synchronization system. The goal is to create a decentralized network where nodes can join, communicate, and synchronize their local clocks with each other, accounting for network transmission delays. The system operates without a central server; all nodes are equal peers.

The core functionalities of the protocol include:
*   **Dynamic Network Discovery:** A new node can join the network by connecting to a single known peer. It then receives a list of other known nodes and establishes communication with them.
*   **Leader Election:** The network dynamically establishes a time hierarchy. One node can become a "leader" (synchronization level 0), acting as the primary time source. Other nodes synchronize to it directly (level 1) or indirectly through other peers (level 2, 3, etc.), creating a synchronization tree. A node's level indicates its distance from the leader, with a level of 255 signifying it is unsynchronized.
*   **Latency-Aware Synchronization:** The time synchronization algorithm is designed to be robust against network latency. It uses a three-way message exchange (`SYNC_START`, `DELAY_REQUEST`, `DELAY_RESPONSE`) to measure the round-trip time and calculate the clock offset between two peers, ensuring a more accurate synchronization.
*   **Decentralized Control:** Nodes can be instructed to become or cease being a leader via a specific control message. They can also lose synchronization if they don't hear from their upstream peer, at which point they will attempt to find a new peer to sync with.

  Communication is handled entirely over UDP using IPv4.

### What I Learned

This project was a deep dive into low-level network programming in a C environment. I gained practical experience with the Berkeley sockets API for UDP communication, including handling network byte order for data serialization. A key challenge was managing communication with multiple peers simultaneously in a single-threaded, event-driven model, which I solved by using the `select()` system call for non-blocking I/O. This taught me how to build efficient, responsive network applications without relying on multithreading. Furthermore, I learned to design and implement a custom binary protocol, ensuring robust error handling for system calls, corrupted packets, and unexpected protocol states.

---

## 2. The Great Approximator: A Client-Server Game

*   **Directory:** `the-great-approximator/`
*   **Language:** C++

### Project Description

"The Great Approximator" is a multiplayer client-server game implemented in C++. The server application manages multiple concurrent game sessions, while clients connect to compete. The objective for each player is to approximate a secret polynomial function, `f(x)`, assigned to them by the server.

The game flow is as follows:
*   **Concurrent Server Architecture:** The server is built to handle many simultaneous TCP connections from different clients. It supports both IPv4 and IPv6, making it compatible with modern network environments.
*   **Game Logic:** Upon connecting, each client receives the coefficients of a unique polynomial of degree `N`. The client's task is to build an approximation of this function over a range of integer points from 0 to `K`.
*   **Interactive Approximation:** Players improve their approximation by sending `PUT` commands, which add a specified value to their function at a given point. The server validates these commands, updates the player's state, and responds with the current state of their approximation function.
*   **Scoring and Game Rounds:** The game ends after a collective total of `M` valid `PUT` commands have been processed by the server across all players. A player's score is calculated based on the sum of squared errors between their approximation and the actual polynomial, with penalties added for protocol violations (e.g., sending commands out of order). At the end of a round, the server broadcasts a final scoreboard to all players and resets for the next game.
*   **Dual-Mode Client:** The client application was implemented with two distinct strategies: a manual mode that takes user input from the console to make approximation moves, and an automated mode that implements a strategy to win the game algorithmically.

### What I Learned

This project solidified my understanding of the client-server model and TCP socket programming in C++. The main focus was on building a robust server capable of managing the state of multiple concurrent clients without communication with one client blocking others. This involved careful design of a non-blocking server loop and session management. I also gained experience designing a text-based application-layer protocol, including command parsing and strict validation. Implementing dual-stack (IPv4/IPv6) support taught me the modern approach to writing network-agnostic code.
